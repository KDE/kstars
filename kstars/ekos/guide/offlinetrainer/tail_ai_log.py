#!/usr/bin/env python3
"""
tail_ai_log.py — Live tail/plot of the AI Guider debug CSV during a bench session.

Watches the currently-growing ai_debug_logs/ai_guider_*.csv (KStars opens it once per
session and flushes a row every guide frame — see gmath.cpp's "AI DEBUG FILE LOGGER")
and plots a rolling window of:
  1. ra_error_arcsec / dec_error_arcsec  — raw drift
  2. conf, with ai_state annotated on change
  3. ra_total_pulse_ms / dec_total_pulse_ms — the pulse actually sent to the mount, with
     a zero line; this is the direct oscillation-watch panel
  4. ra_active_prop_gain — to correlate P-gain backoff with instability onset

Panels 3-4 require the blend-breakdown columns added alongside this script; tailing an
older-format CSV degrades gracefully (those panels are skipped with a one-time warning).

Usage:
    python3 tail_ai_log.py                       # tails the newest file in the default dir
    python3 tail_ai_log.py --log path/to/x.csv
    python3 tail_ai_log.py --window 60 --refresh-ms 250
"""

import argparse
import csv
import glob
import os
import sys
from collections import deque

import matplotlib.pyplot as plt
import matplotlib.animation as animation

DEFAULT_LOG_DIR = os.path.expanduser("~/.local/share/kstars/ai_debug_logs/")
# Hard cap so an all-night session doesn't grow memory unbounded; the plot only ever
# shows the last --window seconds anyway.
MAX_POINTS = 50000

BLEND_PROBE_COLUMNS = ("ra_total_pulse_ms", "dec_total_pulse_ms", "ra_active_prop_gain")


def find_latest_log(logdir: str) -> str:
    candidates = glob.glob(os.path.join(logdir, "ai_guider_*.csv"))
    if not candidates:
        return None
    return max(candidates, key=os.path.getmtime)


class LogTailer:
    """Incrementally reads a growing CSV: tracks a byte offset and only parses newly
    appended COMPLETE lines each poll, rather than re-reading the whole file."""

    def __init__(self, path: str):
        self.path = path
        self.columns = None
        self.col_index = {}
        self.offset = 0
        self.buffers = {}
        self.has_blend = False
        self._warned_old_format = False

    def _init_header(self, f) -> bool:
        header = f.readline()
        if not header:
            return False
        self.columns = [c.strip() for c in header.decode("utf-8", errors="replace").strip().split(",")]
        self.col_index = {name: i for i, name in enumerate(self.columns)}
        self.has_blend = all(c in self.col_index for c in BLEND_PROBE_COLUMNS)
        if not self.has_blend and not self._warned_old_format:
            print("[warn] old-format CSV (no blend breakdown columns) — pulse/active-gain "
                  "panels will be blank.", file=sys.stderr)
            self._warned_old_format = True
        for name in self.columns:
            self.buffers[name] = deque(maxlen=MAX_POINTS)
        self.offset = f.tell()
        return True

    def poll(self) -> bool:
        """Read any newly-appended complete lines. Returns True if new rows were parsed."""
        if not os.path.exists(self.path):
            return False
        with open(self.path, "rb") as f:
            if self.columns is None:
                if not self._init_header(f):
                    return False
            f.seek(self.offset)
            raw = f.read()
        if not raw:
            return False
        last_nl = raw.rfind(b"\n")
        if last_nl < 0:
            return False  # partial line only; wait for the next poll
        complete = raw[:last_nl + 1]
        self.offset += len(complete)

        lines = complete.decode("utf-8", errors="replace").splitlines()
        got_new = False
        for row in csv.reader(lines):
            if len(row) != len(self.columns):
                continue
            for name, val in zip(self.columns, row):
                try:
                    self.buffers[name].append(float(val))
                except ValueError:
                    self.buffers[name].append(val)  # non-numeric: algorithm/direction/state strings
            got_new = True
        return got_new

    def windowed(self, column: str, window_sec: float):
        """Returns (t, values) for `column`, clipped to the last window_sec of t_session."""
        t = self.buffers.get("t_session")
        v = self.buffers.get(column)
        if not t or not v:
            return [], []
        t_arr = list(t)
        v_arr = list(v)
        n = min(len(t_arr), len(v_arr))
        t_arr, v_arr = t_arr[-n:], v_arr[-n:]
        if not t_arr:
            return [], []
        t_max = t_arr[-1]
        cutoff = t_max - window_sec
        out_t, out_v = [], []
        for ti, vi in zip(t_arr, v_arr):
            if ti >= cutoff:
                out_t.append(ti)
                out_v.append(vi)
        return out_t, out_v


def main():
    parser = argparse.ArgumentParser(description="Live-tail and plot an AI Guider debug CSV.")
    parser.add_argument("--log", type=str, default=None,
                         help="Path to the AI debug CSV (default: newest in "
                              f"{DEFAULT_LOG_DIR})")
    parser.add_argument("--window", type=float, default=120.0, help="Rolling display window, seconds")
    parser.add_argument("--refresh-ms", type=int, default=500, help="Poll/redraw interval, milliseconds")
    args = parser.parse_args()

    log_path = args.log
    if log_path is None:
        log_path = find_latest_log(DEFAULT_LOG_DIR)
        if log_path is None:
            print(f"No log found yet in {DEFAULT_LOG_DIR} — waiting for guiding to start...")

    tailer = LogTailer(log_path) if log_path else None

    fig, axes = plt.subplots(4, 1, sharex=True, figsize=(11, 9))
    ax_err, ax_conf, ax_pulse, ax_gain = axes
    fig.suptitle("AI Guider — live tail" + (f" ({os.path.basename(log_path)})" if log_path else " (waiting...)"))

    line_ra_err, = ax_err.plot([], [], label="RA error (arcsec)", color="#c0392b", linewidth=0.9)
    line_dec_err, = ax_err.plot([], [], label="DEC error (arcsec)", color="#2980b9", linewidth=0.9)
    ax_err.legend(loc="upper right", fontsize=8)
    ax_err.set_ylabel("arcsec")
    ax_err.grid(alpha=0.3)

    line_conf, = ax_conf.plot([], [], label="confidence", color="#27ae60", linewidth=1.2)
    ax_conf.set_ylim(-0.05, 1.05)
    ax_conf.set_ylabel("confidence")
    ax_conf.grid(alpha=0.3)
    ax_conf.legend(loc="upper right", fontsize=8)

    line_ra_pulse, = ax_pulse.plot([], [], label="RA total pulse (ms)", color="#c0392b", linewidth=0.9)
    line_dec_pulse, = ax_pulse.plot([], [], label="DEC total pulse (ms)", color="#2980b9", linewidth=0.9)
    ax_pulse.axhline(0, color="gray", linewidth=0.6)
    ax_pulse.set_ylabel("ms")
    ax_pulse.grid(alpha=0.3)
    ax_pulse.legend(loc="upper right", fontsize=8)

    line_ra_gain, = ax_gain.plot([], [], label="RA active prop gain", color="#8e44ad", linewidth=0.9)
    ax_gain.set_ylabel("gain")
    ax_gain.set_xlabel("t_session (s)")
    ax_gain.grid(alpha=0.3)
    ax_gain.legend(loc="upper right", fontsize=8)

    state_annotations = {"last": None}

    def autoscale(ax, *y_lists):
        vals = [v for lst in y_lists for v in lst if v == v]  # drop NaN
        if not vals:
            return
        lo, hi = min(vals), max(vals)
        if lo == hi:
            lo, hi = lo - 1, hi + 1
        pad = 0.1 * (hi - lo)
        ax.set_ylim(lo - pad, hi + pad)

    def update(_frame):
        nonlocal tailer, log_path
        if tailer is None:
            found = find_latest_log(DEFAULT_LOG_DIR)
            if found is None:
                return ()
            log_path = found
            tailer = LogTailer(log_path)
            fig.suptitle(f"AI Guider — live tail ({os.path.basename(log_path)})")

        tailer.poll()
        w = args.window

        t_err, ra_err = tailer.windowed("ra_error_arcsec", w)
        _, dec_err = tailer.windowed("dec_error_arcsec", w)
        line_ra_err.set_data(t_err, ra_err)
        line_dec_err.set_data(t_err, dec_err)

        t_conf, conf = tailer.windowed("conf", w)
        line_conf.set_data(t_conf, conf)

        artists = [line_ra_err, line_dec_err, line_conf]

        if tailer.has_blend:
            t_p, ra_pulse = tailer.windowed("ra_total_pulse_ms", w)
            _, dec_pulse = tailer.windowed("dec_total_pulse_ms", w)
            line_ra_pulse.set_data(t_p, ra_pulse)
            line_dec_pulse.set_data(t_p, dec_pulse)

            t_g, ra_gain = tailer.windowed("ra_active_prop_gain", w)
            line_ra_gain.set_data(t_g, ra_gain)
            artists += [line_ra_pulse, line_dec_pulse, line_ra_gain]

            if t_err:
                autoscale(ax_pulse, ra_pulse, dec_pulse)
                autoscale(ax_gain, ra_gain)

        # Annotate ai_state transitions with a vertical marker.
        ai_states = tailer.buffers.get("ai_state")
        if ai_states and len(ai_states) > 0:
            current_state = ai_states[-1]
            if current_state != state_annotations["last"] and t_err:
                ax_conf.axvline(t_err[-1], color="orange", linestyle="--", linewidth=0.8)
                ax_conf.text(t_err[-1], 1.02, str(current_state), fontsize=7, color="orange",
                             ha="right", va="bottom", rotation=90)
                state_annotations["last"] = current_state

        for ax in (ax_err, ax_conf, ax_pulse, ax_gain):
            ax.relim()
            ax.autoscale_view(scalex=True, scaley=False)
        if t_err:
            for ax in (ax_err, ax_conf, ax_pulse, ax_gain):
                ax.set_xlim(max(0, t_err[0]), t_err[-1] + 0.5)
            autoscale(ax_err, ra_err, dec_err)

        return artists

    anim = animation.FuncAnimation(fig, update, interval=args.refresh_ms, cache_frame_data=False)
    plt.tight_layout()
    plt.show()
    del anim  # keep a reference alive for the duration of plt.show(); silence unused warnings


if __name__ == "__main__":
    main()
