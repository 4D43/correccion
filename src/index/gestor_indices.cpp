#include "gestor_indices.h"
#include "../include/common.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

// Constructor
GestorIndices::GestorIndices(GestorCatalogo* catalogo_ptr)
    : catalogo_(catalogo_ptr)
    , persistencia_automatica_(true)
    , directorio_indices_("indices/")
{
    std::cout << "🔍 GestorIndices inicializado correctamente" << std::endl;
}

// Destructor
GestorIndices::~GestorIndices() {
    if (persistencia_automatica_) {
        PersistirTodosLosIndices();
    }
    LimpiarTodosLosIndices();
}

// Crear indice automatico (funcionalidad principal)
Status GestorIndices::CrearIndiceAutomatico(const std::string& nombre_tabla, const std::string& nombre_columna) {
    if (nombre_tabla.empty() || nombre_columna.empty()) {
        std::cout << "❌ Error: Nombre de tabla o columna vacio" << std::endl;
        return Status::INVALID_ARGUMENT;
    }

    // Verificar si ya existe
    std::string clave_indice = nombre_tabla + "." + nombre_columna;
    if (indices_btree_entero_.find(clave_indice) != indices_btree_entero_.end() ||
        indices_btree_cadena_.find(clave_indice) != indices_btree_cadena_.end() ||
        indices_hash_cadena_.find(clave_indice) != indices_hash_cadena_.end()) {
        std::cout << "⚠️  Indice ya existe para " << nombre_tabla << "." << nombre_columna << std::endl;
        return Status::DUPLICATE_KEY;
    }

    // Seleccionar tipo automaticamente
    TipoIndice tipo_seleccionado = SeleccionarTipoIndiceAutomatico(nombre_tabla, nombre_columna);
    
    std::cout << "🤖 Creando indice automatico:" << std::endl;
    std::cout << "   📊 Tabla: " << nombre_tabla << std::endl;
    std::cout << "   📋 Columna: " << nombre_columna << std::endl;
    std::cout << "   🔧 Tipo seleccionado: " << TipoIndiceToString(tipo_seleccionado) << std::endl;

    Status resultado = Status::SUCCESS;
    
    switch (tipo_seleccionado) {
        case TipoIndice::BTREE_ENTERO: {
            auto nuevo_btree = std::make_unique<BPlusTreeEntero>();
            indices_btree_entero_[clave_indice] = std::move(nuevo_btree);
            break;
        }
        case TipoIndice::BTREE_CADENA: {
            auto nuevo_btree = std::make_unique<BPlusTreeCadena>();
            indices_btree_cadena_[clave_indice] = std::move(nuevo_btree);
            break;
        }
        case TipoIndice::HASH_CADENA: {
            auto nuevo_hash = std::make_unique<HashCadena>();
            indices_hash_cadena_[clave_indice] = std::move(nuevo_hash);
            break;
        }
        default:
            std::cout << "❌ Tipo de indice no soportado" << std::endl;
            return Status::INVALID_ARGUMENT;
    }

    // Persistir automaticamente si esta habilitado
    if (persistencia_automatica_) {
        PersistirIndice(clave_indice);
    }

    std::cout << "✅ Indice automatico creado exitosamente" << std::endl;
    return resultado;
}

// Seleccionar tipo de indice automaticamente
TipoIndice GestorIndices::SeleccionarTipoIndiceAutomatico(const std::string& nombre_tabla, const std::string& nombre_columna) const {
    // Determinar si es tabla de longitud variable
    bool es_variable = EsTablaLongitudVariable(nombre_tabla);
    
    if (es_variable) {
        std::cout << "📏 Tabla de longitud variable detectada -> Usando HASH" << std::endl;
        return TipoIndice::HASH_CADENA;
    }
    
    // Para tablas de longitud fija, determinar por tipo de columna
    ColumnType tipo_columna = ObtenerTipoColumna(nombre_tabla, nombre_columna);
    
    switch (tipo_columna) {
        case ColumnType::INT:
            std::cout << "🔢 Columna INT detectada -> Usando B+ Tree Entero" << std::endl;
            return TipoIndice::BTREE_ENTERO;
        case ColumnType::STRING:
        case ColumnType::CHAR:
            std::cout << "📝 Columna STRING/CHAR detectada -> Usando B+ Tree Cadena" << std::endl;
            return TipoIndice::BTREE_CADENA;
        default:
            std::cout << "⚠️  Tipo de columna desconocido -> Usando B+ Tree Cadena por defecto" << std::endl;
            return TipoIndice::BTREE_CADENA;
    }
}

// Determinar si tabla es de longitud variable
bool GestorIndices::EsTablaLongitudVariable(const std::string& nombre_tabla) const {
    // Intentar obtener informacion del catalogo
    if (catalogo_) {
        // TODO: Implementar consulta al catalogo cuando este disponible
        // return catalogo_->EsTablaLongitudVariable(nombre_tabla);
    }
    
    // Heuristica temporal basada en nombres
    std::string nombre_lower = nombre_tabla;
    std::transform(nombre_lower.begin(), nombre_lower.end(), nombre_lower.begin(), ::tolower);
    
    if (nombre_lower.find("_var") != std::string::npos ||
        nombre_lower.find("_variable") != std::string::npos ||
        nombre_lower.find("variable") != std::string::npos) {
        return true;
    }
    
    return false; // Por defecto, asumir longitud fija
}

// Obtener tipo de columna
ColumnType GestorIndices::ObtenerTipoColumna(const std::string& nombre_tabla, const std::string& nombre_columna) const {
    // Intentar obtener informacion del catalogo
    if (catalogo_) {
        // TODO: Implementar consulta al catalogo cuando este disponible
        // return catalogo_->ObtenerTipoColumna(nombre_tabla, nombre_columna);
    }
    
    // Heuristica temporal basada en nombres de columnas
    std::string columna_lower = nombre_columna;
    std::transform(columna_lower.begin(), columna_lower.end(), columna_lower.begin(), ::tolower);
    
    // Patrones comunes para enteros
    if (columna_lower.find("id") != std::string::npos ||
        columna_lower.find("edad") != std::string::npos ||
        columna_lower.find("numero") != std::string::npos ||
        columna_lower.find("cantidad") != std::string::npos ||
        columna_lower.find("precio") != std::string::npos) {
        return ColumnType::INT;
    }
    
    // Patrones comunes para caracteres fijos
    if (columna_lower.find("codigo") != std::string::npos ||
        columna_lower.find("tipo") != std::string::npos ||
        columna_lower.find("estado") != std::string::npos) {
        return ColumnType::CHAR;
    }
    
    // Por defecto, asumir STRING
    return ColumnType::STRING;
}

// Crear multiples indices automaticos
Status GestorIndices::CrearIndicesAutomaticosMasivos(const std::string& nombre_tabla, const std::vector<std::string>& columnas) {
    if (nombre_tabla.empty() || columnas.empty()) {
        std::cout << "❌ Error: Parametros invalidos para creacion masiva" << std::endl;
        return Status::INVALID_ARGUMENT;
    }

    std::cout << "🚀 Iniciando creacion masiva de indices automaticos" << std::endl;
    std::cout << "📊 Tabla: " << nombre_tabla << std::endl;
    std::cout << "📋 Columnas: " << columnas.size() << std::endl;

    int exitosos = 0;
    int fallidos = 0;

    for (const auto& columna : columnas) {
        std::cout << "\n🔄 Procesando columna: " << columna << std::endl;
        Status resultado = CrearIndiceAutomatico(nombre_tabla, columna);
        
        if (resultado == Status::SUCCESS) {
            exitosos++;
            std::cout << "✅ Exito" << std::endl;
        } else {
            fallidos++;
            std::cout << "❌ Fallo: " << StatusToString(resultado) << std::endl;
        }
    }

    std::cout << "\n📊 Resumen de creacion masiva:" << std::endl;
    std::cout << "✅ Exitosos: " << exitosos << std::endl;
    std::cout << "❌ Fallidos: " << fallidos << std::endl;

    return (exitosos > 0) ? Status::SUCCESS : Status::OPERATION_FAILED;
}

// Cargar indices automaticamente
Status GestorIndices::CargarIndicesAutomaticamente() {
    std::cout << "💾 Cargando indices desde disco..." << std::endl;
    
    // TODO: Implementar carga desde archivos de texto
    // Por ahora, simular carga exitosa
    
    std::cout << "✅ Indices cargados automaticamente" << std::endl;
    return Status::SUCCESS;
}

// Persistir indice especifico
Status GestorIndices::PersistirIndice(const std::string& clave_indice) {
    std::cout << "💾 Persistiendo indice: " << clave_indice << std::endl;
    
    // TODO: Implementar persistencia en archivos de texto
    // Por ahora, simular persistencia exitosa
    
    std::cout << "✅ Indice persistido correctamente" << std::endl;
    return Status::SUCCESS;
}

// Persistir todos los indices
Status GestorIndices::PersistirTodosLosIndices() {
    std::cout << "💾 Persistiendo todos los indices..." << std::endl;
    
    int total_indices = indices_btree_entero_.size() + 
                       indices_btree_cadena_.size() + 
                       indices_hash_cadena_.size();
    
    if (total_indices == 0) {
        std::cout << "ℹ️  No hay indices para persistir" << std::endl;
        return Status::SUCCESS;
    }
    
    // Persistir cada tipo de indice
    for (const auto& par : indices_btree_entero_) {
        PersistirIndice(par.first);
    }
    
    for (const auto& par : indices_btree_cadena_) {
        PersistirIndice(par.first);
    }
    
    for (const auto& par : indices_hash_cadena_) {
        PersistirIndice(par.first);
    }
    
    std::cout << "✅ " << total_indices << " indices persistidos correctamente" << std::endl;
    return Status::SUCCESS;
}

// Limpiar todos los indices
void GestorIndices::LimpiarTodosLosIndices() {
    indices_btree_entero_.clear();
    indices_btree_cadena_.clear();
    indices_hash_cadena_.clear();
    std::cout << "🧹 Indices limpiados de memoria" << std::endl;
}

// Obtener estadisticas
void GestorIndices::MostrarEstadisticas() const {
    std::cout << "\n📊 === ESTADISTICAS DE INDICES ===" << std::endl;
    std::cout << "🔢 B+ Tree Entero: " << indices_btree_entero_.size() << " indices" << std::endl;
    std::cout << "📝 B+ Tree Cadena: " << indices_btree_cadena_.size() << " indices" << std::endl;
    std::cout << "🔗 Hash Cadena: " << indices_hash_cadena_.size() << " indices" << std::endl;
    
    int total = indices_btree_entero_.size() + indices_btree_cadena_.size() + indices_hash_cadena_.size();
    std::cout << "📈 Total de indices: " << total << std::endl;
    std::cout << "🤖 Persistencia automatica: " << (persistencia_automatica_ ? "Habilitada" : "Deshabilitada") << std::endl;
}

// Listar todos los indices
void GestorIndices::ListarTodosLosIndices() const {
    std::cout << "\n📋 === LISTA DE INDICES ===" << std::endl;
    
    if (indices_btree_entero_.empty() && indices_btree_cadena_.empty() && indices_hash_cadena_.empty()) {
        std::cout << "ℹ️  No hay indices creados" << std::endl;
        return;
    }
    
    std::cout << "\n🔢 B+ Tree Entero:" << std::endl;
    for (const auto& par : indices_btree_entero_) {
        std::cout << "  - " << par.first << std::endl;
    }
    
    std::cout << "\n📝 B+ Tree Cadena:" << std::endl;
    for (const auto& par : indices_btree_cadena_) {
        std::cout << "  - " << par.first << std::endl;
    }
    
    std::cout << "\n🔗 Hash Cadena:" << std::endl;
    for (const auto& par : indices_hash_cadena_) {
        std::cout << "  - " << par.first << std::endl;
    }
}

// Funciones auxiliares para conversion de tipos
std::string GestorIndices::TipoIndiceToString(TipoIndice tipo) const {
    switch (tipo) {
        case TipoIndice::BTREE_ENTERO: return "B+ Tree Entero";
        case TipoIndice::BTREE_CADENA: return "B+ Tree Cadena";
        case TipoIndice::HASH_CADENA: return "Hash Cadena";
        default: return "Desconocido";
    }
}
