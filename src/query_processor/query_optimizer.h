#ifndef QUERY_OPTIMIZER_H
#define QUERY_OPTIMIZER_H

#include "query_parser.h"
#include "../include/common.h"
#include <memory>
#include <vector>

// Tipos de operaciones físicas
enum class PhysicalOperationType {
    TABLE_SCAN,     // Escaneo completo de tabla
    INDEX_SCAN,     // Escaneo usando índice
    FILTER,         // Filtrado con condiciones WHERE
    PROJECT,        // Proyección de columnas
    INSERT_OP,      // Operación de inserción
    UPDATE_OP,      // Operación de actualización
    DELETE_OP       // Operación de eliminación
};

// Estructura para representar un operador físico
struct PhysicalOperator {
    PhysicalOperationType type;
    std::string table_name;
    std::vector<std::string> columns;
    std::vector<WhereCondition> conditions;
    std::vector<std::string> values;
    std::vector<std::pair<std::string, std::string>> set_clauses;
    
    PhysicalOperator(PhysicalOperationType t) : type(t) {}
    virtual ~PhysicalOperator() = default;
};

// Plan de ejecución - secuencia de operadores físicos
struct ExecutionPlan {
    std::vector<std::unique_ptr<PhysicalOperator>> operators;
    double estimated_cost;
    
    ExecutionPlan() : estimated_cost(0.0) {}
};

// Clase principal del optimizador
class QueryOptimizer {
public:
    QueryOptimizer();
    ~QueryOptimizer();
    
    // Optimiza una consulta parseada y genera un plan de ejecución
    std::unique_ptr<ExecutionPlan> Optimize(const ParsedQuery& parsed_query);
    
    // Obtiene el último error de optimización
    std::string GetLastError() const { return last_error_; }
    
private:
    std::string last_error_;
    
    // Métodos para generar planes para diferentes tipos de consulta
    std::unique_ptr<ExecutionPlan> OptimizeSelect(const SelectQuery& query);
    std::unique_ptr<ExecutionPlan> OptimizeInsert(const InsertQuery& query);
    std::unique_ptr<ExecutionPlan> OptimizeUpdate(const UpdateQuery& query);
    std::unique_ptr<ExecutionPlan> OptimizeDelete(const DeleteQuery& query);
    
    // Métodos auxiliares para optimización
    double EstimateTableScanCost(const std::string& table_name);
    double EstimateFilterCost(const std::vector<WhereCondition>& conditions);
    bool CanUseIndex(const std::vector<WhereCondition>& conditions);
    
    // Crea operadores físicos específicos
    std::unique_ptr<PhysicalOperator> CreateTableScanOperator(const std::string& table_name);
    std::unique_ptr<PhysicalOperator> CreateFilterOperator(const std::vector<WhereCondition>& conditions);
    std::unique_ptr<PhysicalOperator> CreateProjectOperator(const std::vector<std::string>& columns);
    std::unique_ptr<PhysicalOperator> CreateInsertOperator(const std::string& table_name, 
                                                           const std::vector<std::string>& columns,
                                                           const std::vector<std::string>& values);
    std::unique_ptr<PhysicalOperator> CreateUpdateOperator(const std::string& table_name,
                                                           const std::vector<std::pair<std::string, std::string>>& set_clauses,
                                                           const std::vector<WhereCondition>& conditions);
    std::unique_ptr<PhysicalOperator> CreateDeleteOperator(const std::string& table_name,
                                                           const std::vector<WhereCondition>& conditions);
};

#endif // QUERY_OPTIMIZER_H