// Catalog_Manager/gestor_catalogo.cpp
#include "gestor_catalogo.h"
#include <iostream>  // Para std::cout, std::cerr
#include <sstream>   // Para std::stringstream
#include <algorithm> // Para std::find_if
#include <iomanip>   // Para std::setw, std::put_time
#include <chrono>    // Para std::chrono::system_clock
#include <ctime>     // Para std::localtime, std::time_t
#include <cstring>   // Para std::strncpy, std::memcpy

// ===== IMPLEMENTACIÓN DE MetadataTabla (Clase Base) =====

MetadataTabla::MetadataTabla(const std::string& nombre_tabla, uint32_t id_tabla)
    : nombre_tabla_(nombre_tabla), id_tabla_(id_tabla), numero_registros_(0) {
    // Constructor base - inicializa valores comunes
}

Status MetadataTabla::AñadirColumna(const std::string& nombre_columna, ColumnType tipo_columna, uint32_t tamaño_columna) {
    // Validar que el nombre de columna no esté vacío
    if (nombre_columna.empty()) {
        std::cerr << "Error: El nombre de la columna no puede estar vacío." << std::endl;
        return Status::INVALID_PARAMETER;
    }

    // Verificar que no exista una columna con el mismo nombre
    if (BuscarColumna(nombre_columna) != -1) {
        std::cerr << "Error: Ya existe una columna con el nombre '" << nombre_columna << "'." << std::endl;
        return Status::DUPLICATE_ENTRY;
    }

    // Validar el tipo de columna para este tipo de tabla
    if (!ValidarTipoColumna(tipo_columna)) {
        std::cerr << "Error: Tipo de columna no válido para este tipo de tabla." << std::endl;
        return Status::INVALID_PARAMETER;
    }

    // Crear la metadata de la columna
    ColumnMetadata nueva_columna;
    std::strncpy(nueva_columna.name, nombre_columna.c_str(), sizeof(nueva_columna.name) - 1);
    nueva_columna.name[sizeof(nueva_columna.name) - 1] = '\0'; // Asegurar terminación nula
    nueva_columna.type = tipo_columna;
    nueva_columna.size = tamaño_columna;

    // Añadir la columna al esquema
    esquema_tabla_.push_back(nueva_columna);

    std::cout << "Columna '" << nombre_columna << "' añadida a la tabla '" << nombre_tabla_ << "'." << std::endl;
    return Status::OK;
}

int32_t MetadataTabla::BuscarColumna(const std::string& nombre_columna) const {
    for (size_t i = 0; i < esquema_tabla_.size(); ++i) {
        if (std::string(esquema_tabla_[i].name) == nombre_columna) {
            return static_cast<int32_t>(i);
        }
    }
    return -1; // No encontrada
}

std::string MetadataTabla::SerializarMetadataComun() const {
    std::stringstream ss;
    
    // Serializar información básica
    ss << "NOMBRE_TABLA:" << nombre_tabla_ << std::endl;
    ss << "ID_TABLA:" << id_tabla_ << std::endl;
    ss << "NUMERO_REGISTROS:" << numero_registros_ << std::endl;
    ss << "NUMERO_COLUMNAS:" << esquema_tabla_.size() << std::endl;
    
    // Serializar esquema de columnas
    ss << "ESQUEMA_INICIO" << std::endl;
    for (const auto& columna : esquema_tabla_) {
        ss << "COLUMNA:" << columna.name << ":" << static_cast<int>(columna.type) << ":" << columna.size << std::endl;
    }
    ss << "ESQUEMA_FIN" << std::endl;
    
    return ss.str();
}

Status MetadataTabla::DeserializarMetadataComun(const std::string& contenido) {
    std::istringstream iss(contenido);
    std::string linea;
    bool en_esquema = false;
    
    esquema_tabla_.clear();
    
    while (std::getline(iss, linea)) {
        if (linea.empty()) continue;
        
        if (linea == "ESQUEMA_INICIO") {
            en_esquema = true;
            continue;
        } else if (linea == "ESQUEMA_FIN") {
            en_esquema = false;
            continue;
        }
        
        if (en_esquema) {
            // Parsear línea de columna: COLUMNA:nombre:tipo:tamaño
            if (linea.substr(0, 8) == "COLUMNA:") {
                std::string resto = linea.substr(8);
                size_t pos1 = resto.find(':');
                size_t pos2 = resto.find(':', pos1 + 1);
                
                if (pos1 != std::string::npos && pos2 != std::string::npos) {
                    std::string nombre = resto.substr(0, pos1);
                    int tipo_int = std::stoi(resto.substr(pos1 + 1, pos2 - pos1 - 1));
                    uint32_t tamaño = std::stoul(resto.substr(pos2 + 1));
                    
                    // Usar el método AñadirColumna para asegurar validación
                    Status add_status = AñadirColumna(nombre, static_cast<ColumnType>(tipo_int), tamaño);
                    if (add_status != Status::OK && add_status != Status::DUPLICATE_ENTRY) {
                        // Si hay un error que no sea duplicado (que es manejado por AñadirColumna), retornar error
                        return add_status;
                    }
                } else {
                    std::cerr << "Error de formato en línea de columna: " << linea << std::endl;
                    return Status::INVALID_FORMAT;
                }
            }
        } else {
            // Parsear líneas de metadata común
            size_t pos = linea.find(':');
            if (pos != std::string::npos) {
                std::string clave = linea.substr(0, pos);
                std::string valor = linea.substr(pos + 1);
                
                if (clave == "NOMBRE_TABLA") {
                    nombre_tabla_ = valor;
                } else if (clave == "ID_TABLA") {
                    id_tabla_ = std::stoul(valor);
                } else if (clave == "NUMERO_REGISTROS") {
                    numero_registros_ = std::stoul(valor);
                }
            }
        }
    }
    
    return Status::OK;
}

bool MetadataTabla::ValidarTipoColumna(ColumnType tipo_columna) const {
    // Implementación base - acepta todos los tipos
    // Las clases derivadas pueden sobrescribir para restricciones específicas
    return tipo_columna == ColumnType::INT || 
           tipo_columna == ColumnType::CHAR || 
           tipo_columna == ColumnType::VARCHAR;
}

// ===== IMPLEMENTACIÓN DE MetadataTablaLongitudFija =====

MetadataTablaLongitudFija::MetadataTablaLongitudFija(const std::string& nombre_tabla, uint32_t id_tabla)
    : MetadataTabla(nombre_tabla, id_tabla), tamaño_registro_completo_(0) {
    // Constructor específico para longitud fija
}

uint32_t MetadataTablaLongitudFija::ObtenerTamañoCampo(uint32_t indice_columna) const {
    if (indice_columna >= tamaños_campos_.size()) {
        return 0;
    }
    return tamaños_campos_[indice_columna];
}

uint32_t MetadataTablaLongitudFija::ObtenerOffsetCampo(uint32_t indice_columna) const {
    if (indice_columna >= offsets_campos_.size()) {
        return 0;
    }
    return offsets_campos_[indice_columna];
}

void MetadataTablaLongitudFija::RecalcularTamaños() {
    tamaños_campos_.clear();
    offsets_campos_.clear();
    tamaño_registro_completo_ = 0;
    
    for (const auto& columna : esquema_tabla_) {
        uint32_t tamaño_campo = CalcularTamañoCampo(columna);
        
        // Guardar offset actual
        offsets_campos_.push_back(tamaño_registro_completo_);
        
        // Guardar tamaño del campo
        tamaños_campos_.push_back(tamaño_campo);
        
        // Actualizar tamaño total
        tamaño_registro_completo_ += tamaño_campo;
    }
    
    std::cout << "Tabla '" << nombre_tabla_ << "': Tamaño de registro recalculado a " 
              << tamaño_registro_completo_ << " bytes." << std::endl;
}

uint32_t MetadataTablaLongitudFija::CalcularTamañoCampo(const ColumnMetadata& metadata) const {
    switch (metadata.type) {
        case ColumnType::INT:
            return sizeof(int32_t); // 4 bytes para enteros
        case ColumnType::CHAR:
            return metadata.size; // Tamaño fijo especificado
        case ColumnType::VARCHAR:
            // Para longitud fija, VARCHAR se trata como CHAR del tamaño máximo
            // Esto es una simplificación; en un SGBD real, VARCHAR en longitud fija
            // podría tener un tamaño predefinido o no ser permitido.
            return metadata.size;
        default:
            return 0;
    }
}

std::string MetadataTablaLongitudFija::SerializarMetadataEspecifica() const {
    std::stringstream ss;
    
    ss << "TIPO_TABLA:LONGITUD_FIJA" << std::endl;
    ss << "TAMAÑO_REGISTRO_COMPLETO:" << tamaño_registro_completo_ << std::endl;
    ss << "NUMERO_CAMPOS:" << tamaños_campos_.size() << std::endl;
    
    // Serializar tamaños de campos
    ss << "TAMAÑOS_CAMPOS:";
    for (size_t i = 0; i < tamaños_campos_.size(); ++i) {
        if (i > 0) ss << ",";
        ss << tamaños_campos_[i];
    }
    ss << std::endl;
    
    // Serializar offsets de campos
    ss << "OFFSETS_CAMPOS:";
    for (size_t i = 0; i < offsets_campos_.size(); ++i) {
        if (i > 0) ss << ",";
        ss << offsets_campos_[i];
    }
    ss << std::endl;
    
    return ss.str();
}

Status MetadataTablaLongitudFija::DeserializarMetadataEspecifica(const std::string& contenido) {
    std::istringstream iss(contenido);
    std::string linea;
    
    while (std::getline(iss, linea)) {
        if (linea.empty()) continue;
        
        size_t pos = linea.find(':');
        if (pos != std::string::npos) {
            std::string clave = linea.substr(0, pos);
            std::string valor = linea.substr(pos + 1);
            
            if (clave == "TAMAÑO_REGISTRO_COMPLETO") {
                tamaño_registro_completo_ = std::stoul(valor);
            } else if (clave == "TAMAÑOS_CAMPOS") {
                // Parsear lista de tamaños separados por comas
                std::istringstream tamaños_stream(valor);
                std::string tamaño_str;
                tamaños_campos_.clear();
                
                while (std::getline(tamaños_stream, tamaño_str, ',')) {
                    tamaños_campos_.push_back(std::stoul(tamaño_str));
                }
            } else if (clave == "OFFSETS_CAMPOS") {
                // Parsear lista de offsets separados por comas
                std::istringstream offsets_stream(valor);
                std::string offset_str;
                offsets_campos_.clear();
                
                while (std::getline(offsets_stream, offset_str, ',')) {
                    offsets_campos_.push_back(std::stoul(offset_str));
                }
            }
        }
    }
    
    return Status::OK;
}

// ===== IMPLEMENTACIÓN DE MetadataTablaLongitudVariable =====

MetadataTablaLongitudVariable::MetadataTablaLongitudVariable(const std::string& nombre_tabla, uint32_t id_tabla)
    : MetadataTabla(nombre_tabla, id_tabla), tamaño_estimado_promedio_(0) {
    // Constructor específico para longitud variable
}

void MetadataTablaLongitudVariable::ActualizarIndicesLongitud(uint32_t indice_columna, uint32_t longitud_valor) {
    // Asegurar que los vectores tengan el tamaño correcto
    while (longitudes_minimas_.size() <= indice_columna) {
        InicializarLongitudesColumna();
    }
    
    // Actualizar mínimo
    if (longitud_valor < longitudes_minimas_[indice_columna]) {
        longitudes_minimas_[indice_columna] = longitud_valor;
    }
    
    // Actualizar máximo
    if (longitud_valor > longitudes_maximas_[indice_columna]) {
        longitudes_maximas_[indice_columna] = longitud_valor;
    }
    
    // Recalcular tamaño estimado
    RecalcularTamañoEstimado();
}

uint32_t MetadataTablaLongitudVariable::ObtenerLongitudMinima(uint32_t indice_columna) const {
    if (indice_columna >= longitudes_minimas_.size()) {
        return 0;
    }
    return longitudes_minimas_[indice_columna];
}

uint32_t MetadataTablaLongitudVariable::ObtenerLongitudMaxima(uint32_t indice_columna) const {
    if (indice_columna >= longitudes_maximas_.size()) {
        return 0;
    }
    return longitudes_maximas_[indice_columna];
}

std::pair<uint32_t, uint32_t> MetadataTablaLongitudVariable::CalcularAreaSegura(uint32_t indice_columna) const {
    if (indice_columna >= longitudes_minimas_.size()) {
        return {0, 0};
    }
    
    uint32_t minimo = longitudes_minimas_[indice_columna];
    uint32_t maximo = longitudes_maximas_[indice_columna];
    
    // Calcular área segura: desde el mínimo hasta un punto antes del máximo
    // donde es seguro que no hay delimitadores
    uint32_t area_segura_inicio = minimo;
    uint32_t area_segura_fin = minimo + ((maximo - minimo) / 2);
    
    return {area_segura_inicio, area_segura_fin};
}

uint32_t MetadataTablaLongitudVariable::EstimarTamañoPromedio() const {
    return tamaño_estimado_promedio_;
}

uint32_t MetadataTablaLongitudVariable::CalcularTamañoRegistro() const {
    // Para longitud variable, retornar el tamaño estimado promedio
    return tamaño_estimado_promedio_;
}

void MetadataTablaLongitudVariable::InicializarLongitudesColumna() {
    longitudes_minimas_.push_back(UINT32_MAX); // Inicializar con valor máximo
    longitudes_maximas_.push_back(0);          // Inicializar con valor mínimo
}

void MetadataTablaLongitudVariable::RecalcularTamañoEstimado() {
    uint32_t tamaño_total = 0;
    
    for (size_t i = 0; i < esquema_tabla_.size() && i < longitudes_maximas_.size(); ++i) {
        const auto& columna = esquema_tabla_[i];
        
        switch (columna.type) {
            case ColumnType::INT:
                tamaño_total += sizeof(int32_t);
                break;
            case ColumnType::CHAR:
                tamaño_total += columna.size;
                break;
            case ColumnType::VARCHAR:
                // Usar promedio entre mínimo y máximo
                uint32_t min_len = (longitudes_minimas_[i] == UINT32_MAX) ? 0 : longitudes_minimas_[i];
                uint32_t max_len = longitudes_maximas_[i];
                tamaño_total += (min_len + max_len) / 2;
                break;
        }
    }
    
    // Añadir overhead por delimitadores (estimado)
    tamaño_total += esquema_tabla_.size() * 2; // 2 bytes por delimitador (estimado)
    
    tamaño_estimado_promedio_ = tamaño_total;
}

std::string MetadataTablaLongitudVariable::SerializarMetadataEspecifica() const {
    std::stringstream ss;
    
    ss << "TIPO_TABLA:LONGITUD_VARIABLE" << std::endl;
    ss << "TAMAÑO_ESTIMADO_PROMEDIO:" << tamaño_estimado_promedio_ << std::endl;
    
    // Serializar longitudes mínimas
    ss << "LONGITUDES_MINIMAS:";
    for (size_t i = 0; i < longitudes_minimas_.size(); ++i) {
        if (i > 0) ss << ",";
        ss << longitudes_minimas_[i];
    }
    ss << std::endl;
    
    // Serializar longitudes máximas
    ss << "LONGITUDES_MAXIMAS:";
    for (size_t i = 0; i < longitudes_maximas_.size(); ++i) {
        if (i > 0) ss << ",";
        ss << longitudes_maximas_[i];
    }
    ss << std::endl;
    
    return ss.str();
}

Status MetadataTablaLongitudVariable::DeserializarMetadataEspecifica(const std::string& contenido) {
    std::istringstream iss(contenido);
    std::string linea;
    
    while (std::getline(iss, linea)) {
        if (linea.empty()) continue;
        
        size_t pos = linea.find(':');
        if (pos != std::string::npos) {
            std::string clave = linea.substr(0, pos);
            std::string valor = linea.substr(pos + 1);
            
            if (clave == "TAMAÑO_ESTIMADO_PROMEDIO") {
                tamaño_estimado_promedio_ = std::stoul(valor);
            } else if (clave == "LONGITUDES_MINIMAS") {
                std::istringstream stream(valor);
                std::string item;
                longitudes_minimas_.clear();
                
                while (std::getline(stream, item, ',')) {
                    longitudes_minimas_.push_back(std::stoul(item));
                }
            } else if (clave == "LONGITUDES_MAXIMAS") {
                std::istringstream stream(valor);
                std::string item;
                longitudes_maximas_.clear();
                
                while (std::getline(stream, item, ',')) {
                    longitudes_maximas_.push_back(std::stoul(item));
                }
            }
        }
    }
    
    return Status::OK;
}

// ===== IMPLEMENTACIÓN DE GestorCatalogo =====

GestorCatalogo::GestorCatalogo(std::shared_ptr<GestorDisco> gestor_disco)
    : gestor_disco_(gestor_disco), siguiente_id_tabla_(1), bloque_catalogo_(0) {
    
    if (!gestor_disco_) {
        throw std::invalid_argument("GestorCatalogo: El gestor de disco no puede ser nulo.");
    }
    
    std::cout << "GestorCatalogo: Inicializado correctamente." << std::endl;
}

GestorCatalogo::~GestorCatalogo() {
    // Guardar el catálogo antes de destruir
    Status estado = GuardarCatalogo();
    if (estado != Status::OK) {
        std::cerr << "Error: No se pudo guardar el catálogo al destruir GestorCatalogo." << std::endl;
    }
}

uint32_t GestorCatalogo::CrearTablaLongitudFija(const std::string& nombre_tabla, 
                                               const std::vector<ColumnMetadata>& esquema) {
    
    if (!ValidarNombreTabla(nombre_tabla)) {
        std::cerr << "Error: Nombre de tabla inválido: '" << nombre_tabla << "'." << std::endl;
        return 0;
    }
    
    // Verificar que no exista una tabla con el mismo nombre
    if (BuscarTablaPorNombre(nombre_tabla) != nullptr) {
        std::cerr << "Error: Ya existe una tabla con el nombre '" << nombre_tabla << "'." << std::endl;
        return 0;
    }
    
    // Crear la nueva tabla
    uint32_t id_tabla = GenerarNuevoIdTabla();
    auto nueva_tabla = std::make_shared<MetadataTablaLongitudFija>(nombre_tabla, id_tabla);
    
    // Añadir columnas del esquema
    for (const auto& columna : esquema) {
        // Usar el nombre de columna del ColumnMetadata
        Status estado = nueva_tabla->AñadirColumna(std::string(columna.name), columna.type, columna.size);
        if (estado != Status::OK) {
            std::cerr << "Error: No se pudo añadir la columna '" << columna.name << "'." << std::endl;
            return 0;
        }
    }
    
    // Recalcular tamaños para la tabla de longitud fija
    nueva_tabla->RecalcularTamaños();
    
    // Añadir al catálogo
    tablas_[id_tabla] = nueva_tabla;
    indices_nombres_[nombre_tabla] = id_tabla;
    
    std::cout << "Tabla de longitud fija '" << nombre_tabla << "' creada con ID " << id_tabla << "." << std::endl;
    return id_tabla;
}

uint32_t GestorCatalogo::CrearTablaLongitudVariable(const std::string& nombre_tabla, 
                                                   const std::vector<ColumnMetadata>& esquema) {
    
    if (!ValidarNombreTabla(nombre_tabla)) {
        std::cerr << "Error: Nombre de tabla inválido: '" << nombre_tabla << "'." << std::endl;
        return 0;
    }
    
    // Verificar que no exista una tabla con el mismo nombre
    if (BuscarTablaPorNombre(nombre_tabla) != nullptr) {
        std::cerr << "Error: Ya existe una tabla con el nombre '" << nombre_tabla << "'." << std::endl;
        return 0;
    }
    
    // Crear la nueva tabla
    uint32_t id_tabla = GenerarNuevoIdTabla();
    auto nueva_tabla = std::make_shared<MetadataTablaLongitudVariable>(nombre_tabla, id_tabla);
    
    // Añadir columnas del esquema
    for (const auto& columna : esquema) {
        // Usar el nombre de columna del ColumnMetadata
        Status estado = nueva_tabla->AñadirColumna(std::string(columna.name), columna.type, columna.size);
        if (estado != Status::OK) {
            std::cerr << "Error: No se pudo añadir la columna '" << columna.name << "'." << std::endl;
            return 0;
        }
    }
    
    // Añadir al catálogo
    tablas_[id_tabla] = nueva_tabla;
    indices_nombres_[nombre_tabla] = id_tabla;
    
    std::cout << "Tabla de longitud variable '" << nombre_tabla << "' creada con ID " << id_tabla << "." << std::endl;
    return id_tabla;
}

std::shared_ptr<MetadataTabla> GestorCatalogo::BuscarTablaPorNombre(const std::string& nombre_tabla) {
    auto it = indices_nombres_.find(nombre_tabla);
    if (it != indices_nombres_.end()) {
        return tablas_[it->second];
    }
    return nullptr;
}

std::shared_ptr<MetadataTabla> GestorCatalogo::BuscarTablaPorId(uint32_t id_tabla) {
    auto it = tablas_.find(id_tabla);
    if (it != tablas_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::string> GestorCatalogo::ListarTablas() const {
    std::vector<std::string> nombres_tablas;
    for (const auto& par : indices_nombres_) {
        nombres_tablas.push_back(par.first);
    }
    return nombres_tablas;
}

bool GestorCatalogo::ValidarNombreTabla(const std::string& nombre_tabla) const {
    // Validar que no esté vacío
    if (nombre_tabla.empty()) {
        return false;
    }
    
    // Validar longitud máxima
    if (nombre_tabla.length() > 63) { // Dejar espacio para terminador nulo (si se usa char[64])
        return false;
    }
    
    // Validar caracteres válidos (letras, números, guión bajo)
    for (char c : nombre_tabla) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') { // Usar static_cast<unsigned char> para isalnum
            return false;
        }
    }
    
    // Validar que no empiece con número
    if (std::isdigit(static_cast<unsigned char>(nombre_tabla[0]))) { // Usar static_cast<unsigned char> para isdigit
        return false;
    }
    
    return true;
}

void GestorCatalogo::ImprimirCatalogoCompleto() const {
    std::cout << "\n=== CATÁLOGO COMPLETO ===" << std::endl;
    std::cout << "Número total de tablas: " << tablas_.size() << std::endl;
    std::cout << "Siguiente ID de tabla: " << siguiente_id_tabla_ << std::endl;
    std::cout << std::endl;
    
    if (tablas_.empty()) {
        std::cout << "No hay tablas en el catálogo." << std::endl;
        return;
    }

    for (const auto& par : tablas_) {
        const auto& tabla = par.second;
        std::cout << "Tabla ID " << tabla->ObtenerIdTabla() << ": " << tabla->ObtenerNombreTabla() << std::endl;
        std::cout << "  Tipo: " << (tabla->EsLongitudFija() ? "Longitud Fija" : "Longitud Variable") << std::endl;
        std::cout << "  Registros: " << tabla->ObtenerNumeroRegistros() << std::endl;
        std::cout << "  Columnas: " << tabla->ObtenerNumeroColumnas() << std::endl;
        std::cout << "  Tamaño registro: " << tabla->CalcularTamañoRegistro() << " bytes" << std::endl;
        std::cout << std::endl;
    }
}

// Implementación de EliminarTabla que recibe nombre
Status GestorCatalogo::EliminarTabla(const std::string& nombre_tabla) {
    auto it = indices_nombres_.find(nombre_tabla);
    if (it == indices_nombres_.end()) {
        std::cerr << "Error: No existe una tabla con el nombre '" << nombre_tabla << "'." << std::endl;
        return Status::NOT_FOUND;
    }
    
    uint32_t id_tabla = it->second;
    
    // Eliminar de ambos mapas
    tablas_.erase(id_tabla);
    indices_nombres_.erase(it);
    
    std::cout << "Tabla '" << nombre_tabla << "' eliminada del catálogo." << std::endl;
    return Status::OK;
}

Status GestorCatalogo::CargarCatalogo() {
    // Verificar si existe un bloque de catálogo asignado
    // El bloque_catalogo_ se inicializa a 0, lo que podría indicar que no hay catálogo persistido.
    // En un sistema real, se podría buscar un bloque especial o un archivo de metadatos.
    if (bloque_catalogo_ == 0) {
        // Intentar asignar un nuevo bloque para el catálogo si no hay uno.
        // Esto solo debería pasar la primera vez que se inicia el SGBD.
        BlockId nuevo_bloque;
        Status estado = gestor_disco_->AsignarBloque(PageType::CATALOG, nuevo_bloque);
        if (estado != Status::OK) {
            std::cerr << "Error: No se pudo asignar bloque para el catálogo." << std::endl;
            return estado;
        }
        bloque_catalogo_ = nuevo_bloque;
        std::cout << "Nuevo bloque de catálogo asignado: " << bloque_catalogo_ << std::endl;
        // Si es un catálogo nuevo, no hay nada que cargar, solo se inicializa.
        return Status::OK; 
    }
    
    // Cargar el bloque de catálogo desde disco
    BloqueMemoria bloque_memoria;
    Status estado = gestor_disco_->LeerBloque(bloque_catalogo_, bloque_memoria);
    if (estado != Status::OK) {
        std::cerr << "Error: No se pudo leer el bloque de catálogo " << bloque_catalogo_ << "." << std::endl;
        return estado;
    }
    
    // Deserializar el contenido del catálogo
    // Asegurarse de que el tamaño usado sea válido antes de construir la string
    if (bloque_memoria.tamaño_usado > 0 && bloque_memoria.tamaño_usado <= BLOCK_SIZE) {
        std::string contenido_catalogo(bloque_memoria.data.data(), bloque_memoria.tamaño_usado);
        return DeserializarCatalogo(contenido_catalogo);
    } else {
        std::cerr << "Advertencia: Bloque de catálogo vacío o tamaño usado inválido. Inicializando catálogo vacío." << std::endl;
        // Si el bloque está vacío o corrupto, inicializar un catálogo vacío.
        tablas_.clear();
        indices_nombres_.clear();
        siguiente_id_tabla_ = 1;
        return Status::OK; // Catálogo vacío cargado (o inicializado)
    }
}

Status GestorCatalogo::GuardarCatalogo() {
    // Verificar que tenemos un bloque asignado
    if (bloque_catalogo_ == 0) {
        BlockId nuevo_bloque;
        Status estado = gestor_disco_->AsignarBloque(PageType::CATALOG, nuevo_bloque);
        if (estado != Status::OK) {
            std::cerr << "Error: No se pudo asignar bloque para guardar el catálogo." << std::endl;
            return estado;
        }
        bloque_catalogo_ = nuevo_bloque;
    }
    
    // Serializar el catálogo completo
    std::string contenido_serializado = SerializarCatalogo();
    
    // Verificar que el contenido cabe en un bloque
    if (contenido_serializado.size() > BLOCK_SIZE) {
        std::cerr << "Error: El catálogo es demasiado grande para un solo bloque (" 
                  << contenido_serializado.size() << " bytes > " << BLOCK_SIZE << " bytes)." << std::endl;
        return Status::BUFFER_OVERFLOW;
    }
    
    // Preparar el bloque de memoria
    BloqueMemoria bloque_memoria; // Se inicializa con data.fill(0) y tamaño_usado = 0
    std::memcpy(bloque_memoria.data.data(), contenido_serializado.c_str(), contenido_serializado.size());
    bloque_memoria.tamaño_usado = contenido_serializado.size();
    
    // Escribir al disco
    Status estado = gestor_disco_->EscribirBloque(bloque_catalogo_, bloque_memoria);
    if (estado == Status::OK) {
        std::cout << "Catálogo guardado exitosamente en bloque " << bloque_catalogo_ << "." << std::endl;
    } else {
        std::cerr << "Error: No se pudo guardar el catálogo en disco." << std::endl;
    }
    
    return estado;
}

std::string GestorCatalogo::SerializarCatalogo() const {
    std::stringstream ss;
    
    // Cabecera del catálogo
    ss << "CATALOGO_SGBD_VERSION:1.0" << std::endl;
    ss << "NUMERO_TABLAS:" << tablas_.size() << std::endl;
    ss << "SIGUIENTE_ID_TABLA:" << siguiente_id_tabla_ << std::endl;
    ss << "BLOQUE_CATALOGO:" << bloque_catalogo_ << std::endl;
    ss << "FECHA_GUARDADO:" << ObtenerTimestampActual() << std::endl;
    ss << "===INICIO_TABLAS===" << std::endl;
    
    // Serializar cada tabla
    for (const auto& par : tablas_) {
        const auto& tabla = par.second;
        
        ss << "---INICIO_TABLA---" << std::endl;
        ss << tabla->SerializarMetadataComun();
        ss << tabla->SerializarMetadataEspecifica();
        ss << "---FIN_TABLA---" << std::endl;
    }
    
    ss << "===FIN_TABLAS===" << std::endl;
    
    return ss.str();
}

Status GestorCatalogo::DeserializarCatalogo(const std::string& contenido) {
    std::istringstream iss(contenido);
    std::string linea;
    bool en_tablas = false;
    bool en_tabla_individual = false;
    std::string contenido_tabla_actual;
    
    // Limpiar catálogo actual antes de cargar
    tablas_.clear();
    indices_nombres_.clear();
    
    while (std::getline(iss, linea)) {
        if (linea.empty()) continue;
        
        if (linea == "===INICIO_TABLAS===") {
            en_tablas = true;
            continue;
        } else if (linea == "===FIN_TABLAS===") {
            en_tablas = false;
            if (en_tabla_individual) { // Si una tabla no se cerró correctamente
                std::cerr << "Error: Fin de tablas inesperado, tabla individual no cerrada." << std::endl;
                return Status::INVALID_FORMAT;
            }
            continue;
        } else if (linea == "---INICIO_TABLA---") {
            if (en_tabla_individual) { // Si ya estamos procesando una tabla y encontramos otra
                std::cerr << "Error: Inicio de tabla inesperado, tabla anterior no cerrada." << std::endl;
                return Status::INVALID_FORMAT;
            }
            en_tabla_individual = true;
            contenido_tabla_actual.clear();
            continue;
        } else if (linea == "---FIN_TABLA---") {
            if (!en_tabla_individual) { // Si no estamos procesando una tabla y encontramos fin de tabla
                std::cerr << "Error: Fin de tabla inesperado, no hay tabla en proceso." << std::endl;
                return Status::INVALID_FORMAT;
            }
            en_tabla_individual = false;
            // Procesar la tabla actual
            Status estado = DeserializarTablaIndividual(contenido_tabla_actual);
            if (estado != Status::OK) {
                std::cerr << "Error: No se pudo deserializar una tabla del catálogo. Estado: " << static_cast<int>(estado) << std::endl;
                return estado;
            }
            continue;
        }
        
        if (en_tabla_individual) {
            contenido_tabla_actual += linea + "\n";
        } else if (!en_tablas) { // Procesar cabecera del catálogo
            size_t pos = linea.find(':');
            if (pos != std::string::npos) {
                std::string clave = linea.substr(0, pos);
                std::string valor = linea.substr(pos + 1);
                
                if (clave == "SIGUIENTE_ID_TABLA") {
                    siguiente_id_tabla_ = std::stoul(valor);
                } else if (clave == "BLOQUE_CATALOGO") {
                    bloque_catalogo_ = std::stoul(valor);
                }
                // Otros campos de cabecera como VERSION, NUMERO_TABLAS, FECHA_GUARDADO pueden ser ignorados si no se necesitan
            }
        }
    }
    
    // Si al final en_tabla_individual es true, significa que la última tabla no se cerró
    if (en_tabla_individual) {
        std::cerr << "Error: Archivo de catálogo terminó inesperadamente, última tabla no cerrada." << std::endl;
        return Status::INVALID_FORMAT;
    }

    std::cout << "Catálogo cargado: " << tablas_.size() << " tablas." << std::endl;
    return Status::OK;
}

// Implementación de DeserializarTablaIndividual (ahora retorna Status)
Status GestorCatalogo::DeserializarTablaIndividual(const std::string& contenido_tabla) {
    // Primero, determinar el tipo de tabla
    std::string tipo_tabla;
    std::istringstream iss(contenido_tabla);
    std::string linea;
    
    while (std::getline(iss, linea)) {
        if (linea.substr(0, 11) == "TIPO_TABLA:") {
            tipo_tabla = linea.substr(11);
            break;
        }
    }
    
    if (tipo_tabla.empty()) {
        std::cerr << "Error: No se pudo determinar el tipo de tabla en el contenido: " << contenido_tabla.substr(0, 50) << "..." << std::endl;
        return Status::INVALID_FORMAT;
    }
    
    // Crear la tabla según su tipo
    std::shared_ptr<MetadataTabla> nueva_tabla;
    
    if (tipo_tabla == "LONGITUD_FIJA") {
        nueva_tabla = std::make_shared<MetadataTablaLongitudFija>("", 0);
    } else if (tipo_tabla == "LONGITUD_VARIABLE") {
        nueva_tabla = std::make_shared<MetadataTablaLongitudVariable>("", 0);
    } else {
        std::cerr << "Error: Tipo de tabla desconocido: " << tipo_tabla << std::endl;
        return Status::INVALID_FORMAT;
    }
    
    // Deserializar metadata común
    Status estado = nueva_tabla->DeserializarMetadataComun(contenido_tabla);
    if (estado != Status::OK) {
        std::cerr << "Error: No se pudo deserializar metadata común de la tabla. Estado: " << static_cast<int>(estado) << std::endl;
        return estado;
    }
    
    // Deserializar metadata específica
    estado = nueva_tabla->DeserializarMetadataEspecifica(contenido_tabla);
    if (estado != Status::OK) {
        std::cerr << "Error: No se pudo deserializar metadata específica de la tabla. Estado: " << static_cast<int>(estado) << std::endl;
        return estado;
    }
    
    // Añadir al catálogo
    uint32_t id_tabla = nueva_tabla->ObtenerIdTabla();
    std::string nombre_tabla = nueva_tabla->ObtenerNombreTabla();

    // Verificar si ya existe una tabla con este ID o nombre (podría ser un catálogo corrupto)
    if (tablas_.count(id_tabla) || indices_nombres_.count(nombre_tabla)) {
        std::cerr << "Advertencia: Se encontró una tabla duplicada (ID: " << id_tabla << ", Nombre: " << nombre_tabla << ") durante la deserialización. Se ignorará esta entrada." << std::endl;
        return Status::DUPLICATE_ENTRY; // O un error más específico si se desea detener la carga
    }
    
    tablas_[id_tabla] = nueva_tabla;
    indices_nombres_[nombre_tabla] = id_tabla;
    
    // Actualizar siguiente ID si es necesario
    if (id_tabla >= siguiente_id_tabla_) {
        siguiente_id_tabla_ = id_tabla + 1;
    }
    
    return Status::OK;
}

uint32_t GestorCatalogo::GenerarNuevoIdTabla() {
    return siguiente_id_tabla_++;
}

std::string GestorCatalogo::ObtenerTimestampActual() const {
    auto ahora = std::chrono::system_clock::now();
    std::time_t tiempo_t = std::chrono::system_clock::to_time_t(ahora);
    
    std::stringstream ss;
    // std::localtime devuelve un puntero a un objeto estático, no es thread-safe en algunos casos.
    // Para aplicaciones más robustas, se debería usar std::gmtime_r o std::localtime_r si están disponibles.
    ss << std::put_time(std::localtime(&tiempo_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string GestorCatalogo::ObtenerEstadisticas() const {
    std::stringstream ss;
    ss << "Estadísticas del Catálogo:\n";
    ss << "  Número de Tablas: " << ObtenerNumeroTablas() << "\n";
    ss << "  Siguiente ID de Tabla: " << siguiente_id_tabla_ << "\n";
    ss << "  Bloque de Catálogo en Disco: " << bloque_catalogo_ << "\n";
    return ss.str();
}

void GestorCatalogo::ImprimirInformacionTabla(uint32_t id_tabla) const {
    auto it = tablas_.find(id_tabla);
    if (it != tablas_.end()) {
        const auto& tabla = it->second;
        std::cout << "\n=== INFORMACIÓN DE TABLA (ID: " << id_tabla << ") ===" << std::endl;
        std::cout << "  Nombre: " << tabla->ObtenerNombreTabla() << std::endl;
        std::cout << "  Tipo: " << (tabla->EsLongitudFija() ? "Longitud Fija" : "Longitud Variable") << std::endl;
        std::cout << "  Registros: " << tabla->ObtenerNumeroRegistros() << std::endl;
        std::cout << "  Columnas: " << tabla->ObtenerNumeroColumnas() << std::endl;
        std::cout << "  Tamaño registro: " << tabla->CalcularTamañoRegistro() << " bytes" << std::endl;
        
        std::cout << "  Esquema:" << std::endl;
        for (const auto& col : tabla->ObtenerEsquema()) {
            std::cout << "    - " << col.name << " (Tipo: " << static_cast<int>(col.type) << ", Tamaño: " << col.size << ")" << std::endl;
        }
    } else {
        std::cout << "Error: Tabla con ID " << id_tabla << " no encontrada." << std::endl;
    }
}

// ===== IMPLEMENTACIÓN DE MÉTODOS ADICIONALES PARA COMPATIBILIDAD CON MAIN.CPP =====

Status GestorCatalogo::CreateTable(const std::string& nombre_tabla, const std::string& ruta_archivo) {
    // Este método es un placeholder. La lógica real de "crear tabla desde archivo"
    // debería residir en GestorTablasAvanzado, que maneja la lectura del archivo y la inferencia del esquema.
    std::cerr << "Error: CreateTable(nombre, archivo) no implementado directamente en GestorCatalogo. Use GestorTablasAvanzado." << std::endl;
    return Status::ERROR;
}

Status GestorCatalogo::CreateTableWithSchema(const std::string& nombre_tabla, 
                                             const std::vector<std::string>& columnas,
                                             const std::vector<std::string>& tipos) {
    // Este método es un placeholder. La lógica real de "crear tabla con esquema manual"
    // debería residir en GestorTablasAvanzado, que maneja la interacción con el usuario y la conversión de tipos.
    std::cerr << "Error: CreateTableWithSchema no implementado directamente en GestorCatalogo. Use GestorTablasAvanzado." << std::endl;
    return Status::ERROR;
}

Status GestorCatalogo::DropTable(const std::string& nombre_tabla) {
    return EliminarTabla(nombre_tabla);
}

bool GestorCatalogo::TableExists(const std::string& nombre_tabla) {
    return BuscarTablaPorNombre(nombre_tabla) != nullptr;
}

std::string GestorCatalogo::GetTableInfo(const std::string& nombre_tabla) {
    std::shared_ptr<MetadataTabla> tabla = BuscarTablaPorNombre(nombre_tabla);
    if (tabla) {
        std::stringstream ss;
        ss << "Tabla ID: " << tabla->ObtenerIdTabla() << "\n"
           << "Nombre: " << tabla->ObtenerNombreTabla() << "\n"
           << "Tipo: " << (tabla->EsLongitudFija() ? "Longitud Fija" : "Longitud Variable") << "\n"
           << "Registros: " << tabla->ObtenerNumeroRegistros() << "\n"
           << "Columnas: " << tabla->ObtenerNumeroColumnas() << "\n"
           << "Tamaño registro: " << tabla->CalcularTamañoRegistro() << " bytes";
        return ss.str();
    }
    return "Tabla no encontrada.";
}

std::vector<std::string> GestorCatalogo::GetAllTables() {
    return ListarTablas();
}

std::vector<std::pair<std::string, std::string>> GestorCatalogo::GetTableSchema(const std::string& nombre_tabla) {
    std::vector<std::pair<std::string, std::string>> schema_info;
    std::shared_ptr<MetadataTabla> tabla = BuscarTablaPorNombre(nombre_tabla);
    if (tabla) {
        for (const auto& col : tabla->ObtenerEsquema()) {
            std::string type_str;
            if (col.type == ColumnType::INT) type_str = "INT";
            else if (col.type == ColumnType::CHAR) type_str = "CHAR(" + std::to_string(col.size) + ")";
            else if (col.type == ColumnType::VARCHAR) type_str = "VARCHAR(" + std::to_string(col.size) + ")";
            schema_info.push_back({std::string(col.name), type_str});
        }
    }
    return schema_info;
}

uint32_t GestorCatalogo::GetTableId(const std::string& nombre_tabla) {
    auto it = indices_nombres_.find(nombre_tabla);
    if (it != indices_nombres_.end()) {
        return it->second;
    }
    return 0; // 0 indica no encontrado
}

std::string GestorCatalogo::GetTableName(uint32_t id_tabla) {
    auto it = tablas_.find(id_tabla);
    if (it != tablas_.end()) {
        return it->second->ObtenerNombreTabla();
    }
    return ""; // Cadena vacía indica no encontrado
}

Status GestorCatalogo::UpdateRecordCount(const std::string& nombre_tabla, uint32_t nuevo_numero) {
    std::shared_ptr<MetadataTabla> tabla = BuscarTablaPorNombre(nombre_tabla);
    if (tabla) {
        tabla->EstablecerNumeroRegistros(nuevo_numero);
        return Status::OK;
    }
    return Status::NOT_FOUND;
}

uint32_t GestorCatalogo::GetRecordCount(const std::string& nombre_tabla) {
    std::shared_ptr<MetadataTabla> tabla = BuscarTablaPorNombre(nombre_tabla);
    if (tabla) {
        return tabla->ObtenerNumeroRegistros();
    }
    return 0; // 0 si no se encuentra la tabla
}
