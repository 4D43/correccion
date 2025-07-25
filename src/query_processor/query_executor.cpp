#include "query_executor.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>

QueryExecutor::QueryExecutor(CatalogManager& catalog_manager, RecordManager& record_manager)
    : catalog_manager_(catalog_manager), record_manager_(record_manager) {}

QueryExecutor::~QueryExecutor() {}

std::unique_ptr<QueryResult> QueryExecutor::Execute(const ExecutionPlan& plan) {
    last_error_.clear();
    auto result = std::make_unique<QueryResult>();
    
    try {
        ExecutionContext context;
        
        // Ejecutar cada operador en secuencia
        for (const auto& op : plan.operators) {
            Status status = Status::ERROR;
            
            switch (op->type) {
                case PhysicalOperationType::TABLE_SCAN:
                    status = ExecuteTableScan(*op, context);
                    break;
                case PhysicalOperationType::FILTER:
                    status = ExecuteFilter(*op, context);
                    break;
                case PhysicalOperationType::PROJECT:
                    status = ExecuteProject(*op, context);
                    break;
                case PhysicalOperationType::INSERT_OP:
                    status = ExecuteInsert(*op, context);
                    break;
                case PhysicalOperationType::UPDATE_OP:
                    status = ExecuteUpdate(*op, context);
                    break;
                case PhysicalOperationType::DELETE_OP:
                    status = ExecuteDelete(*op, context);
                    break;
                default:
                    last_error_ = "Unsupported physical operator type";
                    result->error_message = last_error_;
                    return result;
            }
            
            if (status != Status::OK) {
                result->error_message = last_error_;
                return result;
            }
        }
        
        // Copiar resultados al objeto resultado
        result->rows = std::move(context.current_rows);
        result->column_names = std::move(context.current_columns);
        result->affected_rows = context.current_rows.size();
        result->success = true;
        
    } catch (const std::exception& e) {
        last_error_ = "Execution error: " + std::string(e.what());
        result->error_message = last_error_;
        result->success = false;
    }
    
    return result;
}

std::unique_ptr<QueryResult> QueryExecutor::ExecuteDDL(const ParsedQuery& parsed_query) {
    switch (parsed_query.type) {
        case QueryType::CREATE_TABLE:
            return ExecuteCreateTable(static_cast<const CreateTableQuery&>(parsed_query));
        case QueryType::DROP_TABLE:
            return ExecuteDropTable(static_cast<const DropTableQuery&>(parsed_query));
        default:
            auto result = std::make_unique<QueryResult>();
            result->error_message = "Unsupported DDL operation";
            return result;
    }
}

Status QueryExecutor::ExecuteTableScan(const PhysicalOperator& op, ExecutionContext& context) {
    // Obtener esquema de la tabla
    Status status = catalog_manager_.GetTableSchema(op.table_name, context.current_schema);
    if (status != Status::OK) {
        last_error_ = "Table not found: " + op.table_name;
        return status;
    }
    
    context.current_table = op.table_name;
    context.current_rows.clear();
    
    // Configurar nombres de columnas
    context.current_columns.clear();
    for (const auto& col : context.current_schema.columns) {
        context.current_columns.push_back(col.name);
    }
    
    // Escanear todas las páginas de datos de la tabla
    for (PageId page_id : context.current_schema.base_metadata.data_page_ids) {
        // Obtener número de slots en la página
        uint32_t num_records;
        Status get_num_status = record_manager_.GetNumRecords(page_id, num_records);
        if (get_num_status != Status::OK) {
            continue; // Saltar esta página si hay error
        }
        
        // Leer cada registro en la página
        for (uint32_t slot_id = 0; slot_id < num_records; ++slot_id) {
            Record record;
            Status get_record_status = record_manager_.GetRecord(page_id, slot_id, record);
            if (get_record_status == Status::OK) {
                // Parsear el registro y añadirlo a los resultados
                std::vector<std::string> row = ParseRecord(record, context.current_schema);
                context.current_rows.push_back(row);
            }
        }
    }
    
    return Status::OK;
}

Status QueryExecutor::ExecuteFilter(const PhysicalOperator& op, ExecutionContext& context) {
    std::vector<std::vector<std::string>> filtered_rows;
    
    // Evaluar cada fila contra las condiciones
    for (const auto& row : context.current_rows) {
        bool passes_all_conditions = true;
        
        for (const auto& condition : op.conditions) {
            if (!EvaluateCondition(condition, row, context.current_columns)) {
                passes_all_conditions = false;
                break;
            }
        }
        
        if (passes_all_conditions) {
            filtered_rows.push_back(row);
        }
    }
    
    context.current_rows = std::move(filtered_rows);
    return Status::OK;
}

Status QueryExecutor::ExecuteProject(const PhysicalOperator& op, ExecutionContext& context) {
    if (op.columns.empty() || (op.columns.size() == 1 && op.columns[0] == "*")) {
        // SELECT * - no hay proyección
        return Status::OK;
    }
    
    // Encontrar índices de las columnas solicitadas
    std::vector<size_t> column_indices;
    std::vector<std::string> new_column_names;
    
    for (const std::string& requested_col : op.columns) {
        auto it = std::find(context.current_columns.begin(), context.current_columns.end(), requested_col);
        if (it != context.current_columns.end()) {
            column_indices.push_back(std::distance(context.current_columns.begin(), it));
            new_column_names.push_back(requested_col);
        } else {
            last_error_ = "Column not found: " + requested_col;
            return Status::NOT_FOUND;
        }
    }
    
    // Proyectar las filas
    std::vector<std::vector<std::string>> projected_rows;
    for (const auto& row : context.current_rows) {
        std::vector<std::string> projected_row;
        for (size_t col_idx : column_indices) {
            if (col_idx < row.size()) {
                projected_row.push_back(row[col_idx]);
            } else {
                projected_row.push_back("");
            }
        }
        projected_rows.push_back(projected_row);
    }
    
    context.current_rows = std::move(projected_rows);
    context.current_columns = std::move(new_column_names);
    return Status::OK;
}

Status QueryExecutor::ExecuteInsert(const PhysicalOperator& op, ExecutionContext& context) {
    // Obtener esquema de la tabla
    FullTableSchema schema;
    Status status = catalog_manager_.GetTableSchema(op.table_name, schema);
    if (status != Status::OK) {
        last_error_ = "Table not found: " + op.table_name;
        return status;
    }
    
    // Crear el registro
    Record record = CreateRecord(op.values, schema);
    
    // Intentar insertar en páginas existentes
    uint32_t slot_id;
    Status insert_status = Status::ERROR;
    PageId target_page_id = 0;
    
    for (PageId page_id : schema.base_metadata.data_page_ids) {
        BlockSizeType free_space;
        Status get_space_status = record_manager_.GetFreeSpace(page_id, free_space);
        if (get_space_status == Status::OK && free_space >= record.data.size() + sizeof(SlotDirectoryEntry)) {
            insert_status = record_manager_.InsertRecord(page_id, record, slot_id);
            if (insert_status == Status::OK) {
                target_page_id = page_id;
                break;
            }
        }
    }
    
    // Si no hay espacio, crear nueva página
    if (insert_status != Status::OK) {
        // Esta funcionalidad requiere acceso al BufferManager, que no tenemos aquí directamente
        // Por simplicidad, reportamos error
        last_error_ = "No space available for insert and cannot create new page";
        return Status::DISK_FULL;
    }
    
    // Actualizar conteo de registros
    schema.base_metadata.num_records++;
    catalog_manager_.UpdateTableNumRecords(op.table_name, schema.base_metadata.num_records);
    
    // Configurar resultado
    context.current_rows.clear();
    context.current_columns = {"affected_rows"};
    context.current_rows.push_back({"1"});
    
    return Status::OK;
}

Status QueryExecutor::ExecuteUpdate(const PhysicalOperator& op, ExecutionContext& context) {
    // Primero necesitamos escanear la tabla para encontrar registros que coincidan
    Status scan_status = ExecuteTableScan(PhysicalOperator(PhysicalOperationType::TABLE_SCAN), context);
    if (scan_status != Status::OK) {
        return scan_status;
    }
    
    // Aplicar filtros si los hay
    if (!op.conditions.empty()) {
        PhysicalOperator filter_op(PhysicalOperationType::FILTER);
        const_cast<PhysicalOperator&>(filter_op).conditions = op.conditions;
        Status filter_status = ExecuteFilter(filter_op, context);
        if (filter_status != Status::OK) {
            return filter_status;
        }
    }
    
    size_t updated_count = context.current_rows.size();
    
    // Por simplicidad, reportamos el número de filas que serían actualizadas
    // Una implementación completa requeriría modificar los registros reales
    context.current_rows.clear();
    context.current_columns = {"affected_rows"};
    context.current_rows.push_back({std::to_string(updated_count)});
    
    return Status::OK;
}

Status QueryExecutor::ExecuteDelete(const PhysicalOperator& op, ExecutionContext& context) {
    // Similar a UPDATE, primero escaneamos y filtramos
    Status scan_status = ExecuteTableScan(PhysicalOperator(PhysicalOperationType::TABLE_SCAN), context);
    if (scan_status != Status::OK) {
        return scan_status;
    }
    
    if (!op.conditions.empty()) {
        PhysicalOperator filter_op(PhysicalOperationType::FILTER);
        const_cast<PhysicalOperator&>(filter_op).conditions = op.conditions;
        Status filter_status = ExecuteFilter(filter_op, context);
        if (filter_status != Status::OK) {
            return filter_status;
        }
    }
    
    size_t deleted_count = context.current_rows.size();
    
    // Reportar número de filas que serían eliminadas
    context.current_rows.clear();
    context.current_columns = {"affected_rows"};
    context.current_rows.push_back({std::to_string(deleted_count)});
    
    return Status::OK;
}

bool QueryExecutor::EvaluateCondition(const WhereCondition& condition, const std::vector<std::string>& row,
                                     const std::vector<std::string>& column_names) {
    // Encontrar el índice de la columna
    auto it = std::find(column_names.begin(), column_names.end(), condition.column_name);
    if (it == column_names.end()) {
        return false; // Columna no encontrada
    }
    
    size_t col_index = std::distance(column_names.begin(), it);
    if (col_index >= row.size()) {
        return false;
    }
    
    const std::string& row_value = row[col_index];
    const std::string& condition_value = condition.value;
    
    // Evaluar la condición según el operador
    switch (condition.operator_) {
        case ComparisonOperator::EQUAL:
            return row_value == condition_value;
        case ComparisonOperator::NOT_EQUAL:
            return row_value != condition_value;
        case ComparisonOperator::LESS_THAN:
            return CompareValues(row_value, condition_value, ColumnType::VARCHAR) < 0;
        case ComparisonOperator::LESS_EQUAL:
            return CompareValues(row_value, condition_value, ColumnType::VARCHAR) <= 0;
        case ComparisonOperator::GREATER_THAN:
            return CompareValues(row_value, condition_value, ColumnType::VARCHAR) > 0;
        case ComparisonOperator::GREATER_EQUAL:
            return CompareValues(row_value, condition_value, ColumnType::VARCHAR) >= 0;
        case ComparisonOperator::LIKE:
            // Implementación simple de LIKE (solo % al final)
            if (condition_value.back() == '%') {
                std::string prefix = condition_value.substr(0, condition_value.length() - 1);
                return row_value.substr(0, prefix.length()) == prefix;
            }
            return row_value == condition_value;
        default:
            return false;
    }
}

std::vector<std::string> QueryExecutor::ParseRecord(const Record& record, const FullTableSchema& schema) {
    std::vector<std::string> result;
    std::string record_str(record.data.begin(), record.data.end());
    
    // Parsing simple basado en delimitadores '#'
    std::stringstream ss(record_str);
    std::string field;
    
    while (std::getline(ss, field, '#')) {
        result.push_back(field);
    }
    
    // Asegurar que tenemos el número correcto de campos
    while (result.size() < schema.columns.size()) {
        result.push_back("");
    }
    
    return result;
}

Record QueryExecutor::CreateRecord(const std::vector<std::string>& values, const FullTableSchema& schema) {
    Record record;
    std::string record_str;
    
    // Combinar valores con delimitador '#'
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) record_str += "#";
        record_str += values[i];
    }
    
    record.data.assign(record_str.begin(), record_str.end());
    return record;
}

int QueryExecutor::CompareValues(const std::string& value1, const std::string& value2, ColumnType type) {
    if (type == ColumnType::INT) {
        try {
            int v1 = std::stoi(value1);
            int v2 = std::stoi(value2);
            return (v1 < v2) ? -1 : (v1 > v2) ? 1 : 0;
        } catch (...) {
            // Fall back to string comparison
        }
    }
    
    // String comparison
    return value1.compare(value2);
}

ColumnType QueryExecutor::GetColumnType(const std::string& column_name, const FullTableSchema& schema) {
    for (const auto& col : schema.columns) {
        if (col.name == column_name) {
            return col.type;
        }
    }
    return ColumnType::VARCHAR; // Default
}

std::unique_ptr<QueryResult> QueryExecutor::ExecuteCreateTable(const CreateTableQuery& query) {
    auto result = std::make_unique<QueryResult>();
    
    // Convertir definiciones de columnas
    std::vector<ColumnMetadata> columns;
    for (const auto& col_def : query.column_definitions) {
        ColumnMetadata col;
        strncpy(col.name, col_def.first.c_str(), sizeof(col.name) - 1);
        col.name[sizeof(col.name) - 1] = '\0';
        
        // Mapear tipos de string a ColumnType
        std::string type_upper = col_def.second;
        std::transform(type_upper.begin(), type_upper.end(), type_upper.begin(), ::toupper);
        
        if (type_upper == "INT") {
            col.type = ColumnType::INT;
            col.size = sizeof(int);
        } else if (type_upper.substr(0, 4) == "CHAR") {
            col.type = ColumnType::CHAR;
            col.size = 50; // Tamaño por defecto
        } else if (type_upper.substr(0, 7) == "VARCHAR") {
            col.type = ColumnType::VARCHAR;
            col.size = 255; // Tamaño por defecto
        } else {
            result->error_message = "Unsupported column type: " + col_def.second;
            return result;
        }
        
        columns.push_back(col);
    }
    
    // Crear la tabla
    Status status = catalog_manager_.CreateTable(query.table_name, columns, true);
    if (status == Status::OK) {
        result->success = true;
        result->affected_rows = 1;
        result->column_names = {"result"};
        result->rows.push_back({"Table created successfully"});
    } else {
        result->error_message = "Failed to create table: " + StatusToString(status);
    }
    
    return result;
}

std::unique_ptr<QueryResult> QueryExecutor::ExecuteDropTable(const DropTableQuery& query) {
    auto result = std::make_unique<QueryResult>();
    
    Status status = catalog_manager_.DropTable(query.table_name);
    if (status == Status::OK) {
        result->success = true;
        result->affected_rows = 1;
        result->column_names = {"result"};
        result->rows.push_back({"Table dropped successfully"});
    } else {
        result->error_message = "Failed to drop table: " + StatusToString(status);
    }
    
    return result;
}