// catalog_manager/catalog_manager.h
#ifndef CATALOG_MANAGER_H
#define CATALOG_MANAGER_H

#include "../include/common.h"     // Para Status, PageId, ColumnType, ColumnMetadata, TableMetadata
#include "../data_storage/buffer_manager.h" // Para BufferManager

// Forward declarations para resolver dependencias circulares
class RecordManager; 

// Incluir record_manager.h para obtener la definición completa de Record struct
// Esto es necesario porque SerializeTableSchema y DeserializeTableSchema usan Record por valor/referencia.
#include "../record_manager/record_manager.h" 

// Estructura interna para el CatalogManager que combina TableMetadata con las columnas.
// Esta es la representación en memoria, no necesariamente la que se serializa directamente.
struct FullTableSchema {
    TableMetadata base_metadata;
    std::vector<ColumnMetadata> columns;
};

// Clase CatalogManager: Gestiona los metadatos de las tablas, columnas e índices.
// Almacena estos metadatos en CATALOG_PAGEs en el disco.
class CatalogManager {
public:
    // Constructor del CatalogManager.
    // buffer_manager: Referencia al BufferManager para interactuar con las páginas.
    // NOTA: Se ha simplificado el constructor para romper la dependencia circular.
    CatalogManager(BufferManager& buffer_manager);

    // Método setter para el RecordManager (para resolver la dependencia circular)
    void SetRecordManager(RecordManager& record_manager);

    // Inicializa el catálogo (crea la primera CATALOG_PAGE si no existe).
    Status InitCatalog();

    // Crea una nueva tabla y la registra en el catálogo.
    // table_name: Nombre de la nueva tabla.
    // columns: Vector de metadatos de las columnas de la tabla.
    // is_fixed_length_record: true si los registros de esta tabla son de longitud fija.
    Status CreateTable(const std::string& table_name, const std::vector<ColumnMetadata>& columns, bool is_fixed_length_record);

    // Crea una nueva tabla a partir de un archivo de texto (esquema inferido).
    // file_path: Ruta al archivo de texto con los datos.
    Status CreateTableFromPath(const std::string& file_path);

    // Obtiene el esquema completo de una tabla.
    // table_name: Nombre de la tabla a buscar.
    // schema: Referencia a un FullTableSchema donde se copiará el esquema (salida).
    Status GetTableSchema(const std::string& table_name, FullTableSchema& schema);

    // Elimina una tabla del catálogo.
    // También debe liberar todas las páginas de datos asociadas a la tabla.
    // table_name: Nombre de la tabla a eliminar.
    Status DropTable(const std::string& table_name);

    // Lista todas las tablas registradas en el catálogo.
    // table_names: Vector de strings donde se almacenarán los nombres de las tablas (salida).
    Status ListTables(std::vector<std::string>& table_names);

    // Método para cargar todo el catálogo desde las CATALOG_PAGEs.
    Status LoadCatalog();

    // Método para guardar todo el catálogo en las CATALOG_PAGEs.
    Status SaveCatalog();

    // Método para añadir una nueva PageId de datos a una tabla existente.
    // Esto es crucial para la gestión de tablas multi-página.
    Status AddDataPageToTable(const std::string& table_name, PageId new_data_page_id);

    // NUEVO: Método para actualizar el número de registros de una tabla.
    // table_name: Nombre de la tabla a actualizar.
    // new_num_records: El nuevo conteo de registros para la tabla.
    Status UpdateTableNumRecords(const std::string& table_name, uint32_t new_num_records);

private:
    BufferManager& buffer_manager_;
    RecordManager* record_manager_; // Puntero al gestor de registros (para romper la dependencia circular)

    // El PageId de la primera CATALOG_PAGE. Podríamos tener una lista de ellas si el catálogo crece mucho.
    PageId catalog_page_id_;

    // Mapa en memoria para un acceso rápido a la metadata de las tablas por nombre.
    std::unordered_map<std::string, FullTableSchema> table_schemas_;
    // Contador para asignar TableIds únicos.
    uint32_t next_table_id_;

    // Métodos auxiliares para serializar/deserializar FullTableSchema a/desde Record.
    Record SerializeTableSchema(const FullTableSchema& schema) const;
    FullTableSchema DeserializeTableSchema(const Record& record) const;
};

#endif // CATALOG_MANAGER_H
