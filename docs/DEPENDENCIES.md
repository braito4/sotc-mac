# Current dependency versions

Checked against public upstream sources on **2026-10-27T13:07:46.280620+00:00**. This page is generated from [the dependency lock](../tools/dependencies.json).

The lock selects qualified runtime/CPU/reference commits and library releases or tags; newer reviewed heads are listed separately. An available version is not proof of correct Apple M1 rendering. Native game validation after this dependency update remains pending.

Integration: **October26 renderer a27f0ca native Metal adaptation complete; CPU196ed99 retained. Full ARM64 library build,35renderer checks,26builder tests,22supervisor tests,native startup and input tests pass.50/50 host imports and final load/unload pass. Direct Vulkan/MoltenVK linkage absent; native gameplay and OOM improvement unverified.**.

## Runtime and reference sources

The five owner-requested sources were refreshed on **2026-10-25T17:39:06.501411+00:00**. Selected commits and reviewed upstream heads are separate; the library release review above retains its own date. Additional source references appear below.

| Component | Previous pin | Selected head | Latest reviewed head | Scope |
|---|---|---|---|---|
| runtime | [84daa9f13e5c](https://github.com/bloodborne-native/bloodborne-runtime/commit/84daa9f13e5cf000d5fb59dc65803e28026fa77b) | [a27f0ca477bd](https://github.com/bloodborne-native/bloodborne-runtime/commit/a27f0ca477bd8356a015311d1eaa92e6f6a0b007) | [a27f0ca477bd](https://github.com/bloodborne-native/bloodborne-runtime/commit/a27f0ca477bd8356a015311d1eaa92e6f6a0b007) | Integrated source; exact commit is pinned. October26 RC native Metal migration adapted to SotC and CPU-tested; CPU translator retained at196ed99. Native game validation pending. |
| cpu | [b9652d75edc0](https://github.com/bloodborne-native/bloodborne-recomp/commit/b9652d75edc0a281f06cec0da6ff72e9f5150a70) | [196ed99d961f](https://github.com/bloodborne-native/bloodborne-recomp/commit/196ed99d961f084c772923ce7c62785e271d5f4c) | [196ed99d961f](https://github.com/bloodborne-native/bloodborne-recomp/commit/196ed99d961f084c772923ce7c62785e271d5f4c) | Integrated source; exact commit is pinned. Full owner-requested update October 25; adapted SotC integration and diagnostics. Native validation pending. |
| reference | [40ec52a5a246](https://github.com/AjncSystem/ShadowOfTheColossus-PC/commit/40ec52a5a246bbaf20402520fe8fc9682f0a30c0) | [75b48137bc33](https://github.com/AjncSystem/ShadowOfTheColossus-PC/commit/75b48137bc33c3f26242ed064a53cff28916c88f) | [75b48137bc33](https://github.com/AjncSystem/ShadowOfTheColossus-PC/commit/75b48137bc33c3f26242ed064a53cff28916c88f) | Preparation/reference checkout work/sotc-reference-75b48137bc33. scripts/prepare.py unchanged byte-for-byte; supported EU1.01 remains unchanged. Reviewed C1-C3 page allocator/texture reserve, image barriers, barycentric invariance, UI and FPS changes. Windows/x86 game patches and Vulkan renderer changes are references, not silently applied to native Metal. |

The SotC reference update includes preparation/reference sources. Its Windows renderer does not replace the macOS renderer. The nested-command coroutine correction is separately adapted into our renderer, alongside the existing ARM64/SRT, staging and scene-proxy fixes.

## Linked and recipe libraries

| Dependency | Previous recipe | Selected version | Latest available in the reviewed channel |
|---|---|---|---|
| vulkan-headers | v1.4.365 | [v1.4.366](https://github.com/KhronosGroup/Vulkan-Headers/commit/73814d006638aa00bd553215b054f0f02e457e22) | [v1.4.366](https://github.com/KhronosGroup/Vulkan-Headers/tree/v1.4.366) |
| vulkan-loader | v1.4.365 | [v1.4.366](https://github.com/KhronosGroup/Vulkan-Loader/commit/3187e1e4d463ddf7f60f0bbaf26c7d7b7e40ca34) | [v1.4.366](https://github.com/KhronosGroup/Vulkan-Loader/releases/tag/v1.4.366) |
| moltenvk | v1.4.2 | [v1.4.2](https://github.com/KhronosGroup/MoltenVK/commit/db66022459ffb663aa2b50f6b018bc2e124f5edf) | [v1.4.2](https://github.com/KhronosGroup/MoltenVK/releases/tag/v1.4.2) |
| spirv-cross | vulkan-sdk-1.4.363.0 | [vulkan-sdk-1.4.363.0](https://github.com/KhronosGroup/SPIRV-Cross/commit/f11ba9f0b21ba8fc15153d50a2a1ae31ab1cf8f7) | [vulkan-sdk-1.4.363.0](https://github.com/KhronosGroup/SPIRV-Cross/releases/tag/vulkan-sdk-1.4.363.0) |
| sdl3 | release-3.4.18 | [release-3.4.18](https://github.com/libsdl-org/SDL/commit/829a65d769d935c4852f8159e964312c0957260a) | [release-3.4.18](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.18) |
| fmt | 12.0.0 | [12.2.0](https://github.com/fmtlib/fmt/commit/1be298e1bd68957e4cd352e1f676f00e07dcfb57) | [12.2.0](https://github.com/fmtlib/fmt/releases/tag/12.2.0) |
| magic-enum | v0.9.7 | [v0.9.8](https://github.com/Neargye/magic_enum/commit/1384769c66bd16ec9bb1353f45fe8ec8ccc12dbd) | [v0.9.8](https://github.com/Neargye/magic_enum/releases/tag/v0.9.8) |
| robin-map | v1.4.0 | [v1.4.1](https://github.com/Tessil/robin-map/commit/bd14e6830a1474fed9d2d03f5c3b0683d818d540) | [v1.4.1](https://github.com/Tessil/robin-map/releases/tag/v1.4.1) |
| vma | v3.3.0 | [v3.5.0](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/commit/0f1a757da579831283413295cc7f3b47eb521c77) | [v3.5.0](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/releases/tag/v3.5.0) |
| miniz | 3.1.2 | [3.1.2](https://github.com/richgel999/miniz/commit/77d0dce8627735138c51770d1799a1ef48f2117d) | [3.1.2](https://github.com/richgel999/miniz/releases/tag/3.1.2) |
| xbyak | v7.43 | [v7.44](https://github.com/herumi/xbyak/commit/df112862583c2d327c1d9782fd28f53aa675f11d) | [v7.44](https://github.com/herumi/xbyak/tree/v7.44) |
| zydis | v4.1.1 | [v4.1.1](https://github.com/zyantific/zydis/commit/a2278f1d254e492f6a6b39f6cb5d1f5d515659dc) | [v4.1.1](https://github.com/zyantific/zydis/releases/tag/v4.1.1) |
| xxhash | v0.8.3 | [v0.8.4](https://github.com/Cyan4973/xxHash/commit/c87183a77d67f7d37e3d2d1b7eaac5e7c695e4f0) | [v0.8.4](https://github.com/Cyan4973/xxHash/releases/tag/v0.8.4) |
| boost | 1.89.0 | [1.92.0](https://github.com/boostorg/boost/commit/afdfa32505af73e3d208144b3f623f0096cb62b6) | [boost-1.92.0](https://github.com/boostorg/boost/releases/tag/boost-1.92.0) |
| ffmpeg | n7.1.2 | [n9.0.2](https://github.com/FFmpeg/FFmpeg/commit/946fcce07b6dcd0331c8cc609192aeff5e1924f8) | [n9.0.2](https://github.com/FFmpeg/FFmpeg/tree/n9.0.2) |
| spirv-headers | vendored snapshot | [v1.202609.0](https://github.com/KhronosGroup/SPIRV-Headers/commit/04fd3caa1e8267e4d95c806cad901181728e1006) | [v1.202609.0](https://github.com/KhronosGroup/SPIRV-Headers/releases/tag/v1.202609.0) |

- **vulkan-headers:** Headers and loader now both select1.4.366; device capability support is unchanged by header versions.

- **vulkan-loader:** Reviewed October 27. Stable dependency update; native Metal frame bypasses this component. Build and game validation tracked separately.

- **spirv-cross:** Latest Vulkan SDK tag; the GitHub latest-release endpoint reports an unrelated historical MoltenVK-1.1.5 release.

- **vma:** Reviewed October 27. Stable dependency update; native Metal frame bypasses this component. Build and game validation tracked separately.

- **xbyak:** Recipe header dependency; x86 emitter is not used by the ARM64 CPU path.

- **spirv-headers:** Installed system headers replace the runtime-vendored 1.5.5 snapshot for Sirit.

## Renderer submodules, transitive and host build dependencies

| Dependency | Selected | Latest reviewed | Scope |
|---|---|---|---|
| imgui | [v1.93.0](https://github.com/ocornut/imgui/commit/c1aaac65e896a45f81f47a4d68f9959e0f86c859) | v1.93.0 | Checked renderer override. |
| fsr-vulkan | [main](https://github.com/FireBurn/FSR-Vulkan/commit/c64f093404125e960813f95f1996c14780f0dd69) | main at the same revision; no release tags | Checked renderer override. |
| zycore | [v1.5.2](https://github.com/zyantific/zycore-c/commit/0b2432ced0884fd152b471d97ecf0258ff4d859f) | v1.5.2 | Zydis bundled dependency, installed for CPU and GPU; replaces the older CPU prefix metadata 1.5.0.0. |
| glslang | [16.6.0](https://github.com/KhronosGroup/glslang/commit/e1b562a8bed273a02f30b59b66a5d499793cede5) | 16.6.0 | Host shader compiler, built as ARM64. |
| actions/checkout | [v7.0.1](https://github.com/actions/checkout/commit/3d3c42e5aac5ba805825da76410c181273ba90b1) | v7.0.1 | Source-check CI action, pinned by commit; hosted runner execution is separate from local validation. |

The renderer uses installed current SPIRV-Headers instead of the older embedded copy. Some upstream package metadata keeps an older version string; exact source tags/commits above identify the selected source, rather than inferring it from pkg-config alone.

## Embedded and inactive components

- **sirit (runtime-vendored):** Sirit emitter source follows the current runtime commit; headers now come from the separately updated SPIRV-Headers package.
- **half (1.12.0):** Vendored header verified byte-identical to current ROCm 7.2.4; no source replacement needed. [Verified upstream header](https://github.com/ROCm/half/blob/rocm-7.2.4/include/half.hpp).
- **gcn-and-fonts (runtime-vendored):** Embedded GCN headers and menu fonts follow the selected runtime snapshot; not independently downloaded packages.
- **app-bash-python:** App-bundle runtimes are deliberately excluded from the ARM64 diagnostic recipe. System host tools are prerequisites, not vendored dependencies.
- **LibAtrac9:** Uninitialized upstream submodule, not built/linked by this diagnostic; not an active dependency.
- **kosmickrisp:** Tracked alternative driver; not installed. The active driver is MoltenVK.

macOS frameworks (Metal, MetalFX, CoreAudio and platform libraries), Clang/SDK, Git, CMake, Ninja, pkg-config and Python are host prerequisites. This update does not install or change system software. Original game/firmware modules remain private user input.

## Additional upstream references

| Project | Default branch | Latest reviewed head | Integration scope |
|---|---|---|---|
| bloodborne-mac | main | [9c8ac5c93bbf](https://github.com/bloodborne-native/bloodborne-mac/commit/9c8ac5c93bbf62be7a096134f363f014c42c044d) | October26 RC0.9.0; runtime pinned a27f0ca. Packaging remains SotC-specific; native Metal boot adapted. |
| bbport-mac | macos-native-0.6 | [a69f56af2069](https://github.com/bmy/bbport-mac/commit/a69f56af2069df45357e86c27396baa945b53231) | Selected copy batching and stale-proxy detection adapted; its Rosetta/KosmicKrisp architecture is not substituted. |
| MoltenVK development | main | [5189f8de86ec](https://github.com/KhronosGroup/MoltenVK/commit/5189f8de86ec365860f63bf1da736815cfaea5cd) | Development head tracked; selected installed release is v1.4.2. |
| SPIRV-Cross development | main | [300681bbcb83](https://github.com/KhronosGroup/SPIRV-Cross/commit/300681bbcb8365a8d1eacdacdbdd0249bbf1d6b1) | Development head tracked; selected compiler uses the latest SDK tag. |
| mesa-kosmickrisp | main | [52d87b35f3a4](https://github.com/shadexternals/mesa-kosmickrisp/commit/52d87b35f3a4fc8aff5acb4daaa548c8ff7870f1) | Alternative driver reference; not installed in the active renderer. |
| Pink-shadPS4 | main | [4a3511ba9aa3](https://github.com/luizgustavs/Pink-shadPS4/commit/4a3511ba9aa33b59ab0e57048013b0b85d29402a) | Permanent SotC/shadPS4 reference requested by the owner. Reviewed readback lifetime, pending label writes, optional GCN NaN/MAD and heap/stack workarounds; no Windows/NVIDIA-specific settings or code imported. |
| Box64 JIT comparison | main | [420eda69ff51](https://github.com/ptitSeb/box64/commit/420eda69ff51ab5d5a7cbc3dee2e70433be3411d) | Permanent owner-requested independent semantic/testing reference for CPU/JIT investigations. Compare minimized original inputs under local interpreter/JIT and Box64 where executable; resolve disputed results with native x86 or ISA semantics. Range-limited COSIM and no-dynarec diagnostics reviewed. Not installed, executed, linked or substituted as a CPU backend. |

## Open proposals

| Proposal | State | Exact reviewed head |
|---|---|---|
| [MoltenVK #2858](https://github.com/KhronosGroup/MoltenVK/pull/2858) | open / draft / unmerged | `6ed66f1b5d3575eed04b04e6332b389f398f9533` |
| [SPIRV-Cross #2699](https://github.com/KhronosGroup/SPIRV-Cross/pull/2699) | open / draft / unmerged | `81739edf516c2bef043ac61f510b4bd78e126a22` |
| [SPIRV-Cross #2697](https://github.com/KhronosGroup/SPIRV-Cross/pull/2697) | open / non-draft / unmerged | `aa7879476d018098bd2c1279893409ba61cf3f03` |
| [SotC reference #219](https://github.com/AjncSystem/ShadowOfTheColossus-PC/pull/219) | open / non-draft / unmerged; incorporated commit adapted separately | `e391e03e956808abcb178a9e714df06ad871be78` |

Open proposals are tracked separately from integrated released dependencies. Author tests on other machines are not our M1 validation.

## Reproduce and maintain

A separate versioned integration workspace preserves the preceding qualified build. The selected workspace key and exact required Git HEAD are recorded and checked independently against the lock. Local overlays and the Dear ImGui override are digest/revision checked. Dependency caches are separated by the version lock; installed markers contain the actual selected version, so an old empty marker cannot skip an update.

Run `python3 tools/dependency_inventory.py --check` to check this document and lock. After reviewing new public sources, update the lock, adapt/build/test, then regenerate with `python3 tools/dependency_inventory.py`. This command does not query GitHub, change pins or claim an online refresh.

See [upstream review](UPSTREAM_REVIEW.md), [build instructions](guide/building.md) and the [current handoff](DEVELOPMENT_HANDOFF.md#continuation-checkpoint) for validation and the native test boundary.
