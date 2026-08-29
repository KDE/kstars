# Batch stacking / post-processing pipeline

This directory implements KStars' **headless batch image processing
pipeline**: calibrate → align (plate-solve) → stack → post-process a folder
of subs, entirely offscreen, driven over the EkosLive websocket protocol
(`postprocess_*` commands, dispatched from
`kstars/ekos/ekoslive/message.cpp`). It reuses the same stacking engine as
the interactive LiveStacker feature (`FITSStack`/`FITSData` in
`kstars/fitsviewer/`), but adds the pieces LiveStacker's live-capture-first
design didn't need: master-frame building from a folder of raw calibration
subs, folder inspection, narrowband multi-session blending, and headless
plate-solving with no GUI dialog involved.

If you're integrating against this pipeline (an EkosLive client, a test
harness, or another part of KStars), this file is the reference. If you're
modifying the pipeline itself, keep this file in sync — every default and
behavior below was read from the code, not assumed.

## Architecture

| File | Responsibility |
|---|---|
| `stackcontroller.h/.cpp` | Thin per-session wrapper around one `FITSData` instance. Owns the headless plate-solve path (`runExtract`/`runSolve`/`handleExtractDone`/`handleSolveDone`, using `SolverUtils` directly — no `PlateSolve` QDialog), and relays `FITSData`'s stacking signals (`stackReady`, `stackFailed`, `stackUpdateStats`, ...) up to whoever owns the session (`message.cpp`). `adopt()` lets a session be seeded from an externally-computed image (used by channel blending) instead of a real directory stack. |
| `masterbuilder.h/.cpp` | `MasterBuilder` — combines a folder of raw calibration subs (bias/dark/flat) into one master frame via sigma-clip. The "simplest case of stacking": no alignment, no debayer handling beyond what's already there. Supports `matchExptime` filtering for a shared calibration folder that mixes multiple exposure lengths, and bias/dark subtraction before combining (for building a proper master flat). |
| `directoryinspector.h/.cpp` | `DirectoryInspector` — header-only (no pixel decoding) survey of a folder: `EXPTIME`/`FILTER`/binning/`IMAGETYP` per file, plus a grouped summary. Exists so a caller can discover what's actually in a folder (and what to pass as `matchExptime`) without external tooling. |
| `autostretch.h/.cpp` | `AutoStretch` — an XISF-spec MTF (midtones transfer function) stretch, baked permanently into the image (unlike the display-only `Stretch` class elsewhere in `fitsviewer/`). Supports `linked` (one shared curve across all channels) vs unlinked (independent per-channel curves). |
| `cropoperation.h/.cpp` | `CropOperation` — crops the working image in place, adjusting the WCS reference pixel if one exists. |
| `curveoperation.h/.cpp` | `CurveOperation` — a control-point tone curve, applied identically to every channel (`apply()`) or independently per channel (`applyPerChannel()`, per-channel color grading). |
| `saturationoperation.h/.cpp` | `SaturationOperation` — HSV-style saturation scale. |
| `contrastoperation.h/.cpp` | `ContrastOperation` — contrast scale pivoted on the image's own mean. |
| `channelblendoperation.h/.cpp` | `ChannelBlendOperation` — the narrowband "pixel math": arbitrary weighted sums of any named, already-stacked mono session into each output R/G/B channel. This is what makes HOO/SHO/bicolor composites possible without forcing data through the engine's fixed positional RGB/RGBL slot assignment. |

The actual stacking/calibration engine (`FITSStack`, `FITSData`) lives one
level up, in `kstars/fitsviewer/`. This directory's classes are consumed by
`FITSStack::postProcessImage()` (gradient/denoise/deconvolution/sharpen —
these still live in `fitsstack.cpp` itself, run per-channel pre-combine) and
by `kstars/ekos/ekoslive/message.cpp`'s `processPostProcessCommands()` (the
actual command dispatcher).

## Session model

A **session** is one `StackController` (wrapping one `FITSData`), keyed by
`sessionId` in `Message`'s session map. Most commands take an optional
`sessionId` — when omitted, an internal default key is used, which is all
you need for the simple single-session case. Multiple sessions can be alive
concurrently (e.g. one per narrowband filter), each independently stackable,
post-processable, and saveable.

A session comes into being one of two ways:
- **`postprocess_start`** — a real directory stack (calibrate → align →
  combine).
- **`postprocess_blend_channels`** — an "adopted" image, computed from other
  sessions' already-stacked results rather than from raw subs. Behaves like
  a real stack from that point on (crop/stretch/curve/saturation/
  contrast/save all work identically), but has no `FITSStack` per-channel
  workers behind it and no WCS.

General response shape: every command replies via `new_postprocess_state`.
Errors are always `{"state": "error", "message": "<text>"}`. Field types
follow Qt's `QJsonValue::toX(default)` behavior — a field of the wrong JSON
type, or absent, silently becomes the stated default; there's no schema
validation or "missing required field" error except where noted below.

## Command reference

### `postprocess_start`

Two modes, chosen by whether the payload has a `channels` field.

**Mode A — `channels`** (filter-tagged, independent per-filter sessions —
narrowband, or anything where you don't want the engine's fixed positional
R/G/B/L assignment):

```json
{
  "channels": [
    { "filter": "Ha",   "directory": "/data/Ha/lights",   "masterDark": "/masters/dark_Ha.fits", "masterFlat": "/masters/flat_Ha.fits" },
    { "filter": "OIII", "directory": "/data/OIII/lights", "masterDark": "/masters/dark_OIII.fits", "masterFlat": "/masters/flat_OIII.fits" }
  ],
  "alignMethod": 0, "stackingMethod": 1
  // remaining stacking/post-processing fields below apply to every channel in this call
}
```

Each entry stacks as its own independent mono (`n==1`) session, tracked by
`filter` (which becomes that session's `sessionId` for every later
command). No fixed count — 2 filters, 5 filters, whatever you have.
`filter`/`directory` are required per entry; `masterDark`/`masterFlat` are
per-channel (not broadcast, unlike Mode B). Missing `filter`/`directory` on
any entry, or an empty `channels` array, starts nothing and returns an
error.

Response: immediately `{"state": "started", "sessions": ["Ha", "OIII"]}`,
then the same `progress`/`ready`/`cancelled`/error events Mode B produces
per channel, each tagged `"sessionId": "<filter>"`.

**Mode B — `directory`/`directories`** (single session):

| Field | Type | Default | Notes |
|---|---|---|---|
| `sessionId` | string | *(internal default key)* | Names this session for later commands. |
| `directory` | string | — | Mono/single-channel folder. Ignored if `directories` is present. |
| `directories` | array of string | — | Multi-channel: exactly 3 (RGB) or 4 (RGB+L) entries, in that **exact positional order** (`[0]`=RED, `[1]`=GREEN, `[2]`=BLUE, `[3]`=LUM — matches `FITSData::initStackChannels()`'s own convention). A 2-entry array is rejected with a clean error — use Mode A for a 2-filter set. |
| `calcSNR` | bool | `true` | |
| `alignMethod` | int | `0` | `0`=PLATE_SOLVE, `1`=NONE |
| `stackingMethod` | int | `0` | `0`=MEAN, `1`=SIGMA, `2`=WINDSOR, `3`=IMAGEMM. **`MEAN` performs no outlier rejection at all** — every pixel from every sub is averaged in at full weight, so a satellite trail or cosmic-ray hit in even one frame survives into the final stack. `SIGMA` (per-pixel median ± `lowSigma`/`highSigma`·σ) or `WINDSOR` (clamp instead of exclude — more robust at low sub counts) actually reject outliers. **Not defaulted to `SIGMA`/`WINDSOR`** — left as an explicit choice — but any real batch of more than a couple of subs should set one. |
| `downscale` | int | `0` | `0`=NONE, `1`=X2, `2`=X3, `3`=X4 |
| `numInMem` | int | `10` | Internal batch/chunk size the engine works through at a time. Has no effect on how many total frames get stacked — every file in the directory gets processed regardless of this value. |
| `weighting` | int | `0` | `0`=EQUAL, `1`=HFR, `2`=NUM_STARS — **soft down-weighting only**, not rejection. See `rejectTrailedSubs` for hard rejection. |
| `lowSigma` | double | `2.0` | |
| `highSigma` | double | `3.0` | |
| `rejectTrailedSubs` | bool | `true` | Hard-reject a sub with obvious star trailing (tracking failure) **before** calibration/alignment/combine — independent of `alignMethod`/`weighting`. Fast OpenCV ellipse-fit heuristic (`FITSData::detectStarTrailing()`), not real source extraction. |
| `maxStarElongation` | double | `0.08` | Rejection threshold for `rejectTrailedSubs` (0 = perfectly round, →1 = a full streak). Calibrated against real data — a well-tracked sub typically measures 0.002–0.05, real tracking-error trailing measures ~0.1+. Can vary by dataset/optics. |
| `hotPixels` | bool | `false` | Per-sub cosmetic correction: replaces a pixel that's a k-sigma outlier vs. its local 3×3 median with that median, **before** the sub reaches alignment/stacking. The only thing in the pipeline that can catch a genuinely stuck/hot sensor pixel — such a pixel is consistently bright in *every* sub (a physical defect, not a per-frame anomaly), so frame-to-frame outlier rejection (`SIGMA`/`WINDSOR`) can't touch it. |
| `coldPixels` | bool | `false` | Same mechanism, mirrored for pixels below the local median. Additionally isolation-masked to reduce the chance of eating real fine structure. |
| `masterDarkPath` | string | *(none)* | Single dark, broadcast to every channel in `directories`. Ignored if `masterDarkPaths` is present. |
| `masterDarkPaths` | array of string | *(none)* | Per-channel darks, positionally aligned with `directories`. |
| `masterFlatPath` | string | *(none)* | Single flat, broadcast to every channel. Ignored if `masterFlatPaths` is present. |
| `masterFlatPaths` | array of string | *(none)* | Per-channel flats, positionally aligned with `directories` — the normal case, since flats are filter-specific. |
| `postProcess` | bool | `false` | Gate for the 8 fields below (`gradientAmt` through `sharpenSigma`). Does **not** gate `hotPixels`/`coldPixels` above. |
| `gradientAmt` | double | `0.0` | Background/gradient removal strength, `[0,1]` — dual-pass TPS-fit background model with star-protected sampling. Runs **per-channel, pre-combine**. See "Gradient correction limitations" below. |
| `denoiseAmt` | double | `0.0` | Denoise strength, `[0,1]` — 3-level Gaussian-pyramid decomposition per channel, adaptive MAD-based threshold (`0.5–5σ` significance multiplier, not a fixed constant). Roughly the *idea* behind wavelet/multiscale denoising, not a literal wavelet transform. |
| `denoiseMethod` | int | `0` | `0`=HARD (binary keep/kill at threshold), `1`=SOFT (Donoho-style shrinkage — pulls coefficients toward zero *by* the threshold rather than passing them through unchanged; smoother, softens fine structure slightly more). |
| `chromaDenoiseAmt` | double | `0.0` | Separate, opt-in noise reduction targeting only **inter-channel color noise** (independent of `denoiseAmt`, which processes each channel independently and can't distinguish real color from single-channel noise). Decomposes into luma/color-difference planes, blurs only the color-difference planes, reconstructs from blurred chroma + untouched luma. `[0,1]` maps to a 1–8px chroma blur radius. |
| `deconvAmt` | double | `0.0` | Deconvolution strength |
| `PSFSigma` | double | `1.0` | PSF sigma used for deconvolution |
| `sharpenAmt` | double | `0.0` | Unsharp-mask amount |
| `sharpenKernal` | int | `3` | Unsharp-mask kernel size (forced odd, minimum 3) |
| `sharpenSigma` | double | `3.0` | Unsharp-mask Gaussian blur sigma |

Responses over the session's lifetime: immediately `{"state": "started"}`
(or a directory/validation error); per-sub during stacking
(`{"state": "progress", "ok": bool, "sub": int, "total": int, "meanSNR"/"minSNR"/"maxSNR": double}`);
on completion, `{"state": "ready"}` (or `{"state": "cancelled"}` if
`postprocess_stop` was called mid-stack, or `{"state": "error", "message": "..."}`
if the stack finished but produced nothing usable — e.g. every sub failed
calibration/alignment/plate-solving).

### `postprocess_stop`

`{"sessionId": ...}` (default key if omitted). Cancels the named session's
in-flight stack if one is running — safe no-op otherwise. Always responds
`{"state": "stopped"}`.

### `postprocess_close`

`{"sessionId": ...}`. Removes the named session from the session map. With
multiple concurrent sessions, each must be closed individually. Always
responds `{"state": "closed"}`, even if no session existed under that id.

### `postprocess_build_master`

Standalone — no active session needed. Combines a folder of raw calibration
subs into one master frame (`MasterBuilder::buildAndSave()`).

| Field | Type | Default | Notes |
|---|---|---|---|
| `directory` | string | — | **Required** with `outputPath`. |
| `type` | string | — | **Required** — exactly `"bias"`, `"dark"`, or `"flat"`. |
| `outputPath` | string | — | **Required**. |
| `lowSigma` | double | `3.0` | Sigma-clip rejection threshold. |
| `highSigma` | double | `3.0` | Sigma-clip rejection threshold. |
| `biasPath` | string | *(none)* | A pre-built master bias (or matching-exposure dark), subtracted from each raw sub before combining. Real use case: building a proper master **flat** — flats are usually taken at a much shorter exposure than lights, where dark current is negligible but the sensor's bias/offset pattern still isn't. |
| `matchExptime` | double | `-1.0` | When ≥0, only combine files whose `EXPTIME` header is within `exptimeTolerance` seconds of this value; everything else is skipped (header-checked only, never pixel-decoded). Negative disables filtering — every FITS-loadable file in the directory is combined. See "Shared calibration folders" below for why this matters. Also fixes the `NCOMBINE` header to reflect the actual post-filter count. |
| `exptimeTolerance` | double | `0.5` | Seconds of slack around `matchExptime` — real exposures rarely land exactly on a nominal value (auto-exposed flats, shutter-timing variance). |

Response: `{"state": "master_built", "outputPath": "<path>"}`, or
`{"state": "error", "message": "<reason>"}` (empty folder, dimension
mismatch between subs, `matchExptime` filtering leaving zero usable files,
etc.).

### `postprocess_inspect_directory`

Standalone, same as `postprocess_build_master`. Header-only survey of a
folder (`DirectoryInspector::inspect()`) — no pixel decoding, cheap even for
hundreds of files. Exists so a caller can discover a folder's actual
`EXPTIME`/`FILTER`/binning contents (and therefore what to pass as
`matchExptime`) without external tooling. Useful for **any** folder
(bias/dark/flat/light), not just darks — flats are often auto-exposed (the
real per-sub spread matters for picking `exptimeTolerance`), and any folder
can have a stray wrong-filter or wrong-exposure file mixed in by mistake.

| Field | Type | Notes |
|---|---|---|
| `directory` | string | **Required.** |

Response: `{"state": "inspected", "directory": "<dir>", "fileCount": N, "files": [...], "groups": [...]}`,
or `{"state": "error", "message": "<reason>"}` if the directory doesn't
exist or has no FITS-loadable files.

- `files[]` — one entry per file, in directory order:
  `{"filename", "exptime", "filter", "binning", "imagetyp"}`, plus an
  `"error"` field if that file's header couldn't be read at all (excluded
  from `groups` in that case, but not fatal to the rest of the call — one
  corrupt file doesn't hide what's in the rest of the folder). A missing
  individual key (e.g. no `FILTER` on a bias frame) just leaves that field
  empty/`-1`, not an error.
- `groups[]` — one entry per distinct `(exptime, filter, binning, imagetyp)`
  combination, sorted by `exptime` ascending:
  `{"exptime", "filter", "binning", "imagetyp", "count"}`. **This is what
  you actually scan** to decide what `matchExptime` to use for each master
  you need.

### `postprocess_blend_channels`

The narrowband "pixel math" — arbitrary weighted sums of any named,
already-stacked mono session into each output R/G/B channel
(`ChannelBlendOperation::blendRGB()`).

```json
// HOO bicolor: red=Ha, green=0.7*OIII + 0.3*Ha, blue=OIII
{
  "red":   [ { "filter": "Ha", "weight": 1.0 } ],
  "green": [ { "filter": "OIII", "weight": 0.7 }, { "filter": "Ha", "weight": 0.3 } ],
  "blue":  [ { "filter": "OIII", "weight": 1.0 } ],
  "outputSessionId": "final"
}
```

| Field | Type | Notes |
|---|---|---|
| `red` | array of `{"filter"` or `"sessionId": <id>, "weight": <number>}` | **Required**, at least one entry. `filter`/`sessionId` are interchangeable keys (same lookup). `weight` defaults to `1.0`. |
| `green` | same shape | **Required.** |
| `blue` | same shape | **Required.** |
| `outputSessionId` | string | Default `"blended"`. Where the result is stored as a new session. |

Weights are **not** renormalized — a literal weighted sum, not a weighted
average (two inputs at weight `1.0` each produce roughly double the
brightness of either alone). A single input at weight `1.0` is an exact
passthrough. Every named session must exist and already have a stacked
image, or the call fails with a specific message identifying which one.
Inputs feeding the same or different output channels must all be the same
pixel dimensions.

Response: `{"state": "blended", "outputSessionId": "<id>"}`, or
`{"state": "error", "message": "<reason>"}`. The output session behaves
exactly like a real stack from here on (crop/apply_*/save all work against
it normally), but carries no WCS.

**See "Narrowband/multi-channel palette choice" below** for why the plain
`green: [{"filter":"OIII"}]` mapping usually looks wrong, and the mixed
weighting above is the standard fix.

### `postprocess_redo_postprocess`

Recomputes gradient/denoise/deconvolution/sharpen from the
**already-combined** stack, without re-running calibration/plate-solve/
alignment/combine — the fast path for iterating on post-processing strength
once a session has already reached `"ready"`. Works against *any* session,
including one created by `postprocess_blend_channels`.

| Field | Type | Default |
|---|---|---|
| `sessionId` | string | *(internal default key)* |
| `postProcess` | bool | `true` (defaults **on** here, unlike `postprocess_start` — the point of calling this is to apply post-processing) |
| `gradientAmt` / `denoiseAmt` / `denoiseMethod` / `chromaDenoiseAmt` / `deconvAmt` / `PSFSigma` / `sharpenAmt` / `sharpenKernal` / `sharpenSigma` | — | same fields/defaults as `postprocess_start` |

**This overwrites whatever crop/autostretch/curve/saturation/contrast was
already applied** — it recomputes from the pre-post-processing combined
buffer and copies straight over the working image. Re-apply
`postprocess_crop`/`apply_*` after this if you want them back; that's the
correct order anyway (post-processing runs before tone/color adjustments,
not after).

Response: immediately `{"state": "redoing", "sessionId": "<id>"}` —
**asynchronous**, unlike `crop`/`apply_*`/`save`. Completion arrives later
via the same `{"state": "ready"}` / `{"state": "error", "message": "..."}`
event `postprocess_start` uses for this session.

### `postprocess_crop`

`{"x", "y", "width", "height"}` (all int, default `0`). Crops the working
image in place; adjusts the WCS reference pixel automatically if one
exists. Omitting `width`/`height` produces a degenerate 0×0 crop, rejected
as an out-of-bounds error. Response: `{"state": "cropped"}` or an error.

### `postprocess_apply_autostretch`

| Field | Type | Default | Notes |
|---|---|---|---|
| `targetBackground` | double | `0.25` | Where the background level lands post-stretch, `[0,1]`. |
| `shadowsClipping` | double | `2.8` | MADN (robust sigma) units below/above the median to clip at. |
| `linked` | bool | `true` | See "Linked vs. unlinked autostretch" below — **this choice matters a lot** and depends entirely on what kind of data you're stretching. |

Response: `{"state": "stretched"}` or an error.

### `postprocess_apply_curve`

`{"points": [{"x","y"}, ...]}` — at least 2 points, strictly increasing
`x`, each in `[0,1]×[0,1]`. One shared curve applied identically to every
channel. Response: `{"state": "curve_applied"}` or an error (non-monotonic
points, fewer than 2).

### `postprocess_apply_curve_per_channel`

`{"red": [...], "green": [...], "blue": [...]}` — same point rules as
above, independent curves per channel (color grading).
Fails against a mono/single-channel stack. Response:
`{"state": "curve_applied"}` or an error.

### `postprocess_apply_saturation`

`{"amt": double}`, default `1.0` (`1.0`=unchanged, `0.0`=grayscale,
`>1.0`=more saturated). No-op (still succeeds) on a mono image. Fails if
the current image isn't normalized to `[0,1]` yet (stretch/curve first).
Response: `{"state": "saturation_applied"}` or an error.

### `postprocess_apply_contrast`

`{"amt": double}`, default `1.0` (`1.0`=unchanged, `0.0`=flat at the pivot,
`>1.0`=more contrast). Pivots on the image's own mean; output clamped to
`[0,1]`. Response: `{"state": "contrast_applied"}` or an error.

### `postprocess_save`

`{"outputPath": string}` — required. Plain byte-for-byte write of the
already-FITS-encoded in-memory buffer, no re-encoding. Response:
`{"state": "saved", "outputPath": "<path>"}`, or
`{"state": "error", "message": "<reason>", "outputPath": "<path>"}` — note
`outputPath` is present either way.

## Key concepts and guidance

These are lessons from actually running this pipeline against real data
(multiple full OSC and narrowband datasets), not just reading the code —
each one caused a visibly wrong result before it was understood.

### `MEAN` vs `SIGMA`/`WINDSOR` — outlier rejection

`stackingMethod` defaults to `MEAN`, which performs **no** outlier
rejection — a satellite trail, plane trail, or cosmic-ray hit in even one
sub survives straight into the final stack at full weight. Any real batch
with more than a couple of subs should explicitly set `SIGMA` (per-pixel
median ± σ, reject-and-average the rest) or `WINDSOR` (clamp instead of
exclude — more robust at low sub counts, since a hard-rejected outlier
can't inflate its own judging σ out of range). This is deliberately **not**
defaulted for you — confirmed on a real 4-sub stack where a satellite trail
was clearly visible under `MEAN` and removed (down to a faint residual)
under `SIGMA`.

### Hot/cold pixel correction vs. stacking rejection

`hotPixels`/`coldPixels` and `SIGMA`/`WINDSOR` stacking solve *different*
problems and neither substitutes for the other. `SIGMA`/`WINDSOR` reject a
pixel that's an outlier **relative to the other subs at that position** —
a satellite trail, a cosmic-ray hit, anything that varies frame-to-frame. A
genuinely stuck/hot sensor pixel is consistently bright in **every single
sub** (it's a physical defect, not a per-frame anomaly), so from the
stacker's point of view its consistently-high value looks like "the
truth" — sigma-clipping never rejects it. `hotPixels`/`coldPixels` run
per-sub, before alignment/stacking, comparing each pixel to its own local
3×3 neighborhood — the only mechanism in the pipeline that can actually
catch this. Confirmed on real data: isolated, fully-saturated
single-pixel-per-channel squares (worst in the green channel) survived
`SIGMA` stacking, `rejectTrailedSubs`, both denoise passes, and the linked
autostretch fix below — none of those operate early enough, or look at the
right thing, to catch a per-sub-consistent defect.

### `denoiseAmt` vs. `chromaDenoiseAmt`

`denoiseAmt` processes each channel **independently** — it can suppress
real per-channel noise, but has no way to tell "these channels disagree
because of genuine color" from "these channels disagree because of
uncorrelated per-channel shot/read noise" (an OSC sensor's R/G/B are
physically separate photosites with independent noise). `chromaDenoiseAmt`
targets exactly that: it separates luma from color-difference, smooths
only the color-difference planes, and leaves luminance detail untouched.
Confirmed on real OSC data: a stack still showed clear fine-grained
red/green/blue speckle in the background at a tight zoom even after fixing
the autostretch clipping bug below and applying `denoiseAmt`/SOFT — that
fix only stopped the noise from being independently clipped to full
saturation per channel, it didn't touch the underlying uncorrelated noise
itself. Use both together for real OSC data with visible color grain.

### Linked vs. unlinked autostretch — **pick based on what the channels represent**

This is the single most impactful parameter for how a multi-channel image
looks, and the right choice is the *opposite* for OSC vs. narrowband data:

- **OSC (Bayer/one-shot-color) data: use `linked: true`.** R/G/B come from
  the same physical scene through the same optical path, with only their
  Bayer-pattern demosaicing differing — they *should* share a background
  statistic. With `linked: false` (independent per-channel curves),
  uncorrelated per-channel sensor noise gets stretched to a *different*
  black/white point per channel, turning faint neutral background noise
  into fully-saturated red/green/blue speckle. Confirmed: a real stack's
  background pixel minimum was exactly `0.0` (evidence of per-channel
  clipping) under `linked: false`; under `linked: true`, the minimum was
  `0.0106` and the speckle was visibly gone.
- **Narrowband/multi-filter composites (HOO, SHO, LRGB, ...): use
  `linked: false`.** Each channel is an **independent filter** capturing a
  different signal at a different absolute brightness — there's no reason
  for them to share a background level, and forcing them to actively hurts
  the result. Confirmed on a real Hα/OIII bicolor composite: OIII's raw
  background was genuinely ~30% brighter than Hα's; `linked: true` pools
  both channels' statistics into one shared black-point, which — since
  OIII occupied 2 of the 3 output channels (G and B) — skewed the shared
  threshold toward OIII's level, crushing Hα (R) more than its own
  background warranted. The visible symptom was a flat, wrong-looking cyan
  cast over the entire image (should have been a neutral dark background
  with the nebula's real color standing out against it). Switching to
  `linked: false` immediately made the underlying structure and correct
  relative coloring visible.

If a composite still looks tinted after switching, check the individual
channels' raw background levels directly (percentile sampling per channel)
before assuming it's a stretch-parameter problem — see "Shared calibration
folders" and "Gradient correction limitations" below for two real causes of
a *content* difference that no amount of stretch tuning fixes.

### Narrowband/multi-channel palette choice

`postprocess_blend_channels`'s weights are literal pixel math — nothing
stops you from writing `green: [{"filter":"OIII","weight":1.0}]` for a
plain HOO mapping (R=Hα, G=OIII, B=OIII), but this has a real, visible
downside: any star with real Hα signal and negligible OIII signal (common
— narrowband filters sample a star's continuum at two specific
wavelengths, and a cooler/redder star's continuum is naturally much
brighter near Hα than near OIII) gets **zero** contribution to G, so it
clips to a fully-saturated pure red rather than a natural warm white/orange
tone. The standard fix, confirmed to work on real data: mix a fraction of
Hα into green, e.g. `green: [{"filter":"OIII","weight":0.7},{"filter":"Ha","weight":0.3}]`
— this alone turned a field of garish pure-red stars into natural-looking
warm orange/white ones, since every star now gets *some* contribution to
every channel regardless of which filter it's brightest in.

### Shared calibration folders (`matchExptime`)

`MasterBuilder` has no built-in awareness of exposure time, frame type, or
filter — it combines every FITS-loadable file in whatever directory you
point it at, unconditionally. A real shared calibration library (multiple
sessions' darks dumped in one folder, say) can mix several exposure lengths
under one **identical** `IMAGETYP='Dark Frame'` header with no other way to
tell them apart — pointing `postprocess_build_master` at that folder
without filtering silently sigma-clip-averages everything together into a
master matching *nothing*. Confirmed on a real dataset: a `Darks/` folder
held 202 files across 7 different exposures, all identically labeled, where
only 3 of those 7 groups were actually usable for the session at hand.
**Always run `postprocess_inspect_directory` on a calibration folder you
didn't build yourself before trusting it**, and use `matchExptime`/
`exptimeTolerance` to isolate the group you actually need for each master.
Also watch for a genuinely missing flat-dark (no exposure in the shared
library matches the flat's own exposure) — there may be no clean fix beyond
accepting the closest available exposure (defensible for very short,
sub-second exposures where dark current is negligible) or building the
master flat without dark/bias correction at all.

### Gradient correction limitations

`gradientAmt` (`FITSStack::gradientCorrection()`) runs **per-channel,
pre-combine**, and is a real two-pass TPS-fit background model with
star-protected sampling — not a stub. But on a real, difficult dataset
(dense star field, non-trivial background gradient, e.g. from an
imperfectly flat-corrected channel) it can under-correct at a modest
strength and produce a visible seam artifact at a strong one, rather than
scaling smoothly in between. If a composite shows a smooth but clearly
wrong background gradient (a color wash across the frame that doesn't
match what the target should look like) even after correct `linked`/
palette choices:

1. **Measure each channel's actual background independently** — sample a
   grid of low-percentile (star-robust) tiles across each channel and
   compare. A channel whose flat lacked proper dark/bias correction can
   show a real, substantial gradient (confirmed: ~23% background variation
   across one channel, vs. ~4% in a properly-calibrated sibling channel)
   that a shared or per-channel `gradientAmt` pass may not fully remove.
2. If `gradientAmt` alone doesn't resolve it, consider fitting a smooth 2D
   model (even a low-order polynomial) to that channel's measured
   background grid directly and subtracting it out before blending — a
   manual workaround, not a built-in command, but a reliable one when the
   built-in per-channel correction is insufficient for a specific dataset.

There is currently no **post-combine** background-extraction step exposed
or built — only the pre-combine, per-channel `gradientAmt`.

### Split-exposure light sequences (mixing exposure lengths within one filter)

A real session can capture the same filter at more than one exposure
length (e.g. switching from 600s to 900s sub-exposures partway through).
`postprocess_start` applies one `masterDarkPath` uniformly to an entire
directory — there's no per-sub exposure matching within a single
`postprocess_start` call. The correct approach, confirmed on real data:

1. Use `postprocess_inspect_directory` on the lights folder first — don't
   assume uniform exposure from a filename or the first file. (A real
   dataset's "600s Hα" folder turned out to actually be 53 subs at 600s,
   49 at 900s, and 1 completely unrelated contaminant frame at a different
   exposure *and* binning.)
2. Split the lights into separate per-exposure folders (excluding any
   contaminant frames found in step 1).
3. Run `postprocess_start` once per exposure group, each with its own
   correctly `matchExptime`-filtered dark, `postProcess: false` (defer
   post-processing to the final merge).
4. Save each result (`postprocess_save`), then feed both saved stacks back
   in as a two-file "directory" to one more `postprocess_start` call with
   `alignMethod: 1` (NONE — both inputs are already independently
   plate-solved to the real sky and should already share a consistent
   pixel grid; verify this visually — no doubled/ghosted stars — before
   trusting it) and `stackingMethod: 0` (MEAN — a plain average of two
   already-calibrated, already-aligned deep stacks). No `masterDarkPath`/
   `masterFlatPath` needed at this step, since both inputs are already
   calibrated.

Note: attempting the same merge with `alignMethod: 0` (PLATE_SOLVE) against
already-stacked, already-processed images was found to silently fail — see
"Known gaps" below.

## Worked example 1 — single-color (OSC) full pipeline

A typical one-shot-color (OSC/Bayer) target: bias, darks, flats, and lights
all in their own folders, one filter (no filter wheel), single exposure
length throughout.

```jsonc
// 1. Build calibration masters (each is its own independent call, no session needed)
{"type": "postprocess_build_master", "payload": {
  "directory": "/data/bias", "type": "bias", "outputPath": "/masters/bias.fits"
}}
{"type": "postprocess_build_master", "payload": {
  "directory": "/data/darks", "type": "dark", "outputPath": "/masters/dark.fits"
}}
{"type": "postprocess_build_master", "payload": {
  "directory": "/data/flats", "type": "flat", "outputPath": "/masters/flat.fits",
  "biasPath": "/masters/dark.fits"  // or a matching-exposure dark/bias — see MasterBuilder's biasPath note
}}

// 2. Stack the lights: plate-solve alignment, SIGMA rejection, hot/cold pixel
//    correction, denoise + chroma-denoise, no gradient/sharpen/deconv yet
{"type": "postprocess_start", "payload": {
  "directory": "/data/lights",
  "masterDarkPath": "/masters/dark.fits",
  "masterFlatPath": "/masters/flat.fits",
  "alignMethod": 0, "stackingMethod": 1,
  "rejectTrailedSubs": true, "hotPixels": true, "coldPixels": true,
  "postProcess": true,
  "denoiseAmt": 0.4, "denoiseMethod": 1, "chromaDenoiseAmt": 0.4
}}
// ... wait for {"state": "ready"} (or "error" — check it, don't assume success) ...

// 3. Linked autostretch — OSC data, so linked:true is correct here
{"type": "postprocess_apply_autostretch", "payload": {
  "targetBackground": 0.25, "shadowsClipping": 2.8, "linked": true
}}

// 4. Optional: saturation/contrast, then save
{"type": "postprocess_apply_saturation", "payload": {"amt": 1.2}}
{"type": "postprocess_apply_contrast", "payload": {"amt": 1.1}}
{"type": "postprocess_save", "payload": {"outputPath": "/output/final.fits"}}
```

If the result still shows red/green/blue background speckle after this,
raise `chromaDenoiseAmt`; if isolated saturated single-pixel squares
survive, that's `hotPixels`/`coldPixels` not being enabled (or the defect
being borderline enough to need a stronger threshold — not currently
tunable per-call, see the class comment in `fitsstack.cpp` if it needs
adjusting).

## Worked example 2 — multi-channel mono (narrowband HOO) full pipeline

Two filters (Hα, OIII), a shared calibration library with mixed exposure
lengths, and — as is common — one filter's lights spread across two
different sub-exposure lengths.

```jsonc
// 1. Inspect everything first — never assume a shared calibration folder or
//    a lights folder is uniform.
{"type": "postprocess_inspect_directory", "payload": {"directory": "/data/Darks"}}
{"type": "postprocess_inspect_directory", "payload": {"directory": "/data/Lights/Ha"}}
{"type": "postprocess_inspect_directory", "payload": {"directory": "/data/Lights/OIII"}}
{"type": "postprocess_inspect_directory", "payload": {"directory": "/data/Flats/Ha"}}
{"type": "postprocess_inspect_directory", "payload": {"directory": "/data/Flats/OIII"}}
// Read the "groups" in each response. Don't proceed until you know exactly
// which exposure groups exist and which ones you actually need.

// 2. Build masters, one matchExptime-filtered call per group actually needed
//    (example numbers — use what step 1 actually found)
{"type": "postprocess_build_master", "payload": {
  "directory": "/data/Darks", "type": "dark", "outputPath": "/masters/dark_600.fits",
  "matchExptime": 600.0
}}
{"type": "postprocess_build_master", "payload": {
  "directory": "/data/Darks", "type": "dark", "outputPath": "/masters/dark_900.fits",
  "matchExptime": 900.0
}}
{"type": "postprocess_build_master", "payload": {
  "directory": "/data/Darks", "type": "dark", "outputPath": "/masters/darkflat_Ha.fits",
  "matchExptime": 0.75, "exptimeTolerance": 0.05
}}
{"type": "postprocess_build_master", "payload": {
  "directory": "/data/Flats/Ha", "type": "flat", "outputPath": "/masters/flat_Ha.fits",
  "biasPath": "/masters/darkflat_Ha.fits"
}}
{"type": "postprocess_build_master", "payload": {
  "directory": "/data/Flats/OIII", "type": "flat", "outputPath": "/masters/flat_OIII.fits"
  // no biasPath: no matching flat-dark exists in this example — accepted
  // for a short, sub-second flat exposure where dark current is negligible
}}

// 3. If one filter's lights split across exposure lengths (found in step 1),
//    pre-sort into per-exposure folders yourself, excluding any contaminant
//    frames, then stack each separately (postProcess deferred to the end):
{"type": "postprocess_start", "payload": {
  "sessionId": "ha_600", "directory": "/data/Lights_Ha_600",
  "masterDarkPath": "/masters/dark_600.fits", "masterFlatPath": "/masters/flat_Ha.fits",
  "alignMethod": 0, "stackingMethod": 1,
  "rejectTrailedSubs": true, "hotPixels": true, "coldPixels": true, "postProcess": false
}}
{"type": "postprocess_start", "payload": {
  "sessionId": "ha_900", "directory": "/data/Lights_Ha_900",
  "masterDarkPath": "/masters/dark_900.fits", "masterFlatPath": "/masters/flat_Ha.fits",
  "alignMethod": 0, "stackingMethod": 1,
  "rejectTrailedSubs": true, "hotPixels": true, "coldPixels": true, "postProcess": false
}}
// ...wait for both "ready", then postprocess_save each to its own FITS file,
// copy both saved files into one folder, and merge:
{"type": "postprocess_start", "payload": {
  "sessionId": "ha_final", "directory": "/data/ha_merge_folder",
  "alignMethod": 1, "stackingMethod": 0, "rejectTrailedSubs": false, "postProcess": false
}}
// (verify no star doubling before trusting this — see "Split-exposure
// light sequences" above)

// 4. OIII, single exposure, straightforward
{"type": "postprocess_start", "payload": {
  "sessionId": "oiii_final", "directory": "/data/Lights/OIII",
  "masterDarkPath": "/masters/dark_900.fits", "masterFlatPath": "/masters/flat_OIII.fits",
  "alignMethod": 0, "stackingMethod": 1,
  "rejectTrailedSubs": true, "hotPixels": true, "coldPixels": true, "postProcess": false
}}

// 5. Blend — mixed-green HOO palette (see "Narrowband/multi-channel palette
//    choice" above for why plain green:OIII usually looks wrong)
{"type": "postprocess_blend_channels", "payload": {
  "red":   [{"sessionId": "ha_final", "weight": 1.0}],
  "green": [{"sessionId": "oiii_final", "weight": 0.7}, {"sessionId": "ha_final", "weight": 0.3}],
  "blue":  [{"sessionId": "oiii_final", "weight": 1.0}],
  "outputSessionId": "hoo_final"
}}

// 6. Crop away any edge artifacts from partial-coverage stacking regions
//    (check each channel's own edges for a coverage falloff before assuming
//    none exists), then post-process and stretch UNLINKED (see "Linked vs.
//    unlinked autostretch" above — this is narrowband, not OSC)
{"type": "postprocess_crop", "payload": {"sessionId": "hoo_final", "x": 500, "y": 130, "width": 5600, "height": 3900}}
{"type": "postprocess_redo_postprocess", "payload": {
  "sessionId": "hoo_final", "postProcess": true,
  "gradientAmt": 0.3, "denoiseAmt": 0.4, "denoiseMethod": 1, "chromaDenoiseAmt": 0.4
}}
{"type": "postprocess_apply_autostretch", "payload": {
  "sessionId": "hoo_final", "targetBackground": 0.15, "shadowsClipping": 2.8, "linked": false
}}
{"type": "postprocess_save", "payload": {"sessionId": "hoo_final", "outputPath": "/output/hoo_final.fits"}}
```

If the background still shows a wrong color gradient after this, see
"Gradient correction limitations" above — measure each channel's actual
background directly rather than continuing to guess `gradientAmt` values.

## Known gaps (as of this writing)

- ~~No rejection for a large alignment translation~~ — **fixed.**
  `calcWarpMatrix()` checked the RANSAC inlier ratio and the linear part's
  determinant/distortion, but neither catches a large *pure translation*
  (e.g. a wrong-target sub, or a meridian flip without re-centering) — a
  translation shifts every correspondence point by the same amount, so it
  stays perfectly consistent under RANSAC, and the determinant check only
  looks at the rotation/scale block, not the translation column. Note a
  meridian-flip *rotation* (even a full 180°) was never affected by this gap
  — `estimateAffinePartial2D` fits an arbitrary rotation angle, and a pure
  rotation's determinant stays ~1.0 regardless of angle, so that case was
  already handled correctly by the existing checks. Fixed by rejecting a
  translation magnitude past half the frame's smaller dimension (the two
  subs would share too little real overlap to be worth combining beyond
  that point).
- **Re-solving an already-stacked/processed image with `alignMethod: 0`
  (PLATE_SOLVE) can silently produce nothing.** Confirmed on real data: a
  2-image merge (two partial stacks of the same target) with
  `alignMethod: 0` left both subs stuck at a non-`OK` status and got
  dropped from the combine entirely, still reporting completion (now
  reported as an error rather than false success, since the `stackFailed`
  fix, but the underlying merge still doesn't work this way — use
  `alignMethod: 1`/NONE for this specific "merge two already-aligned
  stacks" case instead, after visually confirming they share a consistent
  pixel grid).
- **`FITSData::detectStarTrailing()` can degenerate to `elongation == 0`
  exactly** on some datasets (confirmed: every sub in one real 51-frame
  dataset measured exactly `0.0` despite 700+ detected sources per frame),
  most likely from `cv::fitEllipse()` degenerating on small/undersampled
  star contours at that dataset's particular plate scale. When this
  happens, `rejectTrailedSubs` is silently inert for that dataset — it
  won't false-positive-reject good subs, but it also won't catch a
  genuinely trailed one. Worth a sanity check (do a few subs measure
  *exactly* zero with high source counts?) before trusting the rejection
  ran meaningfully.
- **No post-combine gradient/background-extraction step.** `gradientAmt`
  runs pre-combine, per-channel, only.
- **No photometric color calibration.**
- No progress/percentage reporting for `crop`/`apply_*`/`save` — these are
  synchronous from the dispatcher's point of view, unlike `postprocess_start`'s
  `progress`/`ready` events or `postprocess_redo_postprocess`'s async
  completion.
