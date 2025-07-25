// replacement_policies/lru.cpp
#include "lru.h"
#include <iostream> // Para depuración

void LRUReplacementPolicy::Pin(FrameId frame_id) {
    auto it = lru_map_.find(frame_id);
    if (it != lru_map_.end()) {
        // Si la página está en la lista LRU, removerla porque está anclada y no puede ser desalojada.
        lru_list_.erase(it->second);
        lru_map_.erase(it);
        // std::cout << "LRU: Frame " << frame_id << " pinned and removed from LRU list." << std::endl;
    }
}

void LRUReplacementPolicy::Unpin(FrameId frame_id) {
    // Solo añadir a la lista LRU si no está ya presente (es decir, si no está anclada).
    // Si ya está en la lista (lo cual no debería pasar si Pin() funciona correctamente),
    // se moverá al final en Access().
    // Aquí simplemente nos aseguramos de que esté en el mapa para futuras llamadas a Access.
    // La lógica de añadir al final de la lista se maneja en Access().
    // std::cout << "LRU: Frame " << frame_id << " unpinned." << std::endl;
    
    // Si el frame no está en el mapa, lo añadimos al final de la lista LRU.
    // Esto es para asegurar que las páginas desancladas se consideren recientemente usadas.
    // La política de reemplazo solo se preocupa por páginas desancladas.
    Access(frame_id); 
}

void LRUReplacementPolicy::Access(FrameId frame_id) {
    auto it = lru_map_.find(frame_id);
    if (it != lru_map_.end()) {
        // Si la página ya está en la lista, moverla al final (más recientemente usada).
        lru_list_.erase(it->second);
    }
    // Añadir la página al final de la lista.
    lru_list_.push_back(frame_id);
    lru_map_[frame_id] = --lru_list_.end(); // Guardar el iterador al nuevo final
    // std::cout << "LRU: Frame " << frame_id << " accessed and moved to MRU end." << std::endl;
}

FrameId LRUReplacementPolicy::Evict() {
    // Buscar la primera página en la lista que no esté anclada (pin_count == 0).
    // La política LRU solo gestiona páginas desancladas.
    // El BufferManager es responsable de verificar el pin_count.
    // Aquí, simplemente retornamos el elemento al principio de la lista.
    // El BufferManager verificará el pin_count antes de desalojar.
    
    // Iterar la lista desde el principio (menos recientemente usado)
    for (auto it = lru_list_.begin(); it != lru_list_.end(); ++it) {
        FrameId frame_to_evict = *it;
        // La política LRU solo sugiere un frame. El BufferManager es quien verifica
        // si el frame está anclado o no. Si está anclado, el BufferManager
        // debería pedir otro frame a la política.
        // Para esta implementación simple, asumimos que el primer elemento no anclado
        // es el candidato. Si todos están anclados, el Evict() del BufferManager
        // debería manejarlo.
        
        // En una implementación más robusta, Evict() podría necesitar información
        // sobre el pin_count de cada frame, que no está directamente en la política.
        // Sin embargo, la interfaz IReplacementPolicy no proporciona pin_count.
        // Por lo tanto, la política solo sugiere el LRU, y el BufferManager decide.

        // Si la lista no está vacía, el primer elemento es el LRU.
        if (!lru_list_.empty()) {
            // std::cout << "LRU: Suggesting frame " << frame_to_evict << " for eviction." << std::endl;
            return frame_to_evict; // Retorna el candidato LRU
        }
    }
    // std::cout << "LRU: No evictable frames found." << std::endl;
    return (FrameId)-1; // No hay frames desalojables
}

void LRUReplacementPolicy::AddFrame(FrameId frame_id) {
    // Cuando se añade un frame, se considera inicialmente no anclado y no accedido.
    // Se añade a la lista LRU como el más recientemente usado (o se deja para que Access lo haga).
    // Para LRU, lo más sencillo es añadirlo al final y dejar que Access lo gestione.
    // Si queremos que sea "menos recientemente usado" al inicio, no lo añadimos aquí,
    // sino que esperamos a que sea "unpinned" o "accessed" por primera vez.
    // Para la simplicidad, lo añadimos como si fuera recién accedido.
    Access(frame_id);
    // std::cout << "LRU: Frame " << frame_id << " added." << std::endl;
}

void LRUReplacementPolicy::RemoveFrame(FrameId frame_id) {
    auto it = lru_map_.find(frame_id);
    if (it != lru_map_.end()) {
        lru_list_.erase(it->second);
        lru_map_.erase(it);
        // std::cout << "LRU: Frame " << frame_id << " removed from LRU list." << std::endl;
    }
}
