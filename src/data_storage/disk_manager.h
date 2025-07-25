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

    // Sobrecarga del operador de igualdad para comparar direcciones físicas.
    bool operator==(const PhysicalAddress& other) const {
        return platter_id == other.platter_id &&
               surface_id == other.surface_id &&
               track_id == other.track_id &&
               sector_id == other.sector_id;
    }
};

// --- Estructura para los Metadatos del Disco (Super-Bloque) ---
// Esta estructura se guardará en la DISK_METADATA_PAGE (PageId 0)
// para persistir la configuración y el estado del disco.
struct DiskMetadata {
    char disk_name[256];             // Nombre del disco
    uint32_t num_platters;             // Número de platos
    uint32_t num_surfaces_per_platter; // Superficies por plato
    uint32_t num_cylinders;            // Número de cilindros (igual a num_tracks_per_surface)
    uint32_t num_sectors_per_track;    // Sectores por pista
    BlockSizeType block_size;          // Tamaño de un bloque lógico
    SectorSizeType sector_size;        // Tamaño de un sector físico
    uint32_t next_logical_page_id;     // Siguiente PageId lógico disponible
    // El sector_status_map_ y logical_to_physical_map_ se serializarán/deserializarán aparte
    // debido a su naturaleza dinámica y complejidad de estructura.
};


// Clase DiskManager: Gestiona la estructura física del disco y la asignación de bloques.
class DiskManager {
public:
    // Constructor del DiskManager.
    // disk_name: Nombre del disco que se va a gestionar.
    // num_platters: Número de platos en el disco.
    // num_surfaces_per_platter: Número de superficies por plato (ej. 2 para arriba y abajo).
    // num_cylinders: Número de cilindros (o pistas por superficie).
    // num_sectors_per_track: Número de sectores por pista.
    // block_size: Tamaño de un bloque lógico en bytes.
    // sector_size: Tamaño de un sector físico en bytes.
    // is_new_disk: Indica si se está creando un disco nuevo o cargando uno existente.
    DiskManager(const std::string& disk_name,
                uint32_t num_platters,
                uint32_t num_surfaces_per_platter,
                uint32_t num_cylinders,
                uint32_t num_sectors_per_track,
                BlockSizeType block_size,
                SectorSizeType sector_size,
                bool is_new_disk = true); // Nuevo parámetro

    // Crea la estructura de directorios y archivos que representan el disco físico.
    // Esto incluye las carpetas 'body' y 'blocks', y los archivos de sectores.
    // También inicializa la DISK_METADATA_PAGE y guarda los metadatos.
    Status CreateDiskStructure();

    // Carga la configuración del disco y el mapa de sectores desde la DISK_METADATA_PAGE.
    Status LoadDiskMetadata();

    // Guarda la configuración del disco y el mapa de sectores en la DISK_METADATA_PAGE.
    Status SaveDiskMetadata();

    // Lee un bloque de datos del disco físico en una dirección específica.
    // La dirección apunta al primer sector del bloque lógico.
    // address: La dirección física del primer sector del bloque a leer.
    // block: Referencia al objeto Block donde se cargarán los datos.
    Status ReadBlock(const PhysicalAddress& address, Block& block);

    // Escribe un bloque de datos en el disco físico en una dirección específica.
    // La dirección apunta al primer sector del bloque lógico.
    // address: La dirección física donde se escribirá el bloque.
    // block: El objeto Block que contiene los datos a escribir.
    Status WriteBlock(const PhysicalAddress& address, const Block& block);

    // Asigna un nuevo bloque lógico a un conjunto de sectores físicos contiguos disponibles.
    // Retorna el PageId lógico asignado y su PhysicalAddress.
    // page_type_hint: Sugerencia del tipo de página para optimizar la ubicación en disco.
    // Retorna Status::DISK_FULL si no hay suficientes sectores libres contiguos.
    Status AllocateBlock(PageId& new_page_id, PhysicalAddress& allocated_address, std::optional<PageType> page_type_hint = std::nullopt);

    // Libera un conjunto de sectores físicos que forman un bloque lógico, marcándolos como disponibles.
    // page_id: El ID lógico del bloque a liberar.
    Status DeallocateBlock(PageId page_id);

    // Actualiza el BlockStatus de un bloque lógico en el sector_status_map_.
    // Esto es usado por el RecordManager para indicar si una página de datos está INCOMPLETE o FULL.
    Status UpdateBlockStatus(PageId page_id, BlockStatus new_status);

    // Obtiene la dirección física de un PageId lógico.
    // Retorna un objeto PhysicalAddress válido si el PageId existe, o un PhysicalAddress por defecto
    // si no se encuentra (y un error de estado).
    PhysicalAddress GetPhysicalAddress(PageId page_id) const;

    // Obtiene la dirección física de la página de metadatos del disco (PageId 0).
    PhysicalAddress GetDiskMetadataPageAddress() const;

    // Obtiene el número total de sectores físicos en el disco.
    uint32_t GetTotalPhysicalSectors() const;

    // Obtiene el número de sectores físicos libres en el disco.
    uint32_t GetFreePhysicalSectors() const;

    // Obtiene el número total de bloques lógicos que el disco puede almacenar.
    uint32_t GetTotalLogicalBlocks() const;

    // Obtiene el tamaño de un bloque lógico en bytes.
    BlockSizeType GetBlockSize() const;

    // Obtiene el tamaño de un sector físico en bytes.
    SectorSizeType GetSectorSize() const;

    // Getters para los parámetros del disco
    std::string GetDiskName() const { return disk_name_; }
    uint32_t GetNumPlatters() const { return num_platters_; }
    uint32_t GetNumSurfacesPerPlatter() const { return num_surfaces_per_platter_; }
    uint32_t GetNumCylinders() const { return num_cylinders_; }
    uint32_t GetNumSectorsPerTrack() const { return num_sectors_per_track_; }

    // Obtiene el número de sectores físicos que ocupa un bloque lógico.
    uint32_t GetSectorsPerBlock() const {
        return block_size_ / sector_size_;
    }

private:
    std::string disk_name_;             // Nombre del disco
    uint32_t num_platters_;             // Número de platos
    uint32_t num_surfaces_per_platter_; // Superficies por plato
    uint32_t num_cylinders_;            // Número de cilindros (igual a num_tracks_per_surface)
    uint32_t num_sectors_per_track_;    // Sectores por pista
    BlockSizeType block_size_;          // Tamaño de un bloque lógico
    SectorSizeType sector_size_;        // Tamaño de un sector físico
    uint32_t next_logical_page_id_;     // Siguiente PageId lógico disponible para asignación

    // Ruta base donde se almacenarán los discos (ej. C:/Discos/)
    std::filesystem::path base_disk_path_;
    // Ruta específica para este disco (ej. C:/Discos/MiDisco/)
    std::filesystem::path current_disk_path_;
    // Ruta para la estructura física del disco (ej. C:/Discos/MiDisco/body/)
    std::filesystem::path body_path_;
    // Ruta para los archivos de bloques lógicos (ej. C:/Discos/MiDisco/blocks/)
    std::filesystem::path blocks_path_;

    // Vector de matrices para controlar el estado de los bloques (ocupado/libre/incompleto).
    // Dimensiones: [Cilindro ID (Pista)][Índice Combinado Plato/Superficie][Sector]
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
};

#endif // DISK_MANAGER_H
