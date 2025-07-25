// record_manager/record_manager.cpp
#include "record_manager.h"
#include "../Catalog_Manager/Catalog_Manager.h" // Incluir para la definición completa de CatalogManager
#include <cstring> // Para std::memcpy
#include <algorithm> // Para std::min, std::max, std::fill
#include <iostream> // Para std::cout, std::cerr

// Constructor del RecordManager.
RecordManager::RecordManager(BufferManager& buffer_manager)
    : buffer_manager_(buffer_manager), catalog_manager_(nullptr) // Inicializar puntero a nullptr
{
    // El tamaño base de la cabecera fija, sin contar el directorio de slots.
    fixed_header_base_size_ = sizeof(BlockHeader);
    std::cout << "RecordManager inicializado. Tamaño base de cabecera de bloque: " << fixed_header_base_size_ << " bytes." << std::endl;
}

// Método setter para el CatalogManager
void RecordManager::SetCatalogManager(CatalogManager& catalog_manager) {
    catalog_manager_ = &catalog_manager;
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

// Inicializa una nueva página de datos con la cabecera adecuada.
Status RecordManager::InitDataPage(PageId page_id) {
    Byte* page_data = buffer_manager_.FetchPage(page_id);
    if (page_data == nullptr) {
        std::cerr << "Error (InitDataPage): No se pudo obtener la página " << page_id << " del BufferManager." << std::endl;
        return Status::ERROR;
    }

    BlockHeader header;
    header.page_id = page_id;
    header.page_type = PageType::DATA_PAGE; // O CATALOG_PAGE si se usa para el catálogo
    header.data_end_offset = buffer_manager_.GetBlockSize(); // Los datos crecen hacia el inicio
    header.num_slots = 0;
    header.header_and_slot_directory_size = fixed_header_base_size_; // Inicialmente solo la cabecera fija

    WriteBlockHeader(page_data, header);

    // Marcar la página como sucia y desanclarla.
    buffer_manager_.UnpinPage(page_id, true);
    std::cout << "Página de datos " << page_id << " inicializada." << std::endl;
    return Status::OK;
}

// Inserta un registro en una página de datos.
Status RecordManager::InsertRecord(PageId page_id, const Record& record, uint32_t& slot_id) {
    Byte* page_data = buffer_manager_.FetchPage(page_id);
    if (page_data == nullptr) {
        std::cerr << "Error (InsertRecord): No se pudo obtener la página " << page_id << " del BufferManager." << std::endl;
        return Status::ERROR;
    }

    BlockHeader header = ReadBlockHeader(page_data);
    if (header.page_type != PageType::DATA_PAGE && header.page_type != PageType::CATALOG_PAGE) {
        std::cerr << "Error (InsertRecord): La página " << page_id << " no es de tipo DATA_PAGE o CATALOG_PAGE. Tipo: " << PageTypeToString(header.page_type) << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::INVALID_PAGE_TYPE;
    }

    BlockSizeType record_size = record.data.size();
    BlockSizeType free_space;
    GetFreeSpace(page_id, free_space); // Esto ancla y desancla la página internamente

    // Re-fetch page_data as GetFreeSpace might unpin it
    page_data = buffer_manager_.FetchPage(page_id);
    if (page_data == nullptr) {
        std::cerr << "Error (InsertRecord): No se pudo re-obtener la página " << page_id << " del BufferManager." << std::endl;
        return Status::ERROR;
    }
    header = ReadBlockHeader(page_data); // Re-read header after re-fetching

    // Calcular el espacio necesario: tamaño del registro + tamaño de una entrada de slot
    BlockSizeType required_space = record_size + sizeof(SlotDirectoryEntry);

    if (free_space < required_space) {
        std::cerr << "Error (InsertRecord): Espacio insuficiente en la página " << page_id << ". Libre: " << free_space << ", Necesario: " << required_space << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::BUFFER_FULL; // O DISK_FULL si no se puede asignar una nueva página
    }

    // Buscar un slot libre o añadir uno nuevo
    int found_slot_id = -1;
    for (uint32_t i = 0; i < header.num_slots; ++i) {
        SlotDirectoryEntry entry = ReadSlotEntry(page_data, i);
        if (!entry.is_occupied) {
            found_slot_id = i;
            break;
        }
    }

    if (found_slot_id == -1) {
        // No hay slots libres, añadir uno nuevo al final del directorio
        found_slot_id = header.num_slots;
        header.num_slots++;
        header.header_and_slot_directory_size += sizeof(SlotDirectoryEntry); // El directorio de slots crece
    }
    slot_id = found_slot_id; // Devolver el slot_id asignado

    // Calcular el offset donde se almacenará el nuevo registro (crece desde el final de la página)
    header.data_end_offset -= record_size;
    BlockSizeType record_offset = header.data_end_offset;

    // Copiar los datos del registro al bloque
    std::memcpy(page_data + record_offset, record.data.data(), record_size);

    // Actualizar la entrada del directorio de slots
    SlotDirectoryEntry new_entry;
    new_entry.offset = record_offset;
    new_entry.length = record_size;
    new_entry.is_occupied = true;
    WriteSlotEntry(page_data, slot_id, new_entry);

    // Escribir la cabecera actualizada de nuevo al bloque
    WriteBlockHeader(page_data, header);

    // Marcar la página como sucia y desanclarla.
    buffer_manager_.UnpinPage(page_id, true);
    std::cout << "Registro insertado en Page " << page_id << ", Slot " << slot_id << ". Tamaño: " << record_size << " bytes." << std::endl;
    return Status::OK;
}

// Obtiene un registro de una página de datos.
Status RecordManager::GetRecord(PageId page_id, uint32_t slot_id, Record& record) {
    Byte* page_data = buffer_manager_.FetchPage(page_id);
    if (page_data == nullptr) {
        std::cerr << "Error (GetRecord): No se pudo obtener la página " << page_id << " del BufferManager." << std::endl;
        return Status::ERROR;
    }

    BlockHeader header = ReadBlockHeader(page_data);
    if (header.page_type != PageType::DATA_PAGE && header.page_type != PageType::CATALOG_PAGE) {
        std::cerr << "Error (GetRecord): La página " << page_id << " no es de tipo DATA_PAGE o CATALOG_PAGE. Tipo: " << PageTypeToString(header.page_type) << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::INVALID_PAGE_TYPE;
    }

    if (slot_id >= header.num_slots) {
        std::cerr << "Error (GetRecord): SlotId " << slot_id << " fuera de rango para la página " << page_id << "." << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::NOT_FOUND;
    }

    SlotDirectoryEntry entry = ReadSlotEntry(page_data, slot_id);
    if (!entry.is_occupied) {
        std::cerr << "Error (GetRecord): El slot " << slot_id << " en la página " << page_id << " está vacío." << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::NOT_FOUND;
    }

    // Copiar los datos del registro
    record.data.resize(entry.length);
    std::memcpy(record.data.data(), page_data + entry.offset, entry.length);

    buffer_manager_.UnpinPage(page_id, false);
    return Status::OK;
}

// Actualiza un registro existente en una página de datos.
Status RecordManager::UpdateRecord(PageId page_id, uint32_t slot_id, const Record& new_record) {
    Byte* page_data = buffer_manager_.FetchPage(page_id);
    if (page_data == nullptr) {
        std::cerr << "Error (UpdateRecord): No se pudo obtener la página " << page_id << " del BufferManager." << std::endl;
        return Status::ERROR;
    }

    BlockHeader header = ReadBlockHeader(page_data);
    if (header.page_type != PageType::DATA_PAGE && header.page_type != PageType::CATALOG_PAGE) {
        std::cerr << "Error (UpdateRecord): La página " << page_id << " no es de tipo DATA_PAGE o CATALOG_PAGE. Tipo: " << PageTypeToString(header.page_type) << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::INVALID_PAGE_TYPE;
    }

    if (slot_id >= header.num_slots) {
        std::cerr << "Error (UpdateRecord): SlotId " << slot_id << " fuera de rango para la página " << page_id << "." << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::NOT_FOUND;
    }

    SlotDirectoryEntry old_entry = ReadSlotEntry(page_data, slot_id);
    if (!old_entry.is_occupied) {
        std::cerr << "Error (UpdateRecord): El slot " << slot_id << " en la página " << page_id << " está vacío." << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::NOT_FOUND;
    }

    // Si el nuevo registro es del mismo tamaño o más pequeño, podemos sobrescribir directamente.
    if (new_record.data.size() <= old_entry.length) {
        std::memcpy(page_data + old_entry.offset, new_record.data.data(), new_record.data.size());
        // Si el nuevo registro es más pequeño, rellenar el resto del espacio con ceros o un marcador.
        if (new_record.data.size() < old_entry.length) {
            std::fill(page_data + old_entry.offset + new_record.data.size(),
                      page_data + old_entry.offset + old_entry.length, 0);
        }
        // Actualizar la longitud en la entrada del slot si cambió
        old_entry.length = new_record.data.size();
        WriteSlotEntry(page_data, slot_id, old_entry);
        buffer_manager_.UnpinPage(page_id, true); // Marcar sucia
        std::cout << "Registro actualizado en Page " << page_id << ", Slot " << slot_id << " (sobrescritura)." << std::endl;
        return Status::OK;
    } else {
        // Si el nuevo registro es más grande, necesitamos reubicarlo.
        // Primero, marcar el slot actual como libre y liberar el espacio antiguo.
        old_entry.is_occupied = false;
        WriteSlotEntry(page_data, slot_id, old_entry);
        
        // Compactar la página para liberar el espacio (opcional, pero buena práctica)
        // Para esta simulación, simplificaremos y solo reinsertaremos.
        
        // Intentar insertar el nuevo registro. Esto buscará un nuevo espacio.
        uint32_t new_slot_id;
        Status insert_status = InsertRecord(page_id, new_record, new_slot_id);
        if (insert_status != Status::OK) {
            std::cerr << "Error (UpdateRecord): Fallo al reinsertar el registro actualizado en la página " << page_id << ". Status: " << StatusToString(insert_status) << std::endl;
            // Si la reinserción falla, la página original ya está modificada (slot marcado como libre).
            // Esto es un estado inconsistente. En un SGBD real, se usarían transacciones para revertir.
            buffer_manager_.UnpinPage(page_id, true); // Marcar sucia por el cambio de slot
            return insert_status;
        }
        std::cout << "Registro actualizado en Page " << page_id << ", Slot " << slot_id << " (reubicado a Slot " << new_slot_id << ")." << std::endl;
        // La página ya fue desanclada y marcada sucia por InsertRecord.
        return Status::OK;
    }
}

// Elimina un registro de una página de datos.
Status RecordManager::DeleteRecord(PageId page_id, uint32_t slot_id) {
    Byte* page_data = buffer_manager_.FetchPage(page_id);
    if (page_data == nullptr) {
        std::cerr << "Error (DeleteRecord): No se pudo obtener la página " << page_id << " del BufferManager." << std::endl;
        return Status::ERROR;
    }

    BlockHeader header = ReadBlockHeader(page_data);
    if (header.page_type != PageType::DATA_PAGE && header.page_type != PageType::CATALOG_PAGE) {
        std::cerr << "Error (DeleteRecord): La página " << page_id << " no es de tipo DATA_PAGE o CATALOG_PAGE. Tipo: " << PageTypeToString(header.page_type) << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::INVALID_PAGE_TYPE;
    }

    if (slot_id >= header.num_slots) {
        std::cerr << "Error (DeleteRecord): SlotId " << slot_id << " fuera de rango para la página " << page_id << "." << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::NOT_FOUND;
    }

    SlotDirectoryEntry entry = ReadSlotEntry(page_data, slot_id);
    if (!entry.is_occupied) {
        std::cerr << "Error (DeleteRecord): El slot " << slot_id << " en la página " << page_id << " ya está vacío." << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::NOT_FOUND;
    }

    // Marcar el slot como no ocupado.
    entry.is_occupied = false;
    WriteSlotEntry(page_data, slot_id, entry);

    // Opcional: Compactar la página para recuperar el espacio.
    // Para esta simulación, simplemente marcamos el slot como libre.
    // El espacio se recuperará cuando se inserte un nuevo registro que quepa en un slot libre
    // o cuando la página se compacte explícitamente.

    buffer_manager_.UnpinPage(page_id, true); // Marcar sucia
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
    if (header.page_type != PageType::DATA_PAGE && header.page_type != PageType::CATALOG_PAGE) {
        std::cerr << "Error (GetNumRecords): La página " << page_id << " no es de tipo DATA_PAGE o CATALOG_PAGE. Tipo: " << PageTypeToString(header.page_type) << std::endl;
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
    if (header.page_type != PageType::DATA_PAGE && header.page_type != PageType::CATALOG_PAGE) {
        std::cerr << "Error (GetFreeSpace): La página " << page_id << " no es de tipo DATA_PAGE o CATALOG_PAGE. Tipo: " << PageTypeToString(header.page_type) << std::endl;
        buffer_manager_.UnpinPage(page_id, false);
        return Status::INVALID_PAGE_TYPE;
    }

    // El espacio libre es la diferencia entre el offset donde terminan los datos
    // y el offset donde termina el directorio de slots.
    free_space = header.data_end_offset - header.header_and_slot_directory_size;

    buffer_manager_.UnpinPage(page_id, false);
    return Status::OK;
}

// Métodos auxiliares para manipular el directorio de slots
SlotDirectoryEntry RecordManager::ReadSlotEntry(Byte* page_data, uint32_t slot_id) const {
    SlotDirectoryEntry entry;
    BlockSizeType offset = GetSlotDirectoryStartOffset() + (slot_id * sizeof(SlotDirectoryEntry));
    std::memcpy(&entry, page_data + offset, sizeof(SlotDirectoryEntry));
    return entry;
}

void RecordManager::WriteSlotEntry(Byte* page_data, uint32_t slot_id, const SlotDirectoryEntry& entry) const {
    BlockSizeType offset = GetSlotDirectoryStartOffset() + (slot_id * sizeof(SlotDirectoryEntry));
    std::memcpy(page_data + offset, &entry, sizeof(SlotDirectoryEntry));
}
