# Current dependency versions

Checked against public upstream sources on **2026-10-22T10:51:27Z**. This page is generated from [the dependency lock](../tools/dependencies.json).

The lock selects the current runtime/CPU/reference heads and stable library releases or tags. An available version is not proof of correct Apple M1 rendering. Native game validation after this dependency update remains pending.

Integration: **integrated; fresh ARM64 renderer/dependency build, nine renderer source regressions, 22 builder tests, native CPU checks and full JIT/interpreter contracts pass**.

## Runtime and reference sources

| Component | Previous pin | Selected / latest reviewed head | Scope |
|---|---|---|---|
| runtime | [3170535f59ca](https://github.com/bloodborne-native/bloodborne-runtime/commit/3170535f59cadbe5ce9c38e6bb961ed5fd215194) | [b2ff9cd0b982](https://github.com/bloodborne-native/bloodborne-runtime/commit/b2ff9cd0b982aa481fc48d4ce51b3cc7b4cd2548) | Integrated source; exact commit is pinned. |
| cpu | [f749c155c2ff](https://github.com/bloodborne-native/bloodborne-recomp/commit/f749c155c2ff3bb932795de85c9c8fe9c4aa59ad) | [b9652d75edc0](https://github.com/bloodborne-native/bloodborne-recomp/commit/b9652d75edc0a281f06cec0da6ff72e9f5150a70) | Integrated source; exact commit is pinned. |
| reference | [2250f328feaf](https://github.com/AjncSystem/ShadowOfTheColossus-PC/commit/2250f328feafab953aac85e758e7bddb2e80e891) | [411fdd86dadc](https://github.com/AjncSystem/ShadowOfTheColossus-PC/commit/411fdd86dadce347dcc4443228b009ae9265a548) | Reference/preparation source; the game renderer remains the macOS runtime with selected SotC adaptations. |

The SotC reference update includes preparation/reference sources. Its Windows renderer does not replace the macOS renderer. The nested-command coroutine correction is separately adapted into our renderer, alongside the existing ARM64/SRT, staging and scene-proxy fixes.

## Linked and recipe libraries

| Dependency | Previous recipe | Selected version | Latest available in the reviewed channel |
|---|---|---|---|
| vulkan-headers | v1.4.365 | [v1.4.365](https://github.com/KhronosGroup/Vulkan-Headers/commit/c46850864f4661461b0f6cb9922c058ffea4915e) | [v1.4.365](https://github.com/KhronosGroup/Vulkan-Headers/tree/v1.4.365) |
| vulkan-loader | v1.4.365 | [v1.4.365](https://github.com/KhronosGroup/Vulkan-Loader/commit/f866657ff687a36d767cd7783bab800e31ebafaa) | [v1.4.365](https://github.com/KhronosGroup/Vulkan-Loader/tree/v1.4.365) |
| moltenvk | v1.4.2 | [v1.4.2](https://github.com/KhronosGroup/MoltenVK/commit/db66022459ffb663aa2b50f6b018bc2e124f5edf) | [v1.4.2](https://github.com/KhronosGroup/MoltenVK/releases/tag/v1.4.2) |
| spirv-cross | vulkan-sdk-1.4.363.0 | [vulkan-sdk-1.4.363.0](https://github.com/KhronosGroup/SPIRV-Cross/commit/f11ba9f0b21ba8fc15153d50a2a1ae31ab1cf8f7) | [vulkan-sdk-1.4.363.0](https://github.com/KhronosGroup/SPIRV-Cross/releases/tag/vulkan-sdk-1.4.363.0) |
| sdl3 | release-3.4.18 | [release-3.4.18](https://github.com/libsdl-org/SDL/commit/829a65d769d935c4852f8159e964312c0957260a) | [release-3.4.18](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.18) |
| fmt | 12.0.0 | [12.2.0](https://github.com/fmtlib/fmt/commit/1be298e1bd68957e4cd352e1f676f00e07dcfb57) | [12.2.0](https://github.com/fmtlib/fmt/releases/tag/12.2.0) |
| magic-enum | v0.9.7 | [v0.9.8](https://github.com/Neargye/magic_enum/commit/1384769c66bd16ec9bb1353f45fe8ec8ccc12dbd) | [v0.9.8](https://github.com/Neargye/magic_enum/releases/tag/v0.9.8) |
| robin-map | v1.4.0 | [v1.4.1](https://github.com/Tessil/robin-map/commit/bd14e6830a1474fed9d2d03f5c3b0683d818d540) | [v1.4.1](https://github.com/Tessil/robin-map/releases/tag/v1.4.1) |
| vma | v3.3.0 | [v3.4.0](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/commit/3aa921224c154a0d2c43912bc88e1c42ce1f7607) | [v3.4.0](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/releases/tag/v3.4.0) |
| miniz | 3.1.2 | [3.1.2](https://github.com/richgel999/miniz/commit/77d0dce8627735138c51770d1799a1ef48f2117d) | [3.1.2](https://github.com/richgel999/miniz/releases/tag/3.1.2) |
| xbyak | v7.43 | [v7.44](https://github.com/herumi/xbyak/commit/df112862583c2d327c1d9782fd28f53aa675f11d) | [v7.44](https://github.com/herumi/xbyak/tree/v7.44) |
| zydis | v4.1.1 | [v4.1.1](https://github.com/zyantific/zydis/commit/a2278f1d254e492f6a6b39f6cb5d1f5d515659dc) | [v4.1.1](https://github.com/zyantific/zydis/releases/tag/v4.1.1) |
| xxhash | v0.8.3 | [v0.8.4](https://github.com/Cyan4973/xxHash/commit/c87183a77d67f7d37e3d2d1b7eaac5e7c695e4f0) | [v0.8.4](https://github.com/Cyan4973/xxHash/releases/tag/v0.8.4) |
| boost | 1.89.0 | [1.92.0](https://github.com/boostorg/boost/commit/afdfa32505af73e3d208144b3f623f0096cb62b6) | [boost-1.92.0](https://github.com/boostorg/boost/releases/tag/boost-1.92.0) |
| ffmpeg | n7.1.2 | [n9.0.2](https://github.com/FFmpeg/FFmpeg/commit/946fcce07b6dcd0331c8cc609192aeff5e1924f8) | [n9.0.2](https://github.com/FFmpeg/FFmpeg/tree/n9.0.2) |
| spirv-headers | vendored snapshot | [v1.202609.0](https://github.com/KhronosGroup/SPIRV-Headers/commit/04fd3caa1e8267e4d95c806cad901181728e1006) | [v1.202609.0](https://github.com/KhronosGroup/SPIRV-Headers/releases/tag/v1.202609.0) |

- **spirv-cross:** Latest Vulkan SDK tag; the GitHub latest-release endpoint reports an unrelated historical MoltenVK-1.1.5 release.

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
| bloodborne-mac | main | [ab1f0c57d1e2](https://github.com/bloodborne-native/bloodborne-mac/commit/ab1f0c57d1e25cc130c59be9a874e98099822cc4) | App/reference project, not a linked dependency. |
| bbport-mac | macos-native-0.6 | [a69f56af2069](https://github.com/bmy/bbport-mac/commit/a69f56af2069df45357e86c27396baa945b53231) | Selected copy batching and stale-proxy detection adapted; its Rosetta/KosmicKrisp architecture is not substituted. |
| MoltenVK development | main | [5189f8de86ec](https://github.com/KhronosGroup/MoltenVK/commit/5189f8de86ec365860f63bf1da736815cfaea5cd) | Development head tracked; selected installed release is v1.4.2. |
| SPIRV-Cross development | main | [300681bbcb83](https://github.com/KhronosGroup/SPIRV-Cross/commit/300681bbcb8365a8d1eacdacdbdd0249bbf1d6b1) | Development head tracked; selected compiler uses the latest SDK tag. |
| mesa-kosmickrisp | main | [52d87b35f3a4](https://github.com/shadexternals/mesa-kosmickrisp/commit/52d87b35f3a4fc8aff5acb4daaa548c8ff7870f1) | Alternative driver reference; not installed in the active renderer. |

## Open proposals

| Proposal | State | Exact reviewed head |
|---|---|---|
| [MoltenVK #2858](https://github.com/KhronosGroup/MoltenVK/pull/2858) | open / draft / unmerged | `6ed66f1b5d3575eed04b04e6332b389f398f9533` |
| [SPIRV-Cross #2699](https://github.com/KhronosGroup/SPIRV-Cross/pull/2699) | open / draft / unmerged | `81739edf516c2bef043ac61f510b4bd78e126a22` |
| [SPIRV-Cross #2697](https://github.com/KhronosGroup/SPIRV-Cross/pull/2697) | open / non-draft / unmerged | `aa7879476d018098bd2c1279893409ba61cf3f03` |
| [SotC reference #219](https://github.com/AjncSystem/ShadowOfTheColossus-PC/pull/219) | open / non-draft / unmerged; incorporated commit adapted separately | `e391e03e956808abcb178a9e714df06ad871be78` |

Open proposals are tracked separately from integrated released dependencies. Author tests on other machines are not our M1 validation.

## Reproduce and maintain

A separate versioned integration workspace preserves the preceding qualified build. Its directory key stays fixed during this integration; the exact required Git HEAD is checked independently against the lock. Local overlays and the Dear ImGui override are digest/revision checked. Dependency caches are separated by the version lock; installed markers contain the actual selected version, so an old empty marker cannot skip an update.

Run `python3 tools/dependency_inventory.py --check` to check this document and lock. After reviewing new public sources, update the lock, adapt/build/test, then regenerate with `python3 tools/dependency_inventory.py`. This command does not query GitHub, change pins or claim an online refresh.

See [upstream review](UPSTREAM_REVIEW.md), [build instructions](guide/building.md) and the [current handoff](DEVELOPMENT_HANDOFF.md#continuation-checkpoint) for validation and the native test boundary.
