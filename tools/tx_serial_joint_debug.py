import argparse
import json
import math
import os
import re
import subprocess
import sys
import time
from dataclasses import dataclass, asdict
from datetime import datetime
from pathlib import Path

import serial
import vxi11


DEFAULT_SCOPE_IP = "192.168.2.28"
DEFAULT_H743_PORT = "COM5"
DEFAULT_BAUDRATE = 115200
DEFAULT_ANALYZER_PATH = r"F:\CAMKE\WIRELESS\Simple_wireless_automatic_transceiver_deviceV2.0\tools\tek_ch3_signal_accuracy.py"
DEFAULT_ANALYZER_PYTHON = r"C:\Users\liuhao\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
DEFAULT_OUT = r"tools\tx_joint_debug_results"


@dataclass
class SweepMeasurement:
    t_s: float
    freq_hz: float
    rms_v: float
    vpp_v: float


def ensure_dir(path: str) -> None:
    os.makedirs(path, exist_ok=True)


class SerialCli:
    def __init__(self, port: str, baudrate: int, char_delay_ms: float) -> None:
        self._ser = serial.Serial(port, baudrate, timeout=0.05)
        self._char_delay_s = char_delay_ms / 1000.0
        self._ser.reset_input_buffer()
        self._ser.reset_output_buffer()

    def close(self) -> None:
        self._ser.close()

    def send_line(self, line: str, settle_s: float = 0.25) -> str:
        for ch in (line + "\r\n"):
          self._ser.write(ch.encode("ascii"))
          self._ser.flush()
          time.sleep(self._char_delay_s)
        deadline = time.time() + settle_s
        chunks: list[str] = []
        while time.time() < deadline:
            data = self._ser.read(4096)
            if data:
                chunks.append(data.decode("ascii", errors="replace"))
            else:
                time.sleep(0.02)
        return "".join(chunks)


def parse_dds_status(text: str) -> dict[int, dict[str, int]]:
    out: dict[int, dict[str, int]] = {}
    for m in re.finditer(r"DDS CH(\d)\s+FREQ\s+(\d+)\s+AMP\s+(\d+)", text):
        ch = int(m.group(1))
        out[ch] = {"freq_hz": int(m.group(2)), "amp_code": int(m.group(3))}
    return out


def parse_sweep_status(text: str) -> dict[str, int | str]:
    m = re.search(
        r"SWEEP MODE (\S+)\s+START (\d+)\s+STOP (\d+)\s+PERIOD_MS (\d+)\s+ENABLE (\d+)\s+CURRENT (\d+)\s+REL1 (\d+)",
        text,
    )
    if not m:
        return {}
    return {
        "mode": m.group(1),
        "start_hz": int(m.group(2)),
        "stop_hz": int(m.group(3)),
        "period_ms": int(m.group(4)),
        "enabled": int(m.group(5)),
        "current_hz": int(m.group(6)),
        "rel1": int(m.group(7)),
    }


def tek_measure(inst: vxi11.Instrument, measure_type: str) -> float:
    inst.write("MEASUrement:IMMed:SOUrce1 CH3")
    inst.write(f"MEASUrement:IMMed:TYPe {measure_type}")
    value = float(inst.ask("MEASUrement:IMMed:VALue?").strip())
    if math.isinf(value) or math.isnan(value) or abs(value) > 1e36:
        return float("nan")
    return value


def configure_scope_measurement(inst: vxi11.Instrument, use_50r: bool) -> None:
    inst.write("SELECT:CH3 ON")
    inst.write("CH3:COUPLING DC")
    inst.write("CH3:SCALE 0.2")
    inst.write("CH3:POSITION 0")
    if use_50r:
        inst.write("CH3:TERMINATION 50")
    inst.write("TRIGGER:A:MODE AUTO")
    inst.write("ACQUIRE:MODE SAMPLE")
    time.sleep(0.2)


def run_single_mode(analyzer_path: str,
                    analyzer_python: str,
                    mode: str,
                    out_root: str,
                    port: str,
                    scope_ip: str,
                    extra_lines: list[str],
                    extra_args: list[str]) -> tuple[dict, str]:
    before = set(os.listdir(out_root)) if os.path.isdir(out_root) else set()
    cmd = [
        analyzer_python,
        analyzer_path,
        "--mode", mode,
        "--scope", scope_ip,
        "--port", port,
        "--baudrate", str(DEFAULT_BAUDRATE),
        "--char-delay-ms", "15",
        "--settle", "0.8",
        "--out", out_root,
        "--use-50r",
    ]
    if mode in ("FSK", "PSK"):
        cmd.append("--use-iq-ref")
    for line in extra_lines:
        cmd.extend(["--line", line])
    cmd.extend(extra_args)
    proc = subprocess.run(cmd, capture_output=True, text=True, check=True, timeout=240)
    after = set(os.listdir(out_root))
    created = sorted(after - before)
    if created:
        result_dir = os.path.join(out_root, created[-1])
    else:
        m = re.search(r"RESULT_DIR=(.+)", proc.stdout)
        if not m:
            raise RuntimeError(f"Unable to determine result dir for {mode}\n{proc.stdout}\n{proc.stderr}")
        result_dir = m.group(1).strip()
    with open(os.path.join(result_dir, "result.json"), "r", encoding="utf-8") as f:
        return json.load(f), result_dir


def run_cw_sweep(port: str,
                 baudrate: int,
                 char_delay_ms: float,
                 scope_ip: str,
                 use_50r: bool,
                 start_hz: int,
                 stop_hz: int,
                 period_ms: int) -> dict:
    cli = SerialCli(port, baudrate, char_delay_ms)
    scope = vxi11.Instrument(scope_ip)
    scope.timeout = 10
    configure_scope_measurement(scope, use_50r)

    serial_log: list[dict] = []
    def do_cmd(cmd: str, settle: float = 0.25) -> str:
        reply = cli.send_line(cmd, settle_s=settle)
        serial_log.append({"cmd": cmd, "reply": reply})
        return reply

    try:
        do_cmd(f"SWEEP SET START {start_hz} STOP {stop_hz} PERIOD_MS {period_ms}", 0.3)
        sweep_before = parse_sweep_status(do_cmd("SWEEP?", 0.2))
        dds_before = parse_dds_status(do_cmd("DDS?", 0.2))
        do_cmd("SWEEP ON", 0.35)
        sweep_on = parse_sweep_status(do_cmd("SWEEP?", 0.25))

        start_t = time.time()
        duration_s = (period_ms / 1000.0) + 0.8
        dds_samples: list[dict] = []
        scope_samples: list[SweepMeasurement] = []
        next_scope_at = 0.0
        while (time.time() - start_t) < duration_s:
            t_now = time.time() - start_t
            dds = parse_dds_status(do_cmd("DDS?", 0.10))
            if 1 in dds:
                dds_samples.append({
                    "t_s": t_now,
                    "freq_hz": dds[1]["freq_hz"],
                    "amp_code": dds[1]["amp_code"],
                })
            if t_now >= next_scope_at:
                freq_hz = tek_measure(scope, "FREQuency")
                rms_v = tek_measure(scope, "CRMs")
                vpp_v = tek_measure(scope, "PK2Pk")
                scope_samples.append(SweepMeasurement(t_now, freq_hz, rms_v, vpp_v))
                next_scope_at += max(0.18, period_ms / 1000.0 / 6.0)
            time.sleep(0.05)

        do_cmd("SWEEP OFF", 0.2)
        sweep_after = parse_sweep_status(do_cmd("SWEEP?", 0.2))
    finally:
        cli.close()

    freq_values = [row["freq_hz"] for row in dds_samples]
    unique_freqs = sorted(set(freq_values))
    diffs = [b - a for a, b in zip(unique_freqs, unique_freqs[1:])]
    observed_step = int(round(sum(diffs) / len(diffs))) if diffs else 0
    grid = list(range(start_hz, stop_hz + 1, 100_000))
    freq_errors = []
    for s in scope_samples:
        if math.isnan(s.freq_hz):
            continue
        nearest = min(grid, key=lambda g: abs(g - s.freq_hz))
        freq_errors.append({
            "t_s": s.t_s,
            "measured_hz": s.freq_hz,
            "nearest_grid_hz": nearest,
            "error_hz": s.freq_hz - nearest,
        })

    rms_values = [s.rms_v for s in scope_samples if not math.isnan(s.rms_v)]
    vpp_values = [s.vpp_v for s in scope_samples if not math.isnan(s.vpp_v)]
    rms_cv = float((max(rms_values) - min(rms_values)) / max(sum(rms_values) / len(rms_values), 1e-9)) if len(rms_values) >= 2 else 0.0
    vpp_cv = float((max(vpp_values) - min(vpp_values)) / max(sum(vpp_values) / len(vpp_values), 1e-9)) if len(vpp_values) >= 2 else 0.0

    verdict = "PASS"
    notes: list[str] = []
    if sweep_on.get("mode") != "CW" or int(sweep_on.get("rel1", 1)) != 0:
        verdict = "FAIL"
        notes.append("Sweep ON did not force CW mode with REL1 low")
    if unique_freqs and (unique_freqs[0] != start_hz or unique_freqs[-1] != stop_hz):
        verdict = "FAIL"
        notes.append(f"Sweep range mismatch: observed {unique_freqs[0]}..{unique_freqs[-1]}")
    if diffs and any(d != 100_000 for d in diffs):
        verdict = "FAIL"
        notes.append(f"Sweep step mismatch: diffs={diffs}")
    if freq_errors and max(abs(x["error_hz"]) for x in freq_errors) > 60_000:
        verdict = "FAIL"
        notes.append("CH3 frequency deviated too far from expected sweep grid")
    if rms_cv > 0.25:
        verdict = "FAIL"
        notes.append(f"CH3 RMS variation too large: {rms_cv:.3f}")
    if verdict == "PASS":
        notes.append("CW sweep serial control, RF frequency grid and RMS stability are within first-round tolerance")

    return {
        "verdict": verdict,
        "notes": notes,
        "start_hz": start_hz,
        "stop_hz": stop_hz,
        "period_ms": period_ms,
        "serial_log": serial_log,
        "sweep_before": sweep_before,
        "sweep_on": sweep_on,
        "sweep_after": sweep_after,
        "dds_before": dds_before,
        "dds_samples": dds_samples,
        "scope_samples": [asdict(s) for s in scope_samples],
        "unique_freqs": unique_freqs,
        "observed_step_hz": observed_step,
        "freq_errors": freq_errors,
        "rms_span_ratio": rms_cv,
        "vpp_span_ratio": vpp_cv,
    }


def write_summary(out_dir: str, overall: dict) -> None:
    with open(os.path.join(out_dir, "summary.md"), "w", encoding="utf-8") as f:
        f.write("# 发射机串口联调最小验证\n\n")
        f.write("## 总结\n\n")
        for key in ("cw_sweep", "am", "fm", "ask", "fsk", "psk"):
            item = overall[key]
            if key == "cw_sweep":
                f.write(f"- `{key}`: `{item['verdict']}`\n")
            else:
                f.write(f"- `{key}`: `{item['result']['pass_fail']}`\n")
        f.write("\n## CW Sweep\n\n")
        sweep = overall["cw_sweep"]
        f.write(f"- Verdict: `{sweep['verdict']}`\n")
        f.write(f"- Range: `{sweep['start_hz']} .. {sweep['stop_hz']}`\n")
        f.write(f"- Period: `{sweep['period_ms']} ms`\n")
        f.write(f"- Observed step: `{sweep['observed_step_hz']} Hz`\n")
        f.write(f"- RMS span ratio: `{sweep['rms_span_ratio']:.4f}`\n")
        f.write(f"- Vpp span ratio: `{sweep['vpp_span_ratio']:.4f}`\n")
        if sweep["notes"]:
            f.write("\n### Notes\n\n")
            for note in sweep["notes"]:
                f.write(f"- {note}\n")
        f.write("\n## 单点调制\n\n")
        for key in ("am", "fm", "ask", "fsk", "psk"):
            result = overall[key]["result"]
            f.write(f"### {key.upper()}\n\n")
            f.write(f"- Verdict: `{result['pass_fail']}`\n")
            f.write(f"- Carrier: `{result['carrier_hz']:.3f} Hz`\n")
            f.write(f"- {result['metric_1_name']}: `{result['metric_1_value']}`\n")
            f.write(f"- {result['metric_2_name']}: `{result['metric_2_value']}`\n")
            for note in result["notes"]:
                f.write(f"- {note}\n")
            f.write(f"- Result dir: `{overall[key]['result_dir']}`\n\n")


def main() -> None:
    p = argparse.ArgumentParser(description="Minimal transmitter serial joint-debug runner.")
    p.add_argument("--port", default=DEFAULT_H743_PORT)
    p.add_argument("--baudrate", type=int, default=DEFAULT_BAUDRATE)
    p.add_argument("--char-delay-ms", type=float, default=15.0)
    p.add_argument("--scope", default=DEFAULT_SCOPE_IP)
    p.add_argument("--use-50r", action="store_true")
    p.add_argument("--analyzer", default=DEFAULT_ANALYZER_PATH)
    p.add_argument("--analyzer-python", default=DEFAULT_ANALYZER_PYTHON)
    p.add_argument("--out", default=DEFAULT_OUT)
    args = p.parse_args()

    out_root = os.path.abspath(args.out)
    ensure_dir(out_root)
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    out_dir = os.path.join(out_root, f"joint_debug_{stamp}")
    ensure_dir(out_dir)

    sweep = run_cw_sweep(args.port,
                         args.baudrate,
                         args.char_delay_ms,
                         args.scope,
                         args.use_50r,
                         120_000_000,
                         122_000_000,
                         2000)

    mode_out_root = os.path.join(out_dir, "modes")
    ensure_dir(mode_out_root)
    mode_specs = {
        "am": {
            "lines": ["TXAM 130000000 512 980 0 -50 100 10000 8192 1200 500"],
            "args": ["--points", "300000", "--mod-freq-hz", "10000", "--am-depth-pct", "50"],
        },
        "fm": {
            "lines": ["TXFM 130000000 512 980 0 -50 100 10000 8192 1200 75000"],
            "args": ["--points", "300000", "--mod-freq-hz", "10000", "--fm-dev-hz", "75000"],
        },
        "ask": {
            "lines": ["TXASK 130000000 512 980 0 -50 100 10000 8192 1200 1000"],
            "args": ["--points", "300000", "--bit-rate-hz", "10000"],
        },
        "fsk": {
            "lines": ["TXFSK 130000000 512 980 0 -50 100 10000 8192 1200 20000"],
            "args": ["--points", "300000", "--bit-rate-hz", "10000", "--fsk-shift-hz", "20000"],
        },
        "psk": {
            "lines": ["TXPSK 130000000 512 980 0 -50 100 10000 8192 1200"],
            "args": ["--points", "300000", "--bit-rate-hz", "10000"],
        },
    }

    overall: dict[str, dict] = {"cw_sweep": sweep}
    for mode, spec in mode_specs.items():
        result, result_dir = run_single_mode(args.analyzer,
                                             args.analyzer_python,
                                             mode.upper(),
                                             mode_out_root,
                                             args.port,
                                             args.scope,
                                             spec["lines"],
                                             spec["args"])
        overall[mode] = {
            "result": result["result"],
            "result_dir": result_dir,
        }

    with open(os.path.join(out_dir, "result.json"), "w", encoding="utf-8") as f:
        json.dump(overall, f, indent=2, ensure_ascii=False)
    write_summary(out_dir, overall)

    print(f"RESULT_DIR={out_dir}")
    print(f"CW_SWEEP={sweep['verdict']}")
    for key in ("am", "fm", "ask", "fsk", "psk"):
        print(f"{key.upper()}={overall[key]['result']['pass_fail']}")


if __name__ == "__main__":
    main()
