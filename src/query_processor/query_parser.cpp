#include "query_parser.h"
#include <sstream>
#include <algorithm>
#include <cctype>

QueryParser::QueryParser() {}

QueryParser::~QueryParser() {}

std::unique_ptr<ParsedQuery> QueryParser::Parse(const std::string& sql) {
    last_error_.clear();
    
    if (sql.empty()) {
        last_error_ = "Empty SQL query";
        return nullptr;
    }
    
    // Tokenizar la consulta
    std::vector<std::string> tokens = Tokenize(sql);
    if (tokens.empty()) {
        last_error_ = "No valid tokens found";
        return nullptr;
    }
    
    // Determinar el tipo de consulta basado en la primera palabra clave
    std::string first_token = ToUpperCase(tokens[0]);
    
    try {
        if (first_token == "SELECT") {
            return ParseSelect(tokens);
        } else if (first_token == "INSERT") {
            return ParseInsert(tokens);
        } else if (first_token == "UPDATE") {
            return ParseUpdate(tokens);
        } else if (first_token == "DELETE") {
            return ParseDelete(tokens);
        } else if (first_token == "CREATE") {
            return ParseCreateTable(tokens);
        } else if (first_token == "DROP") {
            return ParseDropTable(tokens);
        } else {
            last_error_ = "Unsupported query type: " + first_token;
            return nullptr;
        }
    } catch (const std::exception& e) {
        last_error_ = "Parse error: " + std::string(e.what());
        return nullptr;
    }
}

std::vector<std::string> QueryParser::Tokenize(const std::string& sql) {
    std::vector<std::string> tokens;
    std::string current_token;
    bool in_quotes = false;
    char quote_char = '\0';
    
    for (size_t i = 0; i < sql.length(); ++i) {
        char c = sql[i];
        
        if (!in_quotes && (c == '\'' || c == '"')) {
            in_quotes = true;
            quote_char = c;
            current_token += c;
        } else if (in_quotes && c == quote_char) {
            in_quotes = false;
            current_token += c;
            quote_char = '\0';
        } else if (!in_quotes && (std::isspace(c) || c == ',' || c == '(' || c == ')' || c == ';')) {
            if (!current_token.empty()) {
                tokens.push_back(Trim(current_token));
                current_token.clear();
            }
            if (c == ',' || c == '(' || c == ')') {
                tokens.push_back(std::string(1, c));
            }
        } else {
            current_token += c;
        }
    }
    
    if (!current_token.empty()) {
        tokens.push_back(Trim(current_token));
    }
    
    return tokens;
}

std::string QueryParser::ToUpperCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

std::string QueryParser::Trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::unique_ptr<SelectQuery> QueryParser::ParseSelect(const std::vector<std::string>& tokens) {
    auto query = std::make_unique<SelectQuery>();
    
    if (tokens.size() < 4) { // SELECT col FROM table
        throw std::runtime_error("Invalid SELECT syntax");
    }
    
    size_t index = 1; // Después de SELECT
    
    // Parsear columnas
    query->columns = ParseColumnList(tokens, index);
    
    // Buscar FROM
    if (index >= tokens.size() || ToUpperCase(tokens[index]) != "FROM") {
        throw std::runtime_error("Missing FROM clause");
    }
    index++; // Saltar FROM
    
    // Obtener nombre de tabla
    if (index >= tokens.size()) {
        throw std::runtime_error("Missing table name");
    }
    query->table_name = tokens[index++];
    
    // Parsear WHERE si existe
    if (index < tokens.size() && ToUpperCase(tokens[index]) == "WHERE") {
        index++; // Saltar WHERE
        query->where_conditions = ParseWhereClause(tokens, index);
    }
    
    return query;
}

std::unique_ptr<InsertQuery> QueryParser::ParseInsert(const std::vector<std::string>& tokens) {
    auto query = std::make_unique<InsertQuery>();
    
    if (tokens.size() < 4) { // INSERT INTO table VALUES
        throw std::runtime_error("Invalid INSERT syntax");
    }
    
    size_t index = 1; // Después de INSERT
    
    // Verificar INTO
    if (ToUpperCase(tokens[index]) != "INTO") {
        throw std::runtime_error("Missing INTO keyword");
    }
    index++; // Saltar INTO
    
    // Obtener nombre de tabla
    query->table_name = tokens[index++];
    
    // Verificar si hay lista de columnas
    if (index < tokens.size() && tokens[index] == "(") {
        index++; // Saltar (
        query->columns = ParseColumnList(tokens, index);
        if (index >= tokens.size() || tokens[index] != ")") {
            throw std::runtime_error("Missing closing parenthesis for column list");
        }
        index++; // Saltar )
    }
    
    // Verificar VALUES
    if (index >= tokens.size() || ToUpperCase(tokens[index]) != "VALUES") {
        throw std::runtime_error("Missing VALUES keyword");
    }
    index++; // Saltar VALUES
    
    // Parsear valores
    if (index >= tokens.size() || tokens[index] != "(") {
        throw std::runtime_error("Missing opening parenthesis for values");
    }
    index++; // Saltar (
    
    query->values = ParseValueList(tokens, index);
    
    if (index >= tokens.size() || tokens[index] != ")") {
        throw std::runtime_error("Missing closing parenthesis for values");
    }
    
    return query;
}

std::unique_ptr<UpdateQuery> QueryParser::ParseUpdate(const std::vector<std::string>& tokens) {
    auto query = std::make_unique<UpdateQuery>();
    
    if (tokens.size() < 6) { // UPDATE table SET col = val
        throw std::runtime_error("Invalid UPDATE syntax");
    }
    
    size_t index = 1; // Después de UPDATE
    
    // Obtener nombre de tabla
    query->table_name = tokens[index++];
    
    // Verificar SET
    if (ToUpperCase(tokens[index]) != "SET") {
        throw std::runtime_error("Missing SET keyword");
    }
    index++; // Saltar SET
    
    // Parsear cláusulas SET
    while (index < tokens.size() && ToUpperCase(tokens[index]) != "WHERE") {
        if (index + 2 >= tokens.size()) {
            throw std::runtime_error("Invalid SET clause");
        }
        
        std::string column = tokens[index++];
        if (tokens[index] != "=") {
            throw std::runtime_error("Missing = in SET clause");
        }
        index++; // Saltar =
        std::string value = tokens[index++];
        
        query->set_clauses.push_back({column, value});
        
        // Saltar coma si existe
        if (index < tokens.size() && tokens[index] == ",") {
            index++;
        }
    }
    
    // Parsear WHERE si existe
    if (index < tokens.size() && ToUpperCase(tokens[index]) == "WHERE") {
        index++; // Saltar WHERE
        query->where_conditions = ParseWhereClause(tokens, index);
    }
    
    return query;
}

std::unique_ptr<DeleteQuery> QueryParser::ParseDelete(const std::vector<std::string>& tokens) {
    auto query = std::make_unique<DeleteQuery>();
    
    if (tokens.size() < 4) { // DELETE FROM table
        throw std::runtime_error("Invalid DELETE syntax");
    }
    
    size_t index = 1; // Después de DELETE
    
    // Verificar FROM
    if (ToUpperCase(tokens[index]) != "FROM") {
        throw std::runtime_error("Missing FROM keyword");
    }
    index++; // Saltar FROM
    
    // Obtener nombre de tabla
    query->table_name = tokens[index++];
    
    // Parsear WHERE si existe
    if (index < tokens.size() && ToUpperCase(tokens[index]) == "WHERE") {
        index++; // Saltar WHERE
        query->where_conditions = ParseWhereClause(tokens, index);
    }
    
    return query;
}

std::unique_ptr<CreateTableQuery> QueryParser::ParseCreateTable(const std::vector<std::string>& tokens) {
    auto query = std::make_unique<CreateTableQuery>();
    
    if (tokens.size() < 5) { // CREATE TABLE name (col type)
        throw std::runtime_error("Invalid CREATE TABLE syntax");
    }
    
    size_t index = 1; // Después de CREATE
    
    // Verificar TABLE
    if (ToUpperCase(tokens[index]) != "TABLE") {
        throw std::runtime_error("Missing TABLE keyword");
    }
    index++; // Saltar TABLE
    
    // Obtener nombre de tabla
    query->table_name = tokens[index++];
    
    // Verificar (
    if (tokens[index] != "(") {
        throw std::runtime_error("Missing opening parenthesis");
    }
    index++; // Saltar (
    
    // Parsear definiciones de columnas
    while (index < tokens.size() && tokens[index] != ")") {
        if (index + 1 >= tokens.size()) {
            throw std::runtime_error("Invalid column definition");
        }
        
        std::string column_name = tokens[index++];
        std::string column_type = tokens[index++];
        
        query->column_definitions.push_back({column_name, column_type});
        
        // Saltar coma si existe
        if (index < tokens.size() && tokens[index] == ",") {
            index++;
        }
    }
    
    return query;
}

std::unique_ptr<DropTableQuery> QueryParser::ParseDropTable(const std::vector<std::string>& tokens) {
    auto query = std::make_unique<DropTableQuery>();
    
    if (tokens.size() < 3) { // DROP TABLE name
        throw std::runtime_error("Invalid DROP TABLE syntax");
    }
    
    size_t index = 1; // Después de DROP
    
    // Verificar TABLE
    if (ToUpperCase(tokens[index]) != "TABLE") {
        throw std::runtime_error("Missing TABLE keyword");
    }
    index++; // Saltar TABLE
    
    // Obtener nombre de tabla
    query->table_name = tokens[index];
    
    return query;
}

std::vector<WhereCondition> QueryParser::ParseWhereClause(const std::vector<std::string>& tokens, size_t& index) {
    std::vector<WhereCondition> conditions;
    
    while (index + 2 < tokens.size()) {
        std::string column = tokens[index++];
        std::string op = tokens[index++];
        std::string value = tokens[index++];
        
        ComparisonOperator comp_op = ParseComparisonOperator(op);
        if (comp_op == ComparisonOperator::INVALID) {
            throw std::runtime_error("Invalid comparison operator: " + op);
        }
        
        conditions.push_back(WhereCondition(column, comp_op, value));
        
        // Verificar AND (por simplicidad, solo soportamos AND por ahora)
        if (index < tokens.size() && ToUpperCase(tokens[index]) == "AND") {
            index++; // Saltar AND
        } else {
            break;
        }
    }
    
    return conditions;
}

ComparisonOperator QueryParser::ParseComparisonOperator(const std::string& op) {
    if (op == "=") return ComparisonOperator::EQUAL;
    if (op == "!=" || op == "<>") return ComparisonOperator::NOT_EQUAL;
    if (op == "<") return ComparisonOperator::LESS_THAN;
    if (op == "<=") return ComparisonOperator::LESS_EQUAL;
    if (op == ">") return ComparisonOperator::GREATER_THAN;
    if (op == ">=") return ComparisonOperator::GREATER_EQUAL;
    if (ToUpperCase(op) == "LIKE") return ComparisonOperator::LIKE;
    return ComparisonOperator::INVALID;
}

std::vector<std::string> QueryParser::ParseColumnList(const std::vector<std::string>& tokens, size_t& index) {
    std::vector<std::string> columns;
    
    while (index < tokens.size()) {
        if (tokens[index] == "," || tokens[index] == ")" || 
            ToUpperCase(tokens[index]) == "FROM" || ToUpperCase(tokens[index]) == "VALUES") {
            break;
        }
        
        if (tokens[index] != ",") {
            columns.push_back(tokens[index]);
        }
        index++;
    }
    
    return columns;
}

std::vector<std::string> QueryParser::ParseValueList(const std::vector<std::string>& tokens, size_t& index) {
    std::vector<std::string> values;
    
    while (index < tokens.size() && tokens[index] != ")") {
        if (tokens[index] != ",") {
            values.push_back(tokens[index]);
        }
        index++;
    }
    
    return values;
}