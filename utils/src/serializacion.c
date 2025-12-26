#include "serializacion.h"
#include <string.h>
#include <stdlib.h>

// --- NEW_QUERY (QC -> Master) ---
// Payload: [tamanio_path (int)] [path (char*)] [prioridad (int)]
void* serializar_new_query(const char* path, int prioridad, int* size) {
    int size_path = strlen(path) + 1;
    *size = sizeof(int) + size_path + sizeof(int);

    void* buffer = malloc(*size);
    int offset = 0;

    memcpy(buffer + offset, &size_path, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, path, size_path);
    offset += size_path;
    memcpy(buffer + offset, &prioridad, sizeof(int));

    return buffer;
}

void deserializar_new_query(void* buffer, char** path, int* prioridad) {
    int offset = 0;
    int size_path;

    memcpy(&size_path, buffer + offset, sizeof(int));
    offset += sizeof(int);
    *path = malloc(size_path);
    memcpy(*path, buffer + offset, size_path);
    offset += size_path;
    memcpy(prioridad, buffer + offset, sizeof(int));
}

// --- NEW_QUERY_ACK y QUERY_FINISHED ---
// Payload: [id (uint64_t)]
void* serializar_ack_con_id(uint64_t id, int* size) {
    *size = sizeof(uint64_t);
    void* buffer = malloc(*size);
    memcpy(buffer, &id, sizeof(uint64_t));
    return buffer;
}

void deserializar_ack_con_id(void* buffer, uint64_t* id) {
    memcpy(id, buffer, sizeof(uint64_t));
}

// --- EXECUTE_QUERY (Master -> Worker) ---
// Payload: [id (uint64_t)] [tamanio_path (int)] [path (char*)] [pc (uint32_t)]
void* serializar_execute_query(uint64_t id, const char* path, uint32_t pc, int* size) {
    int size_path = strlen(path) + 1;
    *size = sizeof(uint64_t) + sizeof(int) + size_path + sizeof(uint32_t);

    void* buffer = malloc(*size);
    int offset = 0;

    memcpy(buffer + offset, &id, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(buffer + offset, &size_path, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, path, size_path);
    offset += size_path;
    memcpy(buffer + offset, &pc, sizeof(uint32_t));
    
    return buffer;
}

void deserializar_execute_query(void* buffer, uint64_t* id, char** path, uint32_t* pc) {
    int offset = 0;
    int size_path;

    memcpy(id, buffer + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(&size_path, buffer + offset, sizeof(int));
    offset += sizeof(int);
    *path = malloc(size_path);
    memcpy(*path, buffer + offset, size_path);
    offset += size_path;
    memcpy(pc, buffer + offset, sizeof(uint32_t));
}

// --- BLOCK_SIZE_RESPONSE (Storage -> Worker) ---
// Payload: [block_size (int)]
void* serializar_respuesta_block_size(int block_size, int* size) {
    *size = sizeof(int);
    void* buffer = malloc(*size);
    memcpy(buffer, &block_size, sizeof(int));
    return buffer;
}

void deserializar_respuesta_block_size(void* buffer, int* block_size) {
    memcpy(block_size, buffer, sizeof(int));
}

void* serializar_file_tag(const char* file, const char* tag, int* size) {
    int size_file = strlen(file) + 1;
    int size_tag = strlen(tag) + 1;
    *size = sizeof(int) + size_file + sizeof(int) + size_tag;

    void* buffer = malloc(*size);
    int offset = 0;

    memcpy(buffer + offset, &size_file, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, file, size_file);
    offset += size_file;
    memcpy(buffer + offset, &size_tag, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, tag, size_tag);

    return buffer;
}

void deserializar_file_tag(void* buffer, char** file, char** tag) {
    int offset = 0;
    int size_file, size_tag;

    memcpy(&size_file, buffer + offset, sizeof(int));
    offset += sizeof(int);
    *file = malloc(size_file);
    memcpy(*file, buffer + offset, size_file);
    offset += size_file;
    
    memcpy(&size_tag, buffer + offset, sizeof(int));
    offset += sizeof(int);
    *tag = malloc(size_tag);
    memcpy(*tag, buffer + offset, size_tag);
}

// --- FS_TRUNCATE_FILE (Worker -> Storage) ---
// Payload: [size_file (int)] [file (char*)] [size_tag (int)] [tag (char*)] [new_size (int)]
void* serializar_truncate(const char* file, const char* tag, int new_size, int* size) {
    int size_file = strlen(file) + 1;
    int size_tag = strlen(tag) + 1;
    *size = sizeof(int) + size_file + sizeof(int) + size_tag + sizeof(int);

    void* buffer = malloc(*size);
    int offset = 0;

    memcpy(buffer + offset, &size_file, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, file, size_file);
    offset += size_file;
    memcpy(buffer + offset, &size_tag, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, tag, size_tag);
    offset += size_tag;
    memcpy(buffer + offset, &new_size, sizeof(int));

    return buffer;
}

// --- FS_TAG_FILE (Worker -> Storage) ---
// Payload: [size_f_o][f_o][size_t_o][t_o][size_f_d][f_d][size_t_d][t_d]
void* serializar_tag_file(const char* file_o, const char* tag_o, const char* file_d, const char* tag_d, int* size) {
    int size_fo = strlen(file_o) + 1;
    int size_to = strlen(tag_o) + 1;
    int size_fd = strlen(file_d) + 1;
    int size_td = strlen(tag_d) + 1;
    *size = sizeof(int)*4 + size_fo + size_to + size_fd + size_td;

    void* buffer = malloc(*size);
    int offset = 0;

    memcpy(buffer + offset, &size_fo, sizeof(int)); offset += sizeof(int);
    memcpy(buffer + offset, file_o, size_fo); offset += size_fo;
    memcpy(buffer + offset, &size_to, sizeof(int)); offset += sizeof(int);
    memcpy(buffer + offset, tag_o, size_to); offset += size_to;
    memcpy(buffer + offset, &size_fd, sizeof(int)); offset += sizeof(int);
    memcpy(buffer + offset, file_d, size_fd); offset += size_fd;
    memcpy(buffer + offset, &size_td, sizeof(int)); offset += sizeof(int);
    memcpy(buffer + offset, tag_d, size_td);

    return buffer;
}

void deserializar_tag_file(void* buffer, char** file_o, char** tag_o, char** file_d, char** tag_d) {
    int offset = 0;
    int size_fo, size_to, size_fd, size_td;

    memcpy(&size_fo, buffer + offset, sizeof(int)); offset += sizeof(int);
    *file_o = malloc(size_fo);
    memcpy(*file_o, buffer + offset, size_fo); offset += size_fo;

    memcpy(&size_to, buffer + offset, sizeof(int)); offset += sizeof(int);
    *tag_o = malloc(size_to);
    memcpy(*tag_o, buffer + offset, size_to); offset += size_to;

    memcpy(&size_fd, buffer + offset, sizeof(int)); offset += sizeof(int);
    *file_d = malloc(size_fd);
    memcpy(*file_d, buffer + offset, size_fd); offset += size_fd;

    memcpy(&size_td, buffer + offset, sizeof(int)); offset += sizeof(int);
    *tag_d = malloc(size_td);
    memcpy(*tag_d, buffer + offset, size_td);
}

void deserializar_truncate(void* buffer, char** file, char** tag, int* new_size) {
    int offset = 0;
    int size_file, size_tag;

    memcpy(&size_file, buffer + offset, sizeof(int));
    offset += sizeof(int);
    *file = malloc(size_file);
    memcpy(*file, buffer + offset, size_file);
    offset += size_file;
    
    memcpy(&size_tag, buffer + offset, sizeof(int));
    offset += sizeof(int);
    *tag = malloc(size_tag);
    memcpy(*tag, buffer + offset, size_tag);
    offset += size_tag;

    memcpy(new_size, buffer + offset, sizeof(int));
}

// --- FS_WRITE_BLOCK (Worker -> Storage) ---
// Payload: [size_file] [file] [size_tag] [tag] [block_num] [data_size] [data]
void* serializar_write_block(const char* file, const char* tag, int block_num, void* data, int data_size, int* size) {
    int size_file = strlen(file) + 1;
    int size_tag = strlen(tag) + 1;
    *size = sizeof(int) + size_file + sizeof(int) + size_tag + sizeof(int) + sizeof(int) + data_size;

    void* buffer = malloc(*size);
    int offset = 0;

    memcpy(buffer + offset, &size_file, sizeof(int)); offset += sizeof(int);
    memcpy(buffer + offset, file, size_file); offset += size_file;
    memcpy(buffer + offset, &size_tag, sizeof(int)); offset += sizeof(int);
    memcpy(buffer + offset, tag, size_tag); offset += size_tag;
    memcpy(buffer + offset, &block_num, sizeof(int)); offset += sizeof(int);
    memcpy(buffer + offset, &data_size, sizeof(int)); offset += sizeof(int);
    memcpy(buffer + offset, data, data_size);

    return buffer;
}

void deserializar_write_block(void* buffer, char** file, char** tag, int* block_num, void** data, int* data_size) {
    int offset = 0;
    int size_file, size_tag;

    memcpy(&size_file, buffer + offset, sizeof(int)); offset += sizeof(int);
    *file = malloc(size_file);
    memcpy(*file, buffer + offset, size_file); offset += size_file;
    
    memcpy(&size_tag, buffer + offset, sizeof(int)); offset += sizeof(int);
    *tag = malloc(size_tag);
    memcpy(*tag, buffer + offset, size_tag); offset += size_tag;

    memcpy(block_num, buffer + offset, sizeof(int)); offset += sizeof(int);
    memcpy(data_size, buffer + offset, sizeof(int)); offset += sizeof(int);
    *data = malloc(*data_size);
    memcpy(*data, buffer + offset, *data_size);
}

// --- FS_READ_BLOCK (Worker -> Storage) ---
// Payload: [size_file] [file] [size_tag] [tag] [block_num]
void* serializar_read_block(const char* file, const char* tag, int block_num, int* size) {
    int size_file = strlen(file) + 1;
    int size_tag = strlen(tag) + 1;
    *size = sizeof(int) + size_file + sizeof(int) + size_tag + sizeof(int);

    void* buffer = malloc(*size);
    int offset = 0;

    memcpy(buffer + offset, &size_file, sizeof(int)); offset += sizeof(int);
    memcpy(buffer + offset, file, size_file); offset += size_file;
    memcpy(buffer + offset, &size_tag, sizeof(int)); offset += sizeof(int);
    memcpy(buffer + offset, tag, size_tag); offset += size_tag;
    memcpy(buffer + offset, &block_num, sizeof(int));

    return buffer;
}

void deserializar_read_block(void* buffer, char** file, char** tag, int* block_num) {
    int offset = 0;
    int size_file, size_tag;

    memcpy(&size_file, buffer + offset, sizeof(int)); offset += sizeof(int);
    *file = malloc(size_file);
    memcpy(*file, buffer + offset, size_file); offset += size_file;
    
    memcpy(&size_tag, buffer + offset, sizeof(int)); offset += sizeof(int);
    *tag = malloc(size_tag);
    memcpy(*tag, buffer + offset, size_tag); offset += size_tag;

    memcpy(block_num, buffer + offset, sizeof(int));
}

// --- BLOCK_CONTENT (Storage -> Worker) ---
// Payload: [data_size] [data]
void* serializar_block_content(void* data, int data_size, int* size) {
    *size = sizeof(int) + data_size;
    void* buffer = malloc(*size);
    
    memcpy(buffer, &data_size, sizeof(int));
    memcpy(buffer + sizeof(int), data, data_size);
    
    return buffer;
}

void deserializar_block_content(void* buffer, void** data, int* data_size) {
    memcpy(data_size, buffer, sizeof(int));
    *data = malloc(*data_size);
    memcpy(*data, buffer + sizeof(int), *data_size);
}

// --- READ_RESULT (Worker -> Master) ---
// Payload: [id] [size_origen] [origen] [size_contenido] [contenido]
void* serializar_read_result(uint64_t id, const char* origen, const char* contenido, int* size) {
    int size_origen = strlen(origen) + 1;
    int size_contenido = strlen(contenido) + 1;
    *size = sizeof(uint64_t) + sizeof(int) + size_origen + sizeof(int) + size_contenido;

    void* buffer = malloc(*size);
    int offset = 0;

    memcpy(buffer + offset, &id, sizeof(uint64_t)); offset += sizeof(uint64_t);
    memcpy(buffer + offset, &size_origen, sizeof(int)); offset += sizeof(int);
    memcpy(buffer + offset, origen, size_origen); offset += size_origen;
    memcpy(buffer + offset, &size_contenido, sizeof(int)); offset += sizeof(int);
    memcpy(buffer + offset, contenido, size_contenido);

    return buffer;
}

void deserializar_read_result(void* buffer, uint64_t* id, char** origen, char** contenido) {
    int offset = 0;
    int size_origen, size_contenido;

    memcpy(id, buffer + offset, sizeof(uint64_t)); offset += sizeof(uint64_t);
    memcpy(&size_origen, buffer + offset, sizeof(int)); offset += sizeof(int);
    *origen = malloc(size_origen);
    memcpy(*origen, buffer + offset, size_origen); offset += size_origen;
    
    memcpy(&size_contenido, buffer + offset, sizeof(int)); offset += sizeof(int);
    *contenido = malloc(size_contenido);
    memcpy(*contenido, buffer + offset, size_contenido);
}

// --- PREEMPTION_ACK (Worker -> Master) ---
// Payload: [pc (uint32_t)]
void* serializar_preemption_ack(uint32_t pc, int* size) {
    *size = sizeof(uint32_t);
    void* buffer = malloc(*size);
    memcpy(buffer, &pc, sizeof(uint32_t));
    return buffer;
}

void deserializar_preemption_ack(void* buffer, uint32_t* pc) {
    memcpy(pc, buffer, sizeof(uint32_t));
}

// --- QUERY_FINISHED con motivo de error ---
void* serializar_query_finished_error(uint64_t id, const char* motivo, int* size) {
    int size_motivo = strlen(motivo) + 1;
    *size = sizeof(uint64_t) + sizeof(int) + size_motivo;

    void* buffer = malloc(*size);
    int offset = 0;

    memcpy(buffer + offset, &id, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(buffer + offset, &size_motivo, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, motivo, size_motivo);

    return buffer;
}

void deserializar_query_finished_error(void* buffer, uint64_t* id, char** motivo) {
    int offset = 0;
    int size_motivo;

    memcpy(id, buffer + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(&size_motivo, buffer + offset, sizeof(int));
    offset += sizeof(int);
    *motivo = malloc(size_motivo);
    memcpy(*motivo, buffer + offset, size_motivo);
}

// --- ERROR_RESPONSE (Respuesta de error genérica) ---
void* serializar_error(error_code_t codigo_error, const char* mensaje, int* size) {
    int size_mensaje = strlen(mensaje) + 1;
    *size = sizeof(error_code_t) + sizeof(int) + size_mensaje;
    
    void* buffer = malloc(*size);
    int offset = 0;
    
    memcpy(buffer + offset, &codigo_error, sizeof(error_code_t));
    offset += sizeof(error_code_t);
    memcpy(buffer + offset, &size_mensaje, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, mensaje, size_mensaje);
    
    return buffer;
}

void deserializar_error(void* buffer, error_code_t* codigo_error, char** mensaje) {
    int offset = 0;
    int size_mensaje;
    
    memcpy(codigo_error, buffer + offset, sizeof(error_code_t));
    offset += sizeof(error_code_t);
    memcpy(&size_mensaje, buffer + offset, sizeof(int));
    offset += sizeof(int);
    *mensaje = malloc(size_mensaje);
    memcpy(*mensaje, buffer + offset, size_mensaje);
}