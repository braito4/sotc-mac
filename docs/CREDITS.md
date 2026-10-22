# Credits and licenses

This project is built on the work of:

- [bloodborne-native](https://github.com/bloodborne-native): macOS ARM64 runtime and bbcpu.
  [bloodborne-mac](https://github.com/bloodborne-native/bloodborne-mac) also provides the README
  presentation used as this repository's reference.
  The pinned runtime's protected fault-field/stack diagnostics inform the local
  bounded [CPU fault context](CPU_DIAGNOSTICS.md); no game-specific layout is imported.
- [bbport / bloodborne_pc](https://github.com/deadinside28/bloodborne_pc) by **deadinside28**:
  foundational native PS4 game runtime and porting work.
- [bmy/bbport-mac](https://github.com/bmy/bbport-mac): reviewed buffer-copy batching
  and stale reduced-scene proxy detection, adapted through checked local overlays.
  The proxy correction adds deferred ownership and cache invalidation specific to
  this integration; see the [source review](UPSTREAM_REVIEW.md#reduced-scene-proxy-retirement).
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
- [AMD64 Architecture Programmer's Manual, Volume 4, revision 3.27](https://docs.amd.com/v/u/en-US/26568_3.27_APM_Vol4_PUB):
  the EXTRQ and INSERTQ entries define the SSE4a field extension and its synthetic oracle.
- [QEMU v10.1.0 SSE helpers](https://github.com/qemu/qemu/blob/v10.1.0/target/i386/ops_sse.h):
  reference for the explicit shift/mask/truncate compatibility policy when AMD leaves
  EXTRQ/INSERTQ overrunning fields undefined. This policy is not a hardware guarantee.
- [OpenOrbis PS4 Toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain):
  [user type definitions](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/include/orbis/_types/user.h)
  and [UserService declarations](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/include/orbis/UserService.h)
  provide the local user adapter's guest ABI reference.

The local typed-integer blit overlay extends bloodborne-runtime's native Metal
fullscreen-triangle blit implementation. Its integer shader bodies and synthetic
fixtures are original local additions, scoped against the
[Vulkan blit contract](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBlitImage.html).
No AnyPS5 or game shader code is copied.

## Pinned source revisions

| Project | Revision |
|---|---|
| bloodborne-runtime | `b2ff9cd0b982aa481fc48d4ce51b3cc7b4cd2548` |
| bloodborne-recomp | `b9652d75edc0a281f06cec0da6ff72e9f5150a70` |
| ShadowOfTheColossus-PC | `411fdd86dadce347dcc4443228b009ae9265a548` |
| README layout reference: bloodborne-mac | `134780d9544dc726061a21993011434c9d746406` |
| Zydis | `v4.1.1` |

The [current dependency inventory](DEPENDENCIES.md) lists all selected library,
submodule and host-tool versions. The nested-command coroutine correction is
adapted from AjncSystem's `c9be8142ac76`, extending completed-frame cleanup to our
two analogous self-test scopes. bmy's proxy detection and our local deferred
ownership/cache invalidation remain. Earlier per-commit backports are now part
of the selected runtime base; the consolidated checked overlay preserves their
credits and the independent SotC/ARM64 adaptations.

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
