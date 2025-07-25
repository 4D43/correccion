// data_storage/page.h
#ifndef PAGE_H
#define PAGE_H

#include "../include/common.h" // Para PageId, FrameId

// Estructura que representa los metadatos de una página (o frame) en el Buffer Pool.
// Esta estructura NO contiene los datos reales del bloque; solo su información de gestión.
// Los datos reales del bloque se almacenarán en un vector de bytes en el BufferManager.
struct Page {
    PageId page_id;     // El ID del bloque de disco al que corresponde esta página en memoria.
                        // PageId es un alias para BlockId.
    int pin_count;      // Número de veces que esta página está "anclada" (pinned).
                        // Una página anclada no puede ser desalojada del buffer.
    bool is_dirty;      // Indica si la página ha sido modificada en memoria y necesita
                        // ser escrita de vuelta al disco antes de ser desalojada.
    bool is_valid;      // Indica si este frame del buffer contiene datos válidos de un bloque.
                        // Útil para frames que aún no han sido usados o que han sido invalidados.

    // Constructor por defecto. Inicializa la página a un estado "vacío" o no válido.
    Page() : page_id(0), pin_count(0), is_dirty(false), is_valid(false) {}

    // Constructor con parámetros.
    Page(PageId id, int pin = 0, bool dirty = false, bool valid = true)
        : page_id(id), pin_count(pin), is_dirty(dirty), is_valid(valid) {}

    // Reinicia los metadatos de la página a su estado por defecto (no válida, no anclada, no sucia).
    void Reset() {
        page_id = 0;
        pin_count = 0;
        is_dirty = false;
        is_valid = false;
    }
};

#endif // PAGE_H
