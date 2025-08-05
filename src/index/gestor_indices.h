// gestor_indices.h - Gestor de Índices refactorizado completamente a español
// Sistema de gestión de índices B+ Tree y Hash para el SGBD educativo

#pragma once

#include "../include/common.h"
#include "../data_storage/cabeceras_especificas.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <optional>
#include <set>
#include <vector>
#include <iostream>

// Forward declarations
class GestorBuffer;
class GestorCatalogo;

/**
 * Estructura para representar una entrada de índice
 * Contiene la clave y el conjunto de ubicaciones de registros
 */
template<typename TipoClave>
struct EntradaIndice {
    TipoClave clave;                    // Clave del índice
    std::set<RecordId> ubicaciones;     // IDs de registros que contienen esta clave
    
    EntradaIndice() = default;
    EntradaIndice(const TipoClave& c) : clave(c) {}
    
    void AgregarUbicacion(RecordId id_registro) {
        ubicaciones.insert(id_registro);
    }
    
    void EliminarUbicacion(RecordId id_registro) {
        ubicaciones.erase(id_registro);
    }
    
    bool EstaVacia() const {
        return ubicaciones.empty();
    }
};

/**
 * Clase base abstracta para todos los tipos de índices
 */
class IndiceBase {
public:
    virtual ~IndiceBase() = default;
    
    // Métodos virtuales puros que deben implementar las clases derivadas
    virtual Status Insertar(const std::string& clave_str, int clave_int, RecordId id_registro) = 0;
    virtual Status Eliminar(const std::string& clave_str, int clave_int, RecordId id_registro) = 0;
    virtual std::optional<std::set<RecordId>> Buscar(const std::string& clave_str, int clave_int) const = 0;
    virtual Status Persistir(const std::string& ruta_archivo) const = 0;
    virtual Status Cargar(const std::string& ruta_archivo) = 0;
    virtual void ImprimirEstructura() const = 0;
    virtual TipoIndice ObtenerTipo() const = 0;
    virtual uint32_t ObtenerNumeroEntradas() const = 0;
    virtual uint32_t ObtenerAltura() const = 0;
};

/**
 * Implementación de índice B+ Tree para valores enteros
 */
class IndiceBTreeEntero : public IndiceBase {
private:
    static const uint32_t ORDEN_ARBOL = 4;  // Orden del árbol B+
    
    struct NodoInterno;
    struct NodoHoja;
    
    struct NodoBase {
        bool es_hoja;
        uint32_t numero_claves;
        NodoBase* padre;
        
        NodoBase(bool hoja) : es_hoja(hoja), numero_claves(0), padre(nullptr) {}
        virtual ~NodoBase() = default;
    };
    
    struct NodoInterno : public NodoBase {
        int claves[ORDEN_ARBOL - 1];
        NodoBase* hijos[ORDEN_ARBOL];
        
        NodoInterno() : NodoBase(false) {
            for (uint32_t i = 0; i < ORDEN_ARBOL; ++i) {
                hijos[i] = nullptr;
            }
        }
    };
    
    struct NodoHoja : public NodoBase {
        EntradaIndice<int> entradas[ORDEN_ARBOL - 1];
        NodoHoja* siguiente;
        
        NodoHoja() : NodoBase(true), siguiente(nullptr) {}
    };
    
    NodoBase* raiz_;
    uint32_t altura_;
    uint32_t numero_entradas_;
    
    // Métodos auxiliares privados
    NodoHoja* BuscarHoja(int clave) const;
    Status InsertarEnHoja(NodoHoja* hoja, int clave, RecordId id_registro);
    Status DividirHoja(NodoHoja* hoja);
    Status EliminarDeHoja(NodoHoja* hoja, int clave, RecordId id_registro);
    void LiberarNodo(NodoBase* nodo);
    
public:
    IndiceBTreeEntero();
    ~IndiceBTreeEntero() override;
    
    Status Insertar(const std::string& clave_str, int clave_int, RecordId id_registro) override;
    Status Eliminar(const std::string& clave_str, int clave_int, RecordId id_registro) override;
    std::optional<std::set<RecordId>> Buscar(const std::string& clave_str, int clave_int) const override;
    Status Persistir(const std::string& ruta_archivo) const override;
    Status Cargar(const std::string& ruta_archivo) override;
    void ImprimirEstructura() const override;
    TipoIndice ObtenerTipo() const override { return TipoIndice::BTREE_ENTERO; }
    uint32_t ObtenerNumeroEntradas() const override { return numero_entradas_; }
    uint32_t ObtenerAltura() const override { return altura_; }
};

/**
 * Implementación de índice B+ Tree para valores de cadena
 */
class IndiceBTreeCadena : public IndiceBase {
private:
    static const uint32_t ORDEN_ARBOL = 4;
    
    struct NodoInterno;
    struct NodoHoja;
    
    struct NodoBase {
        bool es_hoja;
        uint32_t numero_claves;
        NodoBase* padre;
        
        NodoBase(bool hoja) : es_hoja(hoja), numero_claves(0), padre(nullptr) {}
        virtual ~NodoBase() = default;
    };
    
    struct NodoInterno : public NodoBase {
        std::string claves[ORDEN_ARBOL - 1];
        NodoBase* hijos[ORDEN_ARBOL];
        
        NodoInterno() : NodoBase(false) {
            for (uint32_t i = 0; i < ORDEN_ARBOL; ++i) {
                hijos[i] = nullptr;
            }
        }
    };
    
    struct NodoHoja : public NodoBase {
        EntradaIndice<std::string> entradas[ORDEN_ARBOL - 1];
        NodoHoja* siguiente;
        
        NodoHoja() : NodoBase(true), siguiente(nullptr) {}
    };
    
    NodoBase* raiz_;
    uint32_t altura_;
    uint32_t numero_entradas_;
    
    // Métodos auxiliares privados
    NodoHoja* BuscarHoja(const std::string& clave) const;
    Status InsertarEnHoja(NodoHoja* hoja, const std::string& clave, RecordId id_registro);
    Status DividirHoja(NodoHoja* hoja);
    Status EliminarDeHoja(NodoHoja* hoja, const std::string& clave, RecordId id_registro);
    void LiberarNodo(NodoBase* nodo);
    
public:
    IndiceBTreeCadena();
    ~IndiceBTreeCadena() override;
    
    Status Insertar(const std::string& clave_str, int clave_int, RecordId id_registro) override;
    Status Eliminar(const std::string& clave_str, int clave_int, RecordId id_registro) override;
    std::optional<std::set<RecordId>> Buscar(const std::string& clave_str, int clave_int) const override;
    Status Persistir(const std::string& ruta_archivo) const override;
    Status Cargar(const std::string& ruta_archivo) override;
    void ImprimirEstructura() const override;
    TipoIndice ObtenerTipo() const override { return TipoIndice::BTREE_CADENA; }
    uint32_t ObtenerNumeroEntradas() const override { return numero_entradas_; }
    uint32_t ObtenerAltura() const override { return altura_; }
};

/**
 * Implementación de índice Hash para valores de cadena
 */
class IndiceHashCadena : public IndiceBase {
private:
    static const uint32_t TAMAÑO_TABLA_HASH = 1024;  // Tamaño inicial de la tabla hash
    static constexpr double FACTOR_CARGA_MAXIMO = 0.75;   // Factor de carga máximo antes de redimensionar
    
    struct Bucket {
        std::vector<EntradaIndice<std::string>> entradas;
        
        void AgregarEntrada(const std::string& clave, RecordId id_registro);
        bool EliminarEntrada(const std::string& clave, RecordId id_registro);
        std::optional<std::set<RecordId>> BuscarEntrada(const std::string& clave) const;
    };
    
    std::vector<Bucket> tabla_hash_;
    uint32_t numero_entradas_;
    uint32_t numero_buckets_;
    
    // Métodos auxiliares privados
    uint32_t FuncionHash(const std::string& clave) const;
    Status Redimensionar();
    void RehashearEntradas(const std::vector<Bucket>& tabla_anterior);
    
public:
    IndiceHashCadena();
    ~IndiceHashCadena() override = default;
    
    Status Insertar(const std::string& clave_str, int clave_int, RecordId id_registro) override;
    Status Eliminar(const std::string& clave_str, int clave_int, RecordId id_registro) override;
    std::optional<std::set<RecordId>> Buscar(const std::string& clave_str, int clave_int) const override;
    Status Persistir(const std::string& ruta_archivo) const override;
    Status Cargar(const std::string& ruta_archivo) override;
    void ImprimirEstructura() const override;
    TipoIndice ObtenerTipo() const override { return TipoIndice::HASH_CADENA; }
    uint32_t ObtenerNumeroEntradas() const override { return numero_entradas_; }
    uint32_t ObtenerAltura() const override { return 1; } // Los índices hash tienen altura constante
    
    // Métodos específicos del índice hash
    double ObtenerFactorCarga() const;
    uint32_t ObtenerNumeroBuckets() const { return numero_buckets_; }
};

/**
 * Estructura para almacenar información de un índice
 */
struct InformacionIndice {
    std::string nombre_tabla;           // Nombre de la tabla
    std::string nombre_columna;         // Nombre de la columna indexada
    TipoIndice tipo;                   // Tipo de índice
    std::unique_ptr<IndiceBase> indice; // Puntero al índice
    uint64_t timestamp_creacion;       // Timestamp de creación
    uint64_t timestamp_modificacion;   // Timestamp de última modificación
    bool esta_activo;                  // Si el índice está activo
    
    InformacionIndice(const std::string& tabla, const std::string& columna, TipoIndice t)
        : nombre_tabla(tabla), nombre_columna(columna), tipo(t), 
          timestamp_creacion(0), timestamp_modificacion(0), esta_activo(true) {}
};

/**
 * Estadísticas globales del gestor de índices
 */
struct EstadisticasIndices {
    uint32_t indices_creados = 0;
    uint32_t indices_eliminados = 0;
    uint32_t inserciones_realizadas = 0;
    uint32_t eliminaciones_realizadas = 0;
    uint32_t busquedas_realizadas = 0;
    uint32_t indices_btree_entero = 0;
    uint32_t indices_btree_cadena = 0;
    uint32_t indices_hash_cadena = 0;
    uint64_t tiempo_total_busquedas_ms = 0;
    uint64_t tiempo_total_inserciones_ms = 0;
    uint64_t tiempo_total_eliminaciones_ms = 0;
};

/**
 * Clase principal del Gestor de Índices
 * Maneja la creación, mantenimiento y consulta de índices para el SGBD
 */
class GestorIndices {
private:
    // Referencias a otros gestores del sistema
    GestorBuffer* gestor_buffer_;
    GestorCatalogo* gestor_catalogo_;
    
    // Almacenamiento de índices organizados por tabla y columna
    std::unordered_map<std::string, 
        std::unordered_map<std::string, std::unique_ptr<InformacionIndice>>> indices_;
    
    // Estadísticas globales
    EstadisticasIndices estadisticas_;
    
    // Configuración del gestor
    std::string directorio_indices_;    // Directorio donde se almacenan los índices
    bool persistencia_automatica_;     // Si se debe persistir automáticamente
    uint32_t max_indices_por_tabla_;   // Máximo número de índices por tabla
    
    // Métodos auxiliares privados
    std::string GenerarClaveIndice(const std::string& tabla, const std::string& columna) const;
    std::string GenerarRutaArchivo(const std::string& tabla, const std::string& columna) const;
    Status ValidarParametrosIndice(const std::string& tabla, const std::string& columna) const;
    TipoIndice DeterminarTipoIndice(ColumnType tipo_columna, TipoIndice tipo_preferido) const;
    Status CrearDirectorioIndices() const;
    uint64_t ObtenerTimestampActual() const;
    
    // Métodos para selección automática de índices según estrategia del usuario
    TipoIndice SeleccionarTipoIndiceAutomatico(const std::string& nombre_tabla, 
                                               const std::string& nombre_columna) const;
    bool EsTablaLongitudVariable(const std::string& nombre_tabla) const;
    ColumnType ObtenerTipoColumna(const std::string& nombre_tabla, 
                                  const std::string& nombre_columna) const;
    
public:
    // Constructor y destructor
    explicit GestorIndices(GestorBuffer& gestor_buffer);
    ~GestorIndices();
    
    // Métodos de configuración
    void EstablecerGestorCatalogo(GestorCatalogo& gestor_catalogo);
    void EstablecerDirectorioIndices(const std::string& directorio);
    void EstablecerPersistenciaAutomatica(bool activar);
    void EstablecerMaxIndicesPorTabla(uint32_t maximo);
    
    // Métodos de gestión de índices
    Status CrearIndice(const std::string& nombre_tabla, const std::string& nombre_columna, 
                      TipoIndice tipo = TipoIndice::BTREE_ENTERO);
    
    // Método para crear índice automáticamente según la estrategia del usuario:
    // - Tablas de longitud variable: SIEMPRE Hash
    // - Tablas de longitud fija: B+ Tree para INT, String B+ Tree para STR/CHAR
    Status CrearIndiceAutomatico(const std::string& nombre_tabla, const std::string& nombre_columna);
    
    // Método para crear múltiples índices automáticamente para una tabla
    Status CrearIndicesAutomaticosPorTabla(const std::string& nombre_tabla, 
                                            const std::vector<std::string>& columnas);
    
    // Método para cargar automáticamente todos los índices persistidos al iniciar
    Status CargarIndicesAutomaticamente();
    
    Status EliminarIndice(const std::string& nombre_tabla, const std::string& nombre_columna);
    Status ReconstruirIndice(const std::string& nombre_tabla, const std::string& nombre_columna);
    Status ReconstruirTodosLosIndices();
    
    // Métodos de operaciones sobre índices
    Status InsertarEnIndice(const std::string& nombre_tabla, const std::string& nombre_columna,
                           const std::string& valor_cadena, int valor_entero, RecordId id_registro);
    Status EliminarDeIndice(const std::string& nombre_tabla, const std::string& nombre_columna,
                           const std::string& valor_cadena, int valor_entero, RecordId id_registro);
    std::optional<std::set<RecordId>> BuscarEnIndice(const std::string& nombre_tabla, 
                                                    const std::string& nombre_columna,
                                                    const std::string& valor_cadena, 
                                                    int valor_entero) const;
    
    // Métodos de consulta y información
    bool ExisteIndice(const std::string& nombre_tabla, const std::string& nombre_columna) const;
    std::vector<std::string> ObtenerIndicesDeTabla(const std::string& nombre_tabla) const;
    std::vector<std::pair<std::string, std::string>> ObtenerTodosLosIndices() const;
    TipoIndice ObtenerTipoIndice(const std::string& nombre_tabla, const std::string& nombre_columna) const;
    
    // Métodos de persistencia
    Status PersistirIndice(const std::string& nombre_tabla, const std::string& nombre_columna);
    Status PersistirTodosLosIndices();
    Status CargarIndice(const std::string& nombre_tabla, const std::string& nombre_columna);
    Status CargarTodosLosIndices();
    
    // Métodos de optimización y mantenimiento
    Status OptimizarIndice(const std::string& nombre_tabla, const std::string& nombre_columna);
    Status CompactarIndices();
    Status ValidarIntegridadIndices();
    
    // Métodos de estadísticas y depuración
    EstadisticasIndices ObtenerEstadisticas() const;
    void ImprimirInformacionIndice(const std::string& nombre_tabla, const std::string& nombre_columna) const;
    void ImprimirTodosLosIndices() const;
    void ImprimirEstadisticasGenerales() const;
    void LimpiarEstadisticas();
    
    // Métodos de depuración
    void ImprimirEstructuraIndice(const std::string& nombre_tabla, const std::string& nombre_columna) const;
    Status VerificarConsistenciaIndice(const std::string& nombre_tabla, const std::string& nombre_columna) const;
    
    // ===== MÉTODOS ADICIONALES PARA COMPATIBILIDAD CON MAIN.CPP =====
    
    /**
     * @brief Crea un índice con parámetros simplificados
     * @param nombre_tabla Nombre de la tabla
     * @param nombre_columna Nombre de la columna
     * @param tipo_str Tipo de índice como string
     * @return Status de la operación
     */
    Status CreateIndex(const std::string& nombre_tabla, const std::string& nombre_columna, const std::string& tipo_str);
    
    /**
     * @brief Elimina un índice por nombre
     * @param nombre_tabla Nombre de la tabla
     * @param nombre_columna Nombre de la columna
     * @return Status de la operación
     */
    Status DropIndex(const std::string& nombre_tabla, const std::string& nombre_columna);
    
    /**
     * @brief Busca registros usando un índice
     * @param nombre_tabla Nombre de la tabla
     * @param nombre_columna Nombre de la columna
     * @param valor Valor a buscar
     * @param resultados IDs de registros encontrados
     * @return Status de la operación
     */
    Status SearchIndex(const std::string& nombre_tabla, const std::string& nombre_columna, 
                      const std::string& valor, std::vector<RecordId>& resultados);
    
    /**
     * @brief Obtiene información de un índice
     * @param nombre_tabla Nombre de la tabla
     * @param nombre_columna Nombre de la columna
     * @return Información del índice
     */
    std::string GetIndexInfo(const std::string& nombre_tabla, const std::string& nombre_columna);
    
    /**
     * @brief Verifica si existe un índice
     * @param nombre_tabla Nombre de la tabla
     * @param nombre_columna Nombre de la columna
     * @return true si existe
     */
    bool IndexExists(const std::string& nombre_tabla, const std::string& nombre_columna);
    
    /**
     * @brief Obtiene todos los índices disponibles
     * @return Lista de índices como strings
     */
    std::vector<std::string> GetAllIndexes();
    
    /**
     * @brief Actualiza un índice con nuevos datos
     * @param nombre_tabla Nombre de la tabla
     * @param nombre_columna Nombre de la columna
     * @param valor_anterior Valor anterior
     * @param valor_nuevo Valor nuevo
     * @param id_registro ID del registro
     * @return Status de la operación
     */
    Status UpdateIndex(const std::string& nombre_tabla, const std::string& nombre_columna,
                      const std::string& valor_anterior, const std::string& valor_nuevo, RecordId id_registro);
    
    /**
     * @brief Obtiene estadísticas de un índice
     * @param nombre_tabla Nombre de la tabla
     * @param nombre_columna Nombre de la columna
     * @return Estadísticas como string
     */
    std::string GetIndexStats(const std::string& nombre_tabla, const std::string& nombre_columna);
    
    /**
     * @brief Reconstruye todos los índices de una tabla
     * @param nombre_tabla Nombre de la tabla
     * @return Status de la operación
     */
    Status RebuildTableIndexes(const std::string& nombre_tabla);
    
    /**
     * @brief Optimiza todos los índices
     * @return Status de la operación
     */
    Status OptimizeAllIndexes();
    
    /**
     * @brief Obtiene el número total de índices
     * @return Número de índices
     */
    uint32_t GetIndexCount();
    
    /**
     * @brief Valida la integridad de todos los índices
     * @return true si todos son válidos
     */
    bool ValidateAllIndexes();
};
