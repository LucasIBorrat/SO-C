#include "master.h"

// ========== FUNCIONES DE QUERY ==========

query_t* query_crear(uint64_t id, char* path, int priority, int qc_socket) {
    query_t* query = malloc(sizeof(query_t));
    if (!query) return NULL;

    query->id = id;
    strncpy(query->path_query, path, MAX_PATH_SIZE - 1);
    query->path_query[MAX_PATH_SIZE - 1] = '\0';
    query->priority = priority;
    query->priority_original = priority;
    query->state = QUERY_NEW;
    query->pc = 0;
    query->qc_socket = qc_socket;
    memset(query->worker_id, 0, MAX_WORKER_ID_SIZE);

    return query;
}

void query_destruir(query_t* query) {
    if (!query) return;
    free(query);
}

// ========== FUNCIONES DE WORKER ==========

worker_t* worker_crear(char* id, int socket) {
    worker_t* worker = malloc(sizeof(worker_t));
    if (!worker) return NULL;

    strncpy(worker->id, id, MAX_WORKER_ID_SIZE - 1);
    worker->id[MAX_WORKER_ID_SIZE - 1] = '\0';
    worker->socket = socket;
    worker->status = WORKER_IDLE;
    worker->current_query_id = 0;

    return worker;
}

void worker_destruir(worker_t* worker) {
    if (!worker) return;
    
    // NOTA: El socket NO se cierra aquí porque es manejado por el hilo de conexión
    // Cerrar el socket aquí causaría un doble cierre y podría afectar otras conexiones
    
    free(worker);
}

worker_t* buscar_worker_por_id(master_t* master, char* worker_id) {
    if (!master || !worker_id) return NULL;

    pthread_mutex_lock(&master->main_mutex);
    
    for (int i = 0; i < list_size(master->workers); i++) {
        worker_t* worker = (worker_t*)list_get(master->workers, i);
        if (worker && strcmp(worker->id, worker_id) == 0) {
            pthread_mutex_unlock(&master->main_mutex);
            return worker;
        }
    }
    
    pthread_mutex_unlock(&master->main_mutex);
    return NULL;
}

/**
 * @brief Busca un worker en estado IDLE
 * 
 * ⚠️ PRECONDICIÓN CRÍTICA: Esta función DEBE ser llamada con master->main_mutex YA TOMADO.
 * NO toma ni libera el mutex internamente para evitar deadlocks.
 * 
 * @param master Master principal
 * @return worker_t* Puntero al worker IDLE encontrado, o NULL si no hay ninguno disponible
 * 
 * @note Esta función accede a master->workers sin sincronización adicional,
 *       asumiendo que el caller ya tiene el mutex tomado.
 */
worker_t* buscar_worker_libre(master_t* master) {
    if (!master) return NULL;

    // Iterar sobre workers sin tomar mutex (el caller debe tenerlo tomado)
    for (int i = 0; i < list_size(master->workers); i++) {
        worker_t* worker = (worker_t*)list_get(master->workers, i);
        if (worker && worker->status == WORKER_IDLE) {
            return worker;
        }
    }
    
    return NULL;
}

/**
 * @brief Cuenta la cantidad de workers en estado IDLE
 * 
 * ⚠️ PRECONDICIÓN CRÍTICA: Esta función DEBE ser llamada con master->main_mutex YA TOMADO.
 * 
 * @param master Master principal
 * @return int Cantidad de workers IDLE disponibles
 */
int contar_workers_disponibles(master_t* master) {
    if (!master) return 0;

    int count = 0;
    for (int i = 0; i < list_size(master->workers); i++) {
        worker_t* worker = (worker_t*)list_get(master->workers, i);
        if (worker && worker->status == WORKER_IDLE) {
            count++;
        }
    }
    
    return count;
}

/**
 * @brief Obtiene la cantidad total de workers conectados
 * 
 * ⚠️ PRECONDICIÓN CRÍTICA: Esta función DEBE ser llamada con master->main_mutex YA TOMADO.
 * 
 * @param master Master principal
 * @return int Total de workers en cualquier estado
 */
int contar_workers_totales(master_t* master) {
    if (!master) return 0;
    return list_size(master->workers);
}

// ========== FUNCIONES DE QUERY CONTROL ==========

query_control_t* query_control_crear(int socket) {
    query_control_t* qc = malloc(sizeof(query_control_t));
    if (!qc) return NULL;

    qc->socket = socket;
    qc->connected_query_id = 0;

    return qc;
}

void query_control_destruir(query_control_t* qc) {
    if (!qc) return;
    
    // NOTA: El socket NO se cierra aquí porque es manejado por el hilo de conexión
    // Cerrar el socket aquí causaría un doble cierre y podría afectar otras conexiones
    
    free(qc);
}

query_control_t* buscar_query_control_por_socket(master_t* master, int socket) {
    if (!master) return NULL;

    pthread_mutex_lock(&master->main_mutex);
    
    for (int i = 0; i < list_size(master->query_controls); i++) {
        query_control_t* qc = (query_control_t*)list_get(master->query_controls, i);
        if (qc && qc->socket == socket) {
            pthread_mutex_unlock(&master->main_mutex);
            return qc;
        }
    }
    
    pthread_mutex_unlock(&master->main_mutex);
    return NULL;
}