#ifndef QUERY_EXECUTOR_H
#define QUERY_EXECUTOR_H

#include "query_optimizer.h"
#include "../Catalog_Manager/Catalog_Manager.h"
#include "../record_manager/record_manager.h"
#include "../include/common.h"
#include <memory>
#include <vector>

// Estructura para representar el resultado de una consulta
struct QueryResult {
    std::vector<std::vector<std::string>> rows;  // Filas del resultado
    std::vector<std::string> column_names;       // Nombres de las columnas
    bool success;                                // Indica si la consulta fue exitosa
    std::string error_message;                   // Mensaje de error si la consulta falló
    size_t affected_rows;                        // Número de filas afectadas (para INSERT, UPDATE, DELETE)
    
    QueryResult() : success(false), affected_rows(0) {}
};

// Clase principal del ejecutor de consultas
class QueryExecutor {
public:
    // Constructor del QueryExecutor
    QueryExecutor(CatalogManager& catalog_manager, RecordManager& record_manager);
    ~QueryExecutor();
    
    // Ejecuta un plan de consulta y retorna el resultado
    std::unique_ptr<QueryResult> Execute(const ExecutionPlan& plan);
    
    // Ejecuta directamente una consulta parseada (para CREATE/DROP TABLE)
    std::unique_ptr<QueryResult> ExecuteDDL(const ParsedQuery& parsed_query);
    
    // Obtiene el último error de ejecución
    std::string GetLastError() const { return last_error_; }
    
private:
    CatalogManager& catalog_manager_;
    RecordManager& record_manager_;
    std::string last_error_;
    
    // Contexto de ejecución para mantener estado entre operadores
    struct ExecutionContext {
        std::vector<std::vector<std::string>> current_rows;
        std::vector<std::string> current_columns;
        std::string current_table;
        FullTableSchema current_schema;
    };
    
    // Métodos para ejecutar operadores específicos
    Status ExecuteTableScan(const PhysicalOperator& op, ExecutionContext& context);
    Status ExecuteFilter(const PhysicalOperator& op, ExecutionContext& context);
    Status ExecuteProject(const PhysicalOperator& op, ExecutionContext& context);
    Status ExecuteInsert(const PhysicalOperator& op, ExecutionContext& context);
    Status ExecuteUpdate(const PhysicalOperator& op, ExecutionContext& context);
    Status ExecuteDelete(const PhysicalOperator& op, ExecutionContext& context);
    
    // Métodos auxiliares
    bool EvaluateCondition(const WhereCondition& condition, const std::vector<std::string>& row,
                          const std::vector<std::string>& column_names);
    std::vector<std::string> ParseRecord(const Record& record, const FullTableSchema& schema);
    Record CreateRecord(const std::vector<std::string>& values, const FullTableSchema& schema);
    int CompareValues(const std::string& value1, const std::string& value2, ColumnType type);
    ColumnType GetColumnType(const std::string& column_name, const FullTableSchema& schema);
    
    // Métodos para DDL (Data Definition Language)
    std::unique_ptr<QueryResult> ExecuteCreateTable(const CreateTableQuery& query);
    std::unique_ptr<QueryResult> ExecuteDropTable(const DropTableQuery& query);
};

#endif // QUERY_EXECUTOR_H