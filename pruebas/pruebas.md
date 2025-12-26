
# Master of Files
## Documento de pruebas Finales

> *Leading on your code destruction. Test me, you will see, results is all you need*

**Cátedra de Sistemas Operativos**
**Trabajo práctico Cuatrimestral**
**-2C2025 -**
**Versión 1.0**

---

## Índice

*   [Versión de Cambios](#versión-de-cambios)
*   [Criterios de Evaluación](#criterios-de-evaluación)
*   [Aclaraciones](#aclaraciones)
*   [Prueba Planificación](#prueba-planificación)
*   [Prueba Memoria Worker](#prueba-memoria-worker)
*   [Prueba Errores](#prueba-errores)
*   [Prueba Storage](#prueba-storage)
*   [Prueba Estabilidad General](#prueba-estabilidad-general)
*   [Planilla de Evaluación](#planilla-de-evaluación---tp2c2025)

---

## Versión de Cambios

*   **v1.0 (23/11/2025):** Publicación Inicial de Pruebas Finales

---

## Criterios de Evaluación

Los grupos deberán concurrir al laboratorio habiendo corrido las pruebas y siendo conscientes de que las mismas funcionan en un entorno distribuido, es decir, **si el trabajo práctico no puede correr en más de una máquina el mismo no se evaluará.**

Al momento de realizar la evaluación en el laboratorio los alumnos dispondrán de un máximo de **10 minutos** para configurar el ambiente en las computadoras del laboratorio y validar que las conexiones se encuentren funcionando mediante la ejecución de una prueba indicada por el ayudante, caso contrario se considerará que el grupo no se encuentra en condiciones de ser evaluado.

Los grupos contarán con **una única instancia de evaluación por fecha**, es decir, que ante un error no resoluble en el momento, se considerará que el grupo no puede continuar la evaluación y por lo tanto esa entrega se encuentra **desaprobada**, teniendo que presentarse en las siguientes si las hubiera.

---

## Aclaraciones

Todos los scripts para realizar las pruebas que se enumeran en este documento se encuentran subidos al repositorio: [master-of-files-pruebas](https://github.com/sisoputnfrba/master-of-files-pruebas) *(Link inferido)*.

Dentro de las configuraciones propuestas en cada prueba puede haber casos de algunos procesos que no tengan su respectiva configuración, ya que contendrían valores que no afectan al resultado de la prueba en sí.

Los datos de los config que no son provistos en el documento de pruebas es porque dependen de la computadora o del desarrollo de los alumnos (por ejemplo IPs, Puertos o Paths), la configuración del `log_level` siempre deberá estar en **INFO**.

Será responsabilidad del grupo verificar las dependencias requeridas para la compilación, y en caso de requerir bibliotecas provistas por la cátedra, descargarlas e instalarlas en la vm.

Está totalmente prohibido subir archivos binarios o ejecutables al repositorio.

Además de ejecutar cada caso de prueba, se deben comparar los resultados obtenidos con el resultado esperado según los conceptos vistos en la materia. Verificando que lo observado en la prueba coincida con lo que teóricamente debería ocurrir para cada caso puntual.

> **Nota:** Recomendamos leer la Guía de Deploy.

---

## Prueba Planificación

### Actividades

1.  Iniciar los módulos Master, Storage y 2 Workers.
2.  Ejecutar 4 Query Control en orden con los siguientes parámetros:

| Query Control 1 | Query Control 2 |
| :--- | :--- |
| `SCRIPT=FIFO_1` | `SCRIPT=FIFO_2` |
| `PRIORIDAD=4` | `PRIORIDAD=3` |

| Query Control 3 | Query Control 4 |
| :--- | :--- |
| `SCRIPT=FIFO_3` | `SCRIPT=FIFO_4` |
| `PRIORIDAD=5` | `PRIORIDAD=1` |

3.  Una vez finalizados los 4 scripts bajar todos los módulos, cambiar el algoritmo de Master a Prioridades y el fresh start del Storage a False.
4.  Iniciar Master, Storage y 2 Workers.
5.  Ejecutar 4 Query Control en orden con los siguientes parámetros:

| Query Control 1 | Query Control 2 |
| :--- | :--- |
| `SCRIPT=AGING_1` | `SCRIPT=AGING_2` |
| `PRIORIDAD=4` | `PRIORIDAD=3` |

| Query Control 3 | Query Control 4 |
| :--- | :--- |
| `SCRIPT=AGING_3` | `SCRIPT=AGING_4` |
| `PRIORIDAD=5` | `PRIORIDAD=1` |

6.  Esperar la finalización de las queries.

### Resultados Esperados

*   Las Queries se ejecutan respetando el algoritmo elegido y en el caso de Aging empiezan a haber desalojos a medida que se incrementa la prioridad.

### Configuración del sistema

**Master.config**
```ini
ALGORITMO_PLANIFICACION=FIFO
TIEMPO_AGING=500
```

**Worker1.config**
```ini
TAM_MEMORIA=1024
RETARDO_MEMORIA=50
ALGORITMO_REEMPLAZO=LRU
```

**Storage.config**
```ini
RETARDO_OPERACION=25
RETARDO_ACCESO_BLOQUE=25
FRESH_START=TRUE
```

**Worker2.config**
```ini
TAM_MEMORIA=1024
RETARDO_MEMORIA=50
ALGORITMO_REEMPLAZO=CLOCK-M
```

**superblock.config**
```ini
FS_SIZE=65536
BLOCK_SIZE=16
```

---

## Prueba Memoria Worker

### Actividades

1.  Iniciar los módulos Storage, Master y 1 Worker.
2.  Ejecutar una Query con el script: `MEMORIA_WORKER` y prioridad 0.
3.  Esperar la finalización de un script, bajar todo, cambiar el algoritmo de reemplazo de memoria a LRU y volver a iniciar la prueba ejecutando la Query con el script `MEMORIA_WORKER_2`.

### Resultados Esperados

*   La query finaliza correctamente en ambos casos y los reemplazos se dan de acuerdo al algoritmo elegido.

### Configuración del sistema

**Master.config**
```ini
ALGORITMO_PLANIFICACION=FIFO
TIEMPO_AGING=0
```

**Worker.config**
```ini
TAM_MEMORIA=48
RETARDO_MEMORIA=25
ALGORITMO_REEMPLAZO=CLOCK-M
```

**Storage.config**
```ini
RETARDO_OPERACION=50
RETARDO_ACCESO_BLOQUE=50
FRESH_START=FALSE
```

**superblock.config**
```ini
FS_SIZE=65536
BLOCK_SIZE=16
```

---

## Prueba Errores

### Actividades

1.  Iniciar los módulos Storage, Master y 1 Worker.
    a. Ir iniciando Queries con los siguientes scripts y siempre con prioridad 1:
    i. `ESCRITURA_ARCHIVO_COMMITED`
    ii. `FILE_EXISTENTE`
    iii. `LECTURA_FUERA_DEL_LIMITE`
    iv. `TAG_EXISTENTE`

### Resultados Esperados

*   Las queries finalizan con los errores correspondientes.

### Configuración del sistema

**Master.config**
```ini
ALGORITMO_PLANIFICACION=FIFO
TIEMPO_AGING=0
```

**Worker.config**
```ini
TAM_MEMORIA=256
RETARDO_MEMORIA=25
ALGORITMO_REEMPLAZO=LRU
```

**Storage.config**
```ini
RETARDO_OPERACION=50
RETARDO_ACCESO_BLOQUE=50
FRESH_START=FALSE
```

**superblock.config**
```ini
FS_SIZE=65536
BLOCK_SIZE=16
```

---

## Prueba Storage

### Actividades

1.  Iniciar los módulos Storage, Master y 1 Worker.
2.  Ejecutar 4 Query Control en orden con los siguientes parámetros:

| Query Control 1 | Query Control 2 |
| :--- | :--- |
| `SCRIPT=STORAGE_1` | `SCRIPT=STORAGE_2` |
| `PRIORIDAD=0` | `PRIORIDAD=2` |

| Query Control 3 | Query Control 4 |
| :--- | :--- |
| `SCRIPT=STORAGE_3` | `SCRIPT=STORAGE_4` |
| `PRIORIDAD=4` | `PRIORIDAD=6` |

3.  Una vez finalizados las queries validar el estado del FS y a continuación ejecutar un nuevo Query Control con el script `STORAGE_5`.
4.  Esperar la finalización de la query y validar nuevamente el estado del FS.

### Resultados Esperados

Se puede observar correctamente el uso de los bloques y la deduplicación al momento de realizar el commit de los archivos.

### Configuración del sistema

**Master.config**
```ini
ALGORITMO_PLANIFICACION=PRIORIDADES
TIEMPO_AGING=0
```

**Worker.config**
```ini
TAM_MEMORIA=128
RETARDO_MEMORIA=25
ALGORITMO_REEMPLAZO=LRU
```

**Storage.config**
```ini
RETARDO_OPERACION=50
RETARDO_ACCESO_BLOQUE=50
FRESH_START=FALSE
```

**superblock.config**
```ini
FS_SIZE=65536
BLOCK_SIZE=16
```

---

## Prueba Estabilidad General

### Actividades

1.  Iniciar los módulos Storage, Master y los Worker 1 y 2.
2.  Ejecutar en segundo plano 25 instancias de **cada** Query de Aging, todas con prioridad 20.
3.  Esperar entre 30 y 60 segundos e iniciar los workers 3 y 4.
4.  Esperar entre 30 y 60 segundos y finalizar los Workers 1 y 2.
5.  Esperar entre 30 y 60 segundos e iniciar los Workers 5 y 6.
6.  Esperar la finalización de las queries.

### Resultados Esperados

No se observan esperas activas ni memory leaks y el sistema no finaliza de manera abrupta.

### Configuración del sistema

**Master.config**
```ini
ALGORITMO_PLANIFICACION=PRIORIDADES
TIEMPO_AGING=25
```

**Storage.config**
```ini
RETARDO_OPERACION=100
RETARDO_ACCESO_BLOQUE=10
FRESH_START=FALSE
```

**Worker1.config**
```ini
TAM_MEMORIA=128
RETARDO_MEMORIA=10
ALGORITMO_REEMPLAZO=LRU
```

**Worker2.config**
```ini
TAM_MEMORIA=256
RETARDO_MEMORIA=5
ALGORITMO_REEMPLAZO=CLOCK-M
```

**Worker3.config**
```ini
TAM_MEMORIA=64
RETARDO_MEMORIA=15
ALGORITMO_REEMPLAZO=CLOCK-M
```

**Worker4.config**
```ini
TAM_MEMORIA=96
RETARDO_MEMORIA=15
ALGORITMO_REEMPLAZO=LRU
```

**Worker5.config**
```ini
TAM_MEMORIA=160
RETARDO_MEMORIA=10
ALGORITMO_REEMPLAZO=LRU
```

**Worker6.config**
```ini
TAM_MEMORIA=192
RETARDO_MEMORIA=15
ALGORITMO_REEMPLAZO=CLOCK-M
```

**superblock.config**
```ini
FS_SIZE=65536
BLOCK_SIZE=16
```

---

## Planilla de Evaluación - TP2C2025

| Nombre del Grupo | Nota (Grupal) |
| :--- | :--- |
| | |

| Legajo | Apellido y Nombres | Nota (Coloquio) |
| :--- | :--- | :--- |
| | | |
| | | |
| | | |

*   **Evaluador/es Práctica:**
*   **Evaluador/es Coloquio:**
*   **Observaciones:**

### Lista de Chequeo

**Sistema Completo**
- [ ] El deploy se hace compilando los módulos en las máquinas del laboratorio en menos de 10 minutos.
- [ ] Los procesos se ejecutan de forma simultánea y la cantidad de hilos y subprocesos en el sistema es la adecuada.
- [ ] Los procesos establecen conexiones TCP/IP.
- [ ] El sistema no registra casos de Espera Activa ni Memory Leaks.
- [ ] El log respeta los lineamientos de logs mínimos y obligatorios de cada módulo.
- [ ] El sistema no requiere permisos de superuser (sudo/root) para ejecutar correctamente.
- [ ] El sistema no requiere de Valgrind o algún proceso similar para ejecutar correctamente.
- [ ] El sistema utiliza una sincronización determinística (no utiliza más sleeps de los solicitados).

**Módulo Master**
- [ ] Se permite la conexión y desconexión de los worker sin presentar errores.
- [ ] Se permite la conexión y desconexión de las Queries sin presentar errores.
- [ ] El planificador respeta las prioridades definidas.
- [ ] El aging se aplica de manera correcta.
- [ ] El master reenvía los mensajes desde el Worker a la query sin problemas.

**Módulo Worker**
- [ ] Interpreta correctamente las instrucciones definidas.
- [ ] Realiza las traducciones de direcciones lógicas a físicas sin presentar errores.
- [ ] Se respetan los retardos de accesos a memoria.
- [ ] Se respeta el algoritmo LRU al momento de reemplazar las páginas.
- [ ] Se respeta el algoritmo CLOCK-M al momento de reemplazar las páginas.
- [ ] Es capaz de recibir correctamente las interrupciones del Kernel.

**Módulo Storage**
- [ ] Respeta la estructura definida.
- [ ] Actualiza correctamente las estructuras administrativas.
- [ ] Mantiene el estado del FS luego de un reinicio.
- [ ] Aplica correctamente el FRESH_START.
- [ ] Respeta los errores definidos y los informa de manera correcta.
- [ ] Calcula correctamente los hash.