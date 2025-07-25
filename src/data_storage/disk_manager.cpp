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
      next_logical_page_id_(1), // PageId 0 es para DISK_METADATA_PAGE
      base_disk_path_("Discos"), // Carpeta base para todos los discos
      current_disk_path_(base_disk_path_ / disk_name), // Ruta específica para este disco
      body_path_(current_disk_path_ / "body"),         // Ruta para la estructura física
      blocks_path_(current_disk_path_ / "blocks")      // Ruta para los archivos de bloques lógicos
{
    // Validar que block_size sea un múltiplo de sector_size
    if (block_size_ == 0 || sector_size_ == 0 || (block_size_ % sector_size_ != 0)) {
        throw std::invalid_argument("Block size must be a non-zero multiple of sector size.");
    }
    // Validar que el número de sectores por pista sea suficiente para al menos un bloque.
    if (num_sectors_per_track_ < GetSectorsPerBlock()) {
        throw std::invalid_argument("Number of sectors per track must be at least the number of sectors per block.");
    }
    // Validar que el número de platos sea par.
    if (num_platters_ % 2 != 0) {
        throw std::invalid_argument("Number of platters must be an even number.");
    }

    // Inicializa el mapa de estado de sectores con el tamaño correcto, pero no con valores.
    // Los valores se cargarán si es un disco existente, o se inicializarán en CreateDiskStructure.
    uint32_t combined_platter_surface_count = num_platters_ * num_surfaces_per_platter_;
    sector_status_map_.resize(num_cylinders_);
    for (uint32_t t = 0; t < num_cylinders_; ++t) {
        sector_status_map_[t].resize(combined_platter_surface_count);
        for (uint32_t ps_idx = 0; ps_idx < combined_platter_surface_count; ++ps_idx) {
            sector_status_map_[t][ps_idx].resize(num_sectors_per_track_);
        }
    }

    // Si es un disco nuevo, el next_logical_page_id_ ya está en 1.
    // Si se carga un disco, este valor se sobrescribirá en LoadDiskMetadata.
    // El logical_to_physical_map_ se llenará en LoadDiskMetadata o AllocateBlock.

    std::cout << "DiskManager inicializado para disco: " << disk_name_ << std::endl;
    std::cout << "Tamaño de bloque lógico: " << block_size_ << " bytes" << std::endl;
    std::cout << "Tamaño de sector físico: " << sector_size_ << " bytes" << std::endl;
    std::cout << "Sectores físicos por bloque lógico: " << GetSectorsPerBlock() << std::endl;
    std::cout << "Total de sectores físicos disponibles: " << GetTotalPhysicalSectors() << std::endl;
    std::cout << "Total de bloques lógicos disponibles: " << GetTotalLogicalBlocks() << std::endl;
}

// Método auxiliar para obtener la ruta de un sector físico.
fs::path DiskManager::GetSectorPath(const PhysicalAddress& address) const {
    // Formato de ruta: Discos/MiDisco/body/Plato_0/Superficie_0/Pista_0/Sector_0.txt
    std::stringstream ss;
    ss << "Plato_" << address.platter_id;
    fs::path platter_path = body_path_ / ss.str();
    ss.str(""); // Limpiar stringstream
    ss << "Superficie_" << address.surface_id;
    fs::path surface_path = platter_path / ss.str();
    ss.str(""); // Limpiar stringstream
    ss << "Pista_" << address.track_id; // track_id es el cilindro ID
    fs::path track_path = surface_path / ss.str();
    ss.str(""); // Limpiar stringstream
    ss << "Sector_" << address.sector_id << ".txt";
    return track_path / ss.str();
}

// Método auxiliar para obtener la ruta de un archivo de bloque lógico (ahora solo representativo).
fs::path DiskManager::GetBlockFilePath(BlockId block_id) const {
    // Formato de ruta: Discos/MiDisco/blocks/block_00000.bin
    std::stringstream ss;
    ss << "block_" << std::setw(5) << std::setfill('0') << block_id << ".bin";
    return blocks_path_ / ss.str();
}

// Método auxiliar para verificar si una dirección física es válida.
bool DiskManager::IsValidAddress(const PhysicalAddress& address) const {
    return address.platter_id < num_platters_ &&
           address.surface_id < num_surfaces_per_platter_ &&
           address.track_id < num_cylinders_ &&
           address.sector_id < num_sectors_per_track_;
}

// Inicializa el mapa de estado de sectores y los archivos de bloques lógicos.
Status DiskManager::InitializeDiskMapAndBlockFiles() {
    std::cout << "Inicializando mapa de disco y archivos de bloques..." << std::endl;
    uint32_t block_id_counter = 0; // Para los archivos representativos de bloques
    uint32_t combined_platter_surface_count = num_platters_ * num_surfaces_per_platter_;

    for (uint32_t t = 0; t < num_cylinders_; ++t) { // Iterar por cilindros (pistas)
        for (uint32_t ps_idx = 0; ps_idx < combined_platter_surface_count; ++ps_idx) {
            for (uint32_t sec = 0; sec < num_sectors_per_track_; ++sec) {
                // Marcar todos los sectores como EMPTY inicialmente en la nueva estructura
                // Solo el primer sector de un bloque almacena el BlockStatus.
                if (sec % GetSectorsPerBlock() == 0) {
                    sector_status_map_[t][ps_idx][sec] = BlockStatus::EMPTY;
                } else {
                    // Los sectores subsiguientes dentro de un bloque no tienen un BlockStatus independiente.
                    // Podríamos usar un valor "INVALID" o simplemente ignorarlos.
                    // Para evitar confusiones, los inicializamos a EMPTY también, pero su valor real
                    // solo importa para el primer sector del bloque.
                    sector_status_map_[t][ps_idx][sec] = BlockStatus::EMPTY;
                }

                // Crear el archivo de bloque lógico representativo si es el primer sector de un bloque.
                // Esto es solo para visualización, no para la E/S real del DiskManager.
                if (sec % GetSectorsPerBlock() == 0) {
                    fs::path block_file_path = GetBlockFilePath(block_id_counter++);
                    std::ofstream block_file(block_file_path, std::ios::binary);
                    if (!block_file.is_open()) {
                        std::cerr << "Error: No se pudo crear el archivo de bloque representativo: " << block_file_path << std::endl;
                        return Status::IO_ERROR;
                    }
                    // Escribir BLOCK_SIZE bytes de ceros para pre-asignar espacio en el archivo representativo
                    // Nota: Este archivo es solo una representación, no se usa para E/S real de datos.
                    std::vector<char> empty_block_data(block_size_, 0);
                    block_file.write(empty_block_data.data(), block_size_);
                    block_file.close();
                }

                // Crear el archivo de sector físico y llenarlo con ceros
                PhysicalAddress current_sector_addr(ps_idx / num_surfaces_per_platter_, ps_idx % num_surfaces_per_platter_, t, sec);
                fs::path sector_file_path = GetSectorPath(current_sector_addr);
                std::ofstream sector_file(sector_file_path, std::ios::binary);
                if (!sector_file.is_open()) {
                    std::cerr << "Error: No se pudo crear el archivo de sector físico: " << sector_file_path << std::endl;
                    return Status::IO_ERROR;
                }
                std::vector<char> empty_sector_data(sector_size_, 0);
                sector_file.write(empty_sector_data.data(), sector_size_);
                sector_file.close();
            }
        }
    }
    std::cout << "Mapa de disco y archivos de bloques/sectores inicializados." << std::endl;
    return Status::OK;
}


// Crea la estructura de directorios y archivos que representan el disco físico.
Status DiskManager::CreateDiskStructure() {
    std::cout << "Creando estructura de disco para: " << disk_name_ << " en " << current_disk_path_ << std::endl;

    // 1. Crear la carpeta base 'Discos' si no existe.
    if (!fs::exists(base_disk_path_)) {
        if (!fs::create_directory(base_disk_path_)) {
            std::cerr << "Error: No se pudo crear el directorio base 'Discos'." << std::endl;
            return Status::IO_ERROR;
        }
    }

    // 2. Crear la subcarpeta con el nombre del disco.
    if (fs::exists(current_disk_path_)) {
        std::cout << "Advertencia: El disco '" << disk_name_ << "' ya existe. Eliminando contenido existente..." << std::endl;
        fs::remove_all(current_disk_path_); // Eliminar contenido existente para empezar limpio
    }
    if (!fs::create_directory(current_disk_path_)) {
        std::cerr << "Error: No se pudo crear el directorio del disco: " << current_disk_path_ << std::endl;
        return Status::IO_ERROR;
    }

    // 3. Crear las subcarpetas 'body' y 'blocks'.
    if (!fs::create_directory(body_path_)) {
        std::cerr << "Error: No se pudo crear el directorio 'body': " << body_path_ << std::endl;
        return Status::IO_ERROR;
    }
    if (!fs::create_directory(blocks_path_)) {
        std::cerr << "Error: No se pudo crear el directorio 'blocks': " << blocks_path_ << std::endl;
        return Status::IO_ERROR;
    }

    // 4. Generar la estructura física del disco (Platos, Superficies, Pistas, Sectores).
    for (uint32_t p = 0; p < num_platters_; ++p) {
        fs::path platter_path = body_path_ / ("Plato_" + std::to_string(p));
        if (!fs::create_directory(platter_path)) {
            std::cerr << "Error: No se pudo crear el directorio Plato_" << p << std::endl;
            return Status::IO_ERROR;
        }

        for (uint32_t s = 0; s < num_surfaces_per_platter_; ++s) {
            fs::path surface_path = platter_path / ("Superficie_" + std::to_string(s));
            if (!fs::create_directory(surface_path)) {
                std::cerr << "Error: No se pudo crear el directorio Superficie_" << s << std::endl;
                return Status::IO_ERROR;
            }

            for (uint32_t t = 0; t < num_cylinders_; ++t) {
                fs::path track_path = surface_path / ("Pista_" + std::to_string(t));
                if (!fs::create_directory(track_path)) {
                    std::cerr << "Error: No se pudo crear el directorio Pista_" << t << std::endl;
                    return Status::IO_ERROR;
                }
            }
        }
    }

    // 5. Inicializar el mapa de sectores y los archivos de bloques/sectores.
    Status init_status = InitializeDiskMapAndBlockFiles();
    if (init_status != Status::OK) {
        return init_status;
    }

    // 6. Asignar y "reservar" la DISK_METADATA_PAGE (PageId 0)
    // Asumimos que la PageId 0 siempre se mapea a PhysicalAddress(0,0,0,0)
    // y que es un bloque de tamaño completo.
    PhysicalAddress metadata_addr = GetDiskMetadataPageAddress();
    uint32_t sectors_needed = GetSectorsPerBlock();
    uint32_t ps_idx = metadata_addr.platter_id * num_surfaces_per_platter_ + metadata_addr.surface_id;

    // Marcar el estado del bloque de metadatos como FULL (ya que siempre está ocupado)
    // Esto solo afecta al primer sector del bloque.
    sector_status_map_[metadata_addr.track_id][ps_idx][metadata_addr.sector_id] = BlockStatus::FULL;

    std::cout << "DISK_METADATA_PAGE (PageId 0) reservada en "
              << "P" << metadata_addr.platter_id << " S" << metadata_addr.surface_id
              << " T" << metadata_addr.track_id << " Sec" << metadata_addr.sector_id
              << " (ocupando " << sectors_needed << " sectores)." << std::endl;
    
    // Registrar el mapeo para PageId 0
    logical_to_physical_map_[0] = metadata_addr;
    next_logical_page_id_ = 1; // El siguiente PageId disponible después del 0

    // 7. Guardar los metadatos del disco después de la creación.
    return SaveDiskMetadata();
}

// Carga la configuración del disco y el mapa de sectores desde la DISK_METADATA_PAGE.
Status DiskManager::LoadDiskMetadata() {
    std::cout << "Cargando metadatos del disco desde " << current_disk_path_ << "..." << std::endl;

    // Verificar si la carpeta del disco existe.
    if (!fs::exists(current_disk_path_)) {
        std::cerr << "Error (LoadDiskMetadata): El disco '" << disk_name_ << "' no existe en " << current_disk_path_ << std::endl;
        return Status::NOT_FOUND;
    }

    // Leer la DISK_METADATA_PAGE (PageId 0)
    Block metadata_block(block_size_); // Crear un bloque temporal para leer
    PhysicalAddress metadata_addr = GetDiskMetadataPageAddress();
    Status read_status = ReadBlock(metadata_addr, metadata_block);
    if (read_status != Status::OK) {
        std::cerr << "Error (LoadDiskMetadata): No se pudo leer la DISK_METADATA_PAGE (PageId 0)." << std::endl;
        return read_status;
    }

    // Deserializar DiskMetadata
    DiskMetadata loaded_metadata;
    std::memcpy(&loaded_metadata, metadata_block.GetData(), sizeof(DiskMetadata));

    // Asignar los valores cargados a los miembros del DiskManager
    disk_name_ = loaded_metadata.disk_name; // Esto puede ser redundante si ya se pasó en el constructor
    num_platters_ = loaded_metadata.num_platters;
    num_surfaces_per_platter_ = loaded_metadata.num_surfaces_per_platter;
    num_cylinders_ = loaded_metadata.num_cylinders;
    num_sectors_per_track_ = loaded_metadata.num_sectors_per_track;
    block_size_ = loaded_metadata.block_size;
    sector_size_ = loaded_metadata.sector_size;
    next_logical_page_id_ = loaded_metadata.next_logical_page_id;

    // Re-inicializar sector_status_map_ con las dimensiones correctas
    // y luego cargar su estado.
    uint32_t combined_platter_surface_count = num_platters_ * num_surfaces_per_platter_;
    sector_status_map_.resize(num_cylinders_);
    for (uint32_t t = 0; t < num_cylinders_; ++t) {
        sector_status_map_[t].resize(combined_platter_surface_count);
        for (uint32_t ps_idx = 0; ps_idx < combined_platter_surface_count; ++ps_idx) {
            sector_status_map_[t][ps_idx].resize(num_sectors_per_track_);
        }
    }

    // Cargar el sector_status_map_ serializado (bitmap de 2 bits por bloque).
    // Solo el primer sector de cada bloque almacena el BlockStatus.
    uint32_t total_logical_blocks = GetTotalLogicalBlocks();
    // Necesitamos 2 bits por bloque lógico para 3 estados (0, 1, 2).
    // (total_logical_blocks * 2 + 7) / 8 bytes para el bitmap.
    uint32_t map_bytes_needed_for_block_status = (total_logical_blocks * 2 + 7) / 8;

    // El bitmap del estado de bloques comienza después de DiskMetadata en el bloque 0.
    const Byte* bitmap_ptr = metadata_block.GetData() + sizeof(DiskMetadata);

    uint32_t current_block_idx = 0; // Índice del bloque lógico
    for (uint32_t t = 0; t < num_cylinders_; ++t) {
        for (uint32_t ps_idx = 0; ps_idx < combined_platter_surface_count; ++ps_idx) {
            for (uint32_t sec = 0; sec < num_sectors_per_track_; sec += GetSectorsPerBlock()) { // Iterar por el primer sector de cada posible bloque
                if (current_block_idx < total_logical_blocks) {
                    uint32_t bit_offset = current_block_idx * 2; // 2 bits por bloque
                    uint32_t byte_idx = bit_offset / 8;
                    uint32_t bit_in_byte_offset = bit_offset % 8;

                    if (byte_idx < map_bytes_needed_for_block_status) {
                        uint8_t status_value = (bitmap_ptr[byte_idx] >> bit_in_byte_offset) & 0x03; // Leer 2 bits
                        sector_status_map_[t][ps_idx][sec] = static_cast<BlockStatus>(status_value);
                    } else {
                        sector_status_map_[t][ps_idx][sec] = BlockStatus::EMPTY; // Asumir EMPTY si el mapa es más pequeño
                    }
                    current_block_idx++;
                }
            }
        }
    }

    // Deserializar logical_to_physical_map_
    // El mapeo se guarda después del bitmap de block_status.
    const Byte* map_data_ptr = bitmap_ptr + map_bytes_needed_for_block_status;
    
    // Leer el número de entradas en el mapa
    uint32_t num_map_entries;
    std::memcpy(&num_map_entries, map_data_ptr, sizeof(uint32_t));
    map_data_ptr += sizeof(uint32_t);

    logical_to_physical_map_.clear(); // Limpiar el mapa actual
    for (uint32_t i = 0; i < num_map_entries; ++i) {
        PageId page_id;
        PhysicalAddress phys_addr;
        std::memcpy(&page_id, map_data_ptr, sizeof(PageId));
        map_data_ptr += sizeof(PageId);
        std::memcpy(&phys_addr, map_data_ptr, sizeof(PhysicalAddress));
        map_data_ptr += sizeof(PhysicalAddress);
        logical_to_physical_map_[page_id] = phys_addr;
    }

    std::cout << "Metadatos del disco cargados exitosamente." << std::endl;
    std::cout << "Parámetros cargados: Platos=" << num_platters_
              << ", Superficies=" << num_surfaces_per_platter_
              << ", Cilindros=" << num_cylinders_
              << ", Sectores/Pista=" << num_sectors_per_track_
              << ", BlockSize=" << block_size_
              << ", SectorSize=" << sector_size_
              << ", NextLogicalPageId=" << next_logical_page_id_ << std::endl;
    return Status::OK;
}

// Guarda la configuración del disco y el mapa de sectores en la DISK_METADATA_PAGE.
Status DiskManager::SaveDiskMetadata() {
    std::cout << "Guardando metadatos del disco en DISK_METADATA_PAGE (PageId 0)..." << std::endl;

    // Crear un bloque temporal para escribir los metadatos.
    Block metadata_block(block_size_);
    Byte* block_data_ptr = metadata_block.GetMutableData();
    std::fill(block_data_ptr, block_data_ptr + block_size_, 0); // Limpiar el bloque

    // Serializar DiskMetadata
    DiskMetadata current_metadata;
    strncpy(current_metadata.disk_name, disk_name_.c_str(), sizeof(current_metadata.disk_name) - 1);
    current_metadata.disk_name[sizeof(current_metadata.disk_name) - 1] = '\0'; // Asegurar terminación nula
    current_metadata.num_platters = num_platters_;
    current_metadata.num_surfaces_per_platter = num_surfaces_per_platter_;
    current_metadata.num_cylinders = num_cylinders_;
    current_metadata.num_sectors_per_track = num_sectors_per_track_;
    current_metadata.block_size = block_size_;
    current_metadata.sector_size = sector_size_;
    current_metadata.next_logical_page_id = next_logical_page_id_;

    std::memcpy(block_data_ptr, &current_metadata, sizeof(DiskMetadata));
    Byte* current_write_ptr = block_data_ptr + sizeof(DiskMetadata);

    // Serializar el sector_status_map_ como un bitmap de 2 bits por bloque.
    uint32_t total_logical_blocks = GetTotalLogicalBlocks();
    uint32_t map_bytes_needed_for_block_status = (total_logical_blocks * 2 + 7) / 8; // Bytes para el bitmap

    // Asegurarse de que hay suficiente espacio en el bloque para el bitmap.
    if (current_write_ptr + map_bytes_needed_for_block_status > block_data_ptr + block_size_) {
        std::cerr << "Error (SaveDiskMetadata): El mapa de estado de bloques es demasiado grande para la DISK_METADATA_PAGE." << std::endl;
        return Status::OUT_OF_MEMORY;
    }

    std::fill(current_write_ptr, current_write_ptr + map_bytes_needed_for_block_status, 0); // Limpiar el área del bitmap

    uint32_t current_block_idx = 0;
    uint32_t combined_platter_surface_count = num_platters_ * num_surfaces_per_platter_;
    for (uint32_t t = 0; t < num_cylinders_; ++t) {
        for (uint32_t ps_idx = 0; ps_idx < combined_platter_surface_count; ++ps_idx) {
            for (uint32_t sec = 0; sec < num_sectors_per_track_; sec += GetSectorsPerBlock()) { // Iterar por el primer sector de cada posible bloque
                if (current_block_idx < total_logical_blocks) {
                    uint32_t bit_offset = current_block_idx * 2; // 2 bits por bloque
                    uint32_t byte_idx = bit_offset / 8;
                    uint32_t bit_in_byte_offset = bit_offset % 8;

                    uint8_t status_value = static_cast<uint8_t>(sector_status_map_[t][ps_idx][sec]);
                    current_write_ptr[byte_idx] |= (status_value << bit_in_byte_offset); // Escribir 2 bits

                    current_block_idx++;
                }
            }
        }
    }
    current_write_ptr += map_bytes_needed_for_block_status;

    // Serializar logical_to_physical_map_
    uint32_t num_map_entries = logical_to_physical_map_.size();
    size_t map_data_size = sizeof(uint32_t) + (num_map_entries * (sizeof(PageId) + sizeof(PhysicalAddress)));

    if (current_write_ptr + map_data_size > block_data_ptr + block_size_) {
        std::cerr << "Error (SaveDiskMetadata): El mapa lógico a físico es demasiado grande para la DISK_METADATA_PAGE." << std::endl;
        return Status::OUT_OF_MEMORY;
    }

    std::memcpy(current_write_ptr, &num_map_entries, sizeof(uint32_t));
    current_write_ptr += sizeof(uint32_t);

    for (const auto& pair : logical_to_physical_map_) {
        std::memcpy(current_write_ptr, &pair.first, sizeof(PageId));
        current_write_ptr += sizeof(PageId);
        std::memcpy(current_write_ptr, &pair.second, sizeof(PhysicalAddress));
        current_write_ptr += sizeof(PhysicalAddress);
    }

    // Escribir el bloque de metadatos en la DISK_METADATA_PAGE (PageId 0)
    PhysicalAddress metadata_addr = GetDiskMetadataPageAddress();
    Status write_status = WriteBlock(metadata_addr, metadata_block);
    if (write_status != Status::OK) {
        std::cerr << "Error (SaveDiskMetadata): Fallo al escribir la DISK_METADATA_PAGE." << std::endl;
    } else {
        std::cout << "Metadatos del disco guardados exitosamente." << std::endl;
    }
    return write_status;
}


// ReadBlock y WriteBlock (sin cambios en su lógica interna, solo se usan)
Status DiskManager::ReadBlock(const PhysicalAddress& address, Block& block) {
    if (!IsValidAddress(address)) {
        std::cerr << "Error: Dirección física inválida para lectura." << std::endl;
        return Status::INVALID_BLOCK_ID;
    }

    // Asegurarse de que el bloque tenga el tamaño correcto antes de leer.
    if (block.GetSize() != block_size_) {
        block.Resize(block_size_);
    }

    uint32_t sectors_per_block = GetSectorsPerBlock();
    if (address.sector_id + sectors_per_block > num_sectors_per_track_) {
        std::cerr << "Error: El bloque excede los límites de la pista en la lectura." << std::endl;
        return Status::INVALID_BLOCK_ID; // O un estado más específico como BLOCK_OUT_OF_BOUNDS
    }

    Byte* block_data_ptr = block.GetMutableData();
    for (uint32_t i = 0; i < sectors_per_block; ++i) {
        PhysicalAddress current_sector_address = address;
        current_sector_address.sector_id += i; // Avanzar al siguiente sector físico

        fs::path sector_path = GetSectorPath(current_sector_address);
        std::ifstream file(sector_path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error: No se pudo abrir el archivo de sector para lectura: " << sector_path << std::endl;
            return Status::IO_ERROR;
        }

        file.read(block_data_ptr + (i * sector_size_), sector_size_);
        if (!file) { // Comprueba si la lectura fue exitosa y leyó sector_size_ bytes
            std::cerr << "Error: Lectura incompleta o fallida del sector " << current_sector_address.sector_id << " desde: " << sector_path << std::endl;
            file.close();
            return Status::IO_ERROR;
        }
        file.close();
    }
    return Status::OK;
}

Status DiskManager::WriteBlock(const PhysicalAddress& address, const Block& block) {
    if (!IsValidAddress(address)) {
        std::cerr << "Error: Dirección física inválida para escritura." << std::endl;
        return Status::INVALID_BLOCK_ID;
    }

    if (block.GetSize() != block_size_) {
        std::cerr << "Error: El tamaño del bloque a escribir no coincide con el block_size_ configurado." << std::endl;
        return Status::INVALID_PARAMETER;
    }

    uint32_t sectors_per_block = GetSectorsPerBlock();
    if (address.sector_id + sectors_per_block > num_sectors_per_track_) {
        std::cerr << "Error: El bloque excede los límites de la pista en la escritura." << std::endl;
        return Status::INVALID_BLOCK_ID; // O un estado más específico
    }

    const Byte* block_data_ptr = block.GetData();
    for (uint32_t i = 0; i < sectors_per_block; ++i) {
        PhysicalAddress current_sector_address = address;
        current_sector_address.sector_id += i; // Avanzar al siguiente sector físico

        fs::path sector_path = GetSectorPath(current_sector_address);
        std::ofstream file(sector_path, std::ios::binary | std::ios::trunc); // trunc para sobrescribir
        if (!file.is_open()) {
            std::cerr << "Error: No se pudo abrir el archivo de sector para escritura: " << sector_path << std::endl;
            return Status::IO_ERROR;
        }

        file.write(block_data_ptr + (i * sector_size_), sector_size_);
        if (!file) { // Comprueba si la escritura fue exitosa
            std::cerr << "Error: Escritura incompleta o fallida del sector " << current_sector_address.sector_id << " en: " << sector_path << std::endl;
            file.close();
            return Status::IO_ERROR;
        }
        file.close();
    }
    return Status::OK;
}

// Método auxiliar para encontrar un bloque contiguo libre en un rango específico de sectores.
// Retorna true y actualiza allocated_address si se encuentra, false en caso contrario.
// Prioriza bloques INCOMPLETE para DATA_PAGEs.
bool DiskManager::FindContiguousBlock(uint32_t start_sector, uint32_t end_sector,
                                     PhysicalAddress& allocated_address, uint32_t sectors_needed,
                                     bool prioritize_incomplete) {
    uint32_t combined_platter_surface_count = num_platters_ * num_surfaces_per_platter_;

    // Priorizar bloques INCOMPLETE si se solicita y el block_size es mayor que sector_size
    if (prioritize_incomplete && GetSectorsPerBlock() > 1) {
        for (uint32_t t = 0; t < num_cylinders_; ++t) { // Iterar por cilindros
            for (uint32_t ps_idx = 0; ps_idx < combined_platter_surface_count; ++ps_idx) { // Iterar por filas de la matriz del cilindro
                for (uint32_t sec = start_sector; sec <= end_sector - sectors_needed; sec += GetSectorsPerBlock()) { // Iterar por el primer sector de cada posible bloque
                    if (sector_status_map_[t][ps_idx][sec] == BlockStatus::INCOMPLETE) {
                        // Found an INCOMPLETE block, assign it.
                        allocated_address = PhysicalAddress(ps_idx / num_surfaces_per_platter_, ps_idx % num_surfaces_per_platter_, t, sec);
                        // No es necesario marcarlo como ocupado aquí; AllocateBlock lo hará.
                        std::cout << "  Encontrado bloque INCOMPLETO para reutilizar en P" << allocated_address.platter_id
                                  << " S" << allocated_address.surface_id << " T" << allocated_address.track_id
                                  << " Sec" << allocated_address.sector_id << std::endl;
                        return true;
                    }
                }
            }
        }
    }

    // Buscar bloques EMPTY si no se encontró INCOMPLETE o no se prioriza.
    for (uint32_t t = 0; t < num_cylinders_; ++t) { // Iterar por cilindros
        for (uint32_t ps_idx = 0; ps_idx < combined_platter_surface_count; ++ps_idx) { // Iterar por filas de la matriz del cilindro
            for (uint32_t sec = start_sector; sec <= end_sector - sectors_needed; sec += GetSectorsPerBlock()) { // Iterar por el primer sector de cada posible bloque
                if (sector_status_map_[t][ps_idx][sec] == BlockStatus::EMPTY) {
                    // Found an EMPTY block, assign it.
                    allocated_address = PhysicalAddress(ps_idx / num_surfaces_per_platter_, ps_idx % num_surfaces_per_platter_, t, sec);
                    // No es necesario marcarlo como ocupado aquí; AllocateBlock lo hará.
                    std::cout << "  Encontrado bloque EMPTY para asignar en P" << allocated_address.platter_id
                              << " S" << allocated_address.surface_id << " T" << allocated_address.track_id
                              << " Sec" << allocated_address.sector_id << std::endl;
                    return true;
                }
            }
        }
    }
    return false; // No se encontró espacio contiguo en el rango
}


// Asigna un nuevo bloque lógico a un conjunto de sectores físicos contiguos disponibles.
Status DiskManager::AllocateBlock(PageId& new_page_id, PhysicalAddress& allocated_address, std::optional<PageType> page_type_hint) {
    uint32_t sectors_needed = GetSectorsPerBlock();

    // Definir rangos de sectores preferidos. Estos son ejemplos y pueden ser ajustados.
    // Asegurarse de que los rangos sean válidos para el tamaño del bloque
    uint32_t catalog_sector_end = std::min((uint32_t)num_sectors_per_track_ / 10, num_sectors_per_track_);
    uint32_t index_sector_start = catalog_sector_end;
    uint32_t index_sector_end = std::min(catalog_sector_end + (num_sectors_per_track_ / 5), num_sectors_per_track_);
    uint32_t data_sector_start = index_sector_end;
    uint32_t data_sector_end = num_sectors_per_track_; // Resto para datos

    // Asegurarse de que los rangos mínimos sean al menos sectors_needed
    if (catalog_sector_end < sectors_needed) catalog_sector_end = sectors_needed;
    if (index_sector_end < sectors_needed) index_sector_end = sectors_needed;
    if (data_sector_end < sectors_needed) data_sector_end = sectors_needed;


    // Prioridad para DATA_PAGEs: buscar INCOMPLETE primero, luego EMPTY
    bool allocated = false;
    if (page_type_hint.has_value() && page_type_hint.value() == PageType::DATA_PAGE) {
        std::cout << "Intentando asignar bloque de tipo DATA_PAGE (priorizando INCOMPLETE)..." << std::endl;
        allocated = FindContiguousBlock(data_sector_start, data_sector_end, allocated_address, sectors_needed, true);
    }

    // Si no se asignó o no era DATA_PAGE, buscar EMPTY en el rango preferido
    if (!allocated && page_type_hint.has_value()) {
        std::cout << "Intentando asignar bloque de tipo " << PageTypeToString(page_type_hint.value()) << "..." << std::endl;
        switch (page_type_hint.value()) {
            case PageType::DISK_METADATA_PAGE:
                std::cerr << "Error (AllocateBlock): DISK_METADATA_PAGE debe ser asignada en CreateDiskStructure." << std::endl;
                return Status::ERROR;
            case PageType::CATALOG_PAGE:
                allocated = FindContiguousBlock(0, catalog_sector_end, allocated_address, sectors_needed, false);
                break;
            case PageType::INDEX_PAGE:
                allocated = FindContiguousBlock(index_sector_start, index_sector_end, allocated_address, sectors_needed, false);
                break;
            case PageType::DATA_PAGE:
                // Ya se intentó priorizar INCOMPLETE, ahora buscar EMPTY.
                allocated = FindContiguousBlock(data_sector_start, data_sector_end, allocated_address, sectors_needed, false);
                break;
            case PageType::INVALID_PAGE:
                // No hay rango preferido para INVALID_PAGE, se tratará como general.
                break;
        }
    }

    // Si aún no se asignó, buscar EMPTY en cualquier lugar
    if (!allocated) {
        std::cout << "No se encontró espacio en el rango preferido. Buscando en cualquier lugar (EMPTY)..." << std::endl;
        allocated = FindContiguousBlock(0, num_sectors_per_track_, allocated_address, sectors_needed, false);
    }

    if (allocated) {
        // Marcar el primer sector del bloque como INCOMPLETE inicialmente.
        // El RecordManager lo actualizará a FULL cuando sea necesario.
        uint32_t ps_idx = allocated_address.platter_id * num_surfaces_per_platter_ + allocated_address.surface_id;
        sector_status_map_[allocated_address.track_id][ps_idx][allocated_address.sector_id] = BlockStatus::INCOMPLETE;

        // Asignar PageId y registrar mapeo
        new_page_id = next_logical_page_id_++;
        logical_to_physical_map_[new_page_id] = allocated_address;
        SaveDiskMetadata(); // Guardar el cambio del mapa y el estado del bloque
        
        std::cout << "Bloque asignado con PageId " << new_page_id << " en P" << allocated_address.platter_id
                  << " S" << allocated_address.surface_id << " T" << allocated_address.track_id
                  << " Sec" << allocated_address.sector_id << ". Estado inicial: INCOMPLETE." << std::endl;
        return Status::OK;
    }

    std::cerr << "Error: No hay suficientes sectores libres contiguos disponibles en el disco para un bloque de " << block_size_ << " bytes." << std::endl;
    return Status::DISK_FULL;
}

// Libera un conjunto de sectores físicos que forman un bloque lógico, marcándolos como disponibles.
Status DiskManager::DeallocateBlock(PageId page_id) {
    // No permitir eliminar la DISK_METADATA_PAGE (PageId 0)
    if (page_id == 0) {
        std::cerr << "Error (DeallocateBlock): No se puede desasignar la DISK_METADATA_PAGE (PageId 0)." << std::endl;
        return Status::INVALID_PARAMETER;
    }

    auto it = logical_to_physical_map_.find(page_id);
    if (it == logical_to_physical_map_.end()) {
        std::cerr << "Error (DeallocateBlock): PageId " << page_id << " no existe en el mapa lógico a físico." << std::endl;
        return Status::NOT_FOUND;
    }
    PhysicalAddress address = it->second;

    if (!IsValidAddress(address)) {
        std::cerr << "Error: Dirección física inválida para desasignación." << std::endl;
        return Status::INVALID_BLOCK_ID;
    }

    uint32_t sectors_to_deallocate = GetSectorsPerBlock();
    if (address.sector_id + sectors_to_deallocate > num_sectors_per_track_) {
        std::cerr << "Error: El bloque a desasignar excede los límites de la pista." << std::endl;
        return Status::INVALID_BLOCK_ID;
    }

    // Calcular el índice combinado para acceder al mapa
    uint32_t ps_idx = address.platter_id * num_surfaces_per_platter_ + address.surface_id;

    // Marcar el primer sector del bloque como EMPTY.
    // Esto es suficiente ya que solo el primer sector representa el estado del bloque.
    sector_status_map_[address.track_id][ps_idx][address.sector_id] = BlockStatus::EMPTY;

    // Eliminar el mapeo lógico a físico.
    logical_to_physical_map_.erase(page_id);
    SaveDiskMetadata(); // Guardar el cambio del mapa y el estado del bloque

    std::cout << "Bloque desasignado en: Plato " << address.platter_id << ", Superficie " << address.surface_id
              << ", Pista (Cilindro) " << address.track_id << ", Sector inicial " << address.sector_id
              << " (liberando " << sectors_to_deallocate << " sectores)." << std::endl;
    return Status::OK;
}

// Actualiza el BlockStatus de un bloque lógico en el sector_status_map_.
Status DiskManager::UpdateBlockStatus(PageId page_id, BlockStatus new_status) {
    if (page_id == 0) { // No se permite cambiar el estado de la página de metadatos del disco
        std::cerr << "Error (UpdateBlockStatus): No se puede cambiar el estado de la DISK_METADATA_PAGE (PageId 0)." << std::endl;
        return Status::INVALID_PARAMETER;
    }

    auto it = logical_to_physical_map_.find(page_id);
    if (it == logical_to_physical_map_.end()) {
        std::cerr << "Error (UpdateBlockStatus): PageId " << page_id << " no tiene una dirección física mapeada." << std::endl;
        return Status::NOT_FOUND;
    }
    PhysicalAddress address = it->second;

    if (!IsValidAddress(address)) {
        std::cerr << "Error (UpdateBlockStatus): Dirección física inválida para PageId " << page_id << "." << std::endl;
        return Status::INVALID_BLOCK_ID;
    }

    uint32_t ps_idx = address.platter_id * num_surfaces_per_platter_ + address.surface_id;
    
    // Solo actualizamos el estado del primer sector del bloque.
    sector_status_map_[address.track_id][ps_idx][address.sector_id] = new_status;
    SaveDiskMetadata(); // Persistir el cambio de estado del bloque

    std::cout << "Estado del bloque PageId " << page_id << " actualizado a " << BlockStatusToString(new_status) << "." << std::endl;
    return Status::OK;
}


// Obtiene la dirección física de un PageId lógico.
PhysicalAddress DiskManager::GetPhysicalAddress(PageId page_id) const {
    auto it = logical_to_physical_map_.find(page_id);
    if (it != logical_to_physical_map_.end()) {
        return it->second;
    }
    // Si no se encuentra, retornar una dirección inválida (todos ceros) y el llamador debe manejar el error.
    std::cerr << "Error (GetPhysicalAddress): PageId " << page_id << " no tiene una dirección física mapeada." << std::endl;
    return PhysicalAddress();
}


// Obtiene la dirección física de la página de metadatos del disco (PageId 0).
PhysicalAddress DiskManager::GetDiskMetadataPageAddress() const {
    // La página de metadatos del disco siempre estará en la primera posición física.
    return PhysicalAddress(0, 0, 0, 0);
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

// Obtiene el tamaño de un bloque lógico en bytes.
BlockSizeType DiskManager::GetBlockSize() const {
    return block_size_;
}

// Obtiene el tamaño de un sector físico en bytes.
SectorSizeType DiskManager::GetSectorSize() const {
    return sector_size_;
}
