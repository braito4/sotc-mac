# Fast startup

The owner launcher enables the engine's existing `+nointro` flag by default.
The SotC reference uses this flag for repeatable benchmarks; its benchmark notes
describe starting outside the Shrine of Worship. The exact outcome on this port
still needs an owner run. This is not a promise to bypass every menu or loading
screen, and it does not load or modify a save. The first two completed owner
logs confirm exact-profile validation and option application. They do not alone
establish which screens were skipped or successful gameplay entry.

```sh
SOTC_SKIP_INTRO=1 bash prueba_grafica_nativa.command base
```

Restore the previous startup path with:

```sh
SOTC_SKIP_INTRO=0 bash prueba_grafica_nativa.command base
```

A private profile checks the complete prepared-input hash, exact instruction,
executable segment and original command-line literal. Before guest execution,
the loader redirects that instruction to a separate read-only string containing
the original option plus `+nointro`. Initialization still runs normally. Original
game files and the prepared input on disk are unchanged. A mismatch stops startup.
The original-resolution rendering and CPU comparison settings remain active.

Each run records the setting and private validation report. Intro skipping
changes the workload, so compare runs only with matching startup settings. A
synthetic native fixture verifies redirection and refusal cases; profile tests
check identity and bounds. The agent did not launch the game or GPU; the native
evidence above comes from owner runs.
