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
| RigGeometry / GPU pre-compilation | 🔴 High | ✅ Completed |
| Actor-actor collision batching | 🔴 High | ✅ Completed (bounding-sphere pre-rejection) |
| Terrain composite-map resolution | 🟡 Medium | ✅ Completed (WASM cap to 256; object paging disabled) |
| NPC sound-pointer caching | 🟡 Medium | ✅ Completed |
| Resource cache expiry tuning | 🟡 Medium | ✅ Completed (WASM memory-pressure eviction) |
| Object paging default (WASM) | 🔴 High | ✅ Completed (disabled by default in browser builds) |
| Frame-rate cap (WASM) | 🟡 Medium | ✅ Completed (capped at 60 fps in browser builds) |
| Critical memory threshold + HUD | 🔴 High | ✅ Completed (92% → IDBFS sync + HUD warning) |
| Canvas resize observer | 🟡 Medium | ✅ Completed (ResizeObserver + window resize fallback) |
| Particle system transform HACKs | 🟡 Medium | ☐ Not started |
| Physics barrier modernisation (std::barrier) | 🟢 Low | ☐ Not started |
| Water refraction legacy code removal | 🟢 Low | ✅ Completed |
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

### 2. RigGeometry Refactor — Eliminate Useless Clones & Add GPU Pre-compilation ✅

**File**: `components/sceneutil/riggeometry.hpp`, `riggeometry.cpp`

**Problem** (documented in header lines 14–20):
- Template RigGeometries accumulate useless geometry clones during scene graph
  compilation.
- `compileGLObjects` is not implemented, so vertex/index buffers are not uploaded
  to the GPU until the first draw call, causing a one-frame hitch when a new NPC
  or creature enters view.
- Significant code duplication with `MorphGeometry`.

**Fix** (completed):
1. Implemented `compileGLObjects` so that the OSG compile traversal proactively
   uploads the first double-buffered geometry instance (including shared static
   arrays — texcoords, index buffers — and the dynamic vertex/normal/tangent VBOs
   in bind-pose form).  The dynamic data is overwritten by the CPU skinning pass
   on the first update traversal; the VBO allocations are already resident on the
   GPU so the allocation stall on first draw is eliminated.
2. Items 1 and 3 (useless clone removal and MorphGeometry factoring) remain as
   future work; they carry higher risk and require separate testing.

**Risk**: Low for the `compileGLObjects` change alone.  The dynamic arrays are
overwritten every frame regardless; pre-uploading them in bind pose does not
affect the rendered output.

**Expected gain**: Eliminates per-NPC load hitches; reduces VRAM allocation
overhead when many NPCs are on screen simultaneously.

---

### 3. Actor-Actor Collision Detection — Reduce `contactPairTest` Overhead ✅

**File**: `apps/openmw/mwphysics/actorconvexcallback.cpp` (lines 42–46)

**Problem**: Actor-versus-actor collision is resolved by calling Bullet's
`contactPairTest` individually for every pair of overlapping actors. The code
itself notes this is "absolutely terrible" in a comment. At crowd-dense
locations (e.g. Balmora or Vivec city cantons) this creates O(n²) Bullet
queries per physics step.

**Fix** (completed):
Added a fast bounding-sphere pre-rejection check in `addSingleResult` before
calling the expensive (mutex-protected) `contactPairTest`.  If the two actors'
bounding spheres do not overlap, they cannot be in contact and the
`contactPairTest` call is skipped entirely.  This eliminates the O(n²) calls in
crowd scenes where actors are near each other in the broadphase sweep but are
not actually penetrating.

**Risk**: Low. The bounding-sphere check is a conservative pre-rejection: it
only skips `contactPairTest` when it is geometrically impossible for the actors
to be in contact.  Collision semantics are fully preserved for all cases where
penetration is possible.

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

### 6. Terrain Composite Map Resolution — WASM Cap ✅

**File**: `apps/openmw/engine.cpp`

**Problem**: `terrain/composite_map_resolution` controls the texture resolution
of baked terrain composites. The default (512) creates ~1 MB textures per
terrain chunk, putting unnecessary pressure on the WASM GPU memory budget.

**Fix** (completed):
1. In the WASM performance defaults block (`engine.cpp`), the composite map
   resolution is capped to 256 (256 KB per chunk vs 1 MB) if the user has not
   set a lower value.
2. Object paging (`terrain/object paging`) is disabled by default in WASM
   builds. Object paging places thousands of static-mesh imposters for
   non-active cells into the scene, significantly increasing draw-call count
   and VRAM usage; at the capped 4096-unit viewing distance the visual benefit
   is negligible. Both `object paging` and `object paging active grid` are
   disabled.
3. The frame-rate limit is capped at 60 fps in the WASM build.
   `requestAnimationFrame` already delivers vsync; the desktop default of 300
   was wasting CPU cycles and battery power in the browser.

**Risk**: Low. All three values are user-overridable in `settings.cfg`.

**Expected gain**: 10–30 % reduction in GPU memory for exterior terrain; large
reduction in draw calls (object paging disabled); lower CPU/battery use from
the framerate cap.

---

### 7. NPC Sound Pointer Caching ✅

**File**: `apps/openmw/mwrender/npcanimation.cpp`,
`apps/openmw/mwbase/soundmanager.hpp`,
`apps/openmw/mwsound/soundmanagerimp.hpp/cpp`

**Problem**: For each NPC the animation system looks up a `SoundPtr` every
frame to decide whether to play footstep or clothing-rustle sounds. This lookup
goes through the sound manager's map every animation update tick even when the
NPC is not moving.

**Fix** (completed):
Added `getSaySoundLoudnessIfActive()` to the `SoundManager` interface and
implementation.  This single call replaces the previous two separate calls
(`sayActive()` followed by `getSaySoundLoudness()`), reducing the per-frame
map lookups from three to at most two when the NPC is actively talking.
`HeadAnimationTime::update()` in `npcanimation.cpp` now uses this combined
method, removing the FIXME comment.

**Risk**: Very low. The combined query is semantically equivalent to calling the
two methods separately; negative return value is used as the "not saying"
sentinel.

**Expected gain**: Removes redundant map lookups proportional to the number of
NPCs in view; helps in NPC-dense interior cells.

---

### 8. Resource Cache Expiry — Memory-Pressure-Aware Eviction ✅

**File**: `components/resource/resourcemanager.hpp`,
`components/resource/resourcesystem.cpp`

**Problem**: The resource cache uses a fixed expiry time to decide when to drop
unused assets. Under memory pressure (e.g. on the WASM build with 512 MB
allocated, or on 32-bit platforms) the fixed expiry may be too long, leaving
stale meshes, textures, and sounds in memory longer than necessary.

**Fix** (completed — WASM only):
1. `ResourceSystem::updateCache()` now checks WASM heap usage via
   `__builtin_wasm_memory_size()` at the start of each cache-purge cycle.
2. When heap usage exceeds 75 % of `WASM_MAX_HEAP_BYTES` the expiry delays of
   all non-NIF resource managers are halved for that single purge cycle, then
   restored.
3. A `Debug::Verbose` log message is emitted once per pressure episode to aid
   diagnosis.

Desktop platforms are unaffected by this change.

**Risk**: Low. Overly aggressive eviction can cause assets to be reloaded more
often, introducing micro-stutters.  The 75 % threshold leaves a comfortable
headroom; the single-cycle halving is conservative.

**Expected gain**: Prevents out-of-memory crashes and reduces page-fault stalls
on WASM builds; most useful when the player moves rapidly between cells.

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

### 11. Water Rendering — Remove Legacy Refraction Scale Code ✅

**File**: `apps/openmw/mwrender/water.cpp`

**Problem**: Refraction scaling code that predates the current water shader
system is still compiled and executed every frame (referenced in Issue #5709).
This code has no visible effect but occupies shader uniform bandwidth and
developer attention.

**Fix** (completed):
1. Removed the explicit TODO shadow-disable branch
   (`if (mRefractionScale != 1) disableShadowsForStateSet(...)`) from
   `Refraction::setDefaults()`.  This was already marked for removal.
2. Simplified `Refraction::setWaterLevel()` to skip the redundant
   `osg::Matrix::scale * translate` computation when `refractionScale == 1.f`
   (the default), using `makeIdentity()` instead.  When a non-default scale is
   configured the existing matrix path is preserved for backward compatibility.

**Risk**: Very low. The shadow-disable branch only fired for non-default
`refractionScale` values; the default (`1.0`) is unchanged.  The matrix
identity fast-path is mathematically equivalent to the previous computation at
scale 1.

**Expected gain**: Eliminates one unnecessary matrix multiply and avoids the
shadow-manager call per water-level update; cleaner code path for the common
case.

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
3. ~~**NPC sound-pointer caching** (#7)~~ ✅
4. ~~**Water legacy code removal** (#11)~~ ✅
5. ~~**Resource cache memory-pressure eviction** (#8)~~ ✅
6. ~~**Actor-actor collision bounding-sphere pre-rejection** (#3)~~ ✅
7. ~~**RigGeometry GPU pre-compilation** (#2)~~ ✅
8. ~~**Terrain composite-map resolution / object paging / framerate cap** (#6)~~ ✅
9. **Terrain LOD consolidation** (#13) — pure refactor, enables future work.
10. **Shadow caster frustum tightening** (#1) — investigate polytope bounds.
11. **Particle system transform correctness** (#9) — world-space emitter fix.

---

## How to Contribute

1. Pick an item from the table above that is marked ☐.
2. Create a branch named `perf/<short-description>`.
3. Record a before/after benchmark using the profiling instructions above and
   include the results in your merge request description.
4. Tag the MR with the `performance` label and link this roadmap item.
