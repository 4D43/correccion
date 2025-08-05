// gestor_tablas_avanzado.cpp - Implementación del Gestor Avanzado de Tablas
// Funcionalidades: creación por archivo/formulario, inserción CSV/formulario, eliminación con condiciones

#include "gestor_tablas_avanzado.h"
#include <algorithm>
#include <regex>
#include <cctype>
#include <limits> // Para std::numeric_limits

// === CONSTRUCTOR Y DESTRUCTOR ===

GestorTablasAvanzado::GestorTablasAvanzado(GestorCatalogo& catalogo, GestorRegistros& registros)
    : gestor_catalogo_(&catalogo), gestor_registros_(&registros) {
    std::cout << "GestorTablasAvanzado inicializado correctamente." << std::endl;
}

GestorTablasAvanzado::~GestorTablasAvanzado() {
    std::cout << "GestorTablasAvanzado destruido. Espacios disponibles: " << espacios_disponibles_.size() << std::endl;
}

// === CREACIÓN DE TABLAS ===

Status GestorTablasAvanzado::CrearTablaPorArchivo(const std::string& ruta_archivo, const std::string& nombre_tabla) {
    std::cout << "\n=== CREANDO TABLA POR ARCHIVO ===" << std::endl;
    std::cout << "Archivo: " << ruta_archivo << std::endl;
    std::cout << "Nombre tabla: " << nombre_tabla << std::endl;
    
    std::ifstream archivo(ruta_archivo);
    if (!archivo.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo " << ruta_archivo << std::endl;
        return Status::IO_ERROR;
    }

    std::string linea;
    std::getline(archivo, linea); // Leer la primera línea para los nombres de las columnas
    std::vector<std::string> nombres_columnas;
    std::stringstream ss_nombres(linea);
    std::string nombre_col;
    while (std::getline(ss_nombres, nombre_col, ',')) {
        nombres_columnas.push_back(Trim(nombre_col));
    }

    std::getline(archivo, linea); // Leer la segunda línea para los tipos de columnas
    std::vector<std::string> tipos_columnas_str;
    std::stringstream ss_tipos(linea);
    std::string tipo_col;
    while (std::getline(ss_tipos, tipo_col, ',')) {
        tipos_columnas_str.push_back(Trim(tipo_col));
    }

    if (nombres_columnas.size() != tipos_columnas_str.size()) {
        std::cerr << "Error: El número de nombres de columna no coincide con el número de tipos de columna." << std::endl;
        return Status::INVALID_FORMAT; // Corrected enum member
    }

    // Definición de las columnas para el catálogo
    std::vector<DefinicionColumna> definiciones_columnas;
    for (size_t i = 0; i < nombres_columnas.size(); ++i) {
        TipoDatoTabla tipo = StringATipo(tipos_columnas_str[i]);
        if (tipo == TipoDatoTabla::INVALID) { // Assuming INVALID exists or handle unknown types
            std::cerr << "Error: Tipo de columna desconocido en el archivo: " << tipos_columnas_str[i] << std::endl;
            return Status::INVALID_FORMAT; // Corrected enum member
        }
        uint32_t tamano = 0; // Por defecto para INT, STR. Para CHAR se leerá después.
        bool es_pk = false; // Por defecto no es clave primaria

        // Si el tipo es CHAR, el tamaño debería estar especificado (ej. CHAR(10))
        std::regex char_regex("CHAR\\((\\d+)\\)");
        std::smatch matches;
        if (std::regex_match(tipos_columnas_str[i], matches, char_regex)) {
            if (matches.size() == 2) {
                tamano = std::stoul(matches[1].str());
                tipo = TipoDatoTabla::CHAR;
            }
        }
        definiciones_columnas.emplace_back(nombres_columnas[i], tipo, tamano, es_pk);
    }

    // Crear la tabla en el catálogo
    Status status = gestor_catalogo_->CrearTabla(nombre_tabla, definiciones_columnas);
    if (status != Status::OK) {
        std::cerr << "Error al crear la tabla en el catálogo: " << StatusToString(status) << std::endl;
        return status;
    }

    // Insertar datos desde el archivo CSV (resto de las líneas)
    while (std::getline(archivo, linea)) {
        if (linea.empty()) continue; // Saltar líneas vacías
        std::stringstream ss_datos(linea);
        std::string valor_campo;
        DatosRegistro registro;
        for (size_t i = 0; i < nombres_columnas.size(); ++i) {
            if (std::getline(ss_datos, valor_campo, ',')) {
                registro.AñadirCampo(nombres_columnas[i], Trim(valor_campo));
            } else {
                std::cerr << "Advertencia: Faltan campos en el registro. Se usará valor vacío." << std::endl;
                registro.AñadirCampo(nombres_columnas[i], "");
            }
        }
        Status insert_status = gestor_registros_->InsertarRegistro(nombre_tabla, registro);
        if (insert_status != Status::OK) {
            std::cerr << "Advertencia: Fallo al insertar registro: " << StatusToString(insert_status) << std::endl;
            // Podríamos decidir si esto es un error fatal o solo una advertencia
        }
    }

    std::cout << "Tabla '" << nombre_tabla << "' creada y datos insertados desde archivo." << std::endl;
    return Status::OK;
}

Status GestorTablasAvanzado::CrearTablaInteractiva() {
    std::cout << "\n=== CREAR TABLA INTERACTIVAMENTE ===" << std::endl;
    std::string nombre_tabla;
    std::cout << "Ingrese el nombre de la tabla: ";
    std::getline(std::cin, nombre_tabla);

    if (nombre_tabla.empty() || ExisteTabla(nombre_tabla)) {
        std::cerr << "Error: Nombre de tabla inválido o ya existe." << std::endl;
        return Status::INVALID_ARGUMENT;
    }

    std::vector<DefinicionColumna> definiciones_columnas;
    char mas_columnas;
    do {
        std::string nombre_columna;
        std::string tipo_columna_str;
        uint32_t tamano_columna = 0;
        char es_pk_char;
        bool es_pk = false;

        std::cout << "\n--- Nueva Columna ---" << std::endl;
        std::cout << "Nombre de la columna: ";
        std::getline(std::cin, nombre_columna);

        std::cout << "Tipo de dato (INT, STR, CHAR(n)): ";
        std::getline(std::cin, tipo_columna_str);

        TipoDatoTabla tipo = StringATipo(tipo_columna_str);
        if (tipo == TipoDatoTabla::INVALID) { // Assuming INVALID exists
            std::cerr << "Error: Tipo de dato inválido. Intente de nuevo." << std::endl;
            continue;
        }

        if (tipo == TipoDatoTabla::CHAR) {
            std::regex char_regex("CHAR\\((\\d+)\\)");
            std::smatch matches;
            if (std::regex_match(tipo_columna_str, matches, char_regex)) {
                if (matches.size() == 2) {
                    try {
                        tamano_columna = std::stoul(matches[1].str());
                    } catch (const std::exception& e) {
                        std::cerr << "Error: Tamaño de CHAR inválido. " << e.what() << std::endl;
                        continue;
                    }
                } else {
                    std::cerr << "Error: Formato CHAR(n) incorrecto. Intente de nuevo." << std::endl;
                    continue;
                }
            } else {
                std::cerr << "Error: Formato CHAR(n) incorrecto. Intente de nuevo." << std::endl;
                continue;
            }
        }

        std::cout << "¿Es clave primaria? (s/n): ";
        std::cin >> es_pk_char;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Limpiar buffer
        es_pk = (es_pk_char == 's' || es_pk_char == 'S');

        definiciones_columnas.emplace_back(nombre_columna, tipo, tamano_columna, es_pk);

        std::cout << "¿Añadir otra columna? (s/n): ";
        std::cin >> mas_columnas;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Limpiar buffer
    } while (mas_columnas == 's' || mas_columnas == 'S');

    Status status = gestor_catalogo_->CrearTabla(nombre_tabla, definiciones_columnas);
    if (status == Status::OK) {
        std::cout << "Tabla '" << nombre_tabla << "' creada exitosamente." << std::endl;
    } else {
        std::cerr << "Error al crear la tabla: " << StatusToString(status) << std::endl;
    }
    return status;
}

Status GestorTablasAvanzado::InsertarRegistroPorArchivo(const std::string& ruta_archivo, const std::string& nombre_tabla) {
    std::cout << "\n=== INSERTANDO REGISTROS POR ARCHIVO ===\n";
    std::cout << "Archivo: " << ruta_archivo << "\n";
    std::cout << "Tabla: " << nombre_tabla << "\n";

    if (!ExisteTabla(nombre_tabla)) {
        std::cerr << "Error: La tabla '" << nombre_tabla << "' no existe." << std::endl;
        return Status::NOT_FOUND;
    }

    std::ifstream archivo(ruta_archivo);
    if (!archivo.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo " << ruta_archivo << std::endl;
        return Status::IO_ERROR;
    }

    std::shared_ptr<MetadataTabla> metadata_tabla = gestor_catalogo_->ObtenerMetadataTabla(nombre_tabla);
    if (!metadata_tabla) {
        std::cerr << "Error interno: Metadata de tabla no encontrada para '" << nombre_tabla << "'." << std::endl;
        return Status::ERROR;
    }

    std::string linea;
    uint32_t registros_insertados = 0;
    uint32_t registros_fallidos = 0;

    // Asumimos que el archivo CSV no tiene cabecera de columnas en este caso,
    // o que la primera línea de datos es el primer registro.
    // Si el archivo siempre tiene cabecera, se debería leer y descartar la primera línea.

    while (std::getline(archivo, linea)) {
        if (linea.empty()) continue;

        std::stringstream ss_datos(linea);
        std::string valor_campo;
        DatosRegistro registro;

        const auto& columnas = metadata_tabla->ObtenerColumnas();
        for (const auto& col_meta : columnas) {
            if (std::getline(ss_datos, valor_campo, ',')) {
                registro.AñadirCampo(col_meta.nombre, Trim(valor_campo));
            } else {
                std::cerr << "Advertencia: Faltan campos para la columna '" << col_meta.nombre << "' en el registro. Se usará valor vacío." << std::endl;
                registro.AñadirCampo(col_meta.nombre, "");
            }
        }

        Status insert_status = gestor_registros_->InsertarRegistro(nombre_tabla, registro);
        if (insert_status == Status::OK) {
            registros_insertados++;
        } else {
            std::cerr << "Error al insertar registro: " << StatusToString(insert_status) << " -> " << linea << std::endl;
            registros_fallidos++;
        }
    }

    std::cout << "\nResumen de inserción en '" << nombre_tabla << "':\n";
    std::cout << "  Registros insertados: " << registros_insertados << "\n";
    std::cout << "  Registros fallidos: " << registros_fallidos << "\n";
    return (registros_fallidos == 0) ? Status::OK : Status::OPERATION_FAILED;
}

Status GestorTablasAvanzado::InsertarRegistroInteractivo(const std::string& nombre_tabla) {
    std::cout << "\n=== INSERTAR REGISTRO INTERACTIVAMENTE ===" << std::endl;
    if (!ExisteTabla(nombre_tabla)) {
        std::cerr << "Error: La tabla '" << nombre_tabla << "' no existe." << std::endl;
        return Status::NOT_FOUND;
    }

    std::shared_ptr<MetadataTabla> metadata_tabla = gestor_catalogo_->ObtenerMetadataTabla(nombre_tabla);
    if (!metadata_tabla) {
        std::cerr << "Error interno: Metadata de tabla no encontrada para '" << nombre_tabla << "'." << std::endl;
        return Status::ERROR;
    }

    DatosRegistro nuevo_registro;
    std::cout << "Ingrese los valores para cada columna de la tabla '" << nombre_tabla << "':" << std::endl;

    for (const auto& col : metadata_tabla->ObtenerColumnas()) {
        std::string valor;
        std::cout << "  " << col.nombre << " (" << TipoAString(col.tipo);
        if (col.tipo == TipoDatoTabla::CHAR) {
            std::cout << "(" << col.tamano << ")";
        }
        std::cout << "): ";
        std::getline(std::cin, valor);
        nuevo_registro.AñadirCampo(col.nombre, valor);
    }

    Status status = gestor_registros_->InsertarRegistro(nombre_tabla, nuevo_registro);
    if (status == Status::OK) {
        std::cout << "Registro insertado exitosamente en '" << nombre_tabla << "'." << std::endl;
    } else {
        std::cerr << "Error al insertar registro: " << StatusToString(status) << std::endl;
    }
    return status;
}

Status GestorTablasAvanzado::EliminarRegistrosPorCondiciones(const std::string& nombre_tabla, const std::string& condiciones_str) {
    std::cout << "\n=== ELIMINANDO REGISTROS POR CONDICIONES ===" << std::endl;
    std::cout << "Tabla: " << nombre_tabla << std::endl;
    std::cout << "Condiciones: " << condiciones_str << std::endl;

    if (!ExisteTabla(nombre_tabla)) {
        std::cerr << "Error: La tabla '" << nombre_tabla << "' no existe." << std::endl;
        return Status::NOT_FOUND;
    }

    std::shared_ptr<MetadataTabla> metadata_tabla = gestor_catalogo_->ObtenerMetadataTabla(nombre_tabla);
    if (!metadata_tabla) {
        std::cerr << "Error interno: Metadata de tabla no encontrada para '" << nombre_tabla << "'." << std::endl;
        return Status::ERROR;
    }

    std::vector<DatosRegistro> todos_registros;
    Status fetch_status = gestor_registros_->ConsultarTodosLosRegistros(nombre_tabla, todos_registros);
    if (fetch_status != Status::OK) {
        std::cerr << "Error al consultar registros para eliminación: " << StatusToString(fetch_status) << std::endl;
        return fetch_status;
    }

    // Parsear condiciones (simplificado: "columna=valor" o "columna>valor", etc.)
    // Esto es una simplificación. Un parser de SQL real sería mucho más complejo.
    std::string columna_condicion;
    std::string operador_condicion;
    std::string valor_condicion_str;

    std::regex cond_regex("(\\w+)\\s*([=<>!]+)\\s*(.+)", std::regex::icase);
    std::smatch matches;

    if (std::regex_match(condiciones_str, matches, cond_regex) && matches.size() == 4) {
        columna_condicion = Trim(matches[1].str());
        operador_condicion = Trim(matches[2].str());
        valor_condicion_str = Trim(matches[3].str());
    } else {
        std::cerr << "Error: Formato de condición inválido. Use 'columna_nombre operador valor'." << std::endl;
        return Status::INVALID_ARGUMENT;
    }

    // Validar que la columna de condición existe
    int idx_columna = -1;
    for (size_t i = 0; i < metadata_tabla->ObtenerColumnas().size(); ++i) {
        if (metadata_tabla->ObtenerColumnas()[i].nombre == columna_condicion) {
            idx_columna = i;
            break;
        }
    }

    if (idx_columna == -1) {
        std::cerr << "Error: Columna '" << columna_condicion << "' no encontrada en la tabla '" << nombre_tabla << "'." << std::endl;
        return Status::INVALID_ARGUMENT;
    }

    ColumnType tipo_columna_condicion = metadata_tabla->ObtenerColumnas()[idx_columna].tipo;

    uint32_t registros_eliminados_count = 0;
    for (size_t i = 0; i < todos_registros.size(); ++i) {
        const DatosRegistro& registro = todos_registros[i];
        if (registro.campos.size() <= idx_columna) {
            continue; // Registro incompleto o malformado
        }
        std::string valor_registro_str = registro.campos[idx_columna];

        bool cumple_condicion = false;

        // Evaluación de la condición
        if (tipo_columna_condicion == TipoDatoTabla::INT) {
            try {
                long long valor_registro = std::stoll(valor_registro_str);
                long long valor_condicion = std::stoll(valor_condicion_str);
                if (operador_condicion == "=") cumple_condicion = (valor_registro == valor_condicion);
                else if (operador_condicion == "!=") cumple_condicion = (valor_registro != valor_condicion);
                else if (operador_condicion == ">") cumple_condicion = (valor_registro > valor_condicion);
                else if (operador_condicion == "<") cumple_condicion = (valor_registro < valor_condicion);
                else if (operador_condicion == ">=") cumple_condicion = (valor_registro >= valor_condicion);
                else if (operador_condicion == "<=") cumple_condicion = (valor_registro <= valor_condicion);
            } catch (const std::exception& e) {
                std::cerr << "Advertencia: Error de conversión INT en condición: " << e.what() << std::endl;
                continue;
            }
        } else { // STR o CHAR (comparación de cadenas)
            if (operador_condicion == "=") cumple_condicion = (valor_registro_str == valor_condicion_str);
            else if (operador_condicion == "!=") cumple_condicion = (valor_registro_str != valor_condicion_str);
            // Otras comparaciones de cadena (>, <, etc.) son más complejas y no se implementan aquí.
            else {
                std::cerr << "Advertencia: Operador '" << operador_condicion << "' no soportado para tipos de cadena." << std::endl;
                continue;
            }
        }

        if (cumple_condicion) {
            // Asumimos que el RecordId es el índice en el vector de todos_registros para esta simulación.
            // En un sistema real, necesitaríamos el RecordId real del registro.
            RecordId id_registro_a_eliminar = i + 1; // Simplificado: RecordId = índice + 1
            Status delete_status = gestor_registros_->EliminarRegistroPorID(nombre_tabla, id_registro_a_eliminar);
            if (delete_status == Status::OK) {
                registros_eliminados_count++;
            } else {
                std::cerr << "Advertencia: Fallo al eliminar registro con ID " << id_registro_a_eliminar << ": " << StatusToString(delete_status) << std::endl;
            }
        }
    }

    std::cout << "Se eliminaron " << registros_eliminados_count << " registros de la tabla '" << nombre_tabla << "'." << std::endl;
    return Status::OK;
}


Status GestorTablasAvanzado::EliminarRegistrosInteractivo(const std::string& nombre_tabla) {
    std::cout << "\n=== ELIMINAR REGISTROS INTERACTIVAMENTE ===" << std::endl;
    if (!ExisteTabla(nombre_tabla)) {
        std::cerr << "Error: La tabla '" << nombre_tabla << "' no existe." << std::endl;
        return Status::NOT_FOUND;
    }

    std::cout << "Ingrese la condición de eliminación (ej. 'edad > 30' o 'nombre = \"Juan\"'): ";
    std::string condiciones;
    std::getline(std::cin, condiciones);

    std::cout << "\n¡ADVERTENCIA! Esta operación eliminará registros permanentemente." << std::endl;
    std::cout << "¿Continuar con la eliminación? (s/n): ";
    std::string confirmar;
    std::getline(std::cin, confirmar);
    
    if (confirmar != "s" && confirmar != "S" && confirmar != "si" && confirmar != "SI") {
        std::cout << "Eliminación cancelada." << std::endl;
        return Status::CANCELLED;
    }
    
    // Eliminar registros
    return EliminarRegistrosPorCondiciones(nombre_tabla, condiciones);
}

void GestorTablasAvanzado::ImprimirEstadisticas() const {
    std::cout << "\n=== ESTADÍSTICAS DEL GESTOR DE TABLAS AVANZADO ===" << std::endl;
    std::cout << "Espacios disponibles registrados: " << espacios_disponibles_.size() << std::endl;
    
    if (!espacios_disponibles_.empty()) {
        uint32_t espacio_total = 0;
        uint32_t espacios_fijos = 0;
        uint32_t espacios_variables = 0;
        
        for (const auto& espacio : espacios_disponibles_) {
            espacio_total += espacio.tamaño_disponible;
            if (espacio.es_tabla_fija) {
                espacios_fijos++;
            } else {
                espacios_variables++;
            }
        }
        
        std::cout << "Espacio total disponible: " << espacio_total << " bytes" << std::endl;
        std::cout << "Espacios fijos: " << espacios_fijos << std::endl;
        std::cout << "Espacios variables: " << espacios_variables << std::endl;
    }
    std::cout << "---------------------------------------------------" << std::endl;
}

// === MÉTODOS DE CONVERSIÓN ===

std::string GestorTablasAvanzado::TipoAString(TipoDatoTabla tipo) const {
    switch (tipo) {
        case TipoDatoTabla::INT: return "INT";
        case TipoDatoTabla::STR: return "STR";
        case TipoDatoTabla::CHAR: return "CHAR";
        default: return "UNKNOWN";
    }
}

TipoDatoTabla GestorTablasAvanzado::StringATipo(const std::string& tipo_str) const {
    std::string upper_tipo_str = tipo_str;
    std::transform(upper_tipo_str.begin(), upper_tipo_str.end(), upper_tipo_str.begin(), ::toupper);

    if (upper_tipo_str == "INT") return TipoDatoTabla::INT;
    if (upper_tipo_str == "STR") return TipoDatoTabla::STR;
    if (upper_tipo_str.rfind("CHAR", 0) == 0) return TipoDatoTabla::CHAR; // Starts with CHAR
    return TipoDatoTabla::INVALID; // Assuming INVALID exists
}

ColumnMetadata GestorTablasAvanzado::DefinicionAColumnMetadata(const DefinicionColumna& def_col) {
    ColumnMetadata cm;
    std::strncpy(cm.name, def_col.nombre.c_str(), 63);
    cm.name[63] = '\0';
    // Convert TipoDatoTabla to ColumnType
    switch (def_col.tipo) {
        case TipoDatoTabla::INT: cm.type = ColumnType::INT; break;
        case TipoDatoTabla::STR: cm.type = ColumnType::STRING; break;
        case TipoDatoTabla::CHAR: cm.type = ColumnType::CHAR; break;
        default: cm.type = ColumnType::INT; break; // Default or error
    }
    cm.size = def_col.tamaño;
    return cm;
}

std::string GestorTablasAvanzado::Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (std::string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

bool GestorTablasAvanzado::ExisteTabla(const std::string& nombre_tabla) {
    return gestor_catalogo_->ExisteTabla(nombre_tabla);
}

void GestorTablasAvanzado::ImprimirEspaciosDisponibles() const {
    std::cout << "\n=== ESPACIOS DISPONIBLES ===" << std::endl;
    if (espacios_disponibles_.empty()) {
        std::cout << "No hay espacios disponibles registrados." << std::endl;
        return;
    }
    for (const auto& espacio : espacios_disponibles_) {
        std::cout << "  ID Bloque: " << espacio.id_bloque
                  << ", Tamaño Disponible: " << espacio.tamaño_disponible
                  << ", Es Fija: " << (espacio.es_tabla_fija ? "Sí" : "No")
                  << std::endl;
    }
    std::cout << "----------------------------" << std::endl;
}
