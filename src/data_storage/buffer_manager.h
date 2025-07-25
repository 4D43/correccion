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
    // replacement_policy: Un puntero único a la política de reemplazo a usar.
    BufferManager(DiskManager& disk_manager,
                  uint32_t pool_size,
                  BlockSizeType block_size,
                  std::unique_ptr<IReplacementPolicy> replacement_policy);

    // Destructor: Asegura que todas las páginas sucias sean escritas a disco.
    ~BufferManager();

    // Obtiene una página del buffer pool. Si no está, la carga del disco.
    // page_id: El ID de la página a obtener.
    // Retorna un puntero a los datos de la página en memoria, o nullptr si hay un error.
    Byte* FetchPage(PageId page_id);

    // Crea una nueva página en el disco y la carga en el buffer pool.
    // new_page_id: El ID de la nueva página creada (salida).
    // page_type: El tipo de página a crear (DATA_PAGE, CATALOG_PAGE, etc.).
    // Retorna un puntero a los datos de la nueva página en memoria, o nullptr si hay un error.
    Byte* NewPage(PageId& new_page_id, PageType page_type);

    // Elimina una página del disco y del buffer pool (si está presente).
    // page_id: El ID de la página a eliminar.
    Status DeletePage(PageId page_id);

    // Desancla una página, decrementando su pin_count.
    // page_id: El ID de la página a desanclar.
    // is_dirty: true si la página ha sido modificada y necesita ser escrita a disco.
    Status UnpinPage(PageId page_id, bool is_dirty);

    // Escribe todas las páginas marcadas como sucias del buffer pool al disco.
    Status FlushAllPages();

    // Obtiene el número de páginas libres (frames vacíos) en el buffer pool.
    uint32_t GetFreeFramesCount() const;

    // Obtiene el número total de frames en el buffer pool.
    uint32_t GetPoolSize() const;

    // Obtiene el número de páginas actualmente cargadas en el buffer pool.
    uint32_t GetNumBufferedPages() const;

    // Obtiene un puntero a los datos de un bloque lógico específico en la simulación de disco en RAM.
    // Este método es principalmente para depuración o acceso interno simulado.
    const Byte* GetSimulatedBlockData(PageId page_id) const;

    // Obtiene el tamaño de un bloque lógico.
    BlockSizeType GetBlockSize() const { return block_size_; }

    // Obtiene un puntero a los datos de una página que ya está en el buffer pool.
    // NO incrementa el pin_count. Usar con precaución (principalmente para depuración).
    Byte* GetPageDataInPool(PageId page_id);

    // Actualiza el estado de un bloque en el DiskManager.
    Status UpdateBlockStatusOnDisk(PageId page_id, BlockStatus status);

    // NUEVO: Método para obtener la información de todos los frames del buffer pool (para depuración/visualización).
    const std::vector<Page>& GetFrames() const { return frames_; }


private:
    DiskManager& disk_manager_;
    uint32_t pool_size_;          // Tamaño máximo del buffer pool (número de frames)
    BlockSizeType block_size_; // Tamaño de un bloque lógico

    // El Buffer Pool en sí: un vector de vectores de bytes (simula la memoria contigua de los frames).
    // Cada elemento es un frame, que contiene los datos de un bloque.
    std::vector<std::vector<Byte>> buffer_data_pool_;

    // Metadatos de cada frame en el Buffer Pool.
    std::vector<Page> frames_;

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

    // Método auxiliar para escribir una página (datos de buffer_data_pool_)
    // de vuelta al disco físico (usando DiskManager).
    // page_id: El ID lógico del bloque a escribir.
    // Retorna Status::OK si la escritura es exitosa, Status::IO_ERROR en caso contrario.
    Status WritePageToDisk(PageId page_id);

    // Método auxiliar para leer una página (datos del disco físico)
    // y cargarla en buffer_data_pool_.
    // page_id: El ID lógico del bloque a leer.
    // frame_id: El ID del frame donde cargar la página.
    // Retorna Status::OK si la lectura es exitosa, Status::IO_ERROR en caso contrario.
    Status ReadPageFromDisk(PageId page_id, FrameId frame_id);
};

#endif // BUFFER_MANAGER_H
