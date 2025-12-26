#ifndef SOCKETS_H
#define SOCKETS_H

#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "comunicacion.h"
#include <commons/log.h>

// Funciones de Cliente
int crear_conexion(t_log* logger, const char* ip, const char* puerto);
void liberar_conexion(int socket_fd);

// Funciones de Servidor
int iniciar_servidor(t_log* logger, const char* puerto);
int esperar_cliente(t_log* logger, int socket_servidor);

// Funciones de Comunicación 
int enviar_paquete(int socket, op_code codigo, void* payload, int size);
void* recibir_payload(int socket, op_code* codigo, int* size);

// Función helper para enviar errores
int enviar_error(int socket, error_code_t codigo_error, const char* mensaje);

#endif // SOCKETS_H
