import argparse
import csv
import json
import math
import statistics
import time
from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path

import serial
import vxi11


DEFAULT_CONTROL_PORT = "COM6"
DEFAULT_STABILITY_PORT = "COM9"
DEFAULT_BAUDRATE = 115200
DEFAULT_SCOPE_IP = "192.168.2.28"
DEFAULT_OUT_ROOT = "tools/mod_amplitude_cal_results"

FREQ_START_HZ = 110_000_000
FREQ_STOP_HZ = 130_000_000
FREQ_STEP_HZ = 100_000
DEFAULT_MODES = ("AM", "FM", "2ASK", "2FSK", "2PSK")

TARGET_MVRMS = 100.0
AMP_MIN = 1
AMP_MAX = 1023
ATTEN_MIN_DB = 0.0
ATTEN_MAX_DB = 31.5
ATTEN_STEP_DB = 0.5

QG_DEFAULT = 980
QP_DEFAULT = 0
IO_DEFAULT = -50
QO_DEFAULT = 100
LO_AMP_DEFAULT = 512
OFFSET_DEFAULT = 8192
F429_AMP_DEFAULT = 1200
MOD_FREQ_DEFAULT_HZ = 10_000
SYMBOL_RATE_DEFAULT_BPS = 10_000
AM_DEPTH_PERMILLE = 500
ASK_DEPTH_PERMILLE = 1000
FM_DEV_DEFAULT_HZ = 75_000
FSK_SHIFT_DEFAULT_HZ = 20_000


@dataclass
class SerialCommand:
    port: str
    command: str
    reply: str


@dataclass
class StabilityWait:
    port: str
    keyword: str
    matched: bool
    elapsed_s: float
    lines: list[str]


@dataclass
class IterationRecord:
    iteration: int
    mode: str
    freq_hz: int
    target_mvrms: float
    measured_mvrms: float
    error_mvrms: float
    amp_code_ch0: int
    atten_db: float
    action: str


@dataclass
class CalibrationPoint:
    mode: str
    freq_hz: int
    target_mvrms: float
    measured_mvrms: float
    error_mvrms: float
    amp_code_ch0: int
    atten_db: float
    iterations: int
    pass_fail: str
    reason: str
    scope_raw_samples_mvrms: list[float]
    iteration_log: list[IterationRecord]
    com6_serial_log: list[SerialCommand]
    com9_stability_log: list[StabilityWait]


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


def round_atten_db(value: float) -> float:
    steps = round(value / ATTEN_STEP_DB)
    return clamp(steps * ATTEN_STEP_DB, ATTEN_MIN_DB, ATTEN_MAX_DB)


def parse_modes(text: str) -> list[str]:
    modes: list[str] = []
    valid = set(DEFAULT_MODES)
    for part in text.split(","):
        mode = part.strip().upper()
        if not mode:
            continue
        if mode not in valid:
            raise ValueError(f"unsupported mode {mode!r}; valid={','.join(DEFAULT_MODES)}")
        modes.append(mode)
    if not modes:
        raise ValueError("mode list is empty")
    return modes


def build_freqs(start_hz: int, stop_hz: int, step_hz: int) -> list[int]:
    if step_hz <= 0:
        raise ValueError("freq step must be positive")
    if start_hz > stop_hz:
        raise ValueError("freq start must be <= freq stop")
    return list(range(start_hz, stop_hz + 1, step_hz))


def select_pilot_freqs(freqs: list[int]) -> list[int]:
    if not freqs:
        return []
    candidates = [freqs[0], freqs[len(freqs) // 2], freqs[-1]]
    out: list[int] = []
    for freq_hz in candidates:
        if freq_hz not in out:
            out.append(freq_hz)
    return out


def read_existing_keys(csv_path: Path) -> set[tuple[str, int]]:
    keys: set[tuple[str, int]] = set()
    if not csv_path.exists():
        return keys
    with csv_path.open("r", encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f):
            try:
                keys.add((row["mode"].upper(), int(row["freq_hz"])))
            except (KeyError, ValueError):
                continue
    return keys


class SerialCli:
    def __init__(self, port: str, baudrate: int, char_delay_ms: float) -> None:
        self.port = port
        self._ser = serial.Serial(port, baudrate, timeout=0.05)
        self._char_delay_s = char_delay_ms / 1000.0
        self._ser.reset_input_buffer()
        self._ser.reset_output_buffer()

    def close(self) -> None:
        self._ser.close()

    def send_line(self, line: str, expect: str | None = None, timeout_s: float = 3.0) -> str:
        self._ser.reset_input_buffer()
        for ch in (line + "\r\n"):
            self._ser.write(ch.encode("ascii"))
            self._ser.flush()
            if self._char_delay_s > 0:
                time.sleep(self._char_delay_s)

        deadline = time.time() + timeout_s
        chunks: list[str] = []
        while time.time() < deadline:
            data = self._ser.read(4096)
            if data:
                text = data.decode("ascii", errors="replace")
                chunks.append(text)
                reply = "".join(chunks)
                if expect is not None and expect in reply:
                    return reply
                if expect is None and ("\n" in reply or "\r" in reply):
                    return reply
            else:
                time.sleep(0.02)

        reply = "".join(chunks)
        if expect is not None and expect not in reply:
            raise RuntimeError(f"timeout waiting for {expect!r} after {line!r}; reply={reply!r}")
        return reply


class StabilitySerial:
    def __init__(self, port: str, baudrate: int) -> None:
        self.port = port
        self._ser = serial.Serial(port, baudrate, timeout=0.05)
        self.clear()

    def close(self) -> None:
        self._ser.close()

    def clear(self) -> None:
        self._ser.reset_input_buffer()

    def read_lines_for(self, duration_s: float) -> list[str]:
        deadline = time.time() + duration_s
        raw = ""
        while time.time() < deadline:
            data = self._ser.read(4096)
            if data:
                raw += data.decode("ascii", errors="replace")
            else:
                time.sleep(0.02)
        return [line.strip() for line in raw.replace("\r", "\n").split("\n") if line.strip()]

    def wait_for_keyword(self, keyword: str, timeout_s: float) -> StabilityWait:
        start = time.time()
        deadline = start + timeout_s
        key = keyword.upper()
        raw = ""
        lines: list[str] = []
        matched = False
        while time.time() < deadline:
            data = self._ser.read(4096)
            if data:
                raw += data.decode("ascii", errors="replace")
                split = raw.replace("\r", "\n").split("\n")
                raw = split[-1]
                for line in split[:-1]:
                    line = line.strip()
                    if not line:
                        continue
                    lines.append(line)
                    if key in line.upper():
                        matched = True
                        return StabilityWait(self.port, keyword, matched, time.time() - start, lines)
            else:
                time.sleep(0.02)
        if raw.strip():
            lines.append(raw.strip())
            if key in raw.upper():
                matched = True
        return StabilityWait(self.port, keyword, matched, time.time() - start, lines)


class TekScope:
    def __init__(self, ip: str, timeout_s: float) -> None:
        self._inst = vxi11.Instrument(ip)
        self._inst.timeout = int(math.ceil(timeout_s))

    def ask(self, command: str) -> str:
        return self._inst.ask(command).strip()

    def write(self, command: str) -> None:
        self._inst.write(command)

    def configure_ch2_rms(self, use_50r: bool = True) -> None:
        self.write("SELECT:CH2 ON")
        self.write("CH2:COUPLING DC")
        self.write("CH2:SCALE 0.05")
        self.write("CH2:POSITION 0")
        if use_50r:
            self.write("CH2:TERMINATION 50")
        self.write("TRIGGER:A:MODE AUTO")
        self.write("ACQUIRE:MODE SAMPLE")
        self.write("MEASUrement:IMMed:SOUrce1 CH2")
        self.write("MEASUrement:IMMed:TYPe CRMs")
        time.sleep(0.2)

    def read_ch2_rms_v(self, samples: int, sample_delay_s: float) -> tuple[float, list[float]]:
        values: list[float] = []
        for _ in range(max(1, samples)):
            self.write("MEASUrement:IMMed:SOUrce1 CH2")
            self.write("MEASUrement:IMMed:TYPe CRMs")
            raw = self.ask("MEASUrement:IMMed:VALue?")
            value = float(raw)
            if not math.isfinite(value) or abs(value) > 1e30:
                value = float("nan")
            values.append(value)
            time.sleep(sample_delay_s)
        finite = [v for v in values if math.isfinite(v)]
        if not finite:
            return float("nan"), values
        return statistics.median(finite), values


def mode_command(mode: str, freq_hz: int) -> tuple[str, str]:
    common = f"{freq_hz} {LO_AMP_DEFAULT} {QG_DEFAULT} {QP_DEFAULT} {IO_DEFAULT} {QO_DEFAULT}"
    if mode == "AM":
        return (
            f"TXAM {common} {MOD_FREQ_DEFAULT_HZ} {OFFSET_DEFAULT} {F429_AMP_DEFAULT} {AM_DEPTH_PERMILLE}",
            "OK TXAM",
        )
    if mode == "FM":
        return (
            f"TXFM {common} {MOD_FREQ_DEFAULT_HZ} {OFFSET_DEFAULT} {F429_AMP_DEFAULT} {FM_DEV_DEFAULT_HZ}",
            "OK TXFM",
        )
    if mode == "2ASK":
        return (
            f"TXASK {common} {SYMBOL_RATE_DEFAULT_BPS} {OFFSET_DEFAULT} {F429_AMP_DEFAULT} {ASK_DEPTH_PERMILLE}",
            "OK TXASK",
        )
    if mode == "2FSK":
        return (
            f"TXFSK {common} {SYMBOL_RATE_DEFAULT_BPS} {OFFSET_DEFAULT} {F429_AMP_DEFAULT} {FSK_SHIFT_DEFAULT_HZ}",
            "OK TXFSK",
        )
    if mode == "2PSK":
        return (
            f"TXPSK {common} {SYMBOL_RATE_DEFAULT_BPS} {OFFSET_DEFAULT} {F429_AMP_DEFAULT}",
            "OK TXPSK",
        )
    raise ValueError(f"unsupported mode {mode}")


def command_set_atten(cli: SerialCli, atten_db: float, serial_log: list[SerialCommand]) -> None:
    rounded = round_atten_db(atten_db)
    cmd = f"ATTEN {rounded:.1f}"
    reply = cli.send_line(cmd, expect="OK ATTEN")
    serial_log.append(SerialCommand(cli.port, cmd, reply))


def command_set_dds_ch0(cli: SerialCli, freq_hz: int, amp_code: int, serial_log: list[SerialCommand]) -> None:
    cmd = f"DDS CH0 FREQ {freq_hz} AMP {amp_code}"
    reply = cli.send_line(cmd, expect="OK CH0")
    serial_log.append(SerialCommand(cli.port, cmd, reply))


def command_config_f429(cli: SerialCli,
                        stability: StabilitySerial,
                        mode: str,
                        freq_hz: int,
                        stable_keyword: str,
                        stable_timeout_s: float,
                        serial_log: list[SerialCommand],
                        stability_log: list[StabilityWait]) -> bool:
    cmd, expect = mode_command(mode, freq_hz)
    stability.clear()
    reply = cli.send_line(cmd, expect=expect, timeout_s=8.0)
    serial_log.append(SerialCommand(cli.port, cmd, reply))
    wait = stability.wait_for_keyword(stable_keyword, stable_timeout_s)
    stability_log.append(wait)
    return wait.matched


def measure_mvrms(scope: TekScope, samples: int, sample_delay_s: float) -> tuple[float, list[float]]:
    rms_v, raw_v = scope.read_ch2_rms_v(samples, sample_delay_s)
    return rms_v * 1000.0, [v * 1000.0 for v in raw_v]


def pick_best(records: list[IterationRecord]) -> IterationRecord:
    finite_records = [r for r in records if math.isfinite(r.measured_mvrms)]
    if not finite_records:
        return records[-1]
    return min(finite_records, key=lambda r: abs(r.error_mvrms))


def fail_point(mode: str,
               freq_hz: int,
               target_mvrms: float,
               amp_code: int,
               atten_db: float,
               reason: str,
               serial_log: list[SerialCommand],
               stability_log: list[StabilityWait]) -> CalibrationPoint:
    return CalibrationPoint(mode,
                            freq_hz,
                            target_mvrms,
                            float("nan"),
                            float("nan"),
                            amp_code,
                            atten_db,
                            0,
                            "FAIL",
                            reason,
                            [],
                            [],
                            serial_log,
                            stability_log)


def calibrate_point(cli: SerialCli,
                    stability: StabilitySerial,
                    scope: TekScope,
                    mode: str,
                    freq_hz: int,
                    target_mvrms: float,
                    amp_code: int,
                    atten_db: float,
                    tolerance_mvrms: float,
                    max_iterations: int,
                    settle_s: float,
                    samples: int,
                    sample_delay_s: float,
                    stable_keyword: str,
                    stable_timeout_s: float) -> CalibrationPoint:
    serial_log: list[SerialCommand] = []
    stability_log: list[StabilityWait] = []
    iteration_log: list[IterationRecord] = []
    current_amp = int(clamp(amp_code, AMP_MIN, AMP_MAX))
    current_atten = round_atten_db(atten_db)
    last_state: tuple[int, float] | None = None
    last_samples: list[float] = []
    reason = "max iterations"

    try:
        if not command_config_f429(cli,
                                   stability,
                                   mode,
                                   freq_hz,
                                   stable_keyword,
                                   stable_timeout_s,
                                   serial_log,
                                   stability_log):
            return fail_point(mode,
                              freq_hz,
                              target_mvrms,
                              current_amp,
                              current_atten,
                              f"stability keyword {stable_keyword!r} timeout",
                              serial_log,
                              stability_log)
        command_set_atten(cli, current_atten, serial_log)
        command_set_dds_ch0(cli, freq_hz, current_amp, serial_log)
    except Exception as exc:
        return fail_point(mode,
                          freq_hz,
                          target_mvrms,
                          current_amp,
                          current_atten,
                          f"setup failed: {exc}",
                          serial_log,
                          stability_log)

    for iteration in range(1, max_iterations + 1):
        time.sleep(settle_s)
        measured_mvrms, raw_samples = measure_mvrms(scope, samples, sample_delay_s)
        last_samples = raw_samples
        error_mvrms = measured_mvrms - target_mvrms
        action = "measure"
        iteration_log.append(
            IterationRecord(iteration,
                            mode,
                            freq_hz,
                            target_mvrms,
                            measured_mvrms,
                            error_mvrms,
                            current_amp,
                            current_atten,
                            action)
        )

        if math.isfinite(measured_mvrms) and abs(error_mvrms) <= tolerance_mvrms:
            return CalibrationPoint(mode,
                                    freq_hz,
                                    target_mvrms,
                                    measured_mvrms,
                                    error_mvrms,
                                    current_amp,
                                    current_atten,
                                    iteration,
                                    "PASS",
                                    "within tolerance",
                                    raw_samples,
                                    iteration_log,
                                    serial_log,
                                    stability_log)

        try:
            if (not math.isfinite(measured_mvrms)) or measured_mvrms <= 0.0:
                if current_atten > ATTEN_MIN_DB:
                    current_atten = round_atten_db(current_atten - ATTEN_STEP_DB)
                    action = "decrease atten after invalid reading"
                    command_set_atten(cli, current_atten, serial_log)
                elif current_amp < AMP_MAX:
                    current_amp = min(AMP_MAX, max(current_amp + 16, int(current_amp * 1.2)))
                    action = "increase amp after invalid reading"
                    command_set_dds_ch0(cli, freq_hz, current_amp, serial_log)
                else:
                    reason = "invalid reading at minimum attenuation and max amp"
                    break
                iteration_log[-1].action = action
                continue

            desired_atten = round_atten_db(current_atten + 20.0 * math.log10(measured_mvrms / target_mvrms))
            state = (current_amp, desired_atten)
            if desired_atten != current_atten and state != last_state:
                current_atten = desired_atten
                last_state = state
                action = "adjust atten"
                command_set_atten(cli, current_atten, serial_log)
                iteration_log[-1].action = action
                continue

            ratio = target_mvrms / measured_mvrms
            next_amp = int(round(current_amp * ratio))
            next_amp = int(clamp(next_amp, AMP_MIN, AMP_MAX))
            if next_amp == current_amp:
                next_amp += 1 if error_mvrms < 0.0 else -1
                next_amp = int(clamp(next_amp, AMP_MIN, AMP_MAX))

            if next_amp != current_amp:
                current_amp = next_amp
                action = "adjust amp"
                command_set_dds_ch0(cli, freq_hz, current_amp, serial_log)
                iteration_log[-1].action = action
                last_state = None
                continue
        except Exception as exc:
            reason = f"adjustment failed: {exc}"
            iteration_log[-1].action = reason
            break

        if current_amp == AMP_MAX and error_mvrms < 0:
            reason = "under target at max amp and min attenuation"
        elif current_amp == AMP_MIN and error_mvrms > 0:
            reason = "over target at min amp and max attenuation"
        else:
            reason = "no further adjustment possible"
        iteration_log[-1].action = reason
        break

    best = pick_best(iteration_log)
    return CalibrationPoint(mode,
                            freq_hz,
                            target_mvrms,
                            best.measured_mvrms,
                            best.error_mvrms,
                            best.amp_code_ch0,
                            best.atten_db,
                            len(iteration_log),
                            "FAIL",
                            reason,
                            last_samples,
                            iteration_log,
                            serial_log,
                            stability_log)


def append_csv(csv_path: Path, point: CalibrationPoint) -> None:
    exists = csv_path.exists()
    fieldnames = [
        "mode",
        "freq_hz",
        "target_mvrms",
        "measured_mvrms",
        "error_mvrms",
        "amp_code_ch0",
        "atten_db",
        "iterations",
        "pass_fail",
        "reason",
    ]
    with csv_path.open("a", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        if not exists:
            writer.writeheader()
        row = asdict(point)
        writer.writerow({k: row[k] for k in fieldnames})


def write_json(json_path: Path, points: list[CalibrationPoint], meta: dict) -> None:
    data = {
        "meta": meta,
        "points": [asdict(point) for point in points],
    }
    with json_path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)


def write_summary(summary_path: Path, points: list[CalibrationPoint], meta: dict, skipped: int) -> None:
    total = len(points)
    passed = sum(1 for p in points if p.pass_fail == "PASS")
    failed = total - passed
    with summary_path.open("w", encoding="utf-8") as f:
        f.write("# Modulated Amplitude Calibration Summary\n\n")
        f.write("## Setup\n\n")
        f.write(f"- Scope: `{meta['scope']}`\n")
        f.write(f"- Control serial: `{meta['control_port']} @ {meta['control_baudrate']}`\n")
        f.write(f"- Stability serial: `{meta['stability_port']} @ {meta['stability_baudrate']}`\n")
        f.write(f"- Stability keyword: `{meta['stable_keyword']}`\n")
        f.write("- DDS channel: `CH0`\n")
        f.write("- Scope channel: `CH2`, `50 ohm`, `CRMs`\n")
        f.write(f"- Frequency range: `{meta['freq_start_hz']}..{meta['freq_stop_hz']} Hz`, step `{meta['freq_step_hz']} Hz`\n")
        f.write(f"- Modes: `{','.join(meta['modes'])}`\n")
        f.write(f"- Target: `{meta['target_mvrms']} mVrms`\n")
        f.write(f"- Tolerance: `+/-{meta['tolerance_mvrms']} mVrms`\n")
        f.write("- Adjustment priority: `PE4302 attenuation first`, AD9959 CH0 amplitude code second\n\n")
        f.write("## Result\n\n")
        f.write(f"- Measured points this run: `{total}`\n")
        f.write(f"- Skipped existing points: `{skipped}`\n")
        f.write(f"- PASS: `{passed}`\n")
        f.write(f"- FAIL: `{failed}`\n\n")
        f.write("## Calibration Table\n\n")
        f.write("| Mode | Freq Hz | Target mVrms | Measured mVrms | Error mVrms | CH0 code | Atten dB | Iters | Result |\n")
        f.write("|---|---:|---:|---:|---:|---:|---:|---:|---|\n")
        for p in points:
            f.write(
                f"| {p.mode} | {p.freq_hz} | {p.target_mvrms:.3f} | {p.measured_mvrms:.3f} | "
                f"{p.error_mvrms:.3f} | {p.amp_code_ch0} | {p.atten_db:.1f} | "
                f"{p.iterations} | {p.pass_fail} |\n"
            )
        if failed:
            f.write("\n## Failed Points\n\n")
            for p in points:
                if p.pass_fail != "PASS":
                    f.write(
                        f"- `{p.mode}` `{p.freq_hz} Hz`: measured `{p.measured_mvrms:.3f} mVrms`, "
                        f"code `{p.amp_code_ch0}`, atten `{p.atten_db:.1f} dB`, reason `{p.reason}`\n"
                    )


def self_test(cli: SerialCli,
              stability: StabilitySerial,
              scope: TekScope,
              stable_probe_s: float,
              samples: int,
              sample_delay_s: float) -> dict:
    serial_log: list[SerialCommand] = []
    dds_reply = cli.send_line("DDS?", expect="DDS CH0")
    serial_log.append(SerialCommand(cli.port, "DDS?", dds_reply))
    atten_reply = cli.send_line("ATTEN?", expect="ATTEN")
    serial_log.append(SerialCommand(cli.port, "ATTEN?", atten_reply))
    stability_lines = stability.read_lines_for(stable_probe_s)
    idn = scope.ask("*IDN?")
    rms_mvrms, raw_samples = measure_mvrms(scope, samples, sample_delay_s)
    return {
        "dds_reply": dds_reply,
        "atten_reply": atten_reply,
        "stability_probe_lines": stability_lines,
        "scope_idn": idn,
        "ch2_rms_mvrms": rms_mvrms,
        "ch2_raw_samples_mvrms": raw_samples,
        "serial_log": [asdict(x) for x in serial_log],
    }


def find_latest_result_dir(out_root: Path) -> Path | None:
    if not out_root.exists():
        return None
    candidates = [
        path for path in out_root.iterdir()
        if path.is_dir() and (path / "calibration_table.csv").exists()
    ]
    if not candidates:
        return None
    return sorted(candidates, key=lambda p: p.name)[-1]


def main() -> None:
    parser = argparse.ArgumentParser(description="Closed-loop AM/FM/ASK/FSK/PSK amplitude calibration.")
    parser.add_argument("--port", default=DEFAULT_CONTROL_PORT, help="H743 USART1 CLI port.")
    parser.add_argument("--baudrate", type=int, default=DEFAULT_BAUDRATE)
    parser.add_argument("--stable-port", default=DEFAULT_STABILITY_PORT, help="Port that emits STABLE when F429 settles.")
    parser.add_argument("--stable-baudrate", type=int, default=DEFAULT_BAUDRATE)
    parser.add_argument("--stable-keyword", default="STABLE")
    parser.add_argument("--stable-timeout-s", type=float, default=10.0)
    parser.add_argument("--scope", default=DEFAULT_SCOPE_IP)
    parser.add_argument("--freq-start", type=int, default=FREQ_START_HZ)
    parser.add_argument("--freq-stop", type=int, default=FREQ_STOP_HZ)
    parser.add_argument("--freq-step", type=int, default=FREQ_STEP_HZ)
    parser.add_argument("--modes", default=",".join(DEFAULT_MODES))
    parser.add_argument("--target-mv", type=float, default=TARGET_MVRMS)
    parser.add_argument("--tolerance-mv", type=float, default=2.0)
    parser.add_argument("--settle-ms", type=float, default=200.0)
    parser.add_argument("--samples", type=int, default=3)
    parser.add_argument("--sample-delay-ms", type=float, default=60.0)
    parser.add_argument("--max-iterations", type=int, default=8)
    parser.add_argument("--initial-amp", type=int, default=1023)
    parser.add_argument("--initial-atten", type=float, default=0.0)
    parser.add_argument("--char-delay-ms", type=float, default=5.0)
    parser.add_argument("--out", default=DEFAULT_OUT_ROOT)
    parser.add_argument("--run-dir", default=None)
    parser.add_argument("--pilot", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--resume", action="store_true")
    args = parser.parse_args()

    modes = parse_modes(args.modes)
    freqs = build_freqs(args.freq_start, args.freq_stop, args.freq_step)
    if args.pilot:
        freqs = select_pilot_freqs(freqs)

    out_root = Path(args.out)
    if args.run_dir is not None:
        out_dir = Path(args.run_dir)
        stamp = out_dir.name
    elif args.resume:
        latest = find_latest_result_dir(out_root)
        if latest is not None:
            out_dir = latest
            stamp = out_dir.name
        else:
            stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            out_dir = out_root / stamp
    else:
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        out_dir = out_root / stamp
    ensure_dir(out_dir)
    csv_path = out_dir / "calibration_table.csv"
    json_path = out_dir / "result.json"
    summary_path = out_dir / "summary.md"

    meta = {
        "created_at": stamp,
        "control_port": args.port,
        "control_baudrate": args.baudrate,
        "stability_port": args.stable_port,
        "stability_baudrate": args.stable_baudrate,
        "stable_keyword": args.stable_keyword,
        "stable_timeout_s": args.stable_timeout_s,
        "scope": args.scope,
        "freq_start_hz": args.freq_start,
        "freq_stop_hz": args.freq_stop,
        "freq_step_hz": args.freq_step,
        "modes": modes,
        "target_mvrms": args.target_mv,
        "tolerance_mvrms": args.tolerance_mv,
        "settle_ms": args.settle_ms,
        "samples": args.samples,
        "sample_delay_ms": args.sample_delay_ms,
        "max_iterations": args.max_iterations,
        "initial_amp": args.initial_amp,
        "initial_atten": args.initial_atten,
        "pilot": args.pilot,
        "defaults": {
            "qg": QG_DEFAULT,
            "qp": QP_DEFAULT,
            "io": IO_DEFAULT,
            "qo": QO_DEFAULT,
            "lo_amp": LO_AMP_DEFAULT,
            "offset": OFFSET_DEFAULT,
            "f429_amp": F429_AMP_DEFAULT,
            "mod_freq_hz": MOD_FREQ_DEFAULT_HZ,
            "symbol_rate_bps": SYMBOL_RATE_DEFAULT_BPS,
            "am_depth_permille": AM_DEPTH_PERMILLE,
            "ask_depth_permille": ASK_DEPTH_PERMILLE,
            "fm_dev_hz": FM_DEV_DEFAULT_HZ,
            "fsk_shift_hz": FSK_SHIFT_DEFAULT_HZ,
        },
    }

    points: list[CalibrationPoint] = []
    skipped = 0
    existing = read_existing_keys(csv_path) if args.resume else set()
    cli = SerialCli(args.port, args.baudrate, args.char_delay_ms)
    stability = StabilitySerial(args.stable_port, args.stable_baudrate)
    scope = TekScope(args.scope, timeout_s=10)
    try:
        scope.configure_ch2_rms(use_50r=True)
        test_result = self_test(cli,
                                stability,
                                scope,
                                stable_probe_s=1.0,
                                samples=args.samples,
                                sample_delay_s=args.sample_delay_ms / 1000.0)
        meta["self_test"] = test_result
        if args.self_test:
            write_json(json_path, points, meta)
            write_summary(summary_path, points, meta, skipped)
            print(f"RESULT_DIR={out_dir}")
            print(json.dumps(test_result, indent=2, ensure_ascii=False))
            return

        last_by_mode: dict[str, tuple[int, float]] = {
            mode: (int(clamp(args.initial_amp, AMP_MIN, AMP_MAX)), round_atten_db(args.initial_atten))
            for mode in modes
        }
        for mode in modes:
            for freq_hz in freqs:
                if (mode, freq_hz) in existing:
                    skipped += 1
                    continue
                start_amp, start_atten = last_by_mode[mode]
                point = calibrate_point(cli,
                                        stability,
                                        scope,
                                        mode,
                                        freq_hz,
                                        args.target_mv,
                                        start_amp,
                                        start_atten,
                                        args.tolerance_mv,
                                        args.max_iterations,
                                        args.settle_ms / 1000.0,
                                        args.samples,
                                        args.sample_delay_ms / 1000.0,
                                        args.stable_keyword,
                                        args.stable_timeout_s)
                points.append(point)
                append_csv(csv_path, point)
                if point.pass_fail == "PASS":
                    last_by_mode[mode] = (point.amp_code_ch0, point.atten_db)
                write_json(json_path, points, meta)
                write_summary(summary_path, points, meta, skipped)
                print(
                    f"{point.pass_fail} mode={point.mode} freq={point.freq_hz} "
                    f"target={point.target_mvrms:.1f}mV meas={point.measured_mvrms:.3f}mV "
                    f"amp={point.amp_code_ch0} atten={point.atten_db:.1f}dB"
                )
    finally:
        cli.close()
        stability.close()

    write_json(json_path, points, meta)
    write_summary(summary_path, points, meta, skipped)
    print(f"RESULT_DIR={out_dir}")
    print(f"POINTS={len(points)} SKIPPED={skipped}")


if __name__ == "__main__":
    main()
