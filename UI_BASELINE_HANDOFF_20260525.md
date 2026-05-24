# UI Baseline Handoff

## Project

- Path: `F:\CAMKE\backups\restore_h743_20260523_061047\Simple_wireless_automatic_transceiver_deviceV2.0_git`
- Remote: `https://github.com/afar64/Simple_wireless_automatic_transceiver_deviceV2.0.git`
- Current branch seen locally: `codex/trans`

## Current UI Baseline

The current UI baseline is still the button-driven LVGL control surface in:

- `Core/APP/LVGL/app_lvgl_ui.c`
- `Core/APP/LVGL/app_lvgl.c`
- `Core/APP/LVGL/app_lvgl_port_indev.c`
- `Core/APP/Touch/app_touch_gt9xx.c`
- `Core/APP/TxControl/app_tx_control.c`

This baseline is closer to the older stable control style than the later heavy modal/numeric-keypad variants.

## Current Screen Structure

The current main screen contains:

- title: `IQ Modulator`
- LO frequency row
- mode dropdown
- amplitude row
- rate row
- param row
- preset/debug/self-test/sweep badges
- bottom action buttons

The current interaction model is still based on:

- selecting an editable field
- selecting a digit/step
- increment/decrement adjustment
- apply/start-stop buttons

It is not yet the final direct-touch keypad workflow the user wanted.

## Current Runtime Flow

### LVGL main loop

`Core/APP/LVGL/app_lvgl.c` currently does three things in `App_LvglRun()`:

1. `App_TxControl_Service(HAL_GetTick())`
2. `App_LvglUiPoll()`
3. `lv_timer_handler()`

This means the LVGL task is carrying both UI redraw work and part of the TX state service/update path.

### Touch path

Touch path is:

1. GT9xx interrupt
2. `App_TouchNotifyFromISR()`
3. `App_TouchTaskRun()`
4. cached touch state
5. `App_LvglPortIndevRead()`

The LVGL input callback is non-blocking and only reads cached coordinates.

## Changes Already Confirmed

The following touch fix has already been applied and pushed:

- commit: `5d64eb8`
- summary: restore GT9xx dual-address probe fallback

That fix changes `app_touch_gt9xx.c` to:

- try primary I2C address first
- fall back to alternate GT9xx address
- keep the chosen address for later register access

## Known Issues

### 1. UI logic is still too heavy

`app_lvgl_ui.c` has accumulated too much behavior in one file:

- field state management
- mode switching
- sweep controls
- preset/debug controls
- winner bridge coupling
- periodic poll-driven refresh

This makes touch problems harder to isolate.

### 2. LVGL loop carries extra non-UI work

`App_TxControl_Service()` and `App_LvglUiPoll()` both run from the LVGL path.
This increases the chance that touch responsiveness degrades when UI state and TX state update together.

### 3. Interaction model is not yet aligned with the target UX

The user wants a more direct interaction style, but this baseline still behaves like an engineering control panel:

- field-select first
- then adjust
- then apply

That is stable, but not yet the desired final UX.

## Recommended Next Optimization Order

Do not stack major UI redesign changes in one step. Use this order:

1. stabilize touch and redraw latency
2. reduce LVGL loop workload
3. simplify `app_lvgl_ui.c` responsibilities
4. split Single/Sweep/Mod state handling into smaller blocks
5. only then reintroduce direct-touch editing or keypad UX

## Recommended Next Step

The next safe step should be:

### Step 2: reduce UI loop coupling

Review and narrow:

- `App_LvglUiPoll()`
- `App_TxControl_Service(HAL_GetTick())`
- any repeated full-screen refresh or full-state sync in the LVGL task

Goal:

- keep touch input stable
- keep redraw cheap
- avoid large synchronous UI refreshes on every cycle

## Git Workflow For This Project

Required working rule:

1. one small optimization
2. one local commit
3. immediate push

Recommended commands:

```powershell
git status
git add <changed-files>
git commit -m "message"
git push
```

For rollback on shared history, prefer:

```powershell
git revert <commit-id>
git push
```

Avoid force-push unless explicitly required.

## Bench Validation Focus

After each UI change, validate these points on hardware:

1. touch press recognized on first tap
2. no long delay before UI response
3. no frozen state after page/control changes
4. apply/start-stop still work
5. sweep/debug controls do not break touch continuity

