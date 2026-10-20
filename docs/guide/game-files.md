# Game files

The tested input is **Shadow of the Colossus for PS4, CUSA08809, update 1.01**. Other regions
and updates are untested. Provide your own readable game dump; this repository does not
download, distribute or decrypt game files.

Set the input directory to the folder containing `eboot.bin`, `sce_sys/param.sfo`,
`sce_module/libc.prx`, `sce_module/libSceFios2.prx` and the game content:

```sh
export SOTC_GAME_ROOT="/absolute/path/to/CUSA08809"
bash packaging/test-arm64.sh --prepare
bash packaging/test-arm64.sh --linked-test
```

The local compatibility default is `game/CUSA08809/SotC-PC/Game/CUSA08809`.
Using `SOTC_GAME_ROOT` avoids moving an existing dump. Both preparation steps must use the same
input edition. Encrypted or compressed executable segments are unsupported.

Preparation reads the original executable and produces a private BBPROBE2 image in
`work/sotc-original/`. Linking adds the supplied original libc and Fios2 to a BBPROBE5 image
in `work/sotc-linked-arm64/`. Original FS-relative TLS instructions are preserved for bbcpu.
No executable input is patched in place.

The linked probe mounts the content under guest `/app0` and `/hostapp` through read-only
adapters. It has no save mount, renderer or audio path. Successful synthetic read tests do
not prove that the game has loaded content; check the actual file counters in the private log.

`game/`, `work/`, `logs/` and local ZIP archives are ignored. Never add game files, generated
images, extracted code, addresses from execution or private game logs to commits or issues.

The optional `--graphics-game-test` additionally requires your readable original
`libSceSystemGesture.sprx`. By default it reads `../sys_modules` relative to `SOTC_GAME_ROOT`;
`SOTC_SYSTEM_MODULE_ROOT` selects another directory. It links that module with the same
libc/Fios2 into a separate private `work/sotc-graphics-arm64` image, preserving executable
bytes and leaving all supplied files unchanged. No system module is distributed or fetched.
The default headless test does not need this additional module.
