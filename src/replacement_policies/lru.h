// replacement_policies/lru.h
#ifndef LRU_H
#define LRU_H

#include "ireplacement_policy.h" // Incluye la interfaz base
#include <list>                  // Para std::list (usado como lista de acceso reciente)
#include <unordered_map>         // Para mapear FrameId a iterador de lista

// Implementación de la política de reemplazo LRU (Least Recently Used).
// Las páginas más recientemente usadas se mueven al final de la lista.
// Cuando se necesita desalojar, se elige la página al principio de la lista.
class LRUReplacementPolicy : public IReplacementPolicy {
public:
    LRUReplacementPolicy() = default;
    ~LRUReplacementPolicy() override = default;

    // Notifica que una página ha sido anclada.
    // Las páginas ancladas no pueden ser desalojadas, por lo que se remueven de la lista LRU.
    void Pin(FrameId frame_id) override;

    // Notifica que una página ha sido desanclada.
    // Las páginas desancladas se añaden al final de la lista LRU.
    void Unpin(FrameId frame_id) override;

    // Notifica que una página ha sido accedida.
    // Mueve la página al final de la lista (más recientemente usada).
    void Access(FrameId frame_id) override;

    // Obtiene el ID del frame que la política sugiere desalojar (el menos recientemente usado).
    // Retorna -1 si no hay frames desalojables.
    FrameId Evict() override;

    // Añade un frame al conjunto de frames gestionados por la política.
    // Se asume que los frames se añaden inicialmente como desanclados.
    void AddFrame(FrameId frame_id) override;

    // Remueve un frame del conjunto de frames gestionados por la política.
    void RemoveFrame(FrameId frame_id) override;

private:
    // Lista doblemente enlazada para mantener el orden de acceso (LRU al principio, MRU al final).
    std::list<FrameId> lru_list_;
    // Mapa para un acceso rápido a los iteradores de la lista, dado un FrameId.
    std::unordered_map<FrameId, std::list<FrameId>::iterator> lru_map_;
};

#endif // LRU_H
