// include/common.h
#ifndef COMMON_H
#define COMMON_H

#include <cstdint> // Para tipos enteros fijos como uint32_t
#include <string>  // Para std::string
#include <vector>  // Para std::vector
#include <stdexcept> // Para std::invalid_argument
#include <algorithm> // Para std::fill

// Definiciones de tipos comunes
using PageId = uint32_t;     // Identificador único de una página lógica (bloque)
using FrameId = uint32_t;    // Identificador único de un frame en el buffer pool
using BlockId = uint32_t;    // Identificador único de un bloque (añadido)
using BlockSizeType = uint32_t; // Tamaño de un bloque en bytes
using SectorSizeType = uint32_t; // Tamaño de un sector en bytes
using Byte = char;           // Representación de un byte de datos

// Enumeración para el estado de las operaciones
enum class Status : uint8_t {
    OK = 0,
    ERROR,
    NOT_FOUND,
    INVALID_PARAMETER,
    IO_ERROR,
    DISK_FULL,
    BUFFER_FULL,
    PAGE_PINNED,
    INVALID_BLOCK_ID,
    INVALID_PAGE_TYPE,
    OUT_OF_MEMORY,
    DUPLICATE_ENTRY // Para CatalogManager
};

// Función auxiliar para convertir Status a string (para mensajes de error)
inline std::string StatusToString(Status status) {
    switch (status) {
        case Status::OK: return "OK";
        case Status::ERROR: return "ERROR";
        case Status::NOT_FOUND: return "NOT_FOUND";
        case Status::INVALID_PARAMETER: return "INVALID_PARAMETER";
        case Status::IO_ERROR: return "IO_ERROR";
        case Status::DISK_FULL: return "DISK_FULL";
        case Status::BUFFER_FULL: return "BUFFER_FULL";
        case Status::PAGE_PINNED: return "PAGE_PINNED";
        case Status::INVALID_BLOCK_ID: return "INVALID_BLOCK_ID";
        case Status::INVALID_PAGE_TYPE: return "INVALID_PAGE_TYPE";
        case Status::OUT_OF_MEMORY: return "OUT_OF_MEMORY";
        case Status::DUPLICATE_ENTRY: return "DUPLICATE_ENTRY";
        default: return "UNKNOWN_STATUS";
    }
}

// Enumeración para los tipos de página
enum class PageType : uint8_t {
    INVALID_PAGE = 0,
    DISK_METADATA_PAGE, // Página para metadatos del disco (ej. mapa de bits de sectores)
    CATALOG_PAGE,       // Página para metadatos del catálogo (esquemas de tablas)
    DATA_PAGE,          // Página que contiene registros de datos de una tabla
    INDEX_PAGE          // Página que contiene entradas de un índice
};

// Función auxiliar para convertir PageType a string
inline std::string PageTypeToString(PageType type) {
    switch (type) {
        case PageType::INVALID_PAGE: return "INVALID_PAGE";
        case PageType::DISK_METADATA_PAGE: return "DISK_METADATA_PAGE";
        case PageType::CATALOG_PAGE: return "CATALOG_PAGE";
        case PageType::DATA_PAGE: return "DATA_PAGE";
        case PageType::INDEX_PAGE: return "INDEX_PAGE";
        default: return "UNKNOWN_PAGE_TYPE";
    }
}

// Enumeración para el estado de un bloque físico en el disco
enum class BlockStatus : uint8_t {
    EMPTY = 0,      // Bloque completamente libre
    INCOMPLETE,     // Bloque parcialmente ocupado (tiene espacio libre)
    FULL            // Bloque completamente ocupado
};

// Función auxiliar para convertir BlockStatus a string
inline std::string BlockStatusToString(BlockStatus status) {
    switch (status) {
        case BlockStatus::EMPTY: return "EMPTY";
        case BlockStatus::INCOMPLETE: return "INCOMPLETE";
        case BlockStatus::FULL: return "FULL";
        default: return "UNKNOWN_BLOCK_STATUS";
    }
}

// Enumeración para los tipos de columnas (datos) - MOVIDO AQUÍ
enum class ColumnType : uint8_t {
    INT = 0,
    CHAR,
    VARCHAR,
    // Otros tipos de datos pueden ser añadidos aquí (FLOAT, DATE, etc.)
};

// Función auxiliar para convertir ColumnType a string - MOVIDO AQUÍ
inline std::string ColumnTypeToString(ColumnType type) {
    switch (type) {
        case ColumnType::INT: return "INT";
        case ColumnType::CHAR: return "CHAR";
        case ColumnType::VARCHAR: return "VARCHAR";
        default: return "UNKNOWN_COLUMN_TYPE";
    }
}

// NUEVA ESTRUCTURA: Metadata de una columna
// Esta será parte del esquema de la tabla.
struct ColumnMetadata {
    char name[64];      // Nombre de la columna (fijo para serialización)
    ColumnType type;    // Tipo de dato de la columna
    uint32_t size;      // Tamaño para CHAR (longitud fija), para INT (sizeof(int)),
                        // para VARCHAR (tamaño máximo permitido).

    ColumnMetadata() : type(ColumnType::INT), size(0) {
        std::fill(name, name + 64, 0); // Inicializar con ceros
    }
};

// NUEVA ESTRUCTURA: Metadata de una tabla
// Esta será el "registro" que el CatalogManager guardará en CATALOG_PAGEs.
struct TableMetadata {
    uint32_t table_id;              // Identificador único de la tabla
    char table_name[64];            // Nombre de la tabla (fijo para serialización)
    bool is_fixed_length_record;    // true si todos los registros de esta tabla son de longitud fija
    std::vector<PageId> data_page_ids; // Lista de PageIds que pertenecen a esta tabla
    uint32_t num_records;           // Número total de registros en la tabla (puede ser aproximado)
                                    // Este campo se actualizará con Insert/DeleteRecord

    // Para registros de longitud fija:
    uint32_t fixed_record_size;     // Tamaño total de un registro fijo (incluyendo delimitadores si los hubiera)

    TableMetadata() : table_id(0), is_fixed_length_record(true),
                      num_records(0), fixed_record_size(0) {
        std::fill(table_name, table_name + 64, 0); // Inicializar con ceros
    }
};

#endif // COMMON_H
