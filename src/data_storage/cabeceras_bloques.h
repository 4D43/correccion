// data_storage/cabeceras_bloques.h
#ifndef CABECERAS_BLOQUES_H
#define CABECERAS_BLOQUES_H

#include "../include/common.h" // Para BlockId, BlockSizeType, PageType, Status

// ===== CONSTANTES GLOBALES DE CABECERAS =====
constexpr uint32_t MAGIC_NUMBER_SGBD = 0x42534442; // "BSDB" en ASCII (Base de Datos Simple)
constexpr uint32_t VERSION_CABECERAS = 1;          // Versión del formato de cabeceras

// ===== ENUMERACIONES ESPECÍFICAS DE ALMACENAMIENTO =====

/**
 * @enum PageType
 * @brief Tipos de páginas/bloques que pueden existir en el disco.
 * Ya definido en common.h, se incluye aquí para claridad contextual.
 */
// enum class PageType : uint8_t { ... }; // Definido en common.h

/**
 * @enum BlockStatus
 * @brief Estado de un bloque, ya sea en disco o en el buffer.
 */
enum class BlockStatus : uint8_t {
    FREE = 0,   // Bloque libre, no contiene datos válidos
    USED,       // Bloque en uso, contiene datos válidos
    DIRTY,      // Bloque en uso y modificado en memoria (necesita escribirse a disco)
    PINNED,     // Bloque anclado en memoria (no puede ser desalojado)
    CORRUPTED   // Bloque con datos corruptos (ej. checksum inválido)
};

// Función de utilidad para convertir BlockStatus a string
inline std::string BlockStatusToString(BlockStatus status) {
    switch (status) {
        case BlockStatus::FREE: return "FREE";
        case BlockStatus::USED: return "USED";
        case BlockStatus::DIRTY: return "DIRTY";
        case BlockStatus::PINNED: return "PINNED";
        case BlockStatus::CORRUPTED: return "CORRUPTED";
        default: return "UNKNOWN_STATUS";
    }
}

// ==== ESTRUCTURAS DE CABECERAS DE BLOQUES ====

/**
 * @struct CabeceraComun
 * @brief Cabecera base para todos los bloques del sistema.
 * Contiene información esencial para la gestión del bloque.
 * Esta cabecera se almacenará al inicio de cada bloque en disco.
 */
struct CabeceraComun {
    uint32_t magic_number;          // Número mágico para identificar el archivo del SGBD
    uint32_t version;               // Versión del formato de cabeceras
    BlockId id_bloque;              // ID único de este bloque
    BlockSizeType tamano_bloque_total; // Tamaño total del bloque en bytes
    PageType tipo_pagina;           // Tipo de datos que contiene el bloque (DATA, CATALOG, INDEX, FREE)
    uint32_t bytes_usados;          // Cantidad de bytes usados dentro del bloque
    uint32_t bytes_disponibles;     // Cantidad de bytes disponibles dentro del bloque
    uint64_t timestamp_ultima_modificacion; // Timestamp de la última modificación (epoch ms)
    uint32_t checksum;              // Suma de verificación para integridad de datos

    CabeceraComun()
        : magic_number(MAGIC_NUMBER_SGBD), version(VERSION_CABECERAS), id_bloque(INVALID_PAGE_ID),
          tamano_bloque_total(BLOCK_SIZE), tipo_pagina(PageType::DATA_PAGE), bytes_usados(0),
          bytes_disponibles(BLOCK_SIZE - sizeof(CabeceraComun)), timestamp_ultima_modificacion(0), checksum(0) {}
};

// Nota: Las cabeceras específicas para datos, catálogo, índices, etc., se definirán
// en 'cabeceras_especificas.h' si son muy grandes o complejas, o se manejarán
// directamente como parte del contenido del bloque si son simples.
// Para este enfoque de texto plano, la CabeceraComun es lo más relevante a nivel de bloque físico.

#endif // CABECERAS_BLOQUES_H
