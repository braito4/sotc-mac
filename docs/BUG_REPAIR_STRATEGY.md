# Estrategia de corrección: shaders, SIGBUS y bloqueos GPU

Revisión: 25 de octubre de 2026. Objetivo: convertir cada fallo reproducido en una
corrección comprobable. No se puede garantizar encontrar todos los bugs ni fijar un
número de ejecuciones necesario. Sí podemos evitar rondas sin información y definir
qué evidencia permite avanzar. Este documento no declara resueltos los fallos del juego.

## Cambio de base autorizado

El propietario ha priorizado incorporar todas las actualizaciones revisadas antes
que conservar los fallos actuales. Se aplica runtime `84daa9f13e5c`, CPU
`196ed99d961f` y referencia SotC `40ec52a5a246` (0.60.6), con las integraciones y
sondas adaptadas. La base anterior queda como punto de retorno y oráculo de
regresión; sus resultados no se mezclan con los de la actualización.

La primera ejecución nueva es una medición base de 500 segundos. Clasificar su
primer fallo por la etapa más temprana demostrada; abrir o vincular su caso por
contenido, etapa e identidad, sin exigir reproducir las antiguas direcciones.
SHADER-001 permanece documentado, pero no bloquea el análisis de fallos nuevos.
Mantener la metodología y las evidencias importa más que mantener el mismo crash.

## Diagnóstico de la investigación actual

El censo privado contiene 39 capturas de rechazo de metadatos: 33 por ausencia de
prólogo y 6 por metadatos incompatibles con el desplazamiento del prólogo. Son
capturas guardadas, no 39 bugs ni una tasa de fallo por ejecución. En la ronda 36,
el desplazamiento calculado apunta a bytes que no contienen la firma esperada;
una firma posterior no demuestra que pertenezca al shader. No hay motivo para
aceptarla como sustitución ni para culpar todavía al compilador.

La evidencia de una transición válida→inválida en una misma dirección no acredita
que siga siendo la misma asignación. Además, las rondas 35–36 perdieron decenas de
miles de eventos relevantes. La ausencia de una escritura en ese historial no la
descarta. Se han añadido resúmenes por dirección que conservan los contadores de
registro/renovación/expulsión, aunque rote el historial detallado; siguen existiendo
límites de tabla, contención y desconocimiento de la identidad de asignación.

La ronda 37 es otra dependencia: un comando Metal enviado y programado no terminó
en el intervalo del watchdog. Esto es distinto del límite total de 500 segundos.
No identifica el comando GPU culpable ni prueba falta de memoria. Su captura está
sellada y su resultado continúa marcado como no apto para reinicio automático.

## Orden de trabajo y criterios de avance

| Paso | Trabajo concreto | Evidencia necesaria para pasar |
|---|---|---|
| 0. Recuperación GPU | Revisar salida de procesos y estado del dispositivo; ejecutar el chequeo nativo existente desde la sesión gráfica del propietario después de recuperación | Chequeo de dispositivo correcto, sin proceso huérfano; registrar recuperación sin sobrescribir el resultado del stall |
| 1. Cobertura | Una ejecución base con resumen nuevo; consultar la dirección del fallo de ESA ejecución | Registro retenido, contadores/pérdidas explícitos y unión por paquete/captura; si falta, reparar esa pérdida antes de repetir |
| 2. Primera divergencia | Comparar bytes y dirección al registrar, encolar y consumir; separar instancias y reutilización | Último estado válido y primero inválido compatibles, o demostración de que nunca fue una entrada válida |
| 3. Causa | Instrumentar únicamente el intervalo y las rutas de escritura/vida útil que cubren la divergencia | Operación concreta y relación temporal/identidad; si hay hueco o ruta sin cubrir, resultado desconocido |
| 4. Reproductor | Reducir la operación a una prueba sintética original; usar replay privado si necesita bytes capturados | Mismo invariante roto antes del parche, sin depender de otra excepción |
| 5. Corrección | Cambiar el productor, dirección, límites, vida útil o traducción que demuestra el reproductor | Prueba anterior falla y nueva pasa; controles negativos y compilación relevante pasan |
| 6. Cierre | Reejecutar la ruta afectada con el cambio y configuración documentada | Al menos tres exposiciones verificadas sin recurrencia vinculada, más el reproductor causal; no una garantía estadística |

La ronda 37 bloquea solo nuevas ejecuciones nativas hasta recuperar el dispositivo;
no bloquea análisis, implementación ni pruebas sintéticas. SHADER-001 sigue abierto
si aparece SIGSEGV, SIGBUS o un timeout: se conserva el fallo y se retoma el caso,
resolviendo únicamente la dependencia que impide avanzar.

## Árbol de decisión de shaders

1. **Dirección incorrecta desde la entrada:** comparar dirección del registro/paquete,
   base, desplazamiento y tamaño con la dirección consumida. Una discrepancia exige
   comprobar extracción de campos, copia y propiedad del paquete antes del decoder.
2. **Dirección coherente, contenido cambia:** separar escritura real de reutilización.
   Un identificador de vigilancia no sustituye a la generación de la asignación.
   Registrar asignación/liberación donde haya un proveedor verificable; donde no lo
   haya, mantener identidad desconocida. Correlacionar copias CPU, carga, escrituras
   observables y destinos GPU con intervalos solapados. Una intención GPU no demuestra
   los bytes escritos ni su visibilidad CPU.
3. **Entrada estable pero metadatos rechazados:** comprobar el contrato de formato y
   sus límites con una prueba independiente. No buscar una firma posterior y aceptarla
   automáticamente. Una lectura incompleta no es una firma inválida demostrada.
4. **Entrada válida y estable, traducción falla:** reproducir offline decoder → IR →
   SPIR-V → MSL/pipeline, conservando hash, tamaño, etapa, opciones y mensaje exacto.
   Clasificar por la primera etapa que falla, no agrupar todos como «error de shader».
5. **Pipeline válido, ejecución GPU no termina:** investigar recursos, barreras y
   ciclo de vida con los IDs de envío/completado. Reducir un caso reproducible;
   cambiar la fragmentación de envíos solo como experimento controlado, pues modifica
   temporización. No atribuir al último shader registrado toda la cola pendiente.

## Instrumentación: aumentar evidencia útil, no volumen

Disponibles: historial de ciclo de vigilancia, resumen por dirección, observaciones
CPU recientes por hilo, capturas de metadatos, procedencia/copias/bindings y registros
de envío/completado. La batería sintética de ARM64 pasó con el nuevo resumen.
La validación de su retención bajo carga real sigue pendiente.

Siguiente incremento si la nueva ejecución muestra otro hueco: reservar por candidato
la primera referencia, primera discrepancia y una cola corta de eventos; incluir
causa de admisión/expulsión y cobertura. Después, añadir la observación del extremo
que falte entre registro, encolado y consumo. No desplegar todas las sondas a la vez.
Cada probe debe especificar pregunta, dato, límite, contador de pérdida, coste y
condición de retirada. Conservar el primer fallo antes de que el tráfico lo tape.

Medir cobertura por etapa y familia observada, pérdidas y nuevos datos por ronda.
Medir tiempo/carga con controles equivalentes; un incremento de duración no demuestra
más cobertura. Clasificar siempre: evidencia a favor, evidencia en contra con cobertura,
o inconcluso. Una ejecución interrumpida por otro fallo no refuta la hipótesis.

## SIGBUS y SIGSEGV

SIGBUS conserva su cola de reparación: enlazar objeto, puntero hijo, campo índice de
32 bits, multiplicación y base de tabla por hilo/instancia, no solo proximidad temporal.
Distinguir valor incorrecto, objeto reutilizado y acceso inválido a memoria mapeada.
La lectura diagnóstica de 8 bytes no acredita que fallase la lectura original de 4.
Si es un fallo del host, usar también dirección, acceso y mapa del host. No limitar
el índice artificialmente ni sustituir punteros: reproducir y corregir la operación.

SIGSEGV permanece exclusivamente en conservación y clasificación de evidencias,
según la política del propietario. No se usa esta estrategia para reabrir su análisis
causal. La posible coincidencia con otro fallo se conserva sin atribuir causalidad.

## Disciplina de ejecución y persistencia

- Un juego a la vez, 500 segundos máximos por test y configuración base fija durante
  una hipótesis dirigida. Sin límite total de rondas; máximo siete intentos por hipótesis.
- No renombrar la misma hipótesis para reiniciar el contador. Al agotarla, registrar
  lo descartado, lo desconocido y la siguiente pregunta discriminante.
- No repetir una prueba sin cambios si el fallo anterior ya demuestra que su historial
  no conserva los datos necesarios. Reparar el instrumento o elegir un observable nuevo.
- Cada ronda conserva binarios/configuración, hipótesis, exposición, resultado,
  pérdidas, clasificación, artefactos y siguiente acción concreta.
- Una pregunta del propietario no detiene el controlador. Un timeout cooperativo
  confirmado se analiza y permite continuar; un stall GPU, proceso incierto o fallo
  de validación requiere resolver esa condición. Los tests fallidos permiten reparación offline.
- Al cerrar un caso, retirar sondas de alto coste y conservar el test de regresión,
  el contador mínimo y las evidencias selladas. No acumular probes sin función.

## Actualizaciones externas

Integrar un cambio funcional por vez con contrato, dependencia y prueba. Registrar
por separado revisión consultada, cambio adaptado y versión instalada. La migración completa Metal M2–M4 ha sido solicitada y aplicada como nueva base;
no se interpreta como demostración de haber corregido los metadatos inválidos. Las optimizaciones del recompilador estático tampoco
son automáticamente correcciones del traductor dinámico usado en nuestras pruebas.

Se adapta la propiedad de vistas de presentación del runtime `41f289fe81e0`: la vista
vive en la caché de su imagen, por formato y mapeo alfa, en lugar de destruirse tras una
retirada del scheduler que puede preceder al consumidor Metal. Prueba CPU con los
métodos reales: reproduce el uso de una vista retirada antes y conserva la vista después.
No demuestra solucionar el stall de la ronda 37 (escalado desactivado).


## Instrumentación de dispatches y encoders Metal (26 de octubre)

Con `SOTC_GPU_SUBMIT_TRACE=1`, el informe de bloqueo incluye `GPU native trace`.
Se registra la apertura y la solicitud de cierre de encoders render, compute y blit
administrados por `mtl_frame.mm`, incluidos los render temporales de clears y blits.
Los dispatches directos e indirectos de `MetalFrame::Dispatch*` incluyen el nombre
MSL de entrada (con indicador de truncamiento), identidad local del pipeline,
encoder y dimensiones del grupo de hilos. Los directos incluyen grupos; los
indirectos incluyen buffer Metal y offset resuelto, sin leer sus argumentos:
los ceros de grupos en un indirecto significan desconocido, no trabajo vacío.

Cada recording conserva hasta 256 eventos nativos separados de las colas de dibujos,
con ordinal, total y descartes. La identidad se correlaciona con recording, command
buffer y chunk; un handle puede reutilizarse y no es una identidad entre ejecuciones.
El analizador incorpora la cola en `native_encoding` y señala recortes o informes
incompletos. Los nombres de entrada permiten buscar el shader generado en la caché.

Estos eventos observan codificación CPU, no finalización de encoders ni avance GPU.
No cambian las divisiones de command buffers, sincronización ni argumentos. La traza
puede afectar al tiempo de CPU. Los kernels auxiliares de teselación/geometría y los
upscalers externos no tienen identificación individual por esta ruta; no asumir
cobertura de todos los dispatches nativos. No se debe imputar el bloqueo al último
evento conservado. Primero comprobar pérdidas y correlacionar el chunk pendiente.

Validación: wrapper real con dobles Foundation (opt-in, truncamiento, dimensiones y
orden); journal con desbordamiento, reciclado y análisis de salida real; compilación
Objective-C++ del renderer. La validación de la siguiente captura nativa queda pendiente.


### Parámetros CPU de tiling/detiling

`GPU tiling trace` conserva las últimas 128 observaciones y las primeras 16 con
inconsistencias, con totales y descartes explícitos. Cada observación guarda pipeline
backend, timestamp monotónico, dirección tiling/detiling, grupos, bits por elemento,
num_mips, bank_swizzle, micro_tiled_mips, size/pitch/height/offset de hasta 16 niveles,
y handles/offsets/rangos de los tres bindings (tiled, linear y uniform).

Flags de consistencia: 1=número de niveles fuera de 1..16; 2=bits por elemento
inválidos; 4=tamaño/pitch/height cero; 8=tamaño no divisible por bytes por elemento;
16=rango de mip fuera del binding tiled; 32=invocaciones por encima de la suma de
elementos declarados; 64=invocaciones por encima del rango linear. Son comprobaciones
de parámetros CPU, no una simulación completa del direccionamiento tiled. Cero flags
no prueba validez de los accesos GPU; un flag no demuestra la causa del bloqueo.

La observación se toma de los parámetros CPU ya preparados, sin leer el buffer
uniform ni memoria del juego. `compute_bind` en la traza nativa relaciona
`backend_pipeline` con pipeline Metal y función MSL. Es una relación de candidatos:
los handles y las regiones de staging pueden reutilizarse; no constituye identidad
única de dispatch ni prueba que esos bytes fueran los que leyó la GPU. El analizador
incluye los registros en `tiling_parameters`. Los registros `first_invalid` pueden
repetirse en `tail`: deduplicar por seq dentro de la misma captura. Sigue pendiente
comprobar los parámetros de la siguiente ejecución nativa antes de corregir el kernel.


### Reutilización de parámetros y checkpoints GPU

`GPU parameter reuse` observa cada Map exitoso de StreamBuffer contra las regiones
uniformes de las últimas 128 observaciones de tiling. Registra seq de parámetros,
handle/rango mapeado, tick actual, tick asociado a los parámetros y tick completado.
Conserva 128 solapamientos y los primeros 16 cuyo tick de parámetros seguía pendiente.
Los contadores cubren las comparaciones efectuadas; las regiones expulsadas de la
cola de tiling ya no se vigilan. Map es intención de acceso y no prueba de escritura;
el tick registrado al preparar tiling tampoco garantiza el momento real de lectura
GPU. No cubre otros escritores, destrucción de buffers o reservas sin Map.

`SOTC_GPU_ENCODER_PROGRESS=1` junto a `SOTC_GPU_SUBMIT_TRACE=1` habilita señales
MTLSharedEvent tras endEncoding, en los límites instrumentados de mtl_frame.mm.
Se usan eventos independientes por command buffer, máximo 128 señales, sin esperas
ni submissions adicionales. Las señales pueden modificar tiempos/orden efectivo:
si desaparece el fallo, se requiere comparar con el modo sin señales antes de
interpretarlo como corrección. Por defecto está desactivado.

`Metal progress` muestra enabled/available/encoded/reached/omitted/event por chunk;
los eventos nativos `*_checkpoint` llevan checkpoint y progress_event. Correlacionar
por identidad de evento y chunk, no solo por ordinal o handle de encoder reutilizado.
El valor reached confirma el prefijo de comandos anterior a esa señal, no identifica
la instrucción que falló dentro del siguiente encoder. Sin evento disponible o con
omisiones no asumir cobertura completa. El diario retiene el evento diagnóstico,
pero no prolonga la vida del command buffer ni de sus recursos gráficos.

Semántica documentada por Apple: https://developer.apple.com/documentation/metal/mtlcommandbuffer/encodesignalevent(_:value:)
Solo se insertan señales después de cerrar un encoder. El controlador conserva el
presupuesto de 500 s y su parada cooperativa ante un stall.

Validación: tests Foundation del helper real, opt-in, límite de señales, progreso
simulado y rollover; checks de solapamientos pendientes/completados, límites adyacentes
y otro buffer; parser de la salida real de ambos diarios. Ejecución GPU pendiente.


### Primer render: draws, pipelines y attachments

Con `SOTC_GPU_SUBMIT_TRACE=1`, `GPU render trace` conserva el primer encoder render
regular abierto por RenderEncoder en cada recording. Buffer, recording, command,
encoder y begin_event lo enlazan con el evento nativo render_begin y los checkpoints.
No se incluyen aquí los render temporales de clears/blits. En recordings divididos
en varios chunks se conserva solo el primer render del recording, no uno por chunk.

Se conservan los primeros 32 draws y los últimos 128 restantes, con total y descartes.
El inventario cuenta hasta 64 pipelines Metal distintos y registra los draws cuyo
pipeline no cabe en ese inventario. Incluye entradas vertex/fragment, truncamiento,
topología y muestras; los datos son identidades locales, no garantía de vida útil.
Los draws directos guardan cantidades e instancias; los indirectos guardan buffer y
offset nativos sin leer los argumentos (count/instances=0 significan desconocidos).
Kind: 0=directo, 1=indexado, 2=indirecto, 3=indexado indirecto, 4=patches, 5=geometría.
Los patches y la geometría se observan en el draw final, no en cada kernel auxiliar.

Attachments 0..7 son color, 8 profundidad y 9 stencil. Se registran textura/resolve,
formato, dimensiones de textura, capas, muestras, mip/slice/depthPlane y load/store;
las dimensiones son de la textura, con el nivel seleccionado registrado aparte.
Los handles nulos representan attachments ausentes. La traza no copia píxeles ni
argumentos indirectos y no altera draw calls, attachments o sincronización.

El analizador añade `render_scopes` por submission. Antes de elegir un candidato,
comprobar el intervalo GPU no confirmado, la correspondencia del render_begin,
y los contadores de descarte. El último draw retenido no es necesariamente culpable.

### Captura de múltiples pasadas y arranque completo (2026-10-26)

Tras el log 144244Z, registrar las pasadas regulares de render, hasta 32 por
recording/envío, con 32 draws iniciales + 128 finales y 64 pipelines por pasada.
El resumen `GPU render coverage` cuenta todas las pasadas y declara omisiones.
Conservar detalles pesados de las ocho submissions más recientes; las anteriores
mantienen los contadores para declarar el hueco. Los encoders temporales de clear
 y render-blit siguen en el journal nativo, sin inventario detallado de draws.

Cada pasada conserva recording, begin_event, end_event, checkpoint y progress_event.
El analizador cruza recording y evento con la lectura GPU: completed_prefix,
first_unconfirmed_interval, later_unconfirmed o unknown. El primer intervalo sin
confirmar incluye sus dependencias: no identifica un shader culpable. Sin evento
válido no se infiere progreso desde la mera codificación CPU.

`prueba_grafica_nativa.command` arranca ahora por defecto con logos
(SOTC_SKIP_INTRO=0), limpiando un perfil de inicio heredado. Para volver a la
comparación sin intro, indicar explícitamente SOTC_SKIP_INTRO=1. Presupuesto 500 s
incluye el arranque; un stall conserva su clasificación independiente.

### Perfil focalizado tras prueba sin probes (2026-10-26)

El log 151035Z reprodujo STOP_GPU_STALL con probes opcionales desactivados.
`prueba_gpu_focalizada.command` activa únicamente el journal de envíos/pasadas GPU,
checkpoints de encoders, observaciones de memoria y estadísticas de frames.
Salta la intro con el perfil validado y mantiene 500 s por defecto. Limpia selecciones
CPU heredadas y desactiva explícitamente journal general, procedencia/bind/copy de
shaders, audio y capturas visuales. Mantiene la configuración funcional de la prueba
sin debug, incluido task guard=0; no cambia shaders, asignadores ni sincronización
funcional. Los checkpoints sí añaden señales diagnósticas y pueden alterar tiempos.

Objetivo de la próxima captura: relacionar la primera pasada sin checkpoint
alcanzado con draws/pipelines/attachments, y contrastarla con presión de memoria y
vida de recursos. No interpretar las escrituras del archivo Metal de hasta 6,6 s
como pausas directas: el writer opera en un hilo de utilidad. No se modifica aún la
política de caché o memoria, para conservar la comparación. El arranque sin intro
es distinto de la prueba anterior con logos; no comparar FPS entre escenas distintas.
