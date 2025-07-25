// catalog_manager/catalog_manager.h
#ifndef CATALOG_MANAGER_H
#define CATALOG_MANAGER_H

#include "../include/common.h"     // Para Status, PageId, ColumnType, ColumnMetadata, TableMetadata
#include "../data_storage/buffer_manager.h" // Para BufferManager
#include "../record_manager/record_manager.h" // Para RecordManager

#include <vector>                   // Para std::vector
#include <string>                   // Para std::string
#include <unordered_map>            // Para mapas internos de búsqueda rápida
#include <tuple>                    // Para std::tuple (aunque usaremos struct para ColumnMetadata)

// Estructura interna para el CatalogManager que combina TableMetadata con las columnas.
// Esta es la representación en memoria, no necesariamente la que se serializa directamente.
struct FullTableSchema {
    TableMetadata base_metadata;
    std::vector<ColumnMetadata> columns;
    // Podríamos añadir aquí una lista de PageIds que pertenecen a esta tabla,
    // pero por ahora, solo first_data_page_id es suficiente para empezar.
};

// Clase CatalogManager: Gestiona los metadatos de las tablas, columnas e índices.
// Almacena estos metadatos en CATALOG_PAGEs en el disco.
class CatalogManager {
public:
    // Constructor del CatalogManager.
    // buffer_manager: Referencia al BufferManager para interactuar con las páginas.
    // record_manager: Referencia al RecordManager para gestionar registros de metadatos.
    CatalogManager(BufferManager& buffer_manager, RecordManager& record_manager);

    // Inicializa el catálogo (crea la primera CATALOG_PAGE si no existe).
    // Esto se llamará al iniciar el SGBD.
    Status InitCatalog();

    // Crea una nueva tabla y persiste su metadata (interactivo/formulario).
    // table_name: Nombre de la tabla.
    // columns: Vector de ColumnMetadata que define el esquema de la tabla.
    // is_fixed_length_record: true si los registros de la tabla son de longitud fija.
    Status CreateTable(const std::string& table_name,
                       const std::vector<ColumnMetadata>& columns,
                       bool is_fixed_length_record);

    // Crea una nueva tabla y persiste su metadata a partir de un archivo de texto.
    // file_path: Ruta al archivo de texto que contiene el esquema y datos.
    Status CreateTableFromPath(const std::string& file_path);

    // Obtiene la metadata completa de una tabla por su nombre.
    // table_name: Nombre de la tabla a buscar.
    // schema: Referencia donde se almacenará la metadata completa (salida).
    // Retorna Status::OK si se encuentra, Status::NOT_FOUND en caso contrario.
    Status GetTableSchema(const std::string& table_name, FullTableSchema& schema);

    // Elimina una tabla y toda su metadata del catálogo.
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

private:
    BufferManager& buffer_manager_;
    RecordManager& record_manager_;

    // El PageId de la primera CATALOG_PAGE. Podríamos tener una lista de ellas si el catálogo crece mucho.
    PageId catalog_page_id_;

    // Mapa en memoria para un acceso rápido a la metadata de las tablas por nombre.
    std::unordered_map<std::string, FullTableSchema> table_schemas_;
    // Contador para asignar TableIds únicos.
    uint32_t next_table_id_;

    // Métodos auxiliares para serializar/deserializar FullTableSchema a/desde Record.
    Record SerializeTableSchema(const FullTableSchema& schema) const;
    FullTableSchema DeserializeTableSchema(const Record& record) const;

    // Método auxiliar para encontrar un slot libre o crear uno nuevo en la CATALOG_PAGE.
    Status FindOrCreateCatalogSlot(uint32_t& slot_id);
};

#endif // CATALOG_MANAGER_H
