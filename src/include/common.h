#ifndef COMMON_H
#define COMMON_H

#include <cstdint>   // Para uint32_t, etc.
#include <string>    // Para std::string
#include <vector>    // Para std::vector
#include <stdexcept> // Para excepciones estándar
#include <algorithm> // Para algoritmos como std::min, std::max
#include <cstring>   // Para funciones de manipulación de cadenas de C (memset, memcpy)

// ==== ALIAS DE TIPOS COMUNES ====
using PageId = uint32_t;        // Identificador de una página (copia de bloque en buffer)
using FrameId = uint32_t;       // Identificador de un frame en el buffer pool
using BlockId = uint32_t;       // Identificador de un bloque en el disco
using BlockSizeType = uint32_t; // Tamaño de un bloque en bytes
using SectorSizeType = uint32_t; // Tamaño de un sector en bytes
using Byte = char;              // Tipo para representar un byte de datos
using RecordId = uint32_t;      // Identificador de un registro dentro de una tabla

// ==== CONSTANTES DEL SISTEMA ====
const uint32_t BLOCK_SIZE = 4096;          // Tamaño estándar de un bloque en bytes
const FrameId INVALID_FRAME_ID = UINT32_MAX; // Valor inválido para un ID de frame
const PageId INVALID_PAGE_ID = UINT32_MAX;   // Valor inválido para un ID de página/bloque

// ==== ESTADO GENERAL DE LAS OPERACIONES ====
enum class Status : uint8_t {
    OK = 0,
    SUCCESS,
    ERROR,
    NOT_FOUND,
    DUPLICATE_ENTRY,
    DUPLICATE_KEY,
    IO_ERROR,
    DISK_FULL,
    BUFFER_FULL,
    BUFFER_OVERFLOW,
    PAGE_PINNED,
    INVALID_BLOCK_ID,
    INVALID_PAGE_TYPE,
    INVALID_ARGUMENT,
    ALREADY_EXISTS,
    CANCELLED,
    RESOURCE_BUSY,
    OPERATION_FAILED,
    OUT_OF_MEMORY,
    OUT_OF_SPACE_FOR_UPDATE,
    INVALID_PARAMETER,
    INVALID_FORMAT // Añadido para errores de formato en serialización/deserialización
};

// Función de utilidad para convertir Status a string (para mensajes de error)
inline std::string StatusToString(Status status) {
    switch (status) {
        case Status::OK: return "OK";
        case Status::SUCCESS: return "SUCCESS";
        case Status::ERROR: return "ERROR";
        case Status::NOT_FOUND: return "NOT_FOUND";
        case Status::DUPLICATE_ENTRY: return "DUPLICATE_ENTRY";
        case Status::DUPLICATE_KEY: return "DUPLICATE_KEY";
        case Status::IO_ERROR: return "IO_ERROR";
        case Status::DISK_FULL: return "DISK_FULL";
        case Status::BUFFER_FULL: return "BUFFER_FULL";
        case Status::BUFFER_OVERFLOW: return "BUFFER_OVERFLOW";
        case Status::PAGE_PINNED: return "PAGE_PINNED";
        case Status::INVALID_BLOCK_ID: return "INVALID_BLOCK_ID";
        case Status::INVALID_PAGE_TYPE: return "INVALID_PAGE_TYPE";
        case Status::INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case Status::ALREADY_EXISTS: return "ALREADY_EXISTS";
        case Status::CANCELLED: return "CANCELLED";
        case Status::RESOURCE_BUSY: return "RESOURCE_BUSY";
        case Status::OPERATION_FAILED: return "OPERATION_FAILED";
        case Status::OUT_OF_MEMORY: return "OUT_OF_MEMORY";
        case Status::OUT_OF_SPACE_FOR_UPDATE: return "OUT_OF_SPACE_FOR_UPDATE";
        case Status::INVALID_PARAMETER: return "INVALID_PARAMETER";
        case Status::INVALID_FORMAT: return "INVALID_FORMAT";
        default: return "UNKNOWN_STATUS";
    }
}

// ==== TIPOS DE COLUMNAS (UNIFICADO) ====
enum class ColumnType : uint8_t {
    INT = 0,     // Enteros
    VARCHAR,     // Cadenas de longitud variable (STRING en tu original)
    CHAR,        // Cadenas de longitud fija
    REAL,        // Números de punto flotante
    BOOL         // Valores booleanos
};

// Función de utilidad para convertir ColumnType a string
inline std::string ColumnTypeToString(ColumnType type) {
    switch (type) {
        case ColumnType::INT: return "INT";
        case ColumnType::VARCHAR: return "VARCHAR";
        case ColumnType::CHAR: return "CHAR";
        case ColumnType::REAL: return "REAL";
        case ColumnType::BOOL: return "BOOL";
        default: return "UNKNOWN_COLUMN_TYPE";
    }
}

// Función de utilidad para convertir string a ColumnType
inline ColumnType StringToColumnType(const std::string& type_str) {
    std::string upper_type_str = type_str;
    std::transform(upper_type_str.begin(), upper_type_str.end(), upper_type_str.begin(), ::toupper);
    if (upper_type_str == "INT") return ColumnType::INT;
    if (upper_type_str == "VARCHAR") return ColumnType::VARCHAR;
    if (upper_type_str == "CHAR") return ColumnType::CHAR;
    if (upper_type_str == "REAL") return ColumnType::REAL;
    if (upper_type_str == "BOOL") return ColumnType::BOOL;
    return ColumnType::INT; // Valor por defecto o error
}


// ==== METADATA DE COLUMNA (Usando std::string para nombres) ====
struct ColumnMetadata {
    std::string name;       // Nombre de la columna
    ColumnType type;        // Tipo de dato de la columna
    uint32_t size;          // Tamaño para CHAR/VARCHAR, 0 para INT/REAL/BOOL
    bool is_primary_key;    // Indica si es parte de la clave primaria
    bool is_nullable;       // Indica si la columna puede contener valores nulos

    ColumnMetadata() : name(""), type(ColumnType::INT), size(0), is_primary_key(false), is_nullable(true) {}
    ColumnMetadata(const std::string& n, ColumnType t, uint32_t s = 0, bool pk = false, bool nl = true)
        : name(n), type(t), size(s), is_primary_key(pk), is_nullable(nl) {}
};

#endif // COMMON_H
