// data_storage/gestor_disco.cpp
#include "gestor_disco.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <stdexcept>
#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <iomanip>
#include <cstdlib>
#include <cerrno>
#include <sys/stat.h>

// Incluir las cabeceras necesarias para operaciones de directorio
#ifdef _WIN32
    #include <direct.h>
    #define mkdir _mkdir
    #define UNLINK _unlink
    #define RMDIR _rmdir
#else
    #include <sys/stat.h>
    #include <sys/types.h>
#endif

// Definir PATH_SEPARATOR para compatibilidad multiplataforma
#ifdef _WIN32
#define PATH_SEPARATOR '\\'

#else // _WIN32
#endif
// ============================================
// Funciones auxiliares para sistema de archivos
// ============================================

/**
 * @brief Verifica si una ruta (archivo o directorio) existe
 * @param ruta Ruta a verificar
 * @return true si la ruta existe, false en caso contrario
 */ 
#ifdef _WIN32
#define ACCESS _access
#else
#define ACCESS access
 #endif
bool RutaExiste(const std::string& ruta) {
    return ACCESS(ruta.c_str(), 0) == 0;
}

/**
 * @brief Crea un directorio, incluyendo todos los directorios padres necesarios
 * @param ruta Ruta del directorio a crear
 * @return true si se creó correctamente o ya existía, false en caso de error
 */
bool CrearDirectorios(const std::string& ruta) {
    if (ruta.empty()) return false;
    
    // Si la ruta ya existe, retornar éxito
    if (RutaExiste(ruta)) {
        return true;
    }
    
    // Crear cada directorio en la ruta
    std::string ruta_actual;
    size_t pos = 0;
    
    // Manejar rutas absolutas en Windows (C:\)
#ifdef _WIN32
    if (ruta.length() >= 3 && ruta[1] == ':' && (ruta[2] == '\\' || ruta[2] == '/')) {
        ruta_actual = ruta.substr(0, 3);
        pos = 3;
        
        // Crear la unidad (C:)
        if (mkdir(ruta_actual.c_str()) != 0 && errno != EEXIST) {
            return false;
        }
    }
#endif
    
    // Crear cada directorio en la ruta
    while ((pos = ruta.find_first_of("\\/", pos)) != std::string::npos) {
        ruta_actual = ruta.substr(0, pos++);
        if (ruta_actual.empty()) continue; // Ignorar slashes al inicio
        
        if (mkdir(ruta_actual.c_str()) != 0 && errno != EEXIST) {
            return false;
        }
    }
    
    // Crear el último directorio
    if (mkdir(ruta.c_str()) != 0 && errno != EEXIST) {
        return false;
    }
    
    return true;
}

/**
 * @brief Elimina un archivo o directorio vacío
 * @param ruta Ruta al archivo o directorio a eliminar
 * @return true si se eliminó correctamente, false en caso de error
 */
bool EliminarRuta(const std::string& ruta) {
    if (!RutaExiste(ruta)) {
        return true; // No existe, consideramos éxito
    }
    
    // Intentar eliminar como archivo
    if (remove(ruta.c_str()) == 0) {
        return true; // Éxito al eliminar archivo
    }
    
    // Si falla, intentar eliminar como directorio
    return rmdir(ruta.c_str()) == 0;
}

/**
 * @brief Obtiene el separador de ruta según el sistema operativo
 * @return Carácter separador de ruta ('\\' en Windows, '/' en otros sistemas)
 */
char ObtenerSeparadorRuta() {
#define PATH_SEPARATOR '/'
    return PATH_SEPARATOR;
}

/**
 * @brief Une dos partes de una ruta usando el separador correcto
 * @param base Parte inicial de la ruta
 * @param nombre Parte final de la ruta
 * @return Ruta completa unida
 */
std::string UnirRutas(const std::string& base, const std::string& nombre) {
    if (base.empty()) return nombre;
    if (nombre.empty()) return base;
    
    bool base_termina_con_sep = (base.back() == '\\' || base.back() == '/');
    bool nombre_empieza_con_sep = (nombre.front() == '\\' || nombre.front() == '/');
    
    if (base_termina_con_sep && nombre_empieza_con_sep) {
        return base + nombre.substr(1);
    } else if (!base_termina_con_sep && !nombre_empieza_con_sep) {
        return base + ObtenerSeparadorRuta() + nombre;
    } else {
        return base + nombre;
    }
}

// ============================================
// Implementación de la clase GestorDisco
// ============================================

/**
 * @brief Constructor del GestorDisco
 * @param ruta_base Ruta base donde se almacenarán los discos
 * @param num_platos Número de platos del disco (por defecto 2)
 * @param sectores_por_pista Número de sectores por pista (por defecto 64)
 * @param tamano_sector Tamaño de cada sector en bytes (por defecto 512)
 */
GestorDisco::GestorDisco(const std::string& ruta_base, 
                       uint32_t num_platos,
                       uint32_t sectores_por_pista,
                       uint32_t tamano_sector)
    : ruta_base_(ruta_base), nombre_disco_(""), // Inicializar nombre_disco_
      num_platos_(num_platos),
      sectores_por_pista_(sectores_por_pista),
      tamaño_sector_(tamaño_sector),
      siguiente_id_bloque_(0) {
    
    // Validaciones básicas de parámetros
    if (num_platos_ == 0 || sectores_por_pista_ == 0 || tamaño_sector_ == 0) {
        throw std::invalid_argument("Los parámetros del disco no pueden ser cero");
    }
    // Asegurar que la ruta base termine con separador
    if (!ruta_base_.empty() && ruta_base_.back() != ObtenerSeparadorRuta()) {
        ruta_base_ += ObtenerSeparadorRuta();
    }
    
    // La inicialización de cilindros se hará en CrearEstructuraDisco o CargarMetadatosDisco
    
    std::cout << "GestorDisco: Configurado para " << num_platos_ 
              << " platos, " << sectores_por_pista_ << " sectores/pista, "
              << tamano_sector_ << " bytes/sector" << std::endl;
}

/**
 * @brief Destructor del GestorDisco
 * @details Guarda los metadatos del disco antes de destruir la instancia
 */
GestorDisco::~GestorDisco() {
    std::cout << "GestorDisco: Guardando metadatos del disco antes de la destrucción." << std::endl;
    Estado estado = GuardarMetadatosDisco();
    if (estado != Estado::EXITO) {
        // No se puede usar ObtenerMensajeError aquí directamente, ya que no es un método de GestorDisco
        std::cerr << "Error (Destructor GestorDisco): Fallo al guardar los metadatos del disco." << std::endl;
    }
    
    // Liberar recursos
    cilindros_.clear();
    bloques_activos_.clear();
}

/**
 * @brief Inicializa la estructura de cilindros en memoria
 */
void GestorDisco::InicializarCilindros() {
    // Asegurarse de que num_pistas_ esté inicializado antes de usarlo
    // Si no se carga de metadatos, se puede asumir un valor por defecto o calcularlo
    if (num_pistas_ == 0) { // Asumir un valor si no está cargado
        num_pistas_ = 100; // Ejemplo: 100 pistas por superficie
    }

    cilindros_.reserve(num_pistas_); // Pre-reservar espacio
    for (uint32_t pista = 0; pista < num_pistas_; ++pista) {
        cilindros_.emplace_back(pista, num_platos_, NUM_SUPERFICIES, sectores_por_pista_);
    }
    std::cout << "GestorDisco: Inicializados " << cilindros_.size() 
              << " cilindros con " << num_platos_ << " platos y " 
              << sectores_por_pista_ << " sectores por pista." << std::endl;
}

/**
 * @brief Crea la estructura de directorios para un nuevo disco
 * @return Estado con el resultado de la operación
 */
// Se asume que nombre_disco_ ya ha sido establecido antes de llamar a esta función
// por ejemplo, en el constructor o en un método de inicialización.
// Si no, se necesita un parámetro para el nombre del disco.
// Para este diff, asumimos que nombre_disco_ ya está disponible.

Estado GestorDisco::CrearEstructuraDisco() {
    // Crear directorio base si no existe
    if (!RutaExiste(ruta_base_)) {
        if (!CrearDirectorios(ruta_base_)) {
            std::cerr << "Error: No se pudo crear el directorio base: " << ruta_base_ << std::endl;
            return Estado::ERROR_IO;
        }
    }
    
    // Crear el directorio específico para este disco
    std::string ruta_disco_completa = UnirRutas(ruta_base_, nombre_disco_);
    if (!CrearDirectorios(ruta_disco_completa)) {
        std::cerr << "Error: No se pudo crear el directorio del disco: " << ruta_disco_completa << std::endl;
        return Estado::ERROR_IO;
    }
    
    // Crear directorio de bloques lógicos
    std::string ruta_bloques = UnirRutas(ruta_disco_completa, "bloques");
    if (!CrearDirectorios(ruta_bloques)) {
        std::cerr << "Error: No se pudo crear el directorio de bloques" << std::endl;
        return Estado::ERROR_IO;
    }
    
    // Crear archivo de metadatos vacío
    std::string ruta_metadata = UnirRutas(ruta_disco_completa, NOMBRE_ARCHIVO_METADATOS);
    std::ofstream archivo_metadata(ruta_metadata);
    if (!archivo_metadata.is_open()) {
        std::cerr << "Error: No se pudo crear el archivo de metadatos" << std::endl;
        return Estado::ERROR_IO;
    }
    
    // Escribir metadatos básicos
    archivo_metadata << "# Metadatos del disco\n";
    archivo_metadata << "nombre_disco=" << nombre_disco_ << "\n";
    archivo_metadata << "num_platos=" << num_platos_ << "\n";
    archivo_metadata << "num_superficies=" << NUM_SUPERFICIES << "\n"; // Hardcoded 2 superficies
    archivo_metadata << "num_pistas=" << num_pistas_ << "\n"; // Asumir num_pistas_ ya está inicializado
    archivo_metadata << "sectores_por_pista=" << sectores_por_pista_ << "\n";
    archivo_metadata << "tamaño_sector=" << tamaño_sector_ << "\n";
    archivo_metadata << "siguiente_id_bloque=0\n";
    fecha_creacion_ = ObtenerTimestampActual();
    ultima_modificacion_ = fecha_creacion_;
    archivo_metadata << "fecha_creacion=" << fecha_creacion_ << "\n";
    archivo_metadata << "ultima_modificacion=" << ultima_modificacion_ << "\n";
    archivo_metadata.close();
    
    std::cout << "GestorDisco: Estructura de disco creada correctamente en " << ruta_disco_completa << std::endl;
    return Estado::EXITO;
}

/**
 * @brief Carga los metadatos del disco desde archivos de texto
 * @return Estado con el resultado de la operación
 */
Estado GestorDisco::CargarMetadatosDisco() {
    std::string ruta_disco_completa = UnirRutas(ruta_base_, nombre_disco_);
    if (!RutaExiste(ruta_disco_completa)) {
        std::cerr << "Error: El directorio del disco no existe: " << ruta_disco_completa << std::endl;
        return Estado::NO_ENCONTRADO;
    }
    
    std::string ruta_metadata = UnirRutas(ruta_disco_completa, NOMBRE_ARCHIVO_METADATOS);
    
    if (!RutaExiste(ruta_metadata)) {
        std::cerr << "Error: Archivo de metadatos no encontrado: " << ruta_metadata << std::endl;
        return Estado::NO_ENCONTRADO;
    }
    
    try {
        std::ifstream archivo_metadata(ruta_metadata);
        if (!archivo_metadata.is_open()) {
            std::cerr << "Error: No se pudo abrir el archivo de metadatos" << std::endl;
            return Estado::ERROR_IO;
        }
        
        // Variables para almacenar los valores leídos (se inicializan con los valores actuales del objeto
        // para que si un campo no se encuentra en el archivo, se mantenga el valor por defecto o el ya configurado)
        std::string nombre_disco_leido = nombre_disco_;
        uint32_t num_platos_leido = 0;
        uint32_t sectores_pista_leido = 0;
        uint32_t num_pistas_leido = 0;
        uint32_t tamaño_sector_leido = 0;
        
        std::string linea;
        while (std::getline(archivo_metadata, linea)) {
            // Ignorar comentarios y líneas vacías
            if (linea.empty() || linea[0] == '#') continue;
            
            // Buscar el signo de igual
            size_t pos_igual = linea.find('=');
            if (pos_igual == std::string::npos) continue;
            
            // Extraer clave y valor
            std::string clave = linea.substr(0, pos_igual);
            std::string valor = linea.substr(pos_igual + 1);
            
            // Procesar cada parámetro
            if (clave == "nombre_disco") {
                nombre_disco_leido = valor;
            } else if (clave == "num_platos") {
                num_platos_leido = std::stoul(valor);
            } else if (clave == "num_pistas") {
                num_pistas_leido = std::stoul(valor);
            } else if (clave == "sectores_por_pista") {
                sectores_pista_leido = std::stoul(valor);
            } else if (clave == "tamano_sector") {
                tamaño_sector_leido = std::stoul(valor);
            } else if (clave == "fecha_creacion") {
                fecha_creacion_ = valor;
            } else if (clave == "ultima_modificacion") {
                ultima_modificacion_ = valor;
            } else if (clave == "siguiente_id_bloque") {
                siguiente_id_bloque_ = std::stoul(valor);
            }
        }
        
        archivo_metadata.close();
        
        // Validar que los valores leídos coincidan con la configuración actual
        // Si el nombre del disco no coincide, es un error fatal.
        if (nombre_disco_leido != nombre_disco_) {
            std::cerr << "Error: El nombre del disco cargado ('" << nombre_disco_leido 
                      << "') no coincide con el nombre esperado ('" << nombre_disco_ << "')." << std::endl;
            return Estado::ERROR_DATOS;
        }

        // Actualizar los miembros de la clase con los valores leídos
        num_platos_ = num_platos_leido;
        num_pistas_ = num_pistas_leido;
        sectores_por_pista_ = sectores_pista_leido;
        tamaño_sector_ = tamaño_sector_leido;

        // Re-inicializar cilindros con los valores cargados
        InicializarCilindros();

        // Cargar el mapeo lógico a físico
        Estado estado_mapeo = CargarMapeoLogicoFisico();
        if (estado_mapeo != Estado::EXITO) {
            std::cerr << "Error al cargar el mapeo lógico a físico: " << std::endl;
            return estado_mapeo;
        }

        // Cargar la información de bloques activos (si aplica, o si se usa un bitmap)
        // En este diseño, bloques_activos_ es un mapa de BlockId a TipoPagina.
        // Se debería cargar desde un archivo separado o reconstruir desde el mapeo.
        // Por simplicidad, si el mapeo ya está cargado, bloques_activos_ se puede poblar desde ahí.
        // Si se usa un archivo de "bloques_activos.txt", se debería cargar aquí.
        Estado estado_bloques_activos = CargarBloquesActivos(); // Asumiendo que esta función existe
        if (estado_bloques_activos != Estado::EXITO) {
            std::cerr << "Error al cargar la información de bloques activos" << std::endl;
            return estado;
        }
        
        std::cout << "GestorDisco: Metadatos cargados correctamente. "
                  << "Siguiente ID de bloque: " << siguiente_id_bloque_ << std::endl;
        
        return Estado::EXITO;
        
    } catch (const std::exception& e) {
        std::cerr << "Excepción al cargar metadatos: " << e.what() << std::endl;
        return Estado::ERROR;
 * @brief Carga la información de bloques activos desde disco
 * @return Estado con el resultado de la operación (EXITO si se cargó o no hay nada que cargar)
 */
Estado GestorDisco::CargarBloquesActivos() {
    std::string ruta_disco_completa = UnirRutas(ruta_base_, nombre_disco_);
    std::string ruta_activos = UnirRutas(ruta_disco_completa, "bloques_activos.txt"); // Archivo para bloques activos

    // Si no existe el archivo, no hay bloques activos
    if (!RutaExiste(ruta_activos)) {
        bloques_activos_.clear();
        return Estado::EXITO;
    }
    
    try {
        std::ifstream archivo_activos(ruta_activos);
        if (!archivo_activos.is_open()) {
            std::cerr << "Error: No se pudo abrir el archivo de bloques activos" << std::endl;
            return Estado::ERROR_IO;
        }
        
        bloques_activos_.clear();
        std::string linea;

        // Leer cada línea con el formato: id_bloque tipo_pagina_int tipo_pagina_string
        while (std::getline(archivo_activos, linea)) {
            if (linea.empty() || linea[0] == '#') continue;
            
            std::istringstream iss(linea);
            std::string id_str, tipo_int_str, tipo_string_str;
            
            if (iss >> id_str >> tipo_int_str >> tipo_string_str) {
                try {
                    BloqueId id_bloque = std::stoul(id_str);
                    TipoPagina tipo_pagina = static_cast<TipoPagina>(std::stoi(tipo_int_str));

                    if (tipo_pagina != TipoPagina::DESCONOCIDO) {
                        bloques_activos_[id_bloque] = tipo_pagina;
                    } else {
                        std::cerr << "Advertencia: Tipo de página desconocido: " << tipo_str << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error al procesar línea de bloques activos: " << linea << std::endl;
                    continue;
                }
            }
        }
        
        archivo_activos.close();
        std::cout << "Cargados " << bloques_activos_.size() << " bloques activos" << std::endl;
        return Estado::EXITO;
        
    } catch (const std::exception& e) {
        std::cerr << "Excepción al cargar bloques activos: " << e.what() << std::endl;
        return Estado::ERROR;
    }
}
    cabecera.timestamp_creacion = ObtenerTimestampActual();
    cabecera.timestamp_modificacion = cabecera.timestamp_creacion;
    
    // Escribir la cabecera vacía
    std::string buffer_cabecera = SerializarCabecera(cabecera);
    archivo_bloque.write(buffer_cabecera.c_str(), buffer_cabecera.size());
    
    // Rellenar el resto del bloque con ceros
    std::vector<char> ceros(tamaño_sector_ - buffer_cabecera.size(), 0);
    archivo_bloque.write(ceros.data(), ceros.size());
    
    archivo_bloque.close();
    
    // Actualizar la lista de bloques activos
    bloques_activos_[id_bloque] = tipo_pagina;
    
    // Actualizar los metadatos del disco
    Estado estado = ActualizarMetadatos();
    if (estado != Estado::EXITO) {
        std::cerr << "Advertencia: No se pudieron actualizar los metadatos después de asignar el bloque" << std::endl;
        // No fallamos la operación por esto, solo registramos la advertencia
    }
    
    std::cout << "GestorDisco: Bloque " << id_bloque << " asignado correctamente (" 
              << TipoPaginaAString(tipo_pagina) << ")" << std::endl;
    
    return Estado::EXITO;
}

/**
 * @brief Obtiene la ruta completa de un archivo de bloque lógico
 * @param id_bloque ID del bloque
 * @return Ruta completa al archivo del bloque
 */
std::string GestorDisco::ObtenerRutaBloque(BloqueId id_bloque) const {
    std::string nombre_archivo = PREFIJO_BLOQUE + std::to_string(id_bloque) + EXTENSION_BLOQUE;
    return UnirRutas(UnirRutas(ruta_base_, "bloques"), nombre_archivo);
}

/**
 * @brief Obtiene la ruta al archivo de metadatos del disco
 * @return Ruta completa al archivo de metadatos
 */
std::string GestorDisco::ObtenerRutaMetadatos() const {
    return UnirRutas(UnirRutas(ruta_base_, nombre_disco_), NOMBRE_ARCHIVO_METADATOS);
}

/**
 * @brief Obtiene la ruta al archivo de bloques activos
 * @return Ruta completa al archivo de bloques activos
 */
std::string GestorDisco::ObtenerRutaBloquesActivos() const { // Esta función ya existe en el archivo
    return UnirRutas(UnirRutas(ruta_base_, "bloques"), "bloques_activos.txt");
}

/**
 * @brief Actualiza los metadatos del disco en el sistema de archivos
 * @return Estado con el resultado de la operación
 */
Estado GestorDisco::ActualizarMetadatos() {
    // La ruta completa del disco
    std::string ruta_disco_completa = UnirRutas(ruta_base_, nombre_disco_);
    
    // Asegurarse de que el directorio del disco existe
    if (!CrearDirectorios(ruta_disco_completa)) {
        // Asumiendo que CrearDirectorios puede manejar la creación de directorios padres
        std::cerr << "Error: No se pudo crear el directorio de metadatos: " << dir_metadata << std::endl;
        return Estado::ERROR_IO;
    }
    
    std::ofstream archivo_metadata(ruta_metadata);
    
    if (!archivo_metadata.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo de metadatos para escritura: " 
                  << ruta_metadata << std::endl;
        return Estado::ERROR_IO;
    }

    // Escribir metadatos básicos
    archivo_metadata << "# Metadatos del disco\n";
    archivo_metadata << "nombre=" << nombre_disco_ << "\n";
    archivo_metadata << "num_platos=" << num_platos_ << "\n";
    archivo_metadata << "num_superficies=" << NUM_SUPERFICIES << "\n";
    archivo_metadata << "num_platos=" << num_platos_ << "\n";
    archivo_metadata << "sectores_por_pista=" << sectores_por_pista_ << "\n";
    archivo_metadata << "tamaño_sector=" << tamaño_sector_ << "\n";
    archivo_metadata << "siguiente_id_bloque=" << siguiente_id_bloque_ << "\n";
    archivo_metadata << "fecha_creacion=" << fecha_creacion_ << "\n";
    archivo_metadata << "ultima_modificacion=" << ObtenerTimestampActual() << "\n";
    
    archivo_metadata.close();
    
    // Actualizar el archivo de bloques activos
    std::string ruta_activos = UnirRutas(ruta_disco_completa, "bloques_activos.txt");
    
    // Asegurarse de que el directorio de bloques existe
    if (!CrearDirectorios(UnirRutas(ruta_disco_completa, "bloques"))) {
        std::cerr << "Error: No se pudo crear el directorio de bloques: " << dir_activos << std::endl;
        return Estado::ERROR_IO;
    }
    
    std::ofstream archivo_activos(ruta_activos);
    
    if (!archivo_activos.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo de bloques activos: " 
                  << ruta_activos << std::endl;
        return Estado::ERROR_IO;
    }
    
    // Escribir la lista de bloques activos
    archivo_activos << "# Lista de bloques activos\n";
    archivo_activos << "# Formato: id_bloque tipo_pagina\n";
    
    for (const auto& par : bloques_activos_) { // bloques_activos_ es un std::unordered_map<BlockId, TipoPagina>
        archivo_activos << par.first << " " << static_cast<int>(par.second) << " " 
                       << TipoPaginaAString(par.second) << "\n";
    }
    
    archivo_activos.close();
    
    return Estado::EXITO;
}

/**
 * @brief Lee un bloque del disco
 * @param id_bloque ID del bloque a leer
 * @param buffer [out] Buffer donde se almacenarán los datos leídos
 * @param tamano Tamaño del buffer (debe ser al menos el tamaño de un sector)
 * @return Estado con el resultado de la operación
 */
Estado GestorDisco::LeerBloque(BloqueId id_bloque, char* buffer, size_t tamano) {
    if (buffer == nullptr || tamano < tamaño_sector_) {
        std::cerr << "Error: Buffer inválido o tamaño insuficiente" << std::endl;
        return Estado::ARGUMENTO_INVALIDO;
    }
    
    // Verificar que el bloque existe y está activo
    if (bloques_activos_.find(id_bloque) == bloques_activos_.end()) {
        std::cerr << "Error: Intento de leer un bloque no asignado: " << id_bloque << std::endl;
        return Estado::NO_ENCONTRADO;
    }
    
    std::string ruta_bloque = ObtenerRutaBloque(id_bloque);
    std::ifstream archivo_bloque(ruta_bloque, std::ios::binary);
    
    if (!archivo_bloque.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo de bloque: " << ruta_bloque << std::endl;
        return Estado::ERROR_IO;
    }
    
    // Leer el contenido del archivo
    archivo_bloque.read(buffer, tamaño_sector_);
    
    if (archivo_bloque.gcount() < static_cast<std::streamsize>(tamaño_sector_)) {
        std::cerr << "Error: Tamaño de archivo inválido para el bloque " << id_bloque << std::endl;
        archivo_bloque.close();
        return Estado::ERROR_DATOS;
    }
    
    archivo_bloque.close();
    
    // Verificar la cabecera del bloque
    CabeceraBloque cabecera; // Asumiendo que CabeceraBloque es una estructura definida
    Estado estado = ParsearCabecera(std::string(buffer, TAMANO_CABECERA_BLOQUE), cabecera);
    
    if (estado != Estado::EXITO) {
        std::cerr << "Error: Cabecera de bloque inválida en el bloque " << id_bloque << std::endl;
        return estado;
    }
    
    // Actualizar el timestamp de último acceso
    cabecera.timestamp_acceso = ObtenerTimestampActual();

    // Escribir la cabecera actualizada en el buffer (para que el llamador la tenga)
    std::string buffer_cabecera_str = SerializarCabecera(cabecera);
    std::memcpy(buffer, buffer_cabecera_str.c_str(), buffer_cabecera_str.size());

    // Persistir la cabecera actualizada en el archivo del bloque
    std::fstream archivo_actualizacion(ruta_bloque, std::ios::in | std::ios::out | std::ios::binary);
    if (archivo_actualizacion.is_open()) {
        archivo_actualizacion.seekp(0, std::ios::beg); // Ir al inicio del archivo
        archivo_actualizacion.write(buffer_cabecera_str.c_str(), buffer_cabecera_str.size());
        archivo_actualizacion.close();
    } else {
            std::cerr << "Advertencia: No se pudo actualizar la marca de tiempo de acceso del bloque " << id_bloque << std::endl;
            // No fallamos la operación por esto, solo registramos la advertencia
        }
    }
    
    return Estado::EXITO;
}

/**
 * @brief Escribe un bloque en el disco
 * @param id_bloque ID del bloque a escribir
 * @param buffer Datos a escribir
 * @param tamano Tamaño de los datos (debe ser como máximo el tamaño de un sector)
 * @return Estado con el resultado de la operación
 */
Estado GestorDisco::EscribirBloque(BloqueId id_bloque, const char* buffer, size_t tamano) {
    if (buffer == nullptr || tamano > tamaño_sector_) {
        std::cerr << "Error: Buffer inválido o tamaño excesivo" << std::endl;
        return Estado::ARGUMENTO_INVALIDO;
    }
    
    // Verificar que el bloque existe y está activo (ya se hace en LeerBloque, pero es bueno repetirlo aquí)
    if (bloques_activos_.find(id_bloque) == bloques_activos_.end()) {
        std::cerr << "Error: Intento de escribir en un bloque no asignado: " << id_bloque << std::endl;
        return Estado::NO_ENCONTRADO;
    }

    
    std::string ruta_bloque = ObtenerRutaBloque(id_bloque);
    std::fstream archivo_bloque(ruta_bloque, std::ios::in | std::ios::out | std::ios::binary);
    
    if (!archivo_bloque.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo de bloque: " << ruta_bloque << std::endl;
        return Estado::ERROR_IO;
    }
    
    // Leer la cabecera actual
    char cabecera_buffer[TAMANO_CABECERA_BLOQUE]; // Usar el tamaño definido para la cabecera
    archivo_bloque.read(cabecera_buffer, sizeof(cabecera_buffer));
    
    if (archivo_bloque.gcount() < static_cast<std::streamsize>(TAMANO_CABECERA_BLOQUE)) {
        std::cerr << "Error: Tamaño de archivo inválido para el bloque " << id_bloque << std::endl;
        archivo_bloque.close();
        return Estado::ERROR_DATOS;
    }
    
    // Parsear la cabecera
    CabeceraBloque cabecera; // Asumiendo que CabeceraBloque es una estructura definida
    Estado estado = ParsearCabecera(std::string(cabecera_buffer, TAMANO_CABECERA_BLOQUE), cabecera);
    
    if (estado != Estado::EXITO) {
        std::cerr << "Error: Cabecera de bloque inválida en el bloque " << id_bloque << std::endl;
        archivo_bloque.close();
        return estado;
    }
    
    // Actualizar la cabecera
    cabecera.tamano_datos = static_cast<uint32_t>(tamano);
    cabecera.timestamp_modificacion = ObtenerTimestampActual();
    cabecera.timestamp_acceso = cabecera.timestamp_modificacion;
    
    // Volver al inicio del archivo
    archivo_bloque.seekp(0, std::ios::beg);
    
    // Escribir la cabecera actualizada
    std::string buffer_cabecera = SerializarCabecera(cabecera); // Serializar la cabecera actualizada
    archivo_bloque.write(buffer_cabecera.c_str(), buffer_cabecera.size());
    
    // Escribir los datos
    archivo_bloque.write(buffer, tamano);
    
    // Rellenar con ceros si es necesario
    if (tamano < tamaño_sector_ - buffer_cabecera.size()) {
        std::vector<char> ceros(tamaño_sector_ - buffer_cabecera.size() - tamano, 0);
        archivo_bloque.write(ceros.data(), ceros.size());
    }
    
    archivo_bloque.close();
    
    return Estado::EXITO;
}

// Guarda los metadatos del disco en archivos de texto
Status GestorDisco::GuardarMetadatosDisco() { // Esta función ya existe en el archivo
    // Actualizar el archivo de metadatos principal
    std::string ruta_metadata = ObtenerRutaMetadatos();

    try {
        std::ofstream archivo_metadata(ruta_metadata);
        if (!archivo_metadata.is_open()) {
            std::cerr << "Error: No se pudo abrir el archivo de metadatos para escritura: " 
                      << ruta_metadata << std::endl;
            return Estado::ERROR_IO;
        }

        // Escribir el siguiente ID de bloque lógico y el mapeo
        archivo_metadata << siguiente_id_bloque_ << std::endl; // Usar siguiente_id_bloque_
        // Escribir el mapeo lógico a físico
        for (const auto& par : mapeo_logico_fisico_) {
            const BlockId& id_bloque = par.first;
            const DireccionFisica& direccion = par.second;
            archivo_metadata << id_bloque << " " << direccion.id_plato << " " 
                           << direccion.id_superficie << " " << direccion.id_pista << " " 
                           << direccion.id_sector << std::endl;
        }

        archivo_metadata.close();
        return Status::OK;
        
    } catch (const std::exception& e) {
        std::cerr << "Error (GuardarMetadatosDisco): " << e.what() << std::endl;
        return Estado::ERROR;
    }
}

/**
 * @brief Libera un bloque lógico en el disco
 * @param id_bloque ID del bloque a liberar
 * @return Estado con el resultado de la operación
 */
Estado GestorDisco::LiberarBloque(BloqueId id_bloque) {
    // Verificar que el bloque existe en el mapeo lógico a físico
    auto it_mapeo = mapeo_logico_fisico_.find(id_bloque);
    if (it_mapeo == mapeo_logico_fisico_.end()) {
        std::cerr << "Advertencia: Intento de liberar un bloque no asignado: " << id_bloque << std::endl;
        return Estado::NO_ENCONTRADO;
    }

    // Eliminar el archivo del bloque
    std::string ruta_bloque = ObtenerRutaBloque(id_bloque);
    
    if (EliminarArchivo(ruta_bloque)) {
        // Eliminar de la lista de bloques activos
        bloques_activos_.erase(it);
        
        // Marcar los sectores físicos como libres en la estructura de cilindros
        const DireccionFisica& dir_fisica = it_mapeo->second;
        // Asumiendo que un bloque lógico ocupa un sector físico para simplificar
        // Si un bloque ocupa múltiples sectores, se necesitaría un bucle
        cilindros_[dir_fisica.id_pista].sectores_ocupados[dir_fisica.id_plato][dir_fisica.id_superficie * sectores_por_pista_ + dir_fisica.id_sector] = false;
        cilindros_[dir_fisica.id_pista].sectores_libres_total++;

        // Eliminar del mapeo lógico a físico
        mapeo_logico_fisico_.erase(it_mapeo);

        Estado estado = ActualizarMetadatos();
        if (estado != Estado::EXITO) {
            std::cerr << "Advertencia: No se pudieron actualizar los metadatos después de liberar el bloque" << std::endl;
            // No fallamos la operación por esto, solo registramos la advertencia
        }
        
        std::cout << "GestorDisco: Bloque " << id_bloque << " liberado correctamente" << std::endl;
        return Estado::EXITO;
    } else {
        std::cerr << "Error: No se pudo eliminar el archivo del bloque: " << ruta_bloque << std::endl;
        return Estado::ERROR_IO;
    }
}

/**
 * @brief Obtiene el número máximo de bloques que puede almacenar el disco
 * @return Número máximo de bloques
 */
uint32_t GestorDisco::ObtenerMaximoBloques() const {
    // Asumimos que el disco puede almacenar hasta 1 millón de bloques como máximo
    // Esto se puede ajustar según las necesidades del sistema
    return 1000000;
}

/**
 * @brief Obtiene el número de bloques actualmente en uso
 * @return Número de bloques en uso
 */
uint32_t GestorDisco::ObtenerBloquesEnUso() const {
    return static_cast<uint32_t>(bloques_activos_.size());
}

/**
 * @brief Obtiene el espacio total del disco en bytes
 * @return Espacio total en bytes
 */
uint64_t GestorDisco::ObtenerEspacioTotal() const {
    return ObtenerMaximoBloques() * tamaño_sector_;
}

/**
 * @brief Obtiene el espacio utilizado en bytes
 * @return Espacio utilizado en bytes
 */
uint64_t GestorDisco::ObtenerEspacioUtilizado() const {
    return ObtenerBloquesEnUso() * tamaño_sector_;
}

/**
 * @brief Obtiene el espacio disponible en bytes
 * @return Espacio disponible en bytes
 */
uint64_t GestorDisco::ObtenerEspacioDisponible() const {
    return ObtenerEspacioTotal() - ObtenerEspacioUtilizado();
}

/**
 * @brief Obtiene el porcentaje de uso del disco
 * @return Porcentaje de uso (0.0 a 100.0)
 */
double GestorDisco::ObtenerPorcentajeUso() const {
    if (ObtenerEspacioTotal() == 0) return 0.0;
    return (static_cast<double>(ObtenerEspacioUtilizado()) / ObtenerEspacioTotal()) * 100.0;
}

    std::cout << "Número total de cilindros (pistas): " << num_pistas_ << std::endl;
    
    for (uint32_t pista = 0; pista < std::min(static_cast<uint32_t>(5), num_pistas_); ++pista) {
        const auto& cilindro = matrices_cilindros_[pista];
        std::cout << "Cilindro " << pista << " (Pista " << cilindro.numero_pista << "):" << std::endl;
        std::cout << "  Sectores libres: " << cilindro.sectores_libres_total << std::endl;
        std::cout << "  Sectores totales: " << (num_platos_ * num_superficies_por_plato_ * num_sectores_por_pista_) << std::endl;
    }
    
    if (num_pistas_ > 5) {
        std::cout << "... y " << (num_pistas_ - 5) << " cilindros más." << std::endl;
    }
}
