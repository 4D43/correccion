// record_manager/record_manager.h
#ifndef RECORD_MANAGER_H
#define RECORD_MANAGER_H

#include "../include/common.h"     // Para Status, Byte, BlockSizeType, PageId, PageType
#include "../data_storage/buffer_manager.h" // Para BufferManager (dependencia)
#include <vector>                   // Para std::vector
#include <string>                   // Para std::string
#include <memory>                   // Para std::unique_ptr

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
    uint32_t offset; // Offset del registro desde el inicio del bloque
    uint32_t length; // Longitud del registro en bytes
    bool is_occupied; // Indica si este slot está ocupado por un registro

    SlotDirectoryEntry() : offset(0), length(0), is_occupied(false) {}
};

// --- Estructura para representar un Registro ---
// Esto es una abstracción para el usuario del RecordManager.
// Un registro es simplemente un vector de bytes.
struct Record {
    std::vector<Byte> data;
};

// Clase RecordManager: Gestiona la organización y manipulación de registros dentro de los bloques.
// Esta versión se centrará en DATA_PAGEs usando un Slot Directory.
class RecordManager {
public:
    // Constructor del RecordManager.
    // buffer_manager: Una referencia al BufferManager para interactuar con los bloques.
    RecordManager(BufferManager& buffer_manager);

    // Inicializa una nueva página de datos (DATA_PAGE).
    // page_id: El ID de la página a inicializar (debe haber sido obtenida con NewPage de BufferManager).
    // Retorna Status::OK si la inicialización es exitosa.
    Status InitDataPage(PageId page_id);

    // Inserta un nuevo registro en una página de datos.
    // page_id: El ID de la página donde se intentará insertar el registro.
    // record_data: Los datos del registro a insertar.
    // slot_id: El ID del slot donde se insertó el registro (salida).
    // Retorna Status::OK si la inserción es exitosa, Status::BUFFER_FULL (si el bloque está lleno)
    // o Status::ERROR en caso de otros fallos.
    Status InsertRecord(PageId page_id, const Record& record_data, uint32_t& slot_id);

    // Obtiene un registro de una página de datos.
    // page_id: El ID de la página.
    // slot_id: El ID del slot del registro a obtener.
    // record_data: El objeto Record donde se copiarán los datos (salida).
    // Retorna Status::OK si la obtención es exitosa, Status::NOT_FOUND si el slot está vacío,
    // o Status::ERROR en caso de otros fallos.
    Status GetRecord(PageId page_id, uint32_t slot_id, Record& record_data);

    // Actualiza un registro existente en una página de datos.
    // page_id: El ID de la página.
    // slot_id: El ID del slot del registro a actualizar.
    // new_record_data: Los nuevos datos del registro.
    // Retorna Status::OK si la actualización es exitosa, Status::NOT_FOUND si el slot está vacío,
    // o Status::ERROR en caso de otros fallos.
    Status UpdateRecord(PageId page_id, uint32_t slot_id, const Record& new_record_data);

    // Elimina un registro de una página de datos.
    // page_id: El ID de la página.
    // slot_id: El ID del slot del registro a eliminar.
    // Retorna Status::OK si la eliminación es exitosa, Status::NOT_FOUND si el slot está vacío,
    // o Status::ERROR en caso de otros fallos.
    Status DeleteRecord(PageId page_id, uint32_t slot_id);

    // Obtiene el número de registros en una página.
    Status GetNumRecords(PageId page_id, uint32_t& num_records);

    // Obtiene el espacio libre restante en una página.
    Status GetFreeSpace(PageId page_id, BlockSizeType& free_space);

    BlockSizeType GetSlotDirectoryStartOffset() const; // Ahora es fijo después de la cabecera base

    // Métodos auxiliares para leer/escribir la cabecera general del bloque
    // page_data: Puntero a los datos del bloque en memoria.
    BlockHeader ReadBlockHeader(Byte* page_data) const;

    void WriteBlockHeader(Byte* page_data, const BlockHeader& header) const;


private:
    BufferManager& buffer_manager_; // Referencia al gestor de búfer

    // Tamaño de la cabecera general fija (sin contar el slot directory variable).
    BlockSizeType fixed_header_base_size_;
    

    // Métodos auxiliares para manipular el directorio de slots
    // page_data: Puntero a los datos del bloque en memoria.
    // slot_id: El ID del slot a manipular.
    SlotDirectoryEntry ReadSlotEntry(Byte* page_data, uint32_t slot_id) const;
    void WriteSlotEntry(Byte* page_data, uint32_t slot_id, const SlotDirectoryEntry& entry) const;

    // Calcula el offset donde comienza el directorio de slots.
    // El directorio de slots crece desde el final de la cabecera fija hacia el final del bloque.
    // Calcula el espacio libre real en una página.
    BlockSizeType CalculateFreeSpace(Byte* page_data) const;
};

#endif // RECORD_MANAGER_H
