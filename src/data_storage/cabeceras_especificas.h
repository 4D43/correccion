// data_storage/cabeceras_especificas.h
#ifndef CABECERAS_ESPECIFICAS_H
#define CABECERAS_ESPECIFICAS_H

#include "../include/common.h"
#include "bloque.h"  // Incluir bloque.h para usar las definiciones de structs
#include <vector>
#include <string>
#include <chrono>
#include <cstdint>

/**
 * @file cabeceras_especificas.h
 * @brief Cabeceras específicas para diferentes tipos de bloques - DISEÑO CRÍTICO
 *
 * ATENCIÓN: Este archivo define la estructura interna de cada tipo de bloque.
 * Cada cabecera está optimizada para su propósito específico y debe ser
 * tratada con especial cuidado para mantener la integridad de los datos.
 */

// ===== CONSTANTES CRÍTICAS =====

constexpr uint32_t MAGIC_NUMBER_SGBD = 0x42534442; // "BSDB" en ASCII
constexpr uint32_t VERSION_CABECERAS = 1;
constexpr uint32_t MAX_MAPAS_TABLA = 16;
constexpr uint32_t MAX_ENTRADAS_INDICE = 256;

// ===== ENUMERACIONES ESPECÍFICAS =====
// (BlockStatus y TipoIndice ya están definidos en cabeceras_bloques.h)

// NOTA: Las siguientes definiciones de structs (CabeceraComun, CabeceraBloqueDatos, etc.)
// están comentadas porque ya están definidas en cabeceras_bloques.h para evitar
// errores de definición múltiple durante la compilación.
// Si se necesitan, deben ser incluidas desde cabeceras_bloques.h.

/*
struct CabeceraComun {
    uint32_t magic_number;
    uint32_t version_formato;
    PageType tipo_pagina;
    BlockId id_bloque;
    BlockSizeType tamano_bloque_total;
    uint32_t bytes_disponibles;
    uint32_t bytes_usados;
    uint32_t checksum_cabecera;
    uint32_t checksum_datos;
    BlockStatus estado;
    uint64_t timestamp_creacion;
    uint64_t timestamp_modificacion;
    BlockId bloque_anterior;
    BlockId bloque_siguiente;
};

struct CabeceraBloqueDatos {
    uint32_t numero_mapas_tabla;
    MapaTabla mapas_tabla[MAX_MAPAS_TABLA];
    uint32_t espacio_libre_total;
    uint32_t fragmentacion_interna;
    uint32_t offset_espacio_libre;
    uint32_t numero_inserciones;
    uint32_t numero_registros_activos;
    uint32_t numero_registros_eliminados;
    uint32_t offset_directorio_slots;
    uint32_t tamaño_directorio_slots;
    uint32_t factor_carga_porcentaje;
    bool necesita_compactacion;
};
*/

/*
// Ejemplo de una cabecera de bloque libre (si fuera necesaria aquí)
struct CabeceraBloqueLibre {
    BlockId siguiente_bloque_libre; // Siguiente bloque libre en la lista
    BlockId anterior_bloque_libre;  // Anterior bloque libre en la lista
    uint32_t numero_bloques_consecutivos; // Si es parte de un bloque contiguo
    uint32_t tamaño_espacio_libre;  // Espacio libre total en este bloque
    uint32_t numero_fragmentos;     // Número de fragmentos libres
    uint64_t timestamp_liberado;    // Cuándo se liberó
    uint64_t numero_asignaciones;   // Veces asignado
    uint64_t numero_liberaciones;   // Veces liberado
    PageType ultimo_tipo_asignado;  // Último tipo asignado
    uint32_t prioridad_asignacion;  // Prioridad (0-100)
    uint8_t necesita_limpieza;      // 1 si necesita limpieza
    
    CabeceraBloqueLibre()
        : siguiente_bloque_libre(0), anterior_bloque_libre(0)
        , numero_bloques_consecutivos(1), tamaño_espacio_libre(0)
        , numero_fragmentos(0), timestamp_liberado(0)
        , numero_asignaciones(0), numero_liberaciones(0)
        , ultimo_tipo_asignado(PageType::FREE), prioridad_asignacion(50)
        , necesita_limpieza(0) {}
    
    void ActualizarEstadisticasUso(uint32_t tiempo_uso);
    uint32_t CalcularPrioridadAsignacion() const;
    bool EsAdecuadoPara(PageType tipo) const;
    std::string Serializar() const;
    bool Deserializar(const std::string& contenido);
};
*/

// ===== FUNCIONES UTILITARIAS =====
// NOTA: Las declaraciones de estas funciones se han movido a cabeceras_bloques.h
// para evitar redefiniciones y conflictos de sobrecarga.
// Este archivo ahora solo incluye cabeceras_bloques.h para acceder a ellas.

#endif // CABECERAS_ESPECIFICAS_H
