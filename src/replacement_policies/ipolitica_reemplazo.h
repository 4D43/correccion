// replacement_policies/ipolitica_reemplazo.h - Interfaz de Políticas de Reemplazo
// Refactorizado completamente a español con documentación detallada

#ifndef IPOLITICA_REEMPLAZO_H
#define IPOLITICA_REEMPLAZO_H

#include "../include/common.h" // Para FrameId, Status

/**
 * @brief Interfaz abstracta para las políticas de reemplazo de páginas del Buffer Pool
 * 
 * Esta interfaz define el contrato que deben cumplir todas las políticas de reemplazo
 * utilizadas por el GestorBuffer. Implementa el Principio Abierto/Cerrado (OCP),
 * permitiendo que nuevas políticas de reemplazo puedan ser añadidas sin modificar
 * el código del GestorBuffer.
 * 
 * POLÍTICAS SOPORTADAS:
 * - LRU (Least Recently Used): Reemplaza la página menos recientemente usada
 * - CLOCK: Algoritmo de reloj con bit de referencia
 * - FIFO: First In, First Out (opcional)
 * - RANDOM: Selección aleatoria (opcional para pruebas)
 */
class IPoliticaReemplazo {
public:
    /**
     * @brief Destructor virtual para asegurar la correcta liberación de memoria
     * de las clases derivadas
     */
    virtual ~IPoliticaReemplazo() = default;

    /**
     * @brief Notifica a la política que una página ha sido anclada (pinned)
     * 
     * Una página anclada no puede ser desalojada del buffer pool hasta que
     * sea desanclada. Esto es crucial para mantener la integridad de los datos
     * mientras se están procesando.
     * 
     * @param id_frame El ID del frame que ha sido anclado
     */
    virtual void Anclar(FrameId id_frame) = 0;

    /**
     * @brief Notifica a la política que una página ha sido desanclada (unpinned)
     * 
     * Una vez desanclada, la página puede ser candidata para desalojo si
     * es necesario liberar espacio en el buffer pool.
     * 
     * @param id_frame El ID del frame que ha sido desanclado
     */
    virtual void Desanclar(FrameId id_frame) = 0;

    /**
     * @brief Notifica a la política que una página ha sido accedida
     * 
     * Este método es crucial para políticas como LRU o CLOCK que necesitan
     * mantener información sobre el patrón de acceso a las páginas para
     * tomar decisiones de reemplazo informadas.
     * 
     * @param id_frame El ID del frame que ha sido accedido
     */
    virtual void Acceder(FrameId id_frame) = 0;

    /**
     * @brief Obtiene el ID del frame que la política sugiere desalojar
     * 
     * Este método implementa la lógica principal de la política de reemplazo.
     * Debe retornar el frame más apropiado para ser desalojado según los
     * criterios de la política específica.
     * 
     * @return FrameId válido si hay un frame desalojable, o INVALID_FRAME_ID
     *         si no hay frames disponibles para desalojo (todos están anclados)
     */
    virtual FrameId Desalojar() = 0;

    /**
     * @brief Notifica a la política que un frame ha sido añadido al buffer pool
     * 
     * Este método permite a la política inicializar sus estructuras internas
     * para el nuevo frame. Es llamado cuando se expande el buffer pool o
     * cuando se inicializa el sistema.
     * 
     * @param id_frame El ID del frame que ha sido añadido
     */
    virtual void AgregarFrame(FrameId id_frame) = 0;

    /**
     * @brief Notifica a la política que un frame ha sido removido del buffer pool
     * 
     * Este método permite a la política limpiar sus estructuras internas
     * relacionadas con el frame removido. Es llamado cuando se reduce el
     * tamaño del buffer pool o durante la terminación del sistema.
     * 
     * @param id_frame El ID del frame que ha sido removido
     */
    virtual void RemoverFrame(FrameId id_frame) = 0;

    /**
     * @brief Reinicia la política a su estado inicial
     * 
     * Este método limpia todas las estructuras internas de la política,
     * efectivamente reiniciándola como si fuera recién creada. Útil para
     * pruebas y reinicializaciones del sistema.
     */
    virtual void Reiniciar() = 0;

    /**
     * @brief Obtiene el nombre de la política de reemplazo
     * 
     * @return std::string con el nombre de la política (ej: "LRU", "CLOCK")
     */
    virtual std::string ObtenerNombre() const = 0;

    /**
     * @brief Obtiene estadísticas de la política de reemplazo
     * 
     * @return std::string con estadísticas relevantes de la política
     */
    virtual std::string ObtenerEstadisticas() const = 0;

    // ===== MÉTODOS ADICIONALES PARA COMPATIBILIDAD CON GESTORBUFFER =====
    
    /**
     * @brief Inicializa la política con el tamaño del pool
     * @param pool_size Tamaño del buffer pool
     */
    virtual void Initialize(uint32_t pool_size) {
        // Implementación por defecto vacía
        (void)pool_size; // Evitar warning de parámetro no usado
    }
    
    /**
     * @brief Registra acceso a un frame (alias para Acceder)
     * @param id_frame ID del frame accedido
     */
    virtual void RecordAccess(FrameId id_frame) {
        Acceder(id_frame);
    }
    
    /**
     * @brief Registra desalojo de un frame
     * @param id_frame ID del frame desalojado
     */
    virtual void RecordEviction(FrameId id_frame) {
        // Implementación por defecto vacía
        (void)id_frame; // Evitar warning de parámetro no usado
    }
    
    /**
     * @brief Encuentra víctima para desalojo (alias para Desalojar)
     * @return ID del frame víctima o -1 si no hay disponible
     */
    virtual FrameId FindVictim() {
        return Desalojar();
    }

    /**
     * @brief Verifica si un frame específico puede ser desalojado
     * 
     * @param id_frame El ID del frame a verificar
     * @return true si el frame puede ser desalojado, false en caso contrario
     */
    virtual bool PuedeSerDesalojado(FrameId id_frame) const = 0;

    /**
     * @brief Obtiene el número de frames actualmente gestionados por la política
     * 
     * @return uint32_t número de frames en la política
     */
    virtual uint32_t ObtenerNumeroFrames() const = 0;

    /**
     * @brief Valida la consistencia interna de la política
     * 
     * Este método verifica que las estructuras internas de la política
     * estén en un estado consistente. Útil para depuración y pruebas.
     * 
     * @return Status::OK si la política está consistente, error en caso contrario
     */
    virtual Status ValidarConsistencia() const = 0;
};

#endif // IPOLITICA_REEMPLAZO_H
