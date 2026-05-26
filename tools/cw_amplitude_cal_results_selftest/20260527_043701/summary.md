# CW Amplitude Calibration Summary

## Setup

- Scope: `192.168.2.28`
- Serial: `COM6 @ 115200`
- DDS channel: `CH1`
- Scope channel: `CH2`, `50 ohm`, `CRMs`
- Frequency range: `110000000..130000000 Hz`, step `10000 Hz`
- Targets: `10.0,20.0,30.0,40.0,50.0,60.0,70.0,80.0,90.0,100.0 mVrms`
- Tolerance: `+/-1.0 mVrms`
- Adjustment priority: `PE4302 attenuation first`, AD9959 amplitude code only at limits or quantization stalls

## Result

- Measured points this run: `0`
- Skipped existing points: `0`
- PASS: `0`
- FAIL: `0`

## Calibration Table

| Freq Hz | Target mVrms | Measured mVrms | Error mVrms | AD9959 code | Atten dB | Iters | Result |
|---:|---:|---:|---:|---:|---:|---:|---|
