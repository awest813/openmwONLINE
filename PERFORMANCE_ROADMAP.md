# OpenMW Performance & Impact Fix Roadmap

This document tracks planned performance improvements and high-impact fixes for
OpenMW, with a focus on achieving smooth, fully playable Morrowind in the browser.
Each item includes an impact rating, estimated effort, risk level, and the specific
files involved.

> **Goal:** Deliver a stable 30+ fps experience for the complete Morrowind game
> running in modern web browsers (Chrome, Firefox, Edge) via the WebAssembly port,
> while also benefiting the desktop engine.

---

## Priority Legend

| Symbol | Meaning |
|--------|---------|
| 🔴 | High impact — directly affects browser playability |
| 🟡 | Medium impact — noticeable improvement in specific scenarios |
| 🟢 | Low impact / code quality |
| ✅ | Completed |
| ⏳ | In progress |
| ☐  | Not started |

---

## Status Overview

| Area | Impact | Status |
|------|--------|--------|
| Navigation mesh thread tuning | 🔴 High | ✅ Completed |
| Small-feature culling defaults | 🟡 Medium | ✅ Completed |
| Shadow caster frustum culling | 🔴 High | ⏳ Investigated — see findings below |
| RigGeometry / GPU pre-compilation | 🔴 High | ☐ Not started |
| Actor-actor collision batching | 🔴 High | ☐ Not started |
| Terrain composite-map resolution | 🟡 Medium | ☐ Not started |
| NPC sound-pointer caching | 🟡 Medium | ☐ Not started |
| Resource cache expiry tuning | 🟡 Medium | ☐ Not started |
| Particle system transform HACKs | 🟡 Medium | ☐ Not started |
| Physics barrier modernisation (std::barrier) | 🟢 Low | ☐ Not started |
| Water refraction legacy code removal | 🟢 Low | ☐ Not started |
| Fragment-shader alpha-test benchmark | 🟢 Low | ☐ Not started |
| Terrain LOD vertex code consolidation | 🟢 Low | ☐ Not started |

---

## High-Impact Fixes 🔴

### 1. Shadow Caster Frustum Culling ⏳

**File**: `components/sceneutil/mwshadowtechnique.cpp` (around line 1830)

**Problem**: The shadow technique potentially renders shadow maps for shadow
casters that fall outside the intersection of the camera view frustum and the
light volume, wasting GPU time on geometry that cannot contribute to a visible
shadow.

**Investigation findings**: The existing `VDSMCameraCullCallback` already pushes
a `Polytope`-based `CullingSet` (line 376) derived from
`computeLightViewFrustumPolytope()`, which starts from the main camera frustum
and removes planes facing away from the light to allow shadow casters behind
the camera. `ComputeLightSpaceBounds` also applies the same polytope (line
1356). Small-feature culling is explicitly disabled for the shadow camera (line
684) because perspective correction can make "small" objects cast large shadows.

**Remaining work**: The polytope may be tighter in specific cases. Investigate
whether the frustum far-plane distance can be reduced for cascaded shadow maps
so that shadow casters well beyond the viewing distance are excluded earlier.

**Risk**: Low-medium. A tighter polytope could cause shadow pop-in at cascade
boundaries.

**Effort estimate**: 4–6 hours (reduced from original 3–6 due to existing work)

**Expected gain**: 5–15 % reduction in shadow map draw calls, mainly in
exterior cells where the light volume extends far behind the camera.

---

### 2. RigGeometry Refactor — Eliminate Useless Clones & Add GPU Pre-compilation

**File**: `components/sceneutil/riggeometry.hpp`, `riggeometry.cpp`

**Problem** (documented in header lines 14–20):
- Template RigGeometries accumulate useless geometry clones during scene graph
  compilation.
- `compileGLObjects` is not implemented, so vertex/index buffers are not uploaded
  to the GPU until the first draw call, causing a one-frame hitch when a new NPC
  or creature enters view.
- Significant code duplication with `MorphGeometry`.

**Fix**:
1. Rework the clone strategy so only the minimal per-instance data is duplicated.
2. Implement `compileGLObjects` so that the OSG compile traversal uploads buffers
   to VRAM proactively.
3. Factor shared logic into a common base with `MorphGeometry`.

**Risk**: Medium. The skinning path is exercised by every animated character;
thorough regression testing against combat and NPC-dense cells is required.

**Effort estimate**: 8–14 hours

**Expected gain**: Eliminates per-NPC load hitches; reduces VRAM allocation
overhead when many NPCs are on screen simultaneously.

---

### 3. Actor-Actor Collision Detection — Reduce `contactPairTest` Overhead

**File**: `apps/openmw/mwphysics/actorconvexcallback.cpp` (lines 42–46)

**Problem**: Actor-versus-actor collision is resolved by calling Bullet's
`contactPairTest` individually for every pair of overlapping actors. The code
itself notes this is "absolutely terrible" in a comment. At crowd-dense
locations (e.g. Balmora or Vivec city cantons) this creates O(n²) Bullet
queries per physics step.

**Fix**:
1. Replace pair-wise `contactPairTest` calls with a single broadphase query
   that returns all overlapping actor pairs in one pass.
2. Alternatively, maintain a list of known-overlapping pairs from the previous
   frame and only re-test pairs whose bounding boxes still intersect
   (temporal coherence).

**Risk**: Medium. Collision semantics must be preserved exactly; incorrect
changes here can cause actors to walk through each other or get stuck.

**Effort estimate**: 6–10 hours

**Expected gain**: Near-linear rather than quadratic scaling in crowd scenes;
most noticeable in Vivec, Balmora market, and dungeon encounters.

---

### 4. Navigation Mesh — Per-Platform Thread Count Tuning ✅

**File**: `components/detournavigator/asyncnavmeshupdater.cpp`,
`components/detournavigator/settings.hpp`,
`components/settings/categories/navigator.hpp`,
`files/settings-default.cfg`

**Problem**: The async nav mesh updater spawned a fixed 1 thread regardless of
available hardware.

**Fix** (completed):
1. Changed the default config value from `1` to `0` (meaning auto-detect).
2. In `settings.cpp`, when the value is 0, it resolves to
   `max(1, hardware_concurrency / 2)` using `std::thread::hardware_concurrency()`.
3. Updated the config file comment to document `0 = auto-detect` and WASM
   behaviour.
4. In the WASM single-threaded build, threads are not spawned regardless of
   this value (guarded by `#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)`).

**Risk**: Low. Thread count is still user-configurable; this only changes the
default value logic. Explicit numeric values continue to work as before.

**Expected gain**: Faster nav mesh rebuild after fast-travel or cell transitions
on multi-core hardware; reduced CPU contention on low-core hardware.

---

## Medium-Impact Fixes 🟡

### 5. Small-Feature Culling — WASM-Optimised Default ✅

**File**: `apps/openmw/engine.cpp`, `apps/openmw/mwrender/renderingmanager.cpp`,
`files/settings/settings-default.cfg`

**Problem**: The default `camera/small_feature_culling_pixel_size` of 2.0 was
tuned for desktop rendering at high viewing distances. The WASM build already
caps viewing distance at 4096 units, but the culling threshold was not adjusted
to match.

**Fix** (completed):
1. In the WASM performance defaults block (`engine.cpp`), the pixel size is
   raised to 4.0 if the user has not set a higher value. This removes more
   distant small objects (barrels, rocks, clutter) with minimal visual impact
   at the capped viewing distance.
2. The desktop default (2.0) is unchanged.

**Risk**: Very low. The setting is already exposed and user-overridable.

**Expected gain**: 5–15 % reduction in draw calls for exterior cells populated
with many small props, directly improving browser frame rates.

---

### 6. Terrain Composite Map Resolution — Smarter Default Scaling

**File**: `apps/openmw/mwrender/renderingmanager.cpp`, `components/terrain/`

**Problem**: `terrain/composite_map_resolution` controls the texture resolution
of baked terrain composites. A high resolution value forces the GPU to maintain
large textures even for distant terrain chunks that would never be seen at that
detail level.

**Fix**:
1. Apply a resolution scale-down for LOD chunks beyond a configurable distance.
2. Ensure the composite map is regenerated only when the camera has moved far
   enough to warrant an update (hysteresis to avoid repeated re-bakes on
   micro-movement).

**Risk**: Low-medium. Visual regression is possible if the scale-down is too
aggressive; add a setting to disable the distance scaling.

**Effort estimate**: 4–6 hours

**Expected gain**: Reduced VRAM consumption and fewer terrain re-bakes during
normal movement; most noticeable in Ashlands and Grazelands exterior cells.

---

### 7. NPC Sound Pointer Caching

**File**: `apps/openmw/mwrender/npcanimation.cpp`

**Problem**: For each NPC the animation system looks up a `SoundPtr` every
frame to decide whether to play footstep or clothing-rustle sounds. This lookup
goes through the sound manager's map every animation update tick even when the
NPC is not moving.

**Fix**: Cache the `SoundPtr` per NPC on first acquisition and invalidate it
only when the NPC's equipment or movement state changes.

**Risk**: Very low. The sound pointer is already used correctly; this only
changes when the lookup happens.

**Effort estimate**: 1–2 hours

**Expected gain**: Removes redundant map lookups proportional to the number of
NPCs in view; helps in NPC-dense interior cells.

---

### 8. Resource Cache Expiry — Memory-Pressure-Aware Eviction

**File**: `components/resource/resourcemanager.hpp`,
`components/resource/scenemanager.cpp`

**Problem**: The resource cache uses a fixed expiry time to decide when to drop
unused assets. Under memory pressure (e.g. on the WASM build with 512 MB
allocated, or on 32-bit platforms) the fixed expiry may be too long, leaving
stale meshes, textures, and sounds in memory longer than necessary.

**Fix**:
1. Query available system memory at the start of each cache-purge cycle
   (platform-specific; `sysinfo` on Linux, `GlobalMemoryStatusEx` on Windows,
   `__builtin_wasm_memory_size` + allocation watermark on WASM).
2. If available memory is below a configurable threshold (default: 20 % of total
   physical RAM), halve the expiry time for that cycle.
3. Log a debug message when the memory-pressure path triggers so users can
   diagnose stutters.

**Risk**: Low-medium. Overly aggressive eviction can cause assets to be reloaded
more often, introducing micro-stutters. Gated by a tunable threshold.

**Effort estimate**: 4–6 hours

**Expected gain**: Prevents out-of-memory crashes and reduces page-fault stalls
on memory-constrained systems; most useful for the WASM and 32-bit builds.

---

### 9. Particle System Transform — Remove HACKs in SceneManager

**File**: `components/resource/scenemanager.cpp` (lines 72 and 88)

**Problem**: Two `HACK` comments mark workarounds for missing OSG API:
- Line 72: `InverseWorldMatrix` transform attached to particle systems is
  silently ignored, which can cause world-space particle effects (flames, smoke)
  to appear at the wrong position when the parent node is translated at runtime.
- Line 88: `ParticleSystem::getReferenceFrame()` does not exist in the OSG
  version vendored by OpenMW, so reference-frame queries fall back to a
  hard-coded assumption.

**Fix**:
1. Check whether the vendored OSG fork (`extern/` or `FetchContent`) already
   exposes `getReferenceFrame()`; if so, remove the workaround.
2. Implement proper `InverseWorldMatrix` handling by traversing the scene graph
   upward and computing the inverse transform explicitly.
3. Add a regression test using a particle effect attached to a moving object.

**Risk**: Low-medium. Particle positioning is rarely noticed unless the camera
is close to the emitter. A dedicated visual test is recommended.

**Effort estimate**: 3–5 hours

**Expected gain**: Correctness fix that also removes an unnecessary matrix
multiply on every particle update tick.

---

## Low-Impact / Code-Quality Fixes 🟢

### 10. Physics Barrier — Modernise to `std::barrier` (C++20)

**File**: `apps/openmw/mwphysics/mtphysics.hpp`, `mtphysics.cpp`

**Problem**: The physics threading system implements its own barrier primitive
(`mPreStepBarrier`, `mPostStepBarrier`, `mPostSimBarrier`) rather than using the
standard `std::barrier` introduced in C++20. OpenMW already requires C++17 and
is moving toward C++20.

**Fix**: Replace the custom barrier with `std::barrier<>` once C++20 is the
project minimum. Wrap in a compile-time `#if __cplusplus >= 202002L` guard to
keep the old path available during transition.

**Risk**: Very low. Behaviour is identical; the standard library implementation
is typically better optimised for the target platform.

**Effort estimate**: 2–3 hours

**Expected gain**: Slight reduction in synchronisation overhead on physics-heavy
scenes; mainly a code-quality improvement.

---

### 11. Water Rendering — Remove Legacy Refraction Scale Code

**File**: `apps/openmw/mwrender/water.cpp`

**Problem**: Refraction scaling code that predates the current water shader
system is still compiled and executed every frame (referenced in Issue #5709).
This code has no visible effect but occupies shader uniform bandwidth and
developer attention.

**Fix**: Delete the legacy refraction scale uniform upload and any dead shader
branches that read it. Update the water shader to remove the unused uniform
declaration.

**Risk**: Very low. The code has no observable effect; its removal cannot change
visual output.

**Effort estimate**: 1–2 hours

**Expected gain**: Cleaner water shader; eliminates one uniform upload per frame
per water surface.

---

### 12. Shadow Shader — Benchmark Fragment Discard vs Early Alpha Testing

**File**: `components/sceneutil/mwshadowtechnique.cpp`

**Problem**: For alpha-tested geometry (foliage, fences, cloth), the shadow
shader can either:
- Sample the alpha texture and call `discard` in the fragment shader, or
- Rely on the depth pre-pass to eliminate occluded fragments before the alpha
  test.

The relative performance of these two approaches varies by GPU vendor and driver.
No benchmark data is documented for OpenMW's target hardware range.

**Fix**:
1. Use the existing OSG stats infrastructure (`OPENMW_OSG_STATS_FILE`) to
   capture frame times with each approach across a foliage-heavy cell
   (e.g. Grazelands).
2. Document the results in this roadmap and in `scripts/HOWTO-benchmark.md`.
3. Set the faster approach as the default and expose the other as a
   `settings-default.cfg` option.

**Risk**: None for the benchmarking step. Changing the default carries low risk
and can be reverted by a config change.

**Effort estimate**: 2–4 hours

**Expected gain**: Potentially 5–10 % improvement in shadow map render time for
exterior cells with heavy vegetation.

---

### 13. Terrain LOD — Consolidate VertexLodMod Code

**File**: `components/terrain/chunkmanager.cpp`

**Problem**: A `TODO` comment notes that `vertexLodMod` logic is scattered across
multiple call sites within the chunk manager. This makes it difficult to reason
about LOD behaviour and means cache-friendly data layouts are harder to achieve.

**Fix**: Extract all `vertexLodMod` calculations into a single
`ChunkManager::applyVertexLodMod()` method called from one place. This is a
pure refactor with no behaviour change.

**Risk**: Very low. Pure refactor; can be validated by comparing terrain
rendering output before and after.

**Effort estimate**: 1–2 hours

**Expected gain**: Code quality improvement; enables future LOD optimisations to
be made in one place.

---

## Profiling Infrastructure

Before starting any of the above items, it is recommended to establish a baseline
using the existing tooling:

```bash
# Record frame stats for one game-minute in Seyda Neen
OPENMW_OSG_STATS_FILE=baseline.csv \
OPENMW_OSG_STATS_LIST="engine;times;physicsworker_time_taken;mechanics_time_taken" \
./openmw --skip-menu --load-savegame my_save.ess

# Analyse and plot the stats
python3 scripts/osg_stats.py baseline.csv
```

Key metrics to compare before and after each fix:

| Metric | Description |
|--------|-------------|
| `rendering` | Total GPU draw time per frame |
| `physicsworker_time_taken` | Async physics step duration |
| `mechanics_time_taken` | Game mechanics update time |
| `engine` | Total main-thread engine time |
| `frame_rate` | Frames per second (aggregated) |

See `scripts/HOWTO-benchmark.md` for full instructions.

---

## Recommended Fix Order

Completed items are shown for reference. The remaining order minimises risk
while maximising early gains toward fully playable browser performance:

1. ~~**Nav mesh thread tuning** (#4)~~ ✅
2. ~~**Small-feature culling defaults** (#5)~~ ✅
3. **NPC sound-pointer caching** (#7) — isolated change, very low risk.
4. **Water legacy code removal** (#11) — dead code removal only.
5. **Terrain LOD consolidation** (#13) — pure refactor, enables future work.
6. **Shadow caster frustum tightening** (#1) — investigate polytope bounds.
7. **Resource cache memory-pressure eviction** (#8) — critical for WASM 512 MB
   memory limit.
8. **Actor-actor collision** (#3) — requires physics testing.
9. **RigGeometry refactor** (#2) — largest change; do last to avoid blocking
   other work.

---

## How to Contribute

1. Pick an item from the table above that is marked ☐.
2. Create a branch named `perf/<short-description>`.
3. Record a before/after benchmark using the profiling instructions above and
   include the results in your merge request description.
4. Tag the MR with the `performance` label and link this roadmap item.
