// main.cpp
#include <iostream>
#include <string>
#include <limits> // Para std::numeric_limits
#include <vector> // Para std::vector para datos de prueba
#include <fstream> // Para manejo de archivos
#include <sstream> // Para std::stringstream
#include <cctype> // Para std::isspace
#include <memory> // Para std::unique_ptr
#include <cstring> // Para std::memcpy
// Headers portables para compatibilidad
#ifdef _WIN32
    #include <direct.h>     // Para _mkdir en Windows
    #include <io.h>         // Para _access en Windows
    #include <sys/stat.h>   // Para _stat en Windows
#else
    #include <sys/stat.h>   // Para mkdir en Unix/Linux
    #include <unistd.h>     // Para access en Unix/Linux
#endif
#include <cctype>   // Para std::tolower
#include <sstream>  // Para std::stringstream
#include <algorithm> // Para std::replace
#include <iomanip>  // Para std::setw, std::setfill
#include <regex>

// Incluir los headers de los componentes del SGBD (refactorizados en español)
#include "data_storage/gestor_disco.h"
#include "data_storage/bloque.h"
#include "data_storage/gestor_buffer.h"
#include "replacement_policies/lru_espanol.h" // Incluir la política LRU en español
#include "replacement_policies/clock_espanol.h" // Incluir la política CLOCK en español
#include "record_manager/gestor_registros.h" // Incluir el GestorRegistros
#include "Catalog_Manager/gestor_catalogo.h"
#include "include/common.h" // Para Status, BlockSizeType, SectorSizeType, PageType, ColumnMetadata
#include "index/gestor_indices.h"
#include "Catalog_Manager/gestor_tablas_avanzado.h" // Incluir el GestorTablasAvanzado

// Punteros globales para los managers (refactorizados en español)
std::unique_ptr<GestorDisco> g_gestor_disco = nullptr;
std::unique_ptr<GestorBuffer> g_gestor_buffer = nullptr;
std::unique_ptr<GestorRegistros> g_gestor_registros = nullptr;
std::unique_ptr<GestorCatalogo> g_gestor_catalogo = nullptr;
std::unique_ptr<GestorIndices> g_gestor_indices = nullptr;
std::unique_ptr<GestorTablasAvanzado> g_gestor_tablas_avanzado = nullptr;

// Aliases para compatibilidad con nombres en inglés usados en el código
auto& g_disk_manager = g_gestor_disco;
auto& g_buffer_manager = g_gestor_buffer;
auto& g_record_manager = g_gestor_registros;
auto& g_catalog_manager = g_gestor_catalogo;
auto& g_index_manager = g_gestor_indices;

// Aliases de tipos para compatibilidad
using DiskManager = GestorDisco;
using BufferManager = GestorBuffer;
using RecordManager = GestorRegistros;
using CatalogManager = GestorCatalogo;
using IndexManager = GestorIndices;

// ===== DECLARACIONES DE FUNCIONES =====
void HandleQueryProcessor();

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
    std::cout << "    2. VARCHAR" << std::endl;
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

// Función auxiliar para parsear condiciones WHERE simples con AND
struct Condition {
    std::string col;
    std::string val;
};
std::vector<Condition> ParseWhereConditions(const std::string& where_str) {
    std::vector<Condition> conds;
    size_t start = 0, end = 0;
    std::string temp_where_str = where_str; // Copia modificable
    
    // Convertir a minúsculas para coincidencia sin distinción de mayúsculas y minúsculas
    std::transform(temp_where_str.begin(), temp_where_str.end(), temp_where_str.begin(), ::tolower);

    while ((end = temp_where_str.find("and", start)) != std::string::npos) {
        std::string cond = temp_where_str.substr(start, end - start);
        size_t eq = cond.find('=');
        if (eq != std::string::npos) {
            std::string col = cond.substr(0, eq);
            std::string val = cond.substr(eq + 1);
            col.erase(std::remove_if(col.begin(), col.end(), ::isspace), col.end());
            val.erase(std::remove_if(val.begin(), val.end(), ::isspace), val.end()); // Corregido: val.end()
            if (val.front() == '\'' && val.back() == '\'') val = val.substr(1, val.size()-2);
            conds.push_back({col, val});
        }
        start = end + 3;
    }
    std::string cond = temp_where_str.substr(start);
    size_t eq = cond.find('=');
    if (eq != std::string::npos) {
        std::string col = cond.substr(0, eq);
        std::string val = cond.substr(eq + 1);
        col.erase(std::remove_if(col.begin(), col.end(), ::isspace), col.end());
        val.erase(std::remove_if(val.begin(), val.end(), ::isspace), val.end()); // Corregido: val.end()
        if (val.front() == '\'' && val.back() == '\'') val = val.substr(1, val.size()-2);
        conds.push_back({col, val});
    }
    return conds;
}

// Función auxiliar para validar y convertir valores según el tipo de columna
bool ValidateAndConvertValues(const std::vector<std::string>& values, const std::vector<ColumnMetadata>& columns, std::vector<std::string>& out_values, std::string& error) {
    if (values.size() != columns.size()) {
        error = "Cantidad de valores (" + std::to_string(values.size()) + ") no coincide con el número de columnas (" + std::to_string(columns.size()) + ").";
        return false;
    }
    out_values.clear();
    for (size_t i = 0; i < columns.size(); ++i) {
        const auto& col = columns[i];
        std::string val = values[i];
        switch (static_cast<uint8_t>(col.type)) { // Corrección: Cast explícito a uint8_t
            case static_cast<uint8_t>(ColumnType::INT): // Corrección: Cast explícito a uint8_t
                try {
                    // Intentar convertir a int y luego a string para asegurar el formato
                    out_values.push_back(std::to_string(std::stoi(val)));
                } catch (...) {
                    error = "Valor '" + val + "' no es un INT válido para la columna '" + col.name + "'.";
                    return false;
                }
                break;
            case static_cast<uint8_t>(ColumnType::CHAR): // Corrección: Cast explícito a uint8_t
                if (val.length() > col.size) {
                    error = "Valor '" + val + "' excede la longitud CHAR(" + std::to_string(col.size) + ") en columna '" + col.name + "'.";
                    return false;
                }
                // Añadir padding si es necesario
                val.resize(col.size, ' ');
                out_values.push_back(val);
                break;
            case static_cast<uint8_t>(ColumnType::VARCHAR): // Corrección: Cast explícito a uint8_t
                if (val.length() > col.size) {
                    error = "Valor '" + val + "' excede la longitud VARCHAR(" + std::to_string(col.size) + ") en columna '" + col.name + "'.";
                    return false;
                }
                out_values.push_back(val);
                break;
            default:
                error = "Tipo de columna desconocido para la columna '" + *col.name;
                return false;
        }
    }
    return true;
}

// --- Funciones de Menú ---

void DisplayMainMenu() {
    std::cout << "\n=== SGBD - Sistema de Gestión de Base de Datos ===" << std::endl;
    std::cout << "1. Gestión de Discos" << std::endl;
    std::cout << "2. Gestión de Buffer Pool" << std::endl;
    std::cout << "3. Gestión de Datos (Tablas y Registros)" << std::endl;
    std::cout << "4. Gestión de Catálogo" << std::endl;
    std::cout << "5. Gestión de Índices" << std::endl;
    std::cout << "6. Gestión Avanzada de Tablas" << std::endl;
    std::cout << "7. Procesador de Consultas (SQL)" << std::endl;
    std::cout << "8. Gestión de Módulo de Audio" << std::endl;
    std::cout << "9. Salir" << std::endl;
    std::cout << "Seleccione una opción: ";
}

void DisplayDiskManagementMenu() {
    std::cout << "\n--- Menú: Gestión del Disco ---" << std::endl;
    std::cout << "1. Ver Estado del Disco (Resumen)" << std::endl;
    std::cout << "2. Crear Nuevo Disco" << std::endl;
    std::cout << "3. Cargar Disco Existente" << std::endl;
    std::cout << "4. Eliminar Disco" << std::endl;
    std::cout << "5. Ver Información Detallada del Disco" << std::endl;
    std::cout << "6. Ver Mapa de Estado de Bloques" << std::endl;
    std::cout << "7. Ver Mapeo Lógico a Físico" << std::endl;
    std::cout << "8. Volver al Menú Principal" << std::endl;
    std::cout << "Ingrese su opción: ";
}

void DisplayBufferPoolManagementMenu() {
    std::cout << "\n--- Menú: Gestión del Buffer Pool ---" << std::endl;
    std::cout << "1. Ver Estado del Buffer" << std::endl;
    std::cout << "2. Flushar Todas las Páginas Sucias" << std::endl;
    std::cout << "3. Ver Tabla de Páginas del Buffer Pool" << std::endl;
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

void DisplayAdvancedTableManagementMenu() {
    std::cout << "\n--- Menú: Gestión Avanzada de Tablas ---" << std::endl;
    std::cout << "1. Crear Tabla por Archivo (esquema automático)" << std::endl;
    std::cout << "2. Crear Tabla por Formulario (longitud fija)" << std::endl;
    std::cout << "3. Insertar Registros por Archivo CSV" << std::endl;
    std::cout << "4. Insertar Registro por Formulario" << std::endl;
    std::cout << "5. Eliminar Registros por Condiciones (AND)" << std::endl;
    std::cout << "6. Ver Espacios Disponibles" << std::endl;
    std::cout << "7. Ver Estadísticas del Gestor" << std::endl;
    std::cout << "8. Volver al Menú Principal" << std::endl;
    std::cout << "Ingrese su opción: ";
}


// --- Implementación de Funciones de Menú ---

// === FUNCIONES DE GESTIÓN AVANZADA DE TABLAS ===

void CrearTablaPorArchivoAvanzada() {
    if (!g_gestor_tablas_avanzado) {
        std::cerr << "Error: GestorTablasAvanzado no está inicializado." << std::endl;
        return;
    }
    
    std::cout << "\n=== CREAR TABLA POR ARCHIVO (ESQUEMA AUTOMÁTICO) ===" << std::endl;
    
    std::string ruta_archivo = GetStringInput("Ingrese la ruta del archivo: ");
    std::string nombre_tabla = GetStringInput("Ingrese el nombre para la tabla: ");
    
    Status status = g_gestor_tablas_avanzado->CrearTablaPorArchivo(ruta_archivo, nombre_tabla);
    
    if (status == Status::OK) {
        std::cout << "Tabla creada exitosamente desde archivo." << std::endl;
    } else {
        std::cerr << "Error al crear tabla desde archivo: " << StatusToString(status) << std::endl;
    }
}

void CrearTablaPorFormularioAvanzada() {
    if (!g_gestor_tablas_avanzado) {
        std::cerr << "Error: GestorTablasAvanzado no está inicializado." << std::endl;
        return;
    }
    
    Status status = g_gestor_tablas_avanzado->CrearTablaInteractiva();
    
    if (status == Status::OK) {
        std::cout << "Tabla creada exitosamente por formulario." << std::endl;
    } else if (status == Status::CANCELLED) {
        std::cout << "Creación de tabla cancelada por el usuario." << std::endl;
    } else {
        std::cerr << "Error al crear tabla por formulario: " << StatusToString(status) << std::endl;
    }
}

void InsertarRegistrosPorCSVAvanzado() {
    if (!g_gestor_tablas_avanzado) {
        std::cerr << "Error: GestorTablasAvanzado no está inicializado." << std::endl;
        return;
    }
    
    std::cout << "\n=== INSERTAR REGISTROS POR ARCHIVO CSV ===" << std::endl;
    
    std::string nombre_tabla = GetStringInput("Ingrese el nombre de la tabla: ");
    std::string ruta_csv = GetStringInput("Ingrese la ruta del archivo CSV: ");
    
    Status status = g_gestor_tablas_avanzado->InsertarRegistrosPorCSV(nombre_tabla, ruta_csv);
    
    if (status == Status::OK) {
        std::cout << "Registros insertados exitosamente desde CSV." << std::endl;
    } else {
        std::cerr << "Error al insertar registros desde CSV: " << StatusToString(status) << std::endl;
    }
}

void InsertarRegistroPorFormularioAvanzado() {
    if (!g_gestor_tablas_avanzado) {
        std::cerr << "Error: GestorTablasAvanzado no está inicializado." << std::endl;
        return;
    }
    
    std::string nombre_tabla = GetStringInput("Ingrese el nombre de la tabla: ");
    
    Status status = g_gestor_tablas_avanzado->InsertarRegistroInteractivo(nombre_tabla);
    
    if (status == Status::OK) {
        std::cout << "Registro insertado exitosamente por formulario." << std::endl;
    } else if (status == Status::CANCELLED) {
        std::cout << "Inserción cancelada por el usuario." << std::endl;
    } else {
        std::cerr << "Error al insertar registro por formulario: " << StatusToString(status) << std::endl;
    }
}

void EliminarRegistrosPorCondicionesAvanzado() {
    if (!g_gestor_tablas_avanzado) {
        std::cerr << "Error: GestorTablasAvanzado no está inicializado." << std::endl;
        return;
    }
    
    std::string nombre_tabla = GetStringInput("Ingrese el nombre de la tabla: ");
    
    Status status = g_gestor_tablas_avanzado->EliminarRegistrosInteractivo(nombre_tabla);
    
    if (status == Status::OK) {
        std::cout << "Registros eliminados exitosamente." << std::endl;
    } else if (status == Status::CANCELLED) {
        std::cout << "Eliminación cancelada por el usuario." << std::endl;
    } else {
        std::cerr << "Error al eliminar registros: " << StatusToString(status) << std::endl;
    }
}

void VerEspaciosDisponibles() {
    if (!g_gestor_tablas_avanzado) {
        std::cerr << "Error: GestorTablasAvanzado no está inicializado." << std::endl;
        return;
    }
    
    g_gestor_tablas_avanzado->ImprimirEspaciosDisponibles();
}

void VerEstadisticasGestorAvanzado() {
    if (!g_gestor_tablas_avanzado) {
        std::cerr << "Error: GestorTablasAvanzado no está inicializado." << std::endl;
        return;
    }
    
    g_gestor_tablas_avanzado->ImprimirEstadisticas();
}

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
    int replacement_policy_choice;

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

    std::cout << "Seleccione la política de reemplazo para el Buffer Pool:" << std::endl;
    std::cout << "  0. LRU (Least Recently Used)" << std::endl;
    std::cout << "  1. CLOCK" << std::endl;
    replacement_policy_choice = GetNumericInput<int>("Opción: ");

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

        std::unique_ptr<IReplacementPolicy> policy;
        if (replacement_policy_choice == 0) {
            policy = std::make_unique<LRUReplacementPolicy>();
        } else if (replacement_policy_choice == 1) {
            policy = std::make_unique<ClockReplacementPolicy>();
        } else {
            std::cout << "Opción de política de reemplazo inválida. Usando LRU por defecto." << std::endl;
            policy = std::make_unique<LRUReplacementPolicy>();
        }

        g_buffer_manager = std::make_unique<BufferManager>(*g_disk_manager, buffer_pool_size, g_disk_manager->GetBlockSize(), std::move(policy));
        
        // Inicializar RecordManager y CatalogManager con constructores simplificados
        // Ahora sus constructores solo requieren BufferManager, resolviendo la dependencia circular inicial.
        g_record_manager = std::make_unique<RecordManager>(*g_buffer_manager);
        g_catalog_manager = std::make_unique<CatalogManager>(*g_buffer_manager);
        g_record_manager->SetCatalogManager(*g_catalog_manager);
        g_catalog_manager->SetRecordManager(*g_record_manager);
        g_index_manager = std::make_unique<IndexManager>();
        g_record_manager->SetIndexManager(g_index_manager.get());

        g_catalog_manager->InitCatalog();

    } catch (const std::exception& e) {
        std::cerr << "Error al crear el disco: " << e.what() << std::endl;
        g_disk_manager.reset();
        g_buffer_manager.reset();
        g_record_manager.reset();
        g_catalog_manager.reset();
        g_index_manager.reset(); // Asegurarse de resetear el index manager también
    }
}

void LoadExistingDisk() {
    std::string disk_name;
    uint32_t buffer_pool_size;
    int replacement_policy_choice;

    std::cout << "\n--- Cargar Disco Existente ---" << std::endl;
    std::cout << "Ingrese el nombre del disco a cargar: ";
    std::getline(std::cin, disk_name);

    fs::path disk_path = fs::path("Discos") / disk_name;
    if (!fs::exists(disk_path)) {
        std::cerr << "Error: El disco '" << disk_name << "' no existe en " << disk_path << std::endl;
        return;
    }

    try {
        // Al cargar un disco existente, los parámetros del disco se leerán de los metadatos.
        // Los valores iniciales para el constructor de DiskManager (1,1,1,1,512,512) son placeholders.
        g_disk_manager = std::make_unique<DiskManager>(disk_name, 1, 1, 1, 1, 512, 512, false);
        Status status = g_disk_manager->LoadDiskMetadata();
        if (status != Status::OK) {
            std::cerr << "Error al cargar los metadatos del disco: " << StatusToString(status) << std::endl;
            g_disk_manager.reset();
            return;
        }
        std::cout << "Disco '" << disk_name << "' cargado exitosamente." << std::endl;

        buffer_pool_size = GetNumericInput<uint32_t>("Ingrese el tamaño del Buffer Pool para esta sesión (ej. 10): ");
        
        std::cout << "Seleccione la política de reemplazo para el Buffer Pool:" << std::endl;
        std::cout << "  0. LRU (Least Recently Used)" << std::endl;
        std::cout << "  1. CLOCK" << std::endl;
        replacement_policy_choice = GetNumericInput<int>("Opción: ");

        std::unique_ptr<IReplacementPolicy> policy;
        if (replacement_policy_choice == 0) {
            policy = std::make_unique<LRUReplacementPolicy>();
        } else if (replacement_policy_choice == 1) {
            policy = std::make_unique<ClockReplacementPolicy>();
        } else {
            std::cout << "Opción de política de reemplazo inválida. Usando LRU por defecto." << std::endl;
            policy = std::make_unique<LRUReplacementPolicy>();
        }

        g_buffer_manager = std::make_unique<BufferManager>(*g_disk_manager, buffer_pool_size, g_disk_manager->GetBlockSize(), std::move(policy));
        
        // Inicializar RecordManager y CatalogManager con constructores simplificados
        // Ahora sus constructores solo requieren BufferManager, resolviendo la dependencia circular inicial.
        g_record_manager = std::make_unique<RecordManager>(*g_buffer_manager);
        g_catalog_manager = std::make_unique<CatalogManager>(*g_buffer_manager);
        g_record_manager->SetCatalogManager(*g_catalog_manager);
        g_catalog_manager->SetRecordManager(*g_record_manager);
        g_index_manager = std::make_unique<IndexManager>();
        g_record_manager->SetIndexManager(g_index_manager.get());

        g_catalog_manager->InitCatalog();

    } catch (const std::exception& e) {
        std::cerr << "Error al cargar el disco: " << e.what() << std::endl;
        g_disk_manager.reset();
        g_buffer_manager.reset();
        g_record_manager.reset();
        g_catalog_manager.reset();
        g_index_manager.reset(); // Asegurarse de resetear el index manager también
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
                g_index_manager.reset(); // Asegurarse de resetear el index manager también
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
RecordData PrepareRecordData(const std::string& input_content, const FullTableSchema& schema) {
    std::string transformed_content = TransformDelimiters(input_content);
    std::stringstream ss(transformed_content);
    std::string segment;
    RecordData record_data;

    // Parsear los campos del string de entrada
    while(std::getline(ss, segment, '#')) {
        record_data.fields.push_back(segment);
    }

    // Validar y convertir valores según el esquema
    std::vector<std::string> validated_fields;
    std::string error_msg;
    if (!ValidateAndConvertValues(record_data.fields, schema.columns, validated_fields, error_msg)) {
        std::cerr << "Error de validación de datos: " << error_msg << std::endl;
        // Lanzar excepción o manejar el error de forma más robusta en un SGBD real
        return RecordData(); // Retornar un RecordData vacío o inválido
    }
    record_data.fields = validated_fields; // Usar los campos validados y convertidos

    return record_data;
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
    RecordData new_record_data = PrepareRecordData(content, schema);
    if (new_record_data.fields.empty() && !content.empty()) { // Si PrepareRecordData falló la validación
        std::cerr << "Fallo al preparar los datos del registro. Inserción cancelada." << std::endl;
        return;
    }

    uint32_t slot_id;
    Status status = Status::ERROR;
    PageId target_page_id = 0;

    // 1. Intentar insertar en una página existente con espacio
    for (PageId page_id : schema.base_metadata.data_page_ids) {
        BlockSizeType free_space;
        Status get_space_status = g_record_manager->GetFreeSpace(page_id, free_space);
        // El tamaño del registro serializado puede variar, pero para la estimación inicial
        // asumimos que el tamaño de RecordData::fields es una buena aproximación,
        // o podríamos serializarlo temporalmente para obtener el tamaño exacto.
        // Por ahora, una estimación simple: cada campo + delimitador.
        // Una estimación más precisa sería:
        // size_t estimated_record_size = g_record_manager->SerializeRecord(new_record_data, schema).size();
        // Para simplificar, asumimos que el RecordManager manejará el tamaño real.
        if (get_space_status == Status::OK && free_space >= (new_record_data.fields.size() * 10 + sizeof(SlotDirectoryEntry))) { // Estimación
            status = g_record_manager->InsertRecord(page_id, new_record_data, slot_id);
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
        status = g_record_manager->InsertRecord(new_data_page_id, new_record_data, slot_id);
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
        uint32_t num_slots;
        // Obtener el número total de slots en la página (ocupados o no)
        Byte* page_data_peek = g_buffer_manager->FetchPage(page_id);
        if (page_data_peek == nullptr) {
            std::cerr << "Advertencia: No se pudo obtener la página " << page_id << " para seleccionar registros. Saltando esta página." << std::endl;
            continue;
        }
        BlockHeader header_peek = g_record_manager->ReadBlockHeader(page_data_peek);
        num_slots = header_peek.num_slots;
        g_buffer_manager->UnpinPage(page_id, false);

        std::cout << "  --- Página " << page_id << " (Slots: " << num_slots << ") ---" << std::endl;
        for (uint32_t i = 0; i < num_slots; ++i) { // Iterar sobre todos los slots
            RecordData fetched_rec_data; // Usar RecordData
            Status status = g_record_manager->GetRecord(page_id, i, fetched_rec_data);
            if (status == Status::OK) {
                std::cout << "    Page " << page_id << ", Slot " << i << ": ";
                bool first_field = true;
                for (const auto& field : fetched_rec_data.fields) {
                    if (!first_field) std::cout << ", ";
                    std::cout << field;
                    first_field = false;
                }
                std::cout << std::endl;
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
    RecordData updated_record_data = PrepareRecordData(content, schema);
    if (updated_record_data.fields.empty() && !content.empty()) {
        std::cerr << "Fallo al preparar los datos del registro. Actualización cancelada." << std::endl;
        return;
    }

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

    Status status = g_record_manager->UpdateRecord(target_page_id, slot_id, updated_record_data);
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
    Byte* block_data = g_buffer_manager->GetPageDataInPool(page_id);
    if (block_data == nullptr) {
        // If not in buffer, try to fetch it to bring it into the buffer
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
                // Asegurarse de que record_content no lea más allá de los límites del bloque
                size_t actual_length = std::min((size_t)entry.length, (size_t)(g_buffer_manager->GetBlockSize() - entry.offset));
                // Usar RecordData para deserializar y mostrar
                std::vector<Byte> serialized_data(block_data + entry.offset, block_data + entry.offset + actual_length);
                
                // Necesitamos el esquema de la tabla para deserializar correctamente
                FullTableSchema schema_for_debug;
                Status get_schema_status = g_catalog_manager->GetTableSchemaForPage(page_id, schema_for_debug);
                if (get_schema_status == Status::OK) {
                    RecordData debug_rec_data;
                    // Llamamos al método DeserializeRecord del RecordManager
                    debug_rec_data = g_record_manager->DeserializeRecord(serialized_data, schema_for_debug);
                    std::cout << "    Contenido (deserializado): ";
                    bool first_field = true;
                    for (const auto& field : debug_rec_data.fields) {
                        if (!first_field) std::cout << ", ";
                        std::cout << field;
                        first_field = false;
                    }
                    std::cout << std::endl;
                } else {
                    std::cout << "    Contenido (serializado): " << std::string(serialized_data.begin(), serialized_data.end()).substr(0, std::min((size_t)50, serialized_data.size())) << "..." << std::endl;
                    std::cerr << "    Advertencia: No se pudo obtener el esquema para deserializar el registro." << std::endl;
                }
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
        
        switch (static_cast<uint8_t>(col.type)) { // Corrección: Cast explícito a uint8_t
            case static_cast<uint8_t>(ColumnType::INT): col.size = sizeof(int); break; // Corrección: Cast explícito a uint8_t
            case static_cast<uint8_t>(ColumnType::CHAR): col.size = GetNumericInput<uint32_t>("  Ingrese la longitud fija para CHAR (ej. 10): "); break; // Corrección: Cast explícito a uint8_t
            case static_cast<uint8_t>(ColumnType::VARCHAR): // Corrección: Cast explícito a uint8_t
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


// --- Funciones de Integración con Módulo de Audio ---

/**
 * Función para ejecutar el módulo de procesamiento de audio
 * Permite grabar audio y generar consulta SQL automáticamente
 */
void EjecutarModuloAudio() {
    std::cout << "\n=== MÓDULO DE PROCESAMIENTO DE AUDIO ===" << std::endl;
    std::cout << "Este módulo permite convertir comandos de voz en consultas SQL." << std::endl;
    std::cout << "\nPasos del proceso:" << std::endl;
    std::cout << "1. Grabar audio con comando de voz" << std::endl;
    std::cout << "2. Transcribir audio a texto" << std::endl;
    std::cout << "3. Compilar texto natural a consulta SQL" << std::endl;
    std::cout << "4. Generar archivo consulta_para_gestor.txt" << std::endl;
    
    std::string confirmacion = GetStringInput("\n¿Desea ejecutar el módulo de audio? (s/n): ");
    if (confirmacion != "s" && confirmacion != "S" && confirmacion != "si" && confirmacion != "SI") {
        std::cout << "Operación cancelada." << std::endl;
        return;
    }
    
    std::cout << "\n--- Ejecutando Grabadora de Audio ---" << std::endl;
    std::cout << "Ejecutando: src/audio_processor/grabadora_audio.exe" << std::endl;
    
    // Cambiar al directorio del módulo de audio
    std::string comando_grabadora = "cd src/audio_processor && grabadora_audio.exe";
    int resultado_grabadora = system(comando_grabadora.c_str());
    
    if (resultado_grabadora != 0) {
        std::cout << "Error: Fallo al ejecutar la grabadora de audio." << std::endl;
        std::cout << "Verifique que el archivo grabadora_audio.exe existe en src/audio_processor/" << std::endl;
        return;
    }
    
    std::cout << "\n--- Ejecutando Transcripción de Audio ---" << std::endl;
    std::cout << "Ejecutando: python transcribir_audio.py" << std::endl;
    
    std::string comando_transcripcion = "cd src/audio_processor && python transcribir_audio.py";
    int resultado_transcripcion = system(comando_transcripcion.c_str());
    
    if (resultado_transcripcion != 0) {
        std::cout << "Error: Fallo al ejecutar la transcripción de audio." << std::endl;
        std::cout << "Verifique que Python está instalado y transcribir_audio.py existe." << std::endl;
        return;
    }
    
    std::cout << "\n--- Ejecutando Compilador de Lenguaje Natural a SQL ---" << std::endl;
    std::cout << "Ejecutando: python compilador.py" << std::endl;
    
    std::string comando_compilador = "cd src/audio_processor && python compilador.py";
    int resultado_compilador = system(comando_compilador.c_str());
    
    if (resultado_compilador != 0) {
        std::cout << "Error: Fallo al ejecutar el compilador de consultas." << std::endl;
        std::cout << "Verifique que compilador.py existe y está funcionando correctamente." << std::endl;
        return;
    }
    
    std::cout << "\n=== PROCESAMIENTO DE AUDIO COMPLETADO ===" << std::endl;
    std::cout << "La consulta SQL ha sido generada en: src/audio_processor/consulta_para_gestor.txt" << std::endl;
    std::cout << "Ahora puede usar la opción 'Procesador de Consultas' para ejecutar la consulta." << std::endl;
    
    // Mostrar el contenido generado
    std::ifstream archivo_generado("src/audio_processor/consulta_para_gestor.txt");
    if (archivo_generado.is_open()) {
        std::string consulta_generada;
        std::getline(archivo_generado, consulta_generada);
        archivo_generado.close();
        
        if (!consulta_generada.empty()) {
            std::cout << "\n--- Consulta SQL Generada ---" << std::endl;
            std::cout << "Consulta: " << consulta_generada << std::endl;
            
            std::string ejecutar_ahora = GetStringInput("\n¿Desea ejecutar esta consulta ahora? (s/n): ");
            if (ejecutar_ahora == "s" || ejecutar_ahora == "S" || ejecutar_ahora == "si" || ejecutar_ahora == "SI") {
                std::cout << "\nRedirigiendo al procesador de consultas..." << std::endl;
                HandleQueryProcessor();
            }
        }
    }
}

/**
 * Función para mostrar el estado del módulo de audio
 */
void MostrarEstadoModuloAudio() {
    std::cout << "\n=== ESTADO DEL MÓDULO DE AUDIO ===" << std::endl;
    
    // Verificar archivos del módulo
    std::vector<std::pair<std::string, std::string>> archivos_modulo = {
        {"src/audio_processor/grabadora_audio.exe", "Grabadora de Audio"},
        {"src/audio_processor/transcribir_audio.py", "Transcriptor de Audio"},
        {"src/audio_processor/compilador.py", "Compilador SQL"},
        {"src/audio_processor/voz.cpp", "Interfaz C++ Audio"},
        {"src/audio_processor/transcripcion.txt", "Última Transcripción"},
        {"src/audio_processor/consulta_para_gestor.txt", "Última Consulta SQL"},
        {"src/audio_processor/grabacion.wav", "Última Grabación"}
    };
    
    std::cout << "\nArchivos del módulo:" << std::endl;
    for (const auto& archivo : archivos_modulo) {
        std::ifstream verificar(archivo.first);
        std::string estado = verificar.good() ? "✓ Disponible" : "✗ No encontrado";
        std::cout << "  " << archivo.second << ": " << estado << std::endl;
        verificar.close();
    }
    
    // Mostrar contenido de archivos de salida si existen
    std::cout << "\n--- Última Transcripción ---" << std::endl;
    std::ifstream transcripcion("src/audio_processor/transcripcion.txt");
    if (transcripcion.is_open()) {
        std::string contenido;
        std::getline(transcripcion, contenido);
        std::cout << (contenido.empty() ? "(vacío)" : contenido) << std::endl;
        transcripcion.close();
    } else {
        std::cout << "Archivo no disponible" << std::endl;
    }
    
    std::cout << "\n--- Última Consulta SQL ---" << std::endl;
    std::ifstream consulta("src/audio_processor/consulta_para_gestor.txt");
    if (consulta.is_open()) {
        std::string contenido;
        std::getline(consulta, contenido);
        std::cout << (contenido.empty() ? "(vacío)" : contenido) << std::endl;
        consulta.close();
    } else {
        std::cout << "Archivo no disponible" << std::endl;
    }
}

/**
 * Función para manejar el menú de audio
 */
void HandleAudioManagement() {
    int choice;
    do {
        std::cout << "\n=== GESTIÓN DE MÓDULO DE AUDIO ===" << std::endl;
        std::cout << "1. Ejecutar módulo de audio completo" << std::endl;
        std::cout << "2. Ver estado del módulo de audio" << std::endl;
        std::cout << "3. Limpiar archivos de audio temporales" << std::endl;
        std::cout << "4. Volver al menú principal" << std::endl;
        
        choice = GetNumericInput<int>("Seleccione una opción: ");
        
        switch (choice) {
            case 1:
                EjecutarModuloAudio();
                break;
            case 2:
                MostrarEstadoModuloAudio();
                break;
            case 3: {
                std::cout << "\n--- Limpiando Archivos Temporales ---" << std::endl;
                // Limpiar archivos temporales
                std::vector<std::string> archivos_temporales = {
                    "src/audio_processor/grabacion.wav",
                    "src/audio_processor/transcripcion.txt",
                    "src/audio_processor/consulta_para_gestor.txt"
                };
                
                for (const auto& archivo : archivos_temporales) {
                    if (std::remove(archivo.c_str()) == 0) {
                        std::cout << "✓ Eliminado: " << archivo << std::endl;
                    } else {
                        std::cout << "✗ No se pudo eliminar: " << archivo << std::endl;
                    }
                }
                std::cout << "Limpieza completada." << std::endl;
                break;
            }
            case 4:
                std::cout << "Volviendo al menú principal..." << std::endl;
                break;
            default:
                std::cout << "Opción inválida. Intente de nuevo." << std::endl;
                break;
        }
    } while (choice != 4);
}

// --- Función de Entrada Dual SQL ---

/**
 * Función para obtener consulta SQL de entrada dual:
 * 1. Entrada escrita manual
 * 2. Archivo .txt generado por el módulo de audio
 */
std::string ObtenerConsultaSQL() {
    std::cout << "\n=== ENTRADA DE CONSULTA SQL ===" << std::endl;
    std::cout << "1. Escribir consulta manualmente" << std::endl;
    std::cout << "2. Cargar desde archivo de audio (consulta_para_gestor.txt)" << std::endl;
    std::cout << "3. Cargar desde archivo personalizado" << std::endl;
    
    int opcion = GetNumericInput<int>("Seleccione opción (1-3): ");
    std::string consulta_sql;
    
    switch (opcion) {
        case 1: {
            // Entrada manual tradicional
            std::cout << "\n--- Entrada Manual ---" << std::endl;
            std::cout << "Ejemplos de consultas soportadas:" << std::endl;
            std::cout << "  SELECT nombre,edad FROM clientes WHERE id = 5" << std::endl;
            std::cout << "  SELECT * FROM clientes" << std::endl;
            std::cout << "  INSERT INTO clientes VALUES (1,'Juan Perez',30)" << std::endl;
            std::cout << "  UPDATE clientes SET Edad = 31 WHERE Nombre = 'Juan Perez'" << std::endl;
            std::cout << "  DELETE FROM clientes WHERE ID = 1" << std::endl;
            consulta_sql = GetStringInput("\nConsulta SQL: ");
            break;
        }
        
        case 2: {
            // Cargar desde archivo generado por módulo de audio
            std::string ruta_archivo_audio = "src/audio_processor/consulta_para_gestor.txt";
            std::ifstream archivo_audio(ruta_archivo_audio);
            
            if (!archivo_audio.is_open()) {
                std::cout << "Error: No se pudo abrir el archivo de audio '" << ruta_archivo_audio << "'" << std::endl;
                std::cout << "Asegúrese de que el módulo de audio haya generado la consulta." << std::endl;
                std::cout << "Cambiando a entrada manual..." << std::endl;
                consulta_sql = GetStringInput("Consulta SQL: ");
                break;
            }
            
            // Leer toda la consulta del archivo
            std::string linea;
            std::stringstream consulta_completa;
            while (std::getline(archivo_audio, linea)) {
                if (!linea.empty()) {
                    consulta_completa << linea << " ";
                }
            }
            archivo_audio.close();
            
            consulta_sql = consulta_completa.str();
            
            // Limpiar espacios en blanco al final
            while (!consulta_sql.empty() && std::isspace(consulta_sql.back())) {
                consulta_sql.pop_back();
            }
            
            if (consulta_sql.empty()) {
                std::cout << "Error: El archivo de audio está vacío o no contiene consulta válida." << std::endl;
                std::cout << "Cambiando a entrada manual..." << std::endl;
                consulta_sql = GetStringInput("Consulta SQL: ");
            } else {
                std::cout << "\n--- Consulta Cargada desde Audio ---" << std::endl;
                std::cout << "Consulta detectada: " << consulta_sql << std::endl;
                
                // Confirmar si el usuario quiere usar esta consulta
                std::string confirmacion = GetStringInput("¿Usar esta consulta? (s/n): ");
                if (confirmacion != "s" && confirmacion != "S" && confirmacion != "si" && confirmacion != "SI") {
                    std::cout << "Consulta rechazada. Cambiando a entrada manual..." << std::endl;
                    consulta_sql = GetStringInput("Consulta SQL: ");
                }
            }
            break;
        }
        
        case 3: {
            // Cargar desde archivo personalizado
            std::string ruta_archivo = GetStringInput("Ruta del archivo .txt con la consulta: ");
            std::ifstream archivo_personalizado(ruta_archivo);
            
            if (!archivo_personalizado.is_open()) {
                std::cout << "Error: No se pudo abrir el archivo '" << ruta_archivo << "'" << std::endl;
                std::cout << "Cambiando a entrada manual..." << std::endl;
                consulta_sql = GetStringInput("Consulta SQL: ");
                break;
            }
            
            // Leer toda la consulta del archivo
            std::string linea;
            std::stringstream consulta_completa;
            while (std::getline(archivo_personalizado, linea)) {
                if (!linea.empty()) {
                    consulta_completa << linea << " ";
                }
            }
            archivo_personalizado.close();
            
            consulta_sql = consulta_completa.str();
            
            // Limpiar espacios en blanco al final
            while (!consulta_sql.empty() && std::isspace(consulta_sql.back())) {
                consulta_sql.pop_back();
            }
            
            if (consulta_sql.empty()) {
                std::cout << "Error: El archivo está vacío o no contiene consulta válida." << std::endl;
                std::cout << "Cambiando a entrada manual..." << std::endl;
                consulta_sql = GetStringInput("Consulta SQL: ");
            } else {
                std::cout << "\n--- Consulta Cargada desde Archivo ---" << std::endl;
                std::cout << "Consulta detectada: " << consulta_sql << std::endl;
            }
            break;
        }
        
        default: {
            std::cout << "Opción inválida. Usando entrada manual por defecto." << std::endl;
            consulta_sql = GetStringInput("Consulta SQL: ");
            break;
        }
    }
    
    return consulta_sql;
}

// === FUNCIONES DE GESTIÓN AVANZADA DE ÍNDICES ===

void CrearIndiceAutomaticoMenu() {
    std::cout << "\n🤖 === CREACIÓN AUTOMÁTICA DE ÍNDICE ===" << std::endl;
    std::string tabla = GetStringInput("📁 Nombre de la tabla: ");
    std::string columna = GetStringInput("📋 Nombre de la columna: ");
    
    std::cout << "\n📝 Aplicando estrategia inteligente de indexación..." << std::endl;
    std::cout << "   • Tablas de longitud variable → HASH" << std::endl;
    std::cout << "   • Tablas de longitud fija + INT → B+ Tree" << std::endl;
    std::cout << "   • Tablas de longitud fija + STR/CHAR → String B+ Tree" << std::endl;
    
    Status status = g_gestor_indices->CrearIndiceAutomatico(tabla, columna);
    
    if (status == Status::OK) {
        std::cout << "\n✅ ¡Índice creado exitosamente con estrategia automática!" << std::endl;
    } else {
        std::cout << "\n❌ Error al crear el índice automático" << std::endl;
    }
    
    std::cout << "\nPresione Enter para continuar...";
    std::cin.ignore();
    std::cin.get();
}

void CrearIndicesAutomaticosPorTablaMenu() {
    std::cout << "\n📁 === CREACIÓN MASIVA DE ÍNDICES AUTOMÁTICOS ===" << std::endl;
    std::string tabla = GetStringInput("📁 Nombre de la tabla: ");
    
    std::cout << "\n📋 Ingrese las columnas para indexar (separadas por comas): ";
    std::string columnas_input;
    std::getline(std::cin, columnas_input);
    
    // Parsear columnas
    std::vector<std::string> columnas;
    std::stringstream ss(columnas_input);
    std::string columna;
    
    while (std::getline(ss, columna, ',')) {
        // Eliminar espacios en blanco
        columna.erase(0, columna.find_first_not_of(" \t"));
        columna.erase(columna.find_last_not_of(" \t") + 1);
        if (!columna.empty()) {
            columnas.push_back(columna);
        }
    }
    
    if (columnas.empty()) {
        std::cout << "⚠️ No se especificaron columnas válidas" << std::endl;
        return;
    }
    
    std::cout << "\n📊 Columnas a indexar: " << columnas.size() << std::endl;
    for (const auto& col : columnas) {
        std::cout << "   • " << col << std::endl;
    }
    
    std::cout << "\n¿Continuar con la creación masiva? (s/n): ";
    char confirmacion;
    std::cin >> confirmacion;
    
    if (confirmacion == 's' || confirmacion == 'S') {
        Status status = g_gestor_indices->CrearIndicesAutomaticosPorTabla(tabla, columnas);
        if (status == Status::OK) {
            std::cout << "\n✅ ¡Creación masiva completada exitosamente!" << std::endl;
        } else {
            std::cout << "\n⚠️ Creación masiva completada con algunos errores" << std::endl;
        }
    } else {
        std::cout << "\n❌ Operación cancelada" << std::endl;
    }
    
    std::cout << "\nPresione Enter para continuar...";
    std::cin.ignore();
    std::cin.get();
}

void CargarIndicesAutomaticamenteMenu() {
    std::cout << "\n💾 === CARGA AUTOMÁTICA DE ÍNDICES ===" << std::endl;
    std::cout << "📝 Cargando todos los índices persistidos desde disco..." << std::endl;
    
    Status status = g_gestor_indices->CargarIndicesAutomaticamente();
    
    if (status == Status::OK) {
        std::cout << "\n✅ Índices cargados exitosamente desde disco" << std::endl;
    } else {
        std::cout << "\n⚠️ Error o advertencias durante la carga automática" << std::endl;
    }
    
    std::cout << "\nPresione Enter para continuar...";
    std::cin.ignore();
    std::cin.get();
}

void CrearIndiceManualMenu() {
    std::cout << "\n🔨 === CREACIÓN MANUAL DE ÍNDICE ===" << std::endl;
    std::string tabla = GetStringInput("📁 Nombre de la tabla: ");
    std::string columna = GetStringInput("📋 Nombre de la columna: ");
    
    std::cout << "\n🎯 Seleccione el tipo de índice:" << std::endl;
    std::cout << "1. B+ Tree para Enteros (INT)" << std::endl;
    std::cout << "2. B+ Tree para Cadenas (STRING/CHAR)" << std::endl;
    std::cout << "3. Hash para Cadenas (STRING/CHAR)" << std::endl;
    
    int tipo_choice = GetNumericInput<int>("Opción: ");
    TipoIndice tipo;
    
    switch (tipo_choice) {
        case 1:
            tipo = TipoIndice::BTREE_ENTERO;
            break;
        case 2:
            tipo = TipoIndice::BTREE_CADENA;
            break;
        case 3:
            tipo = TipoIndice::HASH_CADENA;
            break;
        default:
            std::cout << "⚠️ Opción inválida. Usando B+ Tree de cadena por defecto." << std::endl;
            tipo = TipoIndice::BTREE_CADENA;
            break;
    }
    
    Status status = g_gestor_indices->CrearIndice(tabla, columna, tipo);
    
    if (status == Status::OK) {
        std::cout << "\n✅ Índice manual creado exitosamente" << std::endl;
    } else {
        std::cout << "\n❌ Error al crear el índice manual" << std::endl;
    }
    
    std::cout << "\nPresione Enter para continuar...";
    std::cin.ignore();
    std::cin.get();
}

void BuscarEnIndiceMenu() {
    std::cout << "\n🔍 === BÚSQUEDA EN ÍNDICE ===" << std::endl;
    std::string tabla = GetStringInput("📁 Nombre de la tabla: ");
    std::string columna = GetStringInput("📋 Nombre de la columna: ");
    
    if (!g_gestor_indices->ExisteIndice(tabla, columna)) {
        std::cout << "⚠️ No existe un índice para la tabla '" << tabla << "', columna '" << columna << "'" << std::endl;
        return;
    }
    
    TipoIndice tipo = g_gestor_indices->ObtenerTipoIndice(tabla, columna);
    
    if (tipo == TipoIndice::BTREE_ENTERO) {
        int clave = GetNumericInput<int>("🔢 Clave entera a buscar: ");
        auto resultado = g_gestor_indices->BuscarEnIndice(tabla, columna, "", clave);
        
        if (resultado && !resultado->empty()) {
            std::cout << "\n✅ Registros encontrados: " << resultado->size() << std::endl;
            std::cout << "📍 IDs de registros: ";
            for (const auto& id : *resultado) {
                std::cout << id << " ";
            }
            std::cout << std::endl;
        } else {
            std::cout << "\n❌ No se encontraron registros con esa clave" << std::endl;
        }
    } else {
        std::string clave = GetStringInput("🔤 Clave de cadena a buscar: ");
        auto resultado = g_gestor_indices->BuscarEnIndice(tabla, columna, clave, 0);
        
        if (resultado && !resultado->empty()) {
            std::cout << "\n✅ Registros encontrados: " << resultado->size() << std::endl;
            std::cout << "📍 IDs de registros: ";
            for (const auto& id : *resultado) {
                std::cout << id << " ";
            }
            std::cout << std::endl;
        } else {
            std::cout << "\n❌ No se encontraron registros con esa clave" << std::endl;
        }
    }
    
    std::cout << "\nPresione Enter para continuar...";
    std::cin.ignore();
    std::cin.get();
}

void EliminarIndiceMenu() {
    std::cout << "\n🚫 === ELIMINACIÓN DE ÍNDICE ===" << std::endl;
    std::string tabla = GetStringInput("📁 Nombre de la tabla: ");
    std::string columna = GetStringInput("📋 Nombre de la columna: ");
    
    if (!g_gestor_indices->ExisteIndice(tabla, columna)) {
        std::cout << "⚠️ No existe un índice para la tabla '" << tabla << "', columna '" << columna << "'" << std::endl;
        return;
    }
    
    std::cout << "\n⚠️ ¿Está seguro de que desea eliminar este índice? (s/n): ";
    char confirmacion;
    std::cin >> confirmacion;
    
    if (confirmacion == 's' || confirmacion == 'S') {
        Status status = g_gestor_indices->EliminarIndice(tabla, columna);
        
        if (status == Status::OK) {
            std::cout << "\n✅ Índice eliminado exitosamente" << std::endl;
        } else {
            std::cout << "\n❌ Error al eliminar el índice" << std::endl;
        }
    } else {
        std::cout << "\n❌ Operación cancelada" << std::endl;
    }
    
    std::cout << "\nPresione Enter para continuar...";
    std::cin.ignore();
    std::cin.get();
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
            case 3: ViewBufferPoolTable(); break;
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

void HandleAdvancedTableManagement() {
    if (!g_gestor_tablas_avanzado) {
        std::cout << "Gestor de Tablas Avanzado no inicializado. Cree o cargue un disco primero." << std::endl;
        return;
    }
    int choice;
    do {
        DisplayAdvancedTableManagementMenu();
        choice = GetNumericInput<int>("");

        switch (choice) {
            case 1: CrearTablaPorArchivoAvanzada(); break;
            case 2: CrearTablaPorFormularioAvanzada(); break;
            case 3: InsertarRegistrosPorCSVAvanzado(); break;
            case 4: InsertarRegistroPorFormularioAvanzado(); break;
            case 5: EliminarRegistrosPorCondicionesAvanzado(); break;
            case 6: VerEspaciosDisponibles(); break;
            case 7: VerEstadisticasGestorAvanzado(); break;
            case 8: std::cout << "Volviendo al Menú Principal." << std::endl; break;
            default: std::cout << "Opción inválida. Intente de nuevo." << std::endl; break;
        }
    } while (choice != 8);
}

void HandleIndexManagement() {
    if (!g_gestor_indices) {
        std::cout << "Gestor de Índices no inicializado. Cargue o cree un disco primero." << std::endl;
        return;
    }
    int choice;
    do {
        std::cout << "\n🔍 === GESTIÓN AVANZADA DE ÍNDICES ===" << std::endl;
        std::cout << "1. 🤖 Crear Índice Automático (Estrategia Inteligente)" << std::endl;
        std::cout << "2. 📁 Crear Múltiples Índices Automáticos por Tabla" << std::endl;
        std::cout << "3. 💾 Cargar Índices Automáticamente desde Disco" << std::endl;
        std::cout << "4. 🔨 Crear Índice Manual (Tipo Específico)" << std::endl;
        std::cout << "5. 🔍 Buscar en Índice" << std::endl;
        std::cout << "6. 📄 Listar Todos los Índices" << std::endl;
        std::cout << "7. 📊 Ver Estadísticas de Índices" << std::endl;
        std::cout << "8. 💾 Persistir Todos los Índices" << std::endl;
        std::cout << "9. 🚫 Eliminar Índice" << std::endl;
        std::cout << "10. ⬅️ Volver al menú principal" << std::endl;
        choice = GetNumericInput<int>("Opción: ");
        
        if (choice == 1) {
            // Crear Índice Automático (Estrategia Inteligente)
            CrearIndiceAutomaticoMenu();
            
        } else if (choice == 2) {
            // Crear Múltiples Índices Automáticos por Tabla
            CrearIndicesAutomaticosPorTablaMenu();
            
        } else if (choice == 3) {
            // Cargar Índices Automáticamente desde Disco
            CargarIndicesAutomaticamenteMenu();
            
        } else if (choice == 4) {
            // Crear Índice Manual (Tipo Específico)
            CrearIndiceManualMenu();
            
        } else if (choice == 5) {
            // Buscar en Índice
            BuscarEnIndiceMenu();
            
        } else if (choice == 6) {
            // Listar Todos los Índices
            g_gestor_indices->ImprimirTodosLosIndices();
            
        } else if (choice == 7) {
            // Ver Estadísticas de Índices
            g_gestor_indices->ImprimirEstadisticasGenerales();
            
        } else if (choice == 8) {
            // Persistir Todos los Índices
            std::cout << "\n💾 Persistiendo todos los índices..." << std::endl;
            Status status = g_gestor_indices->PersistirTodosLosIndices();
            if (status == Status::OK) {
                std::cout << "✅ Todos los índices persistidos exitosamente" << std::endl;
            } else {
                std::cout << "❌ Error al persistir índices" << std::endl;
            }
            
        } else if (choice == 9) {
            // Eliminar Índice
            EliminarIndiceMenu();
            
        } else if (choice != 10) {
            std::cout << "⚠️ Opción inválida. Intente nuevamente." << std::endl;
        }
        
    } while (choice != 10);
}

void HandleQueryProcessor() {
    if (!g_record_manager || !g_catalog_manager || !g_index_manager) {
        std::cout << "Managers no inicializados. Cargue o cree un disco primero." << std::endl;
        return;
    }
    
    std::cout << "\n=== PROCESADOR DE CONSULTAS SQL ===" << std::endl;
    std::cout << "Soporta entrada manual y desde archivos de audio." << std::endl;
    
    // Usar la nueva función de entrada dual
    std::string query = ObtenerConsultaSQL();
    
    if (query.empty()) {
        std::cout << "Error: No se obtuvo ninguna consulta válida." << std::endl;
        return;
    }
    
    std::cout << "\n--- Procesando Consulta ---" << std::endl;
    std::cout << "Consulta a ejecutar: " << query << std::endl;
    std::smatch match;

    // --- INSERT ---
    std::regex insert_regex(R"(INSERT\s+INTO\s+(\w+)\s+VALUES\s*\(([^\)]*)\))", std::regex::icase);
    if (std::regex_match(query, match, insert_regex)) {
        std::string table_name = match[1];
        std::string values_str = match[2];
        
        FullTableSchema schema;
        if (g_catalog_manager->GetTableSchema(table_name, schema) != Status::OK) {
            std::cout << "Error: Tabla '" << table_name << "' no encontrada." << std::endl;
            return;
        }

        RecordData new_record_data = PrepareRecordData(values_str, schema);
        if (new_record_data.fields.empty() && !values_str.empty()) {
            std::cerr << "Fallo al preparar los datos del registro para la inserción. Consulta cancelada." << std::endl;
            return;
        }

        uint32_t slot_id;
        PageId target_page_id = 0;
        Status status = Status::ERROR;

        for (PageId page_id : schema.base_metadata.data_page_ids) {
            BlockSizeType free_space;
            Status get_space_status = g_record_manager->GetFreeSpace(page_id, free_space);
            // Estimación del tamaño del registro serializado para verificar espacio
            size_t estimated_record_size = new_record_data.fields.size() * 10 + sizeof(SlotDirectoryEntry); // Estimación simple
            if (get_space_status == Status::OK && free_space >= estimated_record_size) {
                status = g_record_manager->InsertRecord(page_id, new_record_data, slot_id);
                if (status == Status::OK) {
                    target_page_id = page_id;
                    break;
                }
            }
        }
        
        if (status != Status::OK) { // Si no se encontró espacio en ninguna página existente
            PageId new_data_page_id;
            Byte* new_page_data = g_buffer_manager->NewPage(new_data_page_id, PageType::DATA_PAGE);
            if (new_page_data == nullptr) {
                std::cerr << "Error: Fallo al crear una nueva página de datos para la tabla." << std::endl;
                return;
            }
            Status init_status = g_record_manager->InitDataPage(new_data_page_id);
            if (init_status != Status::OK) {
                std::cerr << "Error: Fallo al inicializar la nueva página de datos." << std::endl;
                g_buffer_manager->DeletePage(new_data_page_id);
                return;
            }
            Status add_page_status = g_catalog_manager->AddDataPageToTable(table_name, new_data_page_id);
            if (add_page_status != Status::OK) {
                std::cerr << "Error: Fallo al añadir la nueva página al catálogo de la tabla." << std::endl;
                return;
            }
            status = g_record_manager->InsertRecord(new_data_page_id, new_record_data, slot_id);
            if (status == Status::OK) {
                target_page_id = new_data_page_id;
            }
        }

        if (status == Status::OK) {
            std::cout << "Registro insertado exitosamente en Page " << target_page_id << ", Slot " << slot_id << "." << std::endl;
            schema.base_metadata.num_records++;
            g_catalog_manager->UpdateTableNumRecords(table_name, schema.base_metadata.num_records);
        } else {
            std::cerr << "Error al insertar registro: " << StatusToString(status) << std::endl;
        }
        return;
    }

    // --- SELECT multi-condición ---
    std::regex select_regex(R"(SELECT\s+([\w\*, ]+)\s+FROM\s+(\w+)(?:\s+WHERE\s+(.+))?)", std::regex::icase);
    if (std::regex_match(query, match, select_regex)) {
        std::string select_cols_str = match[1];
        std::string table_name = match[2];
        std::string where_str;
        bool has_where = false;
        if (match.size() > 3 && match[3].matched) {
            where_str = match[3];
            has_where = true;
        }

        FullTableSchema schema;
        if (g_catalog_manager->GetTableSchema(table_name, schema) != Status::OK) {
            std::cout << "Error: Tabla '" << table_name << "' no encontrada." << std::endl;
            return;
        }

        std::vector<int> select_idxs;
        if (select_cols_str == "*" || select_cols_str == " *") {
            for (size_t i = 0; i < schema.columns.size(); ++i) select_idxs.push_back(i);
        } else {
            std::stringstream ss(select_cols_str);
            std::string col_name;
            while (std::getline(ss, col_name, ',')) {
                col_name.erase(std::remove_if(col_name.begin(), col_name.end(), ::isspace), col_name.end());
                bool found = false;
                for (size_t i = 0; i < schema.columns.size(); ++i) {
                    // Comparar sin distinción de mayúsculas y minúsculas
                    std::string schema_col_name(schema.columns[i].name);
                    std::transform(schema_col_name.begin(), schema_col_name.end(), schema_col_name.begin(), ::tolower);
                    std::transform(col_name.begin(), col_name.end(), col_name.begin(), ::tolower);
                    if (col_name == schema_col_name) {
                        select_idxs.push_back(i);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    std::cerr << "Advertencia: Columna '" << col_name << "' no encontrada en el esquema de la tabla '" << table_name << "'." << std::endl;
                }
            }
        }

        std::vector<Condition> conds;
        if (has_where) {
            conds = ParseWhereConditions(where_str);
        }

        // Imprimir cabecera de la tabla de resultados
        for (int idx : select_idxs) {
            std::cout << std::left << std::setw(15) << schema.columns[idx].name;
        }
        std::cout << std::endl;
        for (int idx : select_idxs) {
            std::cout << std::string(15, '-');
        }
        std::cout << std::endl;

        uint32_t records_displayed = 0;
        for (PageId page_id : schema.base_metadata.data_page_ids) {
            uint32_t num_slots;
            Byte* page_data_peek = g_buffer_manager->FetchPage(page_id);
            if (page_data_peek == nullptr) {
                std::cerr << "Advertencia: No se pudo obtener la página " << page_id << ". Saltando." << std::endl;
                continue;
            }
            BlockHeader header_peek = g_record_manager->ReadBlockHeader(page_data_peek);
            num_slots = header_peek.num_slots;
            g_buffer_manager->UnpinPage(page_id, false);

            for (uint32_t slot = 0; slot < num_slots; ++slot) {
                RecordData rec_data;
                Status get_rec_status = g_record_manager->GetRecord(page_id, slot, rec_data);
                if (get_rec_status == Status::OK) {
                    bool match = true;
                    if (has_where) {
                        for (const auto& cond : conds) {
                            int col_idx = -1;
                            for (size_t i = 0; i < schema.columns.size(); ++i) {
                                std::string schema_col_name(schema.columns[i].name);
                                std::transform(schema_col_name.begin(), schema_col_name.end(), schema_col_name.begin(), ::tolower);
                                if (cond.col == schema_col_name) { // Cond.col ya está en minúsculas
                                    col_idx = i;
                                    break;
                                }
                            }
                            if (col_idx == -1 || col_idx >= rec_data.fields.size()) {
                                match = false; // Columna de condición no encontrada o fuera de límites
                                break;
                            }
                            // Comparar el valor del registro con el valor de la condición
                            // Se asume que los valores en rec_data.fields ya están en el formato correcto
                            // y que cond.val también está preparado (ej. sin comillas).
                            if (rec_data.fields[col_idx] != cond.val) {
                                match = false;
                                break;
                            }
                        }
                    }

                    if (match) {
                        for (int idx : select_idxs) {
                            if (idx < rec_data.fields.size()) {
                                std::cout << std::left << std::setw(15) << rec_data.fields[idx];
                            } else {
                                std::cout << std::left << std::setw(15) << "N/A";
                            }
                        }
                        std::cout << std::endl;
                        records_displayed++;
                    }
                }
            }
        }
        std::cout << "\nTotal de registros seleccionados: " << records_displayed << std::endl;
        return;
    }

    // --- UPDATE multi-condición y actualización de índices ---
    std::regex update_regex(R"(UPDATE\s+(\w+)\s+SET\s+([\w]+)\s*=\s*('?[^']+'?)\s+WHERE\s+(.+))", std::regex::icase); // Regex mejorada para set_val
    if (std::regex_match(query, match, update_regex)) {
        std::string table_name = match[1];
        std::string set_col_name = match[2];
        std::string set_val_str = match[3];
        std::string where_str = match[4];

        // Eliminar comillas de set_val_str si existen
        if (set_val_str.length() >= 2 && set_val_str.front() == '\'' && set_val_str.back() == '\'') {
            set_val_str = set_val_str.substr(1, set_val_str.length() - 2);
        }

        FullTableSchema schema;
        if (g_catalog_manager->GetTableSchema(table_name, schema) != Status::OK) {
            std::cout << "Error: Tabla '" << table_name << "' no encontrada." << std::endl;
            return;
        }

        int set_col_idx = -1;
        for (size_t i = 0; i < schema.columns.size(); ++i) {
            std::string schema_col_name(schema.columns[i].name);
            std::transform(schema_col_name.begin(), schema_col_name.end(), schema_col_name.begin(), ::tolower);
            std::string temp_set_col_name = set_col_name;
            std::transform(temp_set_col_name.begin(), temp_set_col_name.end(), temp_set_col_name.begin(), ::tolower);
            if (temp_set_col_name == schema_col_name) {
                set_col_idx = i;
                break;
            }
        }
        if (set_col_idx == -1) {
            std::cout << "Error: Columna '" << set_col_name << "' no encontrada en la tabla '" << table_name << "'." << std::endl;
            return;
        }

        std::vector<Condition> conds = ParseWhereConditions(where_str);
        uint32_t records_updated = 0;

        for (PageId page_id : schema.base_metadata.data_page_ids) {
            uint32_t num_slots;
            Byte* page_data_peek = g_buffer_manager->FetchPage(page_id);
            if (page_data_peek == nullptr) {
                std::cerr << "Advertencia: No se pudo obtener la página " << page_id << " para actualización. Saltando." << std::endl;
                continue;
            }
            BlockHeader header_peek = g_record_manager->ReadBlockHeader(page_data_peek);
            num_slots = header_peek.num_slots;
            g_buffer_manager->UnpinPage(page_id, false);

            for (uint32_t slot = 0; slot < num_slots; ++slot) {
                RecordData old_rec_data;
                Status get_rec_status = g_record_manager->GetRecord(page_id, slot, old_rec_data);
                if (get_rec_status == Status::OK) {
                    bool match = true;
                    for (const auto& cond : conds) {
                        int col_idx = -1;
                        for (size_t i = 0; i < schema.columns.size(); ++i) {
                            std::string schema_col_name(schema.columns[i].name);
                            std::transform(schema_col_name.begin(), schema_col_name.end(), schema_col_name.begin(), ::tolower);
                            if (cond.col == schema_col_name) {
                                col_idx = i;
                                break;
                            }
                        }
                        if (col_idx == -1 || col_idx >= old_rec_data.fields.size() || old_rec_data.fields[col_idx] != cond.val) {
                            match = false;
                            break;
                        }
                    }

                    if (match) {
                        RecordData new_rec_data = old_rec_data; // Copiar datos existentes
                        // Validar y convertir el nuevo valor para la columna SET
                        std::vector<std::string> temp_values = {set_val_str};
                        std::vector<ColumnMetadata> temp_cols = {schema.columns[set_col_idx]};
                        std::vector<std::string> validated_set_val;
                        std::string error_msg;
                        if (!ValidateAndConvertValues(temp_values, temp_cols, validated_set_val, error_msg)) {
                            std::cerr << "Error de validación para el valor SET: " << error_msg << ". Saltando actualización." << std::endl;
                            continue; // Saltar a la siguiente ranura
                        }
                        new_rec_data.fields[set_col_idx] = validated_set_val[0]; // Asignar el valor validado

                        Status update_status = g_record_manager->UpdateRecord(page_id, slot, new_rec_data);
                        if (update_status == Status::OK) {
                            std::cout << "Registro actualizado en Page " << page_id << ", Slot " << slot << "." << std::endl;
                            records_updated++;
                        } else {
                            std::cerr << "Error al actualizar registro en Page " << page_id << ", Slot " << slot << ": " << StatusToString(update_status) << std::endl;
                        }
                    }
                }
            }
        }
        std::cout << "\nTotal de registros actualizados: " << records_updated << std::endl;
        return;
    }

    // --- DELETE multi-condición y actualización de índices ---
    std::regex delete_regex(R"(DELETE\s+FROM\s+(\w+)\s+WHERE\s+(.+))", std::regex::icase);
    if (std::regex_match(query, match, delete_regex)) {
        std::string table_name = match[1];
        std::string where_str = match[2];

        FullTableSchema schema;
        if (g_catalog_manager->GetTableSchema(table_name, schema) != Status::OK) {
            std::cout << "Error: Tabla '" << table_name << "' no encontrada." << std::endl;
            return;
        }

        std::vector<Condition> conds = ParseWhereConditions(where_str);
        uint32_t records_deleted = 0;
        
        // Recopilar PageId y SlotId de los registros a eliminar
        std::vector<std::pair<PageId, uint32_t>> records_to_delete;

        for (PageId page_id : schema.base_metadata.data_page_ids) {
            uint32_t num_slots;
            Byte* page_data_peek = g_buffer_manager->FetchPage(page_id);
            if (page_data_peek == nullptr) {
                std::cerr << "Advertencia: No se pudo obtener la página " << page_id << " para eliminación. Saltando." << std::endl;
                continue;
            }
            BlockHeader header_peek = g_record_manager->ReadBlockHeader(page_data_peek);
            num_slots = header_peek.num_slots;
            g_buffer_manager->UnpinPage(page_id, false);

            for (uint32_t slot = 0; slot < num_slots; ++slot) {
                RecordData rec_data;
                Status get_rec_status = g_record_manager->GetRecord(page_id, slot, rec_data);
                if (get_rec_status == Status::OK) {
                    bool match = true;
                    for (const auto& cond : conds) {
                        int col_idx = -1;
                        for (size_t i = 0; i < schema.columns.size(); ++i) {
                            std::string schema_col_name(schema.columns[i].name);
                            std::transform(schema_col_name.begin(), schema_col_name.end(), schema_col_name.begin(), ::tolower);
                            if (cond.col == schema_col_name) {
                                col_idx = i;
                                break;
                            }
                        }
                        if (col_idx == -1 || col_idx >= rec_data.fields.size() || rec_data.fields[col_idx] != cond.val) {
                            match = false;
                            break;
                        }
                    }
                    if (match) {
                        records_to_delete.push_back({page_id, slot});
                    }
                }
            }
        }

        // Ahora eliminar los registros y actualizar los contadores
        for (const auto& rec_loc : records_to_delete) {
            PageId page_id = rec_loc.first;
            uint32_t slot = rec_loc.second;
            Status delete_status = g_record_manager->DeleteRecord(page_id, slot);
            if (delete_status == Status::OK) {
                std::cout << "Registro eliminado en Page " << page_id << ", Slot " << slot << "." << std::endl;
                records_deleted++;
                // Actualizar num_records en TableMetadata.
                if (schema.base_metadata.num_records > 0) {
                    schema.base_metadata.num_records--;
                    g_catalog_manager->UpdateTableNumRecords(table_name, schema.base_metadata.num_records);
                }
            } else {
                std::cerr << "Error al eliminar registro en Page " << page_id << ", Slot " << slot << ": " << StatusToString(delete_status) << std::endl;
            }
        }
        std::cout << "\nTotal de registros eliminados: " << records_deleted << std::endl;
        return;
    }

    std::cout << "Consulta no reconocida o formato no soportado." << std::endl;
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
            case 5: HandleIndexManagement(); break;
            case 6: HandleAdvancedTableManagement(); break;
            case 7: HandleQueryProcessor(); break;
            case 8: HandleAudioManagement(); break;
            case 9: std::cout << "Saliendo del SGBD. ¡Adiós!" << std::endl; break;
            default: std::cout << "Opción inválida. Intente de nuevo." << std::endl; break;
        }
    } while (choice != 9);

    // Los destructores de unique_ptr se encargarán de liberar los managers.
    // FlushAllPages se llama en el destructor del BufferManager.

    return 0;
}
