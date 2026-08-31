# DEC/altitude predictability effect: theory log

Tracks every explanation tried for the validated finding below, what tested it, and the verdict.
Rule going forward: no theory gets marked RULED OUT or VALIDATED without a script run against real
data and a pre-registered statistical test — eyeballing a pattern doesn't count (see the pier-side
entry for the concrete mistake that made this rule necessary).

## The core validated finding

DEC residual predictability (causal-EMA R² over the physics-residual rate) rises with altitude.
Mount: EQMod WORM_GEAR. 29 nights, pre-2026-08-28 (before the online DEC filter existed, so the
residual is clean physics-only, untouched by any online correction).

- Script: `pool_ai_debug_dec.py`
- Gate: `validate_altitude_effect()` — OLS trend + Mann-Whitney extreme-bin test, ≥6 independent
  nights required in both extreme bins.
- Result: **VALIDATED** — p_trend=0.0012, p_extreme=0.0043, effect +0.313 R²
  (alt [26.4,35.3) n=6 nights vs alt [62.1,71.0] n=6 nights).
- Status: this is the one solid result of the whole investigation. Everything below is an attempt to
  explain *why* it happens, and separately, whether *acting* on it live helps.

## Theories tried (offline mechanism hunt)

| # | Theory | Script / test | Result | Verdict |
|---|--------|----------------|--------|---------|
| 1 | SNR/seeing: better centroid SNR at high altitude shrinks frame-to-frame noise | `noise_floor_by_altitude.py` — `second_diff()` roughness proxy by altitude bin | Noise floor flat across altitude, no shrinkage | **RULED OUT** — real astronomical airmass/SNR physics isn't disputed, it just doesn't show up as measurable centroid noise over the altitude range actually sampled at this site |
| 2 | Time-in-session confound (not altitude itself, just "later in the night") | `noise_floor_by_altitude.py --split-by-direction` — `classify_night_direction()` | Effect direction identical whether the mount was rising or setting through a given altitude | **RULED OUT** |
| 3 | RA-only artifact (DEC pattern is really some RA-specific pipeline quirk) | `pool_ai_debug_ra.py` — per-segment fundamental-phase fit | RA shows the same altitude-rising shape as DEC, smaller magnitude | **NOT the explanation, but reinforces the effect is real** — shared across both axes, not a DEC-only fluke |
| 4 | Pier-side / hour-angle (mount behaves differently pre/post meridian; altitude is a proxy for HA, not the real driver) | `pool_ai_debug_dec_hourangle.py` — `hour_angle_deg()`, `loocv_predicate_binned()` | altitude/\|HA\| collinear (r=-0.85, can't fully separate). Once altitude-controlled, most of the raw pier-side split vanished except one +0.295 R² gap at near-zenith on a 6-vs-4-night sub-slice | **DOWNGRADED TO NOISE** — a small-sample post-hoc slice; this is the mistake that motivated building `validate_altitude_effect()` as a mandatory pre-registered gate instead of eyeballing sub-slices |
| 5 | Atmospheric refraction (either true Bennett-formula refraction, or the existing empirical `k_ref_dec*sin(q)/cos²(alt)` term being mis-scaled/under-extrapolated outside its fitted 37.9°-69.5° range) | Numeric Bennett-refraction simulator vs. fitted `k_ref_dec`, cross-checked against `fit_alt_min`/`fit_alt_max` and the actual per-bin R² pattern | True refraction magnitude ~30x smaller than the fitted term and opposite sign; `sin(q)/cos²(alt)` fits the true refraction curve badly (R²=0.006); and the bin fully OUTSIDE the fit window (26-35°) shows the LEAST unexplained structure (R²=-0.014) while the bin near the TOP of the fit window (62-71°) shows the MOST (R²=0.299) — backwards from what under-extrapolated refraction predicts | **RULED OUT** — wrong magnitude, wrong functional shape, wrong direction |
| 6 | Mechanical loading / backlash-stiction (gravity-vector torque on the DEC axis varies with pointing altitude, changing backlash/stiction severity) | `pool_backlash_by_altitude.py` (new) — detects DEC correction-direction reversals via `dec_error_arcsec` sign changes, compares near-reversal vs baseline residual magnitude, tests whether that excess itself trends with altitude | (a) Backlash/stiction is real and large: reversal-adjacent RMS residual 1.44x baseline, p=7.5e-213, pooled. (b) The excess trends down with altitude (0.87→0.68→0.43→0.32 log-ratio, bins 2-5) — but the low bin has only 5 independent nights against the required 6; adding 2026-08-30/31 didn't fix it (bin edges shifted from the wider altitude range, dropping it to 4, and that data is contaminated by the online filter actively toggling by altitude anyway) | **OPEN — closest lead, not validated.** One more clean (filter-OFF, intervention-free), low-altitude night of data would settle it either way |

## Separate track: does *acting* on the finding help online?

These test whether a live correction mechanism built on the offline finding actually improves
guiding — a different, harder question than "is the offline predictability effect real."

| Attempt | Test | Result | Verdict |
|---|---|---|---|
| v1 (aggressive `dec_alt_trust_table`, 8.55x distrust at low alt) | Live A/B, 3 clean matched-altitude pairs | Every clean pair: ON worse than OFF (+9.2%, +11.2%, +29.7%). One-tailed p=0.062 | **Suggestive it HURT, not proven** (p>0.05) — not deployed as-is |
| v2 (retuned 75% back toward neutral) | Live A/B, full night | Pooled deltas essentially a wash at every altitude (-1.2% to -4.7%), no clean pair reached significance | **No significant effect either direction** |
| Adaptive-R (innovation-covariance-matching Kalman R, replaces the static trust table) | Live A/B, 5 pairs one night | No pair/altitude reached significance (all p>0.5, n=1-3 pairs) | **No significant effect either direction** |
| Pooled OFF-baseline RMS by altitude, both nights combined (15 legs) | Kruskal-Wallis + Pearson/Spearman correlation | H=4.63 p=0.099; r=-0.36 p=0.19 (Spearman p=0.13) — same direction as the offline finding (tighter RMS at higher altitude) | **Suggestive, not significant** |

**Net state:** the offline predictability effect is real and validated. No online mechanism tried so
far has been *proven* to exploit it — v1 suggestively hurts, v2 and adaptive-R are statistically
neutral. The mechanism (why it happens) remains unidentified; backlash/stiction is the only theory
still alive, one clean night away from a verdict.

## Mitigation designs sketched (and killed) before touching live code

| Design | Offline check | Result | Verdict |
|---|---|---|---|
| Reversal direction asymmetry (different gain per direction to avoid the "expensive" reversal direction) | `pool_backlash_by_altitude.py` direction split: near-reversal RMS for dec_error crossing to + vs to − | 0.1963px vs 0.2064px, ratio 0.951x, Mann-Whitney p=0.77 | **RULED OUT** — backlash excess is symmetric between directions (consistent with classical gear-mesh slack, not gravity-loaded stiction); there is no "cheap" direction to bias toward |
| Physics-informed reversal guard: delay a direction reversal unless it agrees with the sign `physicsDECBase()` predicts, using disagreement as a noise-vs-genuine discriminator (`dec_reversal_guard_replay.py`) | Classified all 2864 historical reversals (pre-08-28 clean data) as physics-agreeing or -disagreeing; compared peak-magnitude and duration-before-flip distributions between the two groups | Split is 49.4%/50.6% — a coin flip, flat across every altitude bin (47.7-53.0%). Peak/duration distributions are statistically indistinguishable between the two groups (median peak 1.78" vs 1.91", median duration 10.0s vs 9.9s). At the threshold needed to catch the median "disagreeing" reversal, 55% of genuine agreeing reversals get caught too | **RULED OUT, before writing any live code** — physicsDECBase() is already fed forward into the correction, so the residual error a reversal-detector watches is by construction close to independent of that term's sign; checking agreement with a signal already compensated for carries no information |

**Revised after further offline testing (`dec_reversal_dwell_analysis.py`)**: the "no ground truth"
objection above was too quick. A directly answerable, hindsight-only question turned out to be
decisive: of 2803 reversal-to-next-reversal pairs, 39.2% are "churn" (the star reverses back again
within 8s -- roughly the measured backlash-disturbance window -- suggesting the first reversal may
have been noise, not real drift). Checked what a real-time hold-timer could actually have observed
*before* committing to each reversal:

| Candidate real-time discriminator | Result |
|---|---|
| Peak magnitude before the flip | Weak and WRONG direction (churn peaks slightly *larger*: 1.97" vs 1.78" median, p=0.0002) |
| Physics-predicted sign agreement | No power at all (49.4%/50.6%, a coin flip — see above) |
| **Duration the opposing signal persisted before the flip** | **Real signal: churn median 5.7s vs non-churn median 14.0s, p=3.6e-22** |

Duration-gated hold-time operating points (T_hold = delay any reversal until the opposing signal has
persisted this long):

| T_hold | Churn caught | Genuine reversals also delayed |
|---|---|---|
| 3s | 41.8% | 24.3% |
| 8s | 57.4% | 39.1% |
| 20s | 76.8% | 59.2% |

**Verdict: OPEN, upgraded from "unvalidated fallback" to "validated real-time-observable signal, net
benefit unresolved."** The catch:false-delay ratio favors the debounce at every threshold tested
(1.3-1.7x), unlike the two discriminators above which showed no edge at all. What remains
unanswerable offline: whether that favorable *count* ratio nets out to better *guiding RMS*, since that
depends on the relative cost of a delayed genuine correction vs. an avoided backlash re-engagement —
a trade this data cannot price, only a live A/B can (recommended starting point: T_hold≈8s, matched-
altitude pairs, same rigor as every other live test in this log).

## Known predictable patterns not currently corrected for (as of 2026-08-31)

A full accounting, so none of these get silently lost between sessions. "Not corrected for" is a
factual statement about the current code/config, not a priority ranking by itself — see the note on
each row for why it matters.

| Pattern | Validated? | Currently corrected for? |
|---|---|---|
| **Mount-level DEC backlash on reversal** | Yes — 1.44x RMS excess on reversal, p=7.5e-213, symmetric both directions | **No.** EQMod's own native backlash compensation (`skywatcher.cpp`, applies a dedicated compensation move on axis reversal) is present in the driver and even has a stored value — `~/.indi/EQMod Mount_config.xml`: `BACKLASHDE=100` — but `USEBACKLASHDE=Off`. This is the single most concrete, lowest-effort, purpose-built fix available and it has never been tested. Provenance/calibration quality of the `100` value is unknown — don't just flip it on without a controlled A/B. |
| **Reversal "churn" (short-dwell noise reversals)** | Yes — duration-before-flip discriminates churn from genuine reversals, p=3.6e-22 (see above) | No — no debounce/hold-timer exists anywhere, in AI Guider or the driver. Independent of the backlash-compensation fix above (that makes each reversal cheaper; this would reduce how many reversals happen at all). |
| **DEC residual altitude-predictability (the core validated finding of this whole log)** | Yes — p=0.0012 trend, p=0.0043 extreme-bin, 29 nights | **No, currently.** Checked live `ai_guider_weights.json` directly: no `dec_alt_trust_table` is loaded. `kf_r_dec_adaptive_enabled: true` (adaptive-R) is what's actually active instead, but adaptive-R doesn't reference altitude at all, and showed no significant live benefit (all p>0.5, n=1-3 pairs). The one statistically solid finding of this investigation isn't being exploited by anything currently running. |
| **RA altitude-dependent shape** (cross-axis confirmation of the DEC finding) | Suggestive — same rising-with-altitude shape as DEC, smaller magnitude | No — never built a correction mechanism for RA at all, only used it as a confirmatory check |
| **Physics feedforward outside its fitted altitude range** (37.9°-69.5°) | Known by design, not a new finding | Partially — deliberately clamped rather than extrapolated outside that window (extrapolating `1/cos²(alt)` blows up near the horizon). Correct engineering caution, but means whatever the true relationship is beyond that range goes uncorrected by choice, not because it's unknowable. |

If picking this back up later, the backlash-compensation row is the recommended starting point: it's
a driver-level config change, not new code, and directly targets the largest, most statistically
overwhelming effect measured in this entire log.

## Process lesson baked into every entry above

Theory #4 (pier-side) was accepted informally for a few hours on a striking-looking 6-vs-4-night
sub-slice before being caught and downgraded. That's the direct reason `validate_altitude_effect()`
(and its sibling `validate_metric_altitude_effect()` in the backlash script) exist as mandatory,
pre-registered, mount-agnostic gates — every entry in this log that says RULED OUT or VALIDATED went
through one of those, not a visual read of a table.
