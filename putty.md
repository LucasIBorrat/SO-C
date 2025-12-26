Para conectarte a las VMs desde Windows con PuTTY:

## Configuracion de PuTTY

### 1. En "Host Name (or IP address)" poner la IP de la VM a la que te quieras conectar:

| Para conectarte a... | IP |
|---------------------|-----|
| PC1 (Master) | `192.168.1.136` |
| PC2 (Worker) | `192.168.1.204` |
| PC3 (Storage) | `192.168.1.59` |

### 2. Dejar el resto asi:
- **Port**: `22`
- **Connection type**: `SSH` (ya seleccionado)

### 3. Click en "Open"

### 4. En la terminal que se abre:
```
login as: utnso
password: utnso
```

---

## Guardar la sesion (opcional pero recomendado)

Para no tener que escribir la IP cada vez:

1. Escribir la IP en "Host Name"
2. En "Saved Sessions" escribir un nombre (ej: `Master`, `Worker`, `Storage`)
3. Click en **Save**
4. La proxima vez solo haces doble click en el nombre guardado

---

## Ejemplo para PC3 (Storage):

```
Host Name: 192.168.1.59
Port: 22
Connection type: SSH
Saved Sessions: Storage
[Click Save]
[Click Open]
```

Luego te pide usuario y contraseña: `utnso` / `utnso`