#include "master.h"

// ========== MANEJADORES DE MENSAJES DE QUERY CONTROL ==========

void manejar_mensaje_query_control(master_t* master, int client_socket, op_code codigo, void* payload, int size) {
    if (!master) return;

    switch (codigo) {
        case HANDSHAKE_QUERY_CONTROL: {
            // Enviar HANDSHAKE_OK al Query Control
            if (enviar_paquete(client_socket, HANDSHAKE_OK, NULL, 0) != 0) {
                log_error(master->logger, "[MASTER] Error enviando HANDSHAKE_OK al Query Control (socket %d)", client_socket);
                return;
            }
            
            log_info(master->logger, "[MASTER] Handshake con Query Control completado (socket %d)", client_socket);
            break;
        }
        
        case NEW_QUERY: {
            // Deserializar el payload
            char* path = NULL;
            int priority = 0;
            deserializar_new_query(payload, &path, &priority);
            
            if (!path) {
                log_error(master->logger, "[MASTER] Error deserializando NEW_QUERY");
                return;
            }
            
            // Generar ID para la nueva query
            uint64_t query_id = generar_id_query(master);
            
            // En modo FIFO, la prioridad se asigna automáticamente según orden de llegada
            // (usamos el ID de query como prioridad, así las primeras tienen menor número = mayor prioridad)
            if (master->config->algoritmo_planificacion == ALGORITHM_FIFO) {
                priority = (int)query_id;
                log_debug(master->logger, "[MASTER] Modo FIFO: prioridad asignada automáticamente = %d (orden de llegada)", priority);
            } else {
                // En modo PRIORIDADES, validar que la prioridad sea mayor o igual a 0 (según enunciado)
                if (priority < 0) {
                    log_error(master->logger, "[MASTER] Prioridad inválida (%d). Debe ser >= 0", priority);
                    free(path);
                    return;
                }
            }
            
            // Crear la query
            query_t* query = query_crear(query_id, path, priority, client_socket);
            if (!query) {
                log_error(master->logger, "[MASTER] Error creando query");
                free(path);
                return;
            }
            
            // Crear QueryControl
            query_control_t* qc = query_control_crear(client_socket);
            qc->connected_query_id = query_id;
            
            // Agregar QC a la lista y obtener worker_count de forma sincronizada
            pthread_mutex_lock(&master->main_mutex);
            list_add(master->query_controls, qc);
            int current_worker_count = master->worker_count;
            pthread_mutex_unlock(&master->main_mutex);
            
            // Log de conexión (usar worker_count capturado)
            log_query_control_connect(master->logger, path, priority, query_id, current_worker_count);
            
            // Enviar ACK usando utils
            int ack_size = 0;
            void* ack_payload = serializar_ack_con_id(query_id, &ack_size);
            if (enviar_paquete(client_socket, NEW_QUERY_ACK, ack_payload, ack_size) != 0) {
                log_error(master->logger, "[MASTER] Error enviando NEW_QUERY_ACK al Query Control");
                // La conexión falló, limpiar la query creada
                pthread_mutex_lock(&master->main_mutex);
                list_remove_element(master->query_controls, qc);
                pthread_mutex_unlock(&master->main_mutex);
                query_control_destruir(qc);
                query_destruir(query);
                free(ack_payload);
                free(path);
                return;
            }
            free(ack_payload);
            
            // Intentar planificar la query
            planificar_query(master, query);
            
            free(path);
            break;
        }
        
        default:
            log_warning(master->logger, "[MASTER] Mensaje desconocido de Query Control: %d", codigo);
            break;
    }
}

// ========== MANEJADORES DE MENSAJES DE WORKER ==========

void manejar_mensaje_worker(master_t* master, int client_socket, op_code codigo, void* payload, int size) {
    if (!master) return;

    switch (codigo) {
        case HANDSHAKE_WORKER: {
            char worker_id[MAX_WORKER_ID_SIZE];

           if (payload != NULL && size >= sizeof(int)) {
                int id_recibido = 0;
                memcpy(&id_recibido, payload, sizeof(int));
                // Formatear el nombre como pide el enunciado o logs
                snprintf(worker_id, MAX_WORKER_ID_SIZE, "WORKER_%d", id_recibido);
            } else {
                log_error(master->logger, "[MASTER] Worker intentó conectar sin enviar ID en payload");
                return; 
            }

            // Crear worker con el ID leido
            worker_t* worker = worker_crear(worker_id, client_socket);
            if (!worker) {
                log_error(master->logger, "[MASTER] Error creando worker %s", worker_id);
                return;
            }
            
            pthread_mutex_lock(&master->main_mutex);
            
            // Verificar que no exista ya un worker con ese socket
            worker_t* existing = NULL;
            for (int i = 0; i < list_size(master->workers); i++) {
                worker_t* w = (worker_t*)list_get(master->workers, i);
                if (w && strcmp(w->id, worker_id) == 0) { // Comparar por ID, no solo por socket
                    existing = w;
                    break;
                }
            }
            
            if (existing) {
                log_warning(master->logger, "[MASTER] Worker %s reconectado. Eliminando sesión anterior.", worker_id);
                list_remove_element(master->workers, existing);
                master->worker_count--;
                // Cerrar socket anterior y liberar
                close(existing->socket); 
                worker_destruir(existing);
            }
            
            // Agregar nuevo worker
            list_add(master->workers, worker);
            master->worker_count++;
            int current_worker_count = master->worker_count;
            
            pthread_mutex_unlock(&master->main_mutex);
            
            // Enviar HANDSHAKE_OK
            if (enviar_paquete(client_socket, HANDSHAKE_OK, NULL, 0) != 0) {
                log_error(master->logger, "[MASTER] Error enviando HANDSHAKE_OK al worker %s", worker_id);
                // Cleanup si falla envío
                pthread_mutex_lock(&master->main_mutex);
                list_remove_element(master->workers, worker);
                master->worker_count--;
                pthread_mutex_unlock(&master->main_mutex);
                worker_destruir(worker);
                return;
            }
            
            log_worker_connect(master->logger, worker_id, current_worker_count);
            
            // Intentar asignar trabajo
            planificar_siguiente_query(master);
            
            break;
        }
        
        case PREEMPTION_ACK: {
            // Buscar el worker
            worker_t* worker = buscar_worker_por_socket(master, client_socket);
            if (!worker) {
                log_warning(master->logger, "[MASTER] Respuesta de desalojo de worker desconocido");
                return;
            }
            
            // Validar payload antes de deserializar
            if (!payload || size < sizeof(uint32_t)) {
                log_error(master->logger, "[MASTER] Payload inválido en PREEMPTION_ACK del worker %s (size=%d)", 
                         worker->id, size);
                return;
            }
            
            // Deserializar el PC del payload
            uint32_t pc = 0;
            memcpy(&pc, payload, sizeof(uint32_t));
            
            // Completar el proceso de desalojo usando la nueva función
            completar_desalojo_worker(master, worker, pc);
            
            break;
        }
        
        case CANCEL_QUERY: {
            // Este es en realidad la RESPUESTA del worker tras una cancelación
            // El worker envía el mismo formato que PREEMPTION_ACK (PC)
            
            // Buscar el worker
            worker_t* worker = buscar_worker_por_socket(master, client_socket);
            if (!worker) {
                log_warning(master->logger, "[MASTER] Respuesta de cancelación de worker desconocido");
                return;
            }
            
            // Validar payload antes de deserializar
            if (!payload || size < sizeof(uint32_t)) {
                log_error(master->logger, "[MASTER] Payload inválido en CANCEL_QUERY del worker %s (size=%d)", 
                         worker->id, size);
                return;
            }
            
            // Deserializar el PC del payload
            uint32_t pc = 0;
            memcpy(&pc, payload, sizeof(uint32_t));
            
            // Completar el proceso de cancelación
            completar_cancelacion_query(master, worker, pc);
            
            break;
        }
        
        case QUERY_FINISHED: {
            // Buscar el worker
            worker_t* worker = buscar_worker_por_socket(master, client_socket);
            if (!worker) {
                log_warning(master->logger, "[MASTER] Query finalizada de worker desconocido");
                return;
            }
            
            // Deserializar query_id
            uint64_t query_id = 0;
            if (payload && size >= sizeof(uint64_t)) {
                deserializar_ack_con_id(payload, &query_id);
            }
            
            // Buscar y finalizar la query
            pthread_mutex_lock(&master->main_mutex);
            query_t* query = (query_t*)dictionary_get(master->exec_map, worker->id);
            
            // Verificar si habia una query esperando en pending_preemptions
            // (esto ocurre cuando el Worker no pudo recibir PREEMPT_QUERY porque estaba bloqueado)
            query_t* pending_query = (query_t*)dictionary_get(master->pending_preemptions, worker->id);
            bool was_preempting = (worker->status == WORKER_PREEMPTING);
            
            if (query && query->id == query_id) {
                // Log de finalización
                log_query_finished(master->logger, query->id, worker->id);
                
                // Notificar al Query Control usando utils
                int finish_size = 0;
                void* finish_payload = serializar_ack_con_id(query->id, &finish_size);
                if (enviar_paquete(query->qc_socket, QUERY_FINISHED, finish_payload, finish_size) != 0) {
                    log_warning(master->logger, "[MASTER] Error enviando QUERY_FINISHED al Query Control (query %lu)", query->id);
                    // Query Control ya se desconectó, pero la query terminó correctamente
                }
                free(finish_payload);
                
                // Cleanup
                dictionary_remove(master->exec_map, worker->id);
                worker->status = WORKER_IDLE;
                worker->current_query_id = 0;
                
                query_destruir(query);
            }
            
            // Si habia una preemption pendiente, asignar DIRECTAMENTE al worker
            // (no pasar por ready_queue para evitar problemas de planificacion)
            if (pending_query && was_preempting) {
                log_info(master->logger, "[MASTER] Query %lu (prioridad %d) estaba esperando preemption, asignando a Worker %s",
                         pending_query->id, pending_query->priority, worker->id);
                
                // Remover de pending_preemptions
                dictionary_remove(master->pending_preemptions, worker->id);
                
                // Asignar directamente al worker
                pending_query->state = QUERY_EXEC;
                strncpy(pending_query->worker_id, worker->id, MAX_WORKER_ID_SIZE - 1);
                pending_query->worker_id[MAX_WORKER_ID_SIZE - 1] = '\0';
                
                worker->status = WORKER_BUSY;
                worker->current_query_id = pending_query->id;
                
                dictionary_put(master->exec_map, worker->id, pending_query);
                
                // Guardar datos para enviar fuera del mutex
                int worker_socket = worker->socket;
                uint64_t q_id = pending_query->id;
                int q_priority = pending_query->priority;
                char* q_path = strdup(pending_query->path_query);
                uint32_t q_pc = pending_query->pc;
                
                pthread_mutex_unlock(&master->main_mutex);
                
                // Enviar EXECUTE_QUERY al worker
                int execute_size = 0;
                void* execute_payload = serializar_execute_query(q_id, q_path, q_pc, &execute_size);
                
                if (execute_payload && enviar_paquete(worker_socket, EXECUTE_QUERY, execute_payload, execute_size) == 0) {
                    log_info(master->logger, "## Query %lu (prioridad %d) ejecutandose en Worker %s (tras preemption fallida)", 
                             q_id, q_priority, worker->id);
                } else {
                    log_error(master->logger, "[MASTER] Error enviando EXECUTE_QUERY tras preemption fallida");
                    // Revertir y mover a ready_queue
                    pthread_mutex_lock(&master->main_mutex);
                    pending_query->state = QUERY_READY;
                    queue_push(master->ready_queue, pending_query);
                    dictionary_remove(master->exec_map, worker->id);
                    worker->status = WORKER_IDLE;
                    worker->current_query_id = 0;
                    pthread_mutex_unlock(&master->main_mutex);
                }
                
                if (execute_payload) free(execute_payload);
                free(q_path);
            } else {
                pthread_mutex_unlock(&master->main_mutex);
                
                // Intentar asignar nueva query de ready_queue
                planificar_siguiente_query(master);
            }
            
            break;
        }
        
        case READ_RESULT: {
            // Buscar el worker
            worker_t* worker = buscar_worker_por_socket(master, client_socket);
            if (!worker) {
                log_warning(master->logger, "[MASTER] Read result de worker desconocido");
                return;
            }
            
            // Deserializar read result
            uint64_t query_id = 0;
            char* origen = NULL;
            char* contenido = NULL;
            deserializar_read_result(payload, &query_id, &origen, &contenido);
            
            // Buscar la query
            pthread_mutex_lock(&master->main_mutex);
            query_t* query = (query_t*)dictionary_get(master->exec_map, worker->id);
            if (query && query->id == query_id) {
                // Log del envío
                log_read_sent_to_qc(master->logger, query->id, worker->id);
                
                // Reenviar al Query Control usando utils
                int read_size = 0;
                void* read_payload = serializar_read_result(query->id, origen, contenido, &read_size);
                if (enviar_paquete(query->qc_socket, READ_RESULT, read_payload, read_size) != 0) {
                    log_warning(master->logger, "[MASTER] Error reenviando READ_RESULT al Query Control (query %lu)", query->id);
                    // Query Control posiblemente desconectado, pero continuar ejecución
                }
                free(read_payload);
            }
            pthread_mutex_unlock(&master->main_mutex);
            
            // Liberar memoria
            if (origen) free(origen);
            if (contenido) free(contenido);
            
            break;
        }
        
        default:
            log_warning(master->logger, "[MASTER] Mensaje desconocido de Worker: %d", codigo);
            break;
    }
}

// ========== FUNCIONES AUXILIARES ==========

worker_t* buscar_worker_por_socket(master_t* master, int socket) {
    if (!master) return NULL;

    pthread_mutex_lock(&master->main_mutex);
    
    for (int i = 0; i < list_size(master->workers); i++) {
        worker_t* worker = (worker_t*)list_get(master->workers, i);
        if (worker && worker->socket == socket) {
            pthread_mutex_unlock(&master->main_mutex);
            return worker;
        }
    }
    
    pthread_mutex_unlock(&master->main_mutex);
    return NULL;
}
