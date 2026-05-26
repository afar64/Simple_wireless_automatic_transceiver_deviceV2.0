# Modulated Amplitude Calibration Summary

## Setup

- Scope: `192.168.2.28`
- Control serial: `COM6 @ 115200`
- Stability serial: `COM9 @ 115200`
- Stability keyword: `STABLE`
- DDS channel: `CH0`
- Scope channel: `CH2`, `50 ohm`, `CRMs`
- Frequency range: `110000000..130000000 Hz`, step `100000 Hz`
- Modes: `AM,FM,2ASK,2FSK,2PSK`
- Target: `100.0 mVrms`
- Tolerance: `+/-2.0 mVrms`
- Adjustment priority: `PE4302 attenuation first`, AD9959 CH0 amplitude code second

## Result

- Measured points this run: `12`
- Skipped existing points: `0`
- PASS: `0`
- FAIL: `12`

## Calibration Table

| Mode | Freq Hz | Target mVrms | Measured mVrms | Error mVrms | CH0 code | Atten dB | Iters | Result |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| AM | 110000000 | 100.000 | nan | nan | 1023 | 0.0 | 0 | FAIL |
| AM | 110100000 | 100.000 | nan | nan | 1023 | 0.0 | 0 | FAIL |
| AM | 110200000 | 100.000 | nan | nan | 1023 | 0.0 | 0 | FAIL |
| AM | 110300000 | 100.000 | nan | nan | 1023 | 0.0 | 0 | FAIL |
| AM | 110400000 | 100.000 | nan | nan | 1023 | 0.0 | 0 | FAIL |
| AM | 110500000 | 100.000 | nan | nan | 1023 | 0.0 | 0 | FAIL |
| AM | 110600000 | 100.000 | nan | nan | 1023 | 0.0 | 0 | FAIL |
| AM | 110700000 | 100.000 | nan | nan | 1023 | 0.0 | 0 | FAIL |
| AM | 110800000 | 100.000 | nan | nan | 1023 | 0.0 | 0 | FAIL |
| AM | 110900000 | 100.000 | nan | nan | 1023 | 0.0 | 0 | FAIL |
| AM | 111000000 | 100.000 | nan | nan | 1023 | 0.0 | 0 | FAIL |
| AM | 111100000 | 100.000 | nan | nan | 1023 | 0.0 | 0 | FAIL |

## Failed Points

- `AM` `110000000 Hz`: measured `nan mVrms`, code `1023`, atten `0.0 dB`, reason `stability keyword 'STABLE' timeout`
- `AM` `110100000 Hz`: measured `nan mVrms`, code `1023`, atten `0.0 dB`, reason `stability keyword 'STABLE' timeout`
- `AM` `110200000 Hz`: measured `nan mVrms`, code `1023`, atten `0.0 dB`, reason `stability keyword 'STABLE' timeout`
- `AM` `110300000 Hz`: measured `nan mVrms`, code `1023`, atten `0.0 dB`, reason `stability keyword 'STABLE' timeout`
- `AM` `110400000 Hz`: measured `nan mVrms`, code `1023`, atten `0.0 dB`, reason `stability keyword 'STABLE' timeout`
- `AM` `110500000 Hz`: measured `nan mVrms`, code `1023`, atten `0.0 dB`, reason `stability keyword 'STABLE' timeout`
- `AM` `110600000 Hz`: measured `nan mVrms`, code `1023`, atten `0.0 dB`, reason `stability keyword 'STABLE' timeout`
- `AM` `110700000 Hz`: measured `nan mVrms`, code `1023`, atten `0.0 dB`, reason `stability keyword 'STABLE' timeout`
- `AM` `110800000 Hz`: measured `nan mVrms`, code `1023`, atten `0.0 dB`, reason `stability keyword 'STABLE' timeout`
- `AM` `110900000 Hz`: measured `nan mVrms`, code `1023`, atten `0.0 dB`, reason `stability keyword 'STABLE' timeout`
- `AM` `111000000 Hz`: measured `nan mVrms`, code `1023`, atten `0.0 dB`, reason `stability keyword 'STABLE' timeout`
- `AM` `111100000 Hz`: measured `nan mVrms`, code `1023`, atten `0.0 dB`, reason `stability keyword 'STABLE' timeout`
