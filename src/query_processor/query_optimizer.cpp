#include "query_optimizer.h"
#include <algorithm>

QueryOptimizer::QueryOptimizer() {}

QueryOptimizer::~QueryOptimizer() {}

std::unique_ptr<ExecutionPlan> QueryOptimizer::Optimize(const ParsedQuery& parsed_query) {
    last_error_.clear();
    
    try {
        switch (parsed_query.type) {
            case QueryType::SELECT:
                return OptimizeSelect(static_cast<const SelectQuery&>(parsed_query));
            case QueryType::INSERT:
                return OptimizeInsert(static_cast<const InsertQuery&>(parsed_query));
            case QueryType::UPDATE:
                return OptimizeUpdate(static_cast<const UpdateQuery&>(parsed_query));
            case QueryType::DELETE:
                return OptimizeDelete(static_cast<const DeleteQuery&>(parsed_query));
            case QueryType::CREATE_TABLE:
            case QueryType::DROP_TABLE:
                // Estas operaciones se manejan directamente en el catalog manager
                // No necesitan optimización compleja
                return std::make_unique<ExecutionPlan>();
            default:
                last_error_ = "Unsupported query type for optimization";
                return nullptr;
        }
    } catch (const std::exception& e) {
        last_error_ = "Optimization error: " + std::string(e.what());
        return nullptr;
    }
}

std::unique_ptr<ExecutionPlan> QueryOptimizer::OptimizeSelect(const SelectQuery& query) {
    auto plan = std::make_unique<ExecutionPlan>();
    
    // 1. Operador de escaneo de tabla (siempre necesario)
    auto table_scan = CreateTableScanOperator(query.table_name);
    double scan_cost = EstimateTableScanCost(query.table_name);
    plan->estimated_cost += scan_cost;
    plan->operators.push_back(std::move(table_scan));
    
    // 2. Operador de filtrado si hay condiciones WHERE
    if (!query.where_conditions.empty()) {
        auto filter_op = CreateFilterOperator(query.where_conditions);
        double filter_cost = EstimateFilterCost(query.where_conditions);
        plan->estimated_cost += filter_cost;
        plan->operators.push_back(std::move(filter_op));
    }
    
    // 3. Operador de proyección si no es SELECT *
    if (!query.columns.empty() && !(query.columns.size() == 1 && query.columns[0] == "*")) {
        auto project_op = CreateProjectOperator(query.columns);
        plan->estimated_cost += 1.0; // Costo mínimo para proyección
        plan->operators.push_back(std::move(project_op));
    }
    
    return plan;
}

std::unique_ptr<ExecutionPlan> QueryOptimizer::OptimizeInsert(const InsertQuery& query) {
    auto plan = std::make_unique<ExecutionPlan>();
    
    // Para INSERT, solo necesitamos un operador de inserción
    auto insert_op = CreateInsertOperator(query.table_name, query.columns, query.values);
    plan->estimated_cost = 2.0; // Costo base para inserción (validación + escritura)
    plan->operators.push_back(std::move(insert_op));
    
    return plan;
}

std::unique_ptr<ExecutionPlan> QueryOptimizer::OptimizeUpdate(const UpdateQuery& query) {
    auto plan = std::make_unique<ExecutionPlan>();
    
    // 1. Escaneo de tabla para encontrar registros
    auto table_scan = CreateTableScanOperator(query.table_name);
    double scan_cost = EstimateTableScanCost(query.table_name);
    plan->estimated_cost += scan_cost;
    plan->operators.push_back(std::move(table_scan));
    
    // 2. Filtrado si hay condiciones WHERE
    if (!query.where_conditions.empty()) {
        auto filter_op = CreateFilterOperator(query.where_conditions);
        double filter_cost = EstimateFilterCost(query.where_conditions);
        plan->estimated_cost += filter_cost;
        plan->operators.push_back(std::move(filter_op));
    }
    
    // 3. Operación de actualización
    auto update_op = CreateUpdateOperator(query.table_name, query.set_clauses, query.where_conditions);
    plan->estimated_cost += 3.0; // Costo para actualización
    plan->operators.push_back(std::move(update_op));
    
    return plan;
}

std::unique_ptr<ExecutionPlan> QueryOptimizer::OptimizeDelete(const DeleteQuery& query) {
    auto plan = std::make_unique<ExecutionPlan>();
    
    // 1. Escaneo de tabla para encontrar registros
    auto table_scan = CreateTableScanOperator(query.table_name);
    double scan_cost = EstimateTableScanCost(query.table_name);
    plan->estimated_cost += scan_cost;
    plan->operators.push_back(std::move(table_scan));
    
    // 2. Filtrado si hay condiciones WHERE
    if (!query.where_conditions.empty()) {
        auto filter_op = CreateFilterOperator(query.where_conditions);
        double filter_cost = EstimateFilterCost(query.where_conditions);
        plan->estimated_cost += filter_cost;
        plan->operators.push_back(std::move(filter_op));
    }
    
    // 3. Operación de eliminación
    auto delete_op = CreateDeleteOperator(query.table_name, query.where_conditions);
    plan->estimated_cost += 2.0; // Costo para eliminación
    plan->operators.push_back(std::move(delete_op));
    
    return plan;
}

double QueryOptimizer::EstimateTableScanCost(const std::string& table_name) {
    // Estimación simple: asumimos un costo base de 10 unidades por tabla
    // En un optimizador real, esto consultaría estadísticas de la tabla
    return 10.0;
}

double QueryOptimizer::EstimateFilterCost(const std::vector<WhereCondition>& conditions) {
    // Costo proporcional al número de condiciones
    return static_cast<double>(conditions.size()) * 2.0;
}

bool QueryOptimizer::CanUseIndex(const std::vector<WhereCondition>& conditions) {
    // Por simplicidad, asumimos que no hay índices disponibles por ahora
    // En un sistema real, esto verificaría si existe un índice apropiado
    return false;
}

std::unique_ptr<PhysicalOperator> QueryOptimizer::CreateTableScanOperator(const std::string& table_name) {
    auto op = std::make_unique<PhysicalOperator>(PhysicalOperationType::TABLE_SCAN);
    op->table_name = table_name;
    return op;
}

std::unique_ptr<PhysicalOperator> QueryOptimizer::CreateFilterOperator(const std::vector<WhereCondition>& conditions) {
    auto op = std::make_unique<PhysicalOperator>(PhysicalOperationType::FILTER);
    op->conditions = conditions;
    return op;
}

std::unique_ptr<PhysicalOperator> QueryOptimizer::CreateProjectOperator(const std::vector<std::string>& columns) {
    auto op = std::make_unique<PhysicalOperator>(PhysicalOperationType::PROJECT);
    op->columns = columns;
    return op;
}

std::unique_ptr<PhysicalOperator> QueryOptimizer::CreateInsertOperator(const std::string& table_name, 
                                                                       const std::vector<std::string>& columns,
                                                                       const std::vector<std::string>& values) {
    auto op = std::make_unique<PhysicalOperator>(PhysicalOperationType::INSERT_OP);
    op->table_name = table_name;
    op->columns = columns;
    op->values = values;
    return op;
}

std::unique_ptr<PhysicalOperator> QueryOptimizer::CreateUpdateOperator(const std::string& table_name,
                                                                       const std::vector<std::pair<std::string, std::string>>& set_clauses,
                                                                       const std::vector<WhereCondition>& conditions) {
    auto op = std::make_unique<PhysicalOperator>(PhysicalOperationType::UPDATE_OP);
    op->table_name = table_name;
    op->set_clauses = set_clauses;
    op->conditions = conditions;
    return op;
}

std::unique_ptr<PhysicalOperator> QueryOptimizer::CreateDeleteOperator(const std::string& table_name,
                                                                       const std::vector<WhereCondition>& conditions) {
    auto op = std::make_unique<PhysicalOperator>(PhysicalOperationType::DELETE_OP);
    op->table_name = table_name;
    op->conditions = conditions;
    return op;
}