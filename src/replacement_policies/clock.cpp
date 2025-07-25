// replacement_policies/clock.cpp
#include "clock.h"
#include <iostream> // Para depuración

void ClockReplacementPolicy::Pin(FrameId frame_id) {
    auto it = frame_to_index_map_.find(frame_id);
    if (it != frame_to_index_map_.end()) {
        clock_buffer_[it->second].is_pinned = true;
        // std::cout << "CLOCK: Frame " << frame_id << " pinned." << std::endl;
    }
}

void ClockReplacementPolicy::Unpin(FrameId frame_id) {
    auto it = frame_to_index_map_.find(frame_id);
    if (it != frame_to_index_map_.end()) {
        clock_buffer_[it->second].is_pinned = false;
        clock_buffer_[it->second].reference_bit = true; // Activar bit de referencia al desanclar
        // std::cout << "CLOCK: Frame " << frame_id << " unpinned. Reference bit set to true." << std::endl;
    }
}

void ClockReplacementPolicy::Access(FrameId frame_id) {
    auto it = frame_to_index_map_.find(frame_id);
    if (it != frame_to_index_map_.end()) {
        clock_buffer_[it->second].reference_bit = true; // Activar bit de referencia
        // std::cout << "CLOCK: Frame " << frame_id << " accessed. Reference bit set to true." << std::endl;
    }
}

FrameId ClockReplacementPolicy::Evict() {
    if (clock_buffer_.empty()) {
        // std::cout << "CLOCK: Buffer is empty, no frames to evict." << std::endl;
        return (FrameId)-1; // No hay frames en el buffer
    }

    // Iterar hasta encontrar un frame desalojable
    size_t initial_hand = hand_;
    while (true) {
        ClockFrameInfo& current_frame = clock_buffer_[hand_];

        if (!current_frame.is_pinned) { // Solo consideramos frames no anclados
            if (current_frame.reference_bit) {
                // Si el bit de referencia está en 1, lo ponemos en 0 y avanzamos la mano.
                current_frame.reference_bit = false;
                // std::cout << "CLOCK: Frame " << current_frame.frame_id << " reference bit reset. Advancing hand." << std::endl;
            } else {
                // Si el bit de referencia está en 0, este es el frame a desalojar.
                FrameId frame_to_evict = current_frame.frame_id;
                // Avanzar la mano antes de retornar
                hand_ = (hand_ + 1) % clock_buffer_.size();
                // std::cout << "CLOCK: Suggesting frame " << frame_to_evict << " for eviction." << std::endl;
                return frame_to_evict;
            }
        }
        
        // Avanzar la mano del reloj
        hand_ = (hand_ + 1) % clock_buffer_.size();

        // Si hemos dado una vuelta completa y no encontramos nada,
        // significa que todas las páginas están ancladas o no hay desalojables.
        if (hand_ == initial_hand) {
            // std::cout << "CLOCK: Full scan completed, no evictable frames found (all pinned or no candidates)." << std::endl;
            return (FrameId)-1; // No hay frames desalojables
        }
    }
}

void ClockReplacementPolicy::AddFrame(FrameId frame_id) {
    // Añadir el frame al final del buffer circular y mapearlo a su índice.
    // Inicialmente no anclado y con bit de referencia en false.
    clock_buffer_.emplace_back(frame_id);
    frame_to_index_map_[frame_id] = clock_buffer_.size() - 1;
    // std::cout << "CLOCK: Frame " << frame_id << " added." << std::endl;
}

void ClockReplacementPolicy::RemoveFrame(FrameId frame_id) {
    auto it = frame_to_index_map_.find(frame_id);
    if (it != frame_to_index_map_.end()) {
        size_t index_to_remove = it->second;

        // Si la mano está en el frame que se va a remover, moverla al siguiente.
        if (hand_ == index_to_remove) {
            hand_ = (hand_ + 1) % clock_buffer_.size();
        }

        // Remover el elemento del vector (esto puede ser costoso para vectores grandes)
        // Para mantener la consistencia del mapa, es mejor copiar el último elemento
        // al lugar del elemento a borrar y luego eliminar el último.
        if (index_to_remove != clock_buffer_.size() - 1) {
            // Mover el último elemento al lugar del elemento a borrar
            clock_buffer_[index_to_remove] = clock_buffer_.back();
            // Actualizar el índice del elemento movido en el mapa
            frame_to_index_map_[clock_buffer_.back().frame_id] = index_to_remove;
        }
        clock_buffer_.pop_back(); // Eliminar el último elemento (que ahora es el duplicado o el original si era el último)
        frame_to_index_map_.erase(it); // Eliminar la entrada del mapa

        // Ajustar la mano si el tamaño del buffer ha cambiado y la mano estaba más allá del nuevo tamaño
        if (hand_ >= clock_buffer_.size() && !clock_buffer_.empty()) {
            hand_ = 0; // O ajustar a (clock_buffer_.size() - 1) si se prefiere
        } else if (clock_buffer_.empty()) {
            hand_ = 0; // Resetear la mano si el buffer está vacío
        }
        // std::cout << "CLOCK: Frame " << frame_id << " removed." << std::endl;
    }
}
