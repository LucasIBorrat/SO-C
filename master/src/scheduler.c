#include "master.h"

// ========== FUNCIONES DEL PLANIFICADOR ==========

void planificador_inicializar(master_t* master) {
    if (!master) return;
    
    log_info(master->logger, "[SCHEDULER] Planificador inicializado con algoritmo %s", 
             master->config->algoritmo_planificacion == ALGORITHM_FIFO ? "FIFO" : "PRIORIDADES");
}

void planificar_query(master_t* master, query_t* query) {
    if (!master || !query) return;
    
    log_debug(master->logger, "[SCHEDULER] Intentando planificar query %lu (prioridad %d)", query->id, query->priority);
    
    pthread_mutex_lock(&master->main_mutex);
    
    // PRIMERO: Buscar worker idle (NOTA: esta función REQUIERE que el mutex esté tomado)
    worker_t* idle_worker = buscar_worker_libre(master);
    
    // Si hay un worker libre, asignar directamente sin desalojar
    if (idle_worker) {
        // Asignar directamente al worker idle
        query->state = QUERY_EXEC;
        strncpy(query->worker_id, idle_worker->id, MAX_WORKER_ID_SIZE - 1);
        query->worker_id[MAX_WORKER_ID_SIZE - 1] = '\0';
        
        idle_worker->status = WORKER_BUSY;
        idle_worker->current_query_id = query->id;
        
        dictionary_put(master->exec_map, idle_worker->id, query);
        
        // FIX: Guardar valores necesarios ANTES de liberar mutex para evitar data race
        int worker_socket = idle_worker->socket;
        char worker_id[MAX_WORKER_ID_SIZE];
        strncpy(worker_id, idle_worker->id, MAX_WORKER_ID_SIZE - 1);
        worker_id[MAX_WORKER_ID_SIZE - 1] = '\0';
        
        // Contar workers disponibles ANTES de liberar mutex
        int workers_disponibles_ahora = contar_workers_disponibles(master);
        int total_workers = contar_workers_totales(master);
        
        // Liberar mutex ANTES de operación de red para evitar bloqueos
        pthread_mutex_unlock(&master->main_mutex);
        
        log_debug(master->logger, "[SCHEDULER] Query %lu asignada DIRECTAMENTE a Worker %s (IDLE). Disponibles: %d/%d", 
                  query->id, worker_id, workers_disponibles_ahora, total_workers);
        
        // Enviar mensaje al worker usando utils
        int execute_size = 0;
        void* execute_payload = serializar_execute_query(query->id, query->path_query, query->pc, &execute_size);
        
        if (!execute_payload) {
            log_error(master->logger, "Error: No se pudo serializar EXECUTE_QUERY para query %lu", query->id);
            // Revertir cambios y devolver query a ready_queue
            pthread_mutex_lock(&master->main_mutex);
            worker_t* worker_check = buscar_worker_por_id(master, worker_id);
            if (worker_check) {
                worker_check->status = WORKER_IDLE;
                worker_check->current_query_id = 0;
            }
            query->state = QUERY_READY;
            queue_push(master->ready_queue, query);
            dictionary_remove(master->exec_map, worker_id);
            pthread_mutex_unlock(&master->main_mutex);
            return;
        }
        
        if (enviar_paquete(worker_socket, EXECUTE_QUERY, execute_payload, execute_size) == 0) {
            log_query_sent_to_worker(master->logger, query->id, query->priority, worker_id);
        } else {
            log_error(master->logger, "[SCHEDULER] Error enviando EXECUTE_QUERY al worker %s", worker_id);
            pthread_mutex_lock(&master->main_mutex);
            worker_t* worker_check = buscar_worker_por_id(master, worker_id);
            if (worker_check) {
                worker_check->status = WORKER_IDLE;
                worker_check->current_query_id = 0;
            }
            query->state = QUERY_READY;
            queue_push(master->ready_queue, query);
            dictionary_remove(master->exec_map, worker_id);
            pthread_mutex_unlock(&master->main_mutex);
        }
        
        free(execute_payload);
        return;
    }
    
    // SEGUNDO: Si no hay workers libres, intentar desalojar (solo en algoritmo PRIORIDADES)
    if (master->config->algoritmo_planificacion == ALGORITHM_PRIORIDADES) {
        worker_t* worker_to_preempt = buscar_worker_con_menor_prioridad_directo(master, query->priority);
        if (worker_to_preempt) {
            log_debug(master->logger, "[SCHEDULER] No hay workers libres. Query %lu (prioridad %d) desalojara a query en worker %s", 
                     query->id, query->priority, worker_to_preempt->id);
            // Desalojar la query con menor prioridad
            desalojar_query_de_worker_directo(master, worker_to_preempt, query);
            pthread_mutex_unlock(&master->main_mutex);
            return;
        }
    }
    
    // TERCERO: No hay workers libres ni queries para desalojar, agregar a ready queue
    query->state = QUERY_READY;
    queue_push(master->ready_queue, query);
    
    int workers_ocupados = contar_workers_totales(master) - contar_workers_disponibles(master);
    int total_workers = contar_workers_totales(master);
    log_debug(master->logger, "[SCHEDULER] Query %lu agregada a ready_queue. Workers ocupados: %d/%d", 
              query->id, workers_ocupados, total_workers);
    
    pthread_mutex_unlock(&master->main_mutex);
}

void planificar_siguiente_query(master_t* master) {
    if (!master) return;
    
    pthread_mutex_lock(&master->main_mutex);
    
    // Verificar si hay queries en espera
    if (queue_is_empty(master->ready_queue)) {
        pthread_mutex_unlock(&master->main_mutex);
        return;
    }
    
    // Buscar worker idle directamente
    worker_t* idle_worker = NULL;
    for (int i = 0; i < list_size(master->workers); i++) {
        worker_t* worker = (worker_t*)list_get(master->workers, i);
        if (worker && worker->status == WORKER_IDLE) {
            idle_worker = worker;
            break;
        }
    }
    
    if (!idle_worker) {
        int workers_disponibles = contar_workers_disponibles(master);
        int total_workers = contar_workers_totales(master);
        log_debug(master->logger, "[SCHEDULER] No hay workers IDLE disponibles. Ocupados: %d / Total: %d", 
                  total_workers - workers_disponibles, total_workers);
        pthread_mutex_unlock(&master->main_mutex);
        return;
    }
    
    // Obtener próxima query según algoritmo
    query_t* next_query = NULL;
    
    if (master->config->algoritmo_planificacion == ALGORITHM_FIFO) {
        // FIFO: tomar el primero
        next_query = (query_t*)queue_pop(master->ready_queue);
    } else {
        // PRIORIDADES: buscar el de mayor prioridad (número menor)
        next_query = obtener_query_mayor_prioridad(master);
    }
    
    if (next_query) {
        // Actualizar estados (ya tenemos el mutex)
        next_query->state = QUERY_EXEC;
        strncpy(next_query->worker_id, idle_worker->id, MAX_WORKER_ID_SIZE - 1);
        next_query->worker_id[MAX_WORKER_ID_SIZE - 1] = '\0';
        
        idle_worker->status = WORKER_BUSY;
        idle_worker->current_query_id = next_query->id;
        
        // Agregar a exec_map
        dictionary_put(master->exec_map, idle_worker->id, next_query);
        
        // FIX: Guardar valores necesarios ANTES de liberar mutex para evitar data race
        int worker_socket = idle_worker->socket;
        char worker_id[MAX_WORKER_ID_SIZE];
        strncpy(worker_id, idle_worker->id, MAX_WORKER_ID_SIZE - 1);
        worker_id[MAX_WORKER_ID_SIZE - 1] = '\0';
        
        // Contar workers disponibles ANTES de liberar mutex (para logging más preciso)
        int workers_disponibles_ahora = contar_workers_disponibles(master);
        int total_workers = contar_workers_totales(master);
        
        pthread_mutex_unlock(&master->main_mutex);
        
        // Log DETALLADO de asignación
        log_debug(master->logger, "[SCHEDULER] Asignando Query %lu (prioridad %d) a Worker %s. Workers disponibles: %d/%d", 
                  next_query->id, next_query->priority, worker_id, workers_disponibles_ahora, total_workers);
        
        // Enviar mensaje EXECUTE_QUERY al worker usando utils
        int execute_size = 0;
        void* execute_payload = serializar_execute_query(next_query->id, next_query->path_query, next_query->pc, &execute_size);
        
        // FIX Bug 1: Verificar si serialización falló (mutex YA fue liberado, no intentar liberarlo de nuevo)
        if (!execute_payload) {
            log_error(master->logger, "Error: No se pudo serializar EXECUTE_QUERY para query %lu", next_query->id);
            // Revertir cambios y devolver query a ready_queue
            pthread_mutex_lock(&master->main_mutex);
            // Re-buscar worker por si fue modificado/eliminado
            worker_t* worker_check = buscar_worker_por_id(master, worker_id);
            if (worker_check) {
                worker_check->status = WORKER_IDLE;
                worker_check->current_query_id = 0;
            }
            next_query->state = QUERY_READY;
            queue_push(master->ready_queue, next_query);
            dictionary_remove(master->exec_map, worker_id);
            pthread_mutex_unlock(&master->main_mutex);
            return;
        }
        
        if (enviar_paquete(worker_socket, EXECUTE_QUERY, execute_payload, execute_size) == 0) {
            log_query_sent_to_worker(master->logger, next_query->id, next_query->priority, worker_id);
        } else {
            log_error(master->logger, "[SCHEDULER] Error enviando EXECUTE_QUERY al worker %s", worker_id);
            // Revertir cambios y devolver query a ready_queue
            pthread_mutex_lock(&master->main_mutex);
            // Re-buscar worker por si fue modificado/eliminado
            worker_t* worker_check = buscar_worker_por_id(master, worker_id);
            if (worker_check) {
                worker_check->status = WORKER_IDLE;
                worker_check->current_query_id = 0;
            }
            next_query->state = QUERY_READY;
            queue_push(master->ready_queue, next_query);
            dictionary_remove(master->exec_map, worker_id);
            pthread_mutex_unlock(&master->main_mutex);
        }
        
        free(execute_payload);
    } else {
        pthread_mutex_unlock(&master->main_mutex);
    }
}

/**
 * @brief Asigna una query a un worker y envía el mensaje EXECUTE_QUERY
 * 
 * NOTA IMPORTANTE: Esta función SÍ toma el mutex internamente para garantizar
 * atomicidad de las operaciones. No debe ser llamada con el mutex ya tomado
 * para evitar deadlocks.
 * 
 * @param master Master principal
 * @param query Query a asignar
 * @param worker Worker al que se asignará la query
 */
void asignar_query_a_worker(master_t* master, query_t* query, worker_t* worker) {
    if (!master || !query || !worker) return;
    
    pthread_mutex_lock(&master->main_mutex);
    
    // Actualizar estados (con mutex tomado para consistencia)
    query->state = QUERY_EXEC;
    strncpy(query->worker_id, worker->id, MAX_WORKER_ID_SIZE - 1);
    query->worker_id[MAX_WORKER_ID_SIZE - 1] = '\0';
    
    worker->status = WORKER_BUSY;
    worker->current_query_id = query->id;
    
    // Agregar a exec_map
    dictionary_put(master->exec_map, worker->id, query);
    
    // FIX: Guardar valores necesarios ANTES de liberar mutex para evitar data race
    int worker_socket = worker->socket;
    char worker_id[MAX_WORKER_ID_SIZE];
    strncpy(worker_id, worker->id, MAX_WORKER_ID_SIZE - 1);
    worker_id[MAX_WORKER_ID_SIZE - 1] = '\0';
    
    // Liberar mutex ANTES de operación de red
    pthread_mutex_unlock(&master->main_mutex);
    
    // Enviar mensaje EXECUTE_QUERY al worker usando utils
    int execute_size = 0;
    void* execute_payload = serializar_execute_query(query->id, query->path_query, query->pc, &execute_size);
    
    // FIX Bug 1: Verificar si serialización falló (mutex YA fue liberado, no intentar liberarlo de nuevo)
    if (!execute_payload) {
        log_error(master->logger, "Error: No se pudo serializar EXECUTE_QUERY para query %lu", query->id);
        // Revertir cambios y devolver query a ready_queue
        pthread_mutex_lock(&master->main_mutex);
        // Re-buscar worker por si fue modificado/eliminado
        worker_t* worker_check = buscar_worker_por_id(master, worker_id);
        if (worker_check) {
            worker_check->status = WORKER_IDLE;
            worker_check->current_query_id = 0;
        }
        query->state = QUERY_READY;
        queue_push(master->ready_queue, query);
        dictionary_remove(master->exec_map, worker_id);
        pthread_mutex_unlock(&master->main_mutex);
        return;
    }
    
    if (enviar_paquete(worker_socket, EXECUTE_QUERY, execute_payload, execute_size) == 0) {
        log_query_sent_to_worker(master->logger, query->id, query->priority, worker_id);
    } else {
        log_error(master->logger, "[SCHEDULER] Error enviando EXECUTE_QUERY al worker %s", worker_id);
        // Revertir cambios y devolver query a ready_queue
        pthread_mutex_lock(&master->main_mutex);
        // Re-buscar worker por si fue modificado/eliminado
        worker_t* worker_check = buscar_worker_por_id(master, worker_id);
        if (worker_check) {
            worker_check->status = WORKER_IDLE;
            worker_check->current_query_id = 0;
        }
        query->state = QUERY_READY;
        queue_push(master->ready_queue, query);
        dictionary_remove(master->exec_map, worker_id);
        pthread_mutex_unlock(&master->main_mutex);
    }
    
    free(execute_payload);
}

query_t* obtener_query_mayor_prioridad(master_t* master) {
    if (!master || queue_is_empty(master->ready_queue)) return NULL;
    
    // Implementación simple: convertir queue a lista, ordenar, y devolver el primero
    // TODO: Implementar con heap para mayor eficiencia
    
    t_list* temp_list = list_create();
    
    // Mover todas las queries a la lista temporal
    while (!queue_is_empty(master->ready_queue)) {
        query_t* query = (query_t*)queue_pop(master->ready_queue);
        list_add(temp_list, query);
    }
    
    if (list_is_empty(temp_list)) {
        list_destroy(temp_list);
        return NULL;
    }
    
    // Buscar la query con mayor prioridad (menor número)
    query_t* highest_priority = (query_t*)list_get(temp_list, 0);
    
    // FIX Bug 3: Validar que highest_priority no sea NULL antes de usarla
    if (!highest_priority) {
        list_destroy(temp_list);
        return NULL;
    }
    
    int best_index = 0;
    
    for (int i = 1; i < list_size(temp_list); i++) {
        query_t* current = (query_t*)list_get(temp_list, i);
        // FIX Bug 2: Validar que current no sea NULL
        if (current && current->priority < highest_priority->priority) {
            highest_priority = current;
            best_index = i;
        }
    }
    
    // FIX Bug 2: Validar que highest_priority sea válida antes de remover
    if (!highest_priority) {
        // Devolver todas las queries a la queue
        for (int i = 0; i < list_size(temp_list); i++) {
            query_t* query = (query_t*)list_get(temp_list, i);
            if (query) queue_push(master->ready_queue, query);
        }
        list_destroy(temp_list);
        return NULL;
    }
    
    // Remover la elegida de la lista
    void* removed = list_remove(temp_list, best_index);
    
    // FIX Bug 2: Validar que list_remove fue exitoso
    if (removed != highest_priority) {
        log_warning(master->logger, "Advertencia: list_remove no retornó el elemento esperado");
    }
    
    // Devolver el resto a la queue
    for (int i = 0; i < list_size(temp_list); i++) {
        query_t* query = (query_t*)list_get(temp_list, i);
        if (query) queue_push(master->ready_queue, query);
    }
    
    list_destroy(temp_list);
    return highest_priority;
}

// ========== HILOS DEL PLANIFICADOR ==========
// Nota: El hilo de planificador fue eliminado - la planificación se hace directamente
// cuando llegan requests para simplificar y evitar problemas de concurrencia

void* funcion_hilo_aging(void* arg) {
    master_t* master = (master_t*)arg;
    
    // Desactivar aging en modo FIFO (no tiene sentido aplicar aging cuando no se usa prioridad)
    if (master->config->algoritmo_planificacion == ALGORITHM_FIFO) {
        log_info(master->logger, "[AGING] Aging deshabilitado (algoritmo FIFO no usa prioridades)");
        return NULL;
    }
    
    if (master->config->tiempo_aging <= 0) {
        log_info(master->logger, "[AGING] Aging deshabilitado");
        return NULL;
    }
    
    log_info(master->logger, "[AGING] Hilo de aging iniciado (cada %d ms)", master->config->tiempo_aging);
    
    while (master->running) {
        // Esperar el tiempo configurado
        sleep_ms(master->config->tiempo_aging);
        
        if (!master->running) break;
        
        // Aplicar aging a queries en ready_queue
        aplicar_aging(master);
    }
    
    log_info(master->logger, "[AGING] Hilo de aging terminado");
    return NULL;
}

void aplicar_aging(master_t* master) {
    if (!master) return;
    
    pthread_mutex_lock(&master->main_mutex);
    
    if (queue_is_empty(master->ready_queue)) {
        pthread_mutex_unlock(&master->main_mutex);
        return;
    }
    
    // Convertir queue a lista para procesar
    t_list* temp_list = list_create();
    
    while (!queue_is_empty(master->ready_queue)) {
        query_t* query = (query_t*)queue_pop(master->ready_queue);
        list_add(temp_list, query);
    }
    
    // Aplicar aging
    for (int i = 0; i < list_size(temp_list); i++) {
        query_t* query = (query_t*)list_get(temp_list, i);
        
        if (query->priority > 0) {
            int old_priority = query->priority;
            query->priority--;
            
            log_priority_change(master->logger, query->id, old_priority, query->priority);
        }
    }
    
    // Devolver queries a la queue
    for (int i = 0; i < list_size(temp_list); i++) {
        query_t* query = (query_t*)list_get(temp_list, i);
        queue_push(master->ready_queue, query);
    }
    
    list_destroy(temp_list);
    pthread_mutex_unlock(&master->main_mutex);
}

// ========== FUNCIONES DE DESALOJO ==========

worker_t* buscar_worker_con_menor_prioridad_directo(master_t* master, int new_priority) {
    if (!master) return NULL;
    
    worker_t* lowest_priority_worker = NULL;
    int lowest_priority = -1; // -1 indica que aún no se ha encontrado ningún candidato
    
    for (int i = 0; i < list_size(master->workers); i++) {
        worker_t* worker = (worker_t*)list_get(master->workers, i);
        // Considerar workers BUSY y PREEMPTING (pueden tener queries en ejecucion)
        if (worker && (worker->status == WORKER_BUSY || worker->status == WORKER_PREEMPTING)) {
            // Buscar la query que está ejecutando este worker
            query_t* running_query = (query_t*)dictionary_get(master->exec_map, worker->id);
            
            if (running_query && running_query->priority > new_priority) {
                // Esta query tiene menor prioridad (número mayor) que la nueva
                // Queremos desalojar la query con el MAYOR número de prioridad (la menos importante)
                if (lowest_priority == -1 || running_query->priority > lowest_priority) {
                    lowest_priority = running_query->priority;
                    lowest_priority_worker = worker;
                }
            }
        }
    }
    
    return lowest_priority_worker;
}

void desalojar_query_de_worker_directo(master_t* master, worker_t* worker, query_t* new_query) {
    if (!master || !worker || !new_query) return;
    
    // Si el worker ya esta en PREEMPTING, verificar si la nueva query tiene mayor prioridad
    // que la que ya esta esperando
    if (worker->status == WORKER_PREEMPTING) {
        query_t* waiting_query = (query_t*)dictionary_get(master->pending_preemptions, worker->id);
        if (waiting_query) {
            if (new_query->priority < waiting_query->priority) {
                // La nueva query tiene mayor prioridad, reemplazarla
                log_info(master->logger, "[SCHEDULER] Query %lu (P%d) reemplaza a Query %lu (P%d) en pending_preemptions de Worker %s",
                         new_query->id, new_query->priority, waiting_query->id, waiting_query->priority, worker->id);
                
                // Mover la query que estaba esperando a ready_queue
                waiting_query->state = QUERY_READY;
                queue_push(master->ready_queue, waiting_query);
                
                // Poner la nueva query en pending_preemptions
                dictionary_put(master->pending_preemptions, worker->id, new_query);
            } else {
                // La nueva query tiene menor o igual prioridad, agregarla a ready_queue
                log_debug(master->logger, "[SCHEDULER] Query %lu (P%d) va a ready_queue porque Query %lu (P%d) ya espera en Worker %s",
                          new_query->id, new_query->priority, waiting_query->id, waiting_query->priority, worker->id);
                new_query->state = QUERY_READY;
                queue_push(master->ready_queue, new_query);
            }
            return;
        }
    }
    
    // Obtener la query que está siendo desalojada (ya tenemos el mutex)
    query_t* preempted_query = (query_t*)dictionary_get(master->exec_map, worker->id);
    if (!preempted_query) {
        // Si no hay query en ejecución, asignar directamente
        // Actualizar estados directamente (ya tenemos el mutex)
        new_query->state = QUERY_EXEC;
        strncpy(new_query->worker_id, worker->id, MAX_WORKER_ID_SIZE - 1);
        new_query->worker_id[MAX_WORKER_ID_SIZE - 1] = '\0';
        worker->status = WORKER_BUSY;
        worker->current_query_id = new_query->id;
        dictionary_put(master->exec_map, worker->id, new_query);
        
        // Preparar payload antes de liberar el mutex
        // Guardar datos necesarios para evitar use-after-free si el worker se desconecta
        uint64_t query_id = new_query->id;
        int query_priority = new_query->priority;
        int execute_size = 0;
        void* execute_payload = serializar_execute_query(new_query->id, new_query->path_query, new_query->pc, &execute_size);
        
        // FIX Bug 1: Verificar si serialización falló (aún tenemos el mutex, revertir cambios)
        if (!execute_payload) {
            log_error(master->logger, "Error: No se pudo serializar EXECUTE_QUERY para query %lu", new_query->id);
            // Revertir cambios
            new_query->state = QUERY_READY;
            queue_push(master->ready_queue, new_query);
            worker->status = WORKER_IDLE;
            worker->current_query_id = 0;
            dictionary_remove(master->exec_map, worker->id);
            // Mutex sigue tomado, el caller lo liberará
            return;
        }
        
        // Liberar mutex temporalmente para operación de red
        pthread_mutex_unlock(&master->main_mutex);
        
        int send_result = enviar_paquete(worker->socket, EXECUTE_QUERY, execute_payload, execute_size);
        
        // Re-tomar el mutex inmediatamente después de la operación de red
        pthread_mutex_lock(&master->main_mutex);
        
        // Verificar si la query todavía existe en exec_map (podría haber sido destruida
        // por manejar_desconexion_worker si el worker se desconectó durante el envío)
        query_t* query_check = (query_t*)dictionary_get(master->exec_map, worker->id);
        if (query_check != new_query) {
            // La query fue removida (worker desconectado), no hacer nada más
            log_warning(master->logger, "[SCHEDULER] Worker %s desconectado durante asignación de query %lu", 
                       worker->id, query_id);
            free(execute_payload);
            return;
        }
        
        if (send_result == 0) {
            log_query_sent_to_worker(master->logger, query_id, query_priority, worker->id);
        } else {
            log_error(master->logger, "[SCHEDULER] Error enviando EXECUTE_QUERY al worker %s", worker->id);
            // Revertir cambios y devolver query a ready_queue
            new_query->state = QUERY_READY;
            queue_push(master->ready_queue, new_query);
            worker->status = WORKER_IDLE;
            worker->current_query_id = 0;
            dictionary_remove(master->exec_map, worker->id);
        }
        
        free(execute_payload);
        
        // El mutex sigue tomado, el caller lo liberará
        return;
    }
    
    // Log del desalojo
    log_preemption(master->logger, preempted_query->id, preempted_query->priority, worker->id);
    
    // Marcar worker como en proceso de desalojo
    worker->status = WORKER_PREEMPTING;
    
    // Guardar la nueva query que está esperando ser asignada
    dictionary_put(master->pending_preemptions, worker->id, new_query);
    
    // Enviar mensaje de desalojo al worker usando utils
    int preempt_size = 0;
    void* preempt_payload = serializar_ack_con_id(preempted_query->id, &preempt_size);
    
    if (enviar_paquete(worker->socket, PREEMPT_QUERY, preempt_payload, preempt_size) == 0) {
        log_debug(master->logger, "[SCHEDULER] Solicitud de desalojo enviada al worker %s para query %lu", 
                 worker->id, preempted_query->id);
    } else {
        log_error(master->logger, "[SCHEDULER] Error enviando desalojo al worker %s", worker->id);
        
        // Si falla el envío, revertir estado y agregar nueva query a ready_queue
        worker->status = WORKER_BUSY;
        dictionary_remove(master->pending_preemptions, worker->id);
        new_query->state = QUERY_READY;
        queue_push(master->ready_queue, new_query);
    }
    
    free(preempt_payload);
}

void completar_desalojo_worker(master_t* master, worker_t* worker, uint32_t pc) {
    if (!master || !worker) return;
    
    pthread_mutex_lock(&master->main_mutex);
    
    // Verificar que el worker esté realmente en proceso de desalojo
    if (worker->status != WORKER_PREEMPTING) {
        log_warning(master->logger, "[SCHEDULER] Worker %s no está en proceso de desalojo", worker->id);
        pthread_mutex_unlock(&master->main_mutex);
        return;
    }
    
    // Obtener la query que estaba ejecutándose
    query_t* preempted_query = (query_t*)dictionary_get(master->exec_map, worker->id);
    if (!preempted_query) {
        log_error(master->logger, "[SCHEDULER] No se encontró query en ejecución para worker %s", worker->id);
        pthread_mutex_unlock(&master->main_mutex);
        return;
    }
    
    // Obtener la nueva query que está esperando
    query_t* new_query = (query_t*)dictionary_get(master->pending_preemptions, worker->id);
    if (!new_query) {
        log_error(master->logger, "[SCHEDULER] No se encontró query pendiente para worker %s", worker->id);
        pthread_mutex_unlock(&master->main_mutex);
        return;
    }
    
    // Actualizar query desalojada con PC real recibido del worker
    preempted_query->state = QUERY_READY;
    preempted_query->pc = pc;
    memset(preempted_query->worker_id, 0, MAX_WORKER_ID_SIZE);
    
    // Remover query desalojada de exec_map
    dictionary_remove(master->exec_map, worker->id);
    
    // Agregar query desalojada a ready_queue
    queue_push(master->ready_queue, preempted_query);
    
    // Remover nueva query de pending_preemptions
    dictionary_remove(master->pending_preemptions, worker->id);
    
    // Asignar nueva query al worker
    new_query->state = QUERY_EXEC;
    strncpy(new_query->worker_id, worker->id, MAX_WORKER_ID_SIZE - 1);
    new_query->worker_id[MAX_WORKER_ID_SIZE - 1] = '\0';
    
    worker->status = WORKER_BUSY;
    worker->current_query_id = new_query->id;
    
    // Agregar nueva query a exec_map
    dictionary_put(master->exec_map, worker->id, new_query);
    
    // FIX: Guardar valores necesarios ANTES de liberar mutex para evitar data race
    int worker_socket = worker->socket;
    char worker_id[MAX_WORKER_ID_SIZE];
    strncpy(worker_id, worker->id, MAX_WORKER_ID_SIZE - 1);
    worker_id[MAX_WORKER_ID_SIZE - 1] = '\0';
    uint64_t preempted_id = preempted_query->id;
    
    pthread_mutex_unlock(&master->main_mutex);
    
    // Enviar mensaje EXECUTE_QUERY al worker con la nueva query
    int execute_size = 0;
    void* execute_payload = serializar_execute_query(new_query->id, new_query->path_query, new_query->pc, &execute_size);
    
    // FIX Bug 1: Verificar si serialización falló (mutex YA fue liberado, no intentar liberarlo de nuevo)
    if (!execute_payload) {
        log_error(master->logger, "Error: No se pudo serializar EXECUTE_QUERY para query %lu", new_query->id);
        // Revertir cambios y devolver query a ready_queue
        pthread_mutex_lock(&master->main_mutex);
        // Re-buscar worker por si fue modificado/eliminado
        worker_t* worker_check = buscar_worker_por_id(master, worker_id);
        if (worker_check) {
            worker_check->status = WORKER_IDLE;
            worker_check->current_query_id = 0;
        }
        dictionary_remove(master->exec_map, worker_id);
        new_query->state = QUERY_READY;
        queue_push(master->ready_queue, new_query);
        pthread_mutex_unlock(&master->main_mutex);
        return;
    }
    
    if (enviar_paquete(worker_socket, EXECUTE_QUERY, execute_payload, execute_size) == 0) {
        log_query_sent_to_worker(master->logger, new_query->id, new_query->priority, worker_id);
        log_info(master->logger, "[SCHEDULER] Desalojo completado - Query %lu (PC=%u) desalojada, Query %lu asignada al Worker %s", 
                 preempted_id, pc, new_query->id, worker_id);
    } else {
        log_error(master->logger, "[SCHEDULER] Error enviando nueva query al worker %s tras desalojo", worker_id);
        
        // Si falla, revertir el estado
        pthread_mutex_lock(&master->main_mutex);
        // Re-buscar worker por si fue modificado/eliminado
        worker_t* worker_check = buscar_worker_por_id(master, worker_id);
        if (worker_check) {
            worker_check->status = WORKER_IDLE;
            worker_check->current_query_id = 0;
        }
        dictionary_remove(master->exec_map, worker_id);
        new_query->state = QUERY_READY;
        queue_push(master->ready_queue, new_query);
        pthread_mutex_unlock(&master->main_mutex);
    }
    
    free(execute_payload);
}

// ========== FUNCIONES DE CANCELACIÓN ==========

void completar_cancelacion_query(master_t* master, worker_t* worker, uint32_t pc) {
    if (!master || !worker) return;
    
    pthread_mutex_lock(&master->main_mutex);
    
    // Verificar que haya una query pendiente de cancelación
    query_t* cancelled_query = (query_t*)dictionary_get(master->pending_cancellations, worker->id);
    if (!cancelled_query) {
        log_warning(master->logger, "[SCHEDULER] No se encontró query pendiente de cancelación para worker %s", worker->id);
        pthread_mutex_unlock(&master->main_mutex);
        return;
    }
    
    // Guardar ID antes de destruir (para evitar use-after-free)
    uint64_t query_id = cancelled_query->id;
    
    // Log del desalojo por desconexión de QC
    log_desalojo_por_desconexion(master->logger, cancelled_query->id, cancelled_query->priority, worker->id);
    
    // Actualizar estado de la query a EXIT
    cancelled_query->state = QUERY_EXIT;
    cancelled_query->pc = pc;
    
    // Remover de pending_cancellations
    dictionary_remove(master->pending_cancellations, worker->id);
    
    // Remover de exec_map
    dictionary_remove(master->exec_map, worker->id);
    
    // Actualizar estado del worker
    worker->status = WORKER_IDLE;
    worker->current_query_id = 0;
    
    pthread_mutex_unlock(&master->main_mutex);
    
    // Destruir la query cancelada
    query_destruir(cancelled_query);
    
    // Intentar asignar nueva query al worker ahora disponible
    planificar_siguiente_query(master);
    
    log_info(master->logger, "[SCHEDULER] Cancelación completada - Query %lu (PC=%u) cancelada, Worker %s ahora disponible", 
             query_id, pc, worker->id);
}