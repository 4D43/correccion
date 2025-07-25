// main.cpp
#include <iostream>
#include <string>
#include <limits> // Para std::numeric_limits
#include <vector> // Para std::vector para datos de prueba
#include <memory> // Para std::unique_ptr
#include <cstring> // Para std::memcpy
#include <filesystem> // Para fs::exists, fs::current_path
#include <cctype>   // Para std::tolower
#include <sstream>  // Para std::stringstream
#include <algorithm> // Para std::replace
#include <iomanip>  // Para std::setw, std::setfill

// Incluir los headers de los componentes del SGBD
#include "data_storage/disk_manager.h"
#include "data_storage/block.h"
#include "data_storage/buffer_manager.h"
#include "replacement_policies/lru.h" // Incluir la política LRU
#include "record_manager/record_manager.h" // Incluir el RecordManager
#include "catalog_manager/catalog_manager.h"
#include "include/common.h" // Para Status, BlockSizeType, SectorSizeType, PageType, ColumnMetadata

namespace fs = std::filesystem;

// Punteros globales para los managers (simplificación para el menú)
std::unique_ptr<DiskManager> g_disk_manager = nullptr;
std::unique_ptr<BufferManager> g_buffer_manager = nullptr;
std::unique_ptr<RecordManager> g_record_manager = nullptr;
std::unique_ptr<CatalogManager> g_catalog_manager = nullptr;

// Función auxiliar para limpiar el buffer de entrada
void ClearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

// Función auxiliar para obtener entrada numérica con validación
template<typename T>
T GetNumericInput(const std::string& prompt) {
    T value;
    while (true) {
        std::cout << prompt;
        std::cin >> value;
        if (std::cin.fail()) {
            std::cout << "Entrada inválida. Por favor, ingrese un número." << std::endl;
            std::cin.clear();
            ClearInputBuffer();
        } else {
            ClearInputBuffer();
            break;
        }
    }
    return value;
}

// Función auxiliar para obtener entrada de cadena
std::string GetStringInput(const std::string& prompt) {
    std::string value;
    std::cout << prompt;
    std::getline(std::cin, value);
    return value;
}

// Función auxiliar para transformar delimitadores
std::string TransformDelimiters(const std::string& input_string) {
    std::string transformed_string = input_string;
    std::replace(transformed_string.begin(), transformed_string.end(), ',', '#');
    std::replace(transformed_string.begin(), transformed_string.end(), '\t', '#');
    return transformed_string;
}

// Función auxiliar para obtener el tipo de columna del usuario
ColumnType GetColumnTypeFromUser() {
    std::cout << "  Seleccione el tipo de dato:" << std::endl;
    std::cout << "    0. INT" << std::endl;
    std::cout << "    1. CHAR" << std::endl;
    std::cout << "2. VARCHAR" << std::endl;
    int type_choice = GetNumericInput<int>("  Opción de tipo: ");
    switch (type_choice) {
        case 0: return ColumnType::INT;
        case 1: return ColumnType::CHAR;
        case 2: return ColumnType::VARCHAR;
        default:
            std::cout << "Tipo inválido. Usando INT por defecto." << std::endl;
            return ColumnType::INT;
    }
}

// --- Funciones de Menú ---

void DisplayMainMenu() {
    std::cout << "\n--- Menú Principal del SGBD ---" << std::endl;
    std::cout << "1. Gestión del Disco" << std::endl;
    std::cout << "2. Gestión del Buffer Pool" << std::endl;
    std::cout << "3. Gestión de Datos (Tablas y Registros)" << std::endl;
    std::cout << "4. Gestión de Metadatos (Catálogo)" << std::endl;
    std::cout << "5. Gestión de Índices [No implementado]" << std::endl;
    std::cout << "6. Procesador de Consultas [No implementado]" << std::endl;
    std::cout << "7. Salir" << std::endl;
    std::cout << "Ingrese su opción: ";
}

void DisplayDiskManagementMenu() {
    std::cout << "\n--- Menú: Gestión del Disco ---" << std::endl;
    std::cout << "1. Ver Estado del Disco (Resumen)" << std::endl; // Cambiado
    std::cout << "2. Crear Nuevo Disco" << std::endl;
    std::cout << "3. Cargar Disco Existente" << std::endl;
    std::cout << "4. Eliminar Disco" << std::endl;
    std::cout << "5. Ver Información Detallada del Disco" << std::endl; // NUEVO
    std::cout << "6. Ver Mapa de Estado de Bloques" << std::endl; // NUEVO
    std::cout << "7. Ver Mapeo Lógico a Físico" << std::endl; // NUEVO
    std::cout << "8. Volver al Menú Principal" << std::endl; // Cambiado
    std::cout << "Ingrese su opción: ";
}

void DisplayBufferPoolManagementMenu() {
    std::cout << "\n--- Menú: Gestión del Buffer Pool ---" << std::endl;
    std::cout << "1. Ver Estado del Buffer" << std::endl;
    std::cout << "2. Flushar Todas las Páginas Sucias" << std::endl;
    std::cout << "3. Ver Tabla de Páginas del Buffer Pool" << std::endl; // NUEVO
    std::cout << "4. Cambiar Tamaño del Buffer Pool [No implementado]" << std::endl;
    std::cout << "5. Cambiar Algoritmo de Reemplazo [No implementado]" << std::endl;
    std::cout << "6. Volver al Menú Principal" << std::endl;
    std::cout << "Ingrese su opción: ";
}

void DisplayDataManagementMenu() {
    std::cout << "\n--- Menú: Gestión de Datos (Tablas y Registros) ---" << std::endl;
    std::cout << "1. Insertar Registro" << std::endl;
    std::cout << "2. Seleccionar Registros" << std::endl;
    std::cout << "3. Actualizar Registro" << std::endl;
    std::cout << "4. Eliminar Registro" << std::endl;
    std::cout << "5. Ver Contenido de Bloque (Debug)" << std::endl;
    std::cout << "6. Volver al Menú Principal" << std::endl;
    std::cout << "Ingrese su opción: ";
}

void DisplayCatalogManagementMenu() {
    std::cout << "\n--- Menú: Gestión de Metadatos (Catálogo) ---" << std::endl;
    std::cout << "1. Crear Nueva Tabla (Formulario)" << std::endl;
    std::cout << "2. Crear Nueva Tabla (Desde Archivo)" << std::endl;
    std::cout << "3. Ver Esquema de Tabla" << std::endl;
    std::cout << "4. Listar Tablas Existentes" << std::endl;
    std::cout << "5. Eliminar Tabla" << std::endl;
    std::cout << "6. Volver al Menú Principal" << std::endl;
    std::cout << "Ingrese su opción: ";
}


// --- Implementación de Funciones de Menú ---

void ViewDiskStatus() {
    if (!g_disk_manager) {
        std::cout << "No hay un disco cargado o creado." << std::endl;
        return;
    }
    std::cout << "\n--- Estado del Disco: " << g_disk_manager->GetDiskName() << " ---" << std::endl;
    std::cout << "Parámetros del Disco:" << std::endl;
    std::cout << "  Platos: " << g_disk_manager->GetNumPlatters() << std::endl;
    std::cout << "  Superficies por Plato: " << g_disk_manager->GetNumSurfacesPerPlatter() << std::endl;
    std::cout << "  Cilindros: " << g_disk_manager->GetNumCylinders() << std::endl;
    std::cout << "  Sectores por Pista: " << g_disk_manager->GetNumSectorsPerTrack() << std::endl;
    std::cout << "  Tamaño de Bloque Lógico: " << g_disk_manager->GetBlockSize() << " bytes" << std::endl;
    std::cout << "  Tamaño de Sector Físico: " << g_disk_manager->GetSectorSize() << " bytes" << std::endl;
    std::cout << "  Sectores Físicos por Bloque Lógico: " << g_disk_manager->GetSectorsPerBlock() << std::endl;
    std::cout << "Uso del Espacio:" << std::endl;
    std::cout << "  Total de Sectores Físicos: " << g_disk_manager->GetTotalPhysicalSectors() << std::endl;
    std::cout << "  Sectores Físicos Libres: " << g_disk_manager->GetFreePhysicalSectors() << std::endl;
    std::cout << "  Total de Bloques Lógicos: " << g_disk_manager->GetTotalLogicalBlocks() << std::endl;
    std::cout << "  Bloques Lógicos Libres: " << g_disk_manager->GetFreePhysicalSectors() / g_disk_manager->GetSectorsPerBlock() << std::endl;
}

// NUEVO: Función para ver información detallada del disco
void ViewDetailedDiskInfo() {
    if (!g_disk_manager) {
        std::cout << "No hay un disco cargado o creado." << std::endl;
        return;
    }
    std::cout << "\n--- Información Detallada del Disco: " << g_disk_manager->GetDiskName() << " ---" << std::endl;

    uint64_t total_capacity_bytes = g_disk_manager->GetTotalCapacityBytes();
    std::cout << "Capacidad Total del Disco: " << total_capacity_bytes << " bytes" << std::endl;

    uint32_t total_logical_blocks = g_disk_manager->GetTotalLogicalBlocks();
    uint32_t occupied_logical_blocks = g_disk_manager->GetOccupiedLogicalBlocks();
    uint32_t free_logical_blocks = total_logical_blocks - occupied_logical_blocks;

    std::cout << "Total de Bloques Lógicos: " << total_logical_blocks << std::endl;
    std::cout << "Bloques Lógicos Ocupados: " << occupied_logical_blocks << std::endl;
    std::cout << "Bloques Lógicos Libres: " << free_logical_blocks << std::endl;
    std::cout << "Porcentaje de Ocupación: " << std::fixed << std::setprecision(2)
              << g_disk_manager->GetDiskUsagePercentage() << "%" << std::endl;
}

// NUEVO: Función para ver el mapa de estado de bloques
void ViewBlockStatusMap() {
    if (!g_disk_manager) {
        std::cout << "No hay un disco cargado o creado." << std::endl;
        return;
    }
    g_disk_manager->PrintBlockStatusMap();
}

// NUEVO: Función para ver el mapeo lógico a físico
void ViewLogicalToPhysicalMap() {
    if (!g_disk_manager) {
        std::cout << "No hay un disco cargado o creado." << std::endl;
        return;
    }
    g_disk_manager->PrintLogicalToPhysicalMap();
}


void CreateNewDisk() {
    std::string disk_name;
    uint32_t num_platters;
    uint32_t num_surfaces_per_platter;
    uint32_t num_cylinders;
    uint32_t num_sectors_per_track;
    BlockSizeType block_size;
    SectorSizeType sector_size;
    uint32_t buffer_pool_size;

    std::cout << "\n--- Crear Nuevo Disco ---" << std::endl;
    std::cout << "Advertencia: Esto eliminará cualquier disco existente con el mismo nombre y sus datos." << std::endl;

    std::cout << "Ingrese el nombre del disco: ";
    std::getline(std::cin, disk_name);

    // Validar que el número de platos sea par
    while (true) {
        num_platters = GetNumericInput<uint32_t>("Ingrese el número de platos (debe ser par, ej. 4): ");
        if (num_platters % 2 == 0) {
            break;
        }
        std::cout << "Error: El número de platos debe ser un número par." << std::endl;
    }
    
    num_surfaces_per_platter = GetNumericInput<uint32_t>("Ingrese el número de superficies por plato (ej. 2): ");
    num_cylinders = GetNumericInput<uint32_t>("Ingrese el número de cilindros (ej. 10): ");
    num_sectors_per_track = GetNumericInput<uint32_t>("Ingrese el número de sectores por pista (ej. 4): ");

    while (true) {
        block_size = GetNumericInput<BlockSizeType>("Ingrese el tamaño de un bloque lógico en bytes (ej. 4096): ");
        sector_size = GetNumericInput<SectorSizeType>("Ingrese el tamaño de un sector físico en bytes (ej. 512): ");
        if (block_size % sector_size != 0) {
            std::cout << "Error: El tamaño del bloque (" << block_size << ") debe ser un múltiplo del tamaño del sector (" << sector_size << ")." << std::endl;
        } else {
            break;
        }
    }

    buffer_pool_size = GetNumericInput<uint32_t>("Ingrese el tamaño del Buffer Pool (número de frames, ej. 10): ");

    try {
        g_disk_manager = std::make_unique<DiskManager>(disk_name, num_platters, num_surfaces_per_platter,
                                                       num_cylinders, num_sectors_per_track,
                                                       block_size, sector_size, true);
        Status status = g_disk_manager->CreateDiskStructure();
        if (status != Status::OK) {
            std::cerr << "Error al crear la estructura del disco: " << StatusToString(status) << std::endl;
            g_disk_manager.reset();
            return;
        }
        std::cout << "Disco '" << disk_name << "' creado exitosamente." << std::endl;

        g_buffer_manager = std::make_unique<BufferManager>(*g_disk_manager, buffer_pool_size, g_disk_manager->GetBlockSize(), std::make_unique<LRUReplacementPolicy>());
        
        // Inicializar RecordManager y CatalogManager con constructores simplificados
        // Ahora sus constructores solo requieren BufferManager, resolviendo la dependencia circular inicial.
        g_record_manager = std::make_unique<RecordManager>(*g_buffer_manager);
        g_catalog_manager = std::make_unique<CatalogManager>(*g_buffer_manager);

        // Establecer las referencias cruzadas usando los setters después de la construcción
        g_record_manager->SetCatalogManager(*g_catalog_manager);
        g_catalog_manager->SetRecordManager(*g_record_manager);

        g_catalog_manager->InitCatalog();

    } catch (const std::exception& e) {
        std::cerr << "Error al crear el disco: " << e.what() << std::endl;
        g_disk_manager.reset();
        g_buffer_manager.reset();
        g_record_manager.reset();
        g_catalog_manager.reset();
    }
}

void LoadExistingDisk() {
    std::string disk_name;
    std::cout << "\n--- Cargar Disco Existente ---" << std::endl;
    std::cout << "Ingrese el nombre del disco a cargar: ";
    std::getline(std::cin, disk_name);

    fs::path disk_path = fs::path("Discos") / disk_name;
    if (!fs::exists(disk_path)) {
        std::cerr << "Error: El disco '" << disk_name << "' no existe en " << disk_path << std::endl;
        return;
    }

    try {
        g_disk_manager = std::make_unique<DiskManager>(disk_name, 1, 1, 1, 1, 512, 512, false);
        Status status = g_disk_manager->LoadDiskMetadata();
        if (status != Status::OK) {
            std::cerr << "Error al cargar los metadatos del disco: " << StatusToString(status) << std::endl;
            g_disk_manager.reset();
            return;
        }
        std::cout << "Disco '" << disk_name << "' cargado exitosamente." << std::endl;

        uint32_t buffer_pool_size = GetNumericInput<uint32_t>("Ingrese el tamaño del Buffer Pool para esta sesión (ej. 10): ");
        g_buffer_manager = std::make_unique<BufferManager>(*g_disk_manager, buffer_pool_size, g_disk_manager->GetBlockSize(), std::make_unique<LRUReplacementPolicy>());
        
        // Inicializar RecordManager y CatalogManager con constructores simplificados
        // Ahora sus constructores solo requieren BufferManager, resolviendo la dependencia circular inicial.
        g_record_manager = std::make_unique<RecordManager>(*g_buffer_manager);
        g_catalog_manager = std::make_unique<CatalogManager>(*g_buffer_manager);

        // Establecer las referencias cruzadas usando los setters después de la construcción
        g_record_manager->SetCatalogManager(*g_catalog_manager);
        g_catalog_manager->SetRecordManager(*g_record_manager);

        g_catalog_manager->InitCatalog();

    } catch (const std::exception& e) {
        std::cerr << "Error al cargar el disco: " << e.what() << std::endl;
        g_disk_manager.reset();
        g_buffer_manager.reset();
        g_record_manager.reset();
        g_catalog_manager.reset();
    }
}

void DeleteDisk() {
    std::string disk_name_to_delete;
    std::cout << "\n--- Eliminar Disco ---" << std::endl;
    std::cout << "Advertencia: Esto eliminará permanentemente el disco y todos sus datos." << std::endl;
    std::cout << "Ingrese el nombre del disco a eliminar: ";
    std::getline(std::cin, disk_name_to_delete);

    fs::path disk_path = fs::path("Discos") / disk_name_to_delete;
    if (fs::exists(disk_path)) {
        try {
            fs::remove_all(disk_path);
            std::cout << "Disco '" << disk_name_to_delete << "' eliminado exitosamente." << std::endl;
            // Si el disco eliminado es el actualmente cargado, resetear los managers
            if (g_disk_manager && g_disk_manager->GetDiskName() == disk_name_to_delete) {
                g_disk_manager.reset();
                g_buffer_manager.reset();
                g_record_manager.reset();
                g_catalog_manager.reset();
                std::cout << "El disco actual ha sido eliminado, managers reseteados." << std::endl;
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Error al eliminar el disco: " << e.what() << std::endl;
        }
    } else {
        std::cout << "El disco '" << disk_name_to_delete << "' no existe." << std::endl;
    }
}


void ViewBufferStatus() {
    if (!g_buffer_manager) {
        std::cout << "No hay un Buffer Manager inicializado. Cargue o cree un disco primero." << std::endl;
        return;
    }
    std::cout << "\n--- Estado del Buffer Pool ---" << std::endl;
    std::cout << "Tamaño total del Buffer Pool: " << g_buffer_manager->GetPoolSize() << " frames" << std::endl;
    std::cout << "Frames libres: " << g_buffer_manager->GetFreeFramesCount() << std::endl;
    std::cout << "Páginas actualmente en Buffer: " << g_buffer_manager->GetNumBufferedPages() << std::endl;
}

void FlushAllPages() {
    if (!g_buffer_manager) {
        std::cout << "No hay un Buffer Manager inicializado. Cargue o cree un disco primero." << std::endl;
        return;
    }
    std::cout << "\n--- Flushando todas las páginas sucias ---" << std::endl;
    Status status = g_buffer_manager->FlushAllPages();
    if (status == Status::OK) {
        std::cout << "Todas las páginas sucias han sido flusheadas exitosamente." << std::endl;
    } else {
        std::cerr << "Error al flushar todas las páginas: " << StatusToString(status) << std::endl;
    }
}

// NUEVO: Función para ver la tabla de páginas del Buffer Pool
void ViewBufferPoolTable() {
    if (!g_buffer_manager) {
        std::cout << "No hay un Buffer Manager inicializado. Cargue o cree un disco primero." << std::endl;
        return;
    }
    std::cout << "\n--- Tabla de Páginas del Buffer Pool ---" << std::endl;
    std::cout << std::left << std::setw(8) << "FrameId"
              << std::setw(10) << "PageId"
              << std::setw(10) << "PinCount"
              << std::setw(8) << "Dirty"
              << std::setw(8) << "Valid" << std::endl;
    std::cout << std::string(54, '-') << std::endl;

    const auto& frames = g_buffer_manager->GetFrames();
    for (FrameId i = 0; i < frames.size(); ++i) {
        const Page& frame_info = frames[i];
        std::cout << std::left << std::setw(8) << i
                  << std::setw(10) << (frame_info.is_valid ? std::to_string(frame_info.page_id) : "N/A")
                  << std::setw(10) << frame_info.pin_count
                  << std::setw(8) << (frame_info.is_dirty ? "Yes" : "No")
                  << std::setw(8) << (frame_info.is_valid ? "Yes" : "No") << std::endl;
    }
}


// --- Funciones de Gestión de Datos (Tablas y Registros) ---
// Estas ahora interactúan con el CatalogManager para obtener el PageId de la tabla.

// Función para preparar los datos del registro (delimitadores y padding)
std::string PrepareRecordData(const std::string& input_content, const FullTableSchema& schema) {
    std::string processed_content = input_content;

    // 1. Transformar delimitadores
    processed_content = TransformDelimiters(processed_content);

    // 2. Si es de longitud fija, aplicar padding
    if (schema.base_metadata.is_fixed_length_record) {
        if (processed_content.length() > schema.base_metadata.fixed_record_size) {
            std::cerr << "Advertencia: El contenido del registro excede el tamaño fijo de la tabla. Se truncará." << std::endl;
            processed_content = processed_content.substr(0, schema.base_metadata.fixed_record_size);
        } else if (processed_content.length() < schema.base_metadata.fixed_record_size) {
            processed_content.resize(schema.base_metadata.fixed_record_size, ' '); // Rellenar con espacios
        }
    }
    // Para VARCHAR, la validación de tamaño máximo se haría en un QueryProcessor más completo
    // y el almacenamiento de la longitud real se haría dentro del formato del registro.
    // Por ahora, el RecordManager solo almacena el vector<Byte> tal cual.

    return processed_content;
}


void InsertRecord() {
    if (!g_record_manager || !g_catalog_manager) {
        std::cout << "Managers no inicializados. Cargue o cree un disco primero." << std::endl;
        return;
    }
    std::cout << "\n--- Insertar Registro ---" << std::endl;
    std::string table_name = GetStringInput("Ingrese el nombre de la tabla donde insertar el registro: ");

    FullTableSchema schema;
    Status get_schema_status = g_catalog_manager->GetTableSchema(table_name, schema);
    if (get_schema_status != Status::OK) {
        return; // Error ya impreso por GetTableSchema
    }

    std::string content;
    std::cout << "Ingrese el contenido del registro (texto, use ',' o '\\t' como delimitadores): ";
    std::getline(std::cin, content);

    // Preparar el contenido del registro (delimitadores y padding si es fijo)
    std::string processed_content = PrepareRecordData(content, schema);

    Record new_rec;
    new_rec.data.assign(processed_content.begin(), processed_content.end()); // Copiar contenido a vector<Byte>

    uint32_t slot_id;
    Status status = Status::ERROR;
    PageId target_page_id = 0;

    // 1. Intentar insertar en una página existente con espacio
    for (PageId page_id : schema.base_metadata.data_page_ids) {
        BlockSizeType free_space;
        Status get_space_status = g_record_manager->GetFreeSpace(page_id, free_space);
        if (get_space_status == Status::OK && free_space >= new_rec.data.size() + sizeof(SlotDirectoryEntry)) {
            // Asumimos que el slot directory entry también necesita espacio
            status = g_record_manager->InsertRecord(page_id, new_rec, slot_id);
            if (status == Status::OK) {
                target_page_id = page_id;
                break; // Registro insertado, salir del bucle
            }
        }
    }

    // 2. Si no se encontró espacio en ninguna página existente, crear una nueva página
    if (status != Status::OK) {
        std::cout << "No hay espacio en las páginas existentes. Creando nueva página para la tabla..." << std::endl;
        PageId new_data_page_id;
        // Corrected: Use g_buffer_manager->NewPage
        Byte* new_page_data = g_buffer_manager->NewPage(new_data_page_id, PageType::DATA_PAGE);
        if (new_page_data == nullptr) {
            std::cerr << "Error: Fallo al crear una nueva página de datos para la tabla." << std::endl;
            return;
        }
        // Inicializar la nueva página
        Status init_status = g_record_manager->InitDataPage(new_data_page_id);
        if (init_status != Status::OK) {
            std::cerr << "Error: Fallo al inicializar la nueva página de datos." << std::endl;
            g_buffer_manager->DeletePage(new_data_page_id); // Limpiar si falla
            return;
        }
        // Añadir la nueva página al esquema de la tabla en el catálogo
        Status add_page_status = g_catalog_manager->AddDataPageToTable(table_name, new_data_page_id);
        if (add_page_status != Status::OK) {
            std::cerr << "Error: Fallo al añadir la nueva página al catálogo de la tabla." << std::endl;
            // Podríamos intentar eliminar la página recién creada aquí, pero podría ser inconsistente.
            return;
        }
        
        // Intentar insertar el registro en la nueva página
        status = g_record_manager->InsertRecord(new_data_page_id, new_rec, slot_id);
        if (status == Status::OK) {
            target_page_id = new_data_page_id;
        }
    }

    if (status == Status::OK) {
        std::cout << "Registro insertado exitosamente en Page " << target_page_id << ", Slot " << slot_id << "." << std::endl;
        // Actualizar num_records en TableMetadata.
        schema.base_metadata.num_records++; // Incrementar el conteo en la copia local
        g_catalog_manager->UpdateTableNumRecords(table_name, schema.base_metadata.num_records); // Persistir el cambio
    } else {
        std::cerr << "Error al insertar registro: " << StatusToString(status) << std::endl;
    }
}

void SelectRecords() {
    if (!g_record_manager || !g_catalog_manager) {
        std::cout << "Managers no inicializados. Cargue o cree un disco primero." << std::endl;
        return;
    }
    std::cout << "\n--- Seleccionar Registros ---" << std::endl;
    std::string table_name = GetStringInput("Ingrese el nombre de la tabla para seleccionar registros: ");

    FullTableSchema schema;
    Status get_schema_status = g_catalog_manager->GetTableSchema(table_name, schema);
    if (get_schema_status != Status::OK) {
        return;
    }

    if (schema.base_metadata.data_page_ids.empty()) {
        std::cout << "La tabla '" << table_name << "' no tiene páginas de datos asignadas." << std::endl;
        return;
    }

    uint32_t total_records_found = 0;
    std::cout << "Registros en la tabla '" << table_name << "':" << std::endl;

    for (PageId page_id : schema.base_metadata.data_page_ids) {
        // Corrected: Use g_buffer_manager->FetchPage
        Byte* page_data_for_read = g_buffer_manager->FetchPage(page_id);
        if (page_data_for_read == nullptr) {
            std::cerr << "Advertencia: No se pudo obtener la página " << page_id << " para seleccionar registros. Saltando esta página." << std::endl;
            continue;
        }
        BlockHeader header_for_read;
        std::memcpy(&header_for_read, page_data_for_read, sizeof(BlockHeader));
        g_buffer_manager->UnpinPage(page_id, false); // Unpin inmediatamente

        std::cout << "  --- Página " << page_id << " (Slots: " << header_for_read.num_slots << ") ---" << std::endl;
        for (uint32_t i = 0; i < header_for_read.num_slots; ++i) {
            Record fetched_rec;
            Status status = g_record_manager->GetRecord(page_id, i, fetched_rec);
            if (status == Status::OK) {
                std::cout << "    Page " << page_id << ", Slot " << i << ": " << std::string(fetched_rec.data.begin(), fetched_rec.data.end()) << std::endl;
                total_records_found++;
            } else if (status == Status::NOT_FOUND) {
                // std::cout << "    Page " << page_id << ", Slot " << i << ": [Vacío]" << std::endl;
            } else {
                std::cerr << "    Error al leer Page " << page_id << ", Slot " << i << ": " << StatusToString(status) << std::endl;
            }
        }
    }
    std::cout << "Total de registros encontrados en la tabla '" << table_name << "': " << total_records_found << std::endl;
}

void UpdateRecord() {
    if (!g_record_manager || !g_catalog_manager) {
        std::cout << "Managers no inicializados. Cargue o cree un disco primero." << std::endl;
        return;
    }
    std::cout << "\n--- Actualizar Registro ---" << std::endl;
    std::string table_name = GetStringInput("Ingrese el nombre de la tabla del registro a actualizar: ");

    FullTableSchema schema;
    Status get_schema_status = g_catalog_manager->GetTableSchema(table_name, schema);
    if (get_schema_status != Status::OK) {
        return;
    }

    if (schema.base_metadata.data_page_ids.empty()) {
        std::cout << "La tabla '" << table_name << "' no tiene páginas de datos asignadas." << std::endl;
        return;
    }

    PageId target_page_id = GetNumericInput<PageId>("Ingrese el PageId del registro a actualizar: ");
    uint32_t slot_id = GetNumericInput<uint32_t>("Ingrese el SlotId del registro a actualizar: ");
    std::string content;
    std::cout << "Ingrese el nuevo contenido del registro (texto, use ',' o '\\t' como delimitadores): ";
    std::getline(std::cin, content);

    // Preparar el contenido del registro (delimitadores y padding si es fijo)
    std::string processed_content = PrepareRecordData(content, schema);

    Record updated_rec;
    updated_rec.data.assign(processed_content.begin(), processed_content.end());

    // Verificar si el PageId proporcionado pertenece a la tabla
    bool page_found_in_table = false;
    for (PageId p_id : schema.base_metadata.data_page_ids) {
        if (p_id == target_page_id) {
            page_found_in_table = true;
            break;
        }
    }

    if (!page_found_in_table) {
        std::cerr << "Error: PageId " << target_page_id << " no pertenece a la tabla '" << table_name << "'." << std::endl;
        return;
    }

    // Para la actualización, necesitamos saber si el tamaño del registro cambia y si eso afecta el conteo.
    // Una actualización que reinserta un registro podría cambiar el slot_id y potencialmente el num_records.
    // Por simplicidad, asumiremos que UpdateRecord no cambia el num_records total de la tabla.
    // Si UpdateRecord borra y reinserta, el num_records se mantendría igual (un borrado, una inserción).
    Status status = g_record_manager->UpdateRecord(target_page_id, slot_id, updated_rec);
    if (status == Status::OK) {
        std::cout << "Registro actualizado exitosamente en Page " << target_page_id << ", Slot " << slot_id << "." << std::endl;
    } else {
        std::cerr << "Error al actualizar registro: " << StatusToString(status) << std::endl;
    }
}

void DeleteRecord() {
    if (!g_record_manager || !g_catalog_manager) {
        std::cout << "Managers no inicializados. Cargue o cree un disco primero." << std::endl;
        return;
    }
    std::cout << "\n--- Eliminar Registro ---" << std::endl;
    std::string table_name = GetStringInput("Ingrese el nombre de la tabla del registro a eliminar: ");

    FullTableSchema schema;
    Status get_schema_status = g_catalog_manager->GetTableSchema(table_name, schema);
    if (get_schema_status != Status::OK) {
        return;
    }

    if (schema.base_metadata.data_page_ids.empty()) {
        std::cout << "La tabla '" << table_name << "' no tiene páginas de datos asignadas." << std::endl;
        return;
    }

    PageId target_page_id = GetNumericInput<PageId>("Ingrese el PageId del registro a eliminar: ");
    uint32_t slot_id = GetNumericInput<uint32_t>("Ingrese el SlotId del registro a eliminar: ");

    // Verificar si el PageId proporcionado pertenece a la tabla
    bool page_found_in_table = false;
    for (PageId p_id : schema.base_metadata.data_page_ids) {
        if (p_id == target_page_id) {
            page_found_in_table = true;
            break;
        }
    }

    if (!page_found_in_table) {
        std::cerr << "Error: PageId " << target_page_id << " no pertenece a la tabla '" << table_name << "'." << std::endl;
        return;
    }

    Status status = g_record_manager->DeleteRecord(target_page_id, slot_id);
    if (status == Status::OK) {
        std::cout << "Registro eliminado exitosamente de Page " << target_page_id << ", Slot " << slot_id << "." << std::endl;
        // Actualizar num_records en TableMetadata.
        if (schema.base_metadata.num_records > 0) {
            schema.base_metadata.num_records--; // Decrementar el conteo en la copia local
            g_catalog_manager->UpdateTableNumRecords(table_name, schema.base_metadata.num_records); // Persistir el cambio
        }
    } else {
        std::cerr << "Error al eliminar registro: " << StatusToString(status) << std::endl;
    }
}

void ViewBlockContentDebug() {
    if (!g_buffer_manager || !g_record_manager) {
        std::cout << "Managers no inicializados. Cargue o cree un disco primero." << std::endl;
        return;
    }
    std::cout << "\n--- Ver Contenido de Bloque (Debug) ---" << std::endl;
    PageId page_id = GetNumericInput<PageId>("Ingrese el PageId del bloque a inspeccionar: ");

    // Fetch the page to ensure it's in buffer and get a pointer to its data in the buffer pool
    // Corrected: Use g_buffer_manager->GetPageDataInPool
    Byte* block_data = g_buffer_manager->GetPageDataInPool(page_id);
    if (block_data == nullptr) {
        // If not in buffer, try to fetch it to bring it into the buffer
        // Corrected: Use g_buffer_manager->FetchPage
        block_data = g_buffer_manager->FetchPage(page_id);
        if (block_data == nullptr) {
            std::cerr << "Error: No se pudo obtener los datos del bloque " << page_id << " (no está en buffer y no se pudo cargar)." << std::endl;
            return;
        }
    }

    std::cout << "Datos brutos del bloque " << page_id << " (primeros 128 bytes):" << std::endl;
    for (int i = 0; i < std::min((int)g_buffer_manager->GetBlockSize(), 128); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)(unsigned char)block_data[i] << " ";
        if ((i + 1) % 16 == 0) {
            std::cout << std::endl;
        }
    }
    std::cout << std::dec << std::endl; // Volver a decimal

    // Intentar leer la cabecera si es una DATA_PAGE
    BlockHeader header;
    std::memcpy(&header, block_data, sizeof(BlockHeader));

    std::cout << "\n--- Interpretación de la Cabecera del Bloque " << page_id << " ---" << std::endl;
    std::cout << "  Page ID: " << header.page_id << std::endl;
    std::cout << "  Page Type: " << PageTypeToString(header.page_type) << std::endl;
    std::cout << "  Data End Offset: " << header.data_end_offset << std::endl;
    std::cout << "  Number of Slots: " << header.num_slots << std::endl;
    std::cout << "  Header + Slot Directory Size: " << header.header_and_slot_directory_size << std::endl;

    if (header.page_type == PageType::DATA_PAGE || header.page_type == PageType::CATALOG_PAGE) { // También para CATALOG_PAGE
        std::cout << "\n--- Directorio de Slots (" << PageTypeToString(header.page_type) << ") ---" << std::endl;
        BlockSizeType slot_directory_entry_size = sizeof(SlotDirectoryEntry);
        BlockSizeType slot_directory_start_offset = g_record_manager->GetSlotDirectoryStartOffset();

        for (uint32_t i = 0; i < header.num_slots; ++i) {
            SlotDirectoryEntry entry;
            // Directly read slot entry from page_data.
            std::memcpy(&entry, block_data + slot_directory_start_offset + (i * slot_directory_entry_size), slot_directory_entry_size);
            std::cout << "  Slot " << i << ": Offset=" << entry.offset
                      << ", Length=" << entry.length
                      << ", Occupied=" << (entry.is_occupied ? "Sí" : "No") << std::endl;

            if (entry.is_occupied) {
                // Mostrar contenido del registro si está ocupado
                // Ensure record_content doesn't read past block boundaries
                size_t actual_length = std::min((size_t)entry.length, (size_t)(g_buffer_manager->GetBlockSize() - entry.offset));
                std::string record_content(block_data + entry.offset, block_data + entry.offset + actual_length);
                std::cout << "    Contenido (parcial): " << record_content.substr(0, std::min((size_t)50, record_content.length())) << "..." << std::endl;
            }
        }
    }
    // Don't forget to unpin the page after fetching it for debug view
    g_buffer_manager->UnpinPage(page_id, false);
}

// --- Funciones de Gestión de Catálogo ---

void CreateNewTableForm() { // Renombrado para diferenciar del path-based
    if (!g_catalog_manager) {
        std::cout << "No hay un Catalog Manager inicializado. Cargue o cree un disco primero." << std::endl;
        return;
    }
    std::cout << "\n--- Crear Nueva Tabla (Formulario) ---" << std::endl;
    std::string table_name = GetStringInput("Ingrese el nombre de la nueva tabla: ");

    int num_columns = GetNumericInput<int>("Ingrese el número de columnas: ");
    if (num_columns <= 0) {
        std::cout << "El número de columnas debe ser mayor que 0." << std::endl;
        return;
    }

    std::vector<ColumnMetadata> columns;
    bool has_varchar = false; // Para determinar si la tabla es de longitud fija o variable
    for (int i = 0; i < num_columns; ++i) {
        ColumnMetadata col;
        std::string col_name = GetStringInput("  Ingrese el nombre de la columna " + std::to_string(i + 1) + ": ");
        strncpy(col.name, col_name.c_str(), sizeof(col.name) - 1);
        col.name[sizeof(col.name) - 1] = '\0';

        col.type = GetColumnTypeFromUser();
        
        switch (col.type) {
            case ColumnType::INT: col.size = sizeof(int); break;
            case ColumnType::CHAR: col.size = GetNumericInput<uint32_t>("  Ingrese la longitud fija para CHAR (ej. 10): "); break;
            case ColumnType::VARCHAR:
                col.size = GetNumericInput<uint32_t>("  Ingrese la longitud máxima para VARCHAR (ej. 255): ");
                has_varchar = true;
                break;
            default: col.size = 0; break; // Debería ser manejado por GetColumnTypeFromUser
        }
        columns.push_back(col);
    }

    // Determinar si la tabla es de longitud fija o variable
    bool is_fixed_length = !has_varchar;
    if (has_varchar) {
        std::cout << "La tabla contiene columnas VARCHAR, por lo tanto, será de longitud variable." << std::endl;
    } else {
        char record_type_choice = GetStringInput("¿Los registros de esta tabla son de longitud fija? (s/n): ")[0];
        is_fixed_length = (std::tolower(record_type_choice) == 's');
        if (!is_fixed_length) {
             std::cout << "Advertencia: Aunque no hay VARCHAR, ha elegido longitud variable. Esto es válido." << std::endl;
        }
    }

    Status status = g_catalog_manager->CreateTable(table_name, columns, is_fixed_length);
    if (status == Status::OK) {
        std::cout << "Tabla '" << table_name << "' creada exitosamente." << std::endl;
    } else {
        std::cerr << "Error al crear la tabla: " << StatusToString(status) << std::endl;
    }
}

void CreateNewTableFromFile() {
    if (!g_catalog_manager) {
        std::cout << "No hay un Catalog Manager inicializado. Cargue o cree un disco primero." << std::endl;
        return;
    }
    std::cout << "\n--- Crear Nueva Tabla (Desde Archivo) ---" << std::endl;
    std::string file_path = GetStringInput("Ingrese la ruta completa al archivo de esquema (ej. 'data/schema.txt'): ");

    Status status = g_catalog_manager->CreateTableFromPath(file_path);
    if (status == Status::OK) {
        std::cout << "Tabla creada exitosamente desde el archivo '" << file_path << "'." << std::endl;
    } else {
        std::cerr << "Error al crear la tabla desde el archivo: " << StatusToString(status) << std::endl;
    }
}


void ViewTableSchema() {
    if (!g_catalog_manager) {
        std::cout << "No hay un Catalog Manager inicializado. Cargue o cree un disco primero." << std::endl;
        return;
    }
    std::cout << "\n--- Ver Esquema de Tabla ---" << std::endl;
    std::string table_name = GetStringInput("Ingrese el nombre de la tabla para ver su esquema: ");

    FullTableSchema schema;
    Status status = g_catalog_manager->GetTableSchema(table_name, schema);
    if (status == Status::OK) {
        std::cout << "\n--- Esquema de Tabla: " << schema.base_metadata.table_name << " ---" << std::endl;
        std::cout << "  ID de Tabla: " << schema.base_metadata.table_id << std::endl;
        std::cout << "  Tipo de Guardado (Longitud Fija): " << (schema.base_metadata.is_fixed_length_record ? "Sí" : "No") << std::endl;
        if (schema.base_metadata.is_fixed_length_record) {
            std::cout << "  Tamaño Total de Registro Fijo: " << schema.base_metadata.fixed_record_size << " bytes" << std::endl;
        } else {
            std::cout << "  Tamaño Total de Registro Variable (se determina en tiempo de ejecución)" << std::endl;
        }
        // Mostrar todas las páginas de datos
        std::cout << "  Páginas de Datos (PageIds): [";
        for (size_t i = 0; i < schema.base_metadata.data_page_ids.size(); ++i) {
            std::cout << schema.base_metadata.data_page_ids[i];
            if (i < schema.base_metadata.data_page_ids.size() - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "]" << std::endl;
        std::cout << "  Número Total de Registros (aproximado): " << schema.base_metadata.num_records << std::endl;

        std::cout << "\n  Columnas:" << std::endl;
        for (const auto& col : schema.columns) {
            std::cout << "    - Nombre: " << col.name
                      << ", Tipo: " << ColumnTypeToString(col.type)
                      << ", Tamaño/Max_Longitud: " << col.size << std::endl;
        }
    } else {
        std::cerr << "Error al obtener el esquema de la tabla: " << StatusToString(status) << std::endl;
    }
}

void ListExistingTables() {
    if (!g_catalog_manager) {
        std::cout << "No hay un Catalog Manager inicializado. Cargue o cree un disco primero." << std::endl;
        return;
    }
    std::cout << "\n--- Listar Tablas Existentes ---" << std::endl;
    std::vector<std::string> table_names;
    Status status = g_catalog_manager->ListTables(table_names);
    if (status == Status::OK) {
        if (table_names.empty()) {
            std::cout << "No hay tablas registradas." << std::endl;
        } else {
            std::cout << "Tablas registradas:" << std::endl;
            for (const std::string& name : table_names) {
                std::cout << "- " << name << std::endl;
            }
        }
    } else {
        std::cerr << "Error al listar tablas: " << StatusToString(status) << std::endl;
    }
}

void DeleteTable() {
    if (!g_catalog_manager) {
        std::cout << "No hay un Catalog Manager inicializado. Cargue o cree un disco primero." << std::endl;
        return;
    }
    std::cout << "\n--- Eliminar Tabla ---" << std::endl;
    std::string table_name = GetStringInput("Ingrese el nombre de la tabla a eliminar: ");
    
    char confirm_choice = GetStringInput("¿Está seguro de que desea eliminar la tabla '" + table_name + "' y todos sus datos? (s/n): ")[0];
    if (std::tolower(confirm_choice) != 's') {
        std::cout << "Operación de eliminación de tabla cancelada." << std::endl;
        return;
    }

    Status status = g_catalog_manager->DropTable(table_name);
    if (status == Status::OK) {
        std::cout << "Tabla '" << table_name << "' eliminada exitosamente." << std::endl;
    } else {
        std::cerr << "Error al eliminar la tabla: " << StatusToString(status) << std::endl;
    }
}


// --- Manejadores de Menú ---

void HandleDiskManagement() {
    int choice;
    do {
        DisplayDiskManagementMenu();
        choice = GetNumericInput<int>("");

        switch (choice) {
            case 1: ViewDiskStatus(); break;
            case 2: CreateNewDisk(); break;
            case 3: LoadExistingDisk(); break;
            case 4: DeleteDisk(); break;
            case 5: ViewDetailedDiskInfo(); break;
            case 6: ViewBlockStatusMap(); break;
            case 7: ViewLogicalToPhysicalMap(); break;
            case 8: std::cout << "Volviendo al Menú Principal." << std::endl; break;
            default: std::cout << "Opción inválida. Intente de nuevo." << std::endl; break;
        }
    } while (choice != 8); // Cambiado a 8
}

void HandleBufferPoolManagement() {
    if (!g_buffer_manager) {
        std::cout << "Buffer Manager no inicializado. Cree o cargue un disco primero." << std::endl;
        return;
    }
    int choice;
    do {
        DisplayBufferPoolManagementMenu();
        choice = GetNumericInput<int>("");

        switch (choice) {
            case 1: ViewBufferStatus(); break;
            case 2: FlushAllPages(); break;
            case 3: ViewBufferPoolTable(); break; // NUEVO
            case 4: std::cout << "Funcionalidad no implementada aún." << std::endl; break;
            case 5: std::cout << "Funcionalidad no implementada aún." << std::endl; break;
            case 6: std::cout << "Volviendo al Menú Principal." << std::endl; break;
            default: std::cout << "Opción inválida. Intente de nuevo." << std::endl; break;
        }
    } while (choice != 6); // Cambiado a 6
}

void HandleDataManagement() {
    if (!g_record_manager || !g_catalog_manager) {
        std::cout << "Managers no inicializados. Cree o cargue un disco primero." << std::endl;
        return;
    }
    int choice;
    do {
        DisplayDataManagementMenu();
        choice = GetNumericInput<int>("");

        switch (choice) {
            case 1: InsertRecord(); break;
            case 2: SelectRecords(); break;
            case 3: UpdateRecord(); break;
            case 4: DeleteRecord(); break;
            case 5: ViewBlockContentDebug(); break;
            case 6: std::cout << "Volviendo al Menú Principal." << std::endl; break;
            default: std::cout << "Opción inválida. Intente de nuevo." << std::endl; break;
        }
    } while (choice != 6);
}

void HandleCatalogManagement() {
    if (!g_catalog_manager) {
        std::cout << "Catalog Manager no inicializado. Cree o cargue un disco primero." << std::endl;
        return;
    }
    int choice;
    do {
        DisplayCatalogManagementMenu();
        choice = GetNumericInput<int>("");

        switch (choice) {
            case 1: CreateNewTableForm(); break;
            case 2: CreateNewTableFromFile(); break;
            case 3: ViewTableSchema(); break;
            case 4: ListExistingTables(); break;
            case 5: DeleteTable(); break;
            case 6: std::cout << "Volviendo al Menú Principal." << std::endl; break;
            default: std::cout << "Opción inválida. Intente de nuevo." << std::endl; break;
        }
    } while (choice != 6);
}


int main() {
    std::cout << "Directorio de trabajo actual: " << fs::current_path() << std::endl; // Debug output

    int choice;
    do {
        DisplayMainMenu();
        choice = GetNumericInput<int>("");

        switch (choice) {
            case 1: HandleDiskManagement(); break;
            case 2: HandleBufferPoolManagement(); break;
            case 3: HandleDataManagement(); break;
            case 4: HandleCatalogManagement(); break;
            case 5: std::cout << "Gestión de Índices - Funcionalidad no implementada aún." << std::endl; break;
            case 6: std::cout << "Procesador de Consultas - Funcionalidad no implementada aún." << std::endl; break;
            case 7: std::cout << "Saliendo del SGBD. ¡Adiós!" << std::endl; break;
            default: std::cout << "Opción inválida. Intente de nuevo." << std::endl; break;
        }
    } while (choice != 7);

    // Los destructores de unique_ptr se encargarán de liberar los managers.
    // FlushAllPages se llama en el destructor del BufferManager.

    return 0;
}
