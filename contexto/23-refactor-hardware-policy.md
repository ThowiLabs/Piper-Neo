# Fecha

19 de junio de 2026

# Objetivo

Separar la detección de hardware de la política automática de recursos del servidor para que `hardware.cpp` no mezcle CPU, RAM, cgroups, perfiles y logging.

# Decisiones tomadas

- Mantener `applyAutoServerResourceConfig()` como API pública de la capa app.
- Mover la detección real de CPU/RAM/cgroups a `hardware_probe.*`.
- Mover la política pura de recursos a `resource_policy.*`.
- Agregar `test_resource_policy` para validar presupuestos temporales, límite de réplicas, defaults y clamps de overrides.
- No cambiar nombres de flags CLI ni defaults públicos.

# Arquitectura actual

- `hardware.cpp`: obtiene hardware detectado, aplica la política y registra el resultado.
- `hardware_probe.*`: detecta threads disponibles y memoria disponible con soporte Linux/cgroups.
- `resource_policy.*`: calcula `cpuThreads`, `chunkWorkers`, `maxConcurrentJobs`, `maxModelReplicas`, `queueSize` y `maxTempBytes`.
- `test_resource_policy.cpp`: prueba reglas puras sin depender de hardware real.

# Librerías usadas

C++17 y librería estándar. `hardware.cpp` mantiene `spdlog` solo para logging final.

# Archivos importantes modificados

- `src/cpp/app/hardware.cpp`
- `src/cpp/app/hardware_probe.hpp`
- `src/cpp/app/hardware_probe.cpp`
- `src/cpp/app/resource_policy.hpp`
- `src/cpp/app/resource_policy.cpp`
- `src/cpp/tests/test_resource_policy.cpp`
- `CMakeLists.txt`
- `script/smoke-project-structure.py`

# Problemas encontrados

La política de recursos era difícil de probar porque dependía directamente del hardware real y de detalles de Linux/cgroups.

# Soluciones implementadas

Se separó el probe del cálculo puro. Ahora las reglas automáticas pueden probarse con valores controlados sin tocar el entorno real.

# Pendientes

- Validar perfiles `eco`, `balanced`, `fast`, `max` en Windows con cargas reales.
- Revisar si se debe documentar cada perfil en `docs/server-api.md` o README.
- Evaluar métricas de uso real para ajustar defaults si hay sobrecarga en máquinas pequeñas.

# Próximos pasos

Ejecutar build real Windows y `test_resource_policy` junto con los demás tests de CMake.
