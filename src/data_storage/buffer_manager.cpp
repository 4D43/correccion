// data_storage/buffer_manager.cpp
#include "buffer_manager.h"
#include "block.h" // Para usar la clase Block para E/S con DiskManager
#include <cstring> // Para std::memcpy
#include <iostream> // Para std::cout, std::cerr
#include <algorithm> // Para std::fill

// Constructor del BufferManager.
BufferManager::BufferManager(DiskManager& disk_manager,
                             uint32_t pool_size,
                             BlockSizeType block_size,
                             std::unique_ptr<IReplacementPolicy> replacement_policy)
    : disk_manager_(disk_manager),
      pool_size_(pool_size),
      block_size_(block_size),
      replacement_policy_(std::move(replacement_policy)) // Transferir la propiedad del unique_ptr
{
    // Validar que el tamaño del bloque coincida con el del DiskManager
    if (block_size_ != disk_manager_.GetBlockSize()) {
        throw std::invalid_argument("BufferManager block_size must match DiskManager's block_size.");
    }

    // Inicializar el vector de metadatos de frames del Buffer Pool.
    frames_.resize(pool_size_);
    for (FrameId i = 0; i < pool_size_; ++i) {
        frames_[i].Reset(); // Inicializar cada frame como no válido
        replacement_policy_->AddFrame(i); // Notificar a la política de reemplazo de los nuevos frames
    }

    // Inicializar el Buffer Pool de datos. Cada frame es un vector de Byte.
    buffer_data_pool_.resize(pool_size_);
    for (auto& frame_data : buffer_data_pool_) {
        frame_data.resize(block_size_, 0); // Redimensionar cada frame al tamaño del bloque e inicializar con ceros
    }

    std::cout << "BufferManager inicializado con pool_size=" << pool_size_
              << " y block_size=" << block_size_ << " bytes." << std::endl;
}

// Destructor: Asegura que todas las páginas sucias sean escritas a disco.
BufferManager::~BufferManager() {
    std::cout << "BufferManager: Flushando todas las páginas sucias antes de la destrucción." << std::endl;
    Status status = FlushAllPages();
    if (status != Status::OK) {
        std::cerr << "Error (Destructor BufferManager): Fallo al flushar todas las páginas sucias: " << StatusToString(status) << std::endl;
    }
}

// Obtiene una página del buffer pool. Si no está, la carga del disco.
Byte* BufferManager::FetchPage(PageId page_id) { // Corregido: Tipo de retorno a Byte*
    // 1. Verificar si la página ya está en el buffer pool.
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        FrameId frame_id = it->second;
        frames_[frame_id].pin_count++; // Incrementar pin_count
        replacement_policy_->Access(frame_id); // Notificar acceso a la política
        // std::cout << "FetchPage: Página " << page_id << " encontrada en frame " << frame_id << ". Pin count: " << frames_[frame_id].pin_count << std::endl;
        return &buffer_data_pool_[frame_id][0]; // Retornar puntero a los datos
    }

    // 2. La página no está en el buffer. Buscar un frame libre o desalojar uno.
    FrameId target_frame_id = FindFreeFrame();
    if (target_frame_id == (FrameId)-1) { // No hay frames libres, intentar desalojar
        Status evict_status = EvictPage();
        if (evict_status != Status::OK) {
            std::cerr << "Error (FetchPage): No se pudo desalojar una página para cargar la página " << page_id << "." << std::endl;
            return nullptr;
        }
        // Después de desalojar, FindFreeFrame debería encontrar un frame libre (el que fue desalojado)
        target_frame_id = FindFreeFrame();
        if (target_frame_id == (FrameId)-1) { // Esto no debería pasar si EvictPage fue exitoso
            std::cerr << "Error (FetchPage): Después de desalojar, no se encontró un frame libre." << std::endl;
            return nullptr;
        }
    }

    // 3. Cargar la página del disco al frame encontrado/desalojado.
    Status read_status = ReadPageFromDisk(page_id, target_frame_id);
    if (read_status != Status::OK) {
        std::cerr << "Error (FetchPage): Fallo al leer la página " << page_id << " del disco. " << StatusToString(read_status) << std::endl;
        // El frame queda en estado inválido o se resetea
        frames_[target_frame_id].Reset();
        replacement_policy_->RemoveFrame(target_frame_id); // Remover de la política si la carga falla
        return nullptr;
    }

    // 4. Actualizar metadatos del frame y page_table_.
    frames_[target_frame_id].page_id = page_id;
    frames_[target_frame_id].pin_count = 1; // Anclada por el fetch
    frames_[target_frame_id].is_dirty = false; // Recién cargada, no sucia
    frames_[target_frame_id].is_valid = true;
    page_table_[page_id] = target_frame_id; // Añadir al mapa

    replacement_policy_->Pin(target_frame_id); // Notificar a la política que la página está anclada
    replacement_policy_->Access(target_frame_id); // Notificar acceso

    // std::cout << "FetchPage: Página " << page_id << " cargada en frame " << target_frame_id << ". Pin count: " << frames_[target_frame_id].pin_count << std::endl;
    return &buffer_data_pool_[target_frame_id][0]; // Retornar puntero a los datos
}

// Crea una nueva página en el disco y la carga en el buffer pool.
Byte* BufferManager::NewPage(PageId& new_page_id, PageType page_type) {
    // 1. Asignar un nuevo bloque en el disco.
    Status allocate_status = disk_manager_.AllocateBlock(page_type, new_page_id);
    if (allocate_status != Status::OK) {
        std::cerr << "Error (NewPage): Fallo al asignar un nuevo bloque en el DiskManager. " << StatusToString(allocate_status) << std::endl;
        return nullptr;
    }

    // 2. Buscar un frame libre o desalojar uno.
    FrameId target_frame_id = FindFreeFrame();
    if (target_frame_id == (FrameId)-1) { // No hay frames libres, intentar desalojar
        Status evict_status = EvictPage();
        if (evict_status != Status::OK) {
            std::cerr << "Error (NewPage): No se pudo desalojar una página para la nueva página " << new_page_id << "." << std::endl;
            // Si no se puede desalojar, desasignar el bloque recién asignado en el disco.
            disk_manager_.DeallocateBlock(new_page_id);
            return nullptr;
        }
        target_frame_id = FindFreeFrame(); // Obtener el frame liberado
        if (target_frame_id == (FrameId)-1) {
            std::cerr << "Error (NewPage): Después de desalojar, no se encontró un frame libre para la nueva página." << std::endl;
            disk_manager_.DeallocateBlock(new_page_id);
            return nullptr;
        }
    }

    // 3. Inicializar el frame en memoria con ceros (representa una página vacía).
    std::fill(buffer_data_pool_[target_frame_id].begin(), buffer_data_pool_[target_frame_id].end(), 0);

    // 4. Actualizar metadatos del frame y page_table_.
    frames_[target_frame_id].page_id = new_page_id;
    frames_[target_frame_id].pin_count = 1; // Anclada por el NewPage
    frames_[target_frame_id].is_dirty = true; // Nueva página, se considera sucia para ser escrita a disco
    frames_[target_frame_id].is_valid = true;
    page_table_[new_page_id] = target_frame_id; // Añadir al mapa

    replacement_policy_->Pin(target_frame_id); // Notificar a la política que la página está anclada
    replacement_policy_->Access(target_frame_id); // Notificar acceso

    // Escribir la página recién creada (vacía) al disco para persistencia inicial.
    // Esto asegura que el bloque exista físicamente en el disco.
    Block new_block(&buffer_data_pool_[target_frame_id][0], block_size_, block_size_);
    Status write_status = disk_manager_.WriteBlock(new_page_id, new_block);
    if (write_status != Status::OK) {
        std::cerr << "Error (NewPage): Fallo al escribir la nueva página " << new_page_id << " al disco. " << StatusToString(write_status) << std::endl;
        // Intentar revertir: desanclar, eliminar del page_table, desasignar del disco.
        UnpinPage(new_page_id, false); // Desanclar sin marcar sucia
        page_table_.erase(new_page_id);
        frames_[target_frame_id].Reset();
        replacement_policy_->RemoveFrame(target_frame_id);
        disk_manager_.DeallocateBlock(new_page_id);
        return nullptr;
    }

    std::cout << "Nueva página " << new_page_id << " creada y cargada en frame " << target_frame_id << "." << std::endl;
    return &buffer_data_pool_[target_frame_id][0]; // Retornar puntero a los datos
}

// Elimina una página del disco y del buffer pool (si está presente).
Status BufferManager::DeletePage(PageId page_id) {
    // 1. Verificar si la página está en el buffer pool.
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        FrameId frame_id = it->second;
        if (frames_[frame_id].pin_count > 0) {
            std::cerr << "Error (DeletePage): La página " << page_id << " está anclada (pin_count > 0) y no puede ser eliminada del buffer." << std::endl;
            return Status::PAGE_PINNED;
        }
        // Si está en el buffer y no anclada, removerla.
        if (frames_[frame_id].is_dirty) {
            Status write_status = WritePageToDisk(page_id); // Flush antes de eliminar
            if (write_status != Status::OK) {
                std::cerr << "Error (DeletePage): Fallo al flushar la página sucia " << page_id << " antes de eliminarla. " << StatusToString(write_status) << std::endl;
                return write_status;
            }
        }
        frames_[frame_id].Reset(); // Resetear metadatos del frame
        page_table_.erase(it); // Eliminar del mapa
        replacement_policy_->RemoveFrame(frame_id); // Notificar a la política
        std::cout << "Página " << page_id << " eliminada del buffer pool." << std::endl;
    }

    // 2. Desasignar el bloque del disco.
    Status deallocate_status = disk_manager_.DeallocateBlock(page_id);
    if (deallocate_status != Status::OK) {
        std::cerr << "Error (DeletePage): Fallo al desasignar el bloque " << page_id << " del DiskManager. " << StatusToString(deallocate_status) << std::endl;
        return deallocate_status;
    }

    std::cout << "Página " << page_id << " eliminada exitosamente del disco y del buffer pool (si estaba presente)." << std::endl;
    return Status::OK;
}

// Desancla una página, decrementando su pin_count.
Status BufferManager::UnpinPage(PageId page_id, bool is_dirty) {
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        std::cerr << "Error (UnpinPage): Página " << page_id << " no encontrada en el buffer pool." << std::endl;
        return Status::NOT_FOUND;
    }

    FrameId frame_id = it->second;
    if (frames_[frame_id].pin_count == 0) {
        std::cerr << "Advertencia (UnpinPage): Página " << page_id << " ya tiene pin_count 0. No se puede desanclar más." << std::endl;
        return Status::INVALID_PARAMETER; // O simplemente OK si no es un error crítico
    }

    frames_[frame_id].pin_count--;
    if (is_dirty) {
        frames_[frame_id].is_dirty = true;
    }

    // Notificar a la política de reemplazo si el pin_count llega a 0
    if (frames_[frame_id].pin_count == 0) {
        replacement_policy_->Unpin(frame_id);
    }

    // std::cout << "UnpinPage: Página " << page_id << " en frame " << frame_id << ". Pin count: " << frames_[frame_id].pin_count << ". Dirty: " << (frames_[frame_id].is_dirty ? "Yes" : "No") << std::endl;
    return Status::OK;
}

// Escribe todas las páginas marcadas como sucias del buffer pool al disco.
Status BufferManager::FlushAllPages() {
    Status overall_status = Status::OK;
    for (FrameId i = 0; i < pool_size_; ++i) {
        if (frames_[i].is_valid && frames_[i].is_dirty) {
            Status write_status = WritePageToDisk(frames_[i].page_id);
            if (write_status != Status::OK) {
                std::cerr << "Error (FlushAllPages): Fallo al flushar la página " << frames_[i].page_id << ". " << StatusToString(write_status) << std::endl;
                overall_status = write_status; // Registrar el primer error, pero continuar
            } else {
                frames_[i].is_dirty = false; // Limpiar el flag dirty si la escritura fue exitosa
            }
        }
    }
    return overall_status;
}

// Obtiene el número de páginas libres (frames vacíos) en el buffer pool.
uint32_t BufferManager::GetFreeFramesCount() const {
    uint32_t free_count = 0;
    for (const auto& page : frames_) {
        if (!page.is_valid) {
            free_count++;
        }
    }
    return free_count;
}

// Obtiene el número total de frames en el buffer pool.
uint32_t BufferManager::GetPoolSize() const {
    return pool_size_;
}

// Obtiene el número de páginas actualmente cargadas en el buffer pool.
uint32_t BufferManager::GetNumBufferedPages() const {
    return page_table_.size();
}

// Obtiene un puntero a los datos de un bloque lógico específico en la simulación de disco en RAM.
const Byte* BufferManager::GetSimulatedBlockData(PageId page_id) const {
    // Este método es un remanente si se usara un disco simulado en RAM directamente en BufferManager.
    // Con DiskManager, la E/S se hace a través de ReadBlock/WriteBlock.
    // Para obtener datos de una página en el buffer pool, se debe usar GetPageDataInPool.
    // Si la página no está en el buffer, este método no la cargará.
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        return &buffer_data_pool_[it->second][0];
    }
    std::cerr << "Error (GetSimulatedBlockData): Página " << page_id << " no está en el buffer pool." << std::endl;
    return nullptr;
}

// Obtiene un puntero a los datos de una página que ya está en el buffer pool.
// NO incrementa el pin_count. Usar con precaución (principalmente para depuración).
Byte* BufferManager::GetPageDataInPool(PageId page_id) {
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        return &buffer_data_pool_[it->second][0];
    }
    return nullptr; // Página no encontrada en el pool
}

// Actualiza el estado de un bloque en el DiskManager.
Status BufferManager::UpdateBlockStatusOnDisk(PageId page_id, BlockStatus status) {
    disk_manager_.UpdateBlockStatus(page_id, status);
    return Status::OK; // Asume que DiskManager maneja sus propios errores
}

// Método auxiliar para encontrar un frame libre en el buffer pool.
FrameId BufferManager::FindFreeFrame() {
    for (FrameId i = 0; i < pool_size_; ++i) {
        if (!frames_[i].is_valid) { // Un frame no válido está libre
            return i;
        }
    }
    return (FrameId)-1; // No hay frames libres
}

// Método auxiliar para desalojar una página del buffer pool.
Status BufferManager::EvictPage() {
    FrameId frame_to_evict = replacement_policy_->Evict();
    if (frame_to_evict == (FrameId)-1) {
        std::cerr << "Error (EvictPage): La política de reemplazo no pudo encontrar un frame desalojable." << std::endl;
        return Status::BUFFER_FULL; // No hay páginas desalojables
    }

    PageId page_id_to_evict = frames_[frame_to_evict].page_id;

    // Si la página está sucia, escribirla a disco antes de desalojarla.
    if (frames_[frame_to_evict].is_dirty) {
        Status write_status = WritePageToDisk(page_id_to_evict);
        if (write_status != Status::OK) {
            std::cerr << "Error (EvictPage): Fallo al escribir la página sucia " << page_id_to_evict << " al disco antes de desalojarla. " << StatusToString(write_status) << std::endl;
            return write_status;
        }
    }

    // Remover la página del page_table_ y resetear los metadatos del frame.
    page_table_.erase(page_id_to_evict);
    frames_[frame_to_evict].Reset();
    replacement_policy_->RemoveFrame(frame_to_evict); // Notificar a la política que el frame ha sido removido

    std::cout << "Página " << page_id_to_evict << " desalojada del frame " << frame_to_evict << "." << std::endl;
    return Status::OK;
}

// Método auxiliar para escribir una página (datos de buffer_data_pool_)
// de vuelta al disco físico (usando DiskManager).
Status BufferManager::WritePageToDisk(PageId page_id) {
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        std::cerr << "Error (WritePageToDisk): Página " << page_id << " no encontrada en el buffer pool para escritura." << std::endl;
        return Status::NOT_FOUND;
    }
    FrameId frame_id = it->second;

    Block block_to_write(&buffer_data_pool_[frame_id][0], block_size_, block_size_);
    Status write_status = disk_manager_.WriteBlock(page_id, block_to_write);
    if (write_status != Status::OK) {
        std::cerr << "Error (WritePageToDisk): Fallo al escribir el bloque " << page_id << " al disco. " << StatusToString(write_status) << std::endl;
        return write_status;
    }
    // std::cout << "Página " << page_id << " escrita a disco desde frame " << frame_id << "." << std::endl;
    return Status::OK;
}

// Método auxiliar para leer una página (datos del disco físico)
// y cargarla en buffer_data_pool_.
Status BufferManager::ReadPageFromDisk(PageId page_id, FrameId frame_id) {
    Block block_to_read(block_size_); // Crear un bloque vacío del tamaño correcto
    Status read_status = disk_manager_.ReadBlock(page_id, block_to_read);
    if (read_status != Status::OK) {
        std::cerr << "Error (ReadPageFromDisk): Fallo al leer el bloque " << page_id << " del disco. " << StatusToString(read_status) << std::endl;
        return read_status;
    }
    // Copiar los datos leídos del bloque al frame del buffer pool
    std::memcpy(&buffer_data_pool_[frame_id][0], block_to_read.GetData(), block_size_);
    // std::cout << "Página " << page_id << " leída del disco a frame " << frame_id << "." << std::endl;
    return Status::OK;
}
