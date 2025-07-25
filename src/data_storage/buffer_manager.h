// data_storage/buffer_manager.h
#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

#include "../include/common.h"           // Para BlockId, FrameId, Status, Byte, BlockSizeType, PageType
#include "disk_manager.h"                 // Para DiskManager (dependencia), PhysicalAddress
#include "page.h"                         // Para la estructura Page (metadatos del frame)
#include "../replacement_policies/ireplacement_policy.h" // Para la interfaz de política de reemplazo

#include <vector>                         // Para std::vector
#include <unordered_map>                  // Para mapear PageId a FrameId
#include <memory>                         // Para std::unique_ptr
#include <optional>                       // Para std::optional

// Clase BufferManager: Gestiona el Buffer Pool en memoria principal.
// Responsabilidades:
// - Cargar y descargar bloques de disco al buffer pool.
// - Anclar (pin) y desanclar (unpin) páginas.
// - Marcar páginas como sucias (dirty) si han sido modificadas.
// - Implementar una política de reemplazo para desalojar páginas cuando el buffer está lleno.
class BufferManager {
public:
    // Constructor del BufferManager.
    // disk_manager: Una referencia al DiskManager para interactuar con el disco.
    // pool_size: El número máximo de páginas (frames) que el buffer pool puede contener.
    // block_size: El tamaño en bytes de un bloque lógico.
    // replacement_policy: Un puntero único a la política de reemplazo a utilizar.
    //                     Esto permite la inyección de dependencias y adhiere al OCP.
    BufferManager(DiskManager& disk_manager,
                  uint32_t pool_size,
                  BlockSizeType block_size,
                  std::unique_ptr<IReplacementPolicy> replacement_policy);

    // Destructor del BufferManager.
    // Asegura que todas las páginas sucias sean escritas de vuelta al disco.
    ~BufferManager();

    // Solicita una página (bloque) del buffer pool.
    // Si la página no está en el buffer, se carga desde el disco.
    // La página se "ancla" (pinned) y su pin_count se incrementa.
    // page_id: El ID del bloque de disco que se desea.
    // Retorna un puntero a los datos del bloque si la operación es exitosa,
    // o nullptr si hay un error (ej. no se puede cargar, buffer lleno y no se puede desalojar).
    Byte* FetchPage(PageId page_id);

    // Desancla una página del buffer pool.
    // Decrementa el pin_count de la página.
    // page_id: El ID del bloque de disco a desanclar.
    // is_dirty: true si la página ha sido modificada y necesita ser escrita de vuelta al disco.
    // Retorna Status::OK si la operación es exitosa, Status::ERROR en caso contrario.
    Status UnpinPage(PageId page_id, bool is_dirty);

    // Fuerza la escritura de una página específica de vuelta al disco, si está sucia.
    // La página NO se desalojará del buffer.
    // page_id: El ID del bloque de disco a forzar la escritura.
    // Retorna Status::OK si la operación es exitosa, Status::ERROR en caso contrario.
    Status FlushPage(PageId page_id);

    // Fuerza la escritura de TODAS las páginas sucias de vuelta al disco.
    // Retorna Status::OK si la operación es exitosa, Status::ERROR en caso contrario.
    Status FlushAllPages();

    // Crea una nueva página (bloque) en el disco y la carga en el buffer pool.
    // page_type: El tipo de página que se va a crear (DATA_PAGE, CATALOG_PAGE, etc.).
    // Retorna un puntero a los datos del nuevo bloque si la operación es exitosa,
    // o nullptr si hay un error (ej. disco lleno, buffer lleno).
    // El page_id del nuevo bloque se asignará automáticamente por DiskManager.
    Byte* NewPage(PageId& new_page_id, PageType page_type);

    // Elimina una página (bloque) del disco y del buffer pool si está presente.
    // page_id: El ID del bloque a eliminar.
    // Retorna Status::OK si la operación es exitosa, Status::ERROR en caso contrario.
    Status DeletePage(PageId page_id);

    // Obtiene el número de páginas libres (frames vacíos) en el buffer pool.
    uint32_t GetFreeFramesCount() const;

    // Obtiene el número total de frames en el buffer pool.
    uint32_t GetPoolSize() const;

    // Obtiene el número de páginas actualmente cargadas en el buffer pool.
    uint32_t GetNumBufferedPages() const;

    // Obtiene un puntero a los datos de un bloque lógico específico en la simulación de disco en RAM.
    // Esto es para propósitos de visualización/depuración, no para uso normal del SGBD.
    // Retorna nullptr si el page_id es inválido.
    const Byte* GetSimulatedBlockData(PageId page_id) const;

    // Obtiene el tamaño de un bloque lógico.
    BlockSizeType GetBlockSize() const {
        return block_size_;
    }

    // Método delegado para actualizar el estado de un bloque en el disco.
    // Permite que otros managers (como RecordManager) soliciten una actualización
    // sin acceder directamente al DiskManager.
    Status UpdateBlockStatusOnDisk(PageId page_id, BlockStatus new_status);

private:
    DiskManager& disk_manager_; // Referencia al gestor de disco

    // El Buffer Pool en sí: vector de metadatos (Page).
    std::vector<Page> frames_; // Metadatos de cada frame en el buffer pool

    // El "vector de arreglos estáticos" que contiene los datos reales de *todos* los bloques lógicos.
    // Su tamaño es igual al número total de bloques lógicos del disco simulado.
    // Cada elemento es un std::vector<Byte> para manejar el tamaño dinámico del bloque.
    std::vector<std::vector<Byte>> buffer_data_simulated_disk_;

    uint32_t pool_size_; // Tamaño máximo del buffer pool (número de frames)
    BlockSizeType block_size_; // Tamaño de un bloque lógico

    // Mapa para buscar rápidamente el FrameId dado un PageId (bloque de disco)
    // para las páginas actualmente cargadas en el Buffer Pool.
    std::unordered_map<PageId, FrameId> page_table_;

    // Puntero a la política de reemplazo. std::unique_ptr asegura la gestión de memoria.
    std::unique_ptr<IReplacementPolicy> replacement_policy_;

    // Método auxiliar para encontrar un frame libre en el buffer pool.
    // Retorna el FrameId del frame libre, o -1 si no hay frames libres.
    FrameId FindFreeFrame();

    // Método auxiliar para desalojar una página del buffer pool.
    // Retorna Status::OK si se desalojó una página, o un error si no se pudo.
    Status EvictPage();

    // Método auxiliar para escribir una página (datos de buffer_data_simulated_disk_)
    // de vuelta al disco físico (usando DiskManager).
    // page_id: El ID lógico del bloque a escribir.
    // Retorna Status::OK si la escritura es exitosa, Status::IO_ERROR en caso contrario.
    Status WritePageToDisk(PageId page_id);

    // Método auxiliar para leer una página (datos del disco físico)
    // y cargarla en buffer_data_simulated_disk_.
    // page_id: El ID lógico del bloque a leer.
    // Retorna Status::OK si la lectura es exitosa, Status::IO_ERROR en caso contrario.
    Status ReadPageFromDisk(PageId page_id);
};

#endif // BUFFER_MANAGER_H
