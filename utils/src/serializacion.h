// utils/src/serializacion.h

#ifndef SERIALIZACION_H
#define SERIALIZACION_H

#include "comunicacion.h"
#include <stdint.h>


// NEW_QUERY (QC -> Master)
void* serializar_new_query(const char* path, int prioridad, int* size);
void deserializar_new_query(void* buffer, char** path, int* prioridad);

// NEW_QUERY_ACK y QUERY_FINISHED (Master -> QC y Worker -> Master)
void* serializar_ack_con_id(uint64_t id, int* size);
void deserializar_ack_con_id(void* buffer, uint64_t* id);

// PREEMPTION_ACK (Worker -> Master)
void* serializar_preemption_ack(uint32_t pc, int* size);
void deserializar_preemption_ack(void* buffer, uint32_t* pc);

// EXECUTE_QUERY (Master -> Worker)
void* serializar_execute_query(uint64_t id, const char* path, uint32_t pc, int* size);
void deserializar_execute_query(void* buffer, uint64_t* id, char** path, uint32_t* pc);

// BLOCK_SIZE_RESPONSE (Storage -> Worker)
void* serializar_respuesta_block_size(int block_size, int* size);
void deserializar_respuesta_block_size(void* buffer, int* block_size);

// Mensajes genéricos con File y Tag (CREATE, COMMIT, DELETE)
void* serializar_file_tag(const char* file, const char* tag, int* size);
void deserializar_file_tag(void* buffer, char** file, char** tag);

// FS_TRUNCATE_FILE (Worker -> Storage)
void* serializar_truncate(const char* file, const char* tag, int new_size, int* size);
void deserializar_truncate(void* buffer, char** file, char** tag, int* new_size);

// FS_WRITE_BLOCK (Worker -> Storage)
void* serializar_write_block(const char* file, const char* tag, int block_num, void* data, int data_size, int* size);
void deserializar_write_block(void* buffer, char** file, char** tag, int* block_num, void** data, int* data_size);

// FS_READ_BLOCK (Worker -> Storage)
void* serializar_read_block(const char* file, const char* tag, int block_num, int* size);
void deserializar_read_block(void* buffer, char** file, char** tag, int* block_num);

// FS_TAG_FILE (Worker -> Storage)
void* serializar_tag_file(const char* file_o, const char* tag_o, const char* file_d, const char* tag_d, int* size);
void deserializar_tag_file(void* buffer, char** file_o, char** tag_o, char** file_d, char** tag_d);

// BLOCK_CONTENT (Storage -> Worker)
void* serializar_block_content(void* data, int data_size, int* size);
void deserializar_block_content(void* buffer, void** data, int* data_size);

// READ_RESULT (Worker -> Master)
void* serializar_read_result(uint64_t id, const char* origen, const char* contenido, int* size);
void deserializar_read_result(void* buffer, uint64_t* id, char** origen, char** contenido);

// QUERY_FINISHED con motivo de error
void* serializar_query_finished_error(uint64_t id, const char* motivo, int* size);
void deserializar_query_finished_error(void* buffer, uint64_t* id, char** motivo);

// ERROR_RESPONSE (Respuesta de error genérica)
void* serializar_error(error_code_t codigo_error, const char* mensaje, int* size);
void deserializar_error(void* buffer, error_code_t* codigo_error, char** mensaje);

#endif