// replacement_policies/lru_espanol.h - Política de Reemplazo LRU en Español
// Implementación de Least Recently Used completamente refactorizada

#ifndef LRU_ESPANOL_H
#define LRU_ESPANOL_H

#include "ipolitica_reemplazo.h"
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <iostream>
#include <sstream>  // <--- ADDED: Para std::ostringstream
#include <iomanip>  // Para std::setprecision

/**
 * @brief Implementación de la política de reemplazo LRU (Least Recently Used)
 * * Esta política mantiene un registro del orden de acceso a las páginas y
 * siempre desaloja la página que ha sido menos recientemente usada entre
 * aquellas que no están ancladas.
 * * ESTRUCTURA DE DATOS:
 * - Lista doblemente enlazada para mantener el orden de acceso
 * - Mapa hash para acceso O(1) a los nodos de la lista
 * - Conjunto de frames anclados que no pueden ser desalojados
 * * COMPLEJIDAD:
 * - Acceder: O(1)
 * - Anclar/Desanclar: O(1)
 * - Desalojar: O(1)
 * - AgregarFrame/RemoverFrame: O(1)
 */
class PoliticaLRU : public IPoliticaReemplazo {
private:
    /**
     * @brief Nodo de la lista LRU que contiene información del frame
     */
    struct NodoLRU {
        FrameId id_frame;
        uint64_t timestamp_acceso;
        uint32_t contador_accesos;
        
        NodoLRU(FrameId id) 
            : id_frame(id), timestamp_acceso(0), contador_accesos(0) {}
    };
    
    // Lista doblemente enlazada para mantener el orden LRU
    // El frente contiene los frames más recientemente usados
    // El final contiene los frames menos recientemente usados
    std::list<NodoLRU> lista_lru_;
    
    // Mapa para acceso rápido a los iteradores de la lista
    std::unordered_map<FrameId, typename std::list<NodoLRU>::iterator> mapa_frames_;
    
    // Conjunto de frames anclados que no pueden ser desalojados
    std::unordered_set<FrameId> frames_anclados_;
    
    // Estadísticas de la política
    struct EstadisticasLRU {
        uint64_t total_accesos = 0;
        uint64_t total_desalojos = 0;
        uint64_t total_anclajes = 0;
        uint64_t total_desanclajes = 0;
        uint64_t frames_agregados = 0;
        uint64_t frames_removidos = 0;
    } estadisticas_;
    
    /**
     * @brief Obtiene el timestamp actual en microsegundos
     * @return uint64_t timestamp actual
     */
    uint64_t ObtenerTimestampActual() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }
    
    /**
     * @brief Mueve un frame al frente de la lista LRU
     * @param iterador Iterador al nodo en la lista
     */
    void MoverAlFrente(typename std::list<NodoLRU>::iterator iterador) {
        // Actualizar información del nodo
        iterador->timestamp_acceso = ObtenerTimestampActual();
        iterador->contador_accesos++;
        
        // Mover al frente de la lista
        lista_lru_.splice(lista_lru_.begin(), lista_lru_, iterador);
    }

public:
    /**
     * @brief Constructor de la política LRU
     */
    PoliticaLRU() {
        std::cout << "Política LRU inicializada correctamente." << std::endl;
    }
    
    /**
     * @brief Destructor de la política LRU
     */
    ~PoliticaLRU() override {
        std::cout << "Política LRU destruida. Estadísticas finales:" << std::endl;
        std::cout << ObtenerEstadisticas() << std::endl;
    }
    
    /**
     * @brief Ancla un frame, impidiendo que sea desalojado
     * @param id_frame ID del frame a anclar
     */
    void Anclar(FrameId id_frame) override {
        frames_anclados_.insert(id_frame);
        estadisticas_.total_anclajes++;
        
        // Si el frame existe en la lista, marcarlo como accedido
        auto it = mapa_frames_.find(id_frame);
        if (it != mapa_frames_.end()) {
            MoverAlFrente(it->second);
        }
    }
    
    /**
     * @brief Desancla un frame, permitiendo que pueda ser desalojado
     * @param id_frame ID del frame a desanclar
     */
    void Desanclar(FrameId id_frame) override {
        frames_anclados_.erase(id_frame);
        estadisticas_.total_desanclajes++;
    }
    
    /**
     * @brief Registra el acceso a un frame
     * @param id_frame ID del frame accedido
     */
    void Acceder(FrameId id_frame) override {
        estadisticas_.total_accesos++;
        
        auto it = mapa_frames_.find(id_frame);
        if (it != mapa_frames_.end()) {
            // El frame existe, moverlo al frente
            MoverAlFrente(it->second);
        } else {
            // El frame no existe, esto podría ser un error
            std::cerr << "Advertencia: Intento de acceder a frame inexistente: " 
                      << id_frame << std::endl;
        }
    }
    
    /**
     * @brief Selecciona un frame para desalojar según la política LRU
     * @return FrameId del frame a desalojar, o INVALID_FRAME_ID si no hay ninguno disponible
     */
    FrameId Desalojar() override {
        // Buscar desde el final de la lista (menos recientemente usado)
        for (auto it = lista_lru_.rbegin(); it != lista_lru_.rend(); ++it) {
            FrameId id_frame = it->id_frame;
            
            // Verificar si el frame puede ser desalojado (no está anclado)
            if (frames_anclados_.find(id_frame) == frames_anclados_.end()) {
                estadisticas_.total_desalojos++;
                return id_frame;
            }
        }
        
        // No hay frames disponibles para desalojar
        return INVALID_FRAME_ID;
    }
    
    /**
     * @brief Agrega un nuevo frame a la política
     * @param id_frame ID del frame a agregar
     */
    void AgregarFrame(FrameId id_frame) override {
        // Verificar que el frame no exista ya
        if (mapa_frames_.find(id_frame) != mapa_frames_.end()) {
            std::cerr << "Advertencia: Intento de agregar frame existente: " 
                      << id_frame << std::endl;
            return;
        }
        
        // Agregar al frente de la lista (más recientemente usado)
        lista_lru_.emplace_front(id_frame);
        auto iterador = lista_lru_.begin();
        iterador->timestamp_acceso = ObtenerTimestampActual();
        
        // Agregar al mapa para acceso rápido
        mapa_frames_[id_frame] = iterador;
        
        estadisticas_.frames_agregados++;
    }
    
    /**
     * @brief Remueve un frame de la política
     * @param id_frame ID del frame a remover
     */
    void RemoverFrame(FrameId id_frame) override {
        auto it = mapa_frames_.find(id_frame);
        if (it != mapa_frames_.end()) {
            // Remover de la lista
            lista_lru_.erase(it->second);
            
            // Remover del mapa
            mapa_frames_.erase(it);
            
            // Remover de frames anclados si estaba anclado
            frames_anclados_.erase(id_frame);
            
            estadisticas_.frames_removidos++;
        } else {
            std::cerr << "Advertencia: Intento de remover frame inexistente: " 
                      << id_frame << std::endl;
        }
    }
    
    /**
     * @brief Reinicia la política a su estado inicial
     */
    void Reiniciar() override {
        lista_lru_.clear();
        mapa_frames_.clear();
        frames_anclados_.clear();
        estadisticas_ = EstadisticasLRU{};
        
        std::cout << "Política LRU reiniciada correctamente." << std::endl;
    }
    
    /**
     * @brief Obtiene el nombre de la política
     * @return std::string nombre de la política
     */
    std::string ObtenerNombre() const override {
        return "LRU (Least Recently Used)";
    }
    
    /**
     * @brief Obtiene estadísticas detalladas de la política
     * @return std::string con las estadísticas formateadas
     */
    std::string ObtenerEstadisticas() const override {
        std::ostringstream ss;
        ss << "\n=== ESTADÍSTICAS POLÍTICA LRU ===\n";
        ss << "Total de accesos: " << estadisticas_.total_accesos << "\n";
        ss << "Total de desalojos: " << estadisticas_.total_desalojos << "\n";
        ss << "Total de anclajes: " << estadisticas_.total_anclajes << "\n";
        ss << "Total de desanclajes: " << estadisticas_.total_desanclajes << "\n";
        ss << "Frames agregados: " << estadisticas_.frames_agregados << "\n";
        ss << "Frames removidos: " << estadisticas_.frames_removidos << "\n";
        ss << "Frames activos: " << lista_lru_.size() << "\n";
        ss << "Frames anclados: " << frames_anclados_.size() << "\n";
        
        if (estadisticas_.total_accesos > 0) {
            double tasa_desalojos = (double)estadisticas_.total_desalojos / estadisticas_.total_accesos * 100.0;
            ss << "Tasa de desalojos: " << std::fixed << std::setprecision(2) << tasa_desalojos << "%\n";
        }
        
        return ss.str();
    }
    
    /**
     * @brief Verifica si un frame puede ser desalojado
     * @param id_frame ID del frame a verificar
     * @return true si puede ser desalojado, false en caso contrario
     */
    bool PuedeSerDesalojado(FrameId id_frame) const override {
        // Un frame puede ser desalojado si existe y no está anclado
        return (mapa_frames_.find(id_frame) != mapa_frames_.end()) &&
               (frames_anclados_.find(id_frame) == frames_anclados_.end());
    }
    
    /**
     * @brief Obtiene el número de frames gestionados
     * @return uint32_t número de frames
     */
    uint32_t ObtenerNumeroFrames() const override {
        return static_cast<uint32_t>(lista_lru_.size());
    }
    
    /**
     * @brief Valida la consistencia interna de la política
     * @return Status::OK si es consistente, error en caso contrario
     */
    Status ValidarConsistencia() const override {
        // Verificar que el tamaño de la lista coincida con el mapa
        if (lista_lru_.size() != mapa_frames_.size()) {
            return Status::ERROR;
        }
        
        // Verificar que todos los frames en el mapa existan en la lista
        for (const auto& par : mapa_frames_) {
            bool encontrado = false;
            for (const auto& nodo : lista_lru_) {
                if (nodo.id_frame == par.first) {
                    encontrado = true;
                    break;
                }
            }
            if (!encontrado) {
                return Status::ERROR;
            }
        }
        
        // Verificar que todos los frames anclados existan en la lista
        for (FrameId id_frame : frames_anclados_) {
            if (mapa_frames_.find(id_frame) == mapa_frames_.end()) {
                return Status::ERROR;
            }
        }
        
        return Status::OK;
    }
    
    /**
     * @brief Imprime el estado actual de la lista LRU para depuración
     */
    void ImprimirEstadoLRU() const {
        std::cout << "\n=== ESTADO ACTUAL DE LA LISTA LRU ===" << std::endl;
        std::cout << "Orden (más reciente → menos reciente):" << std::endl;
        
        int posicion = 0;
        for (const auto& nodo : lista_lru_) {
            std::cout << "  " << posicion << ": Frame " << nodo.id_frame;
            std::cout << " (accesos: " << nodo.contador_accesos;
            std::cout << ", timestamp: " << nodo.timestamp_acceso;
            
            if (frames_anclados_.find(nodo.id_frame) != frames_anclados_.end()) {
                std::cout << ", ANCLADO";
            }
            
            std::cout << ")" << std::endl;
            posicion++;
        }
        
        if (lista_lru_.empty()) {
            std::cout << "  Lista vacía" << std::endl;
        }
    }
};

#endif // LRU_ESPANOL_H
