#ifndef QUERY_PARSER_H
#define QUERY_PARSER_H

#include "../include/common.h"
#include <string>
#include <vector>
#include <memory>

// Tipos de operaciones SQL soportadas
enum class QueryType {
    SELECT,
    INSERT,
    UPDATE,
    DELETE,
    CREATE_TABLE,
    DROP_TABLE,
    INVALID
};

// Tipos de operadores de comparación
enum class ComparisonOperator {
    EQUAL,          // =
    NOT_EQUAL,      // !=, <>
    LESS_THAN,      // <
    LESS_EQUAL,     // <=
    GREATER_THAN,   // >
    GREATER_EQUAL,  // >=
    LIKE,           // LIKE (para strings)
    INVALID
};

// Estructura para representar una condición WHERE
struct WhereCondition {
    std::string column_name;
    ComparisonOperator operator_;
    std::string value;
    
    WhereCondition() : operator_(ComparisonOperator::INVALID) {}
    WhereCondition(const std::string& col, ComparisonOperator op, const std::string& val)
        : column_name(col), operator_(op), value(val) {}
};

// Estructura base para todas las consultas
struct ParsedQuery {
    QueryType type;
    std::string table_name;
    
    ParsedQuery(QueryType t = QueryType::INVALID) : type(t) {}
    virtual ~ParsedQuery() = default;
};

// Estructura para consultas SELECT
struct SelectQuery : public ParsedQuery {
    std::vector<std::string> columns;  // Columnas a seleccionar (* para todas)
    std::vector<WhereCondition> where_conditions;
    
    SelectQuery() : ParsedQuery(QueryType::SELECT) {}
};

// Estructura para consultas INSERT
struct InsertQuery : public ParsedQuery {
    std::vector<std::string> columns;  // Columnas especificadas (vacío si no se especifican)
    std::vector<std::string> values;   // Valores a insertar
    
    InsertQuery() : ParsedQuery(QueryType::INSERT) {}
};

// Estructura para consultas UPDATE
struct UpdateQuery : public ParsedQuery {
    std::vector<std::pair<std::string, std::string>> set_clauses; // columna = valor
    std::vector<WhereCondition> where_conditions;
    
    UpdateQuery() : ParsedQuery(QueryType::UPDATE) {}
};

// Estructura para consultas DELETE
struct DeleteQuery : public ParsedQuery {
    std::vector<WhereCondition> where_conditions;
    
    DeleteQuery() : ParsedQuery(QueryType::DELETE) {}
};

// Estructura para CREATE TABLE
struct CreateTableQuery : public ParsedQuery {
    std::vector<std::pair<std::string, std::string>> column_definitions; // (nombre, tipo)
    
    CreateTableQuery() : ParsedQuery(QueryType::CREATE_TABLE) {}
};

// Estructura para DROP TABLE
struct DropTableQuery : public ParsedQuery {
    DropTableQuery() : ParsedQuery(QueryType::DROP_TABLE) {}
};

// Clase principal del parser
class QueryParser {
public:
    QueryParser();
    ~QueryParser();
    
    // Parsea una consulta SQL y retorna un objeto ParsedQuery
    std::unique_ptr<ParsedQuery> Parse(const std::string& sql);
    
    // Obtiene el último error de parsing
    std::string GetLastError() const { return last_error_; }
    
private:
    std::string last_error_;
    
    // Métodos auxiliares para parsing
    std::vector<std::string> Tokenize(const std::string& sql);
    std::string ToUpperCase(const std::string& str);
    std::string Trim(const std::string& str);
    
    // Métodos específicos para cada tipo de consulta
    std::unique_ptr<SelectQuery> ParseSelect(const std::vector<std::string>& tokens);
    std::unique_ptr<InsertQuery> ParseInsert(const std::vector<std::string>& tokens);
    std::unique_ptr<UpdateQuery> ParseUpdate(const std::vector<std::string>& tokens);
    std::unique_ptr<DeleteQuery> ParseDelete(const std::vector<std::string>& tokens);
    std::unique_ptr<CreateTableQuery> ParseCreateTable(const std::vector<std::string>& tokens);
    std::unique_ptr<DropTableQuery> ParseDropTable(const std::vector<std::string>& tokens);
    
    // Métodos auxiliares para parsing de componentes
    std::vector<WhereCondition> ParseWhereClause(const std::vector<std::string>& tokens, size_t& index);
    ComparisonOperator ParseComparisonOperator(const std::string& op);
    std::vector<std::string> ParseColumnList(const std::vector<std::string>& tokens, size_t& index);
    std::vector<std::string> ParseValueList(const std::vector<std::string>& tokens, size_t& index);
};

#endif // QUERY_PARSER_H