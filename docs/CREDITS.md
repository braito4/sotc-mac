# Credits and licenses

This project is built on the work of:

- [bloodborne-native](https://github.com/bloodborne-native): macOS ARM64 runtime and bbcpu.
  [bloodborne-mac](https://github.com/bloodborne-native/bloodborne-mac) also provides the README
  presentation used as this repository's reference.
- [bbport / bloodborne_pc](https://github.com/deadinside28/bloodborne_pc) by **deadinside28**:
  foundational native PS4 game runtime and porting work.
- [shadPS4](https://github.com/shadps4-emu/shadPS4): PS4 runtime foundations and API reference,
  including the fiber HLE on which the SotC reference is based, the internal module-loader
  ABI, DiscMap query/error definitions, event-flag contracts, and dialog and network ABI/error
  definitions, UserService error definitions, GNM ABI/error definitions, SystemService safe-area layouts/errors, and VideoOut ABI/errors. Unavailable-service error policies
  are specific to this diagnostic.
- [AjncSystem/ShadowOfTheColossus-PC](https://github.com/AjncSystem/ShadowOfTheColossus-PC):
  SotC input preparation, module linking and runtime reference, including fiber API layouts
  and behavior, direct-memory availability handling and the C event-flag reference.
- [Zydis](https://github.com/zyantific/zydis) and [Zycore](https://github.com/zyantific/zycore-c):
  instruction decoding dependencies, under their upstream MIT licenses.
- [Intel Software Developer's Manual, Volume 2B](https://cdrdv2-public.intel.com/774487/253667-sdm-vol-2b.pdf):
  section 4.1 and the PCMPISTRM instruction entry define the string-mask extension's semantics.
- [OpenOrbis PS4 Toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain):
  [user type definitions](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/include/orbis/_types/user.h)
  and [UserService declarations](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/include/orbis/UserService.h)
  provide the local user adapter's guest ABI reference.

## Pinned source revisions

| Project | Revision |
|---|---|
| bloodborne-runtime | `3170535f59cadbe5ce9c38e6bb961ed5fd215194` |
| bloodborne-recomp | `f749c155c2ff3bb932795de85c9c8fe9c4aa59ad` |
| ShadowOfTheColossus-PC | `2250f328feafab953aac85e758e7bddb2e80e891` |
| README layout reference: bloodborne-mac | `134780d9544dc726061a21993011434c9d746406` |
| Zydis | `v4.1.1` |

The project code is licensed under **GNU GPL version 2 or later**; see [LICENSE](../LICENSE).
Upstream submodules retain their own license files and attribution. The memory, CPU context
and vector adapters reuse pinned upstream implementations rather than copying them into a
competing runtime. The memory adapter now includes a digest-checked private copy with growable
GPU notification storage; its allocator and hook dispatch stay upstream. The source checkout
remains unchanged. Fiber adapter comments identify its upstream reference.

No game code, assets, generated game functions, saves or executable images are distributed.

The isolated GPU builder uses bloodborne-runtime's pinned renderer and macOS dependency
recipes (including the bbport and shadPS4 foundations), with a checked build-only subset for
ARM64 renderer dependencies. It excludes the app-bundle runtimes and does not alter or claim
authoring of the upstream renderer, shader recompiler, FSR-Vulkan or Dear ImGui sources.

The isolated renderer now includes three reviewed upstream backports for auxiliary
tessellation locations, cache invalidation and Metal resource lifetime. Their exact
commits, adaptations and regression evidence are recorded in the
[upstream review](UPSTREAM_REVIEW.md); patch files preserve upstream license notices.
