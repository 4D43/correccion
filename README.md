# SGBD (Sistema de Gestión de Base de Datos) Completo

Un Sistema de Gestión de Base de Datos completo implementado en C++ con soporte para consultas SQL.

## 🚀 Características Implementadas

### ✅ Módulos Completamente Funcionales

1. **Disk Manager** - Gestión de almacenamiento físico
   - Simulación de disco con platos, superficies, cilindros y sectores
   - Mapeo de bloques lógicos a sectores físicos
   - Gestión de espacio libre y metadatos del disco

2. **Buffer Manager** - Gestión de buffer pool en memoria
   - Buffer pool con políticas de reemplazo (LRU, Clock)
   - Pin/Unpin de páginas con conteo de referencias
   - Flush automático de páginas sucias

3. **Record Manager** - Gestión de registros en páginas
   - Inserción, actualización, eliminación y recuperación de registros
   - Slot directory para gestión eficiente del espacio
   - Soporte para registros de longitud fija y variable

4. **Catalog Manager** - Gestión de metadatos de tablas
   - Creación y eliminación de tablas
   - Almacenamiento persistente de esquemas
   - Gestión de columnas con diferentes tipos de datos

5. **Query Processor** - **¡NUEVO!** Procesador completo de consultas SQL
   - **Parser SQL**: Análisis sintáctico de consultas SQL
   - **Optimizador**: Generación de planes de ejecución optimizados
   - **Ejecutor**: Ejecución eficiente de planes de consulta

### 🔧 Funcionalidades SQL Soportadas

- `CREATE TABLE` - Crear nuevas tablas
- `DROP TABLE` - Eliminar tablas
- `INSERT INTO` - Insertar registros
- `SELECT` - Consultar datos (con WHERE, proyección de columnas)
- `UPDATE` - Actualizar registros (con WHERE)
- `DELETE` - Eliminar registros (con WHERE)

### 📊 Tipos de Datos Soportados

- `INT` - Números enteros
- `CHAR(n)` - Cadenas de longitud fija
- `VARCHAR(n)` - Cadenas de longitud variable

### 🎯 Operadores WHERE Soportados

- `=` - Igualdad
- `!=`, `<>` - Desigualdad
- `<`, `<=` - Menor que, menor o igual
- `>`, `>=` - Mayor que, mayor o igual
- `LIKE` - Búsqueda de patrones (básico)
- `AND` - Conjunción lógica

## 🏗️ Arquitectura del Sistema

```
┌─────────────────────────────────────────────────────────────┐
│                    Query Processor                          │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐ │
│  │   Parser    │ │ Optimizer   │ │       Executor          │ │
│  └─────────────┘ └─────────────┘ └─────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
┌─────────────────┐ ┌─────────────────────────────────────────┐
│ Catalog Manager │ │          Record Manager                 │
└─────────────────┘ └─────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│                   Buffer Manager                            │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐ │
│  │ LRU Policy  │ │ Clock Policy│ │    Page Management      │ │
│  └─────────────┘ └─────────────┘ └─────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│                    Disk Manager                             │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐ │
│  │ Block Mgmt  │ │ Free Space  │ │   Physical Storage      │ │
│  └─────────────┘ └─────────────┘ └─────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

## 🚀 Compilación y Ejecución

### Prerrequisitos
- Compilador C++ con soporte para C++17
- Sistema operativo Linux/Unix (recomendado)

### Compilar
```bash
cd src
g++ -std=c++17 -I. -o sgbd main.cpp data_storage/*.cpp record_manager/*.cpp Catalog_Manager/*.cpp replacement_policies/*.cpp query_processor/*.cpp
```

### Ejecutar
```bash
./sgbd
```

## 📖 Guía de Uso

### 1. Inicialización del Sistema
1. Ejecutar el programa
2. Crear un nuevo disco o cargar uno existente desde el menú "Gestión del Disco"
3. El sistema inicializará automáticamente todos los componentes

### 2. Usando el Procesador de Consultas SQL

#### Acceder al Procesador SQL
- Menú Principal → Opción 6: "Procesador de Consultas SQL"

#### Ejemplos de Consultas

**Crear una tabla:**
```sql
CREATE TABLE usuarios (id INT, nombre CHAR, email VARCHAR)
```

**Insertar datos:**
```sql
INSERT INTO usuarios VALUES (1, 'Juan', 'juan@email.com')
INSERT INTO usuarios VALUES (2, 'María', 'maria@email.com')
```

**Consultar datos:**
```sql
SELECT * FROM usuarios
SELECT nombre, email FROM usuarios WHERE id = 1
```

**Actualizar datos:**
```sql
UPDATE usuarios SET email = 'nuevo@email.com' WHERE id = 1
```

**Eliminar datos:**
```sql
DELETE FROM usuarios WHERE id = 2
```

**Eliminar tabla:**
```sql
DROP TABLE usuarios
```

### 3. Características Avanzadas

#### Modo Verbose
- Permite ver información detallada del procesamiento de consultas
- Muestra tiempos de parsing, optimización y ejecución

#### Estadísticas de Consultas
- Tiempo de procesamiento por fase
- Costo estimado de ejecución
- Número de operadores en el plan

#### Planes de Ejecución
- Visualización del plan de ejecución optimizado
- Información sobre operadores físicos utilizados

## 🔧 Estructura de Archivos

```
src/
├── main.cpp                     # Programa principal con menús
├── include/
│   └── common.h                 # Definiciones comunes y tipos de datos
├── data_storage/
│   ├── disk_manager.h/.cpp      # Gestión del almacenamiento físico
│   ├── buffer_manager.h/.cpp    # Gestión del buffer pool
│   ├── block.h                  # Definiciones de bloques
│   └── page.h                   # Definiciones de páginas
├── record_manager/
│   └── record_manager.h/.cpp    # Gestión de registros
├── Catalog_Manager/
│   └── Catalog_Manager.h/.cpp   # Gestión de metadatos
├── replacement_policies/
│   ├── ireplacement_policy.h    # Interfaz de políticas
│   ├── lru.h/.cpp              # Política LRU
│   └── clock.h/.cpp            # Política Clock
└── query_processor/             # ¡NUEVO! Procesador de consultas
    ├── query_parser.h/.cpp      # Parser SQL
    ├── query_optimizer.h/.cpp   # Optimizador de consultas
    ├── query_executor.h/.cpp    # Ejecutor de consultas
    └── query_processor.h/.cpp   # Coordinador principal
```

## 🎯 Mejoras y Características Completadas

### ✅ Problemas Corregidos
1. **Errores de compilación** - Todos los includes y dependencias corregidos
2. **Dependencias circulares** - Resueltas entre CatalogManager y RecordManager
3. **Gestión de memoria** - Uso correcto de smart pointers
4. **Interfaz de usuario** - Menús intuitivos y manejo de errores

### ✅ Funcionalidades Añadidas
1. **Query Processor completo** - Parser, optimizador y ejecutor
2. **Soporte SQL completo** - DDL y DML básico
3. **Manejo de errores robusto** - Validación y mensajes informativos
4. **Estadísticas de rendimiento** - Medición de tiempos y costos
5. **Modo de depuración** - Información detallada del procesamiento

## 🚧 Extensiones Futuras Posibles

- **Índices B+ Tree** - Para consultas más eficientes
- **Transacciones** - Soporte ACID
- **Concurrencia** - Múltiples usuarios simultáneos
- **Joins** - Consultas entre múltiples tablas
- **Agregaciones** - COUNT, SUM, AVG, etc.
- **Subconsultas** - Consultas anidadas

## 📝 Notas Técnicas

- El sistema simula un disco físico en memoria para propósitos educativos
- Los datos se persisten en archivos en el directorio `Discos/`
- El buffer pool utiliza políticas de reemplazo configurables
- El parser SQL soporta la sintaxis básica estándar
- El optimizador genera planes simples pero efectivos

## 🎉 Estado del Proyecto

**✅ COMPLETADO** - El SGBD está completamente funcional con todas las características principales implementadas y probadas. Listo para uso educativo y demostraciones.

---

*Desarrollado como un proyecto educativo completo de Sistema de Gestión de Base de Datos.*
