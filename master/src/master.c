#include "master.h"

// Estructura para pasar datos a los hilos de conexión
typedef struct {
    master_t* master;
    int client_socket;
} connection_data_t;

master_t* master_crear(char* config_path) {
    master_t* master = malloc(sizeof(master_t));
    if (!master) {
        return NULL;
    }

    // Cargar configuración
    master->config = cargar_config(config_path);
    if (!master->config) {
        free(master);
        return NULL;
    }

    // Crear logger con el nivel configurado
    t_log_level log_level = log_level_from_string(master->config->log_level);
    master->logger = log_create("master.log", "MASTER", true, log_level);
    if (!master->logger) {
        master_config_destruir(master->config);
        free(master);
        return NULL;
    }

    // Inicializar generador de IDs
    master->next_query_id = 0;
    master->next_worker_id = 1;  // Inicializar contador de workers en 1

    // Inicializar estructuras de datos
    master->ready_queue = queue_create();
    master->exec_map = dictionary_create();
    master->pending_preemptions = dictionary_create();
    master->pending_cancellations = dictionary_create();
    master->workers = list_create();
    master->query_controls = list_create();

    // Inicializar mutex principal
    pthread_mutex_init(&master->main_mutex, NULL);

    // Inicializar contadores
    master->worker_count = 0;
    master->running = false;

    log_info(master->logger, "[MASTER] Master inicializado correctamente");
    
    return master;
}

void master_destruir(master_t* master) {
    if (!master) return;

    log_info(master->logger, "[MASTER] Cerrando Master...");

    // Detener hilos si están corriendo
    if (master->running) {
        master_detener(master);
    }

    // Destruir estructuras de datos
    if (master->ready_queue) {
        queue_destroy_and_destroy_elements(master->ready_queue, (void*)query_destruir);
    }
    
    if (master->exec_map) {
        dictionary_destroy_and_destroy_elements(master->exec_map, (void*)query_destruir);
    }
    
    // NOTA: pending_preemptions y pending_cancellations contienen REFERENCIAS a queries
    // que están en otras estructuras, NO deben destruir los elementos
    if (master->pending_preemptions) {
        dictionary_destroy(master->pending_preemptions);
    }
    
    if (master->pending_cancellations) {
        dictionary_destroy(master->pending_cancellations);
    }
    
    if (master->workers) {
        list_destroy_and_destroy_elements(master->workers, (void*)worker_destruir);
    }
    
    if (master->query_controls) {
        list_destroy_and_destroy_elements(master->query_controls, (void*)query_control_destruir);
    }

    // Destruir mutex
    pthread_mutex_destroy(&master->main_mutex);

    // Cerrar socket
    if (master->server_socket > 0) {
        close(master->server_socket);
    }

    // Cleanup final
    master_config_destruir(master->config);
    log_destroy(master->logger);
    free(master);
}

int configurar_socket_servidor(int port) {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error creando socket");
        return -1;
    }

    // Permitir reutilizar la dirección
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Error en setsockopt");
        close(server_socket);
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en bind");
        close(server_socket);
        return -1;
    }

    if (listen(server_socket, 10) < 0) {
        perror("Error en listen");
        close(server_socket);
        return -1;
    }

    return server_socket;
}

void* manejar_conexion(void* arg) {
    connection_data_t* conn_data = (connection_data_t*)arg;
    master_t* master = conn_data->master;
    int client_socket = conn_data->client_socket;
    free(conn_data);

    log_info(master->logger, "[MASTER] Nueva conexión establecida (socket %d)", client_socket);
    
    while (master->running) {
        op_code codigo;
        int size;
        void* payload = recibir_payload(client_socket, &codigo, &size);
        
        if (!payload && size == 0) {
            log_info(master->logger, "[MASTER] Cliente desconectado (socket %d)", client_socket);
            // Manejar desconexión
            manejar_desconexion_cliente(master, client_socket);
            break;
        }

        log_debug(master->logger, "[MASTER] Mensaje recibido código %d, tamaño %d", codigo, size);

        // Manejar según tipo de mensaje
        switch (codigo) {
            case NEW_QUERY:
            case HANDSHAKE_QUERY_CONTROL:
                manejar_mensaje_query_control(master, client_socket, codigo, payload, size);
                break;
            case HANDSHAKE_WORKER:
            case PREEMPTION_ACK:
            case QUERY_FINISHED:
            case READ_RESULT:
            case CANCEL_QUERY:  // Respuesta del worker tras cancelación (reutiliza PREEMPTION_ACK)
                manejar_mensaje_worker(master, client_socket, codigo, payload, size);
                break;
            default:
                log_warning(master->logger, "[MASTER] Tipo de mensaje desconocido %d desde socket %d", codigo, client_socket);
                break;
        }
        
        // Liberar payload
        if (payload) free(payload);
    }

    close(client_socket);
    return NULL;
}

void master_iniciar(master_t* master) {
    if (!master) return;

    // Configurar socket del servidor
    master->server_socket = configurar_socket_servidor(master->config->puerto_escucha);
    if (master->server_socket < 0) {
        log_error(master->logger, "[MASTER] Error configurando servidor");
        return;
    }

    master->running = true;
    log_info(master->logger, "[MASTER] Servidor iniciado en puerto %d", master->config->puerto_escucha);

    // Iniciar hilo de aging si está habilitado
    if (master->config->tiempo_aging > 0) {
        if (pthread_create(&master->aging_thread, NULL, funcion_hilo_aging, master) != 0) {
            log_warning(master->logger, "[MASTER] Error creando hilo aging");
        }
    }

    // Loop principal - aceptar conexiones
    while (master->running) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        int client_socket = accept(master->server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            if (master->running) {
                log_error(master->logger, "[MASTER] Error aceptando conexión");
            }
            continue;
        }

        // Crear hilo para manejar la conexión
        connection_data_t* conn_data = malloc(sizeof(connection_data_t));
        conn_data->master = master;
        conn_data->client_socket = client_socket;

        pthread_t connection_thread;
        if (pthread_create(&connection_thread, NULL, manejar_conexion, conn_data) != 0) {
            log_error(master->logger, "[MASTER] Error creando hilo para conexión");
            close(client_socket);
            free(conn_data);
        } else {
            pthread_detach(connection_thread);
        }
    }
}

void master_detener(master_t* master) {
    if (!master || !master->running) return;

    log_info(master->logger, "[MASTER] Deteniendo servidor...");
    master->running = false;

    // Cerrar socket para salir del accept()
    if (master->server_socket > 0) {
        close(master->server_socket);
        master->server_socket = -1;
    }

    // Esperar a que termine el hilo de aging
    if (master->config->tiempo_aging > 0 && master->aging_thread) {
        pthread_join(master->aging_thread, NULL);
    }
}

uint64_t generar_id_query(master_t* master) {
    pthread_mutex_lock(&master->main_mutex);
    uint64_t id = master->next_query_id++;
    pthread_mutex_unlock(&master->main_mutex);
    return id;
}

// ========== MANEJO DE DESCONEXIONES ==========

void manejar_desconexion_cliente(master_t* master, int client_socket) {
    if (!master) return;
    
    // Verificar si es un worker
    worker_t* disconnected_worker = buscar_worker_por_socket(master, client_socket);
    if (disconnected_worker) {
        manejar_desconexion_worker(master, disconnected_worker);
        return;
    }
    
    // Verificar si es un query control
    query_control_t* disconnected_qc = buscar_query_control_por_socket(master, client_socket);
    if (disconnected_qc) {
        manejar_desconexion_query_control(master, disconnected_qc);
        return;
    }
    
    log_warning(master->logger, "[MASTER] Desconexión de cliente desconocido (socket %d)", client_socket);
}

void manejar_desconexion_worker(master_t* master, worker_t* worker) {
    if (!master || !worker) return;
    
    uint64_t affected_query_id = 0;
    
    pthread_mutex_lock(&master->main_mutex);
    
    // Si el worker tenía una query asignada, finalizarla con error
    if (worker->current_query_id != 0) {
        query_t* affected_query = (query_t*)dictionary_get(master->exec_map, worker->id);
        if (affected_query) {
            affected_query_id = affected_query->id;
            
            // Log del desalojo por desconexión
            log_desalojo_por_desconexion(master->logger, affected_query->id, affected_query->priority, worker->id);
            
            // Notificar al Query Control sobre el error usando utils
            int error_size = 0;
            void* error_payload = serializar_ack_con_id(affected_query->id, &error_size);
            if (enviar_paquete(affected_query->qc_socket, ERROR, error_payload, error_size) != 0) {
                log_warning(master->logger, "[MASTER] Error enviando ERROR al Query Control (query %lu), posiblemente desconectado", 
                           affected_query->id);
            }
            free(error_payload);
            
            // Remover la query del exec_map
            dictionary_remove(master->exec_map, worker->id);
            
            // Destruir la query
            query_destruir(affected_query);
        }
    }
    
    // Verificar si había una query esperando en pending_preemptions
    query_t* pending_query = (query_t*)dictionary_get(master->pending_preemptions, worker->id);
    if (pending_query) {
        log_info(master->logger, "[MASTER] Query %lu estaba esperando desalojo en worker %s desconectado, moviendo a READY", 
                 pending_query->id, worker->id);
        
        // Devolver la query pendiente a la cola READY
        pending_query->state = QUERY_READY;
        queue_push(master->ready_queue, pending_query);
        
        // Remover de pending_preemptions
        dictionary_remove(master->pending_preemptions, worker->id);
    }
    
    // Verificar si había una query siendo cancelada en pending_cancellations
    query_t* canceling_query = (query_t*)dictionary_get(master->pending_cancellations, worker->id);
    if (canceling_query) {
        log_info(master->logger, "[MASTER] Query %lu estaba siendo cancelada en worker %s desconectado, finalizando", 
                 canceling_query->id, worker->id);
        
        // Finalizar la query cancelada
        canceling_query->state = QUERY_EXIT;
        dictionary_remove(master->pending_cancellations, worker->id);
        query_destruir(canceling_query);
    }
    
    // Log de desconexión del worker
    log_worker_disconnect(master->logger, worker->id, affected_query_id, master->worker_count - 1);
    
    // Remover worker de la lista
    list_remove_element(master->workers, worker);
    master->worker_count--;
    
    pthread_mutex_unlock(&master->main_mutex);
    
    // Destruir worker
    worker_destruir(worker);
    
    // Intentar replanificar queries pendientes
    planificar_siguiente_query(master);
}

void manejar_desconexion_query_control(master_t* master, query_control_t* qc) {
    if (!master || !qc) return;
    
    if (qc->connected_query_id != 0) {
        uint64_t query_id = qc->connected_query_id;
        query_t* query_to_cancel = NULL;
        
        pthread_mutex_lock(&master->main_mutex);
        
        // Buscar en exec_map
        t_list* worker_ids = dictionary_keys(master->exec_map);
        
        for (int i = 0; i < list_size(worker_ids); i++) {
            char* worker_id = (char*)list_get(worker_ids, i);
            query_t* query = (query_t*)dictionary_get(master->exec_map, worker_id);
            
            if (query && query->id == query_id) {
                query_to_cancel = query;
                
                // Buscar worker directamente (ya tenemos el mutex)
                worker_t* worker = NULL;
                for (int j = 0; j < list_size(master->workers); j++) {
                    worker_t* w = (worker_t*)list_get(master->workers, j);
                    if (w && strcmp(w->id, worker_id) == 0) {
                        worker = w;
                        break;
                    }
                }
                
                if (worker) {
                    // IMPLEMENTACIÓN CORRECTA según enunciado:
                    // Se debe solicitar desalojo y ESPERAR la respuesta con el PC
                    
                    // Marcar worker como PREEMPTING (cancelando en este caso)
                    worker->status = WORKER_PREEMPTING;
                    query_to_cancel->state = QUERY_CANCELING;
                    
                    // Guardar query en pending_cancellations para esperar respuesta
                    dictionary_put(master->pending_cancellations, worker->id, query_to_cancel);
                    
                    // Enviar cancelación usando utils
                    int cancel_size = 0;
                    void* cancel_payload = serializar_ack_con_id(query_id, &cancel_size);
                    
                    if (enviar_paquete(worker->socket, CANCEL_QUERY, cancel_payload, cancel_size) == 0) {
                        log_info(master->logger, "[MASTER] Solicitud de cancelación enviada al worker %s para query %lu", 
                                 worker->id, query_id);
                        // NO remover de exec_map ni marcar worker como IDLE aquí
                        // Eso se hará cuando el worker responda con el PC
                    } else {
                        log_error(master->logger, "[MASTER] Error enviando cancelación al worker %s", worker->id);
                        
                        // Si falla el envío, limpiar inmediatamente
                        worker->status = WORKER_IDLE;
                        worker->current_query_id = 0;
                        query_to_cancel->state = QUERY_EXIT;
                        dictionary_remove(master->pending_cancellations, worker->id);
                        dictionary_remove(master->exec_map, worker->id);
                    }
                    
                    free(cancel_payload);
                } else {
                    // Worker no encontrado, remover directamente
                    dictionary_remove(master->exec_map, worker_id);
                }
                break;
            }
        }
        
        list_destroy(worker_ids);
        
        // Si no está en ejecución, buscar en ready_queue
        if (!query_to_cancel) {
            t_list* temp_list = list_create();
            
            while (!queue_is_empty(master->ready_queue)) {
                query_t* query = (query_t*)queue_pop(master->ready_queue);
                if (query->id == query_id) {
                    query_to_cancel = query;
                } else {
                    list_add(temp_list, query);
                }
            }
            
            // Devolver queries no canceladas a la queue
            for (int i = 0; i < list_size(temp_list); i++) {
                query_t* query = (query_t*)list_get(temp_list, i);
                queue_push(master->ready_queue, query);
            }
            
            list_destroy(temp_list);
        }
        
        if (query_to_cancel) {
            // Según enunciado: "En el caso de que la Query se encuentre en READY, 
            // la misma se deberá enviar a EXIT directamente"
            query_to_cancel->state = QUERY_EXIT;
            
            // Log de desconexión
            log_query_control_disconnect(master->logger, query_id, query_to_cancel->priority, master->worker_count);
            
            // Destruir la query cancelada
            query_destruir(query_to_cancel);
        }
        
        // Remover query control de la lista
        list_remove_element(master->query_controls, qc);
        
        pthread_mutex_unlock(&master->main_mutex);
    }
    
    // Destruir query control
    query_control_destruir(qc);
    
    // Intentar replanificar queries pendientes con los workers disponibles
    planificar_siguiente_query(master);
}
