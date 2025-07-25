// catalog_manager/catalog_manager.cpp
#include "Catalog_Manager.h"
#include <iostream>
#include <cstring> // For std::memcpy
#include <algorithm> // For std::min, std::max
#include <fstream>   // For file operations
#include <sstream>   // For std::stringstream
#include <filesystem> // For path operations

// Incluir record_manager.h para obtener las definiciones completas de RecordManager, Record, BlockHeader, SlotDirectoryEntry
#include "../record_manager/record_manager.h" 

// Constructor
CatalogManager::CatalogManager(BufferManager& buffer_manager)
    : buffer_manager_(buffer_manager), record_manager_(nullptr), // Inicializar puntero a nullptr
      catalog_page_id_(0), 
      next_table_id_(1)    
{
    std::cout << "CatalogManager inicializado." << std::endl;
}

// Método setter para el RecordManager
void CatalogManager::SetRecordManager(RecordManager& record_manager) {
    record_manager_ = &record_manager;
}

// Inicializa el catálogo (crea la primera CATALOG_PAGE si no existe).
Status CatalogManager::InitCatalog() {
    // Asegurarse de que record_manager_ esté configurado
    if (record_manager_ == nullptr) {
        std::cerr << "Error (InitCatalog): RecordManager no ha sido configurado en CatalogManager." << std::endl;
        return Status::ERROR;
    }

    // Intentar cargar el catálogo primero.
    Status load_status = LoadCatalog();
    if (load_status == Status::OK) {
        std::cout << "Catálogo existente cargado exitosamente." << std::endl;
        return Status::OK;
    }

    // Si no se pudo cargar (ej. NOT_FOUND), significa que es un catálogo nuevo.
    std::cout << "Catálogo no encontrado. Creando nueva CATALOG_PAGE..." << std::endl;
    
    PageId new_catalog_page_id;
    Byte* catalog_page_data = buffer_manager_.NewPage(new_catalog_page_id, PageType::CATALOG_PAGE);
    if (catalog_page_data == nullptr) {
        std::cerr << "Error (InitCatalog): Fallo al crear la CATALOG_PAGE." << std::endl;
        return Status::ERROR;
    }
    catalog_page_id_ = new_catalog_page_id; 

    // Inicializar la página del catálogo como una página de datos vacía para registros de metadatos.
    Status init_status = record_manager_->InitDataPage(catalog_page_id_);
    if (init_status != Status::OK) {
        std::cerr << "Error (InitCatalog): Fallo al inicializar la CATALOG_PAGE como DATA_PAGE." << std::endl;
        buffer_manager_.UnpinPage(catalog_page_id_, false); 
        return init_status;
    }
    
    buffer_manager_.UnpinPage(catalog_page_id_, true); 

    std::cout << "Nueva CATALOG_PAGE creada exitosamente con PageId: " << catalog_page_id_ << std::endl;
    return Status::OK;
}

// Crea una nueva tabla y la registra en el catálogo.
Status CatalogManager::CreateTable(const std::string& table_name, const std::vector<ColumnMetadata>& columns, bool is_fixed_length_record) {
    if (record_manager_ == nullptr) {
        std::cerr << "Error (CreateTable): RecordManager no ha sido configurado en CatalogManager." << std::endl;
        return Status::ERROR;
    }

    if (table_schemas_.count(table_name) > 0) {
        std::cerr << "Error (CreateTable): La tabla '" << table_name << "' ya existe." << std::endl;
        return Status::DUPLICATE_ENTRY;
    }

    uint32_t current_table_id = next_table_id_++;

    PageId first_data_page_id;
    Byte* data_page_data = buffer_manager_.NewPage(first_data_page_id, PageType::DATA_PAGE);
    if (data_page_data == nullptr) {
        std::cerr << "Error (CreateTable): Fallo al crear la primera DATA_PAGE para la tabla '" << table_name << "'." << std::endl;
        return Status::ERROR;
    }
    Status init_data_page_status = record_manager_->InitDataPage(first_data_page_id);
    if (init_data_page_status != Status::OK) {
        std::cerr << "Error (CreateTable): Fallo al inicializar la primera DATA_PAGE para la tabla '" << table_name << "'." << std::endl;
        buffer_manager_.DeletePage(first_data_page_id); 
        return init_data_page_status;
    }

    FullTableSchema new_schema;
    new_schema.base_metadata.table_id = current_table_id;
    strncpy(new_schema.base_metadata.table_name, table_name.c_str(), sizeof(new_schema.base_metadata.table_name) - 1);
    new_schema.base_metadata.table_name[sizeof(new_schema.base_metadata.table_name) - 1] = '\0';
    new_schema.base_metadata.is_fixed_length_record = is_fixed_length_record;
    new_schema.base_metadata.data_page_ids.push_back(first_data_page_id); 
    new_schema.base_metadata.num_records = 0; 

    new_schema.columns = columns; 

    if (is_fixed_length_record) {
        uint32_t total_size = 0;
        for (const auto& col : columns) {
            total_size += col.size;
        }
        new_schema.base_metadata.fixed_record_size = total_size;
    } else {
        new_schema.base_metadata.fixed_record_size = 0; 
    }

    table_schemas_[table_name] = new_schema; 

    std::cout << "Tabla '" << table_name << "' creada. Primera DATA_PAGE: " << first_data_page_id << std::endl;

    Status save_status = SaveCatalog();
    if (save_status != Status::OK) {
        std::cerr << "Advertencia (CreateTable): Fallo al guardar el catálogo después de crear la tabla '" << table_name << "'." << std::endl;
    }

    return Status::OK;
}

// Crea una nueva tabla a partir de un archivo de texto (esquema inferido).
Status CatalogManager::CreateTableFromPath(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Error (CreateTableFromPath): No se pudo abrir el archivo: " << file_path << std::endl;
        return Status::IO_ERROR;
    }

    std::string table_name = std::filesystem::path(file_path).stem().string();
    if (table_schemas_.count(table_name) > 0) {
        std::cerr << "Error (CreateTableFromPath): La tabla '" << table_name << "' ya existe." << std::endl;
        return Status::DUPLICATE_ENTRY;
    }

    std::string line;
    std::getline(file, line); 
    std::stringstream ss_names(line);
    std::string col_name_str;

    std::getline(file, line); 
    std::stringstream ss_types(line);
    std::string col_type_str;
    std::string sample_data_str;

    std::vector<ColumnMetadata> columns;
    bool is_fixed_length_record = true; 
    
    while (std::getline(ss_names, col_name_str, '#') && std::getline(ss_types, col_type_str, '#')) {
        ColumnMetadata col;
        strncpy(col.name, col_name_str.c_str(), sizeof(col.name) - 1);
        col.name[sizeof(col.name) - 1] = '\0';

        std::string lower_col_type = col_type_str;
        std::transform(lower_col_type.begin(), lower_col_type.end(), lower_col_type.begin(), ::tolower);

        if (lower_col_type == "int") {
            col.type = ColumnType::INT;
            col.size = sizeof(int);
        } else if (lower_col_type.rfind("char(", 0) == 0) { 
            col.type = ColumnType::CHAR;
            size_t start_pos = lower_col_type.find('(');
            size_t end_pos = lower_col_type.find(')');
            if (start_pos != std::string::npos && end_pos != std::string::npos && end_pos > start_pos) {
                col.size = std::stoul(lower_col_type.substr(start_pos + 1, end_pos - start_pos - 1));
            } else {
                col.size = 255; 
                std::cerr << "Advertencia: Formato CHAR inválido para columna " << col.name << ". Usando tamaño por defecto de 255." << std::endl;
            }
        } else if (lower_col_type.rfind("varchar(", 0) == 0) { 
            col.type = ColumnType::VARCHAR;
            is_fixed_length_record = false; 
            size_t start_pos = lower_col_type.find('(');
            size_t end_pos = lower_col_type.find(')');
            if (start_pos != std::string::npos && end_pos != std::string::npos && end_pos > start_pos) {
                col.size = std::stoul(lower_col_type.substr(start_pos + 1, end_pos - start_pos - 1));
            } else {
                col.size = 255; 
                std::cerr << "Advertencia: Formato VARCHAR inválido para columna " << col.name << ". Usando tamaño por defecto de 255." << std::endl;
            }
        } else {
            std::cerr << "Advertencia: Tipo de columna desconocido '" << col_type_str << "' para columna " << col.name << ". Usando INT por defecto." << std::endl;
            col.type = ColumnType::INT;
            col.size = sizeof(int);
        }
        columns.push_back(col);
    }
    file.close();

    return CreateTable(table_name, columns, is_fixed_length_record);
}

// Obtiene el esquema completo de una tabla.
Status CatalogManager::GetTableSchema(const std::string& table_name, FullTableSchema& schema) {
    auto it = table_schemas_.find(table_name);
    if (it == table_schemas_.end()) {
        std::cerr << "Error (GetTableSchema): La tabla '" << table_name << "' no existe en el catálogo." << std::endl;
        return Status::NOT_FOUND;
    }
    schema = it->second; 
    return Status::OK;
}

// Elimina una tabla del catálogo.
Status CatalogManager::DropTable(const std::string& table_name) {
    if (record_manager_ == nullptr) {
        std::cerr << "Error (DropTable): RecordManager no ha sido configurado en CatalogManager." << std::endl;
        return Status::ERROR;
    }

    auto it = table_schemas_.find(table_name);
    if (it == table_schemas_.end()) {
        std::cerr << "Error (DropTable): La tabla '" << table_name << "' no existe." << std::endl;
        return Status::NOT_FOUND;
    }

    FullTableSchema schema_to_delete = it->second;

    for (PageId page_id : schema_to_delete.base_metadata.data_page_ids) {
        Status delete_status = buffer_manager_.DeletePage(page_id);
        if (delete_status != Status::OK) {
            std::cerr << "Advertencia (DropTable): Fallo al eliminar la página de datos " << page_id << " para la tabla '" << table_name << "'." << std::endl;
        }
    }

    table_schemas_.erase(it); 

    std::cout << "Tabla '" << table_name << "' eliminada del catálogo." << std::endl;

    Status save_status = SaveCatalog();
    if (save_status != Status::OK) {
        std::cerr << "Advertencia (DropTable): Fallo al guardar el catálogo después de eliminar la tabla '" << table_name << "'." << std::endl;
    }

    return Status::OK;
}

// Lista todas las tablas registradas en el catálogo.
Status CatalogManager::ListTables(std::vector<std::string>& table_names) {
    table_names.clear();
    for (const auto& pair : table_schemas_) {
        table_names.push_back(pair.first);
    }
    return Status::OK;
}

// Método para cargar todo el catálogo desde las CATALOG_PAGEs.
Status CatalogManager::LoadCatalog() {
    if (record_manager_ == nullptr) {
        std::cerr << "Error (LoadCatalog): RecordManager no ha sido configurado en CatalogManager." << std::endl;
        return Status::ERROR;
    }

    if (catalog_page_id_ == 0) {
        if (buffer_manager_.GetNumBufferedPages() == 0 && buffer_manager_.GetFreeFramesCount() == buffer_manager_.GetPoolSize()) {
            return Status::NOT_FOUND;
        }
        catalog_page_id_ = 1; 
    }

    Byte* catalog_page_data = buffer_manager_.FetchPage(catalog_page_id_);
    if (catalog_page_data == nullptr) {
        std::cerr << "Error (LoadCatalog): No se pudo obtener la CATALOG_PAGE " << catalog_page_id_ << " del BufferManager." << std::endl;
        return Status::NOT_FOUND; 
    }

    BlockHeader header = record_manager_->ReadBlockHeader(catalog_page_data);
    if (header.page_type != PageType::CATALOG_PAGE && header.page_type != PageType::DATA_PAGE) { 
        std::cerr << "Error (LoadCatalog): La página " << catalog_page_id_ << " no es de tipo CATALOG_PAGE o DATA_PAGE. Tipo: " << PageTypeToString(header.page_type) << std::endl;
        buffer_manager_.UnpinPage(catalog_page_id_, false);
        return Status::INVALID_PAGE_TYPE;
    }

    table_schemas_.clear(); 
    next_table_id_ = 1;     

    for (uint32_t i = 0; i < header.num_slots; ++i) {
        SlotDirectoryEntry entry = record_manager_->ReadSlotEntry(catalog_page_data, i);
        if (entry.is_occupied) {
            Record schema_record;
            Status get_record_status = record_manager_->GetRecord(catalog_page_id_, i, schema_record);
            if (get_record_status != Status::OK) {
                std::cerr << "Error (LoadCatalog): Fallo al obtener el registro de esquema del slot " << i << "." << std::endl;
                continue;
            }
            FullTableSchema schema = DeserializeTableSchema(schema_record);
            table_schemas_[schema.base_metadata.table_name] = schema;
            if (schema.base_metadata.table_id >= next_table_id_) {
                next_table_id_ = schema.base_metadata.table_id + 1; 
            }
        }
    }

    buffer_manager_.UnpinPage(catalog_page_id_, false); 
    return Status::OK;
}

// Método para guardar todo el catálogo en las CATALOG_PAGEs.
Status CatalogManager::SaveCatalog() {
    if (record_manager_ == nullptr) {
        std::cerr << "Error (SaveCatalog): RecordManager no ha sido configurado en CatalogManager." << std::endl;
        return Status::ERROR;
    }

    Byte* catalog_page_data = buffer_manager_.FetchPage(catalog_page_id_);
    if (catalog_page_data == nullptr) {
        std::cerr << "Error (SaveCatalog): No se pudo obtener la CATALOG_PAGE " << catalog_page_id_ << " del BufferManager." << std::endl;
        return Status::ERROR;
    }

    Status init_status = record_manager_->InitDataPage(catalog_page_id_); 
    if (init_status != Status::OK) {
        std::cerr << "Error (SaveCatalog): Fallo al inicializar la CATALOG_PAGE antes de guardar." << std::endl;
        return init_status;
    }

    catalog_page_data = buffer_manager_.FetchPage(catalog_page_id_);
    if (catalog_page_data == nullptr) {
        std::cerr << "Error (SaveCatalog): No se pudo obtener la CATALOG_PAGE " << catalog_page_id_ << " después de inicializarla." << std::endl;
        return Status::ERROR;
    }

    for (const auto& pair : table_schemas_) {
        Record catalog_record = SerializeTableSchema(pair.second);
        uint32_t slot_id;
        Status insert_status = record_manager_->InsertRecord(catalog_page_id_, catalog_record, slot_id);
        if (insert_status != Status::OK) {
            std::cerr << "Error (SaveCatalog): Fallo al insertar la metadata de la tabla '" << pair.first << "' en el catálogo." << std::endl;
            buffer_manager_.UnpinPage(catalog_page_id_, true); 
            return insert_status;
        }
    }

    buffer_manager_.UnpinPage(catalog_page_id_, true);

    std::cout << "Catálogo guardado exitosamente." << std::endl;
    return Status::OK;
}

// NUEVO: Método para añadir una nueva PageId de datos a una tabla existente.
Status CatalogManager::AddDataPageToTable(const std::string& table_name, PageId new_data_page_id) {
    auto it = table_schemas_.find(table_name);
    if (it == table_schemas_.end()) {
        std::cerr << "Error (AddDataPageToTable): La tabla '" << table_name << "' no existe." << std::endl;
        return Status::NOT_FOUND;
    }

    it->second.base_metadata.data_page_ids.push_back(new_data_page_id);

    std::cout << "PageId " << new_data_page_id << " añadida a la tabla '" << table_name << "'." << std::endl;

    Status save_status = SaveCatalog();
    if (save_status != Status::OK) {
        std::cerr << "Advertencia (AddDataPageToTable): Fallo al guardar el catálogo después de añadir una página a la tabla '" << table_name << "'." << std::endl;
    }
    return Status::OK;
}

// NUEVO: Método para actualizar el número de registros de una tabla.
Status CatalogManager::UpdateTableNumRecords(const std::string& table_name, uint32_t new_num_records) {
    auto it = table_schemas_.find(table_name);
    if (it == table_schemas_.end()) {
        std::cerr << "Error (UpdateTableNumRecords): La tabla '" << table_name << "' no existe." << std::endl;
        return Status::NOT_FOUND;
    }

    it->second.base_metadata.num_records = new_num_records;

    std::cout << "Número de registros para la tabla '" << table_name << "' actualizado a " << new_num_records << "." << std::endl;

    Status save_status = SaveCatalog();
    if (save_status != Status::OK) {
        std::cerr << "Advertencia (UpdateTableNumRecords): Fallo al guardar el catálogo después de actualizar el conteo de registros para la tabla '" << table_name << "'." << std::endl;
    }
    return Status::OK;
}


// Métodos auxiliares para serializar/deserializar FullTableSchema a/desde Record.
Record CatalogManager::SerializeTableSchema(const FullTableSchema& schema) const {
    std::vector<Byte> data;
    size_t offset = 0;

    // Serializar TableMetadata
    // table_id
    data.resize(offset + sizeof(schema.base_metadata.table_id));
    std::memcpy(data.data() + offset, &schema.base_metadata.table_id, sizeof(schema.base_metadata.table_id));
    offset += sizeof(schema.base_metadata.table_id);

    // table_name
    data.resize(offset + sizeof(schema.base_metadata.table_name));
    std::memcpy(data.data() + offset, schema.base_metadata.table_name, sizeof(schema.base_metadata.table_name));
    offset += sizeof(schema.base_metadata.table_name);

    // is_fixed_length_record
    data.resize(offset + sizeof(schema.base_metadata.is_fixed_length_record));
    std::memcpy(data.data() + offset, &schema.base_metadata.is_fixed_length_record, sizeof(schema.base_metadata.is_fixed_length_record));
    offset += sizeof(schema.base_metadata.is_fixed_length_record);

    // num_records
    data.resize(offset + sizeof(schema.base_metadata.num_records));
    std::memcpy(data.data() + offset, &schema.base_metadata.num_records, sizeof(schema.base_metadata.num_records));
    offset += sizeof(schema.base_metadata.num_records);

    // fixed_record_size
    data.resize(offset + sizeof(schema.base_metadata.fixed_record_size));
    std::memcpy(data.data() + offset, &schema.base_metadata.fixed_record_size, sizeof(schema.base_metadata.fixed_record_size));
    offset += sizeof(schema.base_metadata.fixed_record_size);

    // data_page_ids (vector): primero el tamaño, luego los elementos
    uint32_t num_data_pages = schema.base_metadata.data_page_ids.size();
    data.resize(offset + sizeof(num_data_pages));
    std::memcpy(data.data() + offset, &num_data_pages, sizeof(num_data_pages));
    offset += sizeof(num_data_pages);

    for (PageId page_id : schema.base_metadata.data_page_ids) {
        data.resize(offset + sizeof(page_id));
        std::memcpy(data.data() + offset, &page_id, sizeof(page_id));
        offset += sizeof(page_id);
    }

    // Serializar ColumnMetadata (vector): primero el tamaño, luego los elementos
    uint32_t num_columns = schema.columns.size();
    data.resize(offset + sizeof(num_columns));
    std::memcpy(data.data() + offset, &num_columns, sizeof(num_columns));
    offset += sizeof(num_columns);

    for (const auto& col : schema.columns) {
        // col.name
        data.resize(offset + sizeof(col.name));
        std::memcpy(data.data() + offset, col.name, sizeof(col.name));
        offset += sizeof(col.name);

        // col.type
        data.resize(offset + sizeof(col.type));
        std::memcpy(data.data() + offset, &col.type, sizeof(col.type));
        offset += sizeof(col.type);

        // col.size
        data.resize(offset + sizeof(col.size));
        std::memcpy(data.data() + offset, &col.size, sizeof(col.size));
        offset += sizeof(col.size);
    }

    Record record;
    record.data = data;
    return record;
}

FullTableSchema CatalogManager::DeserializeTableSchema(const Record& record) const {
    FullTableSchema schema;
    const Byte* data_ptr = record.data.data();
    size_t offset = 0;

    // Deserializar TableMetadata
    // table_id
    std::memcpy(&schema.base_metadata.table_id, data_ptr + offset, sizeof(schema.base_metadata.table_id));
    offset += sizeof(schema.base_metadata.table_id);

    // table_name
    std::memcpy(schema.base_metadata.table_name, data_ptr + offset, sizeof(schema.base_metadata.table_name));
    offset += sizeof(schema.base_metadata.table_name);

    // is_fixed_length_record
    std::memcpy(&schema.base_metadata.is_fixed_length_record, data_ptr + offset, sizeof(schema.base_metadata.is_fixed_length_record));
    offset += sizeof(schema.base_metadata.is_fixed_length_record);

    // num_records
    std::memcpy(&schema.base_metadata.num_records, data_ptr + offset, sizeof(schema.base_metadata.num_records));
    offset += sizeof(schema.base_metadata.num_records);

    // fixed_record_size
    std::memcpy(&schema.base_metadata.fixed_record_size, data_ptr + offset, sizeof(schema.base_metadata.fixed_record_size));
    offset += sizeof(schema.base_metadata.fixed_record_size);

    // data_page_ids (vector): primero el tamaño, luego los elementos
    uint32_t num_data_pages;
    std::memcpy(&num_data_pages, data_ptr + offset, sizeof(num_data_pages));
    offset += sizeof(num_data_pages);

    schema.base_metadata.data_page_ids.clear();
    schema.base_metadata.data_page_ids.reserve(num_data_pages);
    for (uint32_t i = 0; i < num_data_pages; ++i) {
        PageId page_id;
        std::memcpy(&page_id, data_ptr + offset, sizeof(page_id));
        offset += sizeof(page_id);
        schema.base_metadata.data_page_ids.push_back(page_id);
    }

    // Deserializar ColumnMetadata (vector): primero el tamaño, luego los elementos
    uint32_t num_columns;
    std::memcpy(&num_columns, data_ptr + offset, sizeof(num_columns));
    offset += sizeof(num_columns);

    schema.columns.clear();
    schema.columns.reserve(num_columns);
    for (uint32_t i = 0; i < num_columns; ++i) {
        ColumnMetadata col;
        // col.name
        std::memcpy(col.name, data_ptr + offset, sizeof(col.name));
        offset += sizeof(col.name);

        // col.type
        std::memcpy(&col.type, data_ptr + offset, sizeof(col.type));
        offset += sizeof(col.type);

        // col.size
        std::memcpy(&col.size, data_ptr + offset, sizeof(col.size));
        offset += sizeof(col.size);
        schema.columns.push_back(col);
    }

    return schema;
}
