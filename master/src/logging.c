#include "master.h"

// ========== FUNCIONES DE LOGGING ESPECÍFICAS DEL ENUNCIADO ==========

void log_query_control_connect(t_log* logger, char* path, int priority, uint64_t query_id, int worker_count) {
    if (!logger) return;
    
    log_info(logger, "## Se conecta un Query Control para ejecutar la Query %s con prioridad %d - Id asignado: %lu. Nivel multiprocesamiento %d",
             path, priority, query_id, worker_count);
}

void log_worker_connect(t_log* logger, char* worker_id, int worker_count) {
    if (!logger) return;
    
    log_info(logger, "## Se conecta el Worker %s - Cantidad total de Workers: %d",
             worker_id, worker_count);
}

void log_query_control_disconnect(t_log* logger, uint64_t query_id, int priority, int worker_count) {
    if (!logger) return;
    
    log_info(logger, "## Se desconecta un Query Control. Se finaliza la Query %lu con prioridad %d. Nivel multiprocesamiento %d",
             query_id, priority, worker_count);
}

void log_worker_disconnect(t_log* logger, char* worker_id, uint64_t query_id, int worker_count) {
    if (!logger) return;
    
    log_info(logger, "## Se desconecta el Worker %s - Se finaliza la Query %lu - Cantidad total de Workers: %d",
             worker_id, query_id, worker_count);
}

void log_query_sent_to_worker(t_log* logger, uint64_t query_id, int priority, char* worker_id) {
    if (!logger) return;
    
    log_info(logger, "## Se envía la Query %lu (%d) al Worker %s",
             query_id, priority, worker_id);
}

void log_preemption(t_log* logger, uint64_t old_query_id, int old_priority, char* worker_id) {
    if (!logger) return;
    
    log_info(logger, "## Se desaloja la Query %lu (%d) del Worker %s - Motivo: PRIORIDAD",
             old_query_id, old_priority, worker_id);
}

void log_desalojo_por_desconexion(t_log* logger, uint64_t query_id, int priority, char* worker_id) {
    if (!logger) return;
    
    log_info(logger, "## Se desaloja la Query %lu (%d) del Worker %s - Motivo: DESCONEXION",
             query_id, priority, worker_id);
}

void log_priority_change(t_log* logger, uint64_t query_id, int old_priority, int new_priority) {
    if (!logger) return;
    
    log_info(logger, "## %lu Cambio de prioridad: %d - %d",
             query_id, old_priority, new_priority);
}

void log_query_finished(t_log* logger, uint64_t query_id, char* worker_id) {
    if (!logger) return;
    
    log_info(logger, "## Se terminó la Query %lu en el Worker %s",
             query_id, worker_id);
}

void log_read_sent_to_qc(t_log* logger, uint64_t query_id, char* worker_id) {
    if (!logger) return;
    
    log_info(logger, "## Se envía un mensaje de lectura de la Query %lu en el Worker %s al Query Control",
             query_id, worker_id);
}

// ========== FUNCIONES DE LOGGING ADICIONALES PARA DEBUG ==========

void log_master_startup(t_log* logger, int port, char* algorithm) {
    if (!logger) return;
    
    log_info(logger, "[MASTER] Servidor iniciado en puerto %d con algoritmo %s", port, algorithm);
}

void log_master_shutdown(t_log* logger) {
    if (!logger) return;
    
    log_info(logger, "[MASTER] Servidor detenido");
}

void log_query_state_change(t_log* logger, uint64_t query_id, char* old_state, char* new_state) {
    if (!logger) return;
    
    log_debug(logger, "[MASTER] Query %lu cambió de estado: %s -> %s", query_id, old_state, new_state);
}

void log_worker_state_change(t_log* logger, char* worker_id, char* old_state, char* new_state) {
    if (!logger) return;
    
    log_debug(logger, "[MASTER] Worker %s cambió de estado: %s -> %s", worker_id, old_state, new_state);
}

void log_scheduler_activity(t_log* logger, char* activity, uint64_t query_id) {
    if (!logger) return;
    
    log_debug(logger, "[SCHEDULER] %s - Query %lu", activity, query_id);
}

void log_aging_applied(t_log* logger, int queries_affected) {
    if (!logger) return;
    
    log_debug(logger, "[AGING] Aging aplicado a %d queries", queries_affected);
}

void log_protocol_error(t_log* logger, char* error_msg, int socket) {
    if (!logger) return;
    
    log_warning(logger, "[PROTOCOL] Error en socket %d: %s", socket, error_msg);
}

void log_connection_error(t_log* logger, char* error_msg) {
    if (!logger) return;
    
    log_error(logger, "[CONNECTION] %s", error_msg);
}

// ========== UTILIDADES DE LOGGING ==========

const char* query_state_to_string(query_state_t state) {
    switch (state) {
        case QUERY_NEW: return "NEW";
        case QUERY_READY: return "READY";
        case QUERY_EXEC: return "EXEC";
        case QUERY_CANCELING: return "CANCELING";
        case QUERY_EXIT: return "EXIT";
        case QUERY_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* worker_state_to_string(worker_state_t state) {
    switch (state) {
        case WORKER_IDLE: return "IDLE";
        case WORKER_BUSY: return "BUSY";
        case WORKER_PREEMPTING: return "PREEMPTING";
        default: return "UNKNOWN";
    }
}

const char* algorithm_to_string(scheduling_algorithm_t algorithm) {
    switch (algorithm) {
        case ALGORITHM_FIFO: return "FIFO";
        case ALGORITHM_PRIORIDADES: return "PRIORIDADES";
        default: return "UNKNOWN";
    }
}

void log_current_status(t_log* logger, master_t* master) {
    if (!logger || !master) return;
    
    pthread_mutex_lock(&master->main_mutex);
    int ready_count = queue_size(master->ready_queue);
    int exec_count = dictionary_size(master->exec_map);
    int worker_count = list_size(master->workers);
    pthread_mutex_unlock(&master->main_mutex);
    
    log_info(logger, "[STATUS] Workers: %d, Queries en READY: %d, Queries en EXEC: %d", 
             worker_count, ready_count, exec_count);
}

t_log_level log_level_from_string(char* level) {
    if (!level) return LOG_LEVEL_INFO;
    
    if (string_equals_ignore_case(level, "TRACE")) return LOG_LEVEL_TRACE;
    if (string_equals_ignore_case(level, "DEBUG")) return LOG_LEVEL_DEBUG;
    if (string_equals_ignore_case(level, "INFO")) return LOG_LEVEL_INFO;
    if (string_equals_ignore_case(level, "WARNING")) return LOG_LEVEL_WARNING;
    if (string_equals_ignore_case(level, "ERROR")) return LOG_LEVEL_ERROR;
    
    return LOG_LEVEL_INFO;
}