// data_storage/pagina.h
#ifndef PAGINA_H
#define PAGINA_H

#include "../include/common.h" // Para PageId, FrameId, BlockId
#include <chrono>              // Para timestamps
#include <string>              // Para std::string

/**
 * @brief Estructura que representa los metadatos de una página (frame) en el Buffer Pool
 * 
 * CONCEPTO IMPORTANTE:
 * - Esta estructura NO contiene los datos reales del bloque
 * - Solo contiene información de gestión y control de la página
 * - Los datos reales se almacenan en el vector de bytes del GestorBuffer
 * - Una página es una COPIA de un bloque del disco en memoria
 */
struct Pagina {
    // ===== IDENTIFICACIÓN =====
    
    /**
     * @brief ID del bloque de disco al que corresponde esta página en memoria
     * PageId es un alias para BlockId - representa el bloque original
     */
    PageId id_bloque;
    
    /**
     * @brief ID del frame en el buffer pool donde está almacenada esta página
     */
    FrameId id_frame;

    // ===== CONTROL DE ACCESO =====
    
    /**
     * @brief Número de veces que esta página está "anclada" (pinned)
     * Una página anclada no puede ser desalojada del buffer
     * Incrementa con AnclarPagina(), decrementa con DesanclarPagina()
     */
    int contador_anclajes;
    
    /**
     * @brief Indica si este frame del buffer contiene datos válidos
     * - true: Frame contiene una copia válida de un bloque
     * - false: Frame vacío o invalidado
     */
    bool es_valida;

    // ===== ESTADO DE MODIFICACIÓN =====
    
    /**
     * @brief Indica si la página ha sido modificada en memoria
     * - true: Página modificada, necesita ser escrita al disco
     * - false: Página sin cambios, coincide con el disco
     */
    bool esta_sucia;
    
    /**
     * @brief Indica si hay cambios pendientes de confirmar
     * - true: Hay modificaciones que el usuario no ha confirmado
     * - false: Todos los cambios han sido confirmados o no hay cambios
     */
    bool cambios_pendientes;

    // ===== INFORMACIÓN TEMPORAL =====
    
    /**
     * @brief Timestamp de la última vez que se accedió a esta página
     * Usado por políticas de reemplazo como LRU
     */
    std::chrono::steady_clock::time_point ultimo_acceso;
    
    /**
     * @brief Timestamp de la última modificación de esta página
     */
    std::chrono::steady_clock::time_point ultima_modificacion;
    
    /**
     * @brief Timestamp de la última confirmación al disco
     */
    std::chrono::steady_clock::time_point ultima_confirmacion;

    // ===== METADATOS ADICIONALES =====
    
    /**
     * @brief Tipo de página (DATA, CATALOG, INDEX, FREE, etc.)
     */
    PageType tipo_pagina;
    
    /**
     * @brief Número de veces que esta página ha sido accedida
     * Útil para estadísticas y políticas de reemplazo
     */
    uint32_t contador_accesos;
    
    /**
     * @brief Número de veces que esta página ha sido modificada
     */
    uint32_t contador_modificaciones;

    // ===== CONSTRUCTORES =====
    
    /**
     * @brief Constructor por defecto
     * Inicializa la página a un estado "vacío" o no válido
     */
    Pagina() 
        : id_bloque(0)
        , id_frame(-1)
        , contador_anclajes(0)
        , es_valida(false)
        , esta_sucia(false)
        , cambios_pendientes(false)
        , ultimo_acceso(std::chrono::steady_clock::now())
        , ultima_modificacion(std::chrono::steady_clock::now())
        , ultima_confirmacion(std::chrono::steady_clock::now())
        , tipo_pagina(PageType::FREE)
        , contador_accesos(0)
        , contador_modificaciones(0) {
    }

    /**
     * @brief Constructor con parámetros básicos
     * @param id_bloque ID del bloque asociado
     * @param tipo Tipo de página
     * @param anclajes Número inicial de anclajes
     * @param sucia Estado inicial de modificación
     * @param valida Estado inicial de validez
     */
    Pagina(PageId id_bloque, PageType tipo = PageType::DATA, 
           int anclajes = 0, bool sucia = false, bool valida = true)
        : id_bloque(id_bloque)
        , id_frame(-1)
        , contador_anclajes(anclajes)
        , es_valida(valida)
        , esta_sucia(sucia)
        , cambios_pendientes(false)
        , ultimo_acceso(std::chrono::steady_clock::now())
        , ultima_modificacion(std::chrono::steady_clock::now())
        , ultima_confirmacion(std::chrono::steady_clock::now())
        , tipo_pagina(tipo)
        , contador_accesos(0)
        , contador_modificaciones(0) {
    }

    // ===== MÉTODOS DE GESTIÓN =====
    
    /**
     * @brief Reinicia los metadatos de la página a su estado por defecto
     */
    void Reiniciar() {
        id_bloque = 0;
        id_frame = -1;
        contador_anclajes = 0;
        es_valida = false;
        esta_sucia = false;
        cambios_pendientes = false;
        tipo_pagina = PageType::FREE;
        contador_accesos = 0;
        contador_modificaciones = 0;
        
        auto ahora = std::chrono::steady_clock::now();
        ultimo_acceso = ahora;
        ultima_modificacion = ahora;
        ultima_confirmacion = ahora;
    }

    /**
     * @brief Marca la página como accedida (actualiza timestamp y contador)
     */
    void MarcarAcceso() {
        ultimo_acceso = std::chrono::steady_clock::now();
        contador_accesos++;
    }

    /**
     * @brief Marca la página como modificada
     * @param pendiente Si true, marca como cambios pendientes de confirmar
     */
    void MarcarModificacion(bool pendiente = true) {
        esta_sucia = true;
        cambios_pendientes = pendiente;
        ultima_modificacion = std::chrono::steady_clock::now();
        contador_modificaciones++;
    }

    /**
     * @brief Marca la página como confirmada (escrita al disco)
     */
    void MarcarConfirmacion() {
        esta_sucia = false;
        cambios_pendientes = false;
        ultima_confirmacion = std::chrono::steady_clock::now();
    }

    /**
     * @brief Incrementa el contador de anclajes
     * @return Nuevo valor del contador
     */
    int Anclar() {
        return ++contador_anclajes;
    }

    /**
     * @brief Decrementa el contador de anclajes
     * @return Nuevo valor del contador (mínimo 0)
     */
    int Desanclar() {
        if (contador_anclajes > 0) {
            contador_anclajes--;
        }
        return contador_anclajes;
    }

    /**
     * @brief Verifica si la página puede ser desalojada
     * @return true si la página no está anclada y es válida
     */
    bool PuedeSerDesalojada() const {
        return contador_anclajes == 0 && es_valida;
    }

    /**
     * @brief Verifica si la página necesita ser escrita al disco
     * @return true si está sucia y es válida
     */
    bool NecesitaEscritura() const {
        return esta_sucia && es_valida;
    }

    // ===== MÉTODOS DE INFORMACIÓN =====
    
    /**
     * @brief Obtiene el tiempo transcurrido desde el último acceso
     * @return Duración en milisegundos
     */
    std::chrono::milliseconds TiempoDesdeUltimoAcceso() const {
        auto ahora = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(ahora - ultimo_acceso);
    }

    /**
     * @brief Obtiene el tiempo transcurrido desde la última modificación
     * @return Duración en milisegundos
     */
    std::chrono::milliseconds TiempoDesdeUltimaModificacion() const {
        auto ahora = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(ahora - ultima_modificacion);
    }

    /**
     * @brief Convierte el tipo de página a string para depuración
     * @return String representando el tipo de página
     */
    std::string TipoPaginaAString() const {
        switch (tipo_pagina) {
            case PageType::DATA: return "DATOS";
            case PageType::CATALOG: return "CATALOGO";
            case PageType::INDEX: return "INDICE";
            case PageType::FREE: return "LIBRE";
            default: return "DESCONOCIDO";
        }
    }

    /**
     * @brief Genera una representación en string de la página para depuración
     * @return String con información detallada de la página
     */
    std::string ToString() const {
        std::string resultado = "Pagina{";
        resultado += "id_bloque=" + std::to_string(id_bloque);
        resultado += ", id_frame=" + std::to_string(id_frame);
        resultado += ", anclajes=" + std::to_string(contador_anclajes);
        resultado += ", valida=";
        resultado += (es_valida ? "true" : "false");
        resultado += ", sucia=";
        resultado += (esta_sucia ? "true" : "false");
        resultado += ", pendiente=";
        resultado += (cambios_pendientes ? "true" : "false");
        resultado += ", tipo=" + TipoPaginaAString();
        resultado += ", accesos=" + std::to_string(contador_accesos);
        resultado += ", modificaciones=" + std::to_string(contador_modificaciones);
        resultado += "}";
        return resultado;
    }
};

#endif // PAGINA_H
