#include "master.h"
#include <signal.h>

// Variable global para el master (para manejo de señales)
master_t* global_master = NULL;

// Manejador de señales para shutdown graceful
void signal_handler(int sig) {
    if (global_master) {
        printf("\n[MASTER] Señal %d recibida. Cerrando servidor...\n", sig);
        master_detener(global_master);
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    // Verificar argumentos
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <archivo_configuracion>\n", argv[0]);
        return 1;
    }

    // Configurar manejadores de señales
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Crear e inicializar el master
    global_master = master_crear(argv[1]);
    if (!global_master) {
        fprintf(stderr, "[ERROR] No se pudo inicializar el Master\n");
        return 1;
    }

    printf("[MASTER] Iniciando servidor en puerto %d...\n", global_master->config->puerto_escucha);
    printf("[MASTER] Algoritmo de planificación: %s\n", 
           global_master->config->algoritmo_planificacion == ALGORITHM_FIFO ? "FIFO" : "PRIORIDADES");
    
    if (global_master->config->tiempo_aging > 0) {
        printf("[MASTER] Aging habilitado cada %d ms\n", global_master->config->tiempo_aging);
    }

    // Iniciar el servidor
    master_iniciar(global_master);

    // Cleanup
    master_destruir(global_master);
    
    return 0;
}
