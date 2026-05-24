# CH3 Accuracy Summary

- Mode: `AM`
- Verdict: `FAIL`
- Scope: `TEKTRONIX,DPO3034,C013425,CF:91.1CT FV:v2.23`
- Sample rate: `1249999999.997 Hz`
- Points: `1000000`
- CH3 Vpp: `0.160000 V`
- CH3 RMS(ac): `0.015076 V`
- Carrier: `131000000.000 Hz`
- mod_freq_hz: `23750.000000`
- am_depth_pct: `2.143407`

## Notes

- AM frequency mismatch: got 23750.0 Hz
- AM depth mismatch: got 2.14%

## Serial Log

```text

[WB] CH0 LO set: 130000000 Hz AMP 512
[WB] USART3 send: QG 980
[WB] USART3 reply: SET Q_GAIN 980

[WB] USART3 send: QP 0
[WB] USART3 reply: S
[WB] USART3 send: IO -50
[WB] USART3 reply: <none>
[WB] USART3 send: QO 100
```
