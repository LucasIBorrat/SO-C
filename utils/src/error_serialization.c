#include "serializacion.h"
#include "comunicacion.h"
#include <stdlib.h>
#include <string.h>

// Funciones para serializar y deserializar mensajes de error
void* serializar_error(error_code_t codigo, const char* mensaje, int* size) {
    *size = sizeof(error_code_t) + 256;  // Tamaño fijo para el mensaje
    void* buffer = malloc(*size);
    int offset = 0;

    // Copiar código de error
    memcpy(buffer + offset, &codigo, sizeof(error_code_t));
    offset += sizeof(error_code_t);

    // Copiar mensaje (con padding fijo)
    memcpy(buffer + offset, mensaje, strlen(mensaje) + 1);

    return buffer;
}

void deserializar_error(void* buffer, error_code_t* codigo, char** mensaje) {
    int offset = 0;

    // Obtener código de error
    memcpy(codigo, buffer + offset, sizeof(error_code_t));
    offset += sizeof(error_code_t);

    // Reservar memoria para el mensaje si es necesario
    if (*mensaje == NULL) {
        *mensaje = malloc(256);
    }

    // Obtener mensaje
    memcpy(*mensaje, buffer + offset, 256);
    (*mensaje)[255] = '\0'; // Asegurar terminación
}