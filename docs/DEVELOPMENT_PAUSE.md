# Development paused: unified-memory exhaustion

Updated October 22, 2026. Development is paused at the owner's request because
the native Apple M1 execution still exhausts the available Metal memory budget.
The first gameplay image has been reached, but the port is not playable yet.
The pause is a project decision, not a claim that every crash has one cause.

## Latest owner-run evidence

The October 22, 2026 **21:16 UTC** run stops after **185.769 seconds** with
`MTLCommandBufferErrorDomain`, status 5, error 8 (`Insufficient Memory`). Child
and supervisor both exit 23. The report-time native allocation is **12.170 GiB**
against **11.840 GiB** recommended. This is unified memory shared by CPU and GPU
on the owner's 16 GB M1, not additional dedicated graphics memory. The recommended
working set is a recommendation, not a guaranteed hard allocation threshold.

The capture records **1,095 presentation results**, which do not establish correct
pixels or playable gameplay. Requested scene resolution remains **1920x1080**,
with zero reduced-scene decisions and zero scene proxies. Presentation output
is predominantly **2560x1440**; the preceding windowed run mainly used 1280x720.
Native fullscreen is requested, but the log does not establish whether macOS
actually activated Game Mode.

There are **417 pipeline-creation failures / 381 distinct shader pairs**. Strict
adjacent-message analysis associates 368 events with `PerVertexKHR` rejection;
49 remain unattributed by that rule. Shader pairs do not represent complete
pipeline keys. **805 SRT offset errors** remain. There are no
`SRT finite choices` expansion messages, so this run provides no evidence that
the prepared finite-choice adaptation handled these cases.

Audio records zero queue-wait failures and a maximum submission gap of
**31.914 ms**; this does not validate audiovisual synchronization. No SIGBUS,
SIGSEGV or shader-metadata stop is recorded in this capture. Earlier corruption,
zero-filled shader metadata and audio-queue failures remain unresolved.

## Memory findings and limits

- Failure inventory: **415 live tracked images / 1,100.014 MiB**, zero observation
  errors and zero dropped memory-journal records in this capture.
- A **3 GiB dedicated upload allocation** remains in the staging sample. Its
  retention alone is not proof of a leak or permission to release it early.
- Staging and GC samples are approximately **4.545 s** and **6.988 s** old at
  failure. Their timing differs from the fresh native allocation observation.
- The native submission cap is active: limit/peak eight, four reserved or
  unfinished commands at failure. This does not establish that eight is optimal
  or that reducing it would resolve memory exhaustion.
- The failed command's CPU submission notes identify candidates; they do not
  pinpoint an executing GPU instruction or a resource responsible for the OOM.

The previous 21:10 UTC run stops after **71.990 s**, also on Metal error 8,
at **12.113 GiB**, with 406 pipeline failures and 805 SRT errors. Longer elapsed
time in the latest run does not establish a Game Mode benefit: output dimensions,
execution timing and workload differ. Neither run demonstrates an SRT fix.

## Prepared work versus native validation

CPU/build controls for bounded SRT data-read expansion pass locally, with shader
cache 26/9; native benefit remains unverified. In a separate, isolated PerVertex
experiment, 16 unambiguous real shader pairs produce 48 MSL sources accepted by
Apple's syntax compiler. Capture/replay is not integrated into the renderer;
syntax acceptance does not validate native bindings, resource lifetime or pixels.
Dependency pins have not been changed for those experiments.

## Conditions for resuming

1. Establish ownership and actual Metal-completion lifetimes for retained upload
   buffers, descriptors, images and reused command batches. Use synthetic controls
   before changing retention or releasing resources.
2. Reconcile fresh native allocation observations with tracked allocations and
   distinguish necessary live storage, driver overhead and avoidable retention.
   Current counters do not identify the whole difference as a leak.
3. Compare memory changes with the same scene/output dimensions and settings;
   preserve explicit failure reporting and previous private captures.
4. Review unsupported SRT expressions and the missing PerVertex renderer
   integration separately. Do not select arbitrary Phi edges, remove shader
   decorations or enable unchecked guest-memory access to bypass errors.

No game was launched to prepare this report. Raw logs, guest addresses, captured
shaders, dumps and game assets remain private. No causal relationship with
AnyPS5 or other upstream projects has been demonstrated.
