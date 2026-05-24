# Avoid Pitfalls

This file records pitfalls found during Codex-assisted work in this repository.
Update it after each completed task.

## 2026-05-25

- Do not bulk-delete files or directories in this repository. Avoid `del /s`, `rd /s`, `rmdir /s`, `Remove-Item -Recurse`, and `rm -rf`. Delete only one explicit file path at a time.
- The repository was already initialized as Git on branch `codex/trans`; do not assume a fresh `git init` is needed.
- `build/Debug` contains stale CMake cache data pointing to `F:/CAMKE/WIRELESS/Simple_wireless_automatic_transceiver_deviceV2.0/build/Debug`. Building that preset directly can cause repeated CMake regeneration and Ninja failure.
- Use a fresh build directory such as `build/codex-flash-20260525` instead of deleting or reusing the stale `build/Debug` directory.
- PowerShell may split `-DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake` unexpectedly if it is not quoted. Use a quoted full path for CMake cache arguments.
- The working toolchain is under `E:/cubeclt/STM32CubeCLT_1.18.0`, including CMake, Ninja, ARM GCC, and STM32CubeProgrammer.
- STM32CubeProgrammer detected ST-LINK serial `38FF6406504D353736550743` and successfully flashed the ELF over SWD.
- UI work should follow `UI_BASELINE_HANDOFF_20260525.md` conservatively: reduce loop coupling first, then split responsibilities, and only later redesign direct-touch editing.
- Keep `lv_timer_handler()` running every LVGL task cycle. Throttle non-UI synchronization around it instead of slowing the LVGL handler itself.
- Do not refresh LVGL labels on every loop when no state changed; repeated `lv_label_set_text*()` calls can add redraw pressure and make touch latency harder to diagnose.
- After each UI step, build and flash the exact ELF from `build/codex-flash-20260525` before judging hardware touch behavior.
