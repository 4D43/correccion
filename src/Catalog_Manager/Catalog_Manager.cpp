// catalog_manager/catalog_manager.cpp
#include "catalog_manager.h"
#include <iostream>
#include <cstring> // For std::memcpy
#include <algorithm> // For std::min, std::max
#include <fstream>   // For file operations
#include <sstream>   // For std::stringstream
#include <filesystem> // For path operations

// Constructor
CatalogManager::CatalogManager(BufferManager& buffer_manager, RecordManager& record_manager)
    : buffer_manager_(buffer_manager), record_manager_(record_manager),
      catalog_page_id_(0), // Asumimos que la primera CATALOG_PAGE será PageId 0 (o una conocida)
      next_table_id_(1)    // TableId 0 podría ser reservado o simplemente empezar en 1
{
    std::cout << "CatalogManager inicializado." << std::endl;
}

// Inicializa el catálogo (crea la primera CATALOG_PAGE si no existe).
Status CatalogManager::InitCatalog() {
    // Intentar cargar el catálogo primero.
    Status load_status = LoadCatalog();
    if (load_status == Status::OK) {
        std::cout << "Catálogo existente cargado exitosamente." << std::endl;
        return Status::OK;
    }

    // Si no se pudo cargar (ej. NOT_FOUND), significa que es un catálogo nuevo.
    std::cout << "Catálogo no encontrado. Creando nueva CATALOG_PAGE..." << std::endl;
    
    // Asignar una nueva CATALOG_PAGE.
    // NewPage ya ancla la página (pin_count = 1).
    PageId new_catalog_page_id;
    Byte* page_data = buffer_manager_.NewPage(new_catalog_page_id, PageType::CATALOG_PAGE);
    if (page_data == nullptr) {
        std::cerr << "Error (InitCatalog): No se pudo asignar una nueva CATALOG_PAGE." << std::endl;
        return Status::ERROR;
    }
    catalog_page_id_ = new_catalog_page_id;

    // Inicializar la nueva CATALOG_PAGE como una página de datos para registros (de metadatos).
    // InitDataPage internamente hará un FetchPage (pin_count++) y UnpinPage (pin_count--).
    // Al final de InitDataPage, la página seguirá anclada una vez por la llamada a NewPage.
    Status init_status = record_manager_.InitDataPage(catalog_page_id_);
    if (init_status != Status::OK) {
        std::cerr << "Error (InitCatalog): Fallo al inicializar la nueva CATALOG_PAGE " << catalog_page_id_ << "." << std::endl;
        // Si InitDataPage falla, la página podría quedar anclada. Desanclarla y eliminarla.
        buffer_manager_.UnpinPage(catalog_page_id_, true); // Desanclar y marcar sucia para asegurar flush si algo se escribió
        buffer_manager_.DeletePage(catalog_page_id_);
        return init_status;
    }
    // Desanclar la página del catálogo que fue creada por NewPage.
    // Esto es crucial para que su pin_count sea 0 antes de cualquier operación que requiera 0 pins.
    buffer_manager_.UnpinPage(catalog_page_id_, true); // Marcar sucia porque se inicializó

    // Guardar el catálogo vacío para persistir la existencia de la CATALOG_PAGE.
    Status save_status = SaveCatalog();
    if (save_status != Status::OK) {
        std::cerr << "Error (InitCatalog): Fallo al guardar el catálogo inicial." << std::endl;
        return save_status;
    }

    std::cout << "Nueva CATALOG_PAGE creada con PageId: " << catalog_page_id_ << std::endl;
    std::cout << "Catálogo inicializado exitosamente." << std::endl;
    return Status::OK;
}

// Serializa FullTableSchema a un Record.
Record CatalogManager::SerializeTableSchema(const FullTableSchema& schema) const {
    Record record;
    // Calcular el tamaño total del record serializado
    size_t total_size = sizeof(TableMetadata) + sizeof(uint32_t); // Base metadata + num_columns
    total_size += schema.columns.size() * sizeof(ColumnMetadata); // Column metadata

    record.data.resize(total_size);
    Byte* ptr = record.data.data();

    // 1. Copiar TableMetadata
    std::memcpy(ptr, &schema.base_metadata, sizeof(TableMetadata));
    ptr += sizeof(TableMetadata);

    // 2. Copiar num_columns
    uint32_t num_columns = schema.columns.size();
    std::memcpy(ptr, &num_columns, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    // 3. Copiar cada ColumnMetadata
    for (const auto& col : schema.columns) {
        std::memcpy(ptr, &col, sizeof(ColumnMetadata));
        ptr += sizeof(ColumnMetadata);
    }
    return record;
}

// Deserializa un Record a FullTableSchema.
FullTableSchema CatalogManager::DeserializeTableSchema(const Record& record) const {
    FullTableSchema schema;
    const Byte* ptr = record.data.data();

    // 1. Leer TableMetadata
    std::memcpy(&schema.base_metadata, ptr, sizeof(TableMetadata));
    ptr += sizeof(TableMetadata);

    // 2. Leer num_columns
    uint32_t num_columns;
    std::memcpy(&num_columns, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    // 3. Leer cada ColumnMetadata
    schema.columns.resize(num_columns);
    for (uint32_t i = 0; i < num_columns; ++i) {
        std::memcpy(&schema.columns[i], ptr, sizeof(ColumnMetadata));
        ptr += sizeof(ColumnMetadata);
    }
    return schema;
}

// Crea una nueva tabla y persiste su metadata (interactivo/formulario).
Status CatalogManager::CreateTable(const std::string& table_name,
                                   const std::vector<ColumnMetadata>& columns,
                                   bool is_fixed_length_record) {
    if (table_schemas_.count(table_name) > 0) {
        std::cerr << "Error (CreateTable): La tabla '" << table_name << "' ya existe." << std::endl;
        return Status::DUPLICATE_ENTRY;
    }

    // 1. Asignar un nuevo TableId
    uint32_t new_table_id = next_table_id_++;

    // 2. Asignar la primera DATA_PAGE para esta nueva tabla
    PageId first_data_page_id;
    Byte* data_page_ptr = buffer_manager_.NewPage(first_data_page_id, PageType::DATA_PAGE);
    if (data_page_ptr == nullptr) {
        std::cerr << "Error (CreateTable): No se pudo asignar la primera DATA_PAGE para la tabla '" << table_name << "'." << std::endl;
        next_table_id_--; // Revertir el TableId
        return Status::ERROR;
    }
    // Inicializar la nueva DATA_PAGE
    Status init_data_page_status = record_manager_.InitDataPage(first_data_page_id);
    if (init_data_page_status != Status::OK) {
        std::cerr << "Error (CreateTable): Fallo al inicializar la primera DATA_PAGE " << first_data_page_id << " para la tabla '" << table_name << "'." << std::endl;
        // Asegurarse de desanclar y eliminar la página si la inicialización falla
        buffer_manager_.UnpinPage(first_data_page_id, true);
        buffer_manager_.DeletePage(first_data_page_id);
        next_table_id_--;
        return init_data_page_status;
    }
    // Desanclar la página de datos después de inicializarla
    buffer_manager_.UnpinPage(first_data_page_id, true);

    // 3. Crear la metadata de la tabla
    FullTableSchema new_schema;
    new_schema.base_metadata.table_id = new_table_id;
    strncpy(new_schema.base_metadata.table_name, table_name.c_str(), sizeof(new_schema.base_metadata.table_name) - 1);
    new_schema.base_metadata.table_name[sizeof(new_schema.base_metadata.table_name) - 1] = '\0';
    new_schema.base_metadata.is_fixed_length_record = is_fixed_length_record;
    new_schema.base_metadata.first_data_page_id = first_data_page_id;
    new_schema.base_metadata.num_records = 0; // Inicialmente 0 registros
    new_schema.columns = columns;

    // Calcular fixed_record_size si aplica
    if (is_fixed_length_record) {
        uint32_t total_fixed_size = 0;
        for (const auto& col : columns) {
            if (col.type == ColumnType::VARCHAR) {
                std::cerr << "Error (CreateTable): No se puede tener VARCHAR en una tabla de longitud fija." << std::endl;
                buffer_manager_.DeletePage(first_data_page_id);
                next_table_id_--;
                return Status::INVALID_PARAMETER;
            }
            total_fixed_size += col.size;
        }
        new_schema.base_metadata.fixed_record_size = total_fixed_size;
    } else {
        new_schema.base_metadata.fixed_record_size = 0; // No aplica para variable
    }

    // 4. Añadir a nuestro mapa en memoria para acceso rápido
    table_schemas_[table_name] = new_schema;

    // 5. Persistir el catálogo actualizado (ahora se guarda todo el mapa en memoria)
    Status save_status = SaveCatalog();
    if (save_status != Status::OK) {
        std::cerr << "Advertencia (CreateTable): La tabla se creó, pero falló la persistencia del catálogo. Los datos podrían perderse." << std::endl;
        // Podríamos considerar revertir la creación de la tabla aquí si esto fuera crítico.
    }

    std::cout << "Tabla '" << table_name << "' (ID: " << new_table_id
              << ", Primera Página de Datos: " << first_data_page_id
              << ") creada exitosamente y registrada en el catálogo." << std::endl;
    return Status::OK;
}

// Crea una nueva tabla y persiste su metadata a partir de un archivo de texto.
Status CatalogManager::CreateTableFromPath(const std::string& file_path) {
    std::cout << "Creando tabla desde archivo: " << file_path << std::endl;

    // 1. Obtener el nombre de la tabla del nombre del archivo
    std::filesystem::path path_obj(file_path);
    std::string table_name = path_obj.stem().string(); // "stem" obtiene el nombre del archivo sin extensión

    if (table_schemas_.count(table_name) > 0) {
        std::cerr << "Error (CreateTableFromPath): La tabla '" << table_name << "' ya existe." << std::endl;
        return Status::DUPLICATE_ENTRY;
    }

    // 2. Abrir el archivo y leer las primeras dos líneas: nombres y datos para inferencia
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Error (CreateTableFromPath): No se pudo abrir el archivo: " << file_path << std::endl;
        return Status::IO_ERROR;
    }

    std::string column_names_line;
    std::string first_data_line; // Esta línea se usará para inferir tipos

    // Leer la primera línea (nombres de columnas)
    if (!std::getline(file, column_names_line)) {
        std::cerr << "Error (CreateTableFromPath): El archivo no tiene la línea de nombres de columna." << std::endl;
        file.close();
        return Status::INVALID_PARAMETER;
    }
    // Leer la segunda línea (primera fila de datos para inferencia de tipos)
    if (!std::getline(file, first_data_line)) {
        std::cerr << "Error (CreateTableFromPath): El archivo no tiene al menos una fila de datos para inferir tipos." << std::endl;
        file.close();
        return Status::INVALID_PARAMETER;
    }
    file.close(); // Cerramos el archivo después de leer las líneas de esquema/inferencia.

    // 3. Parsear nombres de columnas (asumiendo coma como delimitador)
    std::vector<std::string> names;
    std::stringstream ss_names(column_names_line);
    std::string name_token;
    while (std::getline(ss_names, name_token, ',')) {
        // Eliminar espacios en blanco alrededor del token
        name_token.erase(0, name_token.find_first_not_of(" \t\n\r\f\v"));
        name_token.erase(name_token.find_last_not_of(" \t\n\r\f\v") + 1);
        names.push_back(name_token);
    }

    // 4. Parsear la primera línea de datos para inferir tipos y tamaños
    std::vector<ColumnMetadata> columns;
    std::stringstream ss_data(first_data_line);
    std::string data_token;
    bool is_fixed_length_record = true; // Asumimos fija por defecto

    for (const std::string& name : names) {
        if (!std::getline(ss_data, data_token, ',')) {
            std::cerr << "Error (CreateTableFromPath): Número de valores en la fila de datos no coincide con el número de nombres de columna." << std::endl;
            return Status::INVALID_PARAMETER;
        }
        // Eliminar espacios en blanco alrededor del token de datos
        data_token.erase(0, data_token.find_first_not_of(" \t\n\r\f\v"));
        data_token.erase(data_token.find_last_not_of(" \t\n\r\f\v") + 1);

        ColumnMetadata col;
        strncpy(col.name, name.c_str(), sizeof(col.name) - 1);
        col.name[sizeof(col.name) - 1] = '\0';

        // Intentar inferir tipo: INT o VARCHAR
        bool is_int = true;
        if (data_token.empty()) { // Cadena vacía no es INT
            is_int = false;
        } else {
            // Verificar si todos los caracteres son dígitos, opcionalmente con un signo al principio
            size_t start_idx = 0;
            if (data_token[0] == '-' || data_token[0] == '+') {
                start_idx = 1;
            }
            for (size_t i = start_idx; i < data_token.length(); ++i) {
                if (!std::isdigit(data_token[i])) {
                    is_int = false;
                    break;
                }
            }
        }
        
        if (is_int) {
            col.type = ColumnType::INT;
            col.size = sizeof(int); // Tamaño fijo para INT
        } else {
            col.type = ColumnType::VARCHAR; // Si no es INT, asumimos VARCHAR (cadena variable)
            col.size = data_token.length(); // El tamaño será la longitud de la cadena de ejemplo
            is_fixed_length_record = false; // Si hay un VARCHAR, la tabla es de longitud variable
        }
        columns.push_back(col);
    }

    // 5. Llamar a CreateTable con el esquema parseado
    return CreateTable(table_name, columns, is_fixed_length_record);
}

// Obtiene la metadata completa de una tabla por su nombre.
Status CatalogManager::GetTableSchema(const std::string& table_name, FullTableSchema& schema) {
    auto it = table_schemas_.find(table_name);
    if (it != table_schemas_.end()) {
        schema = it->second;
        return Status::OK;
    }
    std::cerr << "Error (GetTableSchema): Tabla '" << table_name << "' no encontrada en el catálogo." << std::endl;
    return Status::NOT_FOUND;
}

// Elimina una tabla y toda su metadata del catálogo.
Status CatalogManager::DropTable(const std::string& table_name) {
    auto it = table_schemas_.find(table_name);
    if (it == table_schemas_.end()) {
        std::cerr << "Error (DropTable): Tabla '" << table_name << "' no encontrada en el catálogo." << std::endl;
        return Status::NOT_FOUND;
    }

    FullTableSchema schema_to_drop = it->second;

    // 1. Eliminar la metadata de la tabla del mapa en memoria.
    table_schemas_.erase(it);

    // 2. Liberar todas las páginas de datos asociadas a la tabla.
    // Por ahora, solo tenemos first_data_page_id. En un SGBD real, las tablas tendrían
    // una lista de PageIds o un PageDirectory.
    Status delete_page_status = buffer_manager_.DeletePage(schema_to_drop.base_metadata.first_data_page_id);
    if (delete_page_status != Status::OK) {
        std::cerr << "Advertencia (DropTable): Fallo al eliminar la primera DATA_PAGE " << schema_to_drop.base_metadata.first_data_page_id
                  << " de la tabla '" << table_name << "'. Estado: " << StatusToString(delete_page_status) << std::endl;
        // Podríamos considerar reinsertar la tabla en table_schemas_ si la eliminación de la página falla.
    } else {
        std::cout << "Primera DATA_PAGE " << schema_to_drop.base_metadata.first_data_page_id << " de la tabla '" << table_name << "' eliminada." << std::endl;
    }

    // 3. Persistir el catálogo actualizado (sin la tabla eliminada).
    Status save_status = SaveCatalog();
    if (save_status != Status::OK) {
        std::cerr << "Advertencia (DropTable): La tabla se eliminó, pero falló la persistencia del catálogo. El catálogo podría estar inconsistente." << std::endl;
    }

    std::cout << "Tabla '" << table_name << "' eliminada exitosamente del catálogo." << std::endl;
    return Status::OK;
}

// Lista todas las tablas registradas en el catálogo.
Status CatalogManager::ListTables(std::vector<std::string>& table_names) {
    table_names.clear();
    for (const auto& pair : table_schemas_) {
        table_names.push_back(pair.first);
    }
    if (table_names.empty()) {
        std::cout << "No hay tablas registradas en el catálogo." << std::endl;
    }
    return Status::OK;
}

// Método para cargar todo el catálogo desde las CATALOG_PAGEs.
Status CatalogManager::LoadCatalog() {
    // Asumimos que la primera CATALOG_PAGE es la PageId 1 (o la que se asignó en InitCatalog)
    // En un sistema real, el DiskManager o un SuperBlock sabría la PageId del Catálogo.
    // Por ahora, si catalog_page_id_ es 0, intentamos cargar la PageId 1.
    if (catalog_page_id_ == 0) {
        // Intentar cargar la PageId 1 como la CATALOG_PAGE inicial.
        // Esto es una heurística para la primera carga después de un reinicio.
        // Si no existe, NewPage la creará.
        // Si existe, FetchPage la cargará.
        // Necesitamos una forma de saber si PageId 1 es realmente una CATALOG_PAGE.
        // Por ahora, asumiremos que si existe y es una DATA_PAGE, es nuestro catálogo.
        // Una mejor forma sería que el DiskMetadata o un SuperBlock guardara la PageId del catálogo.
        
        // Para la primera carga, si no se ha creado el disco, no habrá PageId 1.
        // Si el disco se creó, PageId 0 es metadata, PageId 1 es la primera DATA_PAGE o CATALOG_PAGE.
        // Intentaremos cargar PageId 1 y verificar su tipo.
        Byte* page_data_check = buffer_manager_.FetchPage(1); // Try to fetch PageId 1
        if (page_data_check == nullptr) {
            std::cerr << "Error (LoadCatalog): No se pudo cargar PageId 1. El catálogo podría no existir o el disco no está inicializado." << std::endl;
            return Status::NOT_FOUND;
        }
        BlockHeader header_check;
        std::memcpy(&header_check, page_data_check, sizeof(BlockHeader));
        buffer_manager_.UnpinPage(1, false); // Unpin immediately

        if (header_check.page_type != PageType::CATALOG_PAGE && header_check.page_type != PageType::DATA_PAGE) {
            std::cerr << "Error (LoadCatalog): PageId 1 no es una CATALOG_PAGE ni DATA_PAGE. Tipo: " << PageTypeToString(header_check.page_type) << std::endl;
            return Status::ERROR; // O un estado más específico
        }
        catalog_page_id_ = 1; // Asumimos que PageId 1 es nuestra CATALOG_PAGE
    }

    std::cout << "Cargando catálogo desde CATALOG_PAGE (PageId " << catalog_page_id_ << ")..." << std::endl;

    table_schemas_.clear(); // Limpiar el mapa en memoria antes de cargar

    // Leer todos los registros de la CATALOG_PAGE
    Byte* catalog_page_data = buffer_manager_.FetchPage(catalog_page_id_);
    if (catalog_page_data == nullptr) {
        std::cerr << "Error (LoadCatalog): No se pudo obtener la CATALOG_PAGE " << catalog_page_id_ << " del BufferManager." << std::endl;
        return Status::ERROR;
    }

    BlockHeader header = record_manager_.ReadBlockHeader(catalog_page_data); // Usar RecordManager para leer cabecera
    buffer_manager_.UnpinPage(catalog_page_id_, false); // Desanclar la página después de leer la cabecera

    // Iterar sobre todos los slots de la CATALOG_PAGE
    for (uint32_t i = 0; i < header.num_slots; ++i) {
        Record catalog_record;
        Status get_record_status = record_manager_.GetRecord(catalog_page_id_, i, catalog_record);
        if (get_record_status == Status::OK) {
            FullTableSchema schema = DeserializeTableSchema(catalog_record);
            table_schemas_[schema.base_metadata.table_name] = schema;
            // Actualizar next_table_id_ para que sea mayor que cualquier TableId cargado
            if (schema.base_metadata.table_id >= next_table_id_) {
                next_table_id_ = schema.base_metadata.table_id + 1;
            }
        } else if (get_record_status != Status::NOT_FOUND) {
            // Si no es NOT_FOUND, es un error real al leer el registro
            std::cerr << "Error (LoadCatalog): Fallo al leer el registro del catálogo en slot " << i << "." << std::endl;
            return get_record_status;
        }
    }

    std::cout << "Catálogo cargado. " << table_schemas_.size() << " tablas encontradas." << std::endl;
    return Status::OK;
}

// Método para guardar todo el catálogo en las CATALOG_PAGEs.
Status CatalogManager::SaveCatalog() {
    std::cout << "Guardando catálogo en CATALOG_PAGE (PageId " << catalog_page_id_ << ")..." << std::endl;

    // Obtener la página del catálogo.
    // La página debe estar desanclada para poder ser modificada de esta forma.
    // Si no está en el buffer, FetchPage la cargará y la anclará.
    Byte* catalog_page_data = buffer_manager_.FetchPage(catalog_page_id_);
    if (catalog_page_data == nullptr) {
        std::cerr << "Error (SaveCatalog): No se pudo obtener la CATALOG_PAGE " << catalog_page_id_ << " del BufferManager." << std::endl;
        return Status::ERROR;
    }

    // Resetear la cabecera de la página del catálogo para "limpiarla".
    // Esto marca todos los slots como libres y el espacio como disponible.
    // Es como si la página estuviera recién inicializada, pero sin reasignar en disco.
    BlockHeader header;
    header.page_id = catalog_page_id_;
    header.page_type = PageType::CATALOG_PAGE; // Asegurarse de que el tipo sea correcto
    header.num_slots = 0;
    header.header_and_slot_directory_size = record_manager_.GetSlotDirectoryStartOffset(); // Solo la cabecera fija
    header.data_end_offset = buffer_manager_.GetBlockSize(); // Todo el espacio disponible

    record_manager_.WriteBlockHeader(catalog_page_data, header);
    // Opcional: Llenar el resto del bloque con ceros para claridad (debug).
    std::fill(catalog_page_data + header.header_and_slot_directory_size, 
              catalog_page_data + buffer_manager_.GetBlockSize(), 0);

    // Insertar cada esquema de tabla como un nuevo registro en la CATALOG_PAGE
    for (const auto& pair : table_schemas_) {
        Record catalog_record = SerializeTableSchema(pair.second);
        uint32_t slot_id;
        // InsertRecord ancla y desancla la página internamente, pero la deja sucia.
        Status insert_status = record_manager_.InsertRecord(catalog_page_id_, catalog_record, slot_id);
        if (insert_status != Status::OK) {
            std::cerr << "Error (SaveCatalog): Fallo al insertar la metadata de la tabla '" << pair.first << "' en el catálogo." << std::endl;
            // Desanclar la página antes de retornar en caso de error
            buffer_manager_.UnpinPage(catalog_page_id_, true);
            return insert_status;
        }
    }

    // Desanclar la página del catálogo después de haber insertado todos los registros.
    // Marcarla como sucia para que los cambios se persistan.
    buffer_manager_.UnpinPage(catalog_page_id_, true);

    std::cout << "Catálogo guardado exitosamente." << std::endl;
    return Status::OK;
}

// Método auxiliar para encontrar un slot libre o crear uno nuevo en la CATALOG_PAGE.
// (Este método ya no es directamente necesario con la estrategia de reescritura completa en SaveCatalog,
// pero se mantiene por si se cambia a una gestión de slots más granular).
Status CatalogManager::FindOrCreateCatalogSlot(uint32_t& slot_id) {
    // Implementación futura: buscar slot libre en catalog_page_id_
    // o expandir la página / crear una nueva CATALOG_PAGE si la actual está llena.
    // Por ahora, InsertRecord de RecordManager ya maneja la asignación de slots.
    return Status::OK;
}
