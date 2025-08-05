// Catalog_Manager/gestor_catalogo.h
#ifndef GESTOR_CATALOGO_H
#define GESTOR_CATALOGO_H

#include "../include/common.h" // Para ColumnType, ColumnMetadata, Status, BloqueMemoria, BlockId
#include "../data_storage/gestor_disco.h" // Para GestorDisco
#include <string>              // Para std::string
#include <vector>              // Para std::vector
#include <unordered_map>       // Para std::unordered_map
#include <memory>              // Para std::shared_ptr
#include <fstream>             // Para std::ifstream, std::ofstream
#include <chrono>              // Para std::chrono::system_clock
#include <ctime>               // Para std::localtime, std::put_time
#include <iomanip>             // Para std::put_time

/**
 * Clase base MetadataTabla: Contiene la información común de todas las tablas
 * * Responsabilidades principales:
 * - Almacenar información básica de la tabla (nombre, ID, esquema)
 * - Gestionar el número de registros
 * - Proporcionar interfaz común para clases especializadas
 */
class MetadataTabla {
public:
    /**
     * Constructor de MetadataTabla
     * @param nombre_tabla Nombre de la tabla
     * @param id_tabla Identificador único de la tabla
     */
    MetadataTabla(const std::string& nombre_tabla, uint32_t id_tabla);

    /**
     * Destructor virtual para permitir herencia
     */
    virtual ~MetadataTabla() = default;

    // === MÉTODOS DE GESTIÓN DE ESQUEMA ===

    /**
     * Añade una columna al esquema de la tabla
     * @param nombre_columna Nombre de la columna
     * @param tipo_columna Tipo de dato de la columna
     * @param tamaño_columna Tamaño de la columna (para CHAR/VARCHAR)
     * @return Status::OK si se añadió correctamente
     */
    Status AñadirColumna(const std::string& nombre_columna, ColumnType tipo_columna, uint32_t tamaño_columna = 0);

    /**
     * Obtiene el esquema completo de la tabla
     * @return Vector con metadata de todas las columnas
     */
    const std::vector<ColumnMetadata>& ObtenerEsquema() const { return esquema_tabla_; }

    /**
     * Busca una columna por nombre
     * @param nombre_columna Nombre de la columna a buscar
     * @return Índice de la columna, o -1 si no se encuentra
     */
    int32_t BuscarColumna(const std::string& nombre_columna) const;

    // === MÉTODOS DE GESTIÓN DE REGISTROS ===

    /**
     * Incrementa el contador de registros
     */
    void IncrementarNumeroRegistros() { numero_registros_++; }

    /**
     * Decrementa el contador de registros
     */
    void DecrementarNumeroRegistros() { 
        if (numero_registros_ > 0) numero_registros_--; 
    }

    /**
     * Establece el número de registros
     * @param num_registros Nuevo número de registros
     */
    void EstablecerNumeroRegistros(uint32_t num_registros) { numero_registros_ = num_registros; }

    // === MÉTODOS DE CONSULTA ===

    std::string ObtenerNombreTabla() const { return nombre_tabla_; }
    uint32_t ObtenerIdTabla() const { return id_tabla_; }
    uint32_t ObtenerNumeroRegistros() const { return numero_registros_; }
    uint32_t ObtenerNumeroColumnas() const { return static_cast<uint32_t>(esquema_tabla_.size()); }

    // === MÉTODOS VIRTUALES PUROS ===

    /**
     * Calcula el tamaño de un registro (implementación específica por tipo)
     * @return Tamaño del registro en bytes
     */
    virtual uint32_t CalcularTamañoRegistro() const = 0;

    /**
     * Determina si la tabla maneja registros de longitud fija
     * @return true si es longitud fija, false si es variable
     */
    virtual bool EsLongitudFija() const = 0;

    /**
     * Serializa la metadata específica a texto
     * @return String con la metadata serializada
     */
    virtual std::string SerializarMetadataEspecifica() const = 0;

    /**
     * Deserializa la metadata específica desde texto
     * @param contenido Contenido a deserializar
     * @return Status::OK si se deserializó correctamente
     */
    virtual Status DeserializarMetadataEspecifica(const std::string& contenido) = 0;

    // === MÉTODOS DE SERIALIZACIÓN COMÚN ===

    /**
     * Serializa la metadata común a texto
     * @return String con la metadata común serializada
     */
    std::string SerializarMetadataComun() const;

    /**
     * Deserializa la metadata común desde texto
     * @param contenido Contenido a deserializar
     * @return Status::OK si se deserializó correctamente
     */
    Status DeserializarMetadataComun(const std::string& contenido);

protected:
    std::string nombre_tabla_;                    // Nombre de la tabla
    uint32_t id_tabla_;                          // Identificador único de la tabla
    std::vector<ColumnMetadata> esquema_tabla_;  // Esquema de la tabla (columnas)
    uint32_t numero_registros_;                  // Número de registros en la tabla

    /**
     * Valida que un tipo de columna sea compatible con el tipo de tabla
     * @param tipo_columna Tipo de columna a validar
     * @return true si es compatible, false en caso contrario
     */
    virtual bool ValidarTipoColumna(ColumnType tipo_columna) const;
};

/**
 * Clase MetadataTablaLongitudFija: Especialización para tablas de longitud fija
 * * Características específicas:
 * - Todos los registros tienen el mismo tamaño
 * - Almacena el tamaño completo del registro
 * - Almacena los tamaños individuales de cada campo
 * - Optimizada para acceso directo por posición
 */
class MetadataTablaLongitudFija : public MetadataTabla {
public:
    /**
     * Constructor para tabla de longitud fija
     * @param nombre_tabla Nombre de la tabla
     * @param id_tabla Identificador único de la tabla
     */
    MetadataTablaLongitudFija(const std::string& nombre_tabla, uint32_t id_tabla);

    /**
     * Destructor
     */
    ~MetadataTablaLongitudFija() override = default;

    // === MÉTODOS ESPECÍFICOS DE LONGITUD FIJA ===

    /**
     * Obtiene el tamaño de un campo específico
     * @param indice_columna Índice de la columna
     * @return Tamaño del campo en bytes
     */
    uint32_t ObtenerTamañoCampo(uint32_t indice_columna) const;

    /**
     * Obtiene el offset de un campo específico dentro del registro
     * @param indice_columna Índice de la columna
     * @return Offset del campo en bytes
     */
    uint32_t ObtenerOffsetCampo(uint32_t indice_columna) const;

    /**
     * Recalcula los tamaños y offsets después de modificar el esquema
     */
    void RecalcularTamaños();

    // === IMPLEMENTACIÓN DE MÉTODOS VIRTUALES ===

    uint32_t CalcularTamañoRegistro() const override { return tamaño_registro_completo_; }
    bool EsLongitudFija() const override { return true; }
    std::string SerializarMetadataEspecifica() const override;
    Status DeserializarMetadataEspecifica(const std::string& contenido) override;

    // === MÉTODOS DE CONSULTA ESPECÍFICOS ===

    const std::vector<uint32_t>& ObtenerTamañosCampos() const { return tamaños_campos_; }
    uint32_t ObtenerTamañoRegistroCompleto() const { return tamaño_registro_completo_; }

private:
    uint32_t tamaño_registro_completo_;          // Tamaño total de un registro fijo
    std::vector<uint32_t> tamaños_campos_;       // Tamaños individuales de cada campo
    std::vector<uint32_t> offsets_campos_;       // Offsets de cada campo dentro del registro

    /**
     * Calcula el tamaño de un campo según su tipo
     * @param metadata Metadata de la columna
     * @return Tamaño del campo en bytes
     */
    uint32_t CalcularTamañoCampo(const ColumnMetadata& metadata) const;
};

/**
 * Clase MetadataTablaLongitudVariable: Especialización para tablas de longitud variable
 * * Características específicas:
 * - Los registros pueden tener diferentes tamaños
 * - Almacena índices de valores mínimo y máximo de caracteres
 * - Calcula áreas seguras sin delimitadores para optimizar el parseo
 * - Optimizada para flexibilidad en el almacenamiento
 */
class MetadataTablaLongitudVariable : public MetadataTabla {
public:
    /**
     * Constructor para tabla de longitud variable
     * @param nombre_tabla Nombre de la tabla
     * @param id_tabla Identificador único de la tabla
     */
    MetadataTablaLongitudVariable(const std::string& nombre_tabla, uint32_t id_tabla);

    /**
     * Destructor
     */
    ~MetadataTablaLongitudVariable() override = default;

    // === MÉTODOS ESPECÍFICOS DE LONGITUD VARIABLE ===

    /**
     * Actualiza los índices de longitud mínima y máxima para una columna
     * @param indice_columna Índice de la columna
     * @param longitud_valor Longitud del valor a considerar
     */
    void ActualizarIndicesLongitud(uint32_t indice_columna, uint32_t longitud_valor);

    /**
     * Obtiene la longitud mínima registrada para una columna
     * @param indice_columna Índice de la columna
     * @return Longitud mínima en caracteres
     */
    uint32_t ObtenerLongitudMinima(uint32_t indice_columna) const;

    /**
     * Obtiene la longitud máxima registrada para una columna
     * @param indice_columna Índice de la columna
     * @return Longitud máxima en caracteres
     */
    uint32_t ObtenerLongitudMaxima(uint32_t indice_columna) const;

    /**
     * Calcula el área segura para parseo (sin delimitadores)
     * @param indice_columna Índice de la columna
     * @return Par con inicio y fin del área segura
     */
    std::pair<uint32_t, uint32_t> CalcularAreaSegura(uint32_t indice_columna) const;

    /**
     * Estima el tamaño promedio de un registro basado en estadísticas
     * @return Tamaño estimado en bytes
     */
    uint32_t EstimarTamañoPromedio() const;

    // === IMPLEMENTACIÓN DE MÉTODOS VIRTUALES ===

    uint32_t CalcularTamañoRegistro() const override;
    bool EsLongitudFija() const override { return false; }
    std::string SerializarMetadataEspecifica() const override;
    Status DeserializarMetadataEspecifica(const std::string& contenido) override;

    // === MÉTODOS DE CONSULTA ESPECÍFICOS ===

    const std::vector<uint32_t>& ObtenerLongitudesMinimas() const { return longitudes_minimas_; }
    const std::vector<uint32_t>& ObtenerLongitudesMaximas() const { return longitudes_maximas_; }

private:
    std::vector<uint32_t> longitudes_minimas_;   // Longitudes mínimas por columna
    std::vector<uint32_t> longitudes_maximas_;   // Longitudes máximas por columna
    uint32_t tamaño_estimado_promedio_;          // Tamaño estimado promedio de registro

    /**
     * Inicializa los vectores de longitudes para una nueva columna
     */
    void InicializarLongitudesColumna();

    /**
     * Recalcula el tamaño estimado promedio
     */
    void RecalcularTamañoEstimado();
};

/**
 * Clase GestorCatalogo: Gestiona todas las tablas y sus metadatos
 * * Responsabilidades principales:
 * - Crear, modificar y eliminar tablas
 * - Persistir metadatos en bloques específicos del disco
 * - Gestionar parámetros del disco
 * - Proporcionar acceso rápido a metadatos de tablas
 */
class GestorCatalogo {
public:
    /**
     * Constructor del GestorCatalogo
     * @param gestor_disco Puntero al gestor de disco
     */
    explicit GestorCatalogo(std::shared_ptr<GestorDisco> gestor_disco);

    /**
     * Destructor
     */
    ~GestorCatalogo();

    // === MÉTODOS DE GESTIÓN DE TABLAS ===

    /**
     * Crea una nueva tabla de longitud fija
     * @param nombre_tabla Nombre de la tabla
     * @param esquema Esquema de columnas de la tabla
     * @return ID de la tabla creada, o 0 si hubo error
     */
    uint32_t CrearTablaLongitudFija(const std::string& nombre_tabla, 
                                   const std::vector<ColumnMetadata>& esquema);

    /**
     * Crea una nueva tabla de longitud variable
     * @param nombre_tabla Nombre de la tabla
     * @param esquema Esquema de columnas de la tabla
     * @return ID de la tabla creada, o 0 si hubo error
     */
    uint32_t CrearTablaLongitudVariable(const std::string& nombre_tabla, 
                                       const std::vector<ColumnMetadata>& esquema);

    /**
     * Elimina una tabla del catálogo
     * @param nombre_tabla Nombre de la tabla a eliminar
     * @return Status::OK si se eliminó correctamente
     */
    Status EliminarTabla(const std::string& nombre_tabla); // Cambiado de id_tabla a nombre_tabla

    /**
     * Busca una tabla por nombre
     * @param nombre_tabla Nombre de la tabla a buscar
     * @return Puntero a la metadata de la tabla, o nullptr si no existe
     */
    std::shared_ptr<MetadataTabla> BuscarTablaPorNombre(const std::string& nombre_tabla);

    /**
     * Busca una tabla por ID
     * @param id_tabla ID de la tabla a buscar
     * @return Puntero a la metadata de la tabla, o nullptr si no existe
     */
    std::shared_ptr<MetadataTabla> BuscarTablaPorId(uint32_t id_tabla);

    // === MÉTODOS DE PERSISTENCIA ===

    /**
     * Guarda el catálogo completo en bloques específicos del disco
     * @return Status::OK si se guardó correctamente
     */
    Status GuardarCatalogo();

    /**
     * Carga el catálogo desde bloques específicos del disco
     * @return Status::OK si se cargó correctamente
     */
    Status CargarCatalogo();

    // === MÉTODOS DE CONSULTA ===

    /**
     * Lista todas las tablas existentes
     * @return Vector con nombres de todas las tablas
     */
    std::vector<std::string> ListarTablas() const;

    /**
     * Obtiene estadísticas del catálogo
     * @return String con información estadística
     */
    std::string ObtenerEstadisticas() const; // Implementación simple por ahora

    /**
     * Obtiene el número total de tablas
     * @return Número de tablas en el catálogo
     */
    uint32_t ObtenerNumeroTablas() const { return static_cast<uint32_t>(tablas_.size()); }

    // === MÉTODOS DE DEPURACIÓN ===

    /**
     * Imprime información detallada de todas las tablas
     */
    void ImprimirCatalogoCompleto() const;

    /**
     * Imprime información de una tabla específica
     * @param id_tabla ID de la tabla a imprimir
     */
    void ImprimirInformacionTabla(uint32_t id_tabla) const;
    
    // ===== MÉTODOS ADICIONALES PARA COMPATIBILIDAD CON MAIN.CPP =====
    // Nota: Algunos de estos métodos podrían ser más apropiados en un gestor de nivel superior
    // como GestorTablasAvanzado, pero se implementan aquí para satisfacer la interfaz.
    
    /**
     * @brief Crea una tabla desde un archivo (Placeholder, la lógica real estaría en GestorTablasAvanzado)
     * @param nombre_tabla Nombre de la tabla
     * @param ruta_archivo Ruta del archivo con datos
     * @return Status de la operación
     */
    Status CreateTable(const std::string& nombre_tabla, const std::string& ruta_archivo);
    
    /**
     * @brief Crea una tabla con esquema manual (Placeholder, la lógica real estaría en GestorTablasAvanzado)
     * @param nombre_tabla Nombre de la tabla
     * @param columnas Definición de columnas
     * @param tipos Tipos de las columnas
     * @return Status de la operación
     */
    Status CreateTableWithSchema(const std::string& nombre_tabla, 
                                 const std::vector<std::string>& columnas,
                                 const std::vector<std::string>& tipos);
    
    /**
     * @brief Elimina una tabla por nombre (Delega en EliminarTabla)
     * @param nombre_tabla Nombre de la tabla
     * @return Status de la operación
     */
    Status DropTable(const std::string& nombre_tabla);
    
    /**
     * @brief Verifica si una tabla existe (Delega en BuscarTablaPorNombre)
     * @param nombre_tabla Nombre de la tabla
     * @return true si existe
     */
    bool TableExists(const std::string& nombre_tabla);
    
    /**
     * @brief Obtiene información de una tabla (Delega en BuscarTablaPorNombre y formatea)
     * @param nombre_tabla Nombre de la tabla
     * @return Información de la tabla
     */
    std::string GetTableInfo(const std::string& nombre_tabla);
    
    /**
     * @brief Obtiene todas las tablas disponibles (Delega en ListarTablas)
     * @return Lista de nombres de tablas
     */
    std::vector<std::string> GetAllTables();
    
    /**
     * @brief Obtiene el esquema de una tabla (Delega en ObtenerEsquema)
     * @param nombre_tabla Nombre de la tabla
     * @return Esquema de la tabla como pares de nombre-tipo
     */
    std::vector<std::pair<std::string, std::string>> GetTableSchema(const std::string& nombre_tabla);
    
    /**
     * @brief Obtiene el ID de una tabla por nombre (Delega en indices_nombres_)
     * @param nombre_tabla Nombre de la tabla
     * @return ID de la tabla o 0 si no existe
     */
    uint32_t GetTableId(const std::string& nombre_tabla);
    
    /**
     * @brief Obtiene el nombre de una tabla por ID (Delega en tablas_)
     * @param id_tabla ID de la tabla
     * @return Nombre de la tabla
     */
    std::string GetTableName(uint32_t id_tabla);
    
    /**
     * @brief Actualiza el número de registros de una tabla (Delega en EstablecerNumeroRegistros)
     * @param nombre_tabla Nombre de la tabla
     * @param nuevo_numero Nuevo número de registros
     * @return Status de la operación
     */
    Status UpdateRecordCount(const std::string& nombre_tabla, uint32_t nuevo_numero);
    
    /**
     * @brief Obtiene el número de registros de una tabla (Delega en ObtenerNumeroRegistros)
     * @param nombre_tabla Nombre de la tabla
     * @return Número de registros
     */
    uint32_t GetRecordCount(const std::string& nombre_tabla);
    
    /**
     * @brief Deserializa una tabla individual desde texto
     * @param contenido Contenido serializado
     * @return Status::OK si se deserializó correctamente y se añadió al catálogo
     */
    Status DeserializarTablaIndividual(const std::string& contenido); // Cambiado de shared_ptr a Status
    
    /**
     * @brief Obtiene timestamp actual como string formateado
     * @return Timestamp en formato "YYYY-MM-DD HH:MM:SS"
     */
    std::string ObtenerTimestampActual() const;

private:
    std::shared_ptr<GestorDisco> gestor_disco_;                              // Gestor de disco
    std::unordered_map<uint32_t, std::shared_ptr<MetadataTabla>> tablas_;   // Mapa de tablas por ID
    std::unordered_map<std::string, uint32_t> indices_nombres_;             // Índice de nombres a IDs
    uint32_t siguiente_id_tabla_;                                           // Próximo ID de tabla disponible
    BlockId bloque_catalogo_;                                               // ID del bloque que contiene el catálogo

    /**
     * Genera un nuevo ID único para una tabla
     * @return Nuevo ID de tabla
     */
    uint32_t GenerarNuevoIdTabla() { return siguiente_id_tabla_++; }

    /**
     * Valida que un nombre de tabla sea válido
     * @param nombre_tabla Nombre a validar
     * @return true si es válido, false en caso contrario
     */
    bool ValidarNombreTabla(const std::string& nombre_tabla) const;

    /**
     * Serializa el catálogo completo a texto
     * @return String con el catálogo serializado
     */
    std::string SerializarCatalogo() const;

    /**
     * Deserializa el catálogo desde texto
     * @param contenido Contenido a deserializar
     * @return Status::OK si se deserializó correctamente
     */
    Status DeserializarCatalogo(const std::string& contenido);
};

#endif // GESTOR_CATALOGO_H
