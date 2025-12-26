#ifndef MASTER_H
#define MASTER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define sleep_ms(ms) usleep((ms)*1000)
#define sleep_us(us) usleep(us)
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <commons/collections/dictionary.h>
#include <commons/string.h>
#include <commons/temporal.h>

// Headers de utils para protocolo unificado (sin ruta relativa, el Makefile ya incluye utils/src)
#include "comunicacion.h"
#include "sockets.h"
#include "serializacion.h"

// Constantes
#define MAX_BUFFER_SIZE 4096
#define MAX_PATH_SIZE 256
#define MAX_WORKER_ID_SIZE 32

// Estados de Query
typedef enum {
    QUERY_NEW,
    QUERY_READY,
    QUERY_EXEC,
    QUERY_CANCELING,  // Query está siendo cancelada (esperando contexto del worker)
    QUERY_EXIT,
    QUERY_ERROR
} query_state_t;

// Estados de Worker
typedef enum {
    WORKER_IDLE,
    WORKER_BUSY,
    WORKER_PREEMPTING  // Worker está en proceso de desalojo
} worker_state_t;

// Algoritmos de planificación
typedef enum {
    ALGORITHM_FIFO,
    ALGORITHM_PRIORIDADES
} scheduling_algorithm_t;

// Usar op_code de utils/src/comunicacion.h para los tipos de mensaje
// Los tipos están definidos en utils/src/comunicacion.h

// Estructura de Query
typedef struct {
    uint64_t id;
    char path_query[MAX_PATH_SIZE];
    int priority;
    int priority_original;
    query_state_t state;
    uint32_t pc;
    int qc_socket;
    char worker_id[MAX_WORKER_ID_SIZE];
} query_t;

// Estructura de Worker
typedef struct {
    char id[MAX_WORKER_ID_SIZE];
    int socket;
    worker_state_t status;
    uint64_t current_query_id;
} worker_t;

// Estructura de Query Control
typedef struct {
    int socket;
    uint64_t connected_query_id;
} query_control_t;

// Estructura de configuración del Master
typedef struct {
    int puerto_escucha;
    scheduling_algorithm_t algoritmo_planificacion;
    int tiempo_aging;
    char log_level[32];
} master_config_t;

// Estructura principal del Master
typedef struct {
    master_config_t* config;
    t_log* logger;
    
    // Generador de IDs
    uint64_t next_query_id;
    int next_worker_id;  // Contador secuencial para IDs de workers
    
    // Colas y estructuras de datos
    t_queue* ready_queue;
    t_dictionary* exec_map;  // worker_id -> query_t*
    t_dictionary* pending_preemptions; // worker_id -> query_t* (nueva query esperando)
    t_dictionary* pending_cancellations; // worker_id -> query_t* (query siendo cancelada)
    t_list* workers;
    t_list* query_controls;
    
    // Mutex principal (para simplificar y evitar deadlocks)
    pthread_mutex_t main_mutex;
    
    // Socket del servidor
    int server_socket;
    
    // Control de hilos
    bool running;
    pthread_t aging_thread;
    
    // Contadores para logging
    int worker_count;
    
} master_t;

// Estructura simplificada para datos de mensaje (no incluye op_code que va separado)
typedef struct {
    uint64_t query_id;
    char path[MAX_PATH_SIZE];
    int priority;
    uint32_t pc;
    char worker_id[MAX_WORKER_ID_SIZE];
    char data[MAX_BUFFER_SIZE];
    char reason[256];
} message_data_t;

// Funciones principales
master_t* master_crear(char* config_path);
void master_destruir(master_t* master);
void master_iniciar(master_t* master);
void master_detener(master_t* master);

// Funciones de configuración
master_config_t* cargar_config(char* config_path);
void master_config_destruir(master_config_t* config);

// Funciones de Query
query_t* query_crear(uint64_t id, char* path, int priority, int qc_socket);
void query_destruir(query_t* query);
uint64_t generar_id_query(master_t* master);

// Funciones de Worker
worker_t* worker_crear(char* id, int socket);
void worker_destruir(worker_t* worker);
worker_t* buscar_worker_por_id(master_t* master, char* worker_id);
worker_t* buscar_worker_libre(master_t* master);
int contar_workers_disponibles(master_t* master);  // ⚠️ Llamar con mutex tomado
int contar_workers_totales(master_t* master);     // ⚠️ Llamar con mutex tomado

// Funciones de Query Control
query_control_t* query_control_crear(int socket);
void query_control_destruir(query_control_t* qc);

// Funciones del planificador
void planificador_inicializar(master_t* master);
void planificar_query(master_t* master, query_t* query);
void planificar_siguiente_query(master_t* master);
void asignar_query_a_worker(master_t* master, query_t* query, worker_t* worker);
query_t* obtener_query_mayor_prioridad(master_t* master);
void aplicar_aging(master_t* master);
void* funcion_hilo_aging(void* arg);

// Funciones de desalojo (versiones directas para evitar deadlocks)
worker_t* buscar_worker_con_menor_prioridad_directo(master_t* master, int new_priority);
void desalojar_query_de_worker_directo(master_t* master, worker_t* worker, query_t* new_query);
void completar_desalojo_worker(master_t* master, worker_t* worker, uint32_t pc);

// Funciones de cancelación
void completar_cancelacion_query(master_t* master, worker_t* worker, uint32_t pc);

// Funciones de red
void* manejar_conexion(void* arg);
int configurar_socket_servidor(int port);
void manejar_mensaje_query_control(master_t* master, int client_socket, op_code codigo, void* payload, int size);
void manejar_mensaje_worker(master_t* master, int client_socket, op_code codigo, void* payload, int size);
worker_t* buscar_worker_por_socket(master_t* master, int socket);

// Funciones de manejo de desconexiones
void manejar_desconexion_cliente(master_t* master, int client_socket);
void manejar_desconexion_worker(master_t* master, worker_t* worker);
void manejar_desconexion_query_control(master_t* master, query_control_t* qc);
query_control_t* buscar_query_control_por_socket(master_t* master, int socket);

// Funciones de protocolo - Usar las de utils/src/sockets.h y utils/src/serializacion.h
// enviar_paquete(int socket, op_code codigo, void* payload, int size)
// recibir_payload(int socket, op_code* codigo, int* size)

// Funciones de logging específicas del enunciado
void log_query_control_connect(t_log* logger, char* path, int priority, uint64_t query_id, int worker_count);
void log_worker_connect(t_log* logger, char* worker_id, int worker_count);
void log_query_control_disconnect(t_log* logger, uint64_t query_id, int priority, int worker_count);
void log_worker_disconnect(t_log* logger, char* worker_id, uint64_t query_id, int worker_count);
void log_query_sent_to_worker(t_log* logger, uint64_t query_id, int priority, char* worker_id);
void log_preemption(t_log* logger, uint64_t old_query_id, int old_priority, char* worker_id);
void log_query_finished(t_log* logger, uint64_t query_id, char* worker_id);
void log_priority_change(t_log* logger, uint64_t query_id, int old_priority, int new_priority);
void log_read_sent_to_qc(t_log* logger, uint64_t query_id, char* worker_id);
void log_desalojo_por_desconexion(t_log* logger, uint64_t query_id, int priority, char* worker_id);

// Utilidad para convertir string a t_log_level
t_log_level log_level_from_string(char* level);

#endif // MASTER_H
