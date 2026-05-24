# 发射机串口联调最小验证

## 总结

- `cw_sweep`: `FAIL`
- `am`: `FAIL`
- `fm`: `FAIL`
- `ask`: `FAIL`
- `fsk`: `PASS`
- `psk`: `FAIL`

## CW Sweep

- Verdict: `FAIL`
- Range: `120000000 .. 122000000`
- Period: `2000 ms`
- Observed step: `300000 Hz`
- RMS span ratio: `0.0000`
- Vpp span ratio: `0.0000`

### Notes

- Sweep range mismatch: observed 120600000..120900000
- Sweep step mismatch: diffs=[300000]
- CH3 frequency deviated too far from expected sweep grid

## 单点调制

### AM

- Verdict: `FAIL`
- Carrier: `29990000.000 Hz`
- mod_freq_hz: `20000.000000010357`
- am_depth_pct: `7.474950667663392`
- AM frequency mismatch: got 20000.0 Hz
- AM depth mismatch: got 7.47%
- Result dir: `F:\CAMKE\backups\restore_h743_20260523_061047\Simple_wireless_automatic_transceiver_deviceV2.0\tools\tx_joint_debug_results\joint_debug_20260524_005630\modes\tek_ch3_accuracy_am_20260524_005638`

### FM

- Verdict: `FAIL`
- Carrier: `50000000.000 Hz`
- mod_freq_hz: `2000.0200002010374`
- fm_dev_hz: `2719316.943277327`
- FM modulation frequency mismatch: got 2000.0 Hz
- FM deviation mismatch: got 2719316.9 Hz
- Result dir: `F:\CAMKE\backups\restore_h743_20260523_061047\Simple_wireless_automatic_transceiver_deviceV2.0\tools\tx_joint_debug_results\joint_debug_20260524_005630\modes\tek_ch3_accuracy_fm_20260524_005642`

### ASK

- Verdict: `FAIL`
- Carrier: `1000.000 Hz`
- bit_rate_hz: `4000.1600064023273`
- low_high_ratio: `0.9335821840294334`
- ASK symbol rate mismatch: got 4000.2 Hz
- ASK envelope tone estimate was 1000.0 Hz
- ASK run-length estimate was 21367.5 Hz
- ASK edge-rate estimate was 57091.3 Hz
- ASK low/high envelope ratio too high: 0.934
- Result dir: `F:\CAMKE\backups\restore_h743_20260523_061047\Simple_wireless_automatic_transceiver_deviceV2.0\tools\tx_joint_debug_results\joint_debug_20260524_005630\modes\tek_ch3_accuracy_ask_20260524_005648`

### FSK

- Verdict: `PASS`
- Carrier: `50000000.000 Hz`
- bit_rate_hz: `10500.000000005437`
- fsk_shift_hz: `20000.00000000745`
- FSK I/Q symbol rate and RF peak shift are within tolerance
- Result dir: `F:\CAMKE\backups\restore_h743_20260523_061047\Simple_wireless_automatic_transceiver_deviceV2.0\tools\tx_joint_debug_results\joint_debug_20260524_005630\modes\tek_ch3_accuracy_fsk_20260524_005652`

### PSK

- Verdict: `FAIL`
- Carrier: `50000000.000 Hz`
- bit_rate_hz: `10864.841373321575`
- phase_flip_deg: `180.0`
- PSK symbol rate mismatch: got 10864.8 Hz
- Result dir: `F:\CAMKE\backups\restore_h743_20260523_061047\Simple_wireless_automatic_transceiver_deviceV2.0\tools\tx_joint_debug_results\joint_debug_20260524_005630\modes\tek_ch3_accuracy_psk_20260524_005701`

