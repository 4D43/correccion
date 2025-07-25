// data_storage/buffer_manager.cpp
#include "buffer_manager.h"
#include "block.h" // Para usar la clase Block para E/S con DiskManager
#include <cstring> // Para std::memcpy
#include <iostream> // Para std::cout, std::cerr

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

    // Inicializar el "vector de arreglos estáticos" que simula todo el disco en RAM.
    // Su tamaño es el número total de bloques lógicos en el disco.
    buffer_data_simulated_disk_.resize(disk_manager_.GetTotalLogicalBlocks());
    // Redimensionar cada bloque simulado al tamaño correcto.
    for (auto& block_data : buffer_data_simulated_disk_) {
        block_data.resize(block_size_, 0); // Cada vector<Byte> se redimensiona a block_size_
    }

    // Notificar a la política de reemplazo sobre los frames disponibles.
    for (FrameId i = 0; i < pool_size_; ++i) {
        replacement_policy_->AddFrame(i);
    }

    std::cout << "BufferManager inicializado con pool_size: " << pool_size_ << std::endl;
    std::cout << "Tamaño de bloque lógico en BufferManager: " << block_size_ << " bytes." << std::endl;
    std::cout << "Tamaño del buffer de disco simulado (total bloques): " << buffer_data_simulated_disk_.size() << " bloques." << std::endl;
}

// Destructor del BufferManager.
// Asegura que todas las páginas sucias sean escritas de vuelta al disco.
BufferManager::~BufferManager() {
    std::cout << "Destructor del BufferManager: FlushAllPages..." << std::endl;
    FlushAllPages(); // Asegura que los datos modificados se persistan.
    std::cout << "BufferManager destruido." << std::endl;
}

// Método auxiliar para encontrar un frame libre en el buffer pool.
FrameId BufferManager::FindFreeFrame() {
    for (FrameId i = 0; i < pool_size_; ++i) {
        if (!frames_[i].is_valid) { // Si el frame no contiene datos válidos, está libre.
            return i;
        }
    }
    return (FrameId)-1; // No hay frames libres disponibles.
}

// Método auxiliar para escribir una página (datos de buffer_data_simulated_disk_)
// de vuelta al disco físico (usando DiskManager).
Status BufferManager::WritePageToDisk(PageId page_id) {
    // Obtener la dirección física del DiskManager
    PhysicalAddress physical_address = disk_manager_.GetPhysicalAddress(page_id);
    // Si GetPhysicalAddress imprime un error, ya se maneja allí.
    if (physical_address.platter_id == 0 && physical_address.surface_id == 0 &&
        physical_address.track_id == 0 && physical_address.sector_id == 0 && page_id != 0) {
        // Esto es una heurística simple para detectar una dirección inválida si no es PageId 0
        // (asumiendo que PageId 0 siempre es (0,0,0,0) y es válido).
        return Status::NOT_FOUND; // O un estado más específico como INVALID_BLOCK_ID
    }

    // Crear un Block temporal con el tamaño correcto y copiar los datos.
    Block temp_block(buffer_data_simulated_disk_[page_id].data(), block_size_, block_size_);

    std::cout << "Escribiendo PageId " << page_id << " (física: P" << physical_address.platter_id
              << " S" << physical_address.surface_id << " T" << physical_address.track_id
              << " Sec" << physical_address.sector_id << ") al disco..." << std::endl;

    Status write_status = disk_manager_.WriteBlock(physical_address, temp_block);
    if (write_status != Status::OK) {
        std::cerr << "Error (WritePageToDisk): Fallo al escribir PageId " << page_id << " al disco." << std::endl;
        return write_status;
    }
    return Status::OK;
}

// Método auxiliar para leer una página (datos del disco físico)
// y cargarla en buffer_data_simulated_disk_.
Status BufferManager::ReadPageFromDisk(PageId page_id) {
    // Obtener la dirección física del DiskManager
    PhysicalAddress physical_address = disk_manager_.GetPhysicalAddress(page_id);
    // Si GetPhysicalAddress imprime un error, ya se maneja allí.
    if (physical_address.platter_id == 0 && physical_address.surface_id == 0 &&
        physical_address.track_id == 0 && physical_address.sector_id == 0 && page_id != 0) {
        return Status::NOT_FOUND;
    }

    // Crear un Block temporal del tamaño correcto para leer los datos del DiskManager.
    Block temp_block(block_size_);
    std::cout << "Leyendo PageId " << page_id << " (física: P" << physical_address.platter_id
              << " S" << physical_address.surface_id << " T" << physical_address.track_id
              << " Sec" << physical_address.sector_id << ") desde disco..." << std::endl;

    Status read_status = disk_manager_.ReadBlock(physical_address, temp_block);
    if (read_status != Status::OK) {
        std::cerr << "Error (ReadPageFromDisk): Fallo al leer PageId " << page_id << " desde disco." << std::endl;
        return read_status;
    }

    // Copiar los datos leídos del Block temporal a la posición de page_id en buffer_data_simulated_disk_.
    std::memcpy(buffer_data_simulated_disk_[page_id].data(), temp_block.GetData(), block_size_);
    return Status::OK;
}

// Método auxiliar para desalojar una página del buffer pool.
Status BufferManager::EvictPage() {
    FrameId frame_to_evict = replacement_policy_->Evict();

    if (frame_to_evict == (FrameId)-1) { // Asumiendo -1 como "no se puede desalojar"
        std::cerr << "Error (EvictPage): No hay páginas desalojables disponibles en el buffer pool." << std::endl;
        return Status::BUFFER_FULL; // O PAGE_PINNED si todas están ancladas
    }

    Page& page_info = frames_[frame_to_evict];

    if (page_info.pin_count > 0) {
        std::cerr << "Error (EvictPage): Intentando desalojar una página anclada (pinned): " << page_info.page_id << std::endl;
        return Status::PAGE_PINNED;
    }

    // Si la página está sucia, escribirla de vuelta al disco.
    if (page_info.is_dirty) {
        Status write_status = WritePageToDisk(page_info.page_id);
        if (write_status != Status::OK) {
            std::cerr << "Error (EvictPage): Fallo al escribir la página sucia " << page_info.page_id << " al disco." << std::endl;
            return write_status;
        }
    }

    // Remover la página de la tabla de páginas y resetear el frame.
    page_table_.erase(page_info.page_id);
    replacement_policy_->RemoveFrame(frame_to_evict); // Notificar a la política
    frames_[frame_to_evict].Reset(); // Limpiar metadatos del frame

    std::cout << "Página desalojada del frame " << frame_to_evict << " (PageId " << page_info.page_id << ")." << std::endl;
    return Status::OK;
}

// Solicita una página (bloque) del buffer pool.
Byte* BufferManager::FetchPage(PageId page_id) {
    // Validar que el page_id esté dentro del rango del disco simulado
    if (page_id >= disk_manager_.GetTotalLogicalBlocks()) { // Usar total logical blocks del disk manager
        std::cerr << "Error (FetchPage): PageId " << page_id << " fuera de rango para el disco simulado." << std::endl;
        return nullptr;
    }

    // 1. Verificar si la página ya está en el buffer pool.
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        FrameId frame_id = it->second;
        frames_[frame_id].pin_count++; // Incrementar pin_count
        replacement_policy_->Pin(frame_id); // Notificar a la política que está anclada
        replacement_policy_->Access(frame_id); // Notificar acceso
        std::cout << "Página " << page_id << " ya en buffer (frame " << frame_id << "). Pin count: " << frames_[frame_id].pin_count << std::endl;
        return buffer_data_simulated_disk_[page_id].data(); // Retornar puntero a los datos del bloque lógico
    }

    // 2. La página no está en el buffer, intentar encontrar un frame libre o desalojar uno.
    FrameId frame_id = FindFreeFrame();
    if (frame_id == (FrameId)-1) { // No hay frames libres, intentar desalojar.
        std::cout << "Buffer lleno. Intentando desalojar una página..." << std::endl;
        Status evict_status = EvictPage();
        if (evict_status != Status::OK) {
            std::cerr << "Error (FetchPage): No se pudo desalojar una página para cargar " << page_id << std::endl;
            return nullptr; // No se pudo obtener un frame
        }
        // Después de desalojar, FindFreeFrame debería encontrar el frame recién liberado.
        frame_id = FindFreeFrame();
        if (frame_id == (FrameId)-1) { // Esto no debería pasar si EvictPage fue OK
            std::cerr << "Error (FetchPage): EvictPage exitoso pero no se encontró frame libre." << std::endl;
            return nullptr;
        }
    }

    // 3. Cargar la página desde el disco al buffer_data_simulated_disk_ en la posición de page_id.
    Status read_status = ReadPageFromDisk(page_id);
    if (read_status != Status::OK) {
        std::cerr << "Error (FetchPage): Fallo al leer PageId " << page_id << " desde disco." << std::endl;
        return nullptr;
    }

    // Actualizar metadatos del frame y tabla de páginas.
    frames_[frame_id].page_id = page_id;
    frames_[frame_id].pin_count = 1; // Anclada al cargar
    frames_[frame_id].is_dirty = false; // Recién cargada, no sucia
    frames_[frame_id].is_valid = true;  // Contiene datos válidos

    page_table_[page_id] = frame_id; // Añadir al mapa de PageId a FrameId

    replacement_policy_->Pin(frame_id); // Notificar a la política que está anclada
    replacement_policy_->Access(frame_id); // Notificar acceso

    std::cout << "Página " << page_id << " cargada en frame " << frame_id << ". Pin count: " << frames_[frame_id].pin_count << std::endl;
    return buffer_data_simulated_disk_[page_id].data(); // Retornar puntero a los datos del bloque lógico
}

// Desancla una página del buffer pool.
Status BufferManager::UnpinPage(PageId page_id, bool is_dirty) {
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        std::cerr << "Error (UnpinPage): Página " << page_id << " no encontrada en el buffer pool." << std::endl;
        return Status::NOT_FOUND;
    }

    FrameId frame_id = it->second;
    Page& page_info = frames_[frame_id];

    if (page_info.pin_count <= 0) {
        std::cerr << "Advertencia (UnpinPage): Intentando desanclar una página ya desanclada o con pin_count <= 0: " << page_id << std::endl;
        return Status::ERROR; // O un estado más específico como ALREADY_UNPINNED
    }

    page_info.pin_count--; // Decrementar pin_count
    if (is_dirty) {
        page_info.is_dirty = true; // Marcar como sucia si se indica
    }

    replacement_policy_->Unpin(frame_id); // Notificar a la política que está desanclada

    std::cout << "Página " << page_id << " desanclada (frame " << frame_id << "). Pin count: " << page_info.pin_count << ". Dirty: " << page_info.is_dirty << std::endl;
    return Status::OK;
}

// Fuerza la escritura de una página específica de vuelta al disco, si está sucia.
Status BufferManager::FlushPage(PageId page_id) {
    auto it_page_table = page_table_.find(page_id);
    if (it_page_table == page_table_.end()) {
        std::cout << "Página " << page_id << " no está en el buffer pool, no se puede flushar." << std::endl;
        return Status::NOT_FOUND;
    }

    FrameId frame_id = it_page_table->second;
    Page& page_info = frames_[frame_id];

    if (!page_info.is_dirty) {
        std::cout << "Página " << page_id << " (frame " << frame_id << ") no está sucia, no se necesita flush." << std::endl;
        return Status::OK;
    }

    Status write_status = WritePageToDisk(page_id);
    if (write_status == Status::OK) {
        page_info.is_dirty = false; // Marcar como no sucia después de escribir
        std::cout << "Página " << page_id << " (frame " << frame_id << ") flusheada exitosamente." << std::endl;
    } else {
        std::cerr << "Error (FlushPage): Fallo al flushar la página " << page_id << " (frame " << frame_id << ")." << std::endl;
    }
    return write_status;
}

// Fuerza la escritura de TODAS las páginas sucias de vuelta al disco.
Status BufferManager::FlushAllPages() {
    Status overall_status = Status::OK;
    for (FrameId i = 0; i < pool_size_; ++i) {
        if (frames_[i].is_valid && frames_[i].is_dirty) {
            PageId page_id = frames_[i].page_id;
            Status status = WritePageToDisk(page_id);
            if (status != Status::OK) {
                overall_status = status; // Registrar el primer error, pero intentar flush de todas.
                std::cerr << "Error (FlushAllPages): Fallo al escribir la página " << page_id << " al disco." << std::endl;
            } else {
                frames_[i].is_dirty = false; // Marcar como no sucia
            }
        }
    }
    return overall_status;
}

// Crea una nueva página (bloque) en el disco y la carga en el buffer pool.
Byte* BufferManager::NewPage(PageId& new_page_id, PageType page_type) {
    // 1. Asignar un nuevo bloque en el disco físico, con sugerencia de tipo.
    PhysicalAddress allocated_address;
    Status allocate_status = disk_manager_.AllocateBlock(new_page_id, allocated_address, page_type);
    if (allocate_status != Status::OK) {
        std::cerr << "Error (NewPage): No se pudo asignar un nuevo bloque en el disco." << std::endl;
        return nullptr;
    }

    // Asegurarse de que el new_page_id no exceda el tamaño de buffer_data_simulated_disk_
    if (new_page_id >= buffer_data_simulated_disk_.size()) {
        std::cerr << "Error (NewPage): El PageId generado " << new_page_id << " excede el tamaño del buffer de disco simulado (" << buffer_data_simulated_disk_.size() << ")." << std::endl;
        // No llamamos a disk_manager_.DeallocateBlock aquí, ya que AllocateBlock ya lo hizo si falló.
        // Pero si el error es por nuestro buffer_data_simulated_disk_ ser demasiado pequeño,
        // necesitamos desasignar del disco.
        disk_manager_.DeallocateBlock(new_page_id); // Desasignar el bloque recién asignado
        return nullptr;
    }

    // 2. Intentar encontrar un frame libre en el buffer pool o desalojar uno.
    FrameId frame_id = FindFreeFrame();
    if (frame_id == (FrameId)-1) {
        std::cout << "Buffer lleno. Intentando desalojar una página para nueva página..." << std::endl;
        Status evict_status = EvictPage();
        if (evict_status != Status::OK) {
            std::cerr << "Error (NewPage): No se pudo desalojar una página para la nueva página " << new_page_id << std::endl;
            disk_manager_.DeallocateBlock(new_page_id); // Revertir asignación de disco
            return nullptr;
        }
        frame_id = FindFreeFrame(); // Debería encontrar el frame recién liberado
        if (frame_id == (FrameId)-1) {
            std::cerr << "Error (NewPage): EvictPage exitoso pero no se encontró frame libre." << std::endl;
            disk_manager_.DeallocateBlock(new_page_id);
            return nullptr;
        }
    }

    // 3. Inicializar el contenido del nuevo bloque en buffer_data_simulated_disk_.
    std::fill(buffer_data_simulated_disk_[new_page_id].begin(), buffer_data_simulated_disk_[new_page_id].end(), 0); // Llenar con ceros

    // 4. Actualizar metadatos del frame y tabla de páginas.
    frames_[frame_id].page_id = new_page_id;
    frames_[frame_id].pin_count = 1; // Anclada al crear
    frames_[frame_id].is_dirty = true; // Es nueva, debe ser escrita al disco
    frames_[frame_id].is_valid = true;  // Contiene datos válidos

    page_table_[new_page_id] = frame_id; // Añadir al mapa de PageId a FrameId

    replacement_policy_->Pin(frame_id); // Notificar a la política que está anclada
    replacement_policy_->Access(frame_id); // Notificar acceso

    std::cout << "Nueva página " << new_page_id << " (" << PageTypeToString(page_type)
              << ") creada en disco (física: P" << allocated_address.platter_id
              << " S" << allocated_address.surface_id << " T" << allocated_address.track_id
              << " Sec" << allocated_address.sector_id << ") y cargada en frame " << frame_id << "." << std::endl;
    return buffer_data_simulated_disk_[new_page_id].data();
}

// Elimina una página (bloque) del disco y del buffer pool si está presente.
Status BufferManager::DeletePage(PageId page_id) {
    // No permitir eliminar la DISK_METADATA_PAGE (PageId 0)
    if (page_id == 0) {
        std::cerr << "Error (DeletePage): No se puede eliminar la DISK_METADATA_PAGE (PageId 0)." << std::endl;
        return Status::INVALID_PARAMETER;
    }

    auto it_page_table = page_table_.find(page_id);
    if (it_page_table != page_table_.end()) {
        FrameId frame_id = it_page_table->second;
        Page& page_info = frames_[frame_id];

        if (page_info.pin_count > 0) {
            std::cerr << "Error (DeletePage): No se puede eliminar la página " << page_id << " porque está anclada (pin_count: " << page_info.pin_count << ")." << std::endl;
            return Status::PAGE_PINNED;
        }

        // Si está sucia, forzar la escritura antes de eliminarla del disco.
        if (page_info.is_dirty) {
            Status flush_status = WritePageToDisk(page_id);
            if (flush_status != Status::OK) {
                std::cerr << "Error (DeletePage): Fallo al flushar la página sucia " << page_id << " antes de eliminarla." << std::endl;
                return flush_status;
            }
        }

        // Remover del mapa del buffer y notificar a la política
        page_table_.erase(page_id);
        replacement_policy_->RemoveFrame(frame_id);
        frames_[frame_id].Reset(); // Limpiar metadatos del frame
        std::cout << "Página " << page_id << " removida del buffer pool (frame " << frame_id << ")." << std::endl;
    } else {
        std::cout << "Página " << page_id << " no encontrada en el buffer pool. Solo se eliminará del disco." << std::endl;
    }

    // Desasignar el bloque del disco físico a través del DiskManager.
    Status deallocate_status = disk_manager_.DeallocateBlock(page_id);
    if (deallocate_status != Status::OK) {
        std::cerr << "Error (DeletePage): Fallo al desasignar el bloque " << page_id << " del disco." << std::endl;
        return deallocate_status;
    }

    std::cout << "Página " << page_id << " eliminada exitosamente del disco y del buffer pool (si estaba presente)." << std::endl;
    return Status::OK;
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
    if (page_id >= buffer_data_simulated_disk_.size()) {
        std::cerr << "Error (GetSimulatedBlockData): PageId " << page_id << " fuera de rango para el disco simulado." << std::endl;
        return nullptr;
    }
    return buffer_data_simulated_disk_[page_id].data();
}

// Método delegado para actualizar el estado de un bloque en el disco.
Status BufferManager::UpdateBlockStatusOnDisk(PageId page_id, BlockStatus new_status) {
    return disk_manager_.UpdateBlockStatus(page_id, new_status);
}
