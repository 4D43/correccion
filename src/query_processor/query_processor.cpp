#include "query_processor.h"
#include <chrono>
#include <iostream>

QueryProcessor::QueryProcessor(CatalogManager& catalog_manager, RecordManager& record_manager)
    : catalog_manager_(catalog_manager), record_manager_(record_manager), verbose_mode_(false) {
    
    parser_ = std::make_unique<QueryParser>();
    optimizer_ = std::make_unique<QueryOptimizer>();
    executor_ = std::make_unique<QueryExecutor>(catalog_manager_, record_manager_);
    
    ResetStats();
}

QueryProcessor::~QueryProcessor() {}

std::unique_ptr<QueryResult> QueryProcessor::ProcessQuery(const std::string& sql) {
    ResetStats();
    last_error_.clear();
    
    double start_time = GetCurrentTimeMs();
    
    LogVerbose("Starting query processing: " + sql);
    
    // Fase 1: Parsing
    double parse_start = GetCurrentTimeMs();
    last_parsed_query_ = parser_->Parse(sql);
    last_stats_.parse_time_ms = GetCurrentTimeMs() - parse_start;
    
    if (!last_parsed_query_) {
        last_error_ = "Parse error: " + parser_->GetLastError();
        auto result = std::make_unique<QueryResult>();
        result->error_message = last_error_;
        return result;
    }
    
    LogVerbose("Parsing completed successfully");
    
    // Verificar si es una consulta DDL (CREATE/DROP TABLE)
    if (last_parsed_query_->type == QueryType::CREATE_TABLE || 
        last_parsed_query_->type == QueryType::DROP_TABLE) {
        
        LogVerbose("Processing DDL query directly");
        double exec_start = GetCurrentTimeMs();
        auto result = executor_->ExecuteDDL(*last_parsed_query_);
        last_stats_.execution_time_ms = GetCurrentTimeMs() - exec_start;
        last_stats_.total_time_ms = GetCurrentTimeMs() - start_time;
        
        if (!result->success) {
            last_error_ = "DDL execution error: " + executor_->GetLastError();
        }
        
        return result;
    }
    
    // Fase 2: Optimización
    double opt_start = GetCurrentTimeMs();
    last_execution_plan_ = optimizer_->Optimize(*last_parsed_query_);
    last_stats_.optimization_time_ms = GetCurrentTimeMs() - opt_start;
    
    if (!last_execution_plan_) {
        last_error_ = "Optimization error: " + optimizer_->GetLastError();
        auto result = std::make_unique<QueryResult>();
        result->error_message = last_error_;
        return result;
    }
    
    last_stats_.estimated_cost = last_execution_plan_->estimated_cost;
    last_stats_.plan_operators_count = last_execution_plan_->operators.size();
    
    LogVerbose("Optimization completed. Estimated cost: " + std::to_string(last_stats_.estimated_cost));
    
    // Fase 3: Ejecución
    double exec_start = GetCurrentTimeMs();
    auto result = executor_->Execute(*last_execution_plan_);
    last_stats_.execution_time_ms = GetCurrentTimeMs() - exec_start;
    last_stats_.total_time_ms = GetCurrentTimeMs() - start_time;
    
    if (!result->success) {
        last_error_ = "Execution error: " + executor_->GetLastError();
    } else {
        LogVerbose("Query executed successfully. Rows affected/returned: " + 
                  std::to_string(result->affected_rows));
    }
    
    LogVerbose("Total processing time: " + std::to_string(last_stats_.total_time_ms) + "ms");
    
    return result;
}

std::string QueryProcessor::GetLastParsedQueryInfo() const {
    if (!last_parsed_query_) {
        return "No query parsed yet";
    }
    
    std::string info = "Parsed Query Information:\n";
    info += "  Type: " + QueryTypeToString(last_parsed_query_->type) + "\n";
    info += "  Table: " + last_parsed_query_->table_name + "\n";
    
    // Información específica según el tipo de consulta
    switch (last_parsed_query_->type) {
        case QueryType::SELECT: {
            const SelectQuery* select_query = static_cast<const SelectQuery*>(last_parsed_query_.get());
            info += "  Columns: ";
            for (size_t i = 0; i < select_query->columns.size(); ++i) {
                if (i > 0) info += ", ";
                info += select_query->columns[i];
            }
            info += "\n";
            info += "  WHERE conditions: " + std::to_string(select_query->where_conditions.size()) + "\n";
            break;
        }
        case QueryType::INSERT: {
            const InsertQuery* insert_query = static_cast<const InsertQuery*>(last_parsed_query_.get());
            info += "  Values count: " + std::to_string(insert_query->values.size()) + "\n";
            break;
        }
        case QueryType::UPDATE: {
            const UpdateQuery* update_query = static_cast<const UpdateQuery*>(last_parsed_query_.get());
            info += "  SET clauses: " + std::to_string(update_query->set_clauses.size()) + "\n";
            info += "  WHERE conditions: " + std::to_string(update_query->where_conditions.size()) + "\n";
            break;
        }
        case QueryType::DELETE: {
            const DeleteQuery* delete_query = static_cast<const DeleteQuery*>(last_parsed_query_.get());
            info += "  WHERE conditions: " + std::to_string(delete_query->where_conditions.size()) + "\n";
            break;
        }
        default:
            break;
    }
    
    return info;
}

std::string QueryProcessor::GetLastExecutionPlanInfo() const {
    if (!last_execution_plan_) {
        return "No execution plan generated yet";
    }
    
    std::string info = "Execution Plan Information:\n";
    info += "  Estimated Cost: " + std::to_string(last_execution_plan_->estimated_cost) + "\n";
    info += "  Number of Operators: " + std::to_string(last_execution_plan_->operators.size()) + "\n";
    info += "  Operators:\n";
    
    for (size_t i = 0; i < last_execution_plan_->operators.size(); ++i) {
        const auto& op = last_execution_plan_->operators[i];
        info += "    " + std::to_string(i + 1) + ". " + 
                PhysicalOperationTypeToString(op->type);
        
        if (!op->table_name.empty()) {
            info += " on table '" + op->table_name + "'";
        }
        
        if (!op->conditions.empty()) {
            info += " with " + std::to_string(op->conditions.size()) + " condition(s)";
        }
        
        if (!op->columns.empty()) {
            info += " projecting " + std::to_string(op->columns.size()) + " column(s)";
        }
        
        info += "\n";
    }
    
    return info;
}

void QueryProcessor::ResetStats() {
    last_stats_ = ProcessingStats();
}

void QueryProcessor::LogVerbose(const std::string& message) const {
    if (verbose_mode_) {
        std::cout << "[QueryProcessor] " << message << std::endl;
    }
}

double QueryProcessor::GetCurrentTimeMs() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count() / 1000.0;
}

std::string QueryProcessor::QueryTypeToString(QueryType type) const {
    switch (type) {
        case QueryType::SELECT: return "SELECT";
        case QueryType::INSERT: return "INSERT";
        case QueryType::UPDATE: return "UPDATE";
        case QueryType::DELETE: return "DELETE";
        case QueryType::CREATE_TABLE: return "CREATE TABLE";
        case QueryType::DROP_TABLE: return "DROP TABLE";
        default: return "UNKNOWN";
    }
}

std::string QueryProcessor::PhysicalOperationTypeToString(PhysicalOperationType type) const {
    switch (type) {
        case PhysicalOperationType::TABLE_SCAN: return "Table Scan";
        case PhysicalOperationType::INDEX_SCAN: return "Index Scan";
        case PhysicalOperationType::FILTER: return "Filter";
        case PhysicalOperationType::PROJECT: return "Project";
        case PhysicalOperationType::INSERT_OP: return "Insert";
        case PhysicalOperationType::UPDATE_OP: return "Update";
        case PhysicalOperationType::DELETE_OP: return "Delete";
        default: return "Unknown Operation";
    }
}