# TX Serial Joint Debug 2026-05-24

## Scope

First-round conservative transmitter joint debug on:

- H743 project: `F:\CAMKE\backups\restore_h743_20260523_061047\Simple_wireless_automatic_transceiver_deviceV2.0`
- Main observation channel: Tek `CH3`
- Current priority: frequency, sweep step, spectral correspondence
- Not calibrating amplitude in this round

Implemented in this round:

- Added H743 CLI sweep commands:
  - `SWEEP SET START <HZ> STOP <HZ> PERIOD_MS <MS>`
  - `SWEEP ON`
  - `SWEEP OFF`
  - `SWEEP?`
- Kept sweep logic inside existing `App_TxControl_*`
- Ensured `PB0 / REL1` behavior:
  - `CW -> low`
  - non-CW (`AM/FM/ASK/FSK/PSK`) -> high
- Added test script:
  - `F:\CAMKE\backups\restore_h743_20260523_061047\Simple_wireless_automatic_transceiver_deviceV2.0\tools\tx_serial_joint_debug.py`

## Result Summary

Result directory:

- `F:\CAMKE\backups\restore_h743_20260523_061047\Simple_wireless_automatic_transceiver_deviceV2.0\tools\tx_joint_debug_results\joint_debug_20260524_005630`

First-round verdicts:

- `CW sweep`: `FAIL`
- `AM`: `FAIL`
- `FM`: `FAIL`
- `ASK`: `FAIL`
- `FSK`: `PASS`
- `PSK`: `FAIL`

## Key Findings

### 1. H743 serial command path is working

The new sweep CLI commands responded correctly:

- `SWEEP SET ...` -> `OK SWEEP SET`
- `SWEEP ON` -> `OK SWEEP ON`
- `SWEEP OFF` -> `OK SWEEP OFF`
- `SWEEP?` returned mode/range/period/current/REL1 status

`TXAM/TXFM/TXASK/TXFSK/TXPSK` command chain still works.

### 2. PB0 / REL1 switching path is now connected

This round explicitly tied relay state to the serial mode path:

- `SWEEP ON` forces `CW`, so `REL1` goes low
- `TXAM/TXFM/TXASK/TXFSK/TXPSK` explicitly switch to non-CW mode first, so `REL1` goes high

Observed from `SWEEP?`:

- before `SWEEP ON`: `REL1 1`
- after `SWEEP ON`: `REL1 0`

### 3. CW sweep did not validate cleanly

Observed in first-round result:

- requested range: `120 MHz .. 122 MHz`
- requested period: `2000 ms`
- sparse `DDS?` polling only caught:
  - `120.6 MHz`
  - `120.9 MHz`
- observed step in this run looked like `300 kHz`

This does **not** prove the firmware sweep step is truly `300 kHz`.
It means the current measurement method is too sparse for a `2 s` sweep over `100 kHz` steps.

At the same time, the `CH3` quick measurement path returned `37.5 MHz`, which is inconsistent with the expected CW RF output.
So the current sweep verdict is dominated by **measurement strategy**, not by a proven sweep logic bug.

### 4. AM / FM / ASK / PSK failed in this run because CH3 capture was inconsistent

Examples from this run:

- `AM` carrier estimated around `29.99 MHz`
- `FM` carrier estimated around `50 MHz`
- `ASK` carrier estimated around `1 kHz`
- `PSK` carrier estimated around `50 MHz`

Those are not physically consistent with the intended `130 MHz` LO chain and do not match earlier validated spot tests.

That means this first-round failure is not enough to conclude the transmit functions regressed.
More likely causes in this run:

- `CH3` measurement setup was not stable for all mode transitions
- quick-capture settings used for batch automation were not robust enough
- command-to-measurement settling time was too short for some modes

### 5. FSK passed only under the current mixed判定 strategy

Current FSK result in this round:

- `bit rate ≈ 10.5 kbps`
- `shift ≈ 20 kHz`
- marked `PASS`

But this pass still depends on the existing mixed FSK method:

- `CH1/CH2` for rate tendency
- `CH3` RF peak spacing for shift

So this is a usable baseline, not final sign-off quality.

## Current Assessment

What is solid after this round:

- H743 serial sweep CLI exists and runs
- relay switching path is integrated with serial mode flow
- serial modulation command chain remains alive
- first automation scaffold is usable

What is **not** solid yet:

- `CH3` batch measurement consistency across all modes
- `CW` sweep RF verification under the current `2 s` / sparse sampling method
- one-shot batch sign-off for `AM/FM/ASK/PSK`

## Recommended Next Step

Do **not** widen scope yet.

Recommended next step is a second conservative pass with only two changes:

1. Make CW sweep measurement more robust:
   - use a longer sweep period for validation, or
   - poll `DDS?` more densely, or
   - measure fewer but better-aligned RF points

2. Re-run single-point mode validation with stronger `CH3` settling/measurement settings:
   - keep the same serial commands
   - do not change amplitude logic yet
   - first recover stable `AM/FM/ASK/PSK` carrier reads on `CH3`

Only after that should the next iteration decide whether transmitter functionality itself is wrong, or whether the current failure is mostly in the automated measurement path.
