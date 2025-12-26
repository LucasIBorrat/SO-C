#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stdint.h>

// ========== CÓDIGOS DE ERROR ==========

typedef enum {
    ERROR_SUCCESS = 0,
    ERROR_GENERAL = 1,
    ERROR_MEMORIA = 2,
    ERROR_TIMEOUT = 3,
    ERROR_CONEXION_PERDIDA = 4,
    ERROR_STORAGE_ARCHIVO_NO_EXISTE = 100,
    ERROR_STORAGE_TAG_NO_EXISTE = 101,
    ERROR_STORAGE_BLOQUE_INVALIDO = 102,
    ERROR_STORAGE_SIN_ESPACIO = 103,
    ERROR_STORAGE_TAG_YA_EXISTE = 104,
    ERROR_STORAGE_ARCHIVO_YA_EXISTE = 105,
    ERROR_STORAGE_TAG_COMMITED = 106,
    ERROR_WORKER_PROGRAMA_INVALIDO = 200,
    ERROR_WORKER_STACK_OVERFLOW = 201,
    ERROR_WORKER_DIVISION_CERO = 202,
    ERROR_MASTER_QUERY_INVALIDA = 300,
    ERROR_MASTER_WORKER_NO_DISPONIBLE = 301,
    ERROR_QC_ARCHIVO_NO_ENCONTRADO = 400,
    ERROR_QC_SINTAXIS_INVALIDA = 401
} error_code_t;

// ========== CÓDIGOS DE OPERACIÓN ==========

typedef enum {
    // -- Handshakes --
    HANDSHAKE_QUERY_CONTROL,
    HANDSHAKE_WORKER,
    HANDSHAKE_OK,

    // -- Flujo QC -> Master --
    NEW_QUERY,
    NEW_QUERY_ACK, // Master -> QC (con query_id)

    // -- Flujo Master <-> Worker --
    EXECUTE_QUERY,      // Master -> Worker (path, pc, query_id)
    QUERY_FINISHED,     // Worker -> Master
    READ_RESULT,        // Worker -> Master -> QC (file:tag, data)
    PREEMPT_QUERY,      // Master -> Worker
    PREEMPTION_ACK,     // Worker -> Master (pc)
    CANCEL_QUERY,       // Master -> Worker

    // -- Flujo Worker -> Storage --
    FS_GET_BLOCK_SIZE,
    FS_CREATE_FILE,     // (file, tag)
    FS_TRUNCATE_FILE,   // (file, tag, new_size)
    FS_WRITE_BLOCK,     // (file, tag, block_num, data)
    FS_READ_BLOCK,      // (file, tag, block_num)
    FS_COMMIT_TAG,      // (file, tag)
    FS_DELETE_TAG,      // (file, tag)
    FS_TAG_FILE,        // (file_orig, tag_orig, file_dest, tag_dest)

    // -- Respuestas Genéricas --
    SUCCESS,
    ERROR,

    // -- Contenido de Payloads --
    BLOCK_CONTENT,      // Storage -> Worker
    BLOCK_SIZE_RESPONSE // Storage -> Worker

} op_code;

// -- Respuestas con código de error --
#define ERROR_RESPONSE ERROR

// Estructura para respuestas con error
typedef struct {
    op_code codigo_op;
    error_code_t codigo_error;
    char mensaje[256];
} error_response_t;

#endif