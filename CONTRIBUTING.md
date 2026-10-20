# Contributing

Read [Development status](docs/STATUS.md) and [Building](docs/guide/building.md) first.
Use English for public documentation, issues and pull requests.

Keep changes specific and describe the observed behavior, the ABI/error contract and the
checks performed. Runtime changes should include meaningful synthetic x86-to-ARM64 tests
where possible and run in both JIT and interpreter modes. Report actual game progress
separately from synthetic results.

Use the repository that owns the change: SotC adapters here, general CPU/runtime changes
upstream when appropriate. Preserve credit to bloodborne-native, bbport/deadinside28,
shadPS4 and the SotC reference project. Keep dependency revisions explicit.

Never commit or attach game executables, extracted/generated game code, private images,
game addresses, full private game logs, screenshots, recordings or saves. Summarize the
observed API or symptom without those artifacts. Synthetic fixtures must be original test
code and independent of game data.

Before a contribution:

```sh
bash packaging/test-arm64.sh --contracts-test
python3 tools/check_repository.py
```

Changes to the CPU bridge also need relevant native emitter/fault checks. Record limitations
honestly; the current project cannot run gameplay or validate graphics.
