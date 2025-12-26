#define _POSIX_C_SOURCE 200809L

#include "sockets.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <commons/log.h>
#include "serializacion.h"



// Función interna: lógica de getaddrinfo para cliente y servidor
static int common_getaddrinfo(t_log* logger, const char* host, const char* port, struct addrinfo** server_info, bool is_server) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // Permite IPv4 o IPv6
    hints.ai_socktype = SOCK_STREAM;
    if (is_server) {
        hints.ai_flags = AI_PASSIVE; // Para bind en cualquier IP de la máquina
    }

    int rv = getaddrinfo(host, port, &hints, server_info);
    if (rv != 0) {
        log_error(logger, "Error en getaddrinfo: %s", gai_strerror(rv));
        return -1;
    }
    return 0;
}

// --- Funciones Públicas ---

int iniciar_servidor(t_log* logger, const char* puerto) {
    struct addrinfo *server_info;
    
    if (common_getaddrinfo(logger, NULL, puerto, &server_info, true) != 0) {
        return -1;
    }

    int socket_servidor = -1;
    for (struct addrinfo *p = server_info; p != NULL; p = p->ai_next) {
        socket_servidor = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (socket_servidor == -1) continue;

        int yes = 1;
        if (setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            log_error(logger, "Error en setsockopt");
            close(socket_servidor);
            continue;
        }

        if (bind(socket_servidor, p->ai_addr, p->ai_addrlen) == -1) {
            close(socket_servidor);
            continue;
        }
        break;
    }

    if (socket_servidor == -1) {
        log_error(logger, "No se pudo hacer bind a ningún socket");
        freeaddrinfo(server_info);
        return -1;
    }

    if (listen(socket_servidor, SOMAXCONN) == -1) {
        log_error(logger, "Error en listen");
        close(socket_servidor);
        freeaddrinfo(server_info);
        return -1;
    }

    freeaddrinfo(server_info);
    log_info(logger, "Servidor escuchando en el puerto %s", puerto);
    return socket_servidor;
}

int esperar_cliente(t_log* logger, int socket_servidor) {
    struct sockaddr_storage dir_cliente;
    socklen_t tam_direccion = sizeof(dir_cliente);

    int socket_cliente = accept(socket_servidor, (struct sockaddr*)&dir_cliente, &tam_direccion);
    if (socket_cliente == -1) {
        log_error(logger, "Error al aceptar cliente");
        return -1;
    }

    log_info(logger, "Se conectó un nuevo cliente (socket %d)", socket_cliente);
    return socket_cliente;
}

int crear_conexion(t_log* logger, const char* ip, const char* puerto) {
    struct addrinfo *server_info;

    if (common_getaddrinfo(logger, ip, puerto, &server_info, false) != 0) {
        return -1;
    }

    int socket_cliente = -1;
    for (struct addrinfo *p = server_info; p != NULL; p = p->ai_next) {
        socket_cliente = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (socket_cliente == -1) continue;

        if (connect(socket_cliente, p->ai_addr, p->ai_addrlen) == -1) {
            close(socket_cliente);
            continue;
        }
        break;
    }

    if (socket_cliente == -1) {
        log_error(logger, "No se pudo conectar al servidor %s:%s", ip, puerto);
        freeaddrinfo(server_info);
        return -1;
    }

    freeaddrinfo(server_info);
    log_info(logger, "Conectado al servidor %s:%s (socket %d)", ip, puerto, socket_cliente);
    return socket_cliente;
}

void liberar_conexion(int socket_fd) {
    if (socket_fd >= 0) close(socket_fd);
}

int enviar_paquete(int socket_fd, op_code codigo, void* payload, int size) {
    int header_size = sizeof(op_code) + sizeof(int);
    void* buffer = malloc(header_size + size);
    if (!buffer) return -1;

    memcpy(buffer, &codigo, sizeof(op_code));
    memcpy(buffer + sizeof(op_code), &size, sizeof(int));
    if (size > 0) memcpy(buffer + header_size, payload, size);

    ssize_t bytes_sent = send(socket_fd, buffer, header_size + size, 0);
    free(buffer);
    
    return (bytes_sent == header_size + size) ? 0 : -1;
}

void* recibir_payload(int socket_fd, op_code* codigo, int* size) {
    // Inicializar variables para evitar datos basura
    *codigo = -1;
    *size = 0;
    
    ssize_t recv_result = recv(socket_fd, codigo, sizeof(op_code), MSG_WAITALL);
    
    // Si recv retorna 0, la conexión se cerró limpiamente
    if (recv_result == 0) {
        return NULL;
    }
    
    // Si recv retorna < 0, hubo un error
    if (recv_result < 0) {
        return NULL;
    }
    
    recv_result = recv(socket_fd, size, sizeof(int), MSG_WAITALL);
    
    if (recv_result <= 0) {
        return NULL;
    }

    if (*size > 0) {
        void* payload = malloc(*size);
        if (!payload) {
            return NULL;
        }
        recv_result = recv(socket_fd, payload, *size, MSG_WAITALL);
        
        // Verificar que recibimos exactamente lo esperado
        if (recv_result == *size) {
            return payload;
        }
        
        // Si no recibimos todo o hubo error, liberar y retornar NULL
        free(payload);
        return NULL;
    }
    
    // Si size=0, retornar un payload válido pero vacío (para ser liberado con free)
    void* empty_payload = malloc(1);
    return empty_payload; // Puede ser NULL si malloc falló, el caller debe verificar
}

int enviar_error(int socket, error_code_t codigo_error, const char* mensaje) {
    int size;
    void* buffer = serializar_error(codigo_error, mensaje, &size);
    int resultado = enviar_paquete(socket, ERROR_RESPONSE, buffer, size);
    free(buffer);
    return resultado;
}
