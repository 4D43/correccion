// gestor_registros.cpp - Implementación del Gestor de Registros
// Refactorizado completamente a español con funcionalidad mejorada

#include "gestor_registros.h"
#include "../Catalog_Manager/gestor_catalogo.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <stdexcept> // Para std::runtime_error

// === CONSTRUCTOR Y DESTRUCTOR ===

GestorRegistros::GestorRegistros(GestorBuffer& gestor_buffer)
    : gestor_buffer_(&gestor_buffer)
    , gestor_catalogo_(nullptr)
    , gestor_indices_(nullptr)
    , total_inserciones_(0)
    , total_actualizaciones_(0)
    , total_eliminaciones_(0)
    , total_consultas_(0)
    , total_compactaciones_(0) {
    
    std::cout << "GestorRegistros inicializado correctamente." << std::endl;
}

GestorRegistros::~GestorRegistros() {
    std::cout << "GestorRegistros destruido. Estadísticas finales:" << std::endl;
    std::cout << "  Inserciones: " << total_inserciones_ << std::endl;
    std::cout << "  Actualizaciones: " << total_actualizaciones_ << std::endl;
    std::cout << "  Eliminaciones: " << total_eliminaciones_ << std::endl;
    std::cout << "  Consultas: " << total_consultas_ << std::endl;
    std::cout << "  Compactaciones: " << total_compactaciones_ << std::endl;
}

// === FUNCIÓN UTILITARIA ===

uint64_t GestorRegistros::ObtenerTimestampActual() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

// === MÉTODOS PÚBLICOS ===

Status GestorRegistros::InsertarRegistro(const std::string& nombre_tabla, const DatosRegistro& datos_registro) {
    if (!gestor_catalogo_) {
        std::cerr << "Error: GestorCatalogo no está configurado." << std::endl;
        return Status::ERROR;
    }

    std::shared_ptr<MetadataTabla> metadata_tabla = gestor_catalogo_->ObtenerMetadataTabla(nombre_tabla);
    if (!metadata_tabla) {
        std::cerr << "Error: Tabla '" << nombre_tabla << "' no encontrada." << std::endl;
        return Status::NOT_FOUND;
    }

    EsquemaTablaCompleto esquema;
    esquema.base_metadata.table_id = metadata_tabla->ObtenerIdTabla();
    std::strncpy(esquema.base_metadata.table_name, metadata_tabla->ObtenerNombreTabla().c_str(), 63);
    esquema.base_metadata.table_name[63] = '\0';
    esquema.base_metadata.is_fixed_length_record = metadata_tabla->EsLongitudFija();
    esquema.base_metadata.num_records = metadata_tabla->ObtenerNumeroRegistros();
    esquema.base_metadata.fixed_record_size = metadata_tabla->ObtenerTamanoRegistroFijo();

    for (const auto& col_meta : metadata_tabla->ObtenerColumnas()) {
        ColumnMetadata cm;
        std::strncpy(cm.name, col_meta.nombre.c_str(), 63);
        cm.name[63] = '\0';
        cm.type = col_meta.tipo;
        cm.size = col_meta.tamano;
        esquema.columns.push_back(cm);
    }

    if (!ValidarDatosRegistro(datos_registro, esquema)) {
        std::cerr << "Error: Datos de registro no válidos para el esquema de la tabla." << std::endl;
        return Status::INVALID_ARGUMENT;
    }

    std::vector<Byte> datos_raw = SerializarRegistro(datos_registro, esquema);
    uint32_t tamano_registro_raw = datos_raw.size();

    // Buscar una página existente con espacio o crear una nueva
    PageId id_pagina_destino = INVALID_FRAME_ID; // Usar INVALID_FRAME_ID como valor inicial
    Byte* datos_pagina = nullptr;
    CabeceraBloqueDatos* cabecera_datos = nullptr;
    Pagina pagina_info;

    // Iterar sobre las páginas de datos de la tabla para encontrar espacio
    for (PageId page_id : metadata_tabla->ObtenerPaginasDatos()) {
        Status pin_status = gestor_buffer_->PinPage(page_id, pagina_info);
        if (pin_status == Status::OK) {
            datos_pagina = gestor_buffer_->GetPageData(page_id);
            if (!datos_pagina) {
                gestor_buffer_->UnpinPage(page_id, false);
                continue;
            }
            // Acceder a la cabecera del bloque de datos
            CabeceraComun* cabecera_comun = reinterpret_cast<CabeceraComun*>(datos_pagina);
            if (cabecera_comun->tipo_bloque == PageType::DATA_PAGE) { // Use tipo_bloque
                cabecera_datos = reinterpret_cast<CabeceraBloqueDatos*>(datos_pagina + sizeof(CabeceraComun));

                // Verificar si hay espacio suficiente en esta página
                // Asumimos un modelo simple de slots o espacio contiguo
                if (cabecera_datos->espacio_libre_total >= tamano_registro_raw) {
                    // Encontrado espacio en una página existente
                    id_pagina_destino = page_id;
                    break;
                }
            }
            gestor_buffer_->UnpinPage(page_id, false); // Desanclar si no se usa
        }
    }

    // Si no se encontró espacio en páginas existentes, crear una nueva página
    if (id_pagina_destino == INVALID_FRAME_ID) {
        Status new_page_status = gestor_buffer_->NewPage(id_pagina_destino, pagina_info);
        if (new_page_status != Status::OK) {
            std::cerr << "Error: No se pudo crear una nueva página para insertar el registro." << std::endl;
            return new_page_status;
        }
        // La nueva página ya está anclada, obtener sus datos y cabecera
        datos_pagina = gestor_buffer_->GetPageData(id_pagina_destino);
        if (!datos_pagina) {
            gestor_buffer_->UnpinPage(id_pagina_destino, false);
            return Status::ERROR;
        }
        CabeceraComun* cabecera_comun = reinterpret_cast<CabeceraComun*>(datos_pagina);
        cabecera_datos = reinterpret_cast<CabeceraBloqueDatos*>(datos_pagina + sizeof(CabeceraComun));

        // Inicializar la cabecera de datos para la nueva página
        // Estos campos se inicializaron en el constructor de CabeceraBloqueDatos
        // Asegurarse de que el espacio libre inicial sea correcto
        cabecera_datos->espacio_libre_total = BLOCK_SIZE - sizeof(CabeceraComun) - sizeof(CabeceraBloqueDatos);
        cabecera_datos->numero_registros_activos = 0;
        cabecera_datos->numero_registros_eliminados = 0;
        cabecera_datos->offset_directorio_slots = BLOCK_SIZE; // Directorio crece hacia abajo
        cabecera_datos->tamano_directorio_slots = 0;
        cabecera_datos->factor_carga_porcentaje = 0;
        cabecera_datos->necesita_compactacion = false;

        // Añadir la nueva página a la metadata de la tabla
        metadata_tabla->AñadirPaginaDatos(id_pagina_destino);
        gestor_catalogo_->ActualizarMetadataTabla(metadata_tabla); // Persistir cambio
    }

    // Insertar el registro en la página
    uint32_t offset_insercion;
    if (!CalcularOffsetInsercion(datos_pagina, tamano_registro_raw, offset_insercion)) {
        std::cerr << "Error: No hay espacio contiguo suficiente en la página " << id_pagina_destino << " para el registro." << std::endl;
        gestor_buffer_->UnpinPage(id_pagina_destino, false);
        return Status::OUT_OF_SPACE_FOR_UPDATE;
    }

    std::memcpy(datos_pagina + offset_insercion, datos_raw.data(), tamano_registro_raw);

    // Actualizar cabecera del bloque de datos
    cabecera_datos->espacio_libre_total -= tamano_registro_raw;
    cabecera_datos->numero_registros_activos++;
    // Aquí se debería gestionar el directorio de slots si la tabla es de longitud variable
    // Para longitud fija, RecordId podría ser simplemente (id_pagina * MAX_RECORDS_PER_PAGE) + slot_idx
    // Para simplificar, asumimos que el RecordId es global y asignado secuencialmente por el gestor de registros
    // Se necesitaría un mecanismo para asignar RecordId único y persistente.
    // Por ahora, usaremos un RecordId simple para la prueba.
    RecordId nuevo_record_id = metadata_tabla->ObtenerNumeroRegistros() + 1; // Asignación simple

    metadata_tabla->IncrementarNumeroRegistros();
    gestor_catalogo_->ActualizarMetadataTabla(metadata_tabla); // Persistir cambio

    // Actualizar estadísticas de la página
    ActualizarEstadisticasPagina(id_pagina_destino, "insercion");

    // Desanclar la página, marcándola como sucia
    gestor_buffer_->UnpinPage(id_pagina_destino, true);

    total_inserciones_++;
    std::cout << "Registro insertado exitosamente en tabla '" << nombre_tabla 
              << "', página " << id_pagina_destino << ", RecordId " << nuevo_record_id << "." << std::endl;
    return Status::OK;
}

Status GestorRegistros::ConsultarRegistroPorID(const std::string& nombre_tabla, RecordId id_registro, DatosRegistro& datos_registro_salida) {
    if (!gestor_catalogo_) {
        std::cerr << "Error: GestorCatalogo no está configurado." << std::endl;
        return Status::ERROR;
    }

    std::shared_ptr<MetadataTabla> metadata_tabla = gestor_catalogo_->ObtenerMetadataTabla(nombre_tabla);
    if (!metadata_tabla) {
        std::cerr << "Error: Tabla '" << nombre_tabla << "' no encontrada." << std::endl;
        return Status::NOT_FOUND;
    }

    EsquemaTablaCompleto esquema;
    esquema.base_metadata.table_id = metadata_tabla->ObtenerIdTabla();
    std::strncpy(esquema.base_metadata.table_name, metadata_tabla->ObtenerNombreTabla().c_str(), 63);
    esquema.base_metadata.table_name[63] = '\0';
    esquema.base_metadata.is_fixed_length_record = metadata_tabla->EsLongitudFija();
    esquema.base_metadata.num_records = metadata_tabla->ObtenerNumeroRegistros();
    esquema.base_metadata.fixed_record_size = metadata_tabla->ObtenerTamanoRegistroFijo();

    for (const auto& col_meta : metadata_tabla->ObtenerColumnas()) {
        ColumnMetadata cm;
        std::strncpy(cm.name, col_meta.nombre.c_str(), 63);
        cm.name[63] = '\0';
        cm.type = col_meta.tipo;
        cm.size = col_meta.tamano;
        esquema.columns.push_back(cm);
    }

    // Para una consulta por ID, necesitaríamos un índice o un mapeo RecordId -> (PageId, Offset)
    // Para simplificar, haremos un escaneo lineal de todas las páginas de la tabla.
    // En un sistema real, se usaría un índice para esto.

    for (PageId page_id : metadata_tabla->ObtenerPaginasDatos()) {
        Pagina pagina_info;
        Status pin_status = gestor_buffer_->PinPage(page_id, pagina_info);
        if (pin_status != Status::OK) {
            std::cerr << "Advertencia: No se pudo anclar la página " << page_id << " para consulta." << std::endl;
            continue;
        }

        Byte* datos_pagina = gestor_buffer_->GetPageData(page_id);
        if (!datos_pagina) {
            gestor_buffer_->UnpinPage(page_id, false);
            continue;
        }

        CabeceraComun* cabecera_comun = reinterpret_cast<CabeceraComun*>(datos_pagina);
        CabeceraBloqueDatos* cabecera_datos = reinterpret_cast<CabeceraBloqueDatos*>(datos_pagina + sizeof(CabeceraComun));

        // Asumimos un esquema simple donde RecordId es el índice del registro en la página
        // y los registros están empaquetados secuencialmente.
        // Esto es una simplificación; un sistema real usaría un directorio de slots o un índice.
        uint32_t record_size_guess = esquema.base_metadata.is_fixed_length_record ? 
                                     esquema.base_metadata.fixed_record_size : 128; // Estimado para variable

        for (uint32_t i = 0; i < cabecera_datos->numero_registros_activos + cabecera_datos->numero_registros_eliminados; ++i) {
            uint32_t current_record_offset = sizeof(CabeceraComun) + sizeof(CabeceraBloqueDatos) + (i * record_size_guess);
            if (current_record_offset + record_size_guess > BLOCK_SIZE) {
                break; // Fuera de los límites del bloque
            }

            // Aquí se necesitaría una forma de obtener el RecordId real del registro
            // y verificar si coincide con id_registro.
            // Para esta simulación, asumiremos que los registros tienen un campo 'id'
            // y lo deserializaremos para verificar.
            std::vector<Byte> record_data_raw(datos_pagina + current_record_offset, 
                                              datos_pagina + current_record_offset + record_size_guess);
            DatosRegistro temp_registro = DeserializarRegistro(record_data_raw, esquema);

            // Supongamos que el primer campo es el ID y es un INT
            if (!temp_registro.campos.empty() && esquema.columns[0].type == ColumnType::INT) {
                try {
                    RecordId current_found_id = std::stoul(temp_registro.campos[0]);
                    if (current_found_id == id_registro) {
                        datos_registro_salida = temp_registro;
                        gestor_buffer_->UnpinPage(page_id, false);
                        ActualizarEstadisticasPagina(page_id, "consulta");
                        total_consultas_++;
                        std::cout << "Registro " << id_registro << " encontrado en tabla '" << nombre_tabla << "'." << std::endl;
                        return Status::OK;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Advertencia: Error al parsear ID de registro en página " << page_id << ": " << e.what() << std::endl;
                }
            }
        }
        gestor_buffer_->UnpinPage(page_id, false);
    }

    std::cout << "Registro " << id_registro << " no encontrado en tabla '" << nombre_tabla << "'." << std::endl;
    return Status::NOT_FOUND;
}

Status GestorRegistros::ConsultarTodosLosRegistros(const std::string& nombre_tabla, std::vector<DatosRegistro>& resultados) {
    if (!gestor_catalogo_) {
        std::cerr << "Error: GestorCatalogo no está configurado." << std::endl;
        return Status::ERROR;
    }

    std::shared_ptr<MetadataTabla> metadata_tabla = gestor_catalogo_->ObtenerMetadataTabla(nombre_tabla);
    if (!metadata_tabla) {
        std::cerr << "Error: Tabla '" << nombre_tabla << "' no encontrada." << std::endl;
        return Status::NOT_FOUND;
    }

    EsquemaTablaCompleto esquema;
    esquema.base_metadata.table_id = metadata_tabla->ObtenerIdTabla();
    std::strncpy(esquema.base_metadata.table_name, metadata_tabla->ObtenerNombreTabla().c_str(), 63);
    esquema.base_metadata.table_name[63] = '\0';
    esquema.base_metadata.is_fixed_length_record = metadata_tabla->EsLongitudFija();
    esquema.base_metadata.num_records = metadata_tabla->ObtenerNumeroRegistros();
    esquema.base_metadata.fixed_record_size = metadata_tabla->ObtenerTamanoRegistroFijo();

    for (const auto& col_meta : metadata_tabla->ObtenerColumnas()) {
        ColumnMetadata cm;
        std::strncpy(cm.name, col_meta.nombre.c_str(), 63);
        cm.name[63] = '\0';
        cm.type = col_meta.tipo;
        cm.size = col_meta.tamano;
        esquema.columns.push_back(cm);
    }

    resultados.clear(); // Limpiar resultados anteriores

    for (PageId page_id : metadata_tabla->ObtenerPaginasDatos()) {
        Pagina pagina_info;
        Status pin_status = gestor_buffer_->PinPage(page_id, pagina_info);
        if (pin_status != Status::OK) {
            std::cerr << "Advertencia: No se pudo anclar la página " << page_id << " para consulta de todos los registros." << std::endl;
            continue;
        }

        Byte* datos_pagina = gestor_buffer_->GetPageData(page_id);
        if (!datos_pagina) {
            gestor_buffer_->UnpinPage(page_id, false);
            continue;
        }

        CabeceraComun* cabecera_comun = reinterpret_cast<CabeceraComun*>(datos_pagina);
        CabeceraBloqueDatos* cabecera_datos = reinterpret_cast<CabeceraBloqueDatos*>(datos_pagina + sizeof(CabeceraComun));

        uint32_t record_size_guess = esquema.base_metadata.is_fixed_length_record ? 
                                     esquema.base_metadata.fixed_record_size : 128; // Estimado

        for (uint32_t i = 0; i < cabecera_datos->numero_registros_activos; ++i) { // Solo registros activos
            uint32_t current_record_offset = sizeof(CabeceraComun) + sizeof(CabeceraBloqueDatos) + (i * record_size_guess);
            if (current_record_offset + record_size_guess > BLOCK_SIZE) {
                break; // Fuera de los límites del bloque
            }

            std::vector<Byte> record_data_raw(datos_pagina + current_record_offset, 
                                              datos_pagina + current_record_offset + record_size_guess);
            resultados.push_back(DeserializarRegistro(record_data_raw, esquema));
        }
        gestor_buffer_->UnpinPage(page_id, false);
        ActualizarEstadisticasPagina(page_id, "consulta");
    }

    total_consultas_++;
    std::cout << "Consultados " << resultados.size() << " registros de la tabla '" << nombre_tabla << "'." << std::endl;
    return Status::OK;
}

Status GestorRegistros::ActualizarRegistro(const std::string& nombre_tabla, RecordId id_registro, const DatosRegistro& nuevos_datos) {
    if (!gestor_catalogo_) {
        std::cerr << "Error: GestorCatalogo no está configurado." << std::endl;
        return Status::ERROR;
    }

    std::shared_ptr<MetadataTabla> metadata_tabla = gestor_catalogo_->ObtenerMetadataTabla(nombre_tabla);
    if (!metadata_tabla) {
        std::cerr << "Error: Tabla '" << nombre_tabla << "' no encontrada." << std::endl;
        return Status::NOT_FOUND;
    }

    EsquemaTablaCompleto esquema;
    esquema.base_metadata.table_id = metadata_tabla->ObtenerIdTabla();
    std::strncpy(esquema.base_metadata.table_name, metadata_tabla->ObtenerNombreTabla().c_str(), 63);
    esquema.base_metadata.table_name[63] = '\0';
    esquema.base_metadata.is_fixed_length_record = metadata_tabla->EsLongitudFija();
    esquema.base_metadata.num_records = metadata_tabla->ObtenerNumeroRegistros();
    esquema.base_metadata.fixed_record_size = metadata_tabla->ObtenerTamanoRegistroFijo();

    for (const auto& col_meta : metadata_tabla->ObtenerColumnas()) {
        ColumnMetadata cm;
        std::strncpy(cm.name, col_meta.nombre.c_str(), 63);
        cm.name[63] = '\0';
        cm.type = col_meta.tipo;
        cm.size = col_meta.tamano;
        esquema.columns.push_back(cm);
    }

    if (!ValidarDatosRegistro(nuevos_datos, esquema)) {
        std::cerr << "Error: Nuevos datos de registro no válidos para el esquema de la tabla." << std::endl;
        return Status::INVALID_ARGUMENT;
    }

    std::vector<Byte> nuevos_datos_raw = SerializarRegistro(nuevos_datos, esquema);
    uint32_t nuevo_tamano_registro = nuevos_datos_raw.size();

    // Necesitaríamos un índice o un mapeo RecordId -> (PageId, Offset) para encontrar el registro
    // Para simplificar, escanearemos linealmente y actualizaremos si el tamaño es el mismo.
    // Si el tamaño cambia, es una operación más compleja (eliminar y reinsertar).

    for (PageId page_id : metadata_tabla->ObtenerPaginasDatos()) {
        Pagina pagina_info;
        Status pin_status = gestor_buffer_->PinPage(page_id, pagina_info);
        if (pin_status != Status::OK) {
            std::cerr << "Advertencia: No se pudo anclar la página " << page_id << " para actualización." << std::endl;
            continue;
        }

        Byte* datos_pagina = gestor_buffer_->GetPageData(page_id);
        if (!datos_pagina) {
            gestor_buffer_->UnpinPage(page_id, false);
            continue;
        }

        CabeceraComun* cabecera_comun = reinterpret_cast<CabeceraComun*>(datos_pagina);
        CabeceraBloqueDatos* cabecera_datos = reinterpret_cast<CabeceraBloqueDatos*>(datos_pagina + sizeof(CabeceraComun));

        uint32_t record_size_guess = esquema.base_metadata.is_fixed_length_record ? 
                                     esquema.base_metadata.fixed_record_size : 128; // Estimado

        for (uint32_t i = 0; i < cabecera_datos->numero_registros_activos; ++i) {
            uint32_t current_record_offset = sizeof(CabeceraComun) + sizeof(CabeceraBloqueDatos) + (i * record_size_guess);
            if (current_record_offset + record_size_guess > BLOCK_SIZE) {
                break;
            }

            std::vector<Byte> old_record_data_raw(datos_pagina + current_record_offset, 
                                                  datos_pagina + current_record_offset + record_size_guess);
            DatosRegistro old_registro = DeserializarRegistro(old_record_data_raw, esquema);

            if (!old_registro.campos.empty() && esquema.columns[0].type == ColumnType::INT) {
                try {
                    RecordId current_found_id = std::stoul(old_registro.campos[0]);
                    if (current_found_id == id_registro) {
                        // Encontrado el registro. Ahora verificar si el tamaño es compatible.
                        // Para esta simulación, solo actualizamos si el tamaño es el mismo.
                        if (nuevo_tamano_registro <= record_size_guess) { // Permitir actualización si el nuevo es igual o más pequeño
                            std::memcpy(datos_pagina + current_record_offset, nuevos_datos_raw.data(), nuevo_tamano_registro);
                            // Rellenar el resto con ceros si el nuevo es más pequeño
                            if (nuevo_tamano_registro < record_size_guess) {
                                std::memset(datos_pagina + current_record_offset + nuevo_tamano_registro, 0, record_size_guess - nuevo_tamano_registro);
                            }
                            gestor_buffer_->UnpinPage(page_id, true); // Marcar sucia
                            ActualizarEstadisticasPagina(page_id, "actualizacion");
                            total_actualizaciones_++;
                            std::cout << "Registro " << id_registro << " actualizado exitosamente en tabla '" << nombre_tabla << "'." << std::endl;
                            return Status::OK;
                        } else {
                            std::cerr << "Error: Nuevo tamaño de registro (" << nuevo_tamano_registro 
                                      << ") excede el tamaño del slot existente (" << record_size_guess << "). "
                                      << "Se requiere operación de eliminación/reinserción." << std::endl;
                            gestor_buffer_->UnpinPage(page_id, false);
                            return Status::OUT_OF_SPACE_FOR_UPDATE;
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Advertencia: Error al parsear ID de registro en página " << page_id << " durante actualización: " << e.what() << std::endl;
                }
            }
        }
        gestor_buffer_->UnpinPage(page_id, false);
    }

    std::cout << "Registro " << id_registro << " no encontrado en tabla '" << nombre_tabla << "' para actualizar." << std::endl;
    return Status::NOT_FOUND;
}

Status GestorRegistros::EliminarRegistroPorID(const std::string& nombre_tabla, RecordId id_registro) {
    if (!gestor_catalogo_) {
        std::cerr << "Error: GestorCatalogo no está configurado." << std::endl;
        return Status::ERROR;
    }

    std::shared_ptr<MetadataTabla> metadata_tabla = gestor_catalogo_->ObtenerMetadataTabla(nombre_tabla);
    if (!metadata_tabla) {
        std::cerr << "Error: Tabla '" << nombre_tabla << "' no encontrada." << std::endl;
        return Status::NOT_FOUND;
    }

    EsquemaTablaCompleto esquema;
    esquema.base_metadata.table_id = metadata_tabla->ObtenerIdTabla();
    std::strncpy(esquema.base_metadata.table_name, metadata_tabla->ObtenerNombreTabla().c_str(), 63);
    esquema.base_metadata.table_name[63] = '\0';
    esquema.base_metadata.is_fixed_length_record = metadata_tabla->EsLongitudFija();
    esquema.base_metadata.num_records = metadata_tabla->ObtenerNumeroRegistros();
    esquema.base_metadata.fixed_record_size = metadata_tabla->ObtenerTamanoRegistroFijo();

    for (const auto& col_meta : metadata_tabla->ObtenerColumnas()) {
        ColumnMetadata cm;
        std::strncpy(cm.name, col_meta.nombre.c_str(), 63);
        cm.name[63] = '\0';
        cm.type = col_meta.tipo;
        cm.size = col_meta.tamano;
        esquema.columns.push_back(cm);
    }

    for (PageId page_id : metadata_tabla->ObtenerPaginasDatos()) {
        Pagina pagina_info;
        Status pin_status = gestor_buffer_->PinPage(page_id, pagina_info);
        if (pin_status != Status::OK) {
            std::cerr << "Advertencia: No se pudo anclar la página " << page_id << " para eliminación." << std::endl;
            continue;
        }

        Byte* datos_pagina = gestor_buffer_->GetPageData(page_id);
        if (!datos_pagina) {
            gestor_buffer_->UnpinPage(page_id, false);
            continue;
        }

        CabeceraComun* cabecera_comun = reinterpret_cast<CabeceraComun*>(datos_pagina);
        CabeceraBloqueDatos* cabecera_datos = reinterpret_cast<CabeceraBloqueDatos*>(datos_pagina + sizeof(CabeceraComun));

        uint32_t record_size_guess = esquema.base_metadata.is_fixed_length_record ? 
                                     esquema.base_metadata.fixed_record_size : 128; // Estimado

        for (uint32_t i = 0; i < cabecera_datos->numero_registros_activos + cabecera_datos->numero_registros_eliminados; ++i) {
            uint32_t current_record_offset = sizeof(CabeceraComun) + sizeof(CabeceraBloqueDatos) + (i * record_size_guess);
            if (current_record_offset + record_size_guess > BLOCK_SIZE) {
                break;
            }

            std::vector<Byte> record_data_raw(datos_pagina + current_record_offset, 
                                              datos_pagina + current_record_offset + record_size_guess);
            DatosRegistro temp_registro = DeserializarRegistro(record_data_raw, esquema);

            if (!temp_registro.campos.empty() && esquema.columns[0].type == ColumnType::INT) {
                try {
                    RecordId current_found_id = std::stoul(temp_registro.campos[0]);
                    if (current_found_id == id_registro) {
                        // Marcar el registro como eliminado (no borrar físicamente para permitir compactación)
                        // Para simulación, rellenaremos con un patrón o ceros para indicar eliminación
                        std::memset(datos_pagina + current_record_offset, 0xDD, record_size_guess); // Patrón de "eliminado"

                        cabecera_datos->numero_registros_activos--;
                        cabecera_datos->numero_registros_eliminados++;
                        cabecera_datos->necesita_compactacion = true; // La página necesita compactación

                        metadata_tabla->DecrementarNumeroRegistros();
                        gestor_catalogo_->ActualizarMetadataTabla(metadata_tabla);

                        gestor_buffer_->UnpinPage(page_id, true); // Marcar sucia
                        ActualizarEstadisticasPagina(page_id, "eliminacion");
                        total_eliminaciones_++;
                        std::cout << "Registro " << id_registro << " marcado para eliminación en tabla '" << nombre_tabla << "'." << std::endl;
                        return Status::OK;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Advertencia: Error al parsear ID de registro en página " << page_id << " durante eliminación: " << e.what() << std::endl;
                }
            }
        }
        gestor_buffer_->UnpinPage(page_id, false);
    }

    std::cout << "Registro " << id_registro << " no encontrado en tabla '" << nombre_tabla << "' para eliminar." << std::endl;
    return Status::NOT_FOUND;
}

Status GestorRegistros::CompactarPagina(PageId id_pagina) {
    Pagina pagina_info;
    Status pin_status = gestor_buffer_->PinPage(id_pagina, pagina_info);
    if (pin_status != Status::OK) {
        std::cerr << "Error: No se pudo anclar la página " << id_pagina << " para compactación: " << StatusToString(pin_status) << std::endl;
        return pin_status;
    }

    Byte* datos_pagina = gestor_buffer_->GetPageData(id_pagina);
    if (!datos_pagina) {
        gestor_buffer_->UnpinPage(id_pagina, false);
        return Status::ERROR;
    }

    CabeceraComun* cabecera_comun = reinterpret_cast<CabeceraComun*>(datos_pagina);
    CabeceraBloqueDatos* cabecera_datos = reinterpret_cast<CabeceraBloqueDatos*>(datos_pagina + sizeof(CabeceraComun));

    if (!cabecera_datos->necesita_compactacion) {
        std::cout << "Página " << id_pagina << " no necesita compactación." << std::endl;
        gestor_buffer_->UnpinPage(id_pagina, false);
        return Status::OK;
    }

    std::vector<DatosRegistro> registros_activos;
    uint32_t record_size_guess = 128; // Estimado, se debería obtener del esquema de la tabla

    // Necesitamos el esquema de la tabla para deserializar correctamente
    // Esto implicaría buscar la tabla a la que pertenece esta página, lo cual
    // es complejo sin un mapeo PageId -> TableId.
    // Para esta simulación, asumiremos un esquema genérico o lo pasaremos como parámetro.
    // O, más simple, compactaremos basándonos en el patrón de "eliminado" (0xDD)
    // y el tamaño fijo o estimado.

    // En un sistema real, se iteraría el directorio de slots, moviendo registros activos
    // al inicio del espacio de datos y actualizando los offsets en el directorio.

    // SIMPLIFICACIÓN: Reconstruir la página solo con registros no marcados con 0xDD
    std::vector<Byte> temp_buffer(BLOCK_SIZE, 0); // Buffer temporal para la compactación
    uint32_t current_write_offset = sizeof(CabeceraComun) + sizeof(CabeceraBloqueDatos);
    uint32_t records_moved = 0;

    // Copiar la cabecera común y la de datos al buffer temporal
    std::memcpy(temp_buffer.data(), datos_pagina, sizeof(CabeceraComun) + sizeof(CabeceraBloqueDatos));

    // Iterar sobre los registros existentes
    for (uint32_t i = 0; i < cabecera_datos->numero_registros_activos + cabecera_datos->numero_registros_eliminados; ++i) {
        uint32_t current_read_offset = sizeof(CabeceraComun) + sizeof(CabeceraBloqueDatos) + (i * record_size_guess);
        if (current_read_offset + record_size_guess > BLOCK_SIZE) {
            break;
        }

        // Verificar si el registro está "eliminado" (marcado con 0xDD)
        bool is_deleted = true;
        for (uint32_t byte_idx = 0; byte_idx < record_size_guess; ++byte_idx) {
            if (datos_pagina[current_read_offset + byte_idx] != (Byte)0xDD) {
                is_deleted = false;
                break;
            }
        }

        if (!is_deleted) {
            // Es un registro activo, moverlo al espacio compactado
            std::memcpy(temp_buffer.data() + current_write_offset, datos_pagina + current_read_offset, record_size_guess);
            current_write_offset += record_size_guess;
            records_moved++;
        }
    }

    // Copiar los datos compactados de vuelta a la página original
    std::memcpy(datos_pagina, temp_buffer.data(), BLOCK_SIZE);

    // Actualizar la cabecera de datos después de la compactación
    cabecera_datos->numero_registros_activos = records_moved;
    cabecera_datos->numero_registros_eliminados = 0; // Todos los eliminados fueron removidos
    cabecera_datos->espacio_libre_total = BLOCK_SIZE - current_write_offset; // Update free space
    cabecera_datos->fragmentacion_interna = 0; // No hay fragmentación interna después de compactar
    cabecera_datos->necesita_compactacion = false;

    gestor_buffer_->UnpinPage(id_pagina, true); // Marcar sucia
    ActualizarEstadisticasPagina(id_pagina, "compactacion");
    total_compactaciones_++;
    std::cout << "Página " << id_pagina << " compactada exitosamente. Registros activos: " << records_moved << std::endl;
    return Status::OK;
}

void GestorRegistros::ImprimirEstadisticasTabla(const std::string& nombre_tabla) {
    if (!gestor_catalogo_) {
        std::cerr << "Error: GestorCatalogo no está configurado." << std::endl;
        return;
    }

    std::shared_ptr<MetadataTabla> metadata_tabla = gestor_catalogo_->ObtenerMetadataTabla(nombre_tabla);
    if (!metadata_tabla) {
        std::cerr << "Error: Tabla '" << nombre_tabla << "' no encontrada." << std::endl;
        return;
    }

    std::cout << "\n=== ESTADÍSTICAS DE TABLA: " << nombre_tabla << " ===" << std::endl;
    std::cout << "ID de Tabla: " << metadata_tabla->ObtenerIdTabla() << std::endl;
    std::cout << "Número de Registros: " << metadata_tabla->ObtenerNumeroRegistros() << std::endl;
    std::cout << "Longitud Fija: " << (metadata_tabla->EsLongitudFija() ? "Sí" : "No") << std::endl;
    if (metadata_tabla->EsLongitudFija()) {
        std::cout << "Tamaño Registro Fijo: " << metadata_tabla->ObtenerTamanoRegistroFijo() << " bytes" << std::endl;
    }
    std::cout << "Páginas de Datos Asociadas: ";
    if (metadata_tabla->ObtenerPaginasDatos().empty()) {
        std::cout << "Ninguna" << std::endl;
    } else {
        for (PageId page_id : metadata_tabla->ObtenerPaginasDatos()) {
            std::cout << page_id << " ";
        }
        std::cout << std::endl;
    }

    std::cout << "\nEstadísticas por Página de Datos:" << std::endl;
    for (PageId page_id : metadata_tabla->ObtenerPaginasDatos()) {
        auto it = estadisticas_paginas_.find(page_id); // Corrected typo
        if (it != estadisticas_paginas_.end()) {
            const auto& stats = it->second;
            std::cout << "  Página " << stats.id_pagina << ":" << std::endl;
            std::cout << "    Total Registros: " << stats.numero_registros << std::endl;
            std::cout << "    Registros Activos: " << stats.registros_activos << std::endl;
            std::cout << "    Registros Eliminados: " << stats.registros_eliminados << std::endl;
            std::cout << "    Espacio Libre: " << stats.espacio_libre_bytes << " bytes" << std::endl;
            std::cout << "    Fragmentación: " << stats.fragmentacion_bytes << " bytes" << std::endl;
            std::cout << "    Última Actualización: " << stats.timestamp_ultima_actualizacion << std::endl;
        } else {
            std::cout << "  Página " << page_id << ": Estadísticas no disponibles." << std::endl;
        }
    }
    std::cout << "---------------------------------------" << std::endl;
}

void GestorRegistros::ImprimirEstadisticasGenerales() {
    std::cout << "\n=== ESTADÍSTICAS GENERALES DEL GESTOR DE REGISTROS ===" << std::endl;
    std::cout << "Total de Inserciones: " << total_inserciones_ << std::endl;
    std::cout << "Total de Actualizaciones: " << total_actualizaciones_ << std::endl;
    std::cout << "Total de Eliminaciones: " << total_eliminaciones_ << std::endl;
    std::cout << "Total de Consultas: " << total_consultas_ << std::endl;
    std::cout << "Total de Compactaciones: " << total_compactaciones_ << std::endl;
    
    uint64_t total_operaciones = total_inserciones_ + total_actualizaciones_ + 
                                total_eliminaciones_ + total_consultas_;
    if (total_operaciones > 0) {
        std::cout << "\nDistribución de Operaciones:" << std::endl;
        std::cout << "  Inserciones: " << (total_inserciones_ * 100.0 / total_operaciones) << "%" << std::endl;
        std::cout << "  Actualizaciones: " << (total_actualizaciones_ * 100.0 / total_operaciones) << "%" << std::endl;
        std::cout << "  Eliminaciones: " << (total_eliminaciones_ * 100.0 / total_operaciones) << "%" << std::endl;
        std::cout << "  Consultas: " << (total_consultas_ * 100.0 / total_operaciones) << "%" << std::endl;
    }

    std::cout << "\nEstadísticas Consolidadas de Páginas:" << std::endl;
    uint32_t total_paginas_con_stats = 0;
    uint64_t total_registros_todos_paginas = 0;
    uint64_t total_registros_activos_todos_paginas = 0;
    uint64_t total_registros_eliminados_todos_paginas = 0;
    uint64_t total_espacio_libre_todos_paginas = 0;
    uint64_t total_fragmentacion_todos_paginas = 0;

    for (const auto& pair : estadisticas_paginas_) { // Corrected typo
        const auto& stats = pair.second;
        total_paginas_con_stats++;
        total_registros_todos_paginas += stats.numero_registros;
        total_registros_activos_todos_paginas += stats.registros_activos;
        total_registros_eliminados_todos_paginas += stats.registros_eliminados;
        total_espacio_libre_todos_paginas += stats.espacio_libre_bytes;
        total_fragmentacion_todos_paginas += stats.fragmentacion_bytes;
    }

    if (total_paginas_con_stats > 0) {
        std::cout << "  Páginas con Estadísticas: " << total_paginas_con_stats << std::endl;
        std::cout << "  Registros (Total): " << total_registros_todos_paginas << std::endl;
        std::cout << "  Registros Activos (Total): " << total_registros_activos_todos_paginas << std::endl;
        std::cout << "  Registros Eliminados (Total): " << total_registros_eliminados_todos_paginas << std::endl;
        std::cout << "  Espacio Libre Promedio por Página: " << (total_espacio_libre_todos_paginas / total_paginas_con_stats) << " bytes" << std::endl;
        std::cout << "  Fragmentación Promedio por Página: " << (total_fragmentacion_todos_paginas / total_paginas_con_stats) << " bytes" << std::endl;
    } else {
        std::cout << "  No hay estadísticas de páginas disponibles." << std::endl;
    }
    std::cout << "---------------------------------------" << std::endl;
}

// === MÉTODOS AUXILIARES PRIVADOS ===

std::vector<Byte> GestorRegistros::SerializarRegistro(const DatosRegistro& datos_registro, 
                                                     const EsquemaTablaCompleto& esquema) const {
    std::vector<Byte> buffer;
    std::stringstream ss;

    // Para simplificar, serializaremos los campos como strings separados por un delimitador
    // En un sistema real, se usaría un formato binario más eficiente y preciso.
    for (size_t i = 0; i < datos_registro.campos.size(); ++i) {
        ss << datos_registro.campos[i];
        if (i < datos_registro.campos.size() - 1) {
            ss << "|"; // Delimitador simple
        }
    }
    std::string serialized_string = ss.str();
    
    // Copiar la cadena serializada al buffer de bytes
    buffer.resize(serialized_string.length());
    std::memcpy(buffer.data(), serialized_string.c_str(), serialized_string.length());

    return buffer;
}

DatosRegistro GestorRegistros::DeserializarRegistro(const std::vector<Byte>& datos_raw, 
                                                   const EsquemaTablaCompleto& esquema) const {
    DatosRegistro registro;
    if (datos_raw.empty()) {
        return registro;
    }

    std::string raw_string(datos_raw.begin(), datos_raw.end());
    std::stringstream ss(raw_string);
    std::string segment;

    // Deserializar campos basados en el esquema
    for (size_t i = 0; i < esquema.columns.size(); ++i) {
        if (std::getline(ss, segment, '|')) { // Usar el mismo delimitador
            registro.AñadirCampo(esquema.columns[i].name, segment);
        } else {
            // No hay suficientes segmentos, rellenar con vacío o error
            registro.AñadirCampo(esquema.columns[i].name, ""); 
        }
    }
    return registro;
}

bool GestorRegistros::BuscarSlotLibre(Byte* datos_pagina, const CabeceraBloqueDatos& cabecera_datos, 
                                    uint32_t& id_slot_libre) {
    // Para tablas de longitud fija, un slot libre es simplemente un espacio no usado.
    // Para tablas de longitud variable, se necesitaría un directorio de slots.
    // En esta simulación, asumimos que los registros se insertan de forma contigua
    // y el "slot libre" es el siguiente espacio disponible.

    // Si la página tiene registros eliminados, se podría implementar lógica para reutilizar esos slots.
    // Por ahora, solo buscamos el siguiente espacio después del último registro activo.
    
    // Este método es una simplificación. En un SGBD real, la gestión de slots es más compleja.
    // Si la tabla es de longitud fija, el id_slot_libre podría ser el índice del registro.
    // Si es de longitud variable, se buscaría en el directorio de slots.

    // Dada la estructura actual de CabeceraBloqueDatos, no hay un "directorio de slots" explícito aquí.
    // La lógica de "espacio libre" se basa en `espacio_libre_total` y `offset_espacio_libre`.
    // Para esta implementación simplificada, `BuscarSlotLibre` no es directamente aplicable
    // a cómo se gestiona el espacio en `InsertarRegistro`.
    // La lógica de `CalcularOffsetInsercion` ya maneja la búsqueda de espacio.
    // Este método podría ser más relevante si tuviéramos un bitmap de slots o un directorio.

    // Retornamos false para indicar que la búsqueda de un "slot libre" específico
    // no es el enfoque principal con la implementación actual de espacio contiguo.
    id_slot_libre = 0; // Valor por defecto
    return false;
}

void GestorRegistros::ActualizarEstadisticasPagina(PageId id_pagina, const std::string& tipo_operacion) {
    // Obtener la página y su cabecera para actualizar estadísticas
    Pagina pagina_info;
    Status pin_status = gestor_buffer_->PinPage(id_pagina, pagina_info);
    if (pin_status != Status::OK) {
        std::cerr << "Error al anclar página " << id_pagina << " para actualizar estadísticas: " << StatusToString(pin_status) << std::endl;
        return;
    }

    Byte* datos_pagina = gestor_buffer_->GetPageData(id_pagina);
    if (!datos_pagina) {
        gestor_buffer_->UnpinPage(id_pagina, false);
        return;
    }

    CabeceraComun* cabecera_comun = reinterpret_cast<CabeceraComun*>(datos_pagina);
    CabeceraBloqueDatos* cabecera_datos = reinterpret_cast<CabeceraBloqueDatos*>(datos_pagina + sizeof(CabeceraComun));

    // Obtener o crear la entrada de estadísticas para esta página
    EstadisticasPagina& stats = estadisticas_paginas_[id_pagina]; // Corrected typo
    stats.id_pagina = id_pagina;

    // Actualizar contadores de registros
    stats.numero_registros = cabecera_datos->numero_registros_activos + cabecera_datos->numero_registros_eliminados;
    stats.registros_activos = cabecera_datos->numero_registros_activos;
    stats.registros_eliminados = cabecera_datos->numero_registros_eliminados;
    
    // Actualizar espacio libre y fragmentación
    stats.espacio_libre_bytes = cabecera_datos->espacio_libre_total;
    stats.fragmentacion_bytes = cabecera_datos->fragmentacion_interna; // Asumiendo que esta es la fragmentación

    stats.timestamp_ultima_actualizacion = ObtenerTimestampActual();

    gestor_buffer_->UnpinPage(id_pagina, true); // Marcar sucia si las estadísticas de la cabecera cambiaron
}

bool GestorRegistros::ValidarDatosRegistro(const DatosRegistro& datos_registro, 
                                         const EsquemaTablaCompleto& esquema) const {
    if (datos_registro.campos.size() != esquema.columns.size()) {
        std::cerr << "Error de validación: Número de campos en el registro (" << datos_registro.campos.size()
                  << ") no coincide con el número de columnas en el esquema (" << esquema.columns.size() << ")." << std::endl;
        return false;
    }

    for (size_t i = 0; i < datos_registro.campos.size(); ++i) {
        const auto& campo = datos_registro.campos[i];
        const auto& columna = esquema.columns[i];

        switch (columna.type) {
            case ColumnType::INT:
                try {
                    std::stoll(campo); // Intentar convertir a long long para validar
                } catch (const std::exception&) {
                    std::cerr << "Error de validación: Campo '" << columna.name 
                              << "' se esperaba INT, pero el valor '" << campo << "' no es un entero válido." << std::endl;
                    return false;
                }
                break;
            case ColumnType::CHAR:
                if (campo.length() > columna.size) {
                    std::cerr << "Error de validación: Campo '" << columna.name 
                              << "' (CHAR) excede el tamaño fijo permitido (" << columna.size 
                              << "). Longitud actual: " << campo.length() << "." << std::endl;
                    return false;
                }
                break;
            case ColumnType::STRING:
                // No hay validación de tamaño máximo para STRING aquí, pero se podría añadir
                break;
            default:
                std::cerr << "Error de validación: Tipo de columna desconocido para '" << columna.name << "'." << std::endl;
                return false;
        }
    }
    return true;
}

bool GestorRegistros::CalcularOffsetInsercion(Byte* datos_pagina, uint32_t tamaño_registro, 
                                            uint32_t& offset_insercion) {
    CabeceraComun* cabecera_comun = reinterpret_cast<CabeceraComun*>(datos_pagina);
    CabeceraBloqueDatos* cabecera_datos = reinterpret_cast<CabeceraBloqueDatos*>(datos_pagina + sizeof(CabeceraComun));

    // En un esquema simple de registros contiguos, el offset de inserción es
    // después del último byte usado.
    uint32_t current_used_space = BLOCK_SIZE - cabecera_datos->espacio_libre_total;
    offset_insercion = current_used_space;

    if (cabecera_datos->espacio_libre_total >= tamaño_registro) {
        // Asegurarse de que el offset no sobrepase los límites del bloque
        if (offset_insercion + tamaño_registro <= BLOCK_SIZE) {
            return true;
        }
    }
    return false;
}
