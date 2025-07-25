// data_storage/block.h
#ifndef BLOCK_H
#define BLOCK_H

#include "../include/common.h" // Incluye Byte, BlockSizeType, SectorSizeType
#include <vector> // Para std::vector
#include <stdexcept> // Para std::out_of_range
#include <algorithm> // Para std::copy, std::fill

// Clase que representa un bloque de datos.
// Un bloque es la unidad fundamental de E/S entre la memoria y el disco.
// Ahora, el tamaño del bloque es dinámico y se define en el constructor.
class Block {
public:
    // Constructor por defecto. Crea un bloque vacío de tamaño 0.
    Block() : size_(0) {}

    // Constructor que define el tamaño del bloque.
    // block_size: El tamaño en bytes de este bloque.
    Block(BlockSizeType block_size) : size_(block_size) {
        data_.resize(size_, 0); // Redimensiona el vector y lo inicializa con ceros.
    }

    // Constructor que permite inicializar el bloque con datos externos.
    // data_ptr: Puntero a los datos de origen.
    // data_size: Cantidad de bytes a copiar.
    // block_size: El tamaño total del bloque.
    Block(const Byte* data_ptr, BlockSizeType data_size, BlockSizeType block_size) : size_(block_size) {
        if (data_size > size_) {
            // Lanza una excepción si el tamaño de los datos excede el límite del bloque.
            throw std::out_of_range("Block data size exceeds allocated block size");
        }
        data_.resize(size_); // Redimensiona el vector al tamaño total del bloque.
        // Copia los datos del puntero de origen al vector interno del bloque.
        std::copy(data_ptr, data_ptr + data_size, data_.begin());
        // Rellena el resto del bloque con ceros si el tamaño de los datos es menor que el tamaño del bloque.
        std::fill(data_.begin() + data_size, data_.end(), 0);
    }

    // Retorna un puntero constante a los datos del bloque.
    // Permite leer el contenido del bloque.
    const Byte* GetData() const {
        return data_.data();
    }

    // Retorna un puntero no constante a los datos del bloque.
    // Permite modificar el contenido del bloque.
    Byte* GetMutableData() {
        return data_.data();
    }

    // Retorna el tamaño actual del bloque en bytes.
    BlockSizeType GetSize() const {
        return size_;
    }

    // Redimensiona el bloque. Útil si se necesita un bloque de un tamaño diferente.
    // Los datos existentes se perderán.
    void Resize(BlockSizeType new_size) {
        size_ = new_size;
        data_.resize(size_, 0);
    }

private:
    BlockSizeType size_;      // Tamaño del bloque en bytes
    std::vector<Byte> data_;  // Contenedor para los datos del bloque
};

#endif // BLOCK_H
