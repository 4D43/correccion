// record_manager/record_manager.h
#ifndef RECORD_MANAGER_H
#define RECORD_MANAGER_H

#include "../include/common.h"     // Para Status, Byte, BlockSizeType, PageId, PageType
#include "../data_storage/buffer_manager.h" // Para BufferManager (dependencia)
#include <vector>                   // Para std::vector
#include <string>                   // Para std::string
#include <memory>                   // Para std::unique_ptr

// Forward declaration para resolver dependencia circular con CatalogManager
class CatalogManager; 

// --- Cabecera General del Bloque (Common to ALL Block Types) ---
// Esta estructura se superpondrá al inicio de CADA bloque de datos.
// Contiene metadatos fundamentales para cualquier tipo de página.
struct BlockHeader {
    PageId page_id;             // ID lógico de esta página.
    PageType page_type;         // Tipo de página (DATA_PAGE, CATALOG_PAGE, etc.)
    uint32_t data_end_offset;   // Offset desde el inicio del bloque donde el último registro termina (crece hacia el inicio del bloque)
    uint32_t num_slots;         // Número actual de slots en el directorio de slots
    uint32_t header_and_slot_directory_size; // Tamaño total de la cabecera fija + el directorio de slots (crece hacia el final del bloque)
    // Podríamos añadir un checksum, LSN, etc. aquí para una cabecera más robusta.
};

// --- Entrada del Directorio de Slots (Para DATA_PAGEs) ---
// Cada entrada apunta a un registro dentro del bloque.
struct SlotDirectoryEntry {
    uint32_t offset;        // Offset del registro dentro del bloque (desde el inicio del bloque)
    uint32_t length;        // Longitud del registro en bytes
    bool is_occupied;       // True si el slot está ocupado, false si está libre
};

// Estructura para representar un registro de datos.
// Contiene los datos brutos del registro.
struct Record {
    std::vector<Byte> data; // Contenido del registro
};


// Clase RecordManager: Gestiona los registros dentro de las páginas de datos.
// Responsabilidades:
// - Inicializar páginas de datos.
// - Insertar, eliminar, actualizar y obtener registros.
// - Gestionar el directorio de slots dentro de una página.
class RecordManager {
public:
    // Constructor del RecordManager.
    // buffer_manager: Referencia al BufferManager para interactuar con las páginas.
    // catalog_manager: Referencia al CatalogManager para actualizar metadatos de tablas.
    // NOTA: Se ha simplificado el constructor para romper la dependencia circular.
    RecordManager(BufferManager& buffer_manager);

    // Método setter para el CatalogManager (para resolver la dependencia circular)
    void SetCatalogManager(CatalogManager& catalog_manager);

    // Inicializa una nueva página de datos con la cabecera adecuada.
    // page_id: El ID de la página a inicializar.
    Status InitDataPage(PageId page_id);

    // Inserta un registro en una página de datos.
    // page_id: El ID de la página donde insertar.
    // record: El registro a insertar.
    // slot_id: El ID del slot asignado al nuevo registro (salida).
    Status InsertRecord(PageId page_id, const Record& record, uint32_t& slot_id);

    // Obtiene un registro de una página de datos.
    // page_id: El ID de la página de donde obtener el registro.
    // slot_id: El ID del slot del registro.
    // record: El registro obtenido (salida).
    Status GetRecord(PageId page_id, uint32_t slot_id, Record& record);

    // Actualiza un registro existente en una página de datos.
    // page_id: El ID de la página donde actualizar.
    // slot_id: El ID del slot del registro a actualizar.
    // new_record: El nuevo contenido del registro.
    Status UpdateRecord(PageId page_id, uint32_t slot_id, const Record& new_record);

    // Elimina un registro de una página de datos.
    // page_id: El ID de la página de donde eliminar el registro.
    // slot_id: El ID del slot del registro a eliminar.
    Status DeleteRecord(PageId page_id, uint32_t slot_id);

    // Obtiene el número de registros en una página.
    Status GetNumRecords(PageId page_id, uint32_t& num_records);

    // Obtiene el espacio libre restante en una página.
    Status GetFreeSpace(PageId page_id, BlockSizeType& free_space);

    // Calcula el offset donde comienza el directorio de slots.
    BlockSizeType GetSlotDirectoryStartOffset() const; 

    // Métodos auxiliares para leer/escribir la cabecera general del bloque
    // page_data: Puntero a los datos del bloque en memoria.
    BlockHeader ReadBlockHeader(Byte* page_data) const;
    void WriteBlockHeader(Byte* page_data, const BlockHeader& header) const;

    // Métodos auxiliares para manipular el directorio de slots
    // page_data: Puntero a los datos del bloque en memoria.
    // slot_id: El ID del slot a manipular.
    SlotDirectoryEntry ReadSlotEntry(Byte* page_data, uint32_t slot_id) const;
    void WriteSlotEntry(Byte* page_data, uint32_t slot_id, const SlotDirectoryEntry& entry) const;

private:
    BufferManager& buffer_manager_; // Referencia al gestor de búfer
    CatalogManager* catalog_manager_; // Puntero al gestor de catálogo (para romper la dependencia circular)

    // Tamaño de la cabecera general fija (sin contar el slot directory variable).
    BlockSizeType fixed_header_base_size_;
};

#endif // RECORD_MANAGER_H
