import argparse
import json
import math
import os
import time
from dataclasses import asdict, dataclass
from datetime import datetime

import numpy as np
import serial
import vxi11


DEFAULT_SCOPE_IP = "192.168.2.28"
DEFAULT_POINTS = 1_000_000
DEFAULT_TIME_SCALE = 100e-6
TEST_PATTERN_BITS = np.array([1, 0, 1, 1, 0, 0, 1, 0], dtype=np.int8)


@dataclass
class AccuracyResult:
    mode: str
    scope_idn: str
    sample_rate_hz: float
    points: int
    ch3_vpp_v: float
    ch3_rms_ac_v: float
    carrier_hz: float
    metric_1_name: str
    metric_1_value: float
    metric_2_name: str
    metric_2_value: float
    aux_metrics: dict
    pass_fail: str
    notes: list[str]


def ensure_dir(path: str) -> None:
    os.makedirs(path, exist_ok=True)


def parse_tek_block(raw: bytes) -> np.ndarray:
    if not raw.startswith(b"#"):
      raise ValueError(f"Unexpected waveform block header: {raw[:16]!r}")
    digits = int(raw[1:2])
    count = int(raw[2:2 + digits])
    start = 2 + digits
    return np.frombuffer(raw[start:start + count], dtype=np.int8).astype(np.float64)


def ask_float(inst: vxi11.Instrument, query: str) -> float:
    return float(inst.ask(query).strip())


def configure_scope(inst: vxi11.Instrument,
                    points: int,
                    time_scale: float,
                    use_iq_ref: bool,
                    ch1_scale: float,
                    ch2_scale: float,
                    ch3_scale: float,
                    ch3_position: float,
                    use_50r: bool) -> dict:
    inst.write("ACQUIRE:STATE STOP")
    inst.write(f"SELECT:CH1 {'ON' if use_iq_ref else 'OFF'}")
    inst.write(f"SELECT:CH2 {'ON' if use_iq_ref else 'OFF'}")
    inst.write("SELECT:CH3 ON")
    if use_iq_ref:
        inst.write("CH1:COUPLING DC")
        inst.write("CH2:COUPLING DC")
        inst.write(f"CH1:SCALE {ch1_scale}")
        inst.write(f"CH2:SCALE {ch2_scale}")
    inst.write("CH3:COUPLING DC")
    inst.write(f"CH3:SCALE {ch3_scale}")
    inst.write(f"CH3:POSITION {ch3_position}")
    if use_50r:
        inst.write("CH3:TERMINATION 50")
    inst.write(f"HORIZONTAL:RECORDLENGTH {points}")
    inst.write(f"HORIZONTAL:MAIN:SCALE {time_scale}")
    inst.write("ACQUIRE:MODE SAMPLE")
    inst.write("TRIGGER:A:MODE AUTO")
    time.sleep(0.5)
    return {
        "idn": inst.ask("*IDN?").strip(),
        "record_length": inst.ask("HORIZONTAL:RECORDLENGTH?").strip(),
        "time_scale": inst.ask("HORIZONTAL:MAIN:SCALE?").strip(),
        "sample_rate": inst.ask("HORIZONTAL:MAIN:SAMPLERATE?").strip(),
    }


def acquire_channel(inst: vxi11.Instrument, ch: int, points: int) -> tuple[np.ndarray, np.ndarray, dict]:
    inst.write("ACQUIRE:STATE STOP")
    time.sleep(0.08)
    inst.write("ACQUIRE:STATE RUN")
    time.sleep(0.8)
    inst.write("ACQUIRE:STATE STOP")

    inst.write(f"DATA:SOURCE CH{ch}")
    inst.write("DATA:ENCdg RIBinary")
    inst.write("DATA:WIDTH 1")
    inst.write("DATA:START 1")
    inst.write(f"DATA:STOP {points}")
    time.sleep(0.05)

    xinc = ask_float(inst, "WFMPRE:XINCR?")
    xzero = ask_float(inst, "WFMPRE:XZERO?")
    ymult = ask_float(inst, "WFMPRE:YMULT?")
    yzero = ask_float(inst, "WFMPRE:YZERO?")
    yoff = ask_float(inst, "WFMPRE:YOFF?")
    inst.write("CURVE?")
    raw = inst.read_raw()
    adc = parse_tek_block(raw)
    y = (adc - yoff) * ymult + yzero
    t = xzero + np.arange(len(y), dtype=np.float64) * xinc
    meta = {
        "channel": ch,
        "xinc": xinc,
        "xzero": xzero,
        "ymult": ymult,
        "yzero": yzero,
        "yoff": yoff,
        "points": int(len(y)),
    }
    return t, y, meta


def analytic_signal(x: np.ndarray) -> np.ndarray:
    n = len(x)
    X = np.fft.fft(x)
    h = np.zeros(n)
    if n % 2 == 0:
        h[0] = 1
        h[n // 2] = 1
        h[1:n // 2] = 2
    else:
        h[0] = 1
        h[1:(n + 1) // 2] = 2
    return np.fft.ifft(X * h)


def moving_average(x: np.ndarray, window: int) -> np.ndarray:
    window = max(1, int(window))
    if window <= 1 or len(x) < window:
        return x.copy()
    kernel = np.ones(window, dtype=np.float64) / float(window)
    return np.convolve(x, kernel, mode="same")


def downsample_mean(x: np.ndarray, factor: int) -> np.ndarray:
    factor = max(1, int(factor))
    if factor <= 1 or len(x) < factor:
        return x.copy()
    usable = (len(x) // factor) * factor
    if usable <= 0:
        return x.copy()
    return x[:usable].reshape(-1, factor).mean(axis=1)


def dominant_tone_hz(x: np.ndarray,
                     fs_hz: float,
                     min_hz: float,
                     max_hz: float) -> float:
    x0 = x - np.mean(x)
    if len(x0) < 16:
        return 0.0
    win = np.hanning(len(x0))
    spec = np.fft.rfft(x0 * win)
    freqs = np.fft.rfftfreq(len(x0), d=1.0 / fs_hz)
    mag = np.abs(spec)
    band = (freqs >= min_hz) & (freqs <= max_hz)
    if not np.any(band):
        return 0.0
    idx_local = np.argmax(mag[band])
    idx = np.flatnonzero(band)[idx_local]
    return float(freqs[idx])


def estimate_carrier_hz(y: np.ndarray, fs_hz: float) -> float:
    y0 = y - np.mean(y)
    win = np.hanning(len(y0))
    spec = np.fft.rfft(y0 * win)
    freqs = np.fft.rfftfreq(len(y0), d=1.0 / fs_hz)
    mag = np.abs(spec)
    if len(mag) > 0:
        mag[0] = 0
    idx = int(np.argmax(mag))
    return float(freqs[idx])


def baseband_from_rf(t: np.ndarray, y: np.ndarray, carrier_hz: float) -> np.ndarray:
    z = analytic_signal(y - np.mean(y))
    return z * np.exp(-1j * 2.0 * np.pi * carrier_hz * t)


def instant_freq_hz(bb: np.ndarray, fs_hz: float, smooth_window: int) -> np.ndarray:
    phase = np.unwrap(np.angle(bb))
    fi = np.diff(phase) * (fs_hz / (2.0 * np.pi))
    return moving_average(fi, smooth_window)


def squarewave_rate_hz(x: np.ndarray, fs_hz: float, max_rate_hz: float) -> float:
    y = x - np.mean(x)
    return dominant_tone_hz(y, fs_hz, 100.0, max_rate_hz)


def estimate_nrz_bit_rate_hz(binary_levels: np.ndarray,
                             fs_hz: float,
                             expected_rate_hz: float | None = None) -> float:
    if len(binary_levels) < 8:
        return 0.0

    edges = np.flatnonzero(binary_levels[1:] != binary_levels[:-1]) + 1
    if len(edges) == 0:
        return 0.0

    run_starts = np.concatenate(([0], edges))
    run_ends = np.concatenate((edges, [len(binary_levels)]))
    run_lengths = (run_ends - run_starts).astype(np.float64)
    run_lengths = run_lengths[run_lengths >= 2.0]
    if len(run_lengths) == 0:
        return 0.0

    if expected_rate_hz and expected_rate_hz > 0.0:
        expected_samples = fs_hz / expected_rate_hz
        keep = run_lengths[(run_lengths >= max(2.0, expected_samples * 0.35)) &
                           (run_lengths <= max(4.0, expected_samples * 3.0))]
        if len(keep) != 0:
            run_lengths = keep

    low_band = np.percentile(run_lengths, 25.0)
    short_runs = run_lengths[run_lengths <= max(2.0, low_band * 1.6)]
    if len(short_runs) == 0:
        short_runs = run_lengths

    symbol_samples = float(np.median(short_runs))
    if symbol_samples <= 0.0:
        return 0.0
    return fs_hz / symbol_samples


def hysteresis_binarize(x: np.ndarray, low: float, high: float) -> np.ndarray:
    out = np.zeros(len(x), dtype=np.int8)
    state = 1 if x[0] >= high else 0
    for i, v in enumerate(x):
        if state == 0:
            if v >= high:
                state = 1
        else:
            if v <= low:
                state = 0
        out[i] = state
    return out


def estimate_two_level_spacing(x: np.ndarray) -> float:
    if len(x) == 0:
        return 0.0
    p10, p90 = np.percentile(x, [10, 90])
    return float(p90 - p10)


def estimate_symbol_rate_from_metric(metric: np.ndarray,
                                     fs_hz: float,
                                     expected_rate_hz: float) -> tuple[float, dict]:
    metric0 = metric - np.mean(metric)
    if len(metric0) < 16:
        return 0.0, {"tone_rate_hz": 0.0, "run_rate_hz": 0.0}

    smooth = moving_average(metric0, max(3, int(fs_hz / max(expected_rate_hz * 20.0, 1.0))))
    p25, p75 = np.percentile(smooth, [25, 75])
    levels = hysteresis_binarize(smooth, p25, p75)
    run_rate = estimate_nrz_bit_rate_hz(levels > 0, fs_hz, expected_rate_hz)

    deriv = np.diff(levels.astype(np.float64))
    edge_rate = dominant_tone_hz(np.abs(deriv), fs_hz, 100.0, max(100000.0, expected_rate_hz * 8.0))
    tone_rate = dominant_tone_hz(smooth, fs_hz, 100.0, max(100000.0, expected_rate_hz * 8.0))

    candidates = []
    if run_rate > 0.0:
        candidates.append(run_rate)
    if tone_rate > 0.0:
        candidates.append(tone_rate)
        candidates.append(tone_rate * 2.0)
        candidates.append(tone_rate * 4.0)
    if edge_rate > 0.0:
        candidates.append(edge_rate)
        candidates.append(edge_rate * 0.5)
        candidates.append(edge_rate * 2.0)

    if not candidates:
        return 0.0, {"tone_rate_hz": tone_rate, "run_rate_hz": run_rate, "edge_rate_hz": edge_rate}

    best = min(candidates, key=lambda v: abs(v - expected_rate_hz))
    return float(best), {"tone_rate_hz": tone_rate, "run_rate_hz": run_rate, "edge_rate_hz": edge_rate}


def two_level_centers(x: np.ndarray) -> tuple[float, float]:
    if len(x) == 0:
        return 0.0, 0.0
    c0 = float(np.percentile(x, 20.0))
    c1 = float(np.percentile(x, 80.0))
    for _ in range(12):
        d0 = np.abs(x - c0)
        d1 = np.abs(x - c1)
        g0 = x[d0 <= d1]
        g1 = x[d1 < d0]
        if len(g0) == 0 or len(g1) == 0:
            break
        nc0 = float(np.mean(g0))
        nc1 = float(np.mean(g1))
        if abs(nc0 - c0) < 1e-9 and abs(nc1 - c1) < 1e-9:
            break
        c0, c1 = nc0, nc1
    return (c0, c1) if c0 <= c1 else (c1, c0)


def pattern_fit_rate(binary_levels: np.ndarray,
                     fs_hz: float,
                     expected_rate_hz: float,
                     allow_invert: bool = True) -> tuple[float, dict]:
    if len(binary_levels) < 64 or expected_rate_hz <= 0.0:
        return 0.0, {"score": 0.0}

    expected_samples = fs_hz / expected_rate_hz
    n_min = max(4, int(round(expected_samples * 0.6)))
    n_max = max(n_min + 1, int(round(expected_samples * 1.4)))
    best = {
        "score": -1.0,
        "n": 0,
        "offset": 0,
        "phase": 0,
        "invert": 0,
    }

    for n in range(n_min, n_max + 1):
        offset_step = max(1, n // 8)
        for offset in range(0, n, offset_step):
            nsym = (len(binary_levels) - offset) // n
            if nsym < 8:
                continue
            bits = []
            for k in range(nsym):
                seg = binary_levels[offset + k * n: offset + (k + 1) * n]
                bits.append(1 if np.mean(seg) >= 0.5 else 0)
            bits = np.array(bits, dtype=np.int8)
            for phase in range(len(TEST_PATTERN_BITS)):
                ref = np.resize(np.roll(TEST_PATTERN_BITS, -phase), nsym)
                for inv in ([0, 1] if allow_invert else [0]):
                    cand = 1 - bits if inv else bits
                    score = float(np.mean(cand == ref))
                    score -= 0.02 * abs((fs_hz / n) - expected_rate_hz) / expected_rate_hz
                    if score > best["score"]:
                        best = {
                            "score": score,
                            "n": n,
                            "offset": offset,
                            "phase": phase,
                            "invert": inv,
                            "nsym": nsym,
                        }

    if best["n"] <= 0:
        return 0.0, {"score": 0.0}
    return fs_hz / float(best["n"]), best


def symbol_means(x: np.ndarray, samples_per_symbol: int, offset: int) -> np.ndarray:
    n = int(samples_per_symbol)
    if n <= 0 or offset >= len(x):
        return np.array([], dtype=np.float64)
    nsym = (len(x) - offset) // n
    if nsym <= 0:
        return np.array([], dtype=np.float64)
    vals = []
    for k in range(nsym):
        seg = x[offset + k * n: offset + (k + 1) * n]
        vals.append(float(np.mean(seg)))
    return np.array(vals, dtype=np.float64)


def symbol_center_means(x: np.ndarray,
                        samples_per_symbol: int,
                        offset: int,
                        keep_frac: float = 0.5) -> np.ndarray:
    n = int(samples_per_symbol)
    if n <= 0 or offset >= len(x):
        return np.array([], dtype=np.float64)
    nsym = (len(x) - offset) // n
    if nsym <= 0:
        return np.array([], dtype=np.float64)
    keep_frac = min(max(keep_frac, 0.1), 1.0)
    keep = max(1, int(round(n * keep_frac)))
    trim = max(0, (n - keep) // 2)
    vals = []
    for k in range(nsym):
        base = offset + k * n
        seg = x[base + trim: base + trim + keep]
        vals.append(float(np.mean(seg)))
    return np.array(vals, dtype=np.float64)


def pattern_fit_symbol_levels(levels: np.ndarray,
                              expected_rate_hz: float,
                              fs_hz: float) -> dict:
    if len(levels) < 128 or expected_rate_hz <= 0.0:
        return {"score": -1.0}

    expected_n = fs_hz / expected_rate_hz
    n_min = max(4, int(round(expected_n * 0.7)))
    n_max = max(n_min + 1, int(round(expected_n * 1.3)))
    best = {"score": -1.0}

    for n in range(n_min, n_max + 1):
        offset_step = max(1, n // 8)
        for offset in range(0, n, offset_step):
            vals = symbol_means(levels, n, offset)
            if len(vals) < 8:
                continue
            c0, c1 = two_level_centers(vals)
            mid = 0.5 * (c0 + c1)
            bits = (vals >= mid).astype(np.int8)
            sep = abs(c1 - c0)
            spread0 = float(np.std(vals[bits == 0])) if np.any(bits == 0) else 1e9
            spread1 = float(np.std(vals[bits == 1])) if np.any(bits == 1) else 1e9
            noise = max(spread0 + spread1, 1e-9)
            sep_score = sep / noise

            for phase in range(len(TEST_PATTERN_BITS)):
                ref = np.resize(np.roll(TEST_PATTERN_BITS, -phase), len(bits))
                for inv in (0, 1):
                    cand = 1 - bits if inv else bits
                    match = float(np.mean(cand == ref))
                    rate_hz = fs_hz / float(n)
                    rate_penalty = abs(rate_hz - expected_rate_hz) / expected_rate_hz
                    score = match + 0.05 * sep_score - 0.08 * rate_penalty
                    if score > best["score"]:
                        best = {
                            "score": score,
                            "n": n,
                            "offset": offset,
                            "phase": phase,
                            "invert": inv,
                            "rate_hz": rate_hz,
                            "center0": c0,
                            "center1": c1,
                            "match": match,
                            "sep_score": sep_score,
                        }
    return best


def basic_ch3_metrics(y: np.ndarray, fs_hz: float) -> tuple[float, float, float]:
    y0 = y - np.mean(y)
    vpp = float(np.max(y) - np.min(y))
    rms_ac = float(np.sqrt(np.mean(y0 * y0)))
    carrier_hz = estimate_carrier_hz(y, fs_hz)
    return vpp, rms_ac, carrier_hz


def build_iq_track(ch1: np.ndarray, ch2: np.ndarray) -> np.ndarray:
    i = ch1 - np.mean(ch1)
    q = ch2 - np.mean(ch2)
    i_rms = float(np.sqrt(np.mean(i * i)))
    q_rms = float(np.sqrt(np.mean(q * q)))
    if q_rms > 1e-12 and i_rms > 1e-12:
        q = q * (i_rms / q_rms)
    return i + 1j * q


def analyze_am(t: np.ndarray,
               y: np.ndarray,
               fs_hz: float,
               mod_freq_hz: float,
               depth_pct: float) -> tuple[float, float, str, list[str]]:
    env = np.abs(analytic_signal(y - np.mean(y)))
    env_s = moving_average(env, max(8, int(fs_hz / max(1.0, mod_freq_hz * 40.0))))
    fmod = dominant_tone_hz(env_s - np.mean(env_s), fs_hz, 100.0, max(100000.0, mod_freq_hz * 6.0))
    p5, p95 = np.percentile(env_s, [5, 95])
    depth = 100.0 * float((p95 - p5) / (p95 + p5)) if (p95 + p5) > 0 else 0.0
    notes = []
    ok = True
    if abs(fmod - mod_freq_hz) > max(500.0, mod_freq_hz * 0.05):
        ok = False
        notes.append(f"AM frequency mismatch: got {fmod:.1f} Hz")
    if abs(depth - depth_pct) > max(5.0, depth_pct * 0.12):
        ok = False
        notes.append(f"AM depth mismatch: got {depth:.2f}%")
    if ok:
        notes.append("AM modulation frequency and depth are within tolerance")
    return fmod, depth, "PASS" if ok else "FAIL", notes


def analyze_fm(t: np.ndarray,
               y: np.ndarray,
               fs_hz: float,
               mod_freq_hz: float,
               dev_hz: float) -> tuple[float, float, str, list[str]]:
    carrier_hz = estimate_carrier_hz(y, fs_hz)
    bb = baseband_from_rf(t, y, carrier_hz)
    fi = instant_freq_hz(bb, fs_hz, max(8, int(fs_hz / 200000.0)))
    fi_ac = fi - np.mean(fi)
    fmod = dominant_tone_hz(fi_ac, fs_hz, 100.0, max(150000.0, mod_freq_hz * 8.0))
    p1, p99 = np.percentile(fi_ac, [1, 99])
    dev = 0.5 * float(p99 - p1)
    notes = []
    ok = True
    if abs(fmod - mod_freq_hz) > max(500.0, mod_freq_hz * 0.08):
        ok = False
        notes.append(f"FM modulation frequency mismatch: got {fmod:.1f} Hz")
    if abs(dev - dev_hz) > max(5000.0, dev_hz * 0.15):
        ok = False
        notes.append(f"FM deviation mismatch: got {dev:.1f} Hz")
    if ok:
        notes.append("FM modulation frequency and deviation are within tolerance")
    return fmod, dev, "PASS" if ok else "FAIL", notes


def analyze_ask(t: np.ndarray,
                y: np.ndarray,
                fs_hz: float,
                bit_rate_hz: float) -> tuple[float, float, str, list[str]]:
    env = np.abs(analytic_signal(y - np.mean(y)))
    env_s = moving_average(env, max(16, int(fs_hz / max(1.0, bit_rate_hz * 12.0))))
    ds_factor = max(1, int(fs_hz / max(bit_rate_hz * 64.0, 1.0)))
    env_ds = downsample_mean(env_s, ds_factor)
    fs_ds = fs_hz / ds_factor
    env_track = moving_average(env_ds, max(3, int(fs_ds / max(bit_rate_hz * 8.0, 1.0))))
    lo = float(np.percentile(env_s, 10))
    hi = float(np.percentile(env_s, 90))
    ratio = lo / hi if hi > 1e-12 else 1.0
    rate, aux = estimate_symbol_rate_from_metric(env_track, fs_ds, bit_rate_hz)
    notes = []
    ok = True
    if abs(rate - bit_rate_hz) > max(500.0, bit_rate_hz * 0.08):
        ok = False
        notes.append(f"ASK symbol rate mismatch: got {rate:.1f} Hz")
        if aux.get("tone_rate_hz", 0.0) > 0.0:
            notes.append(f"ASK envelope tone estimate was {aux['tone_rate_hz']:.1f} Hz")
        if aux.get("run_rate_hz", 0.0) > 0.0:
            notes.append(f"ASK run-length estimate was {aux['run_rate_hz']:.1f} Hz")
        if aux.get("edge_rate_hz", 0.0) > 0.0:
            notes.append(f"ASK edge-rate estimate was {aux['edge_rate_hz']:.1f} Hz")
    if ratio > 0.25:
        ok = False
        notes.append(f"ASK low/high envelope ratio too high: {ratio:.3f}")
    if ok:
        notes.append("ASK symbol rate and on/off envelope ratio are within tolerance")
    return rate, ratio, "PASS" if ok else "FAIL", notes


def analyze_fsk(t: np.ndarray,
                y: np.ndarray,
                fs_hz: float,
                bit_rate_hz: float,
                shift_hz: float) -> tuple[float, float, str, list[str]]:
    carrier_hz = estimate_carrier_hz(y, fs_hz)
    bb = baseband_from_rf(t, y, carrier_hz)
    fi = instant_freq_hz(bb, fs_hz, max(16, int(fs_hz / 250000.0)))
    fi_s = moving_average(fi, max(16, int(fs_hz / max(bit_rate_hz * 16.0, 1.0))))
    ds_factor = max(1, int(fs_hz / max(bit_rate_hz * 96.0, 1.0)))
    fi_ds = downsample_mean(fi_s, ds_factor)
    fs_ds = fs_hz / ds_factor
    fi_track = moving_average(fi_ds, max(3, int(fs_ds / max(bit_rate_hz * 8.0, 1.0))))
    c0, c1 = two_level_centers(fi_track)
    shift = float(abs(c1 - c0))
    mid = 0.5 * (c0 + c1)
    hyst = max(shift * 0.15, 500.0)
    levels = hysteresis_binarize(fi_track, mid - hyst, mid + hyst)
    run_rate = estimate_nrz_bit_rate_hz(levels > 0, fs_ds, bit_rate_hz)
    rate, aux = estimate_symbol_rate_from_metric(fi_track, fs_ds, bit_rate_hz)
    fit_rate, fit = pattern_fit_rate(levels, fs_ds, bit_rate_hz, allow_invert=True)
    if fit_rate > 0.0:
        rate = fit_rate
        aux["pattern_rate_hz"] = fit_rate
        sym_vals = symbol_means(fi_track, fit["n"], fit["offset"])
        if len(sym_vals) >= 8:
            ref = np.resize(np.roll(TEST_PATTERN_BITS, -fit["phase"]), len(sym_vals))
            if fit["invert"]:
                ref = 1 - ref
            g0 = sym_vals[ref == 0]
            g1 = sym_vals[ref == 1]
            if len(g0) and len(g1):
                shift = float(abs(np.mean(g1) - np.mean(g0)))
    if run_rate > 0.0 and abs(run_rate - bit_rate_hz) < abs(rate - bit_rate_hz):
        rate = run_rate
        aux["run_rate_hz"] = run_rate
    notes = []
    ok = True
    if abs(rate - bit_rate_hz) > max(500.0, bit_rate_hz * 0.10):
        ok = False
        notes.append(f"FSK symbol rate mismatch: got {rate:.1f} Hz")
        if aux.get("tone_rate_hz", 0.0) > 0.0:
            notes.append(f"FSK tone estimate was {aux['tone_rate_hz']:.1f} Hz")
        if aux.get("run_rate_hz", 0.0) > 0.0:
            notes.append(f"FSK run-length estimate was {aux['run_rate_hz']:.1f} Hz")
    if abs(shift - shift_hz) > max(2000.0, shift_hz * 0.18):
        ok = False
        notes.append(f"FSK shift mismatch: got {shift:.1f} Hz")
    if ok:
        notes.append("FSK symbol rate and shift are within tolerance")
    return rate, shift, "PASS" if ok else "FAIL", notes


def analyze_fsk_iq(ch1: np.ndarray,
                   ch2: np.ndarray,
                   fs_hz: float,
                   bit_rate_hz: float,
                   shift_hz: float) -> tuple[float, float, str, list[str], dict]:
    iq = build_iq_track(ch1, ch2)
    phase = np.unwrap(np.angle(iq))
    fi = np.diff(phase) * (fs_hz / (2.0 * np.pi))
    fi = moving_average(fi, max(4, int(fs_hz / 600000.0)))
    ds_factor = max(1, int(fs_hz / max(bit_rate_hz * 256.0, 1.0)))
    fi_ds = downsample_mean(fi, ds_factor)
    fs_ds = fs_hz / ds_factor
    fi_track = moving_average(fi_ds, max(2, int(fs_ds / max(bit_rate_hz * 20.0, 1.0))))

    fit = pattern_fit_symbol_levels(fi_track, bit_rate_hz, fs_ds)
    if fit.get("score", -1.0) > 0.4:
        rate = float(fit["rate_hz"])
        c0 = float(fit["center0"])
        c1 = float(fit["center1"])
        match = float(fit["match"])
        sym_vals = symbol_center_means(fi_track, fit["n"], fit["offset"], keep_frac=0.5)
        if len(sym_vals) >= 8:
            ref = np.resize(np.roll(TEST_PATTERN_BITS, -fit["phase"]), len(sym_vals))
            if fit["invert"]:
                ref = 1 - ref
            g0 = sym_vals[ref == 0]
            g1 = sym_vals[ref == 1]
            if len(g0) and len(g1):
                c0 = float(np.mean(g0))
                c1 = float(np.mean(g1))
        shift = float(abs(c1 - c0))
    else:
        c0, c1 = two_level_centers(fi_track)
        shift = float(abs(c1 - c0))
        mid = 0.5 * (c0 + c1)
        hyst = max(shift * 0.10, 100.0)
        levels = hysteresis_binarize(fi_track, mid - hyst, mid + hyst)
        rate = estimate_nrz_bit_rate_hz(levels > 0, fs_ds, bit_rate_hz)
        match = 0.0

    notes = []
    ok = True
    if abs(rate - bit_rate_hz) > max(500.0, bit_rate_hz * 0.08):
        ok = False
        notes.append(f"FSK symbol rate mismatch: got {rate:.1f} Hz")
    if abs(shift - shift_hz) > max(2000.0, shift_hz * 0.12):
        ok = False
        notes.append(f"FSK shift mismatch: got {shift:.1f} Hz")
    if ok:
        notes.append("FSK I/Q symbol rate and shift are within tolerance")
    return rate, shift, "PASS" if ok else "FAIL", notes, {
        "fs_iq_ds_hz": fs_ds,
        "fsk_state0_hz": c0,
        "fsk_state1_hz": c1,
        "fsk_pattern_score": float(fit.get("score", 0.0)) if isinstance(fit, dict) else 0.0,
        "fsk_pattern_match": match,
    }


def analyze_psk(t: np.ndarray,
                y: np.ndarray,
                fs_hz: float,
                bit_rate_hz: float) -> tuple[float, float, str, list[str]]:
    carrier_hz = estimate_carrier_hz(y, fs_hz)
    bb = baseband_from_rf(t, y, carrier_hz)
    bb = bb / (np.abs(bb) + 1e-12)
    rot = 0.5 * np.angle(np.mean(bb * bb))
    bb = bb * np.exp(-1j * rot)
    phase_sign = np.real(bb)
    real_track = moving_average(phase_sign, max(24, int(fs_hz / max(bit_rate_hz * 12.0, 1.0))))
    ds_factor = max(1, int(fs_hz / max(bit_rate_hz * 128.0, 1.0)))
    real_ds = downsample_mean(real_track, ds_factor)
    fs_ds = fs_hz / ds_factor
    real_s = moving_average(real_ds, max(5, int(fs_ds / max(bit_rate_hz * 10.0, 1.0))))
    rate, aux = estimate_symbol_rate_from_metric(real_s, fs_ds, bit_rate_hz)

    p25, p75 = np.percentile(real_s, [25, 75])
    bits = hysteresis_binarize(real_s, p25, p75)
    run_rate = estimate_nrz_bit_rate_hz(bits > 0, fs_ds, bit_rate_hz)
    fit_rate, fit = pattern_fit_rate(bits, fs_ds, bit_rate_hz, allow_invert=True)
    if fit_rate > 0.0:
        rate = fit_rate
        aux["pattern_rate_hz"] = fit_rate
    if run_rate > 0.0 and abs(run_rate - bit_rate_hz) < abs(rate - bit_rate_hz):
        rate = run_rate
        aux["run_rate_hz"] = run_rate
    edges = np.flatnonzero(bits[1:] != bits[:-1]) + 1
    phase_flips = []
    win = max(2, int(fs_ds / max(bit_rate_hz * 3.0, 1.0)))
    for idx in edges:
        left0 = max(0, idx - win)
        left1 = idx
        right0 = idx
        right1 = min(len(real_s), idx + win)
        if (left1 - left0) < 2 or (right1 - right0) < 2:
            continue
        a = np.mean(real_s[left0:left1])
        b = np.mean(real_s[right0:right1])
        if abs(a) < 1e-9 or abs(b) < 1e-9:
            continue
        if a * b < 0:
            phase_flips.append(180.0)
        else:
            phase_flips.append(0.0)
    phase_flip_deg = float(np.median(phase_flips)) if phase_flips else 0.0

    env = np.abs(analytic_signal(y - np.mean(y)))
    env_depth = float((np.percentile(env, 95) - np.percentile(env, 5)) /
                      (np.percentile(env, 95) + np.percentile(env, 5) + 1e-12))
    notes = []
    ok = True
    if abs(rate - bit_rate_hz) > max(500.0, bit_rate_hz * 0.10):
        ok = False
        notes.append(f"PSK symbol rate mismatch: got {rate:.1f} Hz")
        if aux.get("tone_rate_hz", 0.0) > 0.0:
            notes.append(f"PSK tone estimate was {aux['tone_rate_hz']:.1f} Hz")
        if aux.get("run_rate_hz", 0.0) > 0.0:
            notes.append(f"PSK run-length estimate was {aux['run_rate_hz']:.1f} Hz")
        if aux.get("pattern_rate_hz", 0.0) > 0.0:
            notes.append(f"PSK pattern-fit estimate was {aux['pattern_rate_hz']:.1f} Hz")
    if phase_flip_deg < 120.0:
        ok = False
        notes.append(f"PSK phase flip is too small: {phase_flip_deg:.1f} deg")
    if env_depth > 0.35:
        ok = False
        notes.append(f"PSK envelope variation is too large: {env_depth:.3f}")
    if ok:
        notes.append("PSK symbol rate, phase flip and envelope stability are within tolerance")
    return rate, phase_flip_deg, "PASS" if ok else "FAIL", notes


def analyze_psk_iq(ch1: np.ndarray,
                   ch2: np.ndarray,
                   fs_hz: float,
                   bit_rate_hz: float) -> tuple[float, float, str, list[str], dict]:
    iq = build_iq_track(ch1, ch2)
    rot = 0.5 * np.angle(np.mean(iq * iq))
    iq = iq * np.exp(-1j * rot)
    i_track = moving_average(np.real(iq), max(8, int(fs_hz / max(bit_rate_hz * 12.0, 1.0))))
    ds_factor = max(1, int(fs_hz / max(bit_rate_hz * 128.0, 1.0)))
    i_ds = downsample_mean(i_track, ds_factor)
    fs_ds = fs_hz / ds_factor
    i_s = moving_average(i_ds, max(5, int(fs_ds / max(bit_rate_hz * 10.0, 1.0))))
    p25, p75 = np.percentile(i_s, [25, 75])
    bits = hysteresis_binarize(i_s, p25, p75)

    rate, fit = pattern_fit_rate(bits, fs_ds, bit_rate_hz, allow_invert=True)
    run_rate = estimate_nrz_bit_rate_hz(bits > 0, fs_ds, bit_rate_hz)
    if run_rate > 0.0 and abs(run_rate - bit_rate_hz) < abs(rate - bit_rate_hz):
        rate = run_rate

    edges = np.flatnonzero(bits[1:] != bits[:-1]) + 1
    phase_flips = []
    win = max(2, int(fs_ds / max(bit_rate_hz * 4.0, 1.0)))
    for idx in edges:
        left0 = max(0, idx - win)
        left1 = idx
        right0 = idx
        right1 = min(len(i_s), idx + win)
        if (left1 - left0) < 2 or (right1 - right0) < 2:
            continue
        a = np.mean(i_s[left0:left1])
        b = np.mean(i_s[right0:right1])
        if a * b < 0:
            phase_flips.append(180.0)
    phase_flip_deg = float(np.median(phase_flips)) if phase_flips else 0.0

    notes = []
    ok = True
    if abs(rate - bit_rate_hz) > max(500.0, bit_rate_hz * 0.08):
        ok = False
        notes.append(f"PSK symbol rate mismatch: got {rate:.1f} Hz")
    if phase_flip_deg < 120.0:
        ok = False
        notes.append(f"PSK phase flip is too small: {phase_flip_deg:.1f} deg")
    if ok:
        notes.append("PSK I/Q symbol rate and phase flip are within tolerance")
    return rate, phase_flip_deg, "PASS" if ok else "FAIL", notes, {
        "fs_iq_ds_hz": fs_ds,
        "psk_pattern_score": float(fit.get("score", 0.0)) if isinstance(fit, dict) else 0.0,
        "psk_run_rate_hz": run_rate,
    }


def send_lines_if_needed(port: str | None,
                         baudrate: int,
                         lines: list[str],
                         char_delay_ms: float,
                         settle_s: float) -> str:
    if not port or not lines:
        return ""
    out = []
    with serial.Serial(port, baudrate, timeout=0.1) as ser:
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        for line in lines:
            for ch in (line + "\r\n"):
                ser.write(ch.encode("ascii"))
                ser.flush()
                time.sleep(char_delay_ms / 1000.0)
            time.sleep(0.05)
            deadline = time.time() + settle_s
            while time.time() < deadline:
                data = ser.read(4096)
                if data:
                    out.append(data.decode("ascii", errors="replace"))
                else:
                    time.sleep(0.03)
    return "".join(out)


def save_csv(path: str,
             t: np.ndarray,
             ch3: np.ndarray,
             ch1: np.ndarray | None = None,
             ch2: np.ndarray | None = None) -> None:
    with open(path, "w", encoding="utf-8") as f:
        if ch1 is not None and ch2 is not None:
            f.write("t_s,ch1_v,ch2_v,ch3_v\n")
        else:
            f.write("t_s,ch3_v\n")
        step = max(1, len(t) // 300000)
        for i in range(0, len(t), step):
            if ch1 is not None and ch2 is not None:
                f.write(f"{t[i]:.12e},{ch1[i]:.9e},{ch2[i]:.9e},{ch3[i]:.9e}\n")
            else:
                f.write(f"{t[i]:.12e},{ch3[i]:.9e}\n")


def run(args: argparse.Namespace) -> None:
    out_root = os.path.abspath(args.out)
    ensure_dir(out_root)
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    out_dir = os.path.join(out_root, f"tek_ch3_accuracy_{args.mode.lower()}_{stamp}")
    ensure_dir(out_dir)

    serial_log = send_lines_if_needed(args.port,
                                      args.baudrate,
                                      args.line or [],
                                      args.char_delay_ms,
                                      args.settle)

    use_iq_ref = (args.use_iq_ref or args.mode.upper() in ("FSK", "PSK"))

    inst = vxi11.Instrument(args.scope)
    inst.timeout = args.scope_timeout
    scope_meta = configure_scope(inst,
                                 args.points,
                                 args.time_scale,
                                 use_iq_ref,
                                 args.ch1_scale,
                                 args.ch2_scale,
                                 args.ch3_scale,
                                 args.ch3_position,
                                 args.use_50r)
    if use_iq_ref:
        t1, ch1, meta1 = acquire_channel(inst, 1, args.points)
        t2, ch2, meta2 = acquire_channel(inst, 2, args.points)
    else:
        t1 = ch1 = meta1 = None
        t2 = ch2 = meta2 = None
    t, y, wave_meta = acquire_channel(inst, 3, args.points)
    fs_hz = 1.0 / float(np.median(np.diff(t)))
    vpp, rms_ac, carrier_hz = basic_ch3_metrics(y, fs_hz)
    aux_metrics = {}

    mode = args.mode.upper()
    if mode == "AM":
        m1, m2, verdict, notes = analyze_am(t, y, fs_hz, args.mod_freq_hz, args.am_depth_pct)
        metric_1_name = "mod_freq_hz"
        metric_2_name = "am_depth_pct"
    elif mode == "FM":
        m1, m2, verdict, notes = analyze_fm(t, y, fs_hz, args.mod_freq_hz, args.fm_dev_hz)
        metric_1_name = "mod_freq_hz"
        metric_2_name = "fm_dev_hz"
    elif mode == "ASK":
        m1, m2, verdict, notes = analyze_ask(t, y, fs_hz, args.bit_rate_hz)
        metric_1_name = "bit_rate_hz"
        metric_2_name = "low_high_ratio"
    elif mode == "FSK":
        if use_iq_ref and ch1 is not None and ch2 is not None:
            fs_iq = 1.0 / float(np.median(np.diff(t1)))
            m1, m2, verdict, notes, aux_metrics = analyze_fsk_iq(ch1, ch2, fs_iq, args.bit_rate_hz, args.fsk_shift_hz)
        else:
            m1, m2, verdict, notes = analyze_fsk(t, y, fs_hz, args.bit_rate_hz, args.fsk_shift_hz)
        metric_1_name = "bit_rate_hz"
        metric_2_name = "fsk_shift_hz"
    elif mode == "PSK":
        if use_iq_ref and ch1 is not None and ch2 is not None:
            fs_iq = 1.0 / float(np.median(np.diff(t1)))
            m1, m2, verdict, notes, aux_metrics = analyze_psk_iq(ch1, ch2, fs_iq, args.bit_rate_hz)
        else:
            m1, m2, verdict, notes = analyze_psk(t, y, fs_hz, args.bit_rate_hz)
        metric_1_name = "bit_rate_hz"
        metric_2_name = "phase_flip_deg"
    else:
        raise ValueError(f"Unsupported mode: {args.mode}")

    result = AccuracyResult(
        mode=mode,
        scope_idn=scope_meta["idn"],
        sample_rate_hz=fs_hz,
        points=int(len(y)),
        ch3_vpp_v=vpp,
        ch3_rms_ac_v=rms_ac,
        carrier_hz=carrier_hz,
        metric_1_name=metric_1_name,
        metric_1_value=float(m1),
        metric_2_name=metric_2_name,
        metric_2_value=float(m2),
        aux_metrics=aux_metrics,
        pass_fail=verdict,
        notes=notes,
    )

    save_csv(os.path.join(out_dir, "waveforms.csv"), t, y, ch1, ch2)
    with open(os.path.join(out_dir, "result.json"), "w", encoding="utf-8") as f:
        json.dump({
            "args": vars(args),
            "scope_meta": scope_meta,
            "wave_meta": {
                "ch3": wave_meta,
                "ch1": meta1,
                "ch2": meta2,
            },
            "serial_log": serial_log,
            "result": asdict(result),
        }, f, indent=2, ensure_ascii=False)

    with open(os.path.join(out_dir, "summary.md"), "w", encoding="utf-8") as f:
        f.write(f"# CH3 Accuracy Summary\n\n")
        f.write(f"- Mode: `{result.mode}`\n")
        f.write(f"- Verdict: `{result.pass_fail}`\n")
        f.write(f"- Scope: `{result.scope_idn}`\n")
        f.write(f"- Sample rate: `{result.sample_rate_hz:.3f} Hz`\n")
        f.write(f"- Points: `{result.points}`\n")
        f.write(f"- CH3 Vpp: `{result.ch3_vpp_v:.6f} V`\n")
        f.write(f"- CH3 RMS(ac): `{result.ch3_rms_ac_v:.6f} V`\n")
        f.write(f"- Carrier: `{result.carrier_hz:.3f} Hz`\n")
        f.write(f"- {result.metric_1_name}: `{result.metric_1_value:.6f}`\n")
        f.write(f"- {result.metric_2_name}: `{result.metric_2_value:.6f}`\n")
        if result.aux_metrics:
            f.write("\n## Aux Metrics\n\n")
            for k, v in result.aux_metrics.items():
                f.write(f"- {k}: `{v}`\n")
        f.write("\n## Notes\n\n")
        for note in result.notes:
            f.write(f"- {note}\n")
        if serial_log:
            f.write("\n## Serial Log\n\n```text\n")
            f.write(serial_log)
            if not serial_log.endswith("\n"):
                f.write("\n")
            f.write("```\n")

    print(f"RESULT_DIR={out_dir}")
    print(f"MODE={result.mode}")
    print(f"VERDICT={result.pass_fail}")
    print(f"CH3_VPP_V={result.ch3_vpp_v:.6f}")
    print(f"CH3_RMS_AC_V={result.ch3_rms_ac_v:.6f}")
    print(f"CARRIER_HZ={result.carrier_hz:.3f}")
    print(f"{result.metric_1_name.upper()}={result.metric_1_value:.6f}")
    print(f"{result.metric_2_name.upper()}={result.metric_2_value:.6f}")
    for note in result.notes:
        print(f"NOTE={note}")


def build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Read Tektronix CH3 waveform and judge AM/FM/ASK/FSK/PSK accuracy.")
    p.add_argument("--scope", default=DEFAULT_SCOPE_IP)
    p.add_argument("--scope-timeout", type=int, default=30)
    p.add_argument("--mode", required=True, choices=["AM", "FM", "ASK", "FSK", "PSK"])
    p.add_argument("--points", type=int, default=DEFAULT_POINTS)
    p.add_argument("--time-scale", type=float, default=DEFAULT_TIME_SCALE)
    p.add_argument("--use-iq-ref", action="store_true", help="Capture CH1/CH2 together with CH3 and use I/Q reference for FSK/PSK.")
    p.add_argument("--ch1-scale", type=float, default=0.5)
    p.add_argument("--ch2-scale", type=float, default=0.5)
    p.add_argument("--ch3-scale", type=float, default=0.5)
    p.add_argument("--ch3-position", type=float, default=0.0)
    p.add_argument("--use-50r", action="store_true")
    p.add_argument("--out", default="tools\\tek_ch3_accuracy_results")
    p.add_argument("--port", default=None, help="Optional serial port. If set, lines will be sent before capture.")
    p.add_argument("--baudrate", type=int, default=115200)
    p.add_argument("--line", action="append", default=[], help="Optional command line to send before capture; can be repeated.")
    p.add_argument("--char-delay-ms", type=float, default=15.0)
    p.add_argument("--settle", type=float, default=0.8)

    p.add_argument("--mod-freq-hz", type=float, default=10000.0)
    p.add_argument("--am-depth-pct", type=float, default=50.0)
    p.add_argument("--fm-dev-hz", type=float, default=75000.0)
    p.add_argument("--bit-rate-hz", type=float, default=10000.0)
    p.add_argument("--fsk-shift-hz", type=float, default=20000.0)
    return p


def main() -> None:
    args = build_arg_parser().parse_args()
    run(args)


if __name__ == "__main__":
    main()
