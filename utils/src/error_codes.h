#ifndef ERROR_CODES_H
#define ERROR_CODES_H

#include <stdint.h>
#include "comunicacion.h"  // Usar la definición de error_code_t de aquí

// Estructura para mensajes de error
typedef struct {
    error_code_t codigo;
    char mensaje[256];
} error_mensaje_t;

#endif // ERROR_CODES_H