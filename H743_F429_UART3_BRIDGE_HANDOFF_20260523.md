# H743 <-> F429 UART3 Bridge Handoff

## Scope

Current target is to let the H743 accept one integrated command on `USART1`, then split it into:

1. local H743 actions:
   - set `AD9959 CH0` LO frequency
   - set `AD9959 CH0` LO amplitude
2. forwarded F429 commands on `USART3 -> UART4`:
   - `QG`
   - `QP`
   - `IO`
   - `QO`
   - modulation command such as `TAM`

This is being done conservatively, one modulation at a time.

## Hardware Mapping

### H743 side

- `USART1`: debug / CLI to PC
- `USART3`: bridge to F429
  - `PB10 = USART3_TX`
  - `PB11 = USART3_RX`
  - `115200 8N1`

### F429 side

- `USART1`: debug / command port to PC
- `UART4`: command port from H743
  - `PA0 = UART4_TX`
  - `PA1 = UART4_RX`
  - `115200 8N1`

## Verified Results

The following bridge path has been bench-verified:

`H743 USART3 -> F429 UART4 -> F429/FPGA AD9767 -> LTC5598 -> Tek CH3`

### Verified command forwarding

H743 sent:

- `QG 1050`

F429 PC serial returned:

- `SET Q_GAIN 1050`

H743 `USART1` mirrored:

- `[WB] USART3 test send: QG 1050`
- `[WB] USART3 reply: SET Q_GAIN 1050`

### Verified full AM chain

H743 explicitly set:

- `CH0 LO = 130000000 Hz`
- `CH0 AMP = 512`

Then H743 forwarded:

- `QG 1050`
- `QP 0`
- `IO 300`
- `QO 0`
- `TAM 10000 8192 1200 500`

F429 PC serial returned:

- `SET Q_GAIN 1050`
- `SET Q_PHASE 0`
- `SET I_DC_TRIM 300`
- `SET Q_DC_TRIM 0`
- `BUILD TAM fm=10000 offset=8192 amp=1200 m_permille=500`
- `SET TABLE_STEP freq=10000 step=343597`
- `REG8=0x3E2D rb=0x3E2D OK`
- `REG9=0x0005 rb=0x0005 OK`

Tektronix `CH3` measurement result:

- envelope frequency about `10.000 kHz`
- AM depth about `46.42%`

Conclusion: the explicit LO + forwarded AM command chain is working.

## Current Code Status

### Files involved

- `Core/App/WinnerBridge/app_winner_bridge.h`
- `Core/App/WinnerBridge/app_winner_bridge.c`
- `Core/APP/Uart/app_uart_cli.c`
- `Core/Src/freertos.c`
- `Core/APP/Tasks/app_ad9959_task.h`
- `Core/APP/Tasks/app_ad9959_task.c`

### Current bridge implementation state

Bridge support was temporarily implemented as an automatic startup AM sequence for bench verification.

That automatic startup behavior should now be replaced by an explicit CLI command.

The header has already been switched toward the next step:

- `app_winner_bridge.h` now declares:
  - `App_WinnerBridge_SendAmSequence(...)`

This means the intended next implementation is command-driven, not auto-run.

## Next Step To Execute

Implement one integrated CLI command on H743 `USART1`:

```text
TXAM <LO_HZ> <LO_AMP> <QG> <QP> <IO> <QO> <FM> <OFFSET> <AMP> <DEPTH>
```

Example:

```text
TXAM 130000000 512 1050 0 300 0 10000 8192 1200 500
```

Expected H743 behavior:

1. set local `CH0`:
   - freq = `130000000`
   - amp = `512`
   - enable = `1`
2. send to F429:
   - `QG 1050`
   - `QP 0`
   - `IO 300`
   - `QO 0`
   - `TAM 10000 8192 1200 500`
3. mirror F429 replies back to `USART1`

## Constraints

- Do not broaden to FM/ASK/FSK/PSK yet.
- Do not hook this into UI yet.
- Do not add a batch command parser for all modes yet.
- Keep the first integrated command limited to AM only.

## Why This Sequence

This keeps risk low:

1. protocol is already bench-verified
2. only one new CLI command is added
3. local LO ownership remains on H743
4. baseband ownership remains on F429
5. scope-side validation can continue one mode at a time
