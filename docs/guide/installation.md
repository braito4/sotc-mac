# Installation

There is no released `.app` or installer. Build and run the developer diagnostic from source.

Requirements:

- An Apple Silicon Mac and a Terminal running as `arm64`.
- Xcode Command Line Tools, Git, CMake, Ninja and Python 3.
- Network access for the initial upstream/dependency checkout, unless supplied locally.
- Your own readable CUSA08809 1.01 dump for game-based probes only.

Follow [Building from source](building.md), then run commands from the repository root.

| Command | Purpose | Game input |
|---|---|---|
| `bash packaging/test-arm64.sh --check` | Inspect the local environment | No |
| `bash packaging/test-arm64.sh --fetch` | Initialize pinned upstream sources | No |
| `bash packaging/test-arm64.sh --test-emitter` | Test ARM64 instruction encoding | No |
| `bash packaging/test-arm64.sh --test` | Build bbcpu; test emitter and JIT fault recovery | No |
| `bash packaging/test-arm64.sh --contracts-test` | Test synthetic runtime contracts in both CPU modes | No |
| `bash packaging/test-arm64.sh --prepare` | Prepare a private executable image | Yes |
| `bash packaging/test-arm64.sh --entry-test` | Stop the original entry at its first unresolved import | Yes |
| `bash packaging/test-arm64.sh --bootstrap-test` | Run initial contracts without linking libc/Fios2 | Yes |
| `bash packaging/test-arm64.sh --linked-test` | Test real shared-memory maps, link libc/Fios2, compare startup stops | Yes |

The existing `prueba_cpu_arm64.command` forwards all options to the English test script.
Generated binaries and images are stored in `work/`; private logs are written to `logs/`.
Neither directory is included in the repository.
