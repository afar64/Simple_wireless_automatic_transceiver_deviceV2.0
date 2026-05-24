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
- When adding the left-side `Continuous`, `Sweep`, `Mod`, and `System` buttons, keep the first step as a navigation skeleton. Do not move all controls into pages in the same change, or hardware touch regressions become hard to isolate.
- Left-side buttons need enough fixed width for the longest label (`Continuous`); shift the content rows right instead of squeezing the label into the old `x=36` row area.
- Keep old bottom controls available while validating the new section buttons so there is still a known-good fallback path for Apply/Start/Field/Digit.
- For the H743 signal-source UI, treat `Single / Sweep / Mod / System` as explicit sections, not just temporary highlights derived from runtime state. Otherwise the UI can jump between sections unexpectedly while the user is interacting.
- `System` should be a view switch, not the same action as toggling debug mode. Keep debug enable/disable as a separate control inside the system section.
- To keep the UI fast, hide whole groups of unrelated controls per section instead of leaving every label and button visible and updating all of them every cycle.
- When matching a hardware photo/reference layout, prioritize geometry first: section positions, spacing, visible controls, and labels. Color and font tuning can come after the interaction path is stable on the board.
- Do not assume larger LVGL Montserrat fonts are enabled in `lv_conf.h`. This project currently does not expose `lv_font_montserrat_20/24`, so layout changes should avoid depending on unavailable font assets unless you explicitly enable them first.
- When switching the H743 UI to a blue theme, keep the palette centralized as constants in `app_lvgl_ui.c`. Do not scatter new `lv_color_hex(...)` values throughout refresh code, or later tuning becomes tedious and inconsistent.
- For the `Single` page, keep only the two core controls in the center: `LO Freq` and `Amp`. Extra controls on this page make the interaction path longer and reduce scan speed on the H743 screen.
- On the `Single` page, bottom buttons should map directly to fields such as `FREQ` and `AMP`, instead of using generic `Field / Digit` cycling. Direct selection is faster and matches the instrument-style workflow better.
- This H743 UI should be a fixed control surface, not a sliding page. Explicitly remove `LV_OBJ_FLAG_SCROLLABLE` and turn scrollbars off on both the screen and the root panel, otherwise touch drags can still be interpreted as scroll gestures later.
- After each UI change on this project, explicitly report whether compile and flash were actually run, plus whether verification and MCU reset succeeded. Do not assume a brief summary is enough.
- Working rule for this repository: after every code change, always run one compile and one flash before closing the task, even if the build is incremental and reports `ninja: no work to do`.
