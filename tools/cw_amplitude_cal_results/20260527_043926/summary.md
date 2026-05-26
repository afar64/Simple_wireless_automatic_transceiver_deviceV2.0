# CW Amplitude Calibration Summary

## Setup

- Scope: `192.168.2.28`
- Serial: `COM6 @ 115200`
- DDS channel: `CH1`
- Scope channel: `CH2`, `50 ohm`, `CRMs`
- Frequency range: `110000000..130000000 Hz`, step `10000 Hz`
- Targets: `10.0,50.0,100.0 mVrms`
- Tolerance: `+/-1.0 mVrms`
- Adjustment priority: `PE4302 attenuation first`, AD9959 amplitude code only at limits or quantization stalls

## Result

- Measured points this run: `9`
- Skipped existing points: `0`
- PASS: `8`
- FAIL: `1`

## Calibration Table

| Freq Hz | Target mVrms | Measured mVrms | Error mVrms | AD9959 code | Atten dB | Iters | Result |
|---:|---:|---:|---:|---:|---:|---:|---|
| 110000000 | 10.000 | 9.664 | -0.336 | 1023 | 30.0 | 4 | PASS |
| 110000000 | 50.000 | 49.465 | -0.535 | 1023 | 15.5 | 3 | PASS |
| 110000000 | 100.000 | 100.081 | 0.081 | 996 | 9.5 | 6 | PASS |
| 120000000 | 10.000 | 9.347 | -0.653 | 1023 | 31.5 | 3 | PASS |
| 120000000 | 50.000 | 51.438 | 1.438 | 1023 | 17.5 | 8 | FAIL |
| 120000000 | 100.000 | 99.707 | -0.293 | 1019 | 11.5 | 3 | PASS |
| 130000000 | 10.000 | 9.115 | -0.885 | 1023 | 30.5 | 2 | PASS |
| 130000000 | 50.000 | 50.239 | 0.239 | 1023 | 17.0 | 3 | PASS |
| 130000000 | 100.000 | 99.018 | -0.982 | 992 | 10.5 | 3 | PASS |

## Failed Points

- `120000000 Hz`, target `50.0 mVrms`: measured `51.438 mVrms`, code `1023`, atten `17.5 dB`, reason `max iterations`
