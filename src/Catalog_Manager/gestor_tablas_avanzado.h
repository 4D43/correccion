// gestor_tablas_avanzado.h - Gestor Avanzado de Tablas con funcionalidades específicas
// Implementa creación por archivo/formulario, inserción CSV/formulario, eliminación con condiciones

#ifndef GESTOR_TABLAS_AVANZADO_H
#define GESTOR_TABLAS_AVANZADO_H

#include "../include/common.h"
#include "gestor_catalogo.h"
#include "../record_manager/gestor_registros.h" // Asumiendo que GestorRegistros existe
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm> // Para std::transform, std::find_if
#include <cctype>    // Para ::toupper, std::isspace

/**
 * @brief Enumeración de tipos de datos permitidos en el sistema
 */
enum class TipoDatoTabla {
    INT,    // Enteros
    STR,    // Cadenas variables (VARCHAR)
    CHAR    // Cadenas fijas (CHAR)
};

/**
 * @brief Estructura para definir una columna de tabla
 */
struct DefinicionColumna {
    std::string nombre;
    TipoDatoTabla tipo;
    uint32_t tamaño;        // Para CHAR y STR (VARCHAR), define el tamaño fijo o máximo
    bool es_clave_primaria; // Indica si es parte de la clave primaria
    
    DefinicionColumna() : tipo(TipoDatoTabla::INT), tamaño(0), es_clave_primaria(false) {}
    
    DefinicionColumna(const std::string& n, TipoDatoTabla t, uint32_t tam = 0, bool pk = false)
        : nombre(n), tipo(t), tamaño(tam), es_clave_primaria(pk) {}
};

/**
 * @brief Estructura para representar el esquema completo de una tabla
 */
struct EsquemaTablaAvanzado {
    std::string nombre_tabla;
    std::vector<DefinicionColumna> columnas;
    bool es_longitud_fija;
    uint32_t tamaño_registro_fijo;  // Solo para tablas de longitud fija
    
    EsquemaTablaAvanzado() : es_longitud_fija(false), tamaño_registro_fijo(0) {}
    
    /**
     * @brief Calcula el tamaño total del registro para tablas fijas o una estimación para variables.
     * Para tablas de longitud variable, esto es una estimación.
     */
    uint32_t CalcularTamañoRegistro() const {
        if (es_longitud_fija) {
            return tamaño_registro_fijo;
        }
        
        uint32_t tamaño_total = 0;
        for (const auto& columna : columnas) {
            switch (columna.tipo) {
                case TipoDatoTabla::INT:
                    tamaño_total += sizeof(int32_t);
                    break;
                case TipoDatoTabla::CHAR:
                    tamaño_total += columna.tamaño;
                    break;
                case TipoDatoTabla::STR:
                    // Estimación para cadenas variables: tamaño de la longitud + tamaño máximo estimado del string
                    // Esto es una simplificación; en un SGBD real, se usaría un puntero a datos externos o un esquema más complejo.
                    tamaño_total += sizeof(uint32_t) + columna.tamaño; // Longitud (4 bytes) + datos (tamaño máximo)
                    break;
            }
        }
        return tamaño_total;
    }
};

/**
 * @brief Estructura para representar un espacio disponible en disco
 */
struct EspacioDisponible {
    PageId id_pagina;
    uint32_t offset;
    uint32_t tamaño_disponible;
    bool es_tabla_fija; // Indica si el espacio es para una tabla de longitud fija o variable
    
    EspacioDisponible(PageId pagina, uint32_t off, uint32_t tam, bool fija)
        : id_pagina(pagina), offset(off), tamaño_disponible(tam), es_tabla_fija(fija) {}
};

/**
 * @brief Estructura para condiciones de eliminación
 */
struct CondicionEliminacion {
    std::string nombre_columna;
    std::string valor;
    
    CondicionEliminacion(const std::string& col, const std::string& val)
        : nombre_columna(col), valor(val) {}
};

/**
 * @brief Clase principal para el manejo avanzado de tablas
 * Actúa como una capa superior sobre GestorCatalogo y GestorRegistros,
 * proporcionando funcionalidades de alto nivel como creación de tablas
 * desde archivos/formularios e inserción/eliminación de registros.
 */
class GestorTablasAvanzado {
private:
    GestorCatalogo* gestor_catalogo_;
    GestorRegistros* gestor_registros_;
    
    // Lista de espacios disponibles para reutilización
    std::vector<EspacioDisponible> espacios_disponibles_;
    
    // Métodos auxiliares privados
    TipoDatoTabla InferirTipoDato(const std::string& valor) const;
    std::vector<std::string> ParsearLineaCSV(const std::string& linea) const;
    bool ValidarDatoConTipo(const std::string& dato, TipoDatoTabla tipo) const;
    std::string LimpiarValor(const std::string& valor) const;
    Status ValidarEsquemaConDatos(const EsquemaTablaAvanzado& esquema, 
                                  const std::vector<std::string>& datos) const;
    
    /**
     * @brief Convierte un vector de DefinicionColumna a un vector de ColumnMetadata.
     * Necesario para interactuar con GestorCatalogo.
     * @param def_columnas Vector de DefinicionColumna
     * @return Vector de ColumnMetadata
     */
    std::vector<ColumnMetadata> ConvertirEsquemaAColumnMetadata(const std::vector<DefinicionColumna>& def_columnas) const;

public:
    /**
     * @brief Constructor del gestor de tablas avanzado
     * @param catalogo Referencia al GestorCatalogo
     * @param registros Referencia al GestorRegistros
     */
    GestorTablasAvanzado(GestorCatalogo& catalogo, GestorRegistros& registros);
    
    /**
     * @brief Destructor
     */
    ~GestorTablasAvanzado();
    
    // === CREACIÓN DE TABLAS ===
    
    /**
     * @brief Crea una tabla a partir de un archivo CSV.
     * El esquema se infiere de las dos primeras líneas (nombres y tipos de datos).
     * Luego, los datos restantes del archivo se insertan en la tabla.
     * * @param ruta_archivo Ruta al archivo CSV con los datos
     * @param nombre_tabla Nombre para la nueva tabla
     * @return Status del resultado de la operación
     */
    Status CrearTablaPorArchivo(const std::string& ruta_archivo, const std::string& nombre_tabla);
    
    /**
     * @brief Crea una tabla mediante un esquema predefinido (usado internamente o por interfaz).
     * Puede crear tablas de longitud fija o variable.
     * * @param esquema Esquema completo de la tabla a crear
     * @return Status del resultado de la operación
     */
    Status CrearTablaPorFormulario(const EsquemaTablaAvanzado& esquema);
    
    // === INSERCIÓN DE REGISTROS ===
    
    /**
     * @brief Inserta registros desde un archivo CSV en una tabla existente.
     * Valida que los datos del CSV coincidan con el esquema de la tabla.
     * * @param nombre_tabla Nombre de la tabla destino
     * @param ruta_archivo_csv Ruta al archivo CSV con los datos
     * @return Status del resultado de la operación
     */
    Status InsertarRegistrosPorCSV(const std::string& nombre_tabla, const std::string& ruta_archivo_csv);
    
    /**
     * @brief Inserta un registro individual en una tabla.
     * Valida los datos proporcionados contra el esquema de la tabla.
     * * @param nombre_tabla Nombre de la tabla destino
     * @param datos_registro Datos del registro a insertar (vector de strings)
     * @return Status del resultado de la operación
     */
    Status InsertarRegistroPorFormulario(const std::string& nombre_tabla, 
                                        const std::vector<std::string>& datos_registro);
    
    // === ELIMINACIÓN DE REGISTROS ===
    
    /**
     * @brief Elimina registros de una tabla que cumplan con un conjunto de condiciones.
     * Múltiples condiciones se unen lógicamente con AND.
     * * @param nombre_tabla Nombre de la tabla
     * @param condiciones Vector de condiciones a aplicar para la eliminación
     * @return Status del resultado de la operación
     */
    Status EliminarRegistrosPorCondiciones(const std::string& nombre_tabla,
                                          const std::vector<CondicionEliminacion>& condiciones);
    
    // === GESTIÓN DE ESPACIOS DISPONIBLES ===
    
    /**
     * @brief Registra un bloque de espacio disponible en disco para su futura reutilización.
     * * @param id_pagina ID de la página donde se encuentra el espacio libre
     * @param offset Offset dentro de la página donde comienza el espacio libre
     * @param tamaño Tamaño del espacio disponible en bytes
     * @param es_tabla_fija Indica si el espacio proviene de una tabla de longitud fija o variable
     */
    void RegistrarEspacioDisponible(PageId id_pagina, uint32_t offset, 
                                   uint32_t tamaño, bool es_tabla_fija);
    
    /**
     * @brief Busca un espacio disponible adecuado en la lista de espacios libres.
     * Prioriza espacios que se ajusten mejor al tamaño necesario.
     * * @param tamaño_necesario Tamaño del registro que necesita ser almacenado
     * @param es_tabla_fija Indica si el registro es para una tabla de longitud fija
     * @return Puntero a la estructura EspacioDisponible encontrada, o nullptr si no hay espacio adecuado
     */
    EspacioDisponible* BuscarEspacioDisponible(uint32_t tamaño_necesario, bool es_tabla_fija);
    
    /**
     * @brief Actualiza o elimina un espacio disponible después de que una parte o la totalidad de este ha sido utilizada.
     * * @param espacio Puntero al espacio disponible que se ha utilizado
     * @param tamaño_usado Tamaño de bytes que se han ocupado del espacio disponible
     */
    void LiberarEspacioUsado(EspacioDisponible* espacio, uint32_t tamaño_usado);
    
    // === MÉTODOS DE UTILIDAD ===
    
    /**
     * @brief Obtiene el esquema completo de una tabla existente desde el catálogo.
     * * @param nombre_tabla Nombre de la tabla cuyo esquema se desea obtener
     * @param esquema Referencia a la estructura EsquemaTablaAvanzado donde se almacenará el esquema
     * @return Status del resultado de la operación (OK si se encontró y cargó el esquema, NOT_FOUND si no existe la tabla)
     */
    Status ObtenerEsquemaTabla(const std::string& nombre_tabla, EsquemaTablaAvanzado& esquema);
    
    /**
     * @brief Valida si una tabla con el nombre especificado existe en el catálogo.
     * * @param nombre_tabla Nombre de la tabla a verificar
     * @return true si la tabla existe, false en caso contrario
     */
    bool ExisteTabla(const std::string& nombre_tabla);
    
    /**
     * @brief Imprime en la consola información detallada sobre los espacios disponibles registrados.
     */
    void ImprimirEspaciosDisponibles() const;
    
    /**
     * @brief Imprime en la consola estadísticas generales del gestor de tablas avanzado.
     */
    void ImprimirEstadisticas() const;
    
    // === MÉTODOS DE FORMULARIO INTERACTIVO ===
    
    /**
     * @brief Proporciona una interfaz interactiva paso a paso para que el usuario cree una nueva tabla.
     * Guía al usuario para definir el nombre de la tabla, el número de columnas y sus propiedades.
     * @return Status del resultado de la operación
     */
    Status CrearTablaInteractiva();
    
    /**
     * @brief Proporciona una interfaz interactiva para que el usuario inserte un nuevo registro en una tabla existente.
     * Solicita al usuario los valores para cada columna según el esquema de la tabla.
     * * @param nombre_tabla Nombre de la tabla destino para la inserción
     * @return Status del resultado de la operación
     */
    Status InsertarRegistroInteractivo(const std::string& nombre_tabla);
    
    /**
     * @brief Proporciona una interfaz interactiva para que el usuario defina condiciones y elimine registros.
     * Guía al usuario para seleccionar columnas y valores para las condiciones de eliminación.
     * * @param nombre_tabla Nombre de la tabla de la cual se eliminarán registros
     * @return Status del resultado de la operación
     */
    Status EliminarRegistrosInteractivo(const std::string& nombre_tabla);
    
    // === MÉTODOS DE CONVERSIÓN ===
    
    /**
     * @brief Convierte un valor de la enumeración TipoDatoTabla a su representación en string.
     * @param tipo El tipo de dato a convertir
     * @return Una cadena de texto que representa el tipo de dato (ej., "INT", "STR", "CHAR")
     */
    std::string TipoAString(TipoDatoTabla tipo) const;
    
    /**
     * @brief Convierte una cadena de texto a su correspondiente valor de la enumeración TipoDatoTabla.
     * La conversión es insensible a mayúsculas/minúsculas.
     * @param tipo_str La cadena de texto a convertir (ej., "int", "STR", "char")
     * @return El valor de TipoDatoTabla correspondiente, o TipoDatoTabla::STR por defecto si no se reconoce
     */
    TipoDatoTabla StringATipo(const std::string& tipo_str) const;
    
    /**
     * @brief Convierte una estructura DefinicionColumna (usada en el gestor avanzado)
     * a una estructura ColumnMetadata (usada por el catálogo).
     * @param def_col La definición de columna a convertir
     * @return La estructura ColumnMetadata resultante
     */
    ColumnMetadata ConvertirAColumnMetadata(const DefinicionColumna& def_col) const;
};

#endif // GESTOR_TABLAS_AVANZADO_H
