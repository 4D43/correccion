// gestor_registros.h - Gestor de Registros del SGBD
// Refactorizado completamente a español con estructura mejorada

#ifndef GESTOR_REGISTROS_H
#define GESTOR_REGISTROS_H

#include "../include/common.h"
#include "../data_storage/gestor_buffer.h"
#include "../data_storage/cabeceras_especificas.h"
#include "../Catalog_Manager/gestor_catalogo.h"
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <unordered_map> // Added for std::unordered_map
#include <iostream>
#include <cstring>

// Declaraciones adelantadas para evitar dependencias circulares
class GestorCatalogo;
class GestorIndices;
struct EsquemaTablaCompleto;

/**
 * Estructura para representar los datos de un registro de forma estructurada
 * Facilita la manipulación antes de serialización/deserialización
 */
struct DatosRegistro {
    std::vector<std::string> campos;                    // Campos del registro como strings
    std::map<std::string, std::string> campos_por_nombre; // Acceso por nombre de columna (opcional)
    
    // Constructor por defecto
    DatosRegistro() = default;
    
    // Constructor con lista de campos
    DatosRegistro(const std::vector<std::string>& campos_iniciales) 
        : campos(campos_iniciales) {}
    
    // Método para añadir campo por nombre
    void AñadirCampo(const std::string& nombre_campo, const std::string& valor) {
        campos.push_back(valor);
        campos_por_nombre[nombre_campo] = valor; // Ensure map is also updated
    }

    // Método para obtener campo por nombre (ejemplo, se puede mejorar con manejo de errores)
    std::string ObtenerCampo(const std::string& nombre_campo) const {
        auto it = campos_por_nombre.find(nombre_campo);
        if (it != campos_por_nombre.end()) {
            return it->second;
        }
        return ""; // O lanzar una excepción, dependiendo de la política de errores
    }

    // Método para verificar si el registro está vacío
    bool EstaVacio() const {
        return campos.empty();
    }
};

/**
 * @brief Estructura para almacenar estadísticas de una página de datos.
 */
struct EstadisticasPagina {
    PageId id_pagina;
    uint32_t numero_registros;      // Total de registros en la página
    uint32_t registros_activos;     // Registros no marcados para eliminación
    uint32_t registros_eliminados;  // Registros marcados para eliminación (espacio reutilizable)
    uint64_t espacio_libre_bytes;   // Espacio libre real en la página
    uint64_t fragmentacion_bytes;   // Espacio fragmentado
    uint64_t timestamp_ultima_actualizacion; // Última vez que se actualizaron las estadísticas

    EstadisticasPagina() 
        : id_pagina(INVALID_FRAME_ID), numero_registros(0), // Changed INVALID_PAGE_ID to INVALID_FRAME_ID
          registros_activos(0), registros_eliminados(0), 
          espacio_libre_bytes(0), fragmentacion_bytes(0), 
          timestamp_ultima_actualizacion(0) {}
};


/**
 * @brief Gestor de Registros: Maneja la inserción, eliminación, actualización y consulta de registros.
 *
 * Responsabilidades principales:
 * - Interactuar con el GestorBuffer para acceder a las páginas de datos.
 * - Serializar y deserializar registros desde/hacia el formato de almacenamiento.
 * - Gestionar el espacio dentro de las páginas (slots, espacio libre).
 * - Mantener estadísticas sobre el uso de las páginas y registros.
 * - Integración con el GestorCatalogo para obtener esquemas de tablas.
 * - Integración con el GestorIndices para mantener los índices actualizados.
 */
class GestorRegistros {
public:
    /**
     * @brief Constructor del GestorRegistros.
     * @param gestor_buffer Referencia al GestorBuffer.
     */
    GestorRegistros(GestorBuffer& gestor_buffer);

    /**
     * @brief Destructor del GestorRegistros.
     */
    ~GestorRegistros();

    /**
     * @brief Establece el puntero al GestorCatalogo.
     * @param catalogo Puntero al GestorCatalogo.
     */
    void SetGestorCatalogo(GestorCatalogo* catalogo) { gestor_catalogo_ = catalogo; }

    /**
     * @brief Establece el puntero al GestorIndices.
     * @param indices Puntero al GestorIndices.
     */
    void SetGestorIndices(GestorIndices* indices) { gestor_indices_ = indices; }

    /**
     * @brief Inserta un nuevo registro en la tabla especificada.
     * @param nombre_tabla Nombre de la tabla.
     * @param datos_registro Datos del registro a insertar.
     * @return Status de la operación.
     */
    Status InsertarRegistro(const std::string& nombre_tabla, const DatosRegistro& datos_registro);

    /**
     * @brief Consulta registros de una tabla por ID.
     * @param nombre_tabla Nombre de la tabla.
     * @param id_registro ID del registro a consultar.
     * @param datos_registro_salida Datos del registro encontrado (salida).
     * @return Status de la operación.
     */
    Status ConsultarRegistroPorID(const std::string& nombre_tabla, RecordId id_registro, DatosRegistro& datos_registro_salida);

    /**
     * @brief Consulta todos los registros de una tabla.
     * @param nombre_tabla Nombre de la tabla.
     * @param resultados Vector de DatosRegistro donde se almacenarán los resultados (salida).
     * @return Status de la operación.
     */
    Status ConsultarTodosLosRegistros(const std::string& nombre_tabla, std::vector<DatosRegistro>& resultados);

    /**
     * @brief Actualiza un registro existente en la tabla.
     * @param nombre_tabla Nombre de la tabla.
     * @param id_registro ID del registro a actualizar.
     * @param nuevos_datos Nuevos datos del registro.
     * @return Status de la operación.
     */
    Status ActualizarRegistro(const std::string& nombre_tabla, RecordId id_registro, const DatosRegistro& nuevos_datos);

    /**
     * @brief Elimina un registro de la tabla por ID.
     * @param nombre_tabla Nombre de la tabla.
     * @param id_registro ID del registro a eliminar.
     * @return Status de la operación.
     */
    Status EliminarRegistroPorID(const std::string& nombre_tabla, RecordId id_registro);

    /**
     * @brief Compacta una página de datos, eliminando espacio fragmentado.
     * @param id_pagina ID de la página a compactar.
     * @return Status de la operación.
     */
    Status CompactarPagina(PageId id_pagina);

    /**
     * @brief Imprime las estadísticas de uso de las páginas de una tabla.
     * @param nombre_tabla Nombre de la tabla.
     */
    void ImprimirEstadisticasTabla(const std::string& nombre_tabla);

    /**
     * @brief Imprime estadísticas generales del gestor de registros.
     */
    void ImprimirEstadisticasGenerales();

private:
    GestorBuffer* gestor_buffer_;             // Puntero al gestor de buffer
    GestorCatalogo* gestor_catalogo_;         // Puntero al gestor de catálogo
    GestorIndices* gestor_indices_;           // Puntero al gestor de índices

    // Estadísticas generales
    uint64_t total_inserciones_;
    uint64_t total_actualizaciones_;
    uint64_t total_eliminaciones_;
    uint64_t total_consultas_;
    uint64_t total_compactaciones_;

    // Mapa para almacenar estadísticas por página
    std::unordered_map<PageId, EstadisticasPagina> estadisticas_paginas_;

    // === MÉTODOS AUXILIARES PRIVADOS ===

    /**
     * @brief Serializa un registro a un vector de bytes.
     * @param datos_registro Datos del registro.
     * @param esquema Esquema de la tabla.
     * @return Vector de bytes serializado.
     */
    std::vector<Byte> SerializarRegistro(const DatosRegistro& datos_registro, 
                                         const EsquemaTablaCompleto& esquema) const;

    /**
     * @brief Deserializa un registro desde un vector de bytes.
     * @param datos_raw Vector de bytes del registro.
     * @param esquema Esquema de la tabla.
     * @return DatosRegistro deserializado.
     */
    DatosRegistro DeserializarRegistro(const std::vector<Byte>& datos_raw, 
                                       const EsquemaTablaCompleto& esquema) const;

    /**
     * @brief Busca un slot libre en una página de datos.
     * @param datos_pagina Puntero a los datos de la página.
     * @param cabecera_datos Cabecera del bloque de datos.
     * @param id_slot_libre ID del slot libre encontrado (salida).
     * @return true si se encontró un slot libre.
     */
    bool BuscarSlotLibre(Byte* datos_pagina, const CabeceraBloqueDatos& cabecera_datos, 
                        uint32_t& id_slot_libre);
    
    /**
     * Actualiza las estadísticas de una página después de una operación
     * @param id_pagina ID de la página
     * @param tipo_operacion Tipo de operación realizada
     */
    void ActualizarEstadisticasPagina(PageId id_pagina, const std::string& tipo_operacion);
    
    /**
     * Valida que los datos del registro sean compatibles con el esquema
     * @param datos_registro Datos del registro a validar
     * @param esquema Esquema de la tabla
     * @return true si los datos son válidos
     */
    bool ValidarDatosRegistro(const DatosRegistro& datos_registro, 
                             const EsquemaTablaCompleto& esquema) const;
    
    /**
     * Calcula el offset donde debe insertarse un nuevo registro
     * @param datos_pagina Puntero a los datos de la página
     * @param tamaño_registro Tamaño del registro a insertar
     * @param offset_insercion Offset calculado (salida)
     * @return true si hay espacio suficiente
     */
    bool CalcularOffsetInsercion(Byte* datos_pagina, uint32_t tamaño_registro, 
                                uint32_t& offset_insercion);
    
    /**
     * Obtiene el timestamp actual en milisegundos desde epoch
     * @return Timestamp actual
     */
    uint64_t ObtenerTimestampActual() const;
};

#endif // GESTOR_REGISTROS_H
