// data_storage/gestor_buffer.cpp
#include "gestor_buffer.h"
#include <iostream>  // Para std::cout, std::cerr
#include <iomanip>   // Para std::setw, std::setfill
#include <algorithm> // Para std::find_if
#include <cstring>   // Para std::memcpy
#include <chrono>    // Para std::chrono::system_clock, etc.

// ===== CONSTRUCTOR Y DESTRUCTOR =====

GestorBuffer::GestorBuffer(std::shared_ptr<GestorDisco> gestor_disco,
                           uint32_t tamaño_pool,
                           BlockSizeType tamaño_bloque,
                           std::unique_ptr<IReplacementPolicy> politica_reemplazo)
    : gestor_disco_(gestor_disco)
    , tamaño_pool_(tamaño_pool)
    , tamaño_bloque_(tamaño_bloque)
    , politica_reemplazo_(std::move(politica_reemplazo)) {
    
    if (!gestor_disco_) {
        throw std::invalid_argument("GestorBuffer: El gestor de disco no puede ser nulo.");
    }
    
    if (tamaño_pool_ == 0) {
        throw std::invalid_argument("GestorBuffer: El tamaño del pool debe ser mayor que 0.");
    }
    
    if (!politica_reemplazo_) {
        throw std::invalid_argument("GestorBuffer: La política de reemplazo no puede ser nula.");
    }
    
    // Inicializar el pool de datos del buffer
    pool_datos_buffer_.resize(tamaño_pool_);
    for (auto& frame : pool_datos_buffer_) {
        frame.resize(tamaño_bloque_); // Cada frame tiene el tamaño de un bloque
    }

    // Inicializar metadatos de los frames
    frames_.resize(tamaño_pool_);
    for (FrameId i = 0; i < tamaño_pool_; ++i) {
        frames_[i].id_frame = i;
        frames_[i].es_valida = false; // Inicialmente no contienen datos válidos
        frames_[i].esta_sucia = false;
        frames_[i].contador_anclajes = 0;
        frames_[i].id_bloque = INVALID_PAGE_ID; // No asociado a ningún bloque de disco inicialmente
        frames_[i].tipo_pagina = PageType::INVALID_PAGE;
        frames_[i].contador_accesos = 0;
        frames_[i].contador_modificaciones = 0;
        frames_[i].timestamp_ultimo_acceso = 0;
        frames_[i].timestamp_ultima_modificacion = 0;
        frames_[i].cambios_pendientes = false;
    }

    // Inicializar la política de reemplazo con el tamaño del pool
    politica_reemplazo_->Inicializar(tamaño_pool_);

    std::cout << "GestorBuffer inicializado con " << tamaño_pool_ << " frames de " 
              << tamaño_bloque_ << " bytes cada uno." << std::endl;
}

GestorBuffer::~GestorBuffer() {
    std::cout << "Destructor de GestorBuffer: Forzando la escritura de todas las páginas sucias a disco..." << std::endl;
    FlushAllPages();
    std::cout << "GestorBuffer destruido." << std::endl;
}

// === MÉTODOS PÚBLICOS DE GESTIÓN DE PÁGINAS ===

Status GestorBuffer::PinPage(BlockId id_bloque, Pagina& pagina_info) {
    std::lock_guard<std::mutex> lock(mutex_buffer_);
    ActualizarEstadisticas("tiempo_total_espera_pin"); // Simula el tiempo de espera para el lock

    // 1. Buscar la página en el buffer pool
    auto it = tabla_paginas_.find(id_bloque);
    if (it != tabla_paginas_.end()) {
        // Cache Hit: La página ya está en el buffer
        FrameId id_frame = it->second;
        frames_[id_frame].contador_anclajes++;
        frames_[id_frame].contador_accesos++;
        frames_[id_frame].timestamp_ultimo_acceso = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        politica_reemplazo_->Acceder(id_frame); // Notificar a la política de reemplazo
        pagina_info = frames_[id_frame]; // Copiar información del frame
        ActualizarEstadisticas("cache_hit");
        return Status::OK;
    }

    // Cache Miss: La página no está en el buffer
    ActualizarEstadisticas("cache_miss");

    // 2. Encontrar un frame libre o desalojar una página
    FrameId id_frame_disponible = EncontrarFrameLibre();
    if (id_frame_disponible == INVALID_FRAME_ID) {
        // No hay frames libres, intentar desalojar una página
        Status desalojo_status = DesalojarPagina();
        if (desalojo_status != Status::OK) {
            std::cerr << "Error: No se pudo desalojar una página para anclar el bloque " << id_bloque << "." << std::endl;
            return desalojo_status;
        }
        // Después del desalojo, debería haber un frame libre (el que fue desalojado)
        id_frame_disponible = politica_reemplazo_->ObtenerVictima(); // La política debe devolver el frame desalojado
        if (id_frame_disponible == INVALID_FRAME_ID) {
            std::cerr << "Error crítico: La política de reemplazo no devolvió un frame válido después del desalojo." << std::endl;
            return Status::ERROR;
        }
    }

    // 3. Cargar la página desde disco al frame disponible
    Status read_status = LeerPaginaDesdeDisco(id_bloque, id_frame_disponible);
    if (read_status != Status::OK) {
        std::cerr << "Error al leer el bloque " << id_bloque << " desde disco." << std::endl;
        // Marcar el frame como no válido si la lectura falla
        frames_[id_frame_disponible].es_valida = false;
        frames_[id_frame_disponible].id_bloque = INVALID_PAGE_ID;
        politica_reemplazo_->RemoverFrame(id_frame_disponible); // Remover de la política
        return read_status;
    }

    // 4. Actualizar metadatos del frame y tabla de páginas
    frames_[id_frame_disponible].id_bloque = id_bloque;
    frames_[id_frame_disponible].es_valida = true;
    frames_[id_frame_disponible].esta_sucia = false; // Recién cargada, no está sucia
    frames_[id_frame_disponible].contador_anclajes = 1; // Anclada por esta operación
    frames_[id_frame_disponible].contador_accesos = 1;
    frames_[id_frame_disponible].timestamp_ultimo_acceso = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    frames_[id_frame_disponible].timestamp_ultima_modificacion = 0; // No modificada aún
    frames_[id_frame_disponible].cambios_pendientes = false;

    // Asumir el tipo de página si no se conoce, o obtenerlo de la cabecera del bloque
    // Para simplificar, asumiremos que es una DATA_PAGE por defecto si no se especifica.
    // En un sistema real, se leería la cabecera del bloque para obtener el tipo.
    frames_[id_frame_disponible].tipo_pagina = PageType::DATA_PAGE; 

    tabla_paginas_[id_bloque] = id_frame_disponible;
    politica_reemplazo_->AgregarFrame(id_frame_disponible); // Notificar a la política que el frame está en uso
    politica_reemplazo_->Acceder(id_frame_disponible); // Registrar el acceso inicial

    pagina_info = frames_[id_frame_disponible]; // Copiar información del frame
    return Status::OK;
}

Status GestorBuffer::UnpinPage(BlockId id_bloque, bool is_dirty) {
    std::lock_guard<std::mutex> lock(mutex_buffer_);

    auto it = tabla_paginas_.find(id_bloque);
    if (it == tabla_paginas_.end()) {
        std::cerr << "Error: Intentando desanclar una página que no está en el buffer: " << id_bloque << std::endl;
        return Status::NOT_FOUND;
    }

    FrameId id_frame = it->second;
    if (frames_[id_frame].contador_anclajes <= 0) {
        std::cerr << "Advertencia: Intentando desanclar una página con contador de anclajes cero o negativo: " << id_bloque << std::endl;
        return Status::INVALID_ARGUMENT; // O un status más específico
    }

    frames_[id_frame].contador_anclajes--;
    if (is_dirty) {
        frames_[id_frame].esta_sucia = true;
        frames_[id_frame].timestamp_ultima_modificacion = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        frames_[id_frame].contador_modificaciones++;
    }
    politica_reemplazo_->Desanclar(id_frame); // Notificar a la política de reemplazo

    return Status::OK;
}

Byte* GestorBuffer::GetPageData(BlockId id_bloque) {
    std::lock_guard<std::mutex> lock(mutex_buffer_);

    auto it = tabla_paginas_.find(id_bloque);
    if (it == tabla_paginas_.end()) {
        std::cerr << "Error: Intentando obtener datos de una página no presente en el buffer: " << id_bloque << std::endl;
        return nullptr;
    }

    FrameId id_frame = it->second;
    if (frames_[id_frame].contador_anclajes <= 0) {
        std::cerr << "Advertencia: Acceso a datos de página no anclada. Esto podría ser un error lógico: " << id_bloque << std::endl;
        // Dependiendo de la política, se podría permitir o no. Por seguridad, retornamos nullptr.
        return nullptr;
    }

    // Notificar acceso a la política de reemplazo (ya se hizo en PinPage, pero si se llama GetPageData directamente)
    politica_reemplazo_->Acceder(id_frame);
    frames_[id_frame].timestamp_ultimo_acceso = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    return pool_datos_buffer_[id_frame].data();
}

Status GestorBuffer::NewPage(BlockId& id_bloque, Pagina& pagina_info) {
    std::lock_guard<std::mutex> lock(mutex_buffer_);

    // 1. Asignar un nuevo bloque en disco
    BlockId nuevo_id_bloque = gestor_disco_->AsignarBloque(PageType::DATA_PAGE); // Asumimos DATA_PAGE para nuevos bloques
    if (nuevo_id_bloque == 0) { // 0 o INVALID_BLOCK_ID si la asignación falla
        std::cerr << "Error: No se pudo asignar un nuevo bloque en disco." << std::endl;
        return Status::DISK_FULL;
    }
    id_bloque = nuevo_id_bloque; // Devolver el ID del nuevo bloque

    // 2. Encontrar un frame libre o desalojar una página para el nuevo bloque
    FrameId id_frame_disponible = EncontrarFrameLibre();
    if (id_frame_disponible == INVALID_FRAME_ID) {
        Status desalojo_status = DesalojarPagina();
        if (desalojo_status != Status::OK) {
            std::cerr << "Error: No se pudo desalojar una página para crear el nuevo bloque " << id_bloque << "." << std::endl;
            // Desasignar el bloque recién creado en disco si no se puede cargar en buffer
            gestor_disco_->DesasignarBloque(id_bloque);
            return desalojo_status;
        }
        id_frame_disponible = politica_reemplazo_->ObtenerVictima();
        if (id_frame_disponible == INVALID_FRAME_ID) {
            std::cerr << "Error crítico: La política de reemplazo no devolvió un frame válido después del desalojo para NewPage." << std::endl;
            gestor_disco_->DesasignarBloque(id_bloque);
            return Status::ERROR;
        }
    }

    // 3. Inicializar el frame en el buffer con los metadatos del nuevo bloque
    frames_[id_frame_disponible].id_bloque = id_bloque;
    frames_[id_frame_disponible].id_frame = id_frame_disponible;
    frames_[id_frame_disponible].es_valida = true;
    frames_[id_frame_disponible].esta_sucia = true; // Nuevo bloque, se considera sucio para ser escrito a disco
    frames_[id_frame_disponible].contador_anclajes = 1; // Anclada por esta operación
    frames_[id_frame_disponible].contador_accesos = 1;
    frames_[id_frame_disponible].timestamp_creacion = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    frames_[id_frame_disponible].timestamp_ultimo_acceso = frames_[id_frame_disponible].timestamp_creacion;
    frames_[id_frame_disponible].timestamp_ultima_modificacion = frames_[id_frame_disponible].timestamp_creacion;
    frames_[id_frame_disponible].cambios_pendientes = true;
    frames_[id_frame_disponible].tipo_pagina = PageType::DATA_PAGE; // Tipo de página del nuevo bloque

    // Limpiar el contenido del frame para el nuevo bloque
    std::memset(pool_datos_buffer_[id_frame_disponible].data(), 0, tamaño_bloque_);

    // Actualizar tabla de páginas
    tabla_paginas_[id_bloque] = id_frame_disponible;
    politica_reemplazo_->AgregarFrame(id_frame_disponible); // Notificar a la política
    politica_reemplazo_->Anclar(id_frame_disponible); // Anclar en la política

    pagina_info = frames_[id_frame_disponible]; // Copiar información del frame
    return Status::OK;
}

Status GestorBuffer::DeletePage(BlockId id_bloque) {
    std::lock_guard<std::mutex> lock(mutex_buffer_);

    auto it = tabla_paginas_.find(id_bloque);
    if (it == tabla_paginas_.end()) {
        std::cerr << "Advertencia: Intentando eliminar una página que no está en el buffer: " << id_bloque << std::endl;
        // Si no está en el buffer, intentar desasignar directamente del disco
        return gestor_disco_->DesasignarBloque(id_bloque);
    }

    FrameId id_frame = it->second;

    // Si la página está anclada, no se puede eliminar
    if (frames_[id_frame].contador_anclajes > 0) {
        std::cerr << "Error: No se puede eliminar la página " << id_bloque << " porque está anclada (" 
                  << frames_[id_frame].contador_anclajes << " anclajes)." << std::endl;
        return Status::PAGE_PINNED;
    }

    // Si la página está sucia, forzar la escritura a disco antes de eliminar
    if (frames_[id_frame].esta_sucia) {
        Status flush_status = EscribirPaginaADisco(id_bloque);
        if (flush_status != Status::OK) {
            std::cerr << "Error: No se pudo escribir la página sucia " << id_bloque << " a disco antes de eliminarla." << std::endl;
            return flush_status;
        }
    }

    // Desasignar el bloque del disco
    Status disk_status = gestor_disco_->DesasignarBloque(id_bloque);
    if (disk_status != Status::OK) {
        std::cerr << "Error al desasignar el bloque " << id_bloque << " del disco: " << StatusToString(disk_status) << std::endl;
        return disk_status;
    }

    // Limpiar el frame en el buffer
    std::memset(pool_datos_buffer_[id_frame].data(), 0, tamaño_bloque_);
    frames_[id_frame] = Pagina(); // Resetear metadatos del frame
    frames_[id_frame].id_frame = id_frame; // Mantener el ID del frame
    
    // Remover de la tabla de páginas y de la política de reemplazo
    tabla_paginas_.erase(id_bloque);
    politica_reemplazo_->RemoverFrame(id_frame);

    std::cout << "Página " << id_bloque << " eliminada exitosamente del buffer y disco." << std::endl;
    return Status::OK;
}

Status GestorBuffer::FlushPage(BlockId id_bloque) {
    std::lock_guard<std::mutex> lock(mutex_buffer_);

    auto it = tabla_paginas_.find(id_bloque);
    if (it == tabla_paginas_.end()) {
        std::cerr << "Advertencia: Intentando forzar escritura de una página no presente en el buffer: " << id_bloque << std::endl;
        return Status::NOT_FOUND;
    }

    FrameId id_frame = it->second;
    if (frames_[id_frame].esta_sucia) {
        Status write_status = EscribirPaginaADisco(id_bloque);
        if (write_status != Status::OK) {
            std::cerr << "Error al forzar la escritura de la página " << id_bloque << " a disco." << std::endl;
            return write_status;
        }
        frames_[id_frame].esta_sucia = false;
        frames_[id_frame].cambios_pendientes = false;
        std::cout << "Página " << id_bloque << " forzada a disco exitosamente." << std::endl;
    } else {
        std::cout << "Página " << id_bloque << " no está sucia, no se requiere escritura." << std::endl;
    }
    return Status::OK;
}

Status GestorBuffer::FlushAllPages() {
    std::lock_guard<std::mutex> lock(mutex_buffer_);
    Status overall_status = Status::OK;

    for (FrameId i = 0; i < tamaño_pool_; ++i) {
        if (frames_[i].es_valida && frames_[i].esta_sucia) {
            Status flush_status = EscribirPaginaADisco(frames_[i].id_bloque);
            if (flush_status != Status::OK) {
                std::cerr << "Error al escribir la página sucia " << frames_[i].id_bloque << " a disco durante FlushAllPages." << std::endl;
                overall_status = Status::ERROR; // Continuar, pero registrar el error
            } else {
                frames_[i].esta_sucia = false;
                frames_[i].cambios_pendientes = false;
            }
        }
    }
    return overall_status;
}

uint32_t GestorBuffer::GetNumFreeFrames() const {
    std::lock_guard<std::mutex> lock(mutex_buffer_);
    uint32_t free_frames = 0;
    for (const auto& frame : frames_) {
        if (!frame.es_valida || frame.id_bloque == INVALID_PAGE_ID) { // Considerar inválido o no asignado como libre
            free_frames++;
        }
    }
    return free_frames;
}

uint32_t GestorBuffer::GetPoolSize() const {
    return tamaño_pool_;
}

void GestorBuffer::ResetStats() {
    std::lock_guard<std::mutex> lock(mutex_buffer_);
    estadisticas_ = BufferStats(); // Resetear a valores por defecto
    estadisticas_.ult_timestamp_reset = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    std::cout << "Estadísticas del GestorBuffer reiniciadas." << std::endl;
}

void GestorBuffer::PrintStats() const {
    std::lock_guard<std::mutex> lock(mutex_buffer_);
    std::cout << "\n=== ESTADÍSTICAS DEL GESTOR DE BUFFER ===" << std::endl;
    std::cout << "Tamaño del Pool: " << tamaño_pool_ << " frames" << std::endl;
    std::cout << "Frames Libres: " << GetNumFreeFrames() << std::endl;
    std::cout << "Hits de Cache: " << estadisticas_.hits_cache << std::endl;
    std::cout << "Misses de Cache: " << estadisticas_.misses_cache << std::endl;
    std::cout << "Lecturas de Disco: " << estadisticas_.lecturas_disco << std::endl;
    std::cout << "Escrituras de Disco: " << estadisticas_.escrituras_disco << std::endl;
    std::cout << "Desalojos: " << estadisticas_.desalojos << std::endl;
    std::cout << "Páginas Ancladas: " << estadisticas_.paginas_ancladas << std::endl; // Actualizar este contador
    std::cout << "Páginas Sucias: " << estadisticas_.paginas_sucias << std::endl;     // Actualizar este contador
    std::cout << "Tiempo Total de Espera (ms): " << estadisticas_.tiempo_total_espera_pin << std::endl;
    std::cout << "Tiempo Total de I/O (ms): " << estadisticas_.tiempo_total_io << std::endl;
    std::cout << "Tiempo Política Reemplazo (ms): " << estadisticas_.tiempo_total_procesamiento_politica << std::endl;
    std::cout << "Último Reset de Estadísticas: " << ObtenerTimestampActualString() << std::endl;
    std::cout << "---------------------------------------" << std::endl;

    // Actualizar paginas_ancladas y paginas_sucias para la impresión
    uint64_t current_pinned = 0;
    uint64_t current_dirty = 0;
    for (const auto& frame : frames_) {
        if (frame.es_valida && frame.contador_anclajes > 0) {
            current_pinned++;
        }
        if (frame.es_valida && frame.esta_sucia) {
            current_dirty++;
        }
    }
    // No modificar estadisticas_ aquí, solo para la impresión actual
    std::cout << "Páginas Ancladas (actual): " << current_pinned << std::endl;
    std::cout << "Páginas Sucias (actual): " << current_dirty << std::endl;
}

GestorBuffer::BufferStats GestorBuffer::GetStats() const {
    std::lock_guard<std::mutex> lock(mutex_buffer_);
    return estadisticas_;
}

Status GestorBuffer::ValidarConsistencia() const {
    std::lock_guard<std::mutex> lock(mutex_buffer_);
    // 1. Verificar que el tamaño del pool sea consistente
    if (pool_datos_buffer_.size() != tamaño_pool_ || frames_.size() != tamaño_pool_) {
        std::cerr << "Error de consistencia: Tamaños de pool o frames inconsistentes." << std::endl;
        return Status::ERROR;
    }

    // 2. Verificar que la tabla_paginas_ refleje correctamente los frames válidos
    if (tabla_paginas_.size() > tamaño_pool_) {
        std::cerr << "Error de consistencia: tabla_paginas_ tiene más entradas que frames en el pool." << std::endl;
        return Status::ERROR;
    }

    for (const auto& entry : tabla_paginas_) {
        BlockId page_id = entry.first;
        FrameId frame_id = entry.second;

        if (frame_id >= tamaño_pool_) {
            std::cerr << "Error de consistencia: FrameId en tabla_paginas_ fuera de rango." << std::endl;
            return Status::ERROR;
        }
        if (!frames_[frame_id].es_valida || frames_[frame_id].id_bloque != page_id) {
            std::cerr << "Error de consistencia: Entrada en tabla_paginas_ no coincide con metadatos del frame." << std::endl;
            return Status::ERROR;
        }
    }

    // 3. Verificar la consistencia de la política de reemplazo
    if (politica_reemplazo_->ValidarConsistencia() != Status::OK) {
        std::cerr << "Error de consistencia: La política de reemplazo reporta inconsistencias." << std::endl;
        return Status::ERROR;
    }

    std::cout << "Consistencia del GestorBuffer validada: OK." << std::endl;
    return Status::OK;
}

// === MÉTODOS AUXILIARES PRIVADOS ===

FrameId GestorBuffer::EncontrarFrameLibre() {
    for (FrameId i = 0; i < tamaño_pool_; ++i) {
        if (!frames_[i].es_valida || frames_[i].id_bloque == INVALID_PAGE_ID) {
            return i; // Encontrado un frame realmente libre o no asignado
        }
    }
    return INVALID_FRAME_ID; // No hay frames completamente libres
}

Status GestorBuffer::DesalojarPagina() {
    FrameId victima_id = politica_reemplazo_->ObtenerVictima();
    if (victima_id == INVALID_FRAME_ID) {
        std::cerr << "Error: No se encontró una página víctima para desalojar. Posiblemente todas están ancladas." << std::endl;
        return Status::BUFFER_FULL;
    }

    // Si la página víctima está sucia, escribirla a disco
    if (frames_[victima_id].esta_sucia) {
        Status write_status = EscribirPaginaADisco(frames_[victima_id].id_bloque);
        if (write_status != Status::OK) {
            std::cerr << "Error: No se pudo escribir la página víctima " << frames_[victima_id].id_bloque << " a disco durante el desalojo." << std::endl;
            return write_status;
        }
    }

    // Remover la página de la tabla de páginas
    tabla_paginas_.erase(frames_[victima_id].id_bloque);

    // Resetear los metadatos del frame desalojado
    frames_[victima_id] = Pagina(); // Resetear a valores por defecto
    frames_[victima_id].id_frame = victima_id; // Mantener el ID del frame
    
    ActualizarEstadisticas("desalojo");
    return Status::OK;
}

Status GestorBuffer::EscribirPaginaADisco(BlockId id_bloque) {
    auto it = tabla_paginas_.find(id_bloque);
    if (it == tabla_paginas_.end()) {
        std::cerr << "Error: Intentando escribir a disco una página no presente en el buffer: " << id_bloque << std::endl;
        return Status::NOT_FOUND;
    }

    FrameId id_frame = it->second;
    if (!frames_[id_frame].es_valida) {
        std::cerr << "Error: Intentando escribir a disco un frame no válido: " << id_frame << " (Bloque: " << id_bloque << ")" << std::endl;
        return Status::INVALID_ARGUMENT;
    }

    ActualizarEstadisticas("escritura_disco");
    Status write_status = gestor_disco_->EscribirBloque(id_bloque, pool_datos_buffer_[id_frame].data(), tamaño_bloque_);
    if (write_status != Status::OK) {
        std::cerr << "Error de E/S: Falló la escritura del bloque " << id_bloque << " a disco." << std::endl;
        return write_status;
    }
    frames_[id_frame].esta_sucia = false; // Ya no está sucia después de escribir
    frames_[id_frame].cambios_pendientes = false;
    return Status::OK;
}

Status GestorBuffer::LeerPaginaDesdeDisco(BlockId id_bloque, FrameId id_frame) {
    if (id_frame >= tamaño_pool_) {
        std::cerr << "Error: ID de frame inválido para lectura desde disco: " << id_frame << std::endl;
        return Status::INVALID_ARGUMENT;
    }
    if (!gestor_disco_->ExisteBloque(id_bloque)) {
        std::cerr << "Error: El bloque " << id_bloque << " no existe en disco para lectura." << std::endl;
        return Status::NOT_FOUND;
    }

    ActualizarEstadisticas("lectura_disco");
    Status read_status = gestor_disco_->LeerBloque(id_bloque, pool_datos_buffer_[id_frame].data(), tamaño_bloque_);
    if (read_status != Status::OK) {
        std::cerr << "Error de E/S: Falló la lectura del bloque " << id_bloque << " desde disco." << std::endl;
        return read_status;
    }
    return Status::OK;
}

bool GestorBuffer::ValidarFrameId(FrameId id_frame) const {
    return id_frame >= 0 && id_frame < static_cast<FrameId>(frames_.size());
}

void GestorBuffer::ActualizarEstadisticas(const std::string& tipo_operacion) {
    if (tipo_operacion == "cache_hit") {
        estadisticas_.hits_cache++;
    } else if (tipo_operacion == "cache_miss") {
        estadisticas_.misses_cache++;
    } else if (tipo_operacion == "lectura_disco") {
        estadisticas_.lecturas_disco++;
    } else if (tipo_operacion == "escritura_disco") {
        estadisticas_.escrituras_disco++;
    } else if (tipo_operacion == "desalojo") {
        estadisticas_.desalojos++;
    } else if (tipo_operacion == "confirmacion") {
        estadisticas_.confirmaciones++;
    } else if (tipo_operacion == "reversion") {
        estadisticas_.reversiones++;
    } else if (tipo_operacion == "tiempo_total_espera_pin") {
        // Simulación de tiempo de espera, en un entorno real se mediría
        estadisticas_.tiempo_total_espera_pin += 1; // Incremento arbitrario para simulación
    } else if (tipo_operacion == "tiempo_total_io") {
        estadisticas_.tiempo_total_io += 10; // Incremento arbitrario para simulación
    } else if (tipo_operacion == "tiempo_total_procesamiento_politica") {
        estadisticas_.tiempo_total_procesamiento_politica += 2; // Incremento arbitrario
    }

    // Actualizar contadores de páginas ancladas y sucias dinámicamente
    uint64_t current_pinned = 0;
    uint64_t current_dirty = 0;
    for (const auto& frame : frames_) {
        if (frame.es_valida && frame.contador_anclajes > 0) {
            current_pinned++;
        }
        if (frame.es_valida && frame.esta_sucia) {
            current_dirty++;
        }
    }
    estadisticas_.paginas_ancladas = current_pinned;
    estadisticas_.paginas_sucias = current_dirty;
}

std::string GestorBuffer::ObtenerTimestampActualString() const {
    auto ahora = std::chrono::system_clock::now();
    std::time_t tiempo = std::chrono::system_clock::to_time_t(ahora);
    std::tm* tm_local = std::localtime(&tiempo);

    std::stringstream ss;
    ss << std::put_time(tm_local, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}
