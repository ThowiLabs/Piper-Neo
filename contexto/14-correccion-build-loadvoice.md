# Fecha

18 de junio de 2026

# Objetivo

Corregir el fallo de compilación detectado después del refactor del sanitizer, antes de hacer commit, manteniendo el repo público de Piper Neo compilable.

# Decisiones tomadas

- No se revierte el refactor del sanitizer porque el error real no está en `text_sanitizer.cpp`.
- `loadVoice()` no necesita modificar el `speakerId` recibido; solo lo lee para copiarlo a `voice.synthesisConfig.speakerId` si el modelo tiene múltiples speakers.
- La firma pública de `loadVoice()` se ajusta para recibir `const std::optional<SpeakerId>&`.
- `prepareVoiceRuntime()` conserva `const RunConfig&`, porque no debe mutar configuración de ejecución al preparar la voz.

# Arquitectura actual

- `src/cpp/app/voice_runtime.cpp` prepara rutas, carga `.neo` temporal si aplica, llama a `loadVoice()`, configura eSpeak/tashkeel e inicializa Piper.
- `src/cpp/piper/api.hpp` expone la API pública del core.
- `src/cpp/core/voice_loader.cpp` implementa `loadVoice()` y carga configuración/modelo.

# Librerías usadas

No se agregaron dependencias nuevas. Se mantiene C++17, standard library, spdlog, ONNX Runtime, piper-phonemize y espeak-ng.

# Archivos importantes modificados

- `src/cpp/piper/api.hpp`
- `src/cpp/core/voice_loader.cpp`
- `script/smoke-project-structure.py`
- `contexto/README.md`
- `contexto/14-correccion-build-loadvoice.md`

# Problemas encontrados

El build de Windows fallaba en `src/cpp/app/voice_runtime.cpp` porque `prepareVoiceRuntime()` recibe `const RunConfig&`, pero pasaba `runConfig.speakerId` a `loadVoice()`. La firma de `loadVoice()` exigía `std::optional<SpeakerId>&`, es decir una referencia mutable.

# Soluciones implementadas

- Cambiar la firma de `loadVoice()` a `const std::optional<SpeakerId>&` en header e implementación.
- Mantener el comportamiento: si hay `speakerId`, se usa; si no hay y el modelo tiene múltiples speakers, se usa speaker `0`.
- Actualizar el smoke estructural para verificar que la API no vuelva a pedir una referencia mutable innecesaria.

# Pendientes

- Probar build completo en Windows con `py script\build-windows.py clean` y `py script\build-windows.py`.
- Vigilar si aparecen errores posteriores que el build no alcanzó a mostrar por detenerse en el primer fallo.

# Próximos pasos

Después de confirmar el build, continuar con `markup_tts.cpp` o con pruebas HTTP/CLI antes de seguir moviendo piezas del servidor.
