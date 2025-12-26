#include "master.h"

master_config_t* cargar_config(char* config_path) {
    t_config* config = config_create(config_path);
    if (!config) {
        printf("[ERROR] No se pudo cargar el archivo de configuración: %s\n", config_path);
        return NULL;
    }

    master_config_t* master_config = malloc(sizeof(master_config_t));
    if (!master_config) {
        config_destroy(config);
        return NULL;
    }

    // Cargar valores de configuración con valores por defecto
    master_config->puerto_escucha = config_has_property(config, "PUERTO_ESCUCHA") ? 
                                   config_get_int_value(config, "PUERTO_ESCUCHA") : 9001;

    // Algoritmo de planificación
    char* algoritmo = config_has_property(config, "ALGORITMO_PLANIFICACION") ? 
                     config_get_string_value(config, "ALGORITMO_PLANIFICACION") : "FIFO";
    
    if (string_equals_ignore_case(algoritmo, "FIFO")) {
        master_config->algoritmo_planificacion = ALGORITHM_FIFO;
    } else if (string_equals_ignore_case(algoritmo, "PRIORIDADES")) {
        master_config->algoritmo_planificacion = ALGORITHM_PRIORIDADES;
    } else {
        printf("[WARNING] Algoritmo desconocido '%s', usando FIFO por defecto\n", algoritmo);
        master_config->algoritmo_planificacion = ALGORITHM_FIFO;
    }

    master_config->tiempo_aging = config_has_property(config, "TIEMPO_AGING") ? 
                                 config_get_int_value(config, "TIEMPO_AGING") : 0;

    char* log_level_str = config_has_property(config, "LOG_LEVEL") ? 
                         config_get_string_value(config, "LOG_LEVEL") : "INFO";
    strncpy(master_config->log_level, log_level_str, 31);
    master_config->log_level[31] = '\0';


    config_destroy(config);
    return master_config;
}

void master_config_destruir(master_config_t* config) {
    if (config) {
        free(config);
    }
}
