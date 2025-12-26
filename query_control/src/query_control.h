#ifndef QUERY_CONTROL_H
#define QUERY_CONTROL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>  // Para PATH_MAX
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>

// Headers de utils para protocolo unificado (sin ruta relativa, el Makefile ya incluye utils/src)
#include "comunicacion.h"
#include "sockets.h"
#include "serializacion.h"

// Constantes
#define MAX_BUFFER_SIZE 4096
#define MAX_PATH_SIZE 512
#define MAX_WORKER_ID_SIZE 32

// Estructura de configuración
typedef struct {
    char* ip_master;
    int puerto_master;
    char* log_level;
    char* path_logs;
    char* queries_dir; // directorio donde buscar archivos de query (opcional)
} query_control_config_t;

// Estructura principal del Query Control
typedef struct {
    query_control_config_t* config;
    t_log* logger;
    int socket_master;
    uint64_t query_id;
    char* archivo_query;
    int prioridad;
    bool query_finished;
    bool connected;
    pthread_mutex_t mutex;
} query_control_t;

// Funciones principales
query_control_t* query_control_crear(char* config_path, char* archivo_query, int prioridad);
void query_control_destruir(query_control_t* qc);
bool query_control_conectar(query_control_t* qc);
void query_control_desconectar(query_control_t* qc);
bool query_control_enviar_query(query_control_t* qc);
void query_control_procesar_respuestas(query_control_t* qc);
void query_control_ejecutar(query_control_t* qc);

// Funciones de configuración
query_control_config_t* cargar_config_qc(char* config_path);
void config_qc_destruir(query_control_config_t* config);

// Funciones de manejo de mensajes
void manejar_new_query_ack(query_control_t* qc, uint64_t query_id);
void manejar_query_finished(query_control_t* qc, uint64_t query_id);
void manejar_read_result(query_control_t* qc, uint64_t query_id, char* origen, void* data, int data_size);
void manejar_error(query_control_t* qc, uint64_t query_id);

// Funciones de logging específicas del enunciado
void log_conexion_exitosa(t_log* logger, char* ip, int puerto);
void log_envio_query(t_log* logger, char* archivo_query, int prioridad);
void log_lectura_realizada(t_log* logger, char* file_tag, char* contenido);
void log_query_finalizada(t_log* logger, char* motivo);

#endif // QUERY_CONTROL_H