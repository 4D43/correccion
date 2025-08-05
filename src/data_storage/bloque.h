// data_storage/bloque.h
#ifndef BLOQUE_H
#define BLOQUE_H

#include "../include/common.h"  // Para PageType, BlockSizeType, Status
#include "cabeceras_bloques.h"  // Para estructuras de cabeceras unificadas
#include <string>               // Para std::string
#include <vector>               // Para std::vector
#include <cstring>              // Para std::memcpy, std::memset
#include <sstream>              // Para std::stringstream
#include <memory>               // Para std::unique_ptr

/**
 * Clase Bloque: Representa un bloque lógico de datos en el sistema.
 *
 * Responsabilidades principales:
 * - Almacenar datos en formato de texto plano
 * - Manejar cabeceras específicas según el tipo de bloque
 * - Serializar/deserializar contenido desde/hacia archivos .txt
 * - Proporcionar métodos para manipular el contenido del bloque
 * - Gestionar el espacio disponible y la fragmentación
 */
class Bloque {
public:
    /**
     * Constructor por defecto - crea un bloque vacío
     * @param tamano_bloque Tamaño del bloque en bytes
     * @param tipo_pagina Tipo de página que contendrá este bloque
     */
    Bloque(BlockSizeType tamano_bloque = 4096, PageType tipo_pagina = PageType::DATA_PAGE);

    /**
     * Constructor de copia
     */
    Bloque(const Bloque& otro);

    /**
     * Operador de asignación
     */
    Bloque& operator=(const Bloque& otro);

    /**
     * Destructor
     */
    ~Bloque();

    // === MÉTODOS DE GESTIÓN DE DATOS ===

    /**
     * @brief Escribe datos en el bloque en un offset específico
     * @param offset Offset de inicio
     * @param datos Puntero a los datos a escribir
     * @param tamano Tamaño de los datos
     * @return Status de la operación
     */
    Status Escribir(uint32_t offset, const Byte* datos, uint32_t tamano);

    /**
     * @brief Lee datos del bloque desde un offset específico
     * @param offset Offset de inicio
     * @param buffer Puntero al buffer donde se leerán los datos
     * @param tamano Tamaño de los datos a leer
     * @return Status de la operación
     */
    Status Leer(uint32_t offset, Byte* buffer, uint32_t tamano) const;

    /**
     * @brief Obtiene un puntero a los datos internos del bloque
     * @return Puntero a los datos
     */
    Byte* ObtenerDatos();

    /**
     * @brief Obtiene un puntero constante a los datos internos del bloque
     * @return Puntero constante a los datos
     */
    const Byte* ObtenerDatos() const;

    /**
     * @brief Limpia (pone a cero) una porción del bloque
     * @param offset Offset de inicio
     * @param tamano Tamaño a limpiar
     * @return Status de la operación
     */
    Status Limpiar(uint32_t offset, uint32_t tamano);

    /**
     * @brief Limpia (pone a cero) todo el bloque (excepto cabeceras)
     * @return Status de la operación
     */
    Status LimpiarContenido();

    // === MÉTODOS DE GESTIÓN DE CABECERAS ===

    /**
     * @brief Obtiene la cabecera común del bloque
     * @return Referencia a la cabecera común
     */
    CabeceraComun& ObtenerCabeceraComun();

    /**
     * @brief Obtiene la cabecera común del bloque (const)
     * @return Referencia constante a la cabecera común
     */
    const CabeceraComun& ObtenerCabeceraComun() const;

    /**
     * @brief Actualiza el checksum de la cabecera común
     */
    void ActualizarChecksumCabecera();

    /**
     * @brief Valida el checksum de la cabecera común
     * @return true si el checksum es válido, false en caso contrario
     */
    bool ValidarChecksumCabecera() const;

    /**
     * @brief Actualiza el checksum de los datos del bloque
     */
    void ActualizarChecksumDatos();

    /**
     * @brief Valida el checksum de los datos del bloque
     * @return true si el checksum es válido, false en caso contrario
     */
    bool ValidarChecksumDatos() const;

    /**
     * @brief Serializa el contenido del bloque a un string
     * @return String con el contenido serializado
     */
    std::string Serializar() const;

    /**
     * @brief Deserializa el contenido del bloque desde un string
     * @param contenido String con el contenido serializado
     * @return Status de la operación
     */
    Status Deserializar(const std::string& contenido);

    // === MÉTODOS DE GESTIÓN DE ESPACIO ===

    /**
     * @brief Obtiene el tamaño total del bloque
     * @return Tamaño del bloque en bytes
     */
    BlockSizeType ObtenerTamanoTotal() const;

    /**
     * @brief Obtiene los bytes disponibles para datos
     * @return Bytes disponibles
     */
    uint32_t ObtenerBytesDisponibles() const;

    /**
     * @brief Obtiene los bytes usados para datos
     * @return Bytes usados
     */
    uint32_t ObtenerBytesUsados() const;

    /**
     * @brief Actualiza los bytes usados y disponibles
     * @param nuevos_bytes_usados Nuevo valor de bytes usados
     */
    void ActualizarEspacio(uint32_t nuevos_bytes_usados);

    // === PROPIEDADES DEL BLOQUE ===
private: // Moved private members to private section
    BlockSizeType tamano_bloque_;                  // Tamaño total del bloque
    PageType tipo_pagina_;                         // Tipo de página que contiene
    uint32_t bytes_disponibles_;                   // Bytes disponibles para datos
    std::unique_ptr<char[]> datos_;                // Buffer de datos del bloque
    uint32_t checksum_;                            // Checksum para verificación de integridad

    // === MÉTODOS AUXILIARES PRIVADOS ===

    /**
     * Calcula el checksum del bloque
     * @return Valor del checksum
     */
    uint32_t CalcularChecksum() const;

    /**
     * Valida que un offset y tamano sean válidos para el bloque
     * @param offset Posición a validar
     * @param tamano Tamaño a validar
     * @return true si es válido, false en caso contrario
     */
    bool ValidarAcceso(uint32_t offset, uint32_t tamano) const;

    /**
     * Inicializa el buffer de datos
     */
    void InicializarBuffer();

    /**
     * Libera la memoria del buffer de datos
     */
    void LiberarBuffer();

    /**
     * Copia los datos de otro bloque
     */
    void CopiarDatos(const Bloque& otro); // Added missing function declaration
};

// Implementación de métodos inline de la clase Bloque
inline Bloque::Bloque(BlockSizeType tamano_bloque, PageType tipo_pagina)
    : tamano_bloque_(tamano_bloque), 
      tipo_pagina_(tipo_pagina),
      bytes_disponibles_(tamano_bloque - sizeof(CabeceraComun)),
      datos_(new char[tamano_bloque]()), // Corrected this line
      checksum_(0) // Initialize checksum_
{
    // Inicializar la cabecera común
    auto* cabecera = reinterpret_cast<CabeceraComun*>(datos_.get());
    new (cabecera) CabeceraComun();
    cabecera->tamano_bloque_total = tamano_bloque_;
    cabecera->bytes_disponibles = bytes_disponibles_; // Corrected this line
    cabecera->tipo_pagina = tipo_pagina_; // Initialize tipo_pagina
    // Set other common header fields as needed
}

// Add other inline method definitions here if they are part of the original file
// For example, the copy constructor, assignment operator, and destructor if they were inline.

#endif // BLOQUE_H
