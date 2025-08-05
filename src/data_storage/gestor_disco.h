#ifndef GESTOR_DISCO_H
#define GESTOR_DISCO_H

#include "../include/common.h"     // Tipos básicos y Status
#include "bloque.h"                // Clase Bloque
#include "cabeceras_bloques.h"     // Estructuras de cabeceras
#include <vector>
#include <unordered_map>
#include <array>
#include <memory>
#include <string>
#include <fstream>
#include <sstream>
#include <mutex>
#include <map>

#ifdef _WIN32
    #include <direct.h>  // _mkdir
    #include <io.h>      // _access
#else
    #include <sys/stat.h>
    #include <unistd.h>
#endif

/**
 * @struct DireccionFisica
 * Representa una dirección física en el disco.
 */
struct DireccionFisica {
    uint32_t id_plato;
    uint32_t id_superficie;
    uint32_t id_pista;
    uint32_t id_sector;

    DireccionFisica() : id_plato(0), id_superficie(0), id_pista(0), id_sector(0) {}
    DireccionFisica(uint32_t plato, uint32_t superficie, uint32_t pista, uint32_t sector)
        : id_plato(plato), id_superficie(superficie), id_pista(pista), id_sector(sector) {}

    bool operator==(const DireccionFisica& other) const {
        return id_plato == other.id_plato &&
               id_superficie == other.id_superficie &&
               id_pista == other.id_pista &&
               id_sector == other.id_sector;
    }
};

// Hash specialization for DireccionFisica to be used in unordered_map
namespace std {
    template <>
    struct hash<DireccionFisica> {
        size_t operator()(const DireccionFisica& d) const {
            size_t h1 = hash<uint32_t>()(d.id_plato);
            size_t h2 = hash<uint32_t>()(d.id_superficie);
            size_t h3 = hash<uint32_t>()(d.id_pista);
            size_t h4 = hash<uint32_t>()(d.id_sector);
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
        }
    };
}

/**
 * @struct Sector
 * Representa un sector físico en el disco.
 */
struct Sector {
    std::vector<Byte> datos;
    SectorStatus estado;
    uint64_t timestamp_ultima_escritura;

    Sector(SectorSizeType tamano) : datos(tamano), estado(SectorStatus::FREE), timestamp_ultima_escritura(0) {}
};

/**
 * @struct Pista
 * Representa una pista en una superficie de un plato.
 */
struct Pista {
    uint32_t numero_pista;
    std::vector<Sector> sectores;

    Pista(uint32_t num_pista, uint32_t num_sectores, SectorSizeType tamano_sector)
        : numero_pista(num_pista), sectores(num_sectores, Sector(tamano_sector)) {}
};

/**
 * @struct Superficie
 * Representa una superficie de un plato.
 */
struct Superficie {
    uint32_t numero_superficie;
    std::vector<Pista> pistas; // Pistas dentro de esta superficie

    Superficie(uint32_t num_superficie, uint32_t num_pistas, uint32_t num_sectores_por_pista, SectorSizeType tamano_sector)
        : numero_superficie(num_superficie), pistas(num_pistas, Pista(0, num_sectores_por_pista, tamano_sector)) {
        for (uint32_t i = 0; i < num_pistas; ++i) {
            pistas[i].numero_pista = i;
        }
    }
};

/**
 * @struct Plato
 * Representa un plato de disco.
 */
struct Plato {
    uint32_t numero_plato;
    std::vector<Superficie> superficies; // Superficies de este plato

    Plato(uint32_t num_plato, uint32_t num_superficies_por_plato, uint32_t num_pistas, uint32_t num_sectores_por_pista, SectorSizeType tamano_sector)
        : numero_plato(num_plato), superficies(num_superficies_por_plato, Superficie(0, num_pistas, num_sectores_por_pista, tamano_sector)) {
        for (uint32_t i = 0; i < num_superficies_por_plato; ++i) {
            superficies[i].numero_superficie = i;
        }
    }
};

/**
 * @struct Cilindro
 * Representa un cilindro de disco (conjunto de pistas a la misma distancia del centro).
 */
struct Cilindro {
    uint32_t numero_pista; // El número de pista que define este cilindro
    std::vector<DireccionFisica> sectores_en_cilindro; // Direcciones físicas de todos los sectores en este cilindro

    Cilindro(uint32_t pista_idx) : numero_pista(pista_idx) {}
};


/**
 * @brief Gestor de Disco: Simula el almacenamiento persistente en un disco.
 *
 * Responsabilidades principales:
 * - Gestionar la estructura física del disco (platos, superficies, pistas, sectores).
 * - Asignar y desasignar bloques lógicos a sectores físicos.
 * - Leer y escribir bloques de datos desde/hacia el disco.
 * - Mantener metadatos del disco (espacio, asignaciones, etc.).
 * - Simular operaciones de E/S y latencia.
 */
class GestorDisco {
public:
    /**
     * @brief Constructor del GestorDisco.
     * @param ruta_base Ruta base donde se almacenarán los archivos del disco.
     * @param nombre_disco Nombre del disco (ej. "mydb").
     * @param num_platos Número de platos.
     * @param num_superficies_por_plato Número de superficies por plato.
     * @param num_pistas Número de pistas por superficie.
     * @param sectores_por_pista Número de sectores por pista.
     * @param tamano_sector Tamaño de cada sector en bytes.
     */
    GestorDisco(const std::string& ruta_base, const std::string& nombre_disco,
                uint32_t num_platos, uint32_t num_superficies_por_plato,
                uint32_t num_pistas, uint32_t sectores_por_pista,
                SectorSizeType tamano_sector);

    /**
     * @brief Destructor del GestorDisco.
     * Asegura que todos los metadatos se guarden al cerrar.
     */
    ~GestorDisco();

    /**
     * @brief Inicializa el disco. Crea la estructura de directorios y archivos si no existen.
     * @return Status de la operación.
     */
    Status Inicializar();

    /**
     * @brief Lee un bloque de datos del disco.
     * @param id_bloque ID del bloque a leer.
     * @param buffer Puntero al buffer donde se cargarán los datos.
     * @param tamano_buffer Tamaño del buffer.
     * @return Status de la operación.
     */
    Status LeerBloque(BlockId id_bloque, Byte* buffer, BlockSizeType tamano_buffer);

    /**
     * @brief Escribe un bloque de datos al disco.
     * @param id_bloque ID del bloque a escribir.
     * @param datos Puntero a los datos a escribir.
     * @param tamano_datos Tamaño de los datos.
     * @return Status de la operación.
     */
    Status EscribirBloque(BlockId id_bloque, const Byte* datos, BlockSizeType tamano_datos);

    /**
     * @brief Asigna un nuevo bloque lógico en el disco.
     * @param tipo_pagina Tipo de página para el nuevo bloque.
     * @return El ID del bloque asignado, o 0 si no hay espacio.
     */
    BlockId AsignarBloque(PageType tipo_pagina);

    /**
     * @brief Desasigna un bloque lógico del disco, marcándolo como libre.
     * @param id_bloque ID del bloque a desasignar.
     * @return Status de la operación.
     */
    Status DesasignarBloque(BlockId id_bloque);

    /**
     * @brief Verifica si un bloque existe en el disco.
     * @param id_bloque ID del bloque a verificar.
     * @return true si el bloque existe, false en caso contrario.
     */
    bool ExisteBloque(BlockId id_bloque) const;

    /**
     * @brief Obtiene el número total de bloques en el disco.
     * @return Número total de bloques.
     */
    uint32_t ObtenerMaximoBloques() const;

    /**
     * @brief Obtiene el número de bloques actualmente en uso.
     * @return Número de bloques en uso.
     */
    uint32_t ObtenerBloquesEnUso() const;

    /**
     * @brief Obtiene el número de bloques libres.
     * @return Número de bloques libres.
     */
    uint32_t ObtenerBloquesLibres() const;

    /**
     * @brief Imprime las estadísticas del disco.
     */
    void ImprimirEstadisticas() const;

    /**
     * @brief Obtiene el espacio total del disco en bytes.
     * @return Espacio total en bytes.
     */
    uint64_t ObtenerEspacioTotal() const;

    /**
     * @brief Obtiene el espacio utilizado en bytes.
     * @return Espacio utilizado en bytes.
     */
    uint64_t ObtenerEspacioUtilizado() const;

    /**
     * @brief Obtiene el espacio disponible en bytes.
     * @return Espacio disponible en bytes.
     */
    uint64_t ObtenerEspacioDisponible() const;

    /**
     * @brief Obtiene el porcentaje de uso del disco.
     * @return Porcentaje de uso (0.0 a 100.0).
     */
    double ObtenerPorcentajeUso() const;

private:
    // === Configuración del disco ===
    std::string ruta_base_;
    std::string nombre_disco_;
    uint32_t num_platos_;
    uint32_t num_superficies_por_plato_;
    uint32_t num_pistas_;
    uint32_t sectores_por_pista_;
    uint32_t tamaño_sector_;
    uint32_t siguiente_id_bloque_;
    std::string fecha_creacion_;
    std::string ultima_modificacion_;

    // === Estructuras de control ===
    std::vector<Cilindro> cilindros_;
    std::unordered_map<BlockId, DireccionFisica> mapeo_logico_fisico_;
    std::vector<bool> bloques_libres_;
    std::mutex mutex_;

    // === Métodos auxiliares privados ===
    std::string ObtenerRutaDisco() const;
    std::string ObtenerRutaPista(uint32_t plato, uint32_t superficie, uint32_t pista) const;
    std::string ObtenerRutaSector(const DireccionFisica& direccion) const;
    DireccionFisica ObtenerDireccionFisica(BlockId id_bloque) const;
    Status CrearEstructuraDirectorios() const;
    Status CargarMetadatosDisco();
    Status ActualizarMetadatos();
    Status GuardarMetadatosDisco();
    Status ActualizarFechaModificacion();
    bool EsDireccionValida(const DireccionFisica& direccion) const;
    uint64_t ObtenerTimestampActual() const;
    std::string TimestampToString(uint64_t timestamp) const; // Corrected type to uint64_t
    uint64_t StringToTimestamp(const std::string& timestamp_str) const;

    /**
     * @brief Inicializa la estructura de cilindros del disco.
     */
    void InicializarCilindros();
};

#endif // GESTOR_DISCO_H
