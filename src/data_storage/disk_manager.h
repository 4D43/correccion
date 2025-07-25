// data_storage/disk_manager.h
#ifndef DISK_MANAGER_H
#define DISK_MANAGER_H

#include "../include/common.h" // Incluye BlockId, Status, Byte, BlockSizeType, SectorSizeType, PageType, BlockStatus
#include "block.h"              // Incluye la definición de Block
#include <string>               // Para std::string
#include <vector>               // Para std::vector
#include <filesystem>           // Para operaciones de sistema de archivos (C++17)
#include <fstream>              // Para std::ofstream
#include <numeric>              // Para std::iota
#include <optional>             // Para std::optional (C++17)
#include <unordered_map>        // Para std::unordered_map

// Estructura para representar una dirección física en el disco.
// Facilita la referencia a un sector específico.
struct PhysicalAddress {
    uint32_t platter_id;  // ID del plato
    uint32_t surface_id;  // ID de la superficie en el plato
    uint32_t track_id;    // ID de la pista en la superficie (que es el ID del cilindro)
    uint32_t sector_id;   // ID del sector en la pista

    // Constructor por defecto
    PhysicalAddress() : platter_id(0), surface_id(0), track_id(0), sector_id(0) {}

    // Constructor con parámetros
    PhysicalAddress(uint32_t p, uint32_t s, uint32_t t, uint32_t sec)
        : platter_id(p), surface_id(s), track_id(t), sector_id(sec) {}

    // Operador de igualdad para comparar dos direcciones físicas.
    bool operator==(const PhysicalAddress& other) const {
        return platter_id == other.platter_id &&
               surface_id == other.surface_id &&
               track_id == other.track_id &&
               sector_id == other.sector_id;
    }
};

// Especialización de std::hash para PhysicalAddress, necesaria para usarla como clave en std::unordered_map
namespace std {
    template <>
    struct hash<PhysicalAddress> {
        size_t operator()(const PhysicalAddress& pa) const {
            // Una función hash simple que combina los hashes de sus miembros.
            // Se puede usar std::hash para cada miembro y combinarlos.
            size_t h1 = std::hash<uint32_t>()(pa.platter_id);
            size_t h2 = std::hash<uint32_t>()(pa.surface_id);
            size_t h3 = std::hash<uint32_t>()(pa.track_id);
            size_t h4 = std::hash<uint32_t>()(pa.sector_id);
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
        }
    };
}


// Clase DiskManager: Simula la gestión de un disco duro físico.
// Responsabilidades:
// - Asignar y desasignar bloques lógicos a direcciones físicas.
// - Leer y escribir bloques de datos.
// - Persistir el estado del disco (mapa de bits y mapeo lógico a físico).
class DiskManager {
public:
    // Constructor del DiskManager.
    // disk_name: Nombre del directorio donde se almacenarán los archivos del disco.
    // num_platters: Número de platos en el disco.
    // num_surfaces_per_platter: Número de superficies por plato (normalmente 2).
    // num_cylinders: Número de cilindros (pistas) por superficie.
    // num_sectors_per_track: Número de sectores por pista.
    // block_size: Tamaño lógico de un bloque de datos en bytes.
    // sector_size: Tamaño físico de un sector en bytes.
    // is_new_disk: true si se está creando un disco nuevo, false si se está cargando uno existente.
    DiskManager(const std::string& disk_name,
                uint32_t num_platters,
                uint32_t num_surfaces_per_platter,
                uint32_t num_cylinders,
                uint32_t num_sectors_per_track,
                BlockSizeType block_size,
                SectorSizeType sector_size,
                bool is_new_disk);

    // Destructor: Guarda el estado del disco antes de salir.
    ~DiskManager();

    // Crea la estructura de directorios y archivos para un nuevo disco.
    Status CreateDiskStructure();

    // Carga los metadatos del disco desde el archivo de metadatos.
    Status LoadDiskMetadata();

    // Asigna un nuevo bloque lógico en el disco.
    // page_type: El tipo de página que se asignará (para optimizar la ubicación).
    // block_id: El ID del bloque lógico asignado (salida).
    // Retorna Status::OK si la asignación es exitosa, o un error en caso contrario.
    Status AllocateBlock(PageType page_type, BlockId& block_id);

    // Desasigna un bloque lógico del disco, marcando su espacio como libre.
    // block_id: El ID del bloque lógico a desasignar.
    Status DeallocateBlock(BlockId block_id);

    // Lee un bloque de datos del disco físico.
    // block_id: El ID del bloque lógico a leer.
    // block: Referencia a un objeto Block donde se cargarán los datos (salida).
    Status ReadBlock(BlockId block_id, Block& block);

    // Escribe un bloque de datos al disco físico.
    // block_id: El ID del bloque lógico a escribir.
    // block: El objeto Block que contiene los datos a escribir.
    Status WriteBlock(BlockId block_id, const Block& block);

    // Actualiza el estado de un bloque lógico en el mapa de bits del disco.
    // Esto no guarda el mapa en disco inmediatamente, solo lo actualiza en memoria.
    void UpdateBlockStatus(BlockId block_id, BlockStatus status);

    // Métodos getter para los parámetros del disco.
    std::string GetDiskName() const { return disk_name_; }
    uint32_t GetNumPlatters() const { return num_platters_; }
    uint32_t GetNumSurfacesPerPlatter() const { return num_surfaces_per_platter_; }
    uint32_t GetNumCylinders() const { return num_cylinders_; }
    uint32_t GetNumSectorsPerTrack() const { return num_sectors_per_track_; }
    BlockSizeType GetBlockSize() const { return block_size_; }
    SectorSizeType GetSectorSize() const { return sector_size_; }
    uint32_t GetSectorsPerBlock() const { return block_size_ / sector_size_; }
    uint32_t GetTotalPhysicalSectors() const;
    uint32_t GetFreePhysicalSectors() const;
    uint32_t GetTotalLogicalBlocks() const;
    
    // NUEVAS FUNCIONES PARA INFORMACIÓN DETALLADA DEL DISCO
    uint64_t GetTotalCapacityBytes() const; // Capacidad total del disco en bytes
    uint32_t GetOccupiedLogicalBlocks() const; // Número de bloques lógicos ocupados
    double GetDiskUsagePercentage() const; // Porcentaje de uso del disco
    
    // Método para imprimir una representación visual del mapa de estado de bloques
    void PrintBlockStatusMap() const;
    // Método para imprimir una representación visual del mapeo lógico a físico
    void PrintLogicalToPhysicalMap() const;


private:
    std::string disk_name_;
    uint32_t num_platters_;
    uint32_t num_surfaces_per_platter_;
    uint32_t num_cylinders_;
    uint32_t num_sectors_per_track_;
    BlockSizeType block_size_;
    SectorSizeType sector_size_;
    
    // El próximo PageId lógico disponible para asignación.
    // ¡AHORA RESIDE EN DISKMANAGER Y SERÁ PERSISTENTE!
    BlockId next_logical_page_id_; 

    // Mapa de bits para el estado de los sectores físicos.
    // sector_status_map_[Cilindro][Combinado Plato/Superficie][Sector]
    // Almacena BlockStatus para el PRIMER sector de cada bloque lógico.
    std::vector<std::vector<std::vector<BlockStatus>>> sector_status_map_;

    // Mapa para almacenar la dirección física de cada PageId lógico.
    // ¡AHORA RESIDE EN DISKMANAGER Y SERÁ PERSISTENTE!
    std::unordered_map<PageId, PhysicalAddress> logical_to_physical_map_;


    // Método auxiliar para obtener la ruta de un sector físico.
    std::filesystem::path GetSectorPath(const PhysicalAddress& address) const;
    // Método auxiliar para obtener la ruta de un archivo de bloque lógico (ahora solo representativo).
    std::filesystem::path GetBlockFilePath(BlockId block_id) const;

    // Inicializa el mapa de estado de sectores y los archivos de bloques lógicos.
    Status InitializeDiskMapAndBlockFiles();

    // Método auxiliar para verificar si una dirección física es válida.
    bool IsValidAddress(const PhysicalAddress& address) const;

    // Método auxiliar para encontrar un bloque contiguo libre en un rango específico de sectores.
    // Retorna true y actualiza allocated_address si se encuentra, false en caso contrario.
    // Prioriza bloques INCOMPLETE.
    bool FindContiguousBlock(uint32_t start_sector, uint32_t end_sector,
                             PhysicalAddress& allocated_address, uint32_t sectors_needed,
                             bool prioritize_incomplete = false); // Nuevo parámetro
    
    // Métodos para persistir y cargar los metadatos internos del DiskManager.
    Status SaveDiskMetadata();

};

#endif // DISK_MANAGER_H
