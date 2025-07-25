#ifndef QUERY_PROCESSOR_H
#define QUERY_PROCESSOR_H

#include "query_parser.h"
#include "query_optimizer.h"
#include "query_executor.h"
#include "../Catalog_Manager/Catalog_Manager.h"
#include "../record_manager/record_manager.h"
#include <memory>
#include <string>

// Clase principal del procesador de consultas
// Integra el parser, optimizador y ejecutor para procesar consultas SQL completas
class QueryProcessor {
public:
    // Constructor del QueryProcessor
    QueryProcessor(CatalogManager& catalog_manager, RecordManager& record_manager);
    ~QueryProcessor();
    
    // Procesa una consulta SQL completa desde el string hasta el resultado
    std::unique_ptr<QueryResult> ProcessQuery(const std::string& sql);
    
    // Obtiene el último error ocurrido en cualquier fase del procesamiento
    std::string GetLastError() const { return last_error_; }
    
    // Obtiene estadísticas del último procesamiento
    struct ProcessingStats {
        double parse_time_ms;
        double optimization_time_ms;
        double execution_time_ms;
        double total_time_ms;
        double estimated_cost;
        size_t plan_operators_count;
        
        ProcessingStats() : parse_time_ms(0), optimization_time_ms(0), 
                           execution_time_ms(0), total_time_ms(0), 
                           estimated_cost(0), plan_operators_count(0) {}
    };
    
    const ProcessingStats& GetLastStats() const { return last_stats_; }
    
    // Métodos para obtener información de depuración
    std::string GetLastParsedQueryInfo() const;
    std::string GetLastExecutionPlanInfo() const;
    
    // Configuración del procesador
    void SetVerboseMode(bool verbose) { verbose_mode_ = verbose; }
    bool IsVerboseMode() const { return verbose_mode_; }
    
private:
    // Componentes del procesador
    std::unique_ptr<QueryParser> parser_;
    std::unique_ptr<QueryOptimizer> optimizer_;
    std::unique_ptr<QueryExecutor> executor_;
    
    // Referencias a los managers del SGBD
    CatalogManager& catalog_manager_;
    RecordManager& record_manager_;
    
    // Estado del procesador
    std::string last_error_;
    ProcessingStats last_stats_;
    bool verbose_mode_;
    
    // Información de depuración de la última consulta
    std::unique_ptr<ParsedQuery> last_parsed_query_;
    std::unique_ptr<ExecutionPlan> last_execution_plan_;
    
    // Métodos auxiliares
    void ResetStats();
    void LogVerbose(const std::string& message) const;
    double GetCurrentTimeMs() const;
    
    // Métodos para generar información de depuración
    std::string QueryTypeToString(QueryType type) const;
    std::string PhysicalOperationTypeToString(PhysicalOperationType type) const;
};

#endif // QUERY_PROCESSOR_H