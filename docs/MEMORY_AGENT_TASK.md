# Tarea activa: error Metal de memoria al arrancar

Directiva del propietario 2026-10-26: investigar memoria con pruebas selectivas,
activando y desactivando instrumentación entre rondas. Esta prioridad sustituye
por ahora la búsqueda dirigida de shaders. Conservar expedientes de shaders y
SIGBUS; SIGSEGV permanece solo como evidencia, sin experimentos causales CPU.

## Evidencia de partida

Leer work/analysis/single-inflight-review-20261026T152731Z/review.md,
work/analysis/startup-oom-review-20261026T152422Z/review.md y
work/analysis/focused-review-20261026T152136Z/review.md.
El error 8 Insufficient Memory aparece en chunk 1 con cero draws, incluso con
limit=1, acquired=1 y completed=0. Varios envíos simultáneos no son condición
necesaria. No repetir esa hipótesis como si estuviera pendiente.
Distinguir este OOM del stall posterior en una pasada de 207 draws/58 pipelines.
Pools importados: ventanas 7616 MiB, solapamiento 1024 MiB; esto no demuestra
7616 MiB físicos distintos. Presupuesto recomendado no es RAM libre.

## Selector de instrumentación, implementado

Usar prueba_memoria.command con exactamente uno de estos perfiles:

| Perfil | Activación | Pregunta |
| --- | --- | --- |
| memory-baseline | Sin probes opcionales | ¿Reproduce el error sin señales ni journals? |
| memory-resources | Solo SOTC_MEMORY_TRACE | ¿Qué recursos se crean/retiran y qué base permanece? |
| memory-render | Solo journal GPU y checkpoints | Si supera el arranque, ¿qué pasada queda pendiente? |

Todos mantienen intro saltada, admisión Metal=1, backend y configuración funcional
fijos, 500 s máximo. CPU pointer/field, shader bind/copy/provenance, audio, captura
visual, estadísticas y journal general quedan apagados. No usar el lanzador
histórico, que vuelve a activar múltiples probes. No combinar perfiles por defecto.
memory-render puede alterar tiempos por sus señales GPU; no considerarlo neutro.

## Secuencia de investigación

1. Revisar el cierre del último proceso, estado del controlador y cualquier bloqueo
   vigente antes de lanzar. No modificar capturas selladas ni limpiar estados unsafe.
2. Analizar offline el coste de importar pools, su solapamiento, las declaraciones
   de residencia y la vida de los objetos. Documentar unidades y qué mide cada API.
   Los handles de journal de imágenes y Metal no son intercambiables sin un puente.
3. Primera comparación propuesta: memory-baseline con límite 1. Los OOM anteriores
   con límite 1 tenían checkpoints; la prueba sin probes anterior usó límite 8.
   Esta nueva combinación separa esas variables. Una ejecución inicial, revisar
   antes de repetir; no es preciso gastar siete intentos automáticamente.
4. Si reproduce, memory-resources en la siguiente ronda. Si faltan datos de memoria
   del sistema o del coste por pool, implementar una observación acotada de host
   antes/después de cada importación y del primer envío, con controles apagados.
   No activar trazas CPU o de escrituras de shaders para esta pregunta.
5. Si supera arranque y termina en stall, conservarlo como resultado distinto.
   memory-render en otra ronda permite localizar el intervalo pendiente.
6. Proponer un solo cambio funcional basado en evidencia (por ejemplo vida/residencia
   demostrablemente incorrecta), verificar el código con un control positivo/negativo
   pertinente y repetir perfil de referencia. No reducir memoria guest, aceptar
   buffers truncados ni omitir comandos para silenciar fallos.
7. Tras cada ronda: preservar log/estado/identidades; resumir hipótesis, variable,
   perfil efectivo, cobertura, resultado y decisión. Desactivar el probe que ya
   respondió la pregunta; conservar pruebas de regresión. Hasta siete intentos
   totales por hipótesis, sin reiniciar contador por renombrarla. Un cierre requiere
   evidencia del defecto, test de regresión y varias exposiciones nativas comparables;
   una pasada aislada no garantiza estabilidad.

## Integración con el controlador

run_adaptive_diagnostics.py acepta schema=2 con los mismos campos de schema=1 más
profile (uno de los tres nombres de la tabla). variant debe ser base; seconds entre
30 y 500. No acepta comandos arbitrarios ni variables de entorno libres.
Huellas obligatorias: work/probe_entry_arm64,
work/gpu-build-84daa9f13e5c/libbbgpu.dylib, prueba_memoria.command,
packaging/test-arm64.sh, tools/run_adaptive_diagnostics.py y
work/startup-no-intro-profile.json. Añadir huellas de otros archivos que cambie la
hipótesis. Preparar huellas solo tras terminar cualquier compilación y validación.

No hay solicitud nativa encolada por esta preparación. El controlador histórico
estaba detenido y conserva un resultado unsafe antiguo; esta tarea no autoriza a
borrarlo ni simula una reanudación. El agente puede empezar por el estudio offline;
la recuperación del bucle debe revisarse explícitamente con el estado actual.
Las preguntas de chat no cancelan tareas ni crean STOP. Un solo juego/controlador,
sin pruebas nativas en paralelo. No automatizar reintentos de OOM sin analizar y
comprobar recuperación de recursos. Mantener paradas reales de limpieza, GPU y
revisión; no pedir un modelo más costoso para eludirlas.

Modelo del reviewer: gpt-6-luna; medium para hipótesis de memoria, low para inventario
rutinario, high solo con razón concreta y registrada. No aumentar esfuerzo por
inercia. La selección existente de esfuerzo por ronda sigue disponible.

## Resultado nuevo: referencia completada 153340Z

memory-baseline con límite 1 reproduce error Metal 8 a 2,209 s; todos los probes opcionales apagados. Leer work/analysis/memory-baseline-review-20261026T153340Z/review.md. La primera comparación del paso 3 ya está realizada: no repetirla por desconocimiento. Siguiente perfil propuesto: memory-resources, sin checkpoints ni trazas CPU/shaders.

## Resultado tras reinicio: 153732Z

memory-resources, límite 1: arranque superado, sin error 8; stall a 48,890 s en submission 25274. Reinicio confirmado por propietario, variable externa a conservar. No declarar corrección de OOM. Journals preservados y análisis en work/analysis/post-reboot-review-20261026T153732Z/review.md. Próximo perfil memory-render; apagar recursos para evitar acumular instrumentación.

## Resultado memory-render: 154004Z

Stall a 49,417 s; primera pasada pendiente, 195 draws/52 pipelines, checkpoint 2 no alcanzado. Ver work/analysis/render-review-20261026T154004Z/review.md. Siguiente control específico: memory-baseline después del reinicio; no confundirlo con baseline pre-reinicio que produjo OOM. Después de ese control evitar más repeticiones sin instrumentar una nueva pregunta.

## Control sin probes tras reinicio: 154224Z

memory-baseline, límite 1, intro saltada: STOP_GPU_STALL a 47,157 s,
submission 24280 pendiente, completada 24279. No error inicial Insufficient Memory.
El stall posterior se reproduce sin probes/checkpoints opcionales; estos no son
condición necesaria. Sin journal no afirmar que sea la misma pasada exacta.
No seguir repitiendo esta comparación sin añadir una pregunta nueva.

Siguiente incremento: captura memory-render con metadatos de longitud de buffers
Metal, tamaño de estructura de argumento indirecto y bytes por índice. Flags
argument_range/index_base_range/index_draw_range: 0 desconocido/no aplicable,
1 dentro de límites, 2 fuera de límites. Para indirectos, index_draw_range sigue
siendo desconocido: los contadores/primer índice del argumento no se leen desde CPU.
Las comprobaciones usan resta/división para evitar overflow; cubren rangos físicos
del buffer Metal, no límites de subasignación guest, contenido ni vida del recurso.
No alteran ni omiten comandos. Perfil mantiene memoria/CPU/audio/shader probes off.
Retención ampliada a 32 primeros + 256 últimos draws por pasada (288), manteniendo
32 pasadas y ocho envíos con detalles. Reportar siempre omisiones.

## Rangos completos 154943Z

Stall a 39,527 s; primera pasada completa de 207 draws/58 pipelines: 107 directos con rangos válidos, 100 indirectos con bloques de argumentos y bases de índices válidos. Rangos efectivos indirectos aún desconocidos; no confundir con validados. Ver work/analysis/render-ranges-review-20261026T154943Z/review.md. Siguiente pregunta: contenido/producción/visibilidad de argumentos indirectos, no otra repetición de los mismos rangos.

## Prueba preparada: bindings compute → argumentos indirectos

La instrumentación de memory-render añade `GPU compute binding` únicamente durante
BindStage de los dispatches directos/indirectos del frame. Guarda recording, ordinal
del siguiente dispatch, pipeline nativo, slot, identidad/longitud del MTLBuffer y
offset, o longitud de bytes inline (sin copiar su contenido). Límite 128 registros
por recording y por submission, con contador explícito de omisiones. La cola nativa
permite verificar que el ordinal realmente corresponde a un dispatch del pipeline;
si falta esa evidencia, el analizador no asigna la relación.

Para cada render retenido, el analizador considera el dispatch previo del mismo
recording y cruza sus bindings con los bloques de argumentos indirectos retenidos.
`preceding_compute_binding_matches` significa mismo buffer y argumento dentro de
la cola del buffer desde el offset enlazado. NO demuestra rango lógico del descriptor,
acceso de escritura, contenido correcto, sincronización suficiente ni vida validada.
Los slots inline se excluyen del cruce. La ausencia de coincidencias no excluye
productores anteriores, otros recursos, acceso por direcciones o alias con handles
diferentes. No identificar automáticamente al compute como productor culpable.

Esta prueba no añade copias GPU, lecturas CPU de argumentos ni nuevas esperas.
Mantiene los checkpoints ya existentes y probes CPU/memoria/audio apagados.
Después de obtener el cruce, decidir si hace falta capturar contenido con una copia
GPU ordenada y completion válido; aún no está implementada esa captura de contenido.
Los tests verifican límite, asociación y controles negativos de identidad/pipeline/
ordinal/rango/bytes inline. Ejecutar `bash prueba_memoria.command memory-render` con
la nueva biblioteca compilada; no cambiar otros parámetros simultáneamente.

## Resultado bindings 155856Z

Primera pasada pendiente: 207 draws/58 pipelines; sin pérdidas en 28 bindings pendientes y 19 previos. Compute inmediatamente previo no comparte identidad del buffer de argumentos. El shader b379f26e05d5867d del envío completado sí lo enlaza, pero el MSL lo LEE, no escribe Args. No atribuirle producción por coincidencia de buffer. Ver work/analysis/compute-binding-review-20261026T155856Z/review.md. Siguiente pregunta: procedencia/subasignación/copia al buffer de argumentos de DrawIndirect, evitando nueva repetición idéntica.

## Siguiente prueba preparada: copias stream síncronas

Al revisar la procedencia, ObtainBuffer tiene una ruta read-only pequeña que usa
Reserve + SmallGuestCopy y otra Map + CopySparseMemory + Commit. El control upstream
BbToggle::DeferredStreamCopies (16384) desactiva la primera ruta. No cambia el
contenido de shaders ni salta draws. La prueba afecta a copias pequeñas elegibles
en general, no exclusivamente a argumentos indirectos.

`bash prueba_copia_argumentos.command sync` crea un archivo único de control con
`16384 0`, de solo lectura, y lanza memory-render. Conserva límite Metal=1, intro
saltada, 500 s, sin probes CPU/memoria/audio adicionales. La copia del archivo se
conserva en work/copy-argument-trials y BB_TOGGLE_FILE queda registrado en settings.
Comprobar `Runtime: disabled optimizations mask=16384` en el log; el watcher aplica
el valor durante inicialización. Si falla antes de confirmar la máscara, marcar
la exposición como no confirmada. No editar ese archivo durante la ejecución.

Control emparejado futuro: `bash prueba_copia_argumentos.command deferred` genera
otro archivo `0 0` y mantiene todo lo demás. No lanzar ambos a la vez. La primera
ronda solicitada es sync, revisar su resultado antes del control. No acumular
máscaras ni combinar otros cambios de memoria/driver en la misma comparación.

Interpretación: si el stall desaparece, copias diferidas/orden/tiempos son una
hipótesis prioritaria, no una corrección ni atribución al productor. Si persiste,
no descartar otras rutas de copia ni datos corruptos. Esta prueba no identifica
la rama que tomó cada argumento; la captura detallada de asignación/copia sigue
pendiente si es necesaria tras este control. La alternativa usa código upstream
ya compilado: no se recompila ni cambia la biblioteca GPU para esta comparación.

## Resultado sync 160753Z

Máscara 16384 confirmada. Stall a 39,686 s; primera pasada 207 draws/58 pipelines, checkpoint 2 pendiente, rangos conocidos válidos. No mejora observada frente a 155856Z. Leer work/analysis/sync-copy-review-20261026T160753Z/review.md. Siguiente prueba única: control deferred (máscara 0). Si reproduce, cerrar esta comparación y capturar procedencia/contenido; no repetir en bucle ni acumular bits.

## Cierre control deferred 161006Z y prueba de procedencia

Control máscara 0: stall a 39,033 s, primera pasada 207 draws/58 pipelines,
checkpoint 2 pendiente. Sync 16384 tampoco mejoró (39,686 s). Cerrar esta comparación
sin mejora observada; no gastar más rondas idénticas. Evidencia preservada en
work/analysis/deferred-control-review-20261026T161006Z.

Ejecutar `bash prueba_origen_argumentos.command`. Mantiene memory-render con máscara
0, límite 1, intro saltada, 500 s; activa SOTC_INDIRECT_PREP_TRACE=1 y desactiva
SOTC_COMPUTE_BIND_TRACE=0. Los perfiles baseline/resources fuerzan estos probes a 0.

Nuevo journal GPU indirect origins: 4096 eventos recientes, totales/omisiones
explícitos. kind=1 preparación en DrawIndirectRecord: guest, requested (producto
64 bits), actual (producto usado por ObtainBuffer), tick, stride, count máximo,
backend, offset y longitud. route=1 stream diferido seleccionado (puede ejecutar
inline), route=2 stream síncrono, route=0 otra ruta/no identificada. No inferir
que route=0 sea in-place. kind=2 resolución en DrawIndirect Metal: backend,
offset lógico, metal, metal_base, longitud, recording, stride/count recibido.

Cruzar identidad backend y offsets con eventos de preparación anteriores, después
metal/metal_base+offset con draws capturados. No equiparar solo buffer compartido
a la misma subasignación: puede reutilizar offsets; reportar ambigüedad/omisiones.
Los eventos son observaciones CPU; no son completion de copias ni lectura del
contenido. No se añade sincronización GPU ni memcpy de diagnóstico. Si falta
procedencia o completion, instrumentar esa frontera específica, no reactivar todos
los probes. Reportar cualquier requested!=actual, sin corregir arbitrariamente.

La biblioteca de esta prueba se recompila; registrar su identidad. Prueba CPU del
journal: controles opt-in, scopes anidados, retención y parser. Validación nativa
pendiente hasta recibir el siguiente log.

## Origen confirmado 161756Z; próxima prueba de contenido copiado

Stall a 39,241 s, 207 draws/58 pipelines. Enlaces de preparación/resolución retenidos
para los 100 indirectos: route=1, requested=actual=20, count=1, stride=20. No hay
truncamiento de tamaño observado. El journal aún no demostraba completion.

`prueba_origen_argumentos.command` activa ahora también SOTC_INDIRECT_COPY_TRACE=1.
Antes de encolar la copia existente se registra origen/destino/tamaño y un ticket.
RunGuestCopy notifica tras CopySparseMemory y Flush; la ruta síncrona notifica tras
Commit. Solo en el destino que acaba de escribir ese mismo camino host se capturan
20 bytes para count=1 y stride=20. No se relee el origen guest ni se añade copia GPU,
lectura de argumentos GPU, espera o barrera. No se afirma que la GPU consuma esos
mismos bytes después: la vida/reutilización y escrituras posteriores siguen abiertas.

kind=3: observación de copia host. copy_status=1 asociación única; =2 ambigua;
=3 seguimiento incompleto por saturación. captured_bytes=20 solo con asociación
única y sin pérdidas de pendientes; 0 no significa argumentos a cero. word0..4 son
palabras crudas (word3 requiere interpretación signed cuando corresponda al formato).
La preparación kind=1 conserva destination/ticket para enlazar la copia aunque se
complete antes de emitirse esa preparación. Resolve sigue siendo kind=2.

Límites: journal 4096 eventos y 4096 copias pendientes, búsquedas por destino para
no recorrer todos los pendientes en cada copia. Si se satura el registro pendiente,
se contabiliza la pérdida y se dejan de capturar valores durante esa ejecución;
no atribuir una completion a una copia nueva no observada. Reportar pending,
pending_dropped, ambiguous y omisiones de eventos. La captura no guarda todas las
copias: solo las registradas bajo el scope de argumentos indirectos.

Pruebas CPU: palabras sintéticas, ticket, duplicados, tamaño distinto, saturación
con destino inválido que NO se desreferencia, parser y controles opt-in. Mantener
compute binding, CPU, audio y memoria generales apagados. Primera prueba con copias
diferidas/máscara 0; no repetir sync hasta que una pregunta concreta lo justifique.

## Valores indirectos 163528Z; siguiente control sync con contenido

Stall 39,729 s. Primera pasada completada (54 draws/19 pipelines); segunda
pendiente (151/38, checkpoint 5; 4 alcanzado). 100 indirectos enlazados con
preparación/copia única: route=1, 20 bytes; ocho tienen instanceCount superior
a un millón (máximo 4.195.488.141), todos con indexCount no cero. Los rangos de
índices caben en el buffer Metal. Datos de host, no prueba de bytes consumidos
ni identificación del productor. No clamp arbitrario como arreglo.

Evidencia: work/analysis/indirect-values-review-20261026T163528Z. Siguiente única
prueba: `bash prueba_origen_argumentos.command sync`. El launcher acepta ahora
el modo; sin argumentos conserva deferred. Máscara esperada 16384, route=2;
misma biblioteca y probes. Esta vez comparar valores, no solamente stall.
Si persisten, investigar generación/origen de instanceCount con captura acotada;
si cambian, revisar orden/vida y repetir comparación antes de atribuir causa.
No se ha lanzado el juego ni reactivado el agente persistente.

## Sync 164137Z: anomalía persiste; control de orden de indirectos

100 argumentos route=2 asociados; diez instanceCount >1 millón, máximo
4.262.287.298. Stall 40,645 s, primera pasada 207/58 pendiente. No explica el
problema únicamente la copia diferida. Evidencia completa del análisis en
work/analysis/indirect-values-review-20261026T164137Z (preservación parcial).

Siguiente: `bash prueba_indirectos_ordenados.command`. Máscara 70368744194048:
mantiene sync16384 y añade exclusivamente PipelinedIndirectDraws (1<<46).
Afecta DrawIndirect y DispatchIndirect, drenando la cola de preparación CPU.
No es sincronización completa GPU ni corrección demostrada. Mismos probes,
biblioteca, límite1, 500s e intro saltada. Comparar contenido y completion;
si cambia, confirmar antes de atribuir causalidad. Si persiste, investigar
producción/coherencia de instanceCount; no repetir modos de copia sin pregunta nueva.

## Ordered indirect 164558Z: native recovery, abnormal values persist

Mask70368744194048 confirmed. Exit23 at39.787s: Metal code1 InnocentVictim,
not OOM or cooperative stall31. Native progress26/26 cannot certify successful
rendering of a failed/recovered command. First passes55/19 and164/43;
100 indirect host-copy associations in second pass,12 instanceCount>1million,
max3571181399. Unique records deduplicated across two concurrent origin dumps,
4096 unique, no conflicts. Evidence:work/analysis/ordered-indirect-review-20261026T164558Z.

Fixed offline analyzer to accept standalone FAILED_METAL with its own identity
and mark render progress unreliable on failure; four CPU tests passed. No native
code/library/probes added. Next single trial after manually rebooting Mac:
`bash prueba_indirectos_ordenados.command`. Same mask,500s,inflight1,skipintro.
Reason for repeat: recovery-class termination, not a blind copy-mode retry.
No proof of stale GPU state; reboot is a control. Abnormal counts already persist
with ordered indirect CPU preparation, so don't label this toggle a fix.

## Repeat165210Z; targeted writable-buffer descriptors

Mask70368744194048: normal stall47.986s exit31; no InnocentVictim. First render
207/58 pending,1/23progress. 100unique indirect associations;10 huge instanceCount,
max4162756212. Thus ordered indirect preparation does not remove host anomaly.
Log cannot independently establish reboot. Evidence partial:
work/analysis/indirect-values-review-20261026T165210Z.

Next `bash prueba_productores_argumentos.command`: same functional settings,
adds SOTC_INDIRECT_WRITER_TRACE=1. Records2048 most recent explicit writable
shader buffer descriptors (guest,bytes,shader,slot,tick,paged), separate journal,
loss counters. Match spans against argument guest ranges offline; intended
writable binding does NOT prove shader execution, actual write or producer.
No new guest reads/GPU synchronization. Does not capture CPU/image/internal
writes or descriptors erroneously classified read-only. Paged ranges are not
necessarily exhaustive. If records missing, report coverage gap, not absence.
Keep general shader-copy/compute-bind/CPU/audio/memory tracing off.

Validación: compilación completa y 35 comprobaciones del renderer correctas;
pruebas CPU del journal ampliadas (opt-in, límite, campos y parser), cuatro
pruebas de submission y dos de perfiles correctas. Launcher verificado.
No se lanzó el juego. Huellas de próxima biblioteca en
work/analysis/indirect-writers-20261026-validation.json.

## Writable journal165947Z: history lost to repetitions; schema2 next

Stall49.764s, first pass196/53 checkpoint2pending.100 linked indirect arguments,
10 huge instanceCount (max3574506854).111783 writer observations;2048tail contains
only48 distinct signatures. Four overlapping shader hashes cover72normal args,
NONE of10huge arguments:41d379bc,8ec555ce,37d979e3,66db0b55. Do not name these as
abnormal-value producers. Missing older records prevent exclusion. Evidence:
work/analysis/indirect-values-review-20261026T165947Z, including writer-overlaps.json.

Next same command `bash prueba_productores_argumentos.command`, same functional
settings/probes. New schema2 writer history groups repetitions by guest,size,
shader,slot,paged, retaining2048distinct signatures with LRU refresh. First/last
sequence/tick and observations count; coalesced repetitions vs omitted evicted
observations distinguished. Do not treat time span as continuous ownership/write
or infer causality from latesttick alone. First,last ticks need not be monotonic
across threads. Reappearance after eviction starts a new summary.
Bounded journal and output; no added guest memory reads or GPU synchronization.

Schema2 recompilado: 35 comprobaciones renderer correctas. Prueba CPU
verifica10000repeticiones, camposfirst/last/count, separación slot/paged,
límite2048, actualización LRU, pérdidas contabilizadas y parser.
Huella próxima biblioteca:work/analysis/indirect-writers-history-20261026-validation.json.
No se lanzó el juego.

## 170632Z: concrete retained shader candidate; trace its inputs next

Stall38.386s. First pass53/19completed, second158/43first-unconfirmed.100indirect
joins,10hugecounts. All10overlap retained shader7d08eb3e slot3 at0x815e5e2d90,
24576bytes,tick20429 (copies tick20501). One retained candidate, not global unique
producer: history still loses2632entries/75942observations. Full evidence:
work/analysis/indirect-values-review-20261026T170632Z.

Exact cached SPIR-V XXH3=c7a0fb173df9fe52 maps to preserved Metal compute source.
Writes matching indirect layout; instanceCount copied from descriptor slot1
(Metalbuffer2), baseInstance from slot2; metadata from slot0, destination slot3.
Do not infer source values or successful execution from static code alone.

Next `bash prueba_entradas_argumentos.command`: same mask/settings, broad writer
journal OFF, SOTC_ARGUMENT_INPUT_SHADER=7d08eb3e. Up to128explicit descriptor
observations for selected hash, read/write, address/size/slot/tick/paged; notice
when truncated. No memory reads, GPU synchronization or functional changes.
Existing argument copies stay captured. Seek upstream input ranges and their
coherence/production. Do not clamp values or skip draws as a fix.

Compilación completada y 35checks renderer correctos. Prueba CPU añade
filtrohash,128registros,truncamiento,read/write y parser. Cuatro pruebas
submission y dos perfiles correctas; launcher validado sin ejecutar juego.
Huella próxima biblioteca:work/analysis/argument-inputs-20261026-validation.json.

## 171427Z: selected input probe missed alternative binding paths

Stall39.324s.54/19first render completed;163/44second pending.100indirect joins,
9hugecounts,max3571729134. Only selected slot3observed, guest0x815e4e28d0,
24576bytes,tick20076; contains all9abnormal spans. No trace truncation.
Probe covered ordinary descriptors only, while constant-ring and special/internal
bindings bypassed it. Missing slots0..2 must not be interpreted as no inputs.
Evidence:work/analysis/indirect-values-review-20261026T171427Z.

Next same `bash prueba_entradas_argumentos.command`, after native rebuild:
route0ordinary,route1ring,route2special. Ring reports source guest address,size,
backend andoffset; special reports type with unknownrange fields0. Same128cap,
hash7d08eb3e, general writerjournal OFF; no buffercontent reads or functional
change. First establish missing input routes/addresses; then investigate actual
copy/lifetime, not assume ring bug from code structure alone.

Recompilación lista: 35comprobaciones renderer correctas; campos route,
backend,offset,type y límite128 verificados en fixture CPU/parser.
Huella:work/analysis/argument-routes-20261026-validation.json. No juego lanzado.

## 171952Z: input routes confirmed; disable constant ring for one control

Stall39.844s, first207/58pending.100indirect joins,10hugecounts,max4067812199.
Selected shader7d08eb3e slots0/1/2 all route1 ring; slot3route0 output.
Inputs:0x815fd45d10(1552),0x815e5e0850(2048),0x815e5e1060(2048).
No truncation. Slot1 provides instanceCount according to preserved shader code;
its values/read bounds remain unknown. Evidence:
work/analysis/indirect-values-review-20261026T171952Z.

Next `bash prueba_entradas_sin_ring.command`. Mask70437463670784 adds only
ConstantRing1<<36 to prior70368744194048; experiments0. Affects all stages and
pending-read tracking/copy timing, not just candidate shader. Same focused probes,
500s,inflight1,skipintro. Verify mask and selected slots0..2 no longer route1.
Same native library, launcher stub/syntax validated; no game launched. Compare
values and GPU result, then paired confirmation if changed. Do not declare a
fix merely because the failure class changes or a different pass completes.

## 172239Z: no-ring control fails to remove anomaly; correlate input producers

Mask70437463670784 and selected slots0..3route0 confirmed. Stall38.683s,
first two scopes50/17and3/2completed; third158/43pending(checkpoint8).
100indirect joins,9hugecounts,max3170424854. No functional correction. Evidence:
work/analysis/indirect-values-review-20261026T172239Z.

Next `bash prueba_productores_entradas.command`: same no-ring mask, add bounded
schema2 writer history to selected7d08eb3e input metadata in SAME run. Correlate
slot1instanceCount,slot2baseInstance,slot0metadata input spans with candidate
writable descriptors. Do not match addresses across runs as identity or equate
write intent with actual write. Account for ticks, omissions, memory reuse and
uncapturedCPU/image/internal writes. General probes off; same500s/inflight1/skip.
Same native library; launcher stub and syntax checks pass. No game launched.

## 190643Z: instance-count producer candidate fcad581d

Stall40.792s;55/19first render completed,164/43second pending.8hugeinstanceCounts,
max3963399682. Exact same-run retained writer candidates for7d08eb3e inputs:
slot0 <-50784801 slot2 tick21249; slot1 <-fcad581d slot2 tick21271;
slot2 <-f73f4118 slot3 tick21274; consumer tick21277. History incomplete;
overlap and ticks do not prove actual write/lifetime/causality.
Evidence:work/analysis/indirect-values-review-20261026T190643Z.

Exact cached fcad581d SPIR-V/MSL preserved (XXH3fece6f0b82549285). Reduces0/1 over
64threads in LDS then atomically adds into slots2and3. No explicit barriers in
SPIR-V or Metal. Investigate synchronization across SIMD groups AND accumulator
initialization/reset; both hypotheses open. No barrier insertion inside divergent
branches, clamp, or skipped shader justified by this alone.

Next `bash prueba_contador_instancias.command`: same mask70437463670784, selected
hashfcad581d instead of7d08eb3e, bounded writerhistory and copied arguments kept.
Obtain input and atomic destination spans; metadata does not capture contents or
reset operations. Same500s/inflight1/skipintro. Same native library; launcher
syntax/stub validated; no game or persistent agent started.

## 191512Z: accumulator initialization coverage gap; DMA-fill probe next

Stall39.473s;54/19completed then163/44pending.8hugecounts,max4208885693.
fcad581d inputs0x815fa0fdf0+260,0x815e480030+393216; retained writable candidates
feaef49e and287f5d44,tick18874. Atomic outputs0x815e4e0850+2048 and
0x8142a0ff80+32,tick18880. Only self in shader writable history does NOT prove
missing initialization: DMA/CPU/copy writes not covered. Evidence:
work/analysis/indirect-values-review-20261026T191512Z.

Next `bash prueba_inicializacion_contadores.command`: same functional mask,
fcad581d metadata + argument copies; broad writerjournal OFF; FILL_TRACE ON.
Keeps2048recent Rasterizer::FillBuffer events and queries overlap at selected
writable binding before later events can evict history. Route1after CPUfill,
route2after GPUfill recording (not completion),route3GDS not matched as guest.
Reports pattern,span,tick,backend/offset,retention/omissions. No buffer reads or
GPU synchronization. Partial overlap insufficient; absence not proof of missing
init because other paths/evictions exist. Maintain LDS reduction hypothesis.
Same500s,inflight1,skipintro; no game/persistent agent started.

Compilación lista y 35checks renderer correctos; fixture CPU verifica
solape/límites, exclusiónGDS, patrónno-cero, omisiones2048 y parser. Dos pruebas
de perfiles y stublauncher correctos. Huella próxima biblioteca:
work/analysis/counter-fills-20261026-validation.json. No juego lanzado.

## 192142Z: no FillBuffer observed; exact-shader LDS synchronization trial

Stall38.779s,9hugecounts. Both accumulator queries total0/retained0/omitted0;
no Rasterizer::FillBuffer before binding. Other initialization routes unobserved,
so NOT evidence of uninitialized buffers. Evidence:
work/analysis/indirect-values-review-20261026T192142Z.

Next `bash prueba_sincronizacion_contador.command`: opt-in exact SPIR-V
fece6f0b82549285 (fcad581d), plain variant only. Six unconditional workgroup
barriers before reductions32,16,8,4,2,1. Unique top-level anchors validated
before mutation; reject unexpected source. Cache/function tag_sotc_lds6_v1.
Require runtime marker GPU reduction trial / barriers=6 before interpreting.
Same mask70437463670784/500s/inflight1/skipintro; selected metadata and argument
copies on, fill/broad-writer histories off. This is experimental, not adopted fix.

CPU exact-source transformation/negative controls and1000pattern reduction
model pass. Native Metal compiler unavailable; API compile-only attempt found
no device in tool environment. User run first validates native shader compilation
and behavior. No game launched here. Initial accumulator/coherence hypotheses
remain open; improved run needs paired confirmation before claiming cause.

Renderer compilado, 35checks correctos. Huella y límites de validación:
work/analysis/reduction-sync-20261026/validation.json. Transformación exacta
y launcher verificados; shader nativo aún pendiente.

## Triple run192849Z/192902Z/192925Z: early OOM, trial not reached

All Metal code8 Insufficient Memory,chunk1,recording2,zero draws. Durations
2.137/1.069/1.479s. Trial flag1/inflight1 confirmed but no reduction marker
or selected shader inputs. Do not treat as three negative shader trials.
Evidence:work/analysis/reduction-trial-early-oom-20261026. Recommended next:
manual reboot then one unchanged prueba_sincronizacion_contador.command.
If early OOM repeats after reboot, investigate startup memory before further
shader tests. Association with earlier reboot improvement does not prove leak.
No code/library change or game launch for this review.

## Post-reboot193248Z: experimental progress, then later stall

User rebooted; no initialOOM. Barrier marker six/tagged/cached0 confirms native
shader compiled. Finished DURING review: stall56.909s exit31. Earlier partial
assessment superseded. Awaited recording22018,progress29/128 with812omitted;
native tail misses earlyevents (256/2840). Do not name exact culprit.
1071retained copied arguments,0hugeinstanceCounts,max5310; changed retention
means no proof prior100arguments normalized. Promising, not fixed/gameplay.
Evidence:work/analysis/reduction-progress-20261026T193248Z (final.log/review).

Next `bash prueba_control_sin_barreras.command`: same mask/library/settings,
explicit barrierflag0 ONLY changed. Launcher validated. Compare with experimental
run; earlyOOM invalidates comparison. If anomaly returns, paired enabled repeat
before adoption. No second run launched during user status question.

## 193539Z OFF control: anomaly returns; repeat ON unchanged

Barrierflag0 confirmed, no reductionmarker, mask70437463670784; no earlyOOM.
Stall37.966s,54/19firstcompleted,152/39nextpending.100indirect joins with10huge
instanceCounts,max4043604573. Prior ON193248Z reached56.909s with0huge in1071
retained copies, though earlier100not necessarily retained. Stronger association,
not yet a confirmed fix or sole cause; ON still stalls later.
Evidence:work/analysis/indirect-values-review-20261026T193539Z.

Next `bash prueba_sincronizacion_contador.command` ON repeat, unchanged library,
mask/probes/500s/inflight1/skipintro. Require barrier6 marker. Do not introduce
new probes/toggles during paired confirmation. EarlyOOM invalidates comparison.
No game launched for this review. After repeat, assess ON/OFF/ON evidence and
only then consider adoption plus separate later-stall diagnosis.

## 193734Z/193749Z/193758Z: repeat ON not reached, startup OOM

All Metalcode8/chunk1/recording2/zero draws,1.416/1.214/1.124s. Flag1confirmed,
no reductionmarker or selectedinputs. Do not count as shader failures. ON/OFF/ON
confirmation remains pending. Evidence:work/analysis/reduction-repeat-early-oom-20261026.
Immediate retries unhelpful. Next manual reboot then ONE unchanged ON command;
if startupOOM persists after reboot, prioritize startup memory diagnosis.
No code change, cache cleanup or game launch during review.

## 194101Z second valid ON: later stall, preserve early encoder events next

Marker6barriers/cached0 confirmed; no startupOOM. Stall53.217s; first16/1and1/1
regular renders completed. Recording20065progress39/128 (826omitted); next
unconfirmed checkpoint40. Native tail256loses corresponding early operation.
1071retained copied arguments,0huge,max1; previous ON56.909s similarly0huge;
OFF37.966s10huge. Supports early benefit, not fullfix or identical100draw coverage.
Evidence:work/analysis/reduction-repeat-20261026T194101Z.

Next same `bash prueba_sincronizacion_contador.command`. Barriers ON and all
functional settings unchanged. Native metadata now first192+last64, same total256;
retain checkpoint30-40region, count omissions, no extraGPUevents. Four submission
CPU tests pass inclfirst/last retention and merge of truncated recordings.
Library rebuilding; no game launched. Later stall separate from earlyOOM;
no memory-leak conclusion from this run.

Recompilación final lista: 35checksrenderer correctos; cuatro pruebas
submission correctas. Huella:work/analysis/later-stall-prefix-20261026-validation.json.
No se lanzó el juego.

## 194817Z: later interval bounded to blit checkpoint34

Stall64.520s;barrier6confirmed;1125retainedcopies,0hugecounts. Recording23541,
progress33/128. First192native capture now retains reachedcompute checkpoint33
(event131), thenblitbegin132/end133/checkpoint34event134unconfirmed. Preceding
computeaa73c19127289baf,5466x64threads; not proof of exact faulting operation.
Evidence:work/analysis/later-blit-review-20261026T194817Z.

Next same `bash prueba_sincronizacion_contador.command`, barrierON/settingssame.
Adds bufferblit copy/update/fill handles,offsets,size,bufferlengths; imageblit
operation-kind markers. Same256events(first192,last64); ordinals shift, match
checkpoint/progress_event/recording instead. No newGPUcommands/sync/contentreads.
Four submission CPUtests pass. Watchdog10s without completion is not proof of
permanent hang; assess workload after obtaining blit metadata. No leak conclusion.
No game launched; library rebuild pending completion.

Blit metadata listo: renderer compilado y 35checks correctos; cuatro
submissiontests pasan con nuevos campos. Huella próxima biblioteca:
work/analysis/blit-details-20261026-validation.json. No juego lanzado.

## 195611Z: pending submission reaches94; test whether10s cutoff is premature

BarrierON confirmed,stall48.864s,1071retainedcopies0huge. Progress94/128;
native256/7814 retainsfirst192+last64,misses checkpoints94/95.149retained
buffer-to-image markers; no specific late operation proven. Increased event
density means prefix no longer covers same checkpoint range.
Evidence:work/analysis/later-wait-review-20261026T195611Z.

GpuSignalLoop threshold is10s elapsed waiting for a submission, NOT internal
progress detection. Next `bash prueba_espera_gpu_60s.command`: threshold60s,
SOTC_GPU_WATCHDOG_SECONDS strict bounded10..120, invalid/default10. Global500s,
DeviceLost handling and cooperative stop preserved. Samebarriers/mask/inflight1/
skipintro/probes. Require GPU wait budget: seconds=60. Completion vs60s timeout
is the question; cross-run94vs33 not proof of intra-run progress. EarlyOOM invalid.
No game launched. Watchdog parser CPU controls passed; rebuild in progress.

Validación final de la prueba de espera GPU de 60 s: compilación completada con código 0; 35 invocaciones de comprobación del renderer completadas. Evidencias en `work/analysis/gpu-wait-budget-20261026/validation.json`. No se ha lanzado el juego. Siguiente comando: `bash prueba_espera_gpu_60s.command`.

## Revisión 20261026T200434Z — espera GPU60s

Confirmed GPU wait budget60s and experimental reduction barriers6 enabled.
STOP_GPU_STALL; elapsed91.566s; child21/supervisor31. Awaited21228,
completed21227. Actual awaited native recording22128, chunk22126; watchdog
text reports recording21230, so use native journal identity for encoder joins.
Native completion callback absent; native status2, error0. Missing scheduled
callback is not evidence of no GPU execution: shared progress event reached128.

Progress128/128 with826 omitted checkpoints: checkpoint budget saturated.
This proves the instrumented prefix reached its last checkpoint, not that the
whole command completed. No time series establishes whether progress continued
through the60s wait. Different runs cannot be compared as intra-run progress.
Native journal256/7926 retained,7670 omitted; render scopes32/110 retained.
Pending operation cannot be identified from retained prefix and tail.

1125 unique retained20-byte copied indirect argument records; zero instanceCount
above1million; maximum5310. Partial retained snapshots, not all draws or proof
of the bytes consumed by GPU. Barrier trial benefit remains consistent with
prior ON/OFF evidence; current GPU stall remains unresolved.

Metal point-in-time allocated_bytes15944269824 (15.94GB,14.85GiB), recommended
12713115648 (12.71GB,11.84GiB), ratio1.254. This is evidence of high allocation
relative to Metal recommendation, not process RSS, total system memory, a hard
allocation limit, or proof of a memory leak. No reported Metal OOM/fatal signal
in this trial. Need comparable resource-lifetime/time-series observations to
separate retained caches, legitimate workload growth and unreleased resources.

Conclusion: increasing pending-submission wait10s->60s did not yield completion
in this run. Does not establish permanent hang. Next useful instrumentation:
bounded coverage across the entire command (e.g. spaced checkpoints plus an
encoder identity mapping), with periodic progress and Metal allocation samples.
Avoid simply increasing timeout or claiming saturated128 means completion.
No code changes or game launch performed during this analysis.

Evidence: `work/analysis/later-wait-review-20261026T200434Z/`.

# Repetición GPU60s — 20261026T200754Z

Final: STOP_GPU_STALL; elapsed96.366s; child21/exit31. Budget60s confirmed;
barriers6 marker confirmed. Awaited25771/completed25770, native recording26867,
chunk26865. GPU checkpoint128/128,812 omitted. Native journal256/7814,
7558 omitted. Saturation prevents locating the pending interval or determining
whether progress continued later in the wait. No completion callback; native
status2/error0. No explicit Metal OOM reported.

1125 unique retained20-byte copied arguments;0instanceCount>1million,max5310.
Partial snapshots only; no proof of all consumed arguments. Metal allocated
15784706048bytes vs recommended12713115648bytes (~15.78GB vs12.71GB).
Similar pressure to preceding run15.94GB, not evidence of monotonic growth or
proof of leak. Different runs/workloads, not a resource lifetime time series.

Second60s trial still did not complete pending submission. User requested an
optional120s trial: new prueba_espera_gpu_120s.command sets120 per submission
and500 global; same barrierON/mask/probes. Existing compiled parser accepts120;
no native code change/rebuild needed. Launcher syntax and stub environment
forwarding checks pass. No game launched. Distributed checkpoints still pending;
this trial only changes wait duration, not diagnostic coverage.

Evidence: `work/analysis/later-wait-review-20261026T200754Z`.

# Memory failure 201020Z

120s GPU budget and6-barrier shader trial confirmed. Final exit23 at200.503s:
STOP_GPU_METAL_COMMAND, Metal status5/code8 Insufficient Memory. Allocated
15912009728bytes vs recommended12713115648. Actual failed recording25555,
chunk25553; checkpoint128/128 with826 omitted. Failure makes prefix completion
unreliable as evidence of successful command execution. No GPU watchdog line.
Callback scheduling/completion reported162296.659/162308.063ms after commit;
120s setting is not a hard wall deadline from commit, nor evidence GPU worked
for120s. This run ended with native OOM rather than diagnostic timeout.
1125 unique retained20-byte copied arguments; 0 instanceCount>1million.

Confirms an allocation failure in this run, not a memory leak or the cause of
all earlier timeouts. Existing memory trace wasOFF, so no resource lifetime
accounting available. Next trial: prueba_memoria_contador_120s.command,
resource profile only, render/checkpoint/indirect probesOFF; same barrierON,
mask70437463670784,inflight1,skipintro,GPU120/global500. Timing may change with
probe removal. Native code unchanged; bash syntax and full profile forwarding
through a stub launcher verified. No game launched. Inspect memory journal
coverage/drops before drawing allocation/lifetime conclusions.

Evidence: `work/analysis/memory-failure-review-20261026T201020Z`.

# Resource-memory trial 201457Z

Final Metal code8 Insufficient Memory,71.756s,exit23. Allocation15062171648,
recommendation12713115648. Barrier6 marker and GPU120s confirmed. Resource
profile enabled; render/checkpoint/indirect journals disabled. No argument-count
conclusions possible in this run.

Image lifetime accounting:1579 created (5043938176bytes),835 destroyed
(2541602944bytes),744 live (2502335232bytes). No tracking overflow, unmatched
frees, observation errors or dropped records in final summary. This accounting
is images only, not every Metal allocation or proof of actual physical release.
One pending image2654208bytes; no scene-proxy pending allocations.

Six summary records. Last regular snapshot live813697408bytes,driver usage
9685139456bytes. Final live2502335232bytes. GC sample at failure is47.359s old;
its9.685GB value cannot describe final pressure. Why GC stopped sampling is
unresolved. Buffer journal228records,mostly207pool hits and20staging selections;
not comprehensive live-buffer accounting. Cannot attribute the gap between
2.50GB tracked images and15.06GB total Metal allocation to a leak.

Next controlled comparison uses existing BB_RESIDENCY_SET=0 option, which skips
explicit residency-set creation. All functional mask/barrier/inflight settings
and resource probes preserved; GPU120/global500. This tests contribution of
explicit residency policy, not a confirmed fix. Other resource retention still
exists; driver may retain memory independently. Script validated through stubbed
full launcher chain, no game started, no native rebuild needed.

Next: `bash prueba_memoria_sin_residencia.command`. Evidence: `work/analysis/memory-resources-review-20261026T201457Z`.

# No explicit residency resource trial 201727Z

STOP_GPU_STALL at157.867s; GPU wait120s; awaited9689/completed9688. No reported
Metal OOM. Barrier6 marker confirmed; usual residency-enabled message absent,
consistent with requested BB_RESIDENCY_SET=0, not independent env attestation.
No final Metal memory observation or checkpoint trace in resource-only profile.

Six regular memory snapshots, no failure snapshot because stop was watchdog.
Last snapshot: live images739666688bytes,created1941018880,destroyed1201352192;
GC driver usage9563881472bytes, dropped0. Last snapshot roughly26s after first;
not a memory measurement at the157.867s termination. Cannot compare that9.56GB
to the preceding run's15.06GB at failure, nor claim OOM fixed or leak excluded.
Observed outcome changed but different workloads/timing and probe effects remain.

Next: prueba_envio_sin_residencia.command. Keep residencyOFF,barriersON,
mask70437463670784,inflight1,GPU120/global500. Switch resource journalOFF and
submission/checkpoint traceON; indirect/compute-binding probesOFF. Existing
checkpoint128 cap remains; goal is native final memory/status, not full encoder
coverage. Syntax and profile forwarding checks passed with stub; no game launch.

Evidence: `work/analysis/no-residency-review-20261026T201727Z`.

# No-residency render trial 202122Z

Metal status5/code8 Insufficient Memory; exit23 after106.101s. Allocated
15776169984bytes vs recommended12713115648. Failed recording23321,chunk23319.
Barrier6 marker confirmed; GPU wait120s. Checkpoints128/128 with812 omitted;
failed command progress does not imply successful execution. No indirect copied
argument probe in this run. No explicit residency-enabled message; requested
profile OFF. Disabling explicit residency did not prevent OOM in this run.
Previous no-residency resource-only trial ended by watchdog, so cannot infer
relative improvement from different termination modes or durations.

Next single instrumentation comparison: prueba_base_sin_residencia.command.
Same BB_RESIDENCY_SET=0,barriersON,mask70437463670784,inflight1,skipintro,
GPU120/global500. Optional resource/submission/checkpoint/indirect probesOFF.
Native failure reporting remains and should report Metal allocation if OOM.
This is a diagnostic-overhead control, not stock runtime or confirmed fix.
If OOM repeats, these optional probes are not necessary for reproduction.
If absent in one trial, do not infer causality; timing/driver state differ.
Full distributed checkpoints and complete buffer lifetime accounting remain
pending. Syntax and full launcher env forwarding validated with stub only;
no game launched, no native changes or rebuild.

Evidence: `work/analysis/no-residency-render-review-20261026T202122Z`.

# Baseline OOM 20261026T202429Z

Final Metal status5/code8 Insufficient Memory,elapsed73.075s,exit23.
Metal allocation14826635264bytes,recommended12713115648. GPU wait120s,
barrier6 exact-shader marker confirmed. Supervisor settings explicitly show
resource,submission,encoder,indirect and compute-binding probesOFF. Inflight1,
23113 acquired,23112 completed at failure. Requested residencyOFF (no enabled
marker; supervisor currently does not log BB_RESIDENCY_SET directly).

Optional probes are not necessary for this failure to occur. This does not
exclude their timing/memory effects in other runs. Not a full unmodified vanilla
runtime: functional trial barriers and mask remain. Native allocation lower
than last probe run, but different workloads/timing preclude causal comparison.
No lifetime data in this baseline, so no leak proven. Imported guest pool:
balanced-v2,3windows,7616MiB imported,1024MiB overlapping virtual ranges;
imported byte accounting must not be equated to duplicated physical residency.

Next: one cold-start repeat after manual macOS restart, same
prueba_base_sin_residencia.command, no settings changes. Existing history includes
early OOM sequences affected by restart, so control prior-run system state before
attributing this late failure exclusively to one run. A successful cold run is
not proof of driver leak; other state/caches/timing also change. If cold run also
OOMs, proceed with accounting of native buffers/heaps and retained resources,
rather than repeating timeout changes. No code changes, rebuild or game launch.

Evidence: `work/analysis/baseline-oom-review-20261026T202429Z`.

# Baseline repeat202850Z

Metal OOM code8 at183.227s,exit23,allocated15839559680bytes vs recommended
12713115648. Optional probesOFF confirmed by supervisor settings; barrier6
confirmed,mask70437463670784. Inflight1,7668 acquired/7667 completed. Requested
cold restart cannot be independently confirmed by this log. Failure still
reproduces without optional probes; no resource lifetime records to prove leak.

Imported pool balanced-v2:3windows,7616MiB imported,1024MiB overlapping ranges.
Next controlled comparison uses existing BB_POOL_SPLIT=0, one import, otherwise
same baseline/residencyOFF/barriersON/GPU120/global500. Runtime pool constructor
explicitly supports this option. Historical source comment warns single import
can cause incorrect geometry for offsets beyond4GiB on MoltenVK. Native backend
correctness must not be assumed. Diagnostic only, not a proposed default fix.
Overlapping imports do not prove physical duplication; test observes whether
allocation/failure changes, not assumed1GiB saving. Confirm runtime layout=single
and overlap=0 in next log. If geometry regresses, preserve evidence and restore
split profile; do not interpret absence of OOM as correctness.

Launcher syntax/full stubbed environment forwarding pass; no native change,
no rebuild, no game launched. Full native allocation/lifetime attribution still
pending. Next command: bash prueba_memoria_pool_unico.command.

Evidence: `work/analysis/baseline-repeat-review-20261026T202850Z`.

# Single pool trial203323Z

Confirmed single window,6592MiB imported,overlap0 vs prior7616MiB/1024MiB.
Metal OOM code8 at40.447s,exit23,allocated13853638656bytes,recommended12713115648.
Barrier6 confirmed; GPU120s. Inflight1,25243 acquired/25242 completed. ProbesOFF.
Still fails despite no overlapping imports and optional probes disabled. Lower
point-in-time allocation vs preceding15.84GB run is not an isolated physical
saving measurement: workload/state/duration differ. No geometry correctness
observation available; single pool remains experimental, not a functional fix.
No live image accounting in this run, no leak determination.

Next: prueba_recursos_pool_unico.command. Same single pool,residencyOFF,
barriersON,mask70437463670784,inflight1,GPU120/global500. Switch ONLY resource
lifetime traceON; render/checkpoints/indirect probes remainOFF. Aim: live image
and retirement accounting at failure, GC sample freshness and driver usage.
This existing journal does not account for all native buffers/heaps; it cannot
by itself identify the full unexplained allocation gap. Check dropped records
and freshness. No timing comparison is a performance conclusion.
Syntax/full launcher stub validation passed; no native changes/rebuild or game
launch. Retain previous split-profile scripts for restoration.

Evidence: `work/analysis/single-pool-review-20261026T203323Z`.

# Single pool resource trial203518Z

Metal OOM code8 at39.537s; allocated13853638656bytes, exactly same reported
allocation as preceding single-pool baseline. Same value is reproducibility
evidence, not proof of fixed hard limit. Single6592MiB pool/overlap0 confirmed.

Last regular snapshot: images723680000bytes,driver8432107520bytes. GC trigger
8899180948bytes; below trigger at all six regular snapshots. Final failure
7.966s later: images2411618304bytes,711 live;1534 created4871865472bytes,
823 destroyed2460247168bytes. Delta live1687938304bytes. No lost records,
tracking overflow or unmatched frees. Final GC sample stale7.966s, so cannot
assume collector saw final pressure. Image lifetime accounting is not full
native buffer/heap accounting or physical release proof. No leak established.

Next controlled trial: existing BB_GC_BUDGET_MB=9216 (MiB) overrides collector
threshold basis. Code calculates70%trigger,85%pressure,95%critical; approximately
6.76GB,8.21GB,9.18GB respectively. Not hard allocation cap or changed system
recommended maximum. Keep single pool/residencyOFF/resource trace/barriersON,
mask70437463670784,inflight1,GPU120/global500. No forced release or changed
resource synchronization; existing GC policy and GPU-content guards retained.
May increase eviction/reupload cost. Next journal must confirm thresholds and
actual freeing; if GC ceases during pending submission, earlier thresholds may
not solve it. No fix claimed. Single-pool geometry caveat still applies.

Launcher syntax and complete stub chain forwarding passed; no native rebuild,
no game launch. Command: bash prueba_recursos_gc_temprano.command.

Evidence: `work/analysis/single-pool-resources-review-20261026T203518Z`.

# Early GC trial203730Z

OOM33.927s,Metal allocation13913227264bytes. Override confirmed by recorded
GC trigger6764573487/pressure8214124940. GC reported10evictions; later pressure
reports0additional evictions. Failure image live2408820736bytes,destroyed
2514323584bytes; staleGC8.253s. Last regular live720882432,driver8432975872.
No dropped records. Lower thresholds did not prevent failure; don't keep
lowering budget blindly or bypass GPU-content retention protections.

Correction to prior descriptions: BB_RESIDENCY_SET=0 skips shim residency only.
Native mtl_frame.mm creates separate device-address residency set unconditionally.
Therefore prior runs must not be called fully residency-disabled. Address buffer
registry is appended uniquely but BufferGone only erases registered map; further
inspection/measurement needed before claiming retention leak or changing release.

Added optional resource-profile CPU metadata summaries before commit (at most
once per5s) and on native failure. Registered map and unique address-buffer list:
counts,sampled counts,sum of lengths,cap4096each; locks taken separately.
No extra retained references,content reads,GPU markers or resource mutations.
Registry sums may alias within/across categories; not physical allocation.
Native allocated bytes and native residency activity included. This is partial
buffer accounting, not a complete heap/driver inventory. A missing periodic
summary does not imply no activity during long encoding/waits.

Next prueba_buffers_nativos.command restores default GC budget, retains single
pool and shim residencyOFF,barriersON,resource profile,GPU120/global500. Render
probes remainOFF. Native rebuild/checks required before presenting command.
Single pool remains diagnostic and may affect geometry.

Evidence: `work/analysis/early-gc-review-20261026T203730Z`.

## Reference review20261026

Verified with isolated git clones (no changes to working runtime base): mac main
9c8ac5c93bbf62be7a096134f363f014c42c044d,RC0.9.0 dated2026-10-26 19:38UTC,
points runtime a27f0ca477bd8356a015311d1eaa92e6f6a0b007. Runtime has24commits
after base84daa9f13e5cf000d5fb59dc65803e28026fa77b.
Priority candidates a834d0e (native backend resource initialization/residency),
91635cf+1ef12f8 (native boot without Vulkan loader),f2b0ce9 (3D images),
1ff76ce+8da4c51+276ad55 (Metal device capabilities). Not independent cherry-picks:
startup/resource changes depend on the backend migration. Inspected a834d0e diff:
uses NativeBoot device/queue and skips Vulkan host-memory usability probe for
MetalFrame; no demonstrated OOM fix or new address_buffers cleanup in this diff.
8982059 explicit1920x1080 treated as native is less relevant to current scripts
which unset BB_RENDER_RES. UI/online/Linux changes lower priority for our OOM.
Recommendation: scoped migration preserving SotC overlay and diagnostics, not
blind update. No upstream code applied in this review. RC requests M1/M2 reports.

Native buffer summary probe compiled; build's last CPU check initially failed
because admission fixture extracts only CommitCommandBuffer and lacked the new
ReportMemoryBuffers symbol. Added explicit no-op stub to that admission-only
fixture and reran check_metal_inflight.py successfully (all prior checks passed).
Validation in work/analysis/native-buffer-summary-20261026/validation.json.
Next diagnostic command ready: bash prueba_buffers_nativos.command. No game run.

## October 26: native Metal RC integration

The active renderer is `a27f0ca477bd8356a015311d1eaa92e6f6a0b007`, the runtime
selected by bloodborne-mac `9c8ac5c` (0.9.0 RC1). Local SotC adaptations were
merged onto this revision, with the existing CPU translator `196ed99` deliberately
retained. New native device/queue initialization, capabilities, 3D texture support,
resource ownership routing and native-resolution classification are included.
The launcher selects `BB_METAL_NATIVE_BOOT=1` without Vulkan discovery. Explicit
`BB_METAL_NATIVE_BOOT=0` retains the previous Metal-on-Vulkan startup comparison;
the dedicated Vulkan check still uses the loader. These modes are not identical.

Four optional button-prompt imports preserve SotC's original icons (no Bloodborne
input-name replacement). Input handling itself stays on our existing SDL adapter.
Old workspace/build `84daa9f13e5c` remains available. Copied dependency install names
were relocated to the new prefix; old libraries were not edited. No game has been
launched as part of this update, and the observed Metal OOM is not claimed fixed.

Next manual trial: `bash prueba_runtime_actualizado.command`. This establishes a
new baseline: native boot, three balanced pool windows, default collector budget,
shim residency enabled, existing exact-shader barrier trial, resource observations,
single inflight command, skipped intro, GPU wait120s and global500s. Other optional
render/indirect/CPU probes are off. This is not a single-variable comparison with
the preceding single-pool/early-GC trial. Native buffer summaries distinguish the
shim's residency switch from the separate renderer address-buffer residency set.
Build/check evidence: `work/analysis/runtime-update-20261026/`.


Final validation October26 runtime update: build exit0;35renderer checks,26builder,22supervisor,2native-startup,2memory-profile,3adaptive tests;pad/gamepad and upstream upscaler tests pass.50/50 host symbols; final load/unload pass. No direct Vulkan/MoltenVK dependency or old prefix in libbbgpu. No game launched. Next: `bash prueba_runtime_actualizado.command`. Evidence: `work/analysis/runtime-update-20261026/validation.json`.

# Native startup fault205653Z

First updated runtime run confirms native adapter entry points,AppleM1/native
queue,noVMA allocator,Metal-owned buffers and Metal swapchain. SIGSEGV0xc at
0.490s/139 immediately after swapchain initialization log. Host PC10b941aec,
LR10b9fca08,x0=0; several other zero registers. Compatible with null+offset access,
but faulting instruction and function are not validated. No game shader barrier
marker,pool initialization or memory journal; no OOM. CPU context explicitly
unavailable outside translated code. Do not merge with historical guest/JIT
SIGSEGV category; classify as updated native-renderer startup regression.

No library slide/base captured, no OS report found by targeted probe-name search.
A trial symbolization using guessed base10b880000 yielded inconsistent symbols
(vmaBuildStatsString / CommandPool constructor), NOT a validated callsite or
stack and must not be used as attribution. Preserved exact dylib,sorted symbols,
full registers in log and SHA256 to enable later reliable symbolization.

Source review shows native mode intentionally has noVMA allocator. This alone
is not a defect; remaining VMA-dependent initialization is a hypothesis requiring
actual symbolization or bounded initialization checkpoints. Last message does
not prove the swapchain itself faulted. Next investigation should capture actual
module load address or function identity before modifying initialization.
No fix or game rerun performed during this analysis. Earlier CPU-only checks
and library load/unload did not initialize the renderer; native startup was
explicitly pending. Memory/OOM comparison cannot be assessed from this run.

Evidence: `work/analysis/native-startup-fault-20261026T205653Z`.
