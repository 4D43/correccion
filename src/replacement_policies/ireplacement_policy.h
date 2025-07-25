// replacement_policies/ireplacement_policy.h
#ifndef IREPLACEMENT_POLICY_H
#define IREPLACEMENT_POLICY_H

#include "../include/common.h" // Para FrameId

// Interfaz abstracta para las políticas de reemplazo de páginas del Buffer Pool.
// Este diseño adhiere al Principio Abierto/Cerrado (OCP), permitiendo que
// nuevas políticas de reemplazo puedan ser añadidas sin modificar el BufferManager.
class IReplacementPolicy {
public:
    // Destructor virtual para asegurar la correcta liberación de memoria de clases derivadas.
    virtual ~IReplacementPolicy() = default;

    // Método para notificar a la política que una página ha sido anclada (pinned).
    // frame_id: El ID del frame que ha sido anclado.
    virtual void Pin(FrameId frame_id) = 0;

    // Método para notificar a la política que una página ha sido desanclada (unpinned).
    // frame_id: El ID del frame que ha sido desanclado.
    virtual void Unpin(FrameId frame_id) = 0;

    // Método para notificar a la política que una página ha sido accedida.
    // Esto es crucial para políticas como LRU o CLOCK.
    // frame_id: El ID del frame que ha sido accedido.
    virtual void Access(FrameId frame_id) = 0;

    // Método para obtener el ID del frame que la política sugiere desalojar.
    // Retorna un FrameId válido si hay un frame desalojable, o un valor especial
    // (ej. -1 o un valor que indique "no hay frame desalojable") si no lo hay.
    // Depende de la implementación concreta de la política.
    virtual FrameId Evict() = 0;

    // Método para notificar a la política que un frame ha sido añadido al buffer pool.
    // Esto es útil para inicializar estructuras internas de la política.
    virtual void AddFrame(FrameId frame_id) = 0;

    // Método para notificar a la política que un frame ha sido removido del buffer pool.
    virtual void RemoveFrame(FrameId frame_id) = 0;
};

#endif // IREPLACEMENT_POLICY_H
