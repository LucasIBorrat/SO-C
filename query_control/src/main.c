#include "query_control.h"

int main(int argc, char* argv[]) {
    // Verificar argumentos de línea de comandos
    if (argc != 4) {
        printf("Uso: %s [archivo_config] [archivo_query] [prioridad]\n", argv[0]);
        return EXIT_FAILURE;
    }

    char* config_path = argv[1];
    char* archivo_query = argv[2];
    int prioridad = atoi(argv[3]);

    // Validar prioridad
    if (prioridad < 0) {
        printf("Error: La prioridad debe ser mayor o igual a 0\n");
        return EXIT_FAILURE;
    }

    // Crear instancia del Query Control
    query_control_t* qc = query_control_crear(config_path, archivo_query, prioridad);
    if (!qc) {
        printf("Error: No se pudo crear el Query Control\n");
        return EXIT_FAILURE;
    }

    // Ejecutar el Query Control
    query_control_ejecutar(qc);

    // Limpiar recursos
    query_control_destruir(qc);

    return EXIT_SUCCESS;
}