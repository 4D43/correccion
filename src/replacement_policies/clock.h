// replacement_policies/clock.h
#ifndef CLOCK_H
#define CLOCK_H

#include "ireplacement_policy.h" // Incluye la interfaz base
#include <vector>                // Para std::vector (simula el buffer circular)
#include <unordered_map>         // Para mapear FrameId a su índice en el buffer y bit de referencia

// Implementación de la política de reemplazo CLOCK.
// Utiliza un puntero ("mano del reloj") que avanza por los frames.
// Cada frame tiene un "bit de referencia". Si la mano encuentra un frame con el bit en 1,
// lo pone en 0 y avanza. Si lo encuentra en 0, lo elige para desalojar y avanza.
class ClockReplacementPolicy : public IReplacementPolicy {
public:
    ClockReplacementPolicy() : hand_(0) {} // Inicializa la mano del reloj en la posición 0
    ~ClockReplacementPolicy() override = default;

    // Notifica a la política que una página ha sido anclada (pinned).
    // Las páginas ancladas no pueden ser desalojadas.
    void Pin(FrameId frame_id) override;

    // Notifica a la política que una página ha sido desanclada (unpinned).
    // La página se convierte en candidata para desalojo y su bit de referencia se activa.
    void Unpin(FrameId frame_id) override;

    // Notifica a la política que una página ha sido accedida.
    // Su bit de referencia se activa.
    void Access(FrameId frame_id) override;

    // Obtiene el ID del frame que la política sugiere desalojar.
    // Implementa la lógica del algoritmo CLOCK.
    // Retorna -1 si no hay frames desalojables.
    FrameId Evict() override;

    // Añade un frame al conjunto de frames gestionados por la política.
    // Se inicializa con el bit de referencia en 0.
    void AddFrame(FrameId frame_id) override;

    // Remueve un frame del conjunto de frames gestionados por la política.
    // Limpia estructuras internas.
    void RemoveFrame(FrameId frame_id) override;

private:
    // Estructura para almacenar el estado de cada frame en el algoritmo CLOCK.
    struct ClockFrameInfo {
        FrameId frame_id;
        bool reference_bit; // Bit de referencia (true si ha sido accedido recientemente)
        bool is_pinned;     // Indica si el frame está anclado (no desalojable)

        ClockFrameInfo(FrameId id) : frame_id(id), reference_bit(false), is_pinned(false) {}
    };

    // Vector que simula el buffer circular del reloj.
    // Contiene información sobre los frames en el buffer pool.
    std::vector<ClockFrameInfo> clock_buffer_;
    
    // Mapa para acceder rápidamente a la posición de un FrameId en clock_buffer_.
    std::unordered_map<FrameId, size_t> frame_to_index_map_;

    // Puntero (índice) de la "mano" del reloj en clock_buffer_.
    size_t hand_;
};

#endif // CLOCK_H
