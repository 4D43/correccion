// data_storage/gestor_buffer.h
#ifndef GESTOR_BUFFER_H
#define GESTOR_BUFFER_H

#include "../include/common.h"           // Para BlockId, FrameId, Status, Byte, BlockSizeType, PageType
#include "gestor_disco.h"                // Para GestorDisco (dependencia)
#include "bloque.h"                      // Para la estructura Bloque
#include "pagina.h"                      // Para la estructura Pagina (metadatos del frame)
#include "../replacement_policies/ipolitica_reemplazo.h" // Para la interfaz de política de reemplazo
#include "../replacement_policies/lru_espanol.h"         // Para PoliticaLRU
#include "../replacement_policies/clock_espanol.h"       // Para PoliticaClock

#include <vector>                        // Para std::vector
#include <unordered_map>                 // Para mapear PageId a FrameId
#include <memory>                        // Para std::unique_ptr y std::shared_ptr
#include <optional>                      // Para std::optional
#include <chrono>                        // Para timestamps
#include <mutex>                         // Para std::mutex

// Aliases para compatibilidad con main.cpp - MOVED AFTER INCLUDES
using IReplacementPolicy = IPoliticaReemplazo;
using LRUReplacementPolicy = PoliticaLRU;
using ClockReplacementPolicy = PoliticaClock;

/**
 * @brief Gestor del Buffer Pool - Maneja páginas (copias de bloques) en memoria
 *
 * CONCEPTO CLAVE:
 * - El buffer pool trabaja con PÁGINAS que son copias de los bloques del disco
 * - Las páginas son copias del arreglo estático del bloque en memoria
 * - Cada FRAME en el buffer pool contiene una PÁGINA
 * - El GestorBuffer es responsable de:
 * - Asignar y liberar frames en el buffer pool
 * - Cargar páginas desde disco (cache miss)
 * - Escribir páginas modificadas a disco (desalojo)
 * - Implementar una política de reemplazo (LRU, CLOCK, etc.)
 * - Gestionar el "pinning" de páginas para evitar su desalojo
 * - Mantener metadatos de las páginas (dirty flag, pin count, etc.)
 * - Proporcionar acceso a los datos de las páginas
 *
 * ESTRUCTURA INTERNA:
 * - `pool_datos_buffer_`: Vector de vectores de bytes (o char arrays) que representan los frames físicos.
 * - `frames_`: Vector de estructuras `Pagina` que contienen los metadatos de cada frame.
 * - `tabla_paginas_`: Un `unordered_map` que mapea `PageId` (ID de bloque en disco) a `FrameId` (ID de frame en buffer).
 * - `politica_reemplazo_`: Puntero a la política de reemplazo activa.
 * - `gestor_disco_`: Puntero al gestor de disco para operaciones de E/S.
 */
class GestorBuffer {
public:
    /**
     * @brief Estadísticas del GestorBuffer
     */
    struct BufferStats {
        uint64_t hits_cache;        // Número de aciertos en caché
        uint64_t misses_cache;      // Número de fallos en caché
        uint64_t lecturas_disco;    // Número de lecturas desde disco
        uint64_t escrituras_disco;  // Número de escrituras a disco
        uint64_t desalojos;         // Número de desalojos de páginas
        uint64_t confirmaciones;    // Número de confirmaciones de transacciones
        uint64_t reversiones;       // Número de reversiones de transacciones
        uint64_t paginas_ancladas;  // Número de páginas actualmente ancladas
        uint64_t paginas_sucias;    // Número de páginas actualmente sucias
        uint64_t tiempo_total_espera_pin; // Tiempo total de espera para anclar páginas
        uint64_t tiempo_total_io;   // Tiempo total gastado en operaciones de E/S
        uint64_t tiempo_total_procesamiento_politica; // Tiempo en política de reemplazo
        uint64_t ult_timestamp_reset; // Último timestamp de reseteo de estadísticas

        BufferStats()
            : hits_cache(0), misses_cache(0), lecturas_disco(0), escrituras_disco(0),
              desalojos(0), confirmaciones(0), reversiones(0), paginas_ancladas(0),
              paginas_sucias(0), tiempo_total_espera_pin(0), tiempo_total_io(0),
              tiempo_total_procesamiento_politica(0), ult_timestamp_reset(0) {}
    };

    /**
     * @brief Constructor del GestorBuffer
     * @param gestor_disco Puntero compartido al GestorDisco
     * @param tamano_pool Tamaño del pool de buffer en número de frames
     * @param tamano_bloque Tamaño de cada bloque/página en bytes
     * @param politica_reemplazo Política de reemplazo a utilizar (LRU, CLOCK, etc.)
     */
    GestorBuffer(std::shared_ptr<GestorDisco> gestor_disco,
                 uint32_t tamano_pool,
                 BlockSizeType tamano_bloque,
                 std::unique_ptr<IReplacementPolicy> politica_reemplazo);

    /**
     * @brief Destructor del GestorBuffer
     * Asegura que todas las páginas sucias sean escritas a disco.
     */
    ~GestorBuffer();

    // === MÉTODOS PÚBLICOS DE GESTIÓN DE PÁGINAS ===

    /**
     * @brief Ancla una página en el buffer pool.
     * Si la página no está en el buffer, la carga desde disco.
     * Incrementa el contador de anclajes de la página.
     * @param id_bloque ID del bloque a anclar
     * @param pagina_info Referencia a la estructura Pagina donde se copiará la información del frame
     * @return Status de la operación
     */
    Status PinPage(BlockId id_bloque, Pagina& pagina_info);

    /**
     * @brief Desancla una página del buffer pool.
     * Decrementa el contador de anclajes de la página.
     * @param id_bloque ID del bloque a desanclar
     * @param is_dirty Indica si la página fue modificada (true) o no (false)
     * @return Status de la operación
     */
    Status UnpinPage(BlockId id_bloque, bool is_dirty);

    /**
     * @brief Obtiene un puntero a los datos de una página anclada.
     * @param id_bloque ID del bloque
     * @return Puntero a los datos de la página, o nullptr si la página no está anclada o no existe.
     */
    Byte* GetPageData(BlockId id_bloque);

    /**
     * @brief Crea una nueva página (bloque) en disco y la ancla en el buffer.
     * @param id_bloque ID del nuevo bloque (salida)
     * @param pagina_info Referencia a la estructura Pagina donde se copiará la información del frame
     * @return Status de la operación
     */
    Status NewPage(BlockId& id_bloque, Pagina& pagina_info);

    /**
     * @brief Elimina una página del buffer pool y del disco.
     * @param id_bloque ID del bloque a eliminar
     * @return Status de la operación
     */
    Status DeletePage(BlockId id_bloque);

    /**
     * @brief Fuerza la escritura de una página específica a disco, si está sucia.
     * @param id_bloque ID del bloque a escribir
     * @return Status de la operación
     */
    Status FlushPage(BlockId id_bloque);

    /**
     * @brief Fuerza la escritura de todas las páginas sucias a disco.
     * @return Status de la operación
     */
    Status FlushAllPages();

    /**
     * @brief Obtiene el número de frames disponibles en el buffer pool.
     * @return Número de frames libres.
     */
    uint32_t GetNumFreeFrames() const;

    /**
     * @brief Obtiene el número total de frames en el buffer pool.
     * @return Tamaño total del pool.
     */
    uint32_t GetPoolSize() const;

    /**
     * @brief Reinicia las estadísticas del buffer.
     */
    void ResetStats();

    /**
     * @brief Imprime las estadísticas actuales del buffer.
     */
    void PrintStats() const;

    /**
     * @brief Obtiene las estadísticas actuales del buffer.
     * @return Estructura BufferStats con las estadísticas.
     */
    BufferStats GetStats() const;

    /**
     * @brief Valida la consistencia interna del gestor de buffer.
     * @return Status::OK si es consistente, error en caso contrario.
     */
    Status ValidarConsistencia() const;

private:
    // === MIEMBROS PRIVADOS ===
    std::shared_ptr<GestorDisco> gestor_disco_;              // Gestor de disco
    uint32_t tamaño_pool_;                                   // Tamaño del pool de buffer (número de frames)
    BlockSizeType tamaño_bloque_;                            // Tamaño de cada bloque/página
    std::unique_ptr<IReplacementPolicy> politica_reemplazo_; // Política de reemplazo
    std::mutex mutex_buffer_;                                // Mutex para proteger el acceso concurrente al buffer

    std::vector<std::vector<Byte>> pool_datos_buffer_;       // Pool de datos físicos (frames)
    std::vector<Pagina> frames_;                             // Metadatos de cada frame en el pool
    std::unordered_map<PageId, FrameId> tabla_paginas_;      // Mapeo de PageId a FrameId

    BufferStats estadisticas_;                               // Estadísticas del buffer

    // === MÉTODOS AUXILIARES PRIVADOS ===

    /**
     * @brief Encuentra un frame libre en el buffer pool
     * @return FrameId del frame libre, o -1 si no hay frames libres
     */
    FrameId EncontrarFrameLibre();

    /**
     * @brief Desaloja una página del buffer pool usando la política de reemplazo
     * @return Status de la operación
     */
    Status DesalojarPagina();

    /**
     * @brief Escribe una página específica al disco
     * @param id_bloque ID del bloque a escribir
     * @return Status de la operación
     */
    Status EscribirPaginaADisco(BlockId id_bloque);

    /**
     * @brief Lee una página del disco y la carga en un frame específico
     * @param id_bloque ID del bloque a leer
     * @param id_frame ID del frame donde cargar la página
     * @return Status de la operación
     */
    Status LeerPaginaDesdeDisco(BlockId id_bloque, FrameId id_frame);

    /**
     * @brief Valida que un frame ID sea válido
     * @param id_frame ID del frame a validar
     * @return true si el frame ID es válido
     */
    bool ValidarFrameId(FrameId id_frame) const;

    /**
     * @brief Actualiza las estadísticas del buffer
     * @param tipo_operacion Tipo de operación realizada
     */
    void ActualizarEstadisticas(const std::string& tipo_operacion);

    /**
     * @brief Obtiene timestamp actual para logging
     * @return String con el timestamp
     */
    std::string ObtenerTimestampActualString() const;
};

#endif // GESTOR_BUFFER_H
