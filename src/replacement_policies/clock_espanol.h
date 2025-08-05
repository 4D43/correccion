// replacement_policies/clock_espanol.h - Política de Reemplazo CLOCK en Español
// Implementación del algoritmo de reloj completamente refactorizada

#ifndef CLOCK_ESPANOL_H
#define CLOCK_ESPANOL_H

#include "ipolitica_reemplazo.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <sstream>  // <--- ADDED: Para std::ostringstream
#include <iomanip>
#include <chrono>   // <--- Ensure this is included for std::chrono

/**
 * @brief Implementación de la política de reemplazo CLOCK (Algoritmo de Reloj)
 * * Esta política utiliza un bit de referencia para cada frame y un puntero
 * que se mueve circularmente. Cuando se necesita desalojar una página,
 * el algoritmo busca la primera página con bit de referencia 0. Si encuentra
 * una página con bit 1, lo cambia a 0 y continúa.
 * * VENTAJAS:
 * - Aproximación eficiente de LRU con menor overhead
 * - Complejidad O(1) amortizada para todas las operaciones
 * - Menor uso de memoria que LRU puro
 * * ESTRUCTURA DE DATOS:
 * - Vector circular de frames con bits de referencia
 * - Puntero de reloj que indica la posición actual
 * - Conjunto de frames anclados
 * - Mapa para acceso rápido a las posiciones
 */
class PoliticaClock : public IPoliticaReemplazo {
private:
    /**
     * @brief Entrada en el reloj que representa un frame
     */
    struct EntradaReloj {
        FrameId id_frame;
        bool bit_referencia;        // Bit de referencia para el algoritmo CLOCK
        bool esta_ocupado;          // Si la entrada está ocupada
        uint32_t contador_accesos;  // Contador de accesos para estadísticas
        uint64_t timestamp_ultimo_acceso; // Timestamp del último acceso
        
        EntradaReloj() 
            : id_frame(INVALID_FRAME_ID), bit_referencia(false), 
              esta_ocupado(false), contador_accesos(0), timestamp_ultimo_acceso(0) {}
              
        EntradaReloj(FrameId id) 
            : id_frame(id), bit_referencia(true), esta_ocupado(true), 
              contador_accesos(1), timestamp_ultimo_acceso(0) {}
    };
    
    // Vector circular que representa el reloj
    std::vector<EntradaReloj> reloj_;
    
    // Puntero del reloj (índice actual en el vector)
    size_t puntero_reloj_;
    
    // Mapa para acceso rápido: FrameId -> índice en el vector
    std::unordered_map<FrameId, size_t> mapa_posiciones_;
    
    // Conjunto de frames anclados que no pueden ser desalojados
    std::unordered_set<FrameId> frames_anclados_;
    
    // Tamaño máximo del reloj
    size_t tamaño_maximo_;
    
    // Número de entradas ocupadas
    size_t entradas_ocupadas_;
    
    // Estadísticas de la política
    struct EstadisticasClock {
        uint64_t total_accesos = 0;
        uint64_t total_desalojos = 0;
        uint64_t total_anclajes = 0;
        uint64_t total_desanclajes = 0;
        uint64_t frames_agregados = 0;
        uint64_t frames_removidos = 0;
        uint64_t vueltas_completas_reloj = 0;
        uint64_t bits_referencia_limpiados = 0;
    } estadisticas_;
    
    /**
     * @brief Obtiene el timestamp actual en microsegundos
     * @return uint64_t timestamp actual
     */
    uint64_t ObtenerTimestampActual() const {
        // Explicitly bring std::chrono into scope for this function
        using namespace std::chrono; 
        return duration_cast<microseconds>(
            high_resolution_clock::now().time_since_epoch()).count();
    }
    
    /**
     * @brief Avanza el puntero del reloj a la siguiente posición
     */
    void AvanzarPunteroReloj() {
        puntero_reloj_ = (puntero_reloj_ + 1) % reloj_.size();
    }
    
    /**
     * @brief Busca una posición libre en el reloj
     * @return size_t índice de la posición libre, o SIZE_MAX si no hay
     */
    size_t BuscarPosicionLibre() {
        for (size_t i = 0; i < reloj_.size(); ++i) {
            if (!reloj_[i].esta_ocupado) {
                return i;
            }
        }
        return SIZE_MAX; // No hay posiciones libres
    }

public:
    /**
     * @brief Constructor de la política CLOCK
     * @param tamaño_maximo Tamaño máximo del buffer pool
     */
    explicit PoliticaClock(size_t tamaño_maximo = 100) 
        : puntero_reloj_(0), tamaño_maximo_(tamaño_maximo), entradas_ocupadas_(0) {
        
        reloj_.resize(tamaño_maximo_);
        std::cout << "Política CLOCK inicializada con tamaño máximo: " << tamaño_maximo_ << std::endl;
    }
    
    /**
     * @brief Destructor de la política CLOCK
     */
    ~PoliticaClock() override {
        std::cout << "Política CLOCK destruida. Estadísticas finales:" << std::endl;
        std::cout << ObtenerEstadisticas() << std::endl;
    }
    
    /**
     * @brief Ancla un frame, impidiendo que sea desalojado
     * @param id_frame ID del frame a anclar
     */
    void Anclar(FrameId id_frame) override {
        frames_anclados_.insert(id_frame);
        estadisticas_.total_anclajes++;
        
        // Si el frame existe, marcarlo como accedido
        auto it = mapa_posiciones_.find(id_frame);
        if (it != mapa_posiciones_.end()) {
            size_t posicion = it->second;
            reloj_[posicion].bit_referencia = true;
            reloj_[posicion].contador_accesos++;
            reloj_[posicion].timestamp_ultimo_acceso = ObtenerTimestampActual();
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
        
        auto it = mapa_posiciones_.find(id_frame);
        if (it != mapa_posiciones_.end()) {
            size_t posicion = it->second;
            
            // Establecer bit de referencia y actualizar estadísticas
            reloj_[posicion].bit_referencia = true;
            reloj_[posicion].contador_accesos++;
            reloj_[posicion].timestamp_ultimo_acceso = ObtenerTimestampActual();
        } else {
            std::cerr << "Advertencia: Intento de acceder a frame inexistente: " 
                      << id_frame << std::endl;
        }
    }
    
    /**
     * @brief Selecciona un frame para desalojar según el algoritmo CLOCK
     * @return FrameId del frame a desalojar, o INVALID_FRAME_ID si no hay ninguno disponible
     */
    FrameId Desalojar() override {
        if (entradas_ocupadas_ == 0) {
            return INVALID_FRAME_ID;
        }
        
        size_t posicion_inicial = puntero_reloj_;
        size_t vueltas = 0;
        
        do {
            EntradaReloj& entrada = reloj_[puntero_reloj_];
            
            // Si la entrada está ocupada
            if (entrada.esta_ocupado) {
                // Verificar si el frame puede ser desalojado (no está anclado)
                if (frames_anclados_.find(entrada.id_frame) == frames_anclados_.end()) {
                    // Si el bit de referencia es 0, desalojar este frame
                    if (!entrada.bit_referencia) {
                        FrameId id_frame_desalojado = entrada.id_frame;
                        estadisticas_.total_desalojos++;
                        
                        // Avanzar el puntero para la próxima vez
                        AvanzarPunteroReloj();
                        
                        return id_frame_desalojado;
                    } else {
                        // Bit de referencia es 1, cambiarlo a 0 y continuar
                        entrada.bit_referencia = false;
                        estadisticas_.bits_referencia_limpiados++;
                    }
                }
            }
            
            // Avanzar el puntero del reloj
            AvanzarPunteroReloj();
            
            // Verificar si hemos dado una vuelta completa
            if (puntero_reloj_ == posicion_inicial) {
                vueltas++;
                if (vueltas == 1) {
                    estadisticas_.vueltas_completas_reloj++;
                }
            }
            
        } while (puntero_reloj_ != posicion_inicial || vueltas < 2);
        
        // No se encontró ningún frame para desalojar
        return INVALID_FRAME_ID;
    }
    
    /**
     * @brief Agrega un nuevo frame a la política
     * @param id_frame ID del frame a agregar
     */
    void AgregarFrame(FrameId id_frame) override {
        // Verificar que el frame no exista ya
        if (mapa_posiciones_.find(id_frame) != mapa_posiciones_.end()) {
            std::cerr << "Advertencia: Intento de agregar frame existente: " 
                      << id_frame << std::endl;
            return;
        }
        
        // Buscar una posición libre
        size_t posicion = BuscarPosicionLibre();
        if (posicion == SIZE_MAX) {
            std::cerr << "Error: No hay espacio disponible en el reloj para el frame: " 
                      << id_frame << std::endl;
            return;
        }
        
        // Agregar el frame en la posición libre
        reloj_[posicion] = EntradaReloj(id_frame);
        reloj_[posicion].timestamp_ultimo_acceso = ObtenerTimestampActual();
        
        // Actualizar el mapa de posiciones
        mapa_posiciones_[id_frame] = posicion;
        
        entradas_ocupadas_++;
        estadisticas_.frames_agregados++;
    }
    
    /**
     * @brief Remueve un frame de la política
     * @param id_frame ID del frame a remover
     */
    void RemoverFrame(FrameId id_frame) override {
        auto it = mapa_posiciones_.find(id_frame);
        if (it != mapa_posiciones_.end()) {
            size_t posicion = it->second;
            
            // Limpiar la entrada en el reloj
            reloj_[posicion] = EntradaReloj();
            
            // Remover del mapa de posiciones
            mapa_posiciones_.erase(it);
            
            // Remover de frames anclados si estaba anclado
            frames_anclados_.erase(id_frame);
            
            entradas_ocupadas_--;
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
        // Limpiar todas las estructuras
        for (auto& entrada : reloj_) {
            entrada = EntradaReloj();
        }
        
        mapa_posiciones_.clear();
        frames_anclados_.clear();
        puntero_reloj_ = 0;
        entradas_ocupadas_ = 0;
        estadisticas_ = EstadisticasClock{};
        
        std::cout << "Política CLOCK reiniciada correctamente." << std::endl;
    }
    
    /**
     * @brief Obtiene el nombre de la política
     * @return std::string nombre de la política
     */
    std::string ObtenerNombre() const override {
        return "CLOCK (Algoritmo de Reloj)";
    }
    
    /**
     * @brief Obtiene estadísticas detalladas de la política
     * @return std::string con las estadísticas formateadas
     */
    std::string ObtenerEstadisticas() const override {
        std::ostringstream ss;
        ss << "\n=== ESTADÍSTICAS POLÍTICA CLOCK ===\n";
        ss << "Total de accesos: " << estadisticas_.total_accesos << "\n";
        ss << "Total de desalojos: " << estadisticas_.total_desalojos << "\n";
        ss << "Total de anclajes: " << estadisticas_.total_anclajes << "\n";
        ss << "Total de desanclajes: " << estadisticas_.total_desanclajes << "\n";
        ss << "Frames agregados: " << estadisticas_.frames_agregados << "\n";
        ss << "Frames removidos: " << estadisticas_.frames_removidos << "\n";
        ss << "Vueltas completas del reloj: " << estadisticas_.vueltas_completas_reloj << "\n";
        ss << "Bits de referencia limpiados: " << estadisticas_.bits_referencia_limpiados << "\n";
        ss << "Entradas ocupadas: " << entradas_ocupadas_ << "/" << tamaño_maximo_ << "\n";
        ss << "Frames anclados: " << frames_anclados_.size() << "\n";
        ss << "Posición actual del puntero: " << puntero_reloj_ << "\n";
        
        if (estadisticas_.total_accesos > 0) {
            double tasa_desalojos = (double)estadisticas_.total_desalojos / estadisticas_.total_accesos * 100.0;
            ss << "Tasa de desalojos: " << std::fixed << std::setprecision(2) << tasa_desalojos << "%\n";
        }
        
        if (estadisticas_.total_desalojos > 0) {
            double promedio_bits_limpiados = (double)estadisticas_.bits_referencia_limpiados / estadisticas_.total_desalojos;
            ss << "Promedio de bits limpiados por desalojo: " << std::fixed << std::setprecision(2) << promedio_bits_limpiados << "\n";
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
        return (mapa_posiciones_.find(id_frame) != mapa_posiciones_.end()) &&
               (frames_anclados_.find(id_frame) == frames_anclados_.end());
    }
    
    /**
     * @brief Obtiene el número de frames gestionados
     * @return uint32_t número de frames
     */
    uint32_t ObtenerNumeroFrames() const override {
        return static_cast<uint32_t>(entradas_ocupadas_);
    }
    
    /**
     * @brief Valida la consistencia interna de la política
     * @return Status::OK si es consistente, error en caso contrario
     */
    Status ValidarConsistencia() const override {
        // Verificar que el número de entradas ocupadas coincida con el mapa
        if (entradas_ocupadas_ != mapa_posiciones_.size()) {
            return Status::ERROR;
        }
        
        // Contar entradas ocupadas manualmente
        size_t contador_ocupadas = 0;
        for (const auto& entrada : reloj_) {
            if (entrada.esta_ocupado) {
                contador_ocupadas++;
            }
        }
        
        if (contador_ocupadas != entradas_ocupadas_) {
            return Status::ERROR;
        }
        
        // Verificar que todos los frames en el mapa existan en el reloj
        for (const auto& par : mapa_posiciones_) {
            size_t posicion = par.second;
            if (posicion >= reloj_.size() || 
                !reloj_[posicion].esta_ocupado || 
                reloj_[posicion].id_frame != par.first) {
                return Status::ERROR;
            }
        }
        
        // Verificar que todos los frames anclados existan en el mapa
        for (FrameId id_frame : frames_anclados_) {
            if (mapa_posiciones_.find(id_frame) == mapa_posiciones_.end()) {
                return Status::ERROR;
            }
        }
        
        return Status::OK;
    }
    
    /**
     * @brief Imprime el estado actual del reloj para depuración
     */
    void ImprimirEstadoReloj() const {
        std::cout << "\n=== ESTADO ACTUAL DEL RELOJ CLOCK ===" << std::endl;
        std::cout << "Puntero del reloj en posición: " << puntero_reloj_ << std::endl;
        std::cout << "Entradas ocupadas: " << entradas_ocupadas_ << "/" << tamaño_maximo_ << std::endl;
        
        for (size_t i = 0; i < reloj_.size() && i < 20; ++i) { // Mostrar máximo 20 entradas
            const auto& entrada = reloj_[i];
            std::cout << "  [" << i << "] ";
            
            if (i == puntero_reloj_) {
                std::cout << "-> ";
            } else {
                std::cout << "   ";
            }
            
            if (entrada.esta_ocupado) {
                std::cout << "Frame " << entrada.id_frame;
                std::cout << " (bit: " << (entrada.bit_referencia ? "1" : "0");
                std::cout << ", accesos: " << entrada.contador_accesos;
                
                if (frames_anclados_.find(entrada.id_frame) != frames_anclados_.end()) {
                    std::cout << ", ANCLADO";
                }
                
                std::cout << ")";
            } else {
                std::cout << "LIBRE";
            }
            
            std::cout << std::endl;
        }
        
        if (reloj_.size() > 20) {
            std::cout << "  ... (mostrando solo las primeras 20 entradas)" << std::endl;
        }
    }
};

#endif // CLOCK_ESPANOL_H
