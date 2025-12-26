# Master of Files - Comandos para Pruebas en VM

## CONFIGURACION DE RED

| PC | Modulo(s) | IP |
|----|-----------|-----|
| PC1 | Master + Query Control | 192.168.1.173 |
| PC2 | Worker | 192.168.1.204 |
| PC3 | Storage | 192.168.1.59 |

---

## CREDENCIALES GIT EN LA VM

```
user: santiagoquiroga04
pass: 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
```

---

## CLONACION RAPIDA (en cada PC)

```bash
git clone --depth 1 --branch main https://github.com/sisoputnfrba/tp-2025-2c-Los-CuervosXeneizes.git
cd tp-2025-2c-Los-CuervosXeneizes
```

---

## COMPILACION (en cada PC)

```bash
# Compilar todo el proyecto (desde la raiz)
make all

# O compilar modulos individualmente
make utils
make master
make storage
make worker
make query_control

# Limpiar y recompilar
make rebuild
```

---

## CAMBIO DE IPs EN ARCHIVOS DE CONFIG

### En PC1 (Master + Query Control) - IP: 192.168.1.173

Ejecutar UNA SOLA VEZ en cualquier terminal (modifica los archivos .config):

```bash
cd tp-2025-2c-Los-CuervosXeneizes

# Cambiar IP_MASTER en configs de query_control (apunta a si mismo)
sed -i 's/IP_MASTER=127\.0\.0\.1/IP_MASTER=192.168.1.173/g' query_control/configs/*.config
```

Despues en cada terminal:

```bash
# Terminal 1 - Master
cd tp-2025-2c-Los-CuervosXeneizes/master
./bin/master configs/PRUEBA_XXX.config

# Terminal 2, 3, 4... - Query Control (una terminal por cada query)
cd tp-2025-2c-Los-CuervosXeneizes/query_control
./bin/query_control configs/XXX.config
```

### En PC2 (Worker) - IP: 192.168.1.204

```bash
cd tp-2025-2c-Los-CuervosXeneizes

# Cambiar IP_MASTER (apunta a PC1)
sed -i 's/IP_MASTER=127\.0\.0\.1/IP_MASTER=192.168.1.173/g' worker/configs/*.config

# Cambiar IP_STORAGE (apunta a PC3)
sed -i 's/IP_STORAGE=127\.0\.0\.1/IP_STORAGE=192.168.1.59/g' worker/configs/*.config
```

### En PC3 (Storage) - IP: 192.168.1.59

No requiere cambios de IP (Storage es servidor, no cliente).

---

## VERIFICAR CAMBIOS

```bash
# En PC1 (Query Control)
grep "IP_" query_control/configs/*.config

# En PC2 (Worker)
grep "IP_" worker/configs/*.config
```

---

---

# PRUEBAS

---

## PRUEBA 1: Planificacion

### Terminales necesarias: 8 (distribuidas en 3 PCs)

| PC | Terminal | Modulo |
|----|----------|--------|
| PC3 | 1 | Storage |
| PC1 | 2 | Master |
| PC2 | 3 | Worker 1 |
| PC2 | 4 | Worker 2 |
| PC1 | 5-8 | Query Control (4 queries) |

### Parte 1 - FIFO

**PC3 - Storage:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/storage
./bin/storage configs/PRUEBA_FIFO.config
```

**PC1 - Master:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/master
./bin/master configs/PRUEBA_FIFO.config
```

**PC2 - Worker 1:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/worker
./bin/worker configs/PRUEBA_PLANI1.config 1
```

**PC2 - Worker 2 (otra terminal):**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/worker
./bin/worker configs/PRUEBA_PLANI2.config 2
```

**PC1 - Ejecutar las 4 Queries en orden rapido (4 terminales diferentes):**

```bash
# Terminal 5 - Query 1 (FIFO_1, prioridad 4)
cd tp-2025-2c-Los-CuervosXeneizes/query_control
./bin/query_control configs/FIFO1.config pruebas/FIFO_1 4

# Terminal 6 - Query 2 (FIFO_2, prioridad 3)
cd tp-2025-2c-Los-CuervosXeneizes/query_control
./bin/query_control configs/FIFO2.config pruebas/FIFO_2 3

# Terminal 7 - Query 3 (FIFO_3, prioridad 5)
cd tp-2025-2c-Los-CuervosXeneizes/query_control
./bin/query_control configs/FIFO3.config pruebas/FIFO_3 5

# Terminal 8 - Query 4 (FIFO_4, prioridad 1)
cd tp-2025-2c-Los-CuervosXeneizes/query_control
./bin/query_control configs/FIFO4.config pruebas/FIFO_4 1
```

**Esperar que finalicen las 4 queries y luego bajar todos los modulos (Ctrl+C).**

---

### Parte 2 - PRIORIDADES con Aging

**PC3 - Storage:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/storage
./bin/storage configs/PRUEBA_AGING.config
```

**PC1 - Master:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/master
./bin/master configs/PRUEBA_AGING.config
```

**PC2 - Worker 1:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/worker
./bin/worker configs/PRUEBA_PLANI1.config 1
```

**PC2 - Worker 2:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/worker
./bin/worker configs/PRUEBA_PLANI2.config 2
```

**PC1 - Ejecutar las 4 Queries AGING en orden rapido:**

```bash
# Terminal 5 - Query 1 (AGING_1, prioridad 4)
cd tp-2025-2c-Los-CuervosXeneizes/query_control
./bin/query_control configs/AGING1.config pruebas/AGING_1 4

# Terminal 6 - Query 2 (AGING_2, prioridad 3)
cd tp-2025-2c-Los-CuervosXeneizes/query_control
./bin/query_control configs/AGING2.config pruebas/AGING_2 3

# Terminal 7 - Query 3 (AGING_3, prioridad 5)
cd tp-2025-2c-Los-CuervosXeneizes/query_control
./bin/query_control configs/AGING3.config pruebas/AGING_3 5

# Terminal 8 - Query 4 (AGING_4, prioridad 1)
cd tp-2025-2c-Los-CuervosXeneizes/query_control
./bin/query_control configs/AGING4.config pruebas/AGING_4 1
```

### Resultado esperado

- **Parte FIFO**: Las queries se ejecutan en el orden en que llegaron (sin importar la prioridad).
- **Parte AGING/PRIORIDADES**: Las queries se ejecutan respetando las prioridades, y con el tiempo de aging (500ms) las prioridades se van incrementando, generando desalojos cuando una query de menor prioridad "envejece" y supera a otra.

---

---

## PRUEBA 2: Memoria Worker

### Terminales necesarias: 4

| PC | Terminal | Modulo |
|----|----------|--------|
| PC3 | 1 | Storage |
| PC1 | 2 | Master |
| PC2 | 3 | Worker |
| PC1 | 4 | Query Control |

### Parte 1 - CLOCK-M

**PC3 - Storage:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/storage
./bin/storage configs/PRUEBA_MEMORIA-WORKER.config
```

**PC1 - Master:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/master
./bin/master configs/PRUEBA_MEMORIA-WORKER.config
```

**PC2 - Worker:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/worker
./bin/worker configs/PRUEBA_MEMORIA-WORKER_CLOCK.config 1
```

**PC1 - Query:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/query_control
./bin/query_control configs/MEMORIA-WORKER1.config pruebas/MEMORIA_WORKER 0
```

**Esperar que finalice, bajar todos los modulos (Ctrl+C).**

---

### Parte 2 - LRU

**PC3 - Storage:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/storage
./bin/storage configs/PRUEBA_MEMORIA-WORKER.config
```

**PC1 - Master:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/master
./bin/master configs/PRUEBA_MEMORIA-WORKER.config
```

**PC2 - Worker:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/worker
./bin/worker configs/PRUEBA_MEMORIA-WORKER_LRU.config 1
```

**PC1 - Query:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/query_control
./bin/query_control configs/MEMORIA-WORKER2.config pruebas/MEMORIA_WORKER_2 0

### Que verificar

- La query finaliza correctamente en ambos casos
- Los reemplazos de paginas siguen el algoritmo configurado (CLOCK-M o LRU)

---

---

## PRUEBA 3: Errores

### Terminales necesarias: 4

| PC | Terminal | Modulo |
|----|----------|--------|
| PC3 | 1 | Storage |
| PC1 | 2 | Master |
| PC2 | 3 | Worker |
| PC1 | 4 | Query Control |

**PC3 - Storage:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/storage
./bin/storage configs/PRUEBA_ERRORES.config
```

**PC1 - Master:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/master
./bin/master configs/PRUEBA_ERRORES.config
```

**PC2 - Worker:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/worker
./bin/worker configs/PRUEBA_ERRORES.config 1
```

**PC1 - Ejecutar una por una las siguientes queries:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/query_control

# Escritura archivo commited
./bin/query_control configs/ERRORES-ESCRITURA.config pruebas/ESCRITURA_ARCHIVO_COMMITED 1

# File existente
./bin/query_control configs/ERRORES-FILE.config pruebas/FILE_EXISTENTE 1

# Lectura fuera del limite
./bin/query_control configs/ERRORES-LECTURA.config pruebas/LECTURA_FUERA_DEL_LIMITE 1

# Tag existente
./bin/query_control configs/ERRORES-TAG.config pruebas/TAG_EXISTENTE 1
```


### Que verificar

- Cada query termina con su error correspondiente (no se rompe el sistema)

---

---

## PRUEBA 4: Storage

### Terminales necesarias: 4

| PC | Terminal | Modulo |
|----|----------|--------|
| PC3 | 1 | Storage |
| PC1 | 2 | Master |
| PC2 | 3 | Worker |
| PC1 | 4 | Query Control |

**PC3 - Storage:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/storage
./bin/storage configs/PRUEBA_STORAGE.config
```

**PC1 - Master:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/master
./bin/master configs/PRUEBA_STORAGE.config
```

**PC2 - Worker:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/worker
./bin/worker configs/PRUEBA_STORAGE.config 1
```

**PC1 - Ejecutar en orden:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/query_control

./bin/query_control configs/STORAGE1.config pruebas/STORAGE_1 0
./bin/query_control configs/STORAGE2.config pruebas/STORAGE_2 2
./bin/query_control configs/STORAGE3.config pruebas/STORAGE_3 4
./bin/query_control configs/STORAGE4.config pruebas/STORAGE_4 6

# Validar estado del FS
# Luego ejecutar:
./bin/query_control configs/STORAGE5.config pruebas/STORAGE_5 0
```

### Que verificar

- Se observa el uso correcto de bloques
- Se observa la deduplicacion al hacer commit de archivos

---

---

## PRUEBA 5: Estabilidad General

### Terminales necesarias: 9+

| PC | Terminal | Modulo |
|----|----------|--------|
| PC3 | 1 | Storage |
| PC1 | 2 | Master |
| PC2 | 3-4 | Workers 1 y 2 (iniciales) |
| PC2 | 5-6 | Workers 3 y 4 (agregar despues de 30-60 seg) |
| PC2 | 7-8 | Workers 5 y 6 (agregar despues de otros 30-60 seg) |
| PC1 | 9+ | Queries (muchas) |

**PC3 - Storage:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/storage
./bin/storage configs/PRUEBA_ESTABILIDAD-GENERAL.config
```

**PC1 - Master:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/master
./bin/master configs/PRUEBA_ESTABILIDAD-GENERAL.config
```

**PC2 - Worker 1:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/worker
./bin/worker configs/PRUEBA_ESTABILIDAD-GENERAL1.config 1
```

**PC2 - Worker 2:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/worker
./bin/worker configs/PRUEBA_ESTABILIDAD-GENERAL2.config 2
```

**PC1 - Ejecutar 25 instancias de CADA query AGING (100 queries total):**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/query_control

for i in {1..25}; do ./bin/query_control configs/AGING1.config pruebas/AGING_1 20 & done
for i in {1..25}; do ./bin/query_control configs/AGING2.config pruebas/AGING_2 20 & done
for i in {1..25}; do ./bin/query_control configs/AGING3.config pruebas/AGING_3 20 & done
for i in {1..25}; do ./bin/query_control configs/AGING4.config pruebas/AGING_4 20 & done

**Esperar 30-60 segundos e iniciar Workers 3 y 4:**

**PC2 - Worker 3:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/worker
./bin/worker configs/PRUEBA_ESTABILIDAD-GENERAL3.config 3
```

**PC2 - Worker 4:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/worker
./bin/worker configs/PRUEBA_ESTABILIDAD-GENERAL4.config 4
```

**Esperar 30-60 segundos y finalizar Workers 1 y 2 (Ctrl+C)**

**Esperar 30-60 segundos e iniciar Workers 5 y 6:**

**PC2 - Worker 5:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/worker
./bin/worker configs/PRUEBA_ESTABILIDAD-GENERAL5.config 5
```

**PC2 - Worker 6:**

```bash
cd tp-2025-2c-Los-CuervosXeneizes/worker
./bin/worker configs/PRUEBA_ESTABILIDAD-GENERAL6.config 6
```

**Esperar que terminen todas las queries.**

### Que verificar

- No hay esperas activas (CPU no al 100%)
- No hay memory leaks
- El sistema no se cae abruptamente
- Todas las queries terminan correctamente

---

---

# RESUMEN DE COMANDOS RAPIDOS

## Cambiar IPs (ejecutar UNA VEZ en cada PC despues de clonar)

### PC1 (Master + Query Control) - 192.168.1.173

Ejecutar UNA VEZ (en cualquier terminal):

```bash
cd tp-2025-2c-Los-CuervosXeneizes
sed -i 's/IP_MASTER=127\.0\.0\.1/IP_MASTER=192.168.1.173/g' query_control/configs/*.config
make all
```

Luego abrir 2+ terminales:
- **Terminal 1**: para ejecutar Master
- **Terminal 2, 3, ...**: para ejecutar Query Control (una por query)

### PC2 (Worker) - 192.168.1.204

Ejecutar UNA VEZ (en cualquier terminal):

```bash
cd tp-2025-2c-Los-CuervosXeneizes
sed -i 's/IP_MASTER=127\.0\.0\.1/IP_MASTER=192.168.1.173/g' worker/configs/*.config
sed -i 's/IP_STORAGE=127\.0\.0\.1/IP_STORAGE=192.168.1.59/g' worker/configs/*.config
make all
```

Luego abrir 1+ terminales (una por cada Worker que quieras levantar)

### PC3 (Storage) - 192.168.1.59

Ejecutar UNA VEZ:

```bash
cd tp-2025-2c-Los-CuervosXeneizes
make all
```

Solo necesitas 1 terminal para Storage

---

## Terminales por Prueba

| Prueba | Terminales PC1 | Terminales PC2 | Terminales PC3 |
|--------|----------------|----------------|----------------|
| 1. Planificacion | 5 (Master + 4 QC) | 2 (Workers) | 1 (Storage) |
| 2. Memoria Worker | 2 (Master + QC) | 1 (Worker) | 1 (Storage) |
| 3. Errores | 2 (Master + QC) | 1 (Worker) | 1 (Storage) |
| 4. Storage | 2 (Master + QC) | 1 (Worker) | 1 (Storage) |
| 5. Estabilidad | 2+ (Master + QCs) | 6 (Workers) | 1 (Storage) |

---

## Orden de inicio (SIEMPRE)

1. **PC3**: Iniciar Storage
2. **PC1**: Iniciar Master
3. **PC2**: Iniciar Worker(s)
4. **PC1**: Ejecutar Query Control(s)

---

## Notas Importantes

1. El `LOG_LEVEL` siempre debe estar en `INFO`
2. En ambiente distribuido, **siempre verificar conectividad** entre PCs (`ping`)
3. El `FRESH_START` del Storage debe estar en `TRUE` para limpiar el FS antes de cada prueba
4. Los Workers necesitan el ID como segundo parametro: `./bin/worker config.config <ID>`

---

## Troubleshooting

### Verificar puertos abiertos

```bash
# En PC1 (Master)
ss -tlnp | grep 9001

# En PC3 (Storage)
ss -tlnp | grep 9002
```

### Verificar conectividad

```bash
# Desde PC2 (Worker) hacia Master
nc -zv 192.168.1.173 9001

# Desde PC2 (Worker) hacia Storage
nc -zv 192.168.1.59 9002
```

### Si hay problemas de firewall

```bash
# Abrir puerto en firewall (ejecutar en PC1 y PC3)
sudo ufw allow 9001/tcp
sudo ufw allow 9002/tcp
```
