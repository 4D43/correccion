// data_storage/disk_manager.cpp
#include "disk_manager.h"
#include <iostream> // Para std::cout, std::cerr
#include <iomanip>  // Para std::setw, std::setfill
#include <sstream>  // Para std::stringstream
#include <algorithm> // Para std::min, std::fill
#include <fstream>  // Para std::ifstream, std::ofstream
#include <vector>   // Para serializar/deserializar el mapa de bits
#include <cstring> // Para std::memcpy

namespace fs = std::filesystem; // Alias para std::filesystem

// Constructor del DiskManager.
DiskManager::DiskManager(const std::string& disk_name,
                         uint32_t num_platters,
                         uint32_t num_surfaces_per_platter,
                         uint32_t num_cylinders,
                         uint32_t num_sectors_per_track,
                         BlockSizeType block_size,
                         SectorSizeType sector_size,
                         bool is_new_disk) // Nuevo parámetro
    : disk_name_(disk_name),
      num_platters_(num_platters),
      num_surfaces_per_platter_(num_surfaces_per_platter),
      num_cylinders_(num_cylinders),
      num_sectors_per_track_(num_sectors_per_track),
      block_size_(block_size),
      sector_size_(sector_size),
      next_logical_page_id_(0) // Se inicializa a 0, se carga o se asigna en CreateDiskStructure
{
    // Validaciones básicas
    if (block_size_ == 0 || sector_size_ == 0 || block_size_ % sector_size_ != 0) {
        throw std::invalid_argument("Block size must be a non-zero multiple of sector size.");
    }
    if (num_platters_ == 0 || num_surfaces_per_platter_ == 0 || num_cylinders_ == 0 || num_sectors_per_track_ == 0) {
        throw std::invalid_argument("Disk dimensions cannot be zero.");
    }

    // Inicializar el mapa de estado de sectores.
    // Dimensiones: [cilindros][platos * superficies_por_plato][sectores_por_pista]
    sector_status_map_.resize(num_cylinders_);
    for (uint32_t t = 0; t < num_cylinders_; ++t) {
        sector_status_map_[t].resize(num_platters_ * num_surfaces_per_platter_);
        for (uint32_t ps_idx = 0; ps_idx < num_platters_ * num_surfaces_per_platter_; ++ps_idx) {
            sector_status_map_[t][ps_idx].resize(num_sectors_per_track_, BlockStatus::EMPTY);
        }
    }

    if (is_new_disk) {
        std::cout << "DiskManager: Creando nuevo disco '" << disk_name_ << "'." << std::endl;
        // La estructura del disco se creará explícitamente con CreateDiskStructure()
    } else {
        std::cout << "DiskManager: Intentando cargar disco existente '" << disk_name_ << "'." << std::endl;
        // Los metadatos se cargarán explícitamente con LoadDiskMetadata()
    }
}

// Destructor: Guarda el estado del disco antes de salir.
DiskManager::~DiskManager() {
    std::cout << "DiskManager: Guardando metadatos del disco '" << disk_name_ << "' antes de la destrucción." << std::endl;
    Status status = SaveDiskMetadata();
    if (status != Status::OK) {
        std::cerr << "Error (Destructor DiskManager): Fallo al guardar los metadatos del disco: " << StatusToString(status) << std::endl;
    }
}

// Crea la estructura de directorios y archivos para un nuevo disco.
Status DiskManager::CreateDiskStructure() {
    fs::path disk_root = fs::path("Discos") / disk_name_;
    try {
        if (fs::exists(disk_root)) {
            std::cout << "Advertencia: El directorio del disco '" << disk_name_ << "' ya existe. Eliminando contenido existente..." << std::endl;
            fs::remove_all(disk_root); // Eliminar todo el contenido existente
        }
        fs::create_directories(disk_root); // Crear el directorio raíz del disco

        // Crear subdirectorios para platos, superficies y cilindros
        for (uint32_t p = 0; p < num_platters_; ++p) {
            fs::create_directory(disk_root / ("Plato" + std::to_string(p)));
            for (uint32_t s = 0; s < num_surfaces_per_platter_; ++s) {
                fs::create_directory(disk_root / ("Plato" + std::to_string(p)) / ("Superficie" + std::to_string(s)));
                for (uint32_t t = 0; t < num_cylinders_; ++t) {
                    fs::create_directory(disk_root / ("Plato" + std::to_string(p)) / ("Superficie" + std::to_string(s)) / ("Cilindro" + std::to_string(t)));
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error (CreateDiskStructure): Fallo al crear la estructura de directorios del disco: " << e.what() << std::endl;
        return Status::IO_ERROR;
    }

    // Inicializar el mapa de estado de sectores y crear archivos de bloques lógicos
    Status init_status = InitializeDiskMapAndBlockFiles();
    if (init_status != Status::OK) {
        std::cerr << "Error (CreateDiskStructure): Fallo al inicializar el mapa de disco y los archivos de bloques." << std::endl;
        return init_status;
    }

    // Guardar los metadatos iniciales del disco (incluyendo next_logical_page_id_ y logical_to_physical_map_)
    Status save_status = SaveDiskMetadata();
    if (save_status != Status::OK) {
        std::cerr << "Error (CreateDiskStructure): Fallo al guardar los metadatos iniciales del disco: " << StatusToString(save_status) << std::endl;
        return save_status;
    }

    std::cout << "Estructura de disco creada exitosamente para '" << disk_name_ << "'." << std::endl;
    return Status::OK;
}

// Carga los metadatos del disco desde el archivo de metadatos.
Status DiskManager::LoadDiskMetadata() {
    fs::path metadata_file_path = fs::path("Discos") / disk_name_ / "disk_metadata.dat";
    if (!fs::exists(metadata_file_path)) {
        std::cerr << "Error (LoadDiskMetadata): Archivo de metadatos no encontrado para el disco '" << disk_name_ << "'." << std::endl;
        return Status::NOT_FOUND;
    }

    std::ifstream metadata_file(metadata_file_path, std::ios::binary);
    if (!metadata_file.is_open()) {
        std::cerr << "Error (LoadDiskMetadata): No se pudo abrir el archivo de metadatos para lectura: " << metadata_file_path << std::endl;
        return Status::IO_ERROR;
    }

    // Leer parámetros del disco
    metadata_file.read(reinterpret_cast<char*>(&num_platters_), sizeof(num_platters_));
    metadata_file.read(reinterpret_cast<char*>(&num_surfaces_per_platter_), sizeof(num_surfaces_per_platter_));
    metadata_file.read(reinterpret_cast<char*>(&num_cylinders_), sizeof(num_cylinders_));
    metadata_file.read(reinterpret_cast<char*>(&num_sectors_per_track_), sizeof(num_sectors_per_track_));
    metadata_file.read(reinterpret_cast<char*>(&block_size_), sizeof(block_size_));
    metadata_file.read(reinterpret_cast<char*>(&sector_size_), sizeof(sector_size_));
    metadata_file.read(reinterpret_cast<char*>(&next_logical_page_id_), sizeof(next_logical_page_id_));

    // Redimensionar sector_status_map_ antes de leerlo
    sector_status_map_.resize(num_cylinders_);
    for (uint32_t t = 0; t < num_cylinders_; ++t) {
        sector_status_map_[t].resize(num_platters_ * num_surfaces_per_platter_);
        for (uint32_t ps_idx = 0; ps_idx < num_platters_ * num_surfaces_per_platter_; ++ps_idx) {
            sector_status_map_[t][ps_idx].resize(num_sectors_per_track_);
        }
    }

    // Leer sector_status_map_
    for (uint32_t t = 0; t < num_cylinders_; ++t) {
        for (uint32_t ps_idx = 0; ps_idx < num_platters_ * num_surfaces_per_platter_; ++ps_idx) {
            metadata_file.read(reinterpret_cast<char*>(sector_status_map_[t][ps_idx].data()), num_sectors_per_track_ * sizeof(BlockStatus));
        }
    }

    // Leer logical_to_physical_map_
    uint32_t map_size;
    metadata_file.read(reinterpret_cast<char*>(&map_size), sizeof(map_size));
    logical_to_physical_map_.clear();
    for (uint32_t i = 0; i < map_size; ++i) {
        PageId logical_id;
        PhysicalAddress physical_addr;
        metadata_file.read(reinterpret_cast<char*>(&logical_id), sizeof(logical_id));
        metadata_file.read(reinterpret_cast<char*>(&physical_addr), sizeof(physical_addr));
        logical_to_physical_map_[logical_id] = physical_addr;
    }

    metadata_file.close();
    std::cout << "Metadatos del disco '" << disk_name_ << "' cargados exitosamente." << std::endl;
    return Status::OK;
}

// Guarda los metadatos internos del DiskManager en el archivo de metadatos.
Status DiskManager::SaveDiskMetadata() {
    fs::path metadata_file_path = fs::path("Discos") / disk_name_ / "disk_metadata.dat";
    std::ofstream metadata_file(metadata_file_path, std::ios::binary | std::ios::trunc); // trunc para sobrescribir
    if (!metadata_file.is_open()) {
        std::cerr << "Error (SaveDiskMetadata): No se pudo abrir el archivo de metadatos para escritura: " << metadata_file_path << std::endl;
        return Status::IO_ERROR;
    }

    // Escribir parámetros del disco
    metadata_file.write(reinterpret_cast<const char*>(&num_platters_), sizeof(num_platters_));
    metadata_file.write(reinterpret_cast<const char*>(&num_surfaces_per_platter_), sizeof(num_surfaces_per_platter_));
    metadata_file.write(reinterpret_cast<const char*>(&num_cylinders_), sizeof(num_cylinders_));
    metadata_file.write(reinterpret_cast<const char*>(&num_sectors_per_track_), sizeof(num_sectors_per_track_));
    metadata_file.write(reinterpret_cast<const char*>(&block_size_), sizeof(block_size_));
    metadata_file.write(reinterpret_cast<const char*>(&sector_size_), sizeof(sector_size_));
    metadata_file.write(reinterpret_cast<const char*>(&next_logical_page_id_), sizeof(next_logical_page_id_));

    // Escribir sector_status_map_
    for (uint32_t t = 0; t < num_cylinders_; ++t) {
        for (uint32_t ps_idx = 0; ps_idx < num_platters_ * num_surfaces_per_platter_; ++ps_idx) {
            metadata_file.write(reinterpret_cast<const char*>(sector_status_map_[t][ps_idx].data()), num_sectors_per_track_ * sizeof(BlockStatus));
        }
    }

    // Escribir logical_to_physical_map_
    uint32_t map_size = logical_to_physical_map_.size();
    metadata_file.write(reinterpret_cast<const char*>(&map_size), sizeof(map_size));
    for (const auto& pair : logical_to_physical_map_) {
        metadata_file.write(reinterpret_cast<const char*>(&pair.first), sizeof(pair.first));
        metadata_file.write(reinterpret_cast<const char*>(&pair.second), sizeof(pair.second));
    }

    metadata_file.close();
    return Status::OK;
}


// Inicializa el mapa de estado de sectores y los archivos de bloques lógicos.
Status DiskManager::InitializeDiskMapAndBlockFiles() {
    // Reiniciar el mapa de bits a EMPTY
    for (uint32_t t = 0; t < num_cylinders_; ++t) {
        for (uint32_t ps_idx = 0; ps_idx < num_platters_ * num_surfaces_per_platter_; ++ps_idx) {
            std::fill(sector_status_map_[t][ps_idx].begin(), sector_status_map_[t][ps_idx].end(), BlockStatus::EMPTY);
        }
    }
    logical_to_physical_map_.clear(); // Limpiar el mapeo lógico a físico
    next_logical_page_id_ = 0; // Resetear el contador de PageId

    // Crear archivos vacíos para cada bloque lógico.
    // Aunque la simulación usa RAM, creamos archivos para simular la persistencia
    // y la estructura de un disco real.
    // Esto se hace una vez al crear el disco.
    Block empty_block(block_size_); // Un bloque de ceros
    uint32_t total_logical_blocks = GetTotalLogicalBlocks();
    for (BlockId i = 0; i < total_logical_blocks; ++i) {
        fs::path block_file_path = GetBlockFilePath(i);
        std::ofstream block_file(block_file_path, std::ios::binary | std::ios::trunc);
        if (!block_file.is_open()) {
            std::cerr << "Error (InitializeDiskMapAndBlockFiles): No se pudo crear el archivo de bloque: " << block_file_path << std::endl;
            return Status::IO_ERROR;
        }
        block_file.write(empty_block.GetData(), block_size_);
        block_file.close();
    }
    std::cout << "Archivos de bloques lógicos inicializados." << std::endl;
    return Status::OK;
}


// Asigna un nuevo bloque lógico en el disco.
Status DiskManager::AllocateBlock(PageType page_type, BlockId& block_id) {
    uint32_t sectors_needed = GetSectorsPerBlock();
    PhysicalAddress allocated_address;
    bool found = false;

    // Estrategia de asignación:
    // 1. Intentar encontrar un bloque INCOMPLETE (parcialmente usado) para reutilizar.
    //    Esto ayuda a reducir la fragmentación y a consolidar el espacio.
    for (uint32_t t = 0; t < num_cylinders_ && !found; ++t) {
        for (uint32_t ps_idx = 0; ps_idx < num_platters_ * num_surfaces_per_platter_ && !found; ++ps_idx) {
            for (uint32_t sec = 0; sec < num_sectors_per_track_; sec += sectors_needed) {
                if (sector_status_map_[t][ps_idx][sec] == BlockStatus::INCOMPLETE) {
                    allocated_address = PhysicalAddress(ps_idx / num_surfaces_per_platter_, ps_idx % num_surfaces_per_platter_, t, sec);
                    found = true;
                    // Buscar el PageId lógico asociado a esta dirección física
                    for (const auto& pair : logical_to_physical_map_) {
                        if (pair.second == allocated_address) {
                            block_id = pair.first;
                            break;
                        }
                    }
                    if (found) break;
                }
            }
        }
    }

    // 2. Si no se encontró un bloque INCOMPLETE, buscar un bloque EMPTY.
    if (!found) {
        for (uint32_t t = 0; t < num_cylinders_ && !found; ++t) {
            for (uint32_t ps_idx = 0; ps_idx < num_platters_ * num_surfaces_per_platter_ && !found; ++ps_idx) {
                for (uint32_t sec = 0; sec < num_sectors_per_track_; sec += sectors_needed) {
                    if (sector_status_map_[t][ps_idx][sec] == BlockStatus::EMPTY) {
                        allocated_address = PhysicalAddress(ps_idx / num_surfaces_per_platter_, ps_idx % num_surfaces_per_platter_, t, sec);
                        found = true;
                        break;
                    }
                }
            }
        }
    }

    if (!found) {
        std::cerr << "Error (AllocateBlock): No se encontró espacio libre en el disco." << std::endl;
        return Status::DISK_FULL;
    }

    // Asignar un nuevo PageId lógico si es un bloque completamente nuevo
    if (logical_to_physical_map_.find(next_logical_page_id_) != logical_to_physical_map_.end() && logical_to_physical_map_[next_logical_page_id_] == allocated_address) {
        // Si el next_logical_page_id_ ya mapea a esta dirección, significa que la estamos reutilizando.
        // No necesitamos incrementar next_logical_page_id_.
    } else {
        block_id = next_logical_page_id_++; // Asignar el nuevo ID y luego incrementar
    }
    
    logical_to_physical_map_[block_id] = allocated_address; // Registrar el mapeo

    // Marcar los sectores como ocupados (INCOMPLETE o FULL, dependiendo del uso inicial)
    // Para una asignación inicial, lo marcamos como INCOMPLETE.
    UpdateBlockStatus(block_id, BlockStatus::INCOMPLETE);

    std::cout << "Bloque lógico " << block_id << " asignado en dirección física: "
              << "P" << allocated_address.platter_id
              << " S" << allocated_address.surface_id
              << " T" << allocated_address.track_id
              << " Sec" << allocated_address.sector_id << std::endl;
    
    // Guardar los metadatos después de cada asignación para persistencia
    Status save_status = SaveDiskMetadata();
    if (save_status != Status::OK) {
        std::cerr << "Advertencia (AllocateBlock): Fallo al guardar metadatos del disco después de la asignación." << std::endl;
        // La asignación en memoria ya se realizó, pero la persistencia falló.
    }

    return Status::OK;
}

// Desasigna un bloque lógico del disco, marcando su espacio como libre.
Status DiskManager::DeallocateBlock(BlockId block_id) {
    auto it = logical_to_physical_map_.find(block_id);
    if (it == logical_to_physical_map_.end()) {
        std::cerr << "Error (DeallocateBlock): Bloque lógico " << block_id << " no encontrado en el mapeo." << std::endl;
        return Status::INVALID_BLOCK_ID;
    }

    PhysicalAddress address = it->second;
    uint32_t sectors_per_block = GetSectorsPerBlock();

    // Marcar todos los sectores físicos que componen el bloque como EMPTY
    for (uint32_t i = 0; i < sectors_per_block; ++i) {
        uint32_t current_sector_id = address.sector_id + i;
        if (IsValidAddress(PhysicalAddress(address.platter_id, address.surface_id, address.track_id, current_sector_id))) {
            // Asegurarse de que no excedemos los límites de la pista/cilindro
            if (current_sector_id < num_sectors_per_track_) {
                sector_status_map_[address.track_id][address.platter_id * num_surfaces_per_platter_ + address.surface_id][current_sector_id] = BlockStatus::EMPTY;
            } else {
                std::cerr << "Advertencia (DeallocateBlock): Intento de desasignar sector fuera de límites de pista." << std::endl;
            }
        } else {
            std::cerr << "Advertencia (DeallocateBlock): Intento de desasignar dirección física inválida." << std::endl;
        }
    }

    logical_to_physical_map_.erase(it); // Eliminar el mapeo lógico a físico

    // Opcional: Limpiar el contenido del archivo físico del bloque (llenar con ceros)
    fs::path block_file_path = GetBlockFilePath(block_id);
    if (fs::exists(block_file_path)) {
        std::ofstream block_file(block_file_path, std::ios::binary | std::ios::trunc);
        if (block_file.is_open()) {
            Block empty_block(block_size_);
            block_file.write(empty_block.GetData(), block_size_);
            block_file.close();
        } else {
            std::cerr << "Advertencia (DeallocateBlock): No se pudo abrir el archivo de bloque para limpiar: " << block_file_path << std::endl;
        }
    } else {
        std::cerr << "Advertencia (DeallocateBlock): Archivo de bloque no encontrado para limpiar: " << block_file_path << std::endl;
    }

    std::cout << "Bloque lógico " << block_id << " desasignado exitosamente." << std::endl;
    
    // Guardar los metadatos después de cada desasignación para persistencia
    Status save_status = SaveDiskMetadata();
    if (save_status != Status::OK) {
        std::cerr << "Advertencia (DeallocateBlock): Fallo al guardar metadatos del disco después de la desasignación." << std::endl;
    }

    return Status::OK;
}

// Lee un bloque de datos del disco físico.
Status DiskManager::ReadBlock(BlockId block_id, Block& block) {
    auto it = logical_to_physical_map_.find(block_id);
    if (it == logical_to_physical_map_.end()) {
        std::cerr << "Error (ReadBlock): Bloque lógico " << block_id << " no encontrado en el mapeo." << std::endl;
        return Status::INVALID_BLOCK_ID;
    }

    // En esta simulación, los datos del bloque lógico se guardan en un único archivo
    // identificado por el BlockId.
    fs::path block_file_path = GetBlockFilePath(block_id);
    std::ifstream file(block_file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error (ReadBlock): No se pudo abrir el archivo de bloque para lectura: " << block_file_path << std::endl;
        return Status::IO_ERROR;
    }

    // Asegurarse de que el bloque tenga el tamaño correcto antes de leer
    if (block.GetSize() != block_size_) {
        block.Resize(block_size_);
    }
    file.read(block.GetMutableData(), block_size_);
    file.close();

    return Status::OK;
}

// Escribe un bloque de datos al disco físico.
Status DiskManager::WriteBlock(BlockId block_id, const Block& block) {
    auto it = logical_to_physical_map_.find(block_id);
    if (it == logical_to_physical_map_.end()) {
        std::cerr << "Error (WriteBlock): Bloque lógico " << block_id << " no encontrado en el mapeo." << std::endl;
        return Status::INVALID_BLOCK_ID;
    }

    // En esta simulación, los datos del bloque lógico se guardan en un único archivo
    // identificado por el BlockId.
    fs::path block_file_path = GetBlockFilePath(block_id);
    std::ofstream file(block_file_path, std::ios::binary | std::ios::trunc); // trunc para sobrescribir
    if (!file.is_open()) {
        std::cerr << "Error (WriteBlock): No se pudo abrir el archivo de bloque para escritura: " << block_file_path << std::endl;
        return Status::IO_ERROR;
    }

    if (block.GetSize() != block_size_) {
        std::cerr << "Error (WriteBlock): El tamaño del bloque a escribir (" << block.GetSize()
                  << ") no coincide con el tamaño de bloque del disco (" << block_size_ << ")." << std::endl;
        file.close();
        return Status::INVALID_PARAMETER;
    }

    file.write(block.GetData(), block_size_);
    file.close();

    return Status::OK;
}

// Actualiza el estado de un bloque lógico en el mapa de bits del disco.
void DiskManager::UpdateBlockStatus(BlockId block_id, BlockStatus status) {
    auto it = logical_to_physical_map_.find(block_id);
    if (it == logical_to_physical_map_.end()) {
        std::cerr << "Advertencia (UpdateBlockStatus): Bloque lógico " << block_id << " no encontrado para actualizar estado." << std::endl;
        return;
    }
    PhysicalAddress address = it->second;
    uint32_t sectors_per_block = GetSectorsPerBlock();

    // Solo necesitamos actualizar el primer sector del bloque lógico para representar su estado.
    // Los otros sectores dentro del mismo bloque lógico se asumen que tienen el mismo estado.
    if (IsValidAddress(address)) {
        sector_status_map_[address.track_id][address.platter_id * num_surfaces_per_platter_ + address.surface_id][address.sector_id] = status;
        // No guardamos metadata aquí, se hace en Allocate/Deallocate o en el destructor.
    } else {
        std::cerr << "Advertencia (UpdateBlockStatus): Dirección física inválida para el bloque " << block_id << std::endl;
    }
}


// Método auxiliar para obtener la ruta de un sector físico.
std::filesystem::path DiskManager::GetSectorPath(const PhysicalAddress& address) const {
    fs::path disk_root = fs::path("Discos") / disk_name_;
    return disk_root / ("Plato" + std::to_string(address.platter_id)) /
           ("Superficie" + std::to_string(address.surface_id)) /
           ("Cilindro" + std::to_string(address.track_id)) /
           ("Sector" + std::to_string(address.sector_id) + ".dat");
}

// Método auxiliar para obtener la ruta de un archivo de bloque lógico (ahora solo representativo).
std::filesystem::path DiskManager::GetBlockFilePath(BlockId block_id) const {
    fs::path disk_root = fs::path("Discos") / disk_name_;
    // Los bloques lógicos se almacenan directamente en el directorio raíz del disco simulado
    // para simplificar la E/S, ya que el mapeo físico es solo una simulación de ubicación.
    return disk_root / ("Block_" + std::to_string(block_id) + ".dat");
}

// Método auxiliar para verificar si una dirección física es válida.
bool DiskManager::IsValidAddress(const PhysicalAddress& address) const {
    return address.platter_id < num_platters_ &&
           address.surface_id < num_surfaces_per_platter_ &&
           address.track_id < num_cylinders_ &&
           address.sector_id < num_sectors_per_track_;
}

// Método auxiliar para encontrar un bloque contiguo libre en un rango específico de sectores.
// Retorna true y actualiza allocated_address si se encuentra, false en caso contrario.
// Prioriza bloques INCOMPLETE.
bool DiskManager::FindContiguousBlock(uint32_t start_sector, uint32_t end_sector,
                                      PhysicalAddress& allocated_address, uint32_t sectors_needed,
                                      bool prioritize_incomplete) {
    uint32_t combined_platter_surface_count = num_platters_ * num_surfaces_per_platter_;

    // Priorizar bloques INCOMPLETE si se solicita
    if (prioritize_incomplete) {
        for (uint32_t t = 0; t < num_cylinders_; ++t) {
            for (uint32_t ps_idx = 0; ps_idx < combined_platter_surface_count; ++ps_idx) {
                for (uint32_t sec = start_sector; sec <= end_sector; sec += sectors_needed) {
                    if (sec + sectors_needed > num_sectors_per_track_) continue; // Asegurarse de no exceder la pista
                    if (sector_status_map_[t][ps_idx][sec] == BlockStatus::INCOMPLETE) {
                        allocated_address = PhysicalAddress(ps_idx / num_surfaces_per_platter_, ps_idx % num_surfaces_per_platter_, t, sec);
                        return true;
                    }
                }
            }
        }
    }

    // Buscar bloques EMPTY
    for (uint32_t t = 0; t < num_cylinders_; ++t) {
        for (uint32_t ps_idx = 0; ps_idx < combined_platter_surface_count; ++ps_idx) {
            for (uint32_t sec = start_sector; sec <= end_sector; sec += sectors_needed) {
                if (sec + sectors_needed > num_sectors_per_track_) continue; // Asegurarse de no exceder la pista
                bool is_free = true;
                // Verificar que todos los sectores necesarios para el bloque estén libres
                for (uint32_t i = 0; i < sectors_needed; ++i) {
                    if (sector_status_map_[t][ps_idx][sec + i] != BlockStatus::EMPTY) {
                        is_free = false;
                        break;
                    }
                }
                if (is_free) {
                    allocated_address = PhysicalAddress(ps_idx / num_surfaces_per_platter_, ps_idx % num_surfaces_per_platter_, t, sec);
                    return true;
                }
            }
        }
    }
    return false;
}

// Obtiene el número total de sectores físicos en el disco.
uint32_t DiskManager::GetTotalPhysicalSectors() const {
    return num_platters_ * num_surfaces_per_platter_ * num_cylinders_ * num_sectors_per_track_;
}

// Obtiene el número de sectores físicos libres en el disco.
uint32_t DiskManager::GetFreePhysicalSectors() const {
    uint32_t free_count = 0;
    uint32_t combined_platter_surface_count = num_platters_ * num_surfaces_per_platter_;

    for (uint32_t t = 0; t < num_cylinders_; ++t) {
        for (uint32_t ps_idx = 0; ps_idx < combined_platter_surface_count; ++ps_idx) {
            for (uint32_t sec = 0; sec < num_sectors_per_track_; sec += GetSectorsPerBlock()) { // Iterar por el primer sector de cada posible bloque
                if (sector_status_map_[t][ps_idx][sec] == BlockStatus::EMPTY) {
                    free_count += GetSectorsPerBlock();
                }
            }
        }
    }
    return free_count;
}

// Obtiene el número total de bloques lógicos que el disco puede almacenar.
uint32_t DiskManager::GetTotalLogicalBlocks() const {
    return GetTotalPhysicalSectors() / GetSectorsPerBlock();
}

// NUEVAS FUNCIONES IMPLEMENTADAS

// Capacidad total del disco en bytes
uint64_t DiskManager::GetTotalCapacityBytes() const {
    return static_cast<uint64_t>(GetTotalPhysicalSectors()) * sector_size_;
}

// Número de bloques lógicos ocupados
uint32_t DiskManager::GetOccupiedLogicalBlocks() const {
    uint32_t occupied_count = 0;
    uint32_t combined_platter_surface_count = num_platters_ * num_surfaces_per_platter_;
    uint32_t sectors_per_block = GetSectorsPerBlock();

    for (uint32_t t = 0; t < num_cylinders_; ++t) {
        for (uint32_t ps_idx = 0; ps_idx < combined_platter_surface_count; ++ps_idx) {
            for (uint32_t sec = 0; sec < num_sectors_per_track_; sec += sectors_per_block) {
                if (sector_status_map_[t][ps_idx][sec] == BlockStatus::FULL ||
                    sector_status_map_[t][ps_idx][sec] == BlockStatus::INCOMPLETE) {
                    occupied_count++;
                }
            }
        }
    }
    return occupied_count;
}

// Porcentaje de uso del disco
double DiskManager::GetDiskUsagePercentage() const {
    uint32_t total_blocks = GetTotalLogicalBlocks();
    if (total_blocks == 0) return 0.0;
    return (static_cast<double>(GetOccupiedLogicalBlocks()) / total_blocks) * 100.0;
}

// Método para imprimir una representación visual del mapa de estado de bloques
void DiskManager::PrintBlockStatusMap() const {
    std::cout << "\n--- Mapa de Estado de Bloques del Disco ---" << std::endl;
    std::cout << "Leyenda: E=EMPTY, I=INCOMPLETE, F=FULL" << std::endl;

    uint32_t sectors_per_block = GetSectorsPerBlock();
    uint32_t combined_ps_count = num_platters_ * num_surfaces_per_platter_;

    for (uint32_t t = 0; t < num_cylinders_; ++t) {
        std::cout << "Cilindro " << t << ":" << std::endl;
        for (uint32_t ps_idx = 0; ps_idx < combined_ps_count; ++ps_idx) {
            std::cout << "  P" << (ps_idx / num_surfaces_per_platter_)
                      << "S" << (ps_idx % num_surfaces_per_platter_) << ": ";
            for (uint32_t sec = 0; sec < num_sectors_per_track_; sec += sectors_per_block) {
                char status_char = 'E';
                switch (sector_status_map_[t][ps_idx][sec]) {
                    case BlockStatus::EMPTY: status_char = 'E'; break;
                    case BlockStatus::INCOMPLETE: status_char = 'I'; break;
                    case BlockStatus::FULL: status_char = 'F'; break;
                }
                std::cout << status_char << " ";
            }
            std::cout << std::endl;
        }
    }
    std::cout << "------------------------------------------" << std::endl;
}

// Método para imprimir una representación visual del mapeo lógico a físico
void DiskManager::PrintLogicalToPhysicalMap() const {
    std::cout << "\n--- Mapeo Lógico a Físico del Disco ---" << std::endl;
    if (logical_to_physical_map_.empty()) {
        std::cout << "No hay bloques lógicos asignados actualmente." << std::endl;
        return;
    }

    std::cout << std::left << std::setw(12) << "PageId"
              << std::setw(10) << "Platter"
              << std::setw(10) << "Surface"
              << std::setw(10) << "Track"
              << std::setw(10) << "Sector" << std::endl;
    std::cout << std::string(52, '-') << std::endl;

    // Ordenar el mapa por PageId para una salida más legible
    std::vector<std::pair<PageId, PhysicalAddress>> sorted_map;
    for (const auto& pair : logical_to_physical_map_) {
        sorted_map.push_back(pair);
    }
    std::sort(sorted_map.begin(), sorted_map.end(), 
              [](const auto& a, const auto& b) { return a.first < b.first; });

    for (const auto& pair : sorted_map) {
        std::cout << std::left << std::setw(12) << pair.first
                  << std::setw(10) << pair.second.platter_id
                  << std::setw(10) << pair.second.surface_id
                  << std::setw(10) << pair.second.track_id
                  << std::setw(10) << pair.second.sector_id << std::endl;
    }
    std::cout << "------------------------------------------" << std::endl;
}
