// record_manager/record_manager.cpp
#include "record_manager.h"
#include <cstring> // Para std::memcpy
#include <algorithm> // Para std::min, std::max, std::fill
#include <iostream> // Para std::cout, std::cerr

// Constructor del RecordManager.
RecordManager::RecordManager(BufferManager& buffer_manager)
    : buffer_manager_(buffer_manager)
{
    // El tamaño base de la cabecera fija, sin contar el directorio de slots.
    fixed_header_base_size_ = sizeof(BlockHeader);
    std::cout << "RecordManager inicializado. Tamaño base de cabecera de bloque: " << fixed_header_base_size_ << " bytes." << std::endl;
}

// Lee la cabecera general del bloque.
BlockHeader RecordManager::ReadBlockHeader(Byte* page_data) const {
    BlockHeader header;
    std::memcpy(&header, page_data, sizeof(BlockHeader));
    return header;
}

// Escribe la cabecera general del bloque.
void RecordManager::WriteBlockHeader(Byte* page_data, const BlockHeader& header) const {
    std::memcpy(page_data, &header, sizeof(BlockHeader));
}

// Calcula el offset donde comienza el directorio de slots.
// El directorio de slots comienza inmediatamente después de la cabecera fija.
BlockSizeType RecordManager::GetSlotDirectoryStartOffset() const {
    return fixed_header_base_size_;
}

// Lee una entrada del directorio de slots.
SlotDirectoryEntry RecordManager::ReadSlotEntry(Byte* page_data, uint32_t slot_id) const {
    // Removed validation and header read here. Caller is responsible for validity.
    BlockSizeType slot_directory_entry_size = sizeof(SlotDirectoryEntry);
    BlockSizeType slot_offset = GetSlotDirectoryStartOffset() + (slot_id * slot_directory_entry_size);

    SlotDirectoryEntry entry;
    std::memcpy(&entry, page_data + slot_offset, slot_directory_entry_size);
    return entry;
}

// Escribe una entrada en el directorio de slots.
void RecordManager::WriteSlotEntry(Byte* page_data, uint32_t slot_id, const SlotDirectoryEntry& entry) const {
    // Removed validation and header read here. Caller is responsible for validity.
    BlockSizeType slot_directory_entry_size = sizeof(SlotDirectoryEntry);
    BlockSizeType slot_offset = GetSlotDirectoryStartOffset() + (slot_id * slot_directory_entry_size);

    std::memcpy(page_data + slot_offset, &entry, slot_directory_entry_size);
}


// Calcula el espacio libre real en una página.
BlockSizeType RecordManager::CalculateFreeSpace(Byte* page_data) const {
    BlockHeader header = ReadBlockHeader(page_data);
    if (header.page_type != PageType::DATA_PAGE) {
        return 0; // No aplica o no es una DATA_PAGE
    }

    // El espacio libre es la diferencia entre el inicio del espacio de registros
    // (que es data_end_offset) y el final del directorio de slots.
    BlockSizeType slot_directory_end_offset = GetSlotDirectoryStartOffset() + (header.num_slots * sizeof(SlotDirectoryEntry));
    return header.data_end_offset - slot_directory_end_offset;
}

// Inicializa una nueva página de datos (DATA_PAGE).
Status RecordManager::InitDataPage(PageId page_id) {
    Byte* page_data = buffer_manager_.FetchPage(page_id);
    if (page_data == nullptr) {
        std::cerr << "Error (InitDataPage): No se pudo obtener la página " << page_id << " del BufferManager." << std::endl;
        return Status::ERROR;
    }

    // Inicializar la cabecera general.
    BlockHeader header;
    header.page_id = page_id;
    header.page_type = PageType::DATA_PAGE;
    header.num_slots = 0; // Inicialmente no hay slots
    header.header_and_slot_directory_size = fixed_header_base_size_; // Tamaño inicial: solo la cabecera fija
    header.data_end_offset = buffer_manager_.GetBlockSize(); // El espacio de datos comienza desde el final del bloque

    // Escribir la cabecera en el bloque.
    WriteBlockHeader(page_data, header);

    // Llenar el resto del bloque con ceros para indicar espacio vacío.
    // Esto es útil para visualización y depuración.
    std::fill(page_data + fixed_header_base_size_, page_data + buffer_manager_.GetBlockSize(), 0);

    // Marcar la página como sucia y desanclarla.
    buffer_manager_.UnpinPage(page_id, true);

    // NOTA: El DiskManager ya marcó este bloque como INCOMPLETE al asignarlo.
    // Si la página está completamente vacía después de la inicialización,
    // podríamos forzar su estado a EMPTY si el DiskManager lo permite,
    // pero INCOMPLETE es un buen valor por defecto para una página de datos recién creada.

    std::cout << "Página de datos " << page_id << " inicializada. Tipo: DATA_PAGE." << std::endl;
    return Status::OK;
}

// Inserta un nuevo registro en una página de datos.
Status RecordManager::InsertRecord(PageId page_id, const Record& record_data, uint32_t& slot_id) {
    Byte* page_data = buffer_manager_.FetchPage(page_id);
    if (page_data == nullptr) {
        std::cerr << "Error (InsertRecord): No se pudo obtener la página " << page_id << " del BufferManager." << std::endl;
        return Status::ERROR;
    }

    BlockHeader header = ReadBlockHeader(page_data);
    if (header.page_type != PageType::DATA_PAGE) {
        std::cerr << "Error (InsertRecord): La página " << page_id << " no es de tipo DATA_PAGE. Tipo: " << PageTypeToString(header.page_type) << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::INVALID_PAGE_TYPE;
    }

    BlockSizeType record_length = record_data.data.size();
    BlockSizeType block_size = buffer_manager_.GetBlockSize();

    // 1. Encontrar un slot libre o determinar si se necesita uno nuevo.
    uint32_t found_slot_id = (uint32_t)-1;
    for (uint32_t i = 0; i < header.num_slots; ++i) { // Iterate over *existing* slots
        SlotDirectoryEntry entry = ReadSlotEntry(page_data, i);
        if (!entry.is_occupied) {
            found_slot_id = i;
            break;
        }
    }

    BlockSizeType space_needed_for_slot_entry = 0;
    if (found_slot_id == (uint32_t)-1) {
        // No hay slots libres existentes, se necesita añadir uno nuevo.
        found_slot_id = header.num_slots; // The new slot will be at the current num_slots index
        space_needed_for_slot_entry = sizeof(SlotDirectoryEntry);
        
        // IMPORTANT: Increment num_slots and update header_and_slot_directory_size *before* checking free space
        // and *before* writing the slot entry, so the header reflects the new state.
        header.num_slots++;
        header.header_and_slot_directory_size = GetSlotDirectoryStartOffset() + (header.num_slots * sizeof(SlotDirectoryEntry));
    }
    
    // Calculate total space needed: record data + (potential new slot entry)
    BlockSizeType total_space_needed = record_length + space_needed_for_slot_entry;

    // Check if there is enough free space for the record AND the new slot entry (if applicable).
    if (CalculateFreeSpace(page_data) < total_space_needed) {
        std::cout << "Advertencia (InsertRecord): Página " << page_id << " llena. No hay suficiente espacio para el registro de " << record_length << " bytes." << std::endl;
        // Revert header changes if we added a new slot but can't fit it.
        if (found_slot_id == header.num_slots - 1 && space_needed_for_slot_entry > 0) {
            header.num_slots--;
            header.header_and_slot_directory_size = GetSlotDirectoryStartOffset() + (header.num_slots * sizeof(SlotDirectoryEntry));
        }
        buffer_manager_.UnpinPage(page_id, false);
        return Status::BUFFER_FULL;
    }

    // 2. Asignar espacio para el registro (desde el final del bloque, moviéndose hacia el inicio).
    header.data_end_offset -= record_length; // Mover el puntero de fin de datos
    BlockSizeType record_offset = header.data_end_offset; // Este es el offset donde comienza el nuevo registro

    // 3. Copiar los datos del registro al bloque.
    std::memcpy(page_data + record_offset, record_data.data.data(), record_length);

    // 4. Actualizar la entrada del slot.
    SlotDirectoryEntry new_slot_entry;
    new_slot_entry.offset = record_offset;
    new_slot_entry.length = record_length;
    new_slot_entry.is_occupied = true;
    
    // Write the slot entry using the determined found_slot_id.
    WriteSlotEntry(page_data, found_slot_id, new_slot_entry);

    // 5. Write the updated header back to the block *after* all modifications.
    WriteBlockHeader(page_data, header);

    slot_id = found_slot_id; // Retornar el ID del slot asignado.

    // Marcar la página como sucia y desanclarla.
    buffer_manager_.UnpinPage(page_id, true);

    // 6. Actualizar el estado del bloque en DiskManager
    BlockStatus new_block_status = BlockStatus::INCOMPLETE;
    if (CalculateFreeSpace(page_data) == 0) {
        new_block_status = BlockStatus::FULL;
    }
    buffer_manager_.UpdateBlockStatusOnDisk(page_id, new_block_status);


    std::cout << "Registro insertado en Page " << page_id << ", Slot " << slot_id
              << ". Longitud: " << record_length << " bytes. Slots en página: " << header.num_slots
              << ". Espacio libre: " << CalculateFreeSpace(page_data) << " bytes." << std::endl;
    return Status::OK;
}

// Obtiene un registro de una página de datos.
Status RecordManager::GetRecord(PageId page_id, uint32_t slot_id, Record& record_data) {
    Byte* page_data = buffer_manager_.FetchPage(page_id);
    if (page_data == nullptr) {
        std::cerr << "Error (GetRecord): No se pudo obtener la página " << page_id << " del BufferManager." << std::endl;
        return Status::ERROR;
    }

    BlockHeader header = ReadBlockHeader(page_data);
    if (header.page_type != PageType::DATA_PAGE) {
        std::cerr << "Error (GetRecord): La página " << page_id << " no es de tipo DATA_PAGE. Tipo: " << PageTypeToString(header.page_type) << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::INVALID_PAGE_TYPE;
    }

    if (slot_id >= header.num_slots) { // Check against current number of slots
        std::cerr << "Error (GetRecord): Slot ID " << slot_id << " fuera de rango para Page " << page_id << "." << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::NOT_FOUND;
    }

    SlotDirectoryEntry entry = ReadSlotEntry(page_data, slot_id);
    if (!entry.is_occupied) {
        std::cerr << "Error (GetRecord): Slot " << slot_id << " en Page " << page_id << " no está ocupado." << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::NOT_FOUND;
    }

    // Redimensionar el vector de datos del registro y copiar.
    record_data.data.resize(entry.length);
    std::memcpy(record_data.data.data(), page_data + entry.offset, entry.length);

    // Desanclar la página.
    buffer_manager_.UnpinPage(page_id, false);

    std::cout << "Registro obtenido de Page " << page_id << ", Slot " << slot_id << "." << std::endl;
    return Status::OK;
}

// Actualiza un registro existente en una página de datos.
Status RecordManager::UpdateRecord(PageId page_id, uint32_t slot_id, const Record& new_record_data) {
    Byte* page_data = buffer_manager_.FetchPage(page_id);
    if (page_data == nullptr) {
        std::cerr << "Error (UpdateRecord): No se pudo obtener la página " << page_id << " del BufferManager." << std::endl;
        return Status::ERROR;
    }

    BlockHeader header = ReadBlockHeader(page_data);
    if (header.page_type != PageType::DATA_PAGE) {
        std::cerr << "Error (UpdateRecord): La página " << page_id << " no es de tipo DATA_PAGE. Tipo: " << PageTypeToString(header.page_type) << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::INVALID_PAGE_TYPE;
    }

    if (slot_id >= header.num_slots || !ReadSlotEntry(page_data, slot_id).is_occupied) { // Check against current number of slots
        std::cerr << "Error (UpdateRecord): Slot " << slot_id << " en Page " << page_id << " no está ocupado o es inválido para actualizar." << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::NOT_FOUND;
    }

    SlotDirectoryEntry old_entry = ReadSlotEntry(page_data, slot_id);
    BlockSizeType new_length = new_record_data.data.size();

    // If the new record is the same size or smaller, we can update in-place.
    if (new_length <= old_entry.length) {
        std::memcpy(page_data + old_entry.offset, new_record_data.data.data(), new_length);
        // If the new size is smaller, fill the rest with zeros (optional).
        if (new_length < old_entry.length) {
            std::fill(page_data + old_entry.offset + new_length, page_data + old_entry.offset + old_entry.length, 0);
        }
        // Update the length in the slot entry if it changed.
        if (new_length != old_entry.length) {
            old_entry.length = new_length;
            WriteSlotEntry(page_data, slot_id, old_entry);
        }
        // Mark the page as dirty and unpin it.
        buffer_manager_.UnpinPage(page_id, true);
    } else {
        // If the new record is larger, we need to reallocate.
        // This is a simplification; in a real SGBD, this could involve defragmentation
        // or searching for new free space. For this simulation, we will delete and reinsert.
        std::cerr << "Advertencia (UpdateRecord): Nuevo registro es más grande. Reubicando registro. Esto puede fragmentar el espacio." << std::endl;

        // Delete the old record (which marks the slot as free and cleans up data).
        // This will unpin the page and mark it dirty.
        Status delete_status = DeleteRecord(page_id, slot_id);
        if (delete_status != Status::OK) {
            std::cerr << "Error (UpdateRecord): Fallo al eliminar el registro antiguo para reubicación." << std::endl;
            // Page is already unpinned by DeleteRecord.
            return delete_status;
        }

        // Now, insert the new record.
        uint32_t new_slot_id; // Could be the same slot if it's reused immediately
        Status insert_status = InsertRecord(page_id, new_record_data, new_slot_id);
        if (insert_status != Status::OK) {
            std::cerr << "Error (UpdateRecord): Fallo al reinsertar el registro actualizado." << std::endl;
            // Page is already unpinned by InsertRecord.
            return insert_status;
        }
        slot_id = new_slot_id; // Update the slot_id if it changed.
        std::cout << "Registro reubicado y actualizado en Page " << page_id << ", Nuevo Slot " << slot_id << "." << std::endl;
    }

    // Update the block status in DiskManager after any update/reallocation
    BlockStatus new_block_status = BlockStatus::INCOMPLETE;
    if (CalculateFreeSpace(page_data) == 0) {
        new_block_status = BlockStatus::FULL;
    }
    buffer_manager_.UpdateBlockStatusOnDisk(page_id, new_block_status);

    std::cout << "Registro actualizado en Page " << page_id << ", Slot " << slot_id << "." << std::endl;
    return Status::OK;
}

// Elimina un registro de una página de datos.
Status RecordManager::DeleteRecord(PageId page_id, uint32_t slot_id) {
    Byte* page_data = buffer_manager_.FetchPage(page_id);
    if (page_data == nullptr) {
        std::cerr << "Error (DeleteRecord): No se pudo obtener la página " << page_id << " del BufferManager." << std::endl;
        return Status::ERROR;
    }

    BlockHeader header = ReadBlockHeader(page_data);
    if (header.page_type != PageType::DATA_PAGE) {
        std::cerr << "Error (DeleteRecord): La página " << page_id << " no es de tipo DATA_PAGE. Tipo: " << PageTypeToString(header.page_type) << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::INVALID_PAGE_TYPE;
    }

    if (slot_id >= header.num_slots || !ReadSlotEntry(page_data, slot_id).is_occupied) { // Check against current number of slots
        std::cerr << "Error (DeleteRecord): Slot " << slot_id << " en Page " << page_id << " no está ocupado o es inválido para eliminar." << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::NOT_FOUND;
    }

    SlotDirectoryEntry entry = ReadSlotEntry(page_data, slot_id);

    // Mark the slot as free. We don't reallocate data in this simple implementation.
    entry.is_occupied = false;
    WriteSlotEntry(page_data, slot_id, entry);

    // Optional: Clear the record data in the block (fill with zeros)
    std::fill(page_data + entry.offset, page_data + entry.offset + entry.length, 0);

    // NOTE: We do not decrement num_slots in the header, as the slot still exists
    // in the directory, just marked as free. This keeps slot_id stable and
    // allows for slot reuse.

    // Marcar la página como sucia y desanclarla.
    buffer_manager_.UnpinPage(page_id, true);

    // Actualizar el estado del bloque en DiskManager.
    // Si la página se vacía por completo, se marca como EMPTY.
    // Si tiene espacio libre después de la eliminación, se marca como INCOMPLETE.
    BlockStatus new_block_status = BlockStatus::INCOMPLETE;
    uint32_t num_occupied_records;
    GetNumRecords(page_id, num_occupied_records); // Get current number of occupied records
    
    if (num_occupied_records == 0) {
        new_block_status = BlockStatus::EMPTY;
    }
    buffer_manager_.UpdateBlockStatusOnDisk(page_id, new_block_status);


    std::cout << "Registro eliminado de Page " << page_id << ", Slot " << slot_id << "." << std::endl;
    return Status::OK;
}

// Obtiene el número de registros en una página.
Status RecordManager::GetNumRecords(PageId page_id, uint32_t& num_records) {
    Byte* page_data = buffer_manager_.FetchPage(page_id);
    if (page_data == nullptr) {
        std::cerr << "Error (GetNumRecords): No se pudo obtener la página " << page_id << " del BufferManager." << std::endl;
        return Status::ERROR;
    }
    BlockHeader header = ReadBlockHeader(page_data);
    if (header.page_type != PageType::DATA_PAGE) {
        std::cerr << "Error (GetNumRecords): La página " << page_id << " no es de tipo DATA_PAGE. Tipo: " << PageTypeToString(header.page_type) << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::INVALID_PAGE_TYPE;
    }

    // Count only the occupied slots.
    num_records = 0;
    for (uint32_t i = 0; i < header.num_slots; ++i) {
        if (ReadSlotEntry(page_data, i).is_occupied) {
            num_records++;
        }
    }

    buffer_manager_.UnpinPage(page_id, false);
    return Status::OK;
}

// Obtiene el espacio libre restante en una página.
Status RecordManager::GetFreeSpace(PageId page_id, BlockSizeType& free_space) {
    Byte* page_data = buffer_manager_.FetchPage(page_id);
    if (page_data == nullptr) {
        std::cerr << "Error (GetFreeSpace): No se pudo obtener la página " << page_id << " del BufferManager." << std::endl;
        return Status::ERROR;
    }
    BlockHeader header = ReadBlockHeader(page_data);
    if (header.page_type != PageType::DATA_PAGE) {
        std::cerr << "Error (GetFreeSpace): La página " << page_id << " no es de tipo DATA_PAGE. Tipo: " << PageTypeToString(header.page_type) << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::INVALID_PAGE_TYPE;
    }

    free_space = CalculateFreeSpace(page_data);
    buffer_manager_.UnpinPage(page_id, false);
    return Status::OK;
}
