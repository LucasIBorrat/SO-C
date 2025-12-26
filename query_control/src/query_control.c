#include "query_control.h"
#include <errno.h>
#include <limits.h>

// ========== FUNCIONES PRINCIPALES ==========

query_control_t* query_control_crear(char* config_path, char* archivo_query, int prioridad) {
    query_control_t* qc = malloc(sizeof(query_control_t));
    if (!qc) {
        printf("[ERROR] No se pudo asignar memoria para Query Control\n");
        return NULL;
    }

    // Cargar configuración
    qc->config = cargar_config_qc(config_path);
    if (!qc->config) {
        free(qc);
        return NULL;
    }

    // Crear logger
    t_log_level log_level = log_level_from_string(qc->config->log_level);
    qc->logger = log_create("query_control.log", "QUERY_CONTROL", true, log_level);
    if (!qc->logger) {
        config_qc_destruir(qc->config);
        free(qc);
        return NULL;
    }

    // Inicializar valores
    qc->socket_master = -1;
    qc->query_id = 0;
    qc->archivo_query = strdup(archivo_query);
    qc->prioridad = prioridad;
    qc->query_finished = false;
    qc->connected = false;
    pthread_mutex_init(&qc->mutex, NULL);

    log_info(qc->logger, "[QUERY_CONTROL] Query Control inicializado para archivo %s con prioridad %d", 
             archivo_query, prioridad);

    return qc;
}

void query_control_destruir(query_control_t* qc) {
    if (!qc) return;

    log_info(qc->logger, "[QUERY_CONTROL] Cerrando Query Control...");

    // Desconectar si está conectado usando función de utils
    if (qc->connected) {
        query_control_desconectar(qc);
    }

    // Limpiar recursos
    if (qc->archivo_query) {
        free(qc->archivo_query);
    }

    config_qc_destruir(qc->config);
    pthread_mutex_destroy(&qc->mutex);
    
    if (qc->logger) {
        log_destroy(qc->logger);
    }

    free(qc);
}

bool query_control_conectar(query_control_t* qc) {
    if (!qc || qc->connected) {
        return false;
    }

    // Usar crear_conexion de utils (igual que Worker)
    char puerto_str[16];
    snprintf(puerto_str, sizeof(puerto_str), "%d", qc->config->puerto_master);
    
    qc->socket_master = crear_conexion(qc->logger, qc->config->ip_master, puerto_str);
    if (qc->socket_master == -1) {
        log_error(qc->logger, "[QUERY_CONTROL] Error conectando al Master %s:%d", 
                  qc->config->ip_master, qc->config->puerto_master);
        return false;
    }

    // Realizar handshake con el Master
    log_info(qc->logger, "[QUERY_CONTROL] Enviando handshake al Master...");
    if (enviar_paquete(qc->socket_master, HANDSHAKE_QUERY_CONTROL, NULL, 0) != 0) {
        log_error(qc->logger, "[QUERY_CONTROL] Error enviando handshake al Master");
        liberar_conexion(qc->socket_master);
        qc->socket_master = -1;
        return false;
    }

    // Esperar HANDSHAKE_OK del Master
    op_code codigo_handshake;
    int size_handshake;
    void* payload_handshake = recibir_payload(qc->socket_master, &codigo_handshake, &size_handshake);
    
    if (!payload_handshake && size_handshake == 0) {
        log_error(qc->logger, "[QUERY_CONTROL] Error recibiendo HANDSHAKE_OK del Master");
        liberar_conexion(qc->socket_master);
        qc->socket_master = -1;
        return false;
    }

    if (codigo_handshake != HANDSHAKE_OK) {
        log_error(qc->logger, "[QUERY_CONTROL] Handshake rechazado por el Master (código: %d)", codigo_handshake);
        if (payload_handshake) free(payload_handshake);
        liberar_conexion(qc->socket_master);
        qc->socket_master = -1;
        return false;
    }

    if (payload_handshake) free(payload_handshake);

    qc->connected = true;
    log_conexion_exitosa(qc->logger, qc->config->ip_master, qc->config->puerto_master);
    log_info(qc->logger, "[QUERY_CONTROL] Handshake con Master completado exitosamente");

    return true;
}

void query_control_desconectar(query_control_t* qc) {
    if (!qc) return;

    pthread_mutex_lock(&qc->mutex);
    
    // Solo desconectar si está conectado
    if (qc->connected && qc->socket_master != -1) {
        // Usar liberar_conexion de utils (igual que Worker)
        liberar_conexion(qc->socket_master);
        log_info(qc->logger, "[QUERY_CONTROL] Desconectado del Master");
    }
    
    qc->socket_master = -1;
    qc->connected = false;
    
    pthread_mutex_unlock(&qc->mutex);
}

bool query_control_enviar_query(query_control_t* qc) {
    if (!qc || !qc->connected) {
        return false;
    }

    // Verificar que el archivo existe. Intentamos abrir la ruta tal cual; si falla y
    // hay un queries_dir configurado, intentamos buscar ahí.
    FILE* archivo = fopen(qc->archivo_query, "r");
    char fallback_path[MAX_PATH_SIZE];
    bool opened = false;
    int saved_errno = 0;

    if (archivo) {
        opened = true;
        fclose(archivo);
        log_info(qc->logger, "[QUERY_CONTROL] Archivo de query encontrado: %s", qc->archivo_query);
    } else {
        saved_errno = errno;
        log_debug(qc->logger, "[QUERY_CONTROL] No se encontró '%s' en ruta directa: %s (errno=%d)", 
                 qc->archivo_query, strerror(saved_errno), saved_errno);
        
        // Si la ruta no es absoluta y tenemos queries_dir, intentar prefijar
        if (qc->config && qc->config->queries_dir && qc->config->queries_dir[0] != '\0' && qc->archivo_query[0] != '/') {
            // Construir ruta: queries_dir + '/' + archivo_query
            snprintf(fallback_path, sizeof(fallback_path), "%s/%s", qc->config->queries_dir, qc->archivo_query);
            log_debug(qc->logger, "[QUERY_CONTROL] Intentando en directorio alternativo: %s", fallback_path);
            
            archivo = fopen(fallback_path, "r");
            if (archivo) {
                opened = true;
                fclose(archivo);
                log_info(qc->logger, "[QUERY_CONTROL] Archivo de query encontrado en directorio de queries: %s", fallback_path);
            } else {
                log_debug(qc->logger, "[QUERY_CONTROL] Tampoco se encontró en: %s (errno=%d: %s)", 
                         fallback_path, errno, strerror(errno));
            }
        }
    }

    if (!opened) {
        // Log más informativo con errno
        log_error(qc->logger, "[QUERY_CONTROL] Error: el archivo de query '%s' no existe o no se puede leer: %s (errno=%d)",
                  qc->archivo_query, strerror(saved_errno ? saved_errno : errno), saved_errno ? saved_errno : errno);
        return false;
    }

    // Usar funciones de serialización de utils (igual que Master/Worker)
    // Se envía el NOMBRE del archivo (no su contenido), el Worker lo leerá
    int payload_size = 0;
    void* payload = serializar_new_query(qc->archivo_query, qc->prioridad, &payload_size);
    if (!payload) {
        log_error(qc->logger, "[QUERY_CONTROL] Error serializando NEW_QUERY");
        return false;
    }

    // Usar enviar_paquete de utils (igual que Master/Worker)
    if (enviar_paquete(qc->socket_master, NEW_QUERY, payload, payload_size) != 0) {
        log_error(qc->logger, "[QUERY_CONTROL] Error enviando NEW_QUERY al Master");
        free(payload);
        return false;
    }
    free(payload);

    log_envio_query(qc->logger, qc->archivo_query, qc->prioridad);
    return true;
}

void query_control_procesar_respuestas(query_control_t* qc) {
    if (!qc || !qc->connected) return;

    log_info(qc->logger, "[QUERY_CONTROL] Esperando respuestas del Master...");

    while (qc->connected && !qc->query_finished) {
        op_code codigo;
        int size;
        
        // Usar recibir_payload de utils (igual que Worker)
        void* payload = recibir_payload(qc->socket_master, &codigo, &size);
        
        // Verificar si la conexión se cerró
        if (!payload && size == 0) {
            log_error(qc->logger, "[QUERY_CONTROL] Error recibiendo mensaje del Master o conexión cerrada");
            pthread_mutex_lock(&qc->mutex);
            qc->query_finished = true;
            qc->connected = false;
            pthread_mutex_unlock(&qc->mutex);
            break;
        }

        // Procesar mensaje según op_code usando funciones de deserialización de utils
        switch (codigo) {
            case NEW_QUERY_ACK: {
                uint64_t query_id = 0;
                if (payload && size >= sizeof(uint64_t)) {
                    deserializar_ack_con_id(payload, &query_id);
                    manejar_new_query_ack(qc, query_id);
                } else {
                    log_error(qc->logger, "[QUERY_CONTROL] Error deserializando NEW_QUERY_ACK");
                }
                break;
            }
            case QUERY_FINISHED: {
                uint64_t query_id = 0;
                if (payload && size >= sizeof(uint64_t)) {
                    deserializar_ack_con_id(payload, &query_id);
                    manejar_query_finished(qc, query_id);
                } else {
                    log_error(qc->logger, "[QUERY_CONTROL] Error deserializando QUERY_FINISHED");
                }
                break;
            }
            case READ_RESULT: {
                uint64_t query_id = 0;
                char* origen = NULL;
                char* data = NULL;
                if (payload && size > 0) {
                    deserializar_read_result(payload, &query_id, &origen, &data);
                    manejar_read_result(qc, query_id, origen, data, strlen(data));
                    if (origen) free(origen);
                    if (data) free(data);
                } else {
                    log_error(qc->logger, "[QUERY_CONTROL] Error deserializando READ_RESULT");
                }
                break;
            }
            case ERROR: {
                uint64_t query_id = 0;
                if (payload && size >= sizeof(uint64_t)) {
                    deserializar_ack_con_id(payload, &query_id);
                    manejar_error(qc, query_id);
                } else {
                    log_error(qc->logger, "[QUERY_CONTROL] Error deserializando ERROR");
                    // Finalizar de todas formas
                    pthread_mutex_lock(&qc->mutex);
                    qc->query_finished = true;
                    pthread_mutex_unlock(&qc->mutex);
                    log_query_finalizada(qc->logger, "Error en la ejecución");
                }
                break;
            }
            default:
                log_warning(qc->logger, "[QUERY_CONTROL] Mensaje desconocido recibido: %d", codigo);
                break;
        }
        
        // Liberar payload
        if (payload) free(payload);
    }

    log_info(qc->logger, "[QUERY_CONTROL] Finalizado el procesamiento de respuestas");
}

void query_control_ejecutar(query_control_t* qc) {
    if (!qc) {
        printf("[ERROR] Query Control es NULL\n");
        return;
    }

    // Logear el directorio de trabajo actual para facilitar debugging
    char cwd[MAX_PATH_SIZE];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        log_info(qc->logger, "[QUERY_CONTROL] Directorio de trabajo actual (CWD): %s", cwd);
    }
    
    log_info(qc->logger, "[QUERY_CONTROL] Iniciando ejecución de Query Control");

    // Conectar al Master usando función de utils (con handshake incluido)
    if (!query_control_conectar(qc)) {
        printf("[ERROR] No se pudo conectar al Master\n");
        log_error(qc->logger, "[QUERY_CONTROL] Falló la conexión al Master");
        return;
    }

    // Enviar query al Master usando protocolo de utils
    if (!query_control_enviar_query(qc)) {
        printf("[ERROR] No se pudo enviar la query al Master\n");
        log_error(qc->logger, "[QUERY_CONTROL] Falló el envío de la query");
        query_control_desconectar(qc);
        return;
    }

    // Procesar respuestas del Master usando protocolo de utils
    // Este loop se mantendrá hasta recibir QUERY_FINISHED o ERROR
    query_control_procesar_respuestas(qc);

    // Desconectar de forma graceful usando función de utils
    log_info(qc->logger, "[QUERY_CONTROL] Finalizando conexión con el Master");
    query_control_desconectar(qc);
    
    log_info(qc->logger, "[QUERY_CONTROL] Ejecución de Query Control finalizada");
}

// ========== FUNCIONES DE CONFIGURACIÓN ==========

query_control_config_t* cargar_config_qc(char* config_path) {
    t_config* config = config_create(config_path);
    if (!config) {
        printf("[ERROR] No se pudo cargar el archivo de configuración: %s\n", config_path);
        return NULL;
    }

    query_control_config_t* qc_config = malloc(sizeof(query_control_config_t));
    if (!qc_config) {
        config_destroy(config);
        return NULL;
    }

    // Cargar valores de configuración
    char* ip_master = config_has_property(config, "IP_MASTER") ? 
                     config_get_string_value(config, "IP_MASTER") : "127.0.0.1";
    qc_config->ip_master = strdup(ip_master);

    qc_config->puerto_master = config_has_property(config, "PUERTO_MASTER") ? 
                              config_get_int_value(config, "PUERTO_MASTER") : 9001;

    char* log_level = config_has_property(config, "LOG_LEVEL") ? 
                     config_get_string_value(config, "LOG_LEVEL") : "INFO";
    qc_config->log_level = strdup(log_level);

    // Opcional: directorio donde se encuentran los archivos de query
    char* queries_dir = config_has_property(config, "QUERIES_DIR") ?
                        config_get_string_value(config, "QUERIES_DIR") : ".";
    qc_config->queries_dir = strdup(queries_dir);

    config_destroy(config);
    return qc_config;
}

void config_qc_destruir(query_control_config_t* config) {
    if (!config) return;
    
    if (config->ip_master) {
        free(config->ip_master);
    }
    if (config->log_level) {
        free(config->log_level);
    }
    if (config->queries_dir) {
        free(config->queries_dir);
    }
    
    free(config);
}

// ========== FUNCIONES DE MANEJO DE MENSAJES ==========

void manejar_new_query_ack(query_control_t* qc, uint64_t query_id) {
    if (!qc) return;

    pthread_mutex_lock(&qc->mutex);
    qc->query_id = query_id;
    pthread_mutex_unlock(&qc->mutex);

    log_info(qc->logger, "[QUERY_CONTROL] Query aceptada por Master con ID: %lu", query_id);
}

void manejar_query_finished(query_control_t* qc, uint64_t query_id) {
    if (!qc) return;

    // Verificar que el ID coincida
    if (qc->query_id != 0 && qc->query_id != query_id) {
        log_warning(qc->logger, "[QUERY_CONTROL] Query ID recibido (%lu) no coincide con el esperado (%lu)", 
                    query_id, qc->query_id);
    }

    pthread_mutex_lock(&qc->mutex);
    qc->query_finished = true;
    pthread_mutex_unlock(&qc->mutex);

    // Log obligatorio según enunciado
    log_query_finalizada(qc->logger, "Finalizada correctamente");
}

void manejar_read_result(query_control_t* qc, uint64_t query_id, char* origen, void* data, int data_size) {
    if (!qc) return;

    if (qc->query_id != 0 && qc->query_id != query_id) {
        log_warning(qc->logger, "[QUERY_CONTROL] Query ID recibido (%lu) no coincide con el esperado (%lu)", 
                    query_id, qc->query_id);
    }

    if (!origen || !data || data_size == 0) {
        log_error(qc->logger, "[QUERY_CONTROL] READ_RESULT recibido con datos inválidos");
        return;
    }

    // --- Lógica segura para loguear el contenido (que puede ser binario) ---
    // Creamos un string temporal y copiamos los datos
    char* contenido_log = malloc(data_size + 1);
    memcpy(contenido_log, data, data_size);
    contenido_log[data_size] = '\0'; // Aseguramos el fin de string

    // Log obligatorio según enunciado
    log_lectura_realizada(qc->logger, origen, contenido_log);
    
    free(contenido_log); // Liberamos el string temporal
}

void manejar_error(query_control_t* qc, uint64_t query_id) {
    if (!qc) return;

    // Verificar que el ID coincida
    if (qc->query_id != 0 && qc->query_id != query_id) {
        log_warning(qc->logger, "[QUERY_CONTROL] Query ID recibido (%lu) no coincide con el esperado (%lu)", 
                    query_id, qc->query_id);
    }

    pthread_mutex_lock(&qc->mutex);
    qc->query_finished = true;
    pthread_mutex_unlock(&qc->mutex);

    // Log obligatorio según enunciado
    log_query_finalizada(qc->logger, "Error en la ejecución");
}

// ========== FUNCIONES DE LOGGING ==========

void log_conexion_exitosa(t_log* logger, char* ip, int puerto) {
    if (!logger) return;
    
    // Log obligatorio según enunciado
    log_info(logger, "## Conexión al Master exitosa. IP: %s, Puerto: %d", ip, puerto);
}

void log_envio_query(t_log* logger, char* archivo_query, int prioridad) {
    if (!logger) return;
    
    // Log obligatorio según enunciado
    log_info(logger, "## Solicitud de ejecución de Query: %s, prioridad: %d", archivo_query, prioridad);
}

void log_lectura_realizada(t_log* logger, char* file_tag, char* contenido) {
    if (!logger) return;
    
    // Log obligatorio según enunciado (v1.1: usar "File" en lugar de "archivo")
    log_info(logger, "## Lectura realizada: File %s, contenido: %s", file_tag, contenido);
}

void log_query_finalizada(t_log* logger, char* motivo) {
    if (!logger) return;
    
    // Log obligatorio según enunciado
    log_info(logger, "## Query Finalizada - %s", motivo);
}