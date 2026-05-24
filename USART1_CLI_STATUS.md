# USART1 CLI Status

## Scope

This note records the commands that have been verified on the current `USART1` asynchronous CLI implementation.

Project:

- `F:\CAMKE\WIRELESS\Simple_wireless_automatic_transceiver_deviceV2.0`

Test date:

- `2026-05-23`

Serial settings:

- `USART1`
- `115200`
- `8N1`

Observed COM port during test:

- `COM6`

## Verified Working Commands

The following commands have been verified to work when sent slowly, byte by byte:

- `PING`
  - Expected response:
    - `PONG`

- `HELP`
  - Expected response:
    - `HELP`
    - `PING`
    - `DDS?`

- `DDS?`
  - Expected response:
    - current DDS CH0..CH3 frequency and amplitude status

In addition, the single-character command below has been verified to work even with fast whole-line send:

- `?`
  - Expected response:
    - `HELP`
    - `PING`
    - `DDS?`

## Important Limitation

Current status is not yet fully stable for fast whole-line commands.

Observed behavior:

- slow byte-by-byte send:
  - `PING` works
  - `HELP` works
  - `DDS?` works

- fast whole-line send:
  - `?` works
  - `PING` may return `ERR unknown command`
  - `HELP` may return `ERR unknown command`
  - `DDS?` may return `ERR unknown command`

## Practical Recommendation

If you need to use the current firmware version before the next serial receive upgrade, use:

- `?` for quick connectivity check
- slow-send mode for:
  - `PING`
  - `HELP`
  - `DDS?`

## Next Likely Fix

The next receive-side improvement should be:

- `USART1 DMA RX + IDLE`

This note is only a status record. It does not change current firmware behavior.
