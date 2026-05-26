#include "app_lvgl_ui.h"

#include "app_tx_control.h"
#include "app_winner_bridge.h"
#include "app_dds_ctrl.h"
#include "app_pe4302.h"

#include "lvgl.h"

#include <stdio.h>
#include <string.h>

#define APP_UI_PANEL_W 720
#define APP_UI_PANEL_H 440
#define APP_UI_LABEL_W 120
#define APP_UI_VALUE_W 280
#define APP_UI_AMP_RMS_MIN_MV 10U
#define APP_UI_AMP_RMS_MAX_MV 100U
#define APP_UI_AMP_RMS_STEP_MV 10U
#define APP_UI_AMP_INTERNAL_SCALE 10U
#define APP_UI_STATE_REFRESH_PERIOD_MS 200U
#define APP_UI_SECTION_BUTTON_X 24
#define APP_UI_SECTION_BUTTON_W 96
#define APP_UI_SECTION_BUTTON_H 56
#define APP_UI_CONTENT_LABEL_X 250
#define APP_UI_CONTENT_VALUE_X 394
#define APP_UI_COLOR_BG           0x101622
#define APP_UI_COLOR_PANEL        0x1E2A3A
#define APP_UI_COLOR_PANEL_BORDER 0x78B8FF
#define APP_UI_COLOR_TEXT_MAIN    0xF5FBFF
#define APP_UI_COLOR_TEXT_SUB     0xB8D8FF
#define APP_UI_COLOR_TEXT_VALUE   0xDCEEFF
#define APP_UI_COLOR_TEXT_MUTED   0x6F86A6
#define APP_UI_COLOR_BUTTON_IDLE  0x2A3E57
#define APP_UI_COLOR_BUTTON_ON    0x3EA9FF
#define APP_UI_COLOR_ACCENT       0x66C7FF

typedef enum
{
  APP_UI_SECTION_SINGLE = 0,
  APP_UI_SECTION_SWEEP,
  APP_UI_SECTION_MOD,
  APP_UI_SECTION_SYSTEM
} AppUiSection;

typedef enum
{
  APP_UI_KEYPAD_LO_FREQ = 0,
  APP_UI_KEYPAD_AMP,
  APP_UI_KEYPAD_SWEEP_START,
  APP_UI_KEYPAD_SWEEP_STOP,
  APP_UI_KEYPAD_SWEEP_TIME,
  APP_UI_KEYPAD_SWEEP_AMP,
  APP_UI_KEYPAD_MOD_FC,
  APP_UI_KEYPAD_MOD_FREQ,
  APP_UI_KEYPAD_MOD_DEPTH
} AppUiKeypadTarget;

typedef enum
{
  APP_UI_PENDING_ACTION_NONE = 0,
  APP_UI_PENDING_ACTION_SINGLE_RUN,
  APP_UI_PENDING_ACTION_SWEEP_RUN,
  APP_UI_PENDING_ACTION_MOD_RUN,
  APP_UI_PENDING_ACTION_SYSTEM_RUN
} AppUiPendingAction;

static lv_obj_t *g_mode_dropdown = NULL;
static lv_obj_t *g_mode_name_label = NULL;
static lv_obj_t *g_section_title_label = NULL;
static lv_obj_t *g_continuous_button = NULL;
static lv_obj_t *g_sweep_section_button = NULL;
static lv_obj_t *g_mod_button = NULL;
static lv_obj_t *g_system_button = NULL;
static lv_obj_t *g_lo_name_label = NULL;
static lv_obj_t *g_lo_value_label = NULL;
static lv_obj_t *g_vpp_name_label = NULL;
static lv_obj_t *g_vpp_value_label = NULL;
static lv_obj_t *g_rf_name_label = NULL;
static lv_obj_t *g_rf_value_label = NULL;
static lv_obj_t *g_rate_name_label = NULL;
static lv_obj_t *g_rate_value_label = NULL;
static lv_obj_t *g_param_name_label = NULL;
static lv_obj_t *g_param_value_label = NULL;
static lv_obj_t *g_preset_name_label = NULL;
static lv_obj_t *g_preset_value_label = NULL;
static lv_obj_t *g_debug_status_label = NULL;
static lv_obj_t *g_ad9959_status_label = NULL;
static lv_obj_t *g_f429_status_label = NULL;
static lv_obj_t *g_sweep_time_name_label = NULL;
static lv_obj_t *g_sweep_time_value_label = NULL;
static lv_obj_t *g_sweep_start_name_label = NULL;
static lv_obj_t *g_sweep_start_value_label = NULL;
static lv_obj_t *g_sweep_stop_name_label = NULL;
static lv_obj_t *g_sweep_stop_value_label = NULL;
static lv_obj_t *g_edit_label = NULL;
static lv_obj_t *g_state_label = NULL;
static lv_obj_t *g_run_button_label = NULL;
static lv_obj_t *g_sweep_button_label = NULL;
static lv_obj_t *g_preset_save_button = NULL;
static lv_obj_t *g_preset_recall_button = NULL;
static lv_obj_t *g_sweep_button = NULL;
static lv_obj_t *g_sweep_time_button = NULL;
static lv_obj_t *g_sweep_start_button = NULL;
static lv_obj_t *g_sweep_stop_button = NULL;
static lv_obj_t *g_debug_button = NULL;
static lv_obj_t *g_debug_button_label = NULL;
static lv_obj_t *g_field_button = NULL;
static lv_obj_t *g_digit_button = NULL;
static lv_obj_t *g_dec_button = NULL;
static lv_obj_t *g_inc_button = NULL;
static lv_obj_t *g_apply_button = NULL;
static lv_obj_t *g_run_button = NULL;
static lv_obj_t *g_keypad_overlay = NULL;
static lv_obj_t *g_keypad_panel = NULL;
static lv_obj_t *g_keypad_title_label = NULL;
static lv_obj_t *g_keypad_current_label = NULL;
static lv_obj_t *g_keypad_value_label = NULL;
static lv_obj_t *g_keypad_unit_label = NULL;
static lv_obj_t *g_keypad_dot_button = NULL;

static AppDacWavegenConfig g_ui_config;
static uint32_t g_lo_freq_hz = APP_AD9959_TASK_DEFAULT_FREQ_HZ;
static uint32_t g_preset_freq_hz = APP_TX_CONTROL_PRESET_DEFAULT_FREQ_HZ;
static uint32_t g_sweep_period_ms = APP_TX_CONTROL_SWEEP_PERIOD_MS;
static uint32_t g_sweep_start_hz = APP_TX_CONTROL_SWEEP_START_DEFAULT_HZ;
static uint32_t g_sweep_stop_hz = APP_TX_CONTROL_SWEEP_STOP_DEFAULT_HZ;
static AppUiEditField g_selected_field = APP_UI_DEFAULT_FIELD;
static uint8_t g_digit_index = APP_UI_DEFAULT_DIGIT_INDEX;
static uint8_t g_lo_freq_dirty = 0U;
static uint8_t g_mode_dirty = 0U;
static uint32_t g_state_refresh_last_tick_ms = 0U;
static AppUiSection g_active_section = APP_UI_SECTION_SINGLE;
static AppUiKeypadTarget g_keypad_target = APP_UI_KEYPAD_LO_FREQ;
static char g_keypad_input[24] = {0};
static int32_t g_system_lo_error_hz = 0;
static uint8_t g_system_error_step_index = 0U;
static AppUiPendingAction g_pending_action = APP_UI_PENDING_ACTION_NONE;
static uint8_t g_pending_tx_on_valid = 0U;
static uint8_t g_pending_tx_on = 0U;
static uint8_t g_pending_sweep_on_valid = 0U;
static uint8_t g_pending_sweep_on = 0U;
static uint8_t g_pending_calibration_on_valid = 0U;
static uint8_t g_pending_calibration_on = 0U;

static const uint32_t s_lo_steps_hz[] = {APP_TX_CONTROL_FREQ_STEP_HZ};
static const uint32_t s_amp_steps_mv[] = {APP_UI_AMP_RMS_STEP_MV};
static const uint32_t s_rate_steps_hz[] = {10000UL, 1000UL, 100UL, 10UL, 1UL};
static const uint32_t s_param_steps_pct[] = {10U, 1U};
static const uint32_t s_system_error_steps_hz[] = {1UL, 10UL, 100UL, 1000UL};

static uint32_t App_LvglUiInternalToAmpRmsMv(uint16_t internal_mv);
static uint16_t App_LvglUiAmpRmsMvToInternal(uint32_t amp_rms_mv);

static lv_obj_t *App_LvglUiCreatePanel(lv_obj_t *parent);
static void App_LvglUiCreateTitle(lv_obj_t *parent);
static void App_LvglUiCreateSectionButtons(lv_obj_t *parent);
static lv_obj_t *App_LvglUiCreateSectionButton(lv_obj_t *parent,
                                               const char *text,
                                               int32_t y,
                                               lv_event_cb_t event_cb);
static void App_LvglUiCreateModeRow(lv_obj_t *parent, AppDacWavegenMode mode, int32_t y);
static void App_LvglUiCreateValueRow(lv_obj_t *parent,
                                     const char *name,
                                     int32_t y,
                                     lv_obj_t **name_label,
                                     lv_obj_t **value_label);
static void App_LvglUiCreatePresetBadge(lv_obj_t *parent);
static void App_LvglUiCreateDebugBadge(lv_obj_t *parent);
static void App_LvglUiCreateSelfTestBadge(lv_obj_t *parent);
static void App_LvglUiCreateSweepTimeBadge(lv_obj_t *parent);
static void App_LvglUiCreateSweepRangeBadge(lv_obj_t *parent);
static void App_LvglUiCreateButtons(lv_obj_t *parent);
static void App_LvglUiCreateKeypad(lv_obj_t *parent);
static lv_obj_t *App_LvglUiCreateActionButton(lv_obj_t *parent,
                                              const char *text,
                                              int32_t x,
                                              int32_t y,
                                              int32_t w,
                                              int32_t h,
                                              lv_event_cb_t event_cb,
                                              void *user_data);
static void App_LvglUiResetModeConfig(AppDacWavegenMode mode);
static uint8_t App_LvglUiIsCwMode(AppDacWavegenMode mode);
static void App_LvglUiEnsureEditableField(void);
static void App_LvglUiRefreshValues(void);
static void App_LvglUiRefreshEditState(void);
static void App_LvglUiRefreshState(void);
static void App_LvglUiRefreshSectionButtons(const AppTxControlSnapshot *snapshot, uint8_t debug_locked);
static void App_LvglUiRefreshSectionVisibility(void);
static void App_LvglUiProcessPendingAction(void);
static void App_LvglUiSetActiveSection(AppUiSection section);
static const char *App_LvglUiSectionTitle(void);
static void App_LvglUiApplySingleLayout(void);
static void App_LvglUiApplySweepLayout(void);
static void App_LvglUiApplyModLayout(void);
static void App_LvglUiApplySystemLayout(void);
static void App_LvglUiRefreshModeVisibility(void);
static const char *App_LvglUiModRateButtonText(void);
static const char *App_LvglUiModParamButtonText(void);
static uint32_t App_LvglUiGetSystemErrorStepHz(void);
static void App_LvglUiAdjustSystemError(int32_t direction);
static void App_LvglUiApplySystemErrorNow(void);
static uint8_t App_LvglUiIsFieldEditable(AppUiEditField field);
static const char *App_LvglUiFieldName(AppUiEditField field);
static uint32_t App_LvglUiGetFieldStepCount(AppUiEditField field);
static uint32_t App_LvglUiGetFieldStepValue(AppUiEditField field, uint8_t index);
static void App_LvglUiGetFieldRange(AppUiEditField field, uint32_t *min_value, uint32_t *max_value);
static uint32_t App_LvglUiGetFieldValue(AppUiEditField field);
static void App_LvglUiSetFieldValue(AppUiEditField field, uint32_t value);
static void App_LvglUiAdjustSelectedField(int32_t direction);
static void App_LvglUiApplyConfig(void);
static void App_LvglUiOpenKeypad(AppUiKeypadTarget target);
static void App_LvglUiCloseKeypad(void);
static void App_LvglUiRefreshKeypad(void);
static uint8_t App_LvglUiTryParseKeypadValue(AppUiKeypadTarget target, const char *text, uint32_t *value_out);
static void App_LvglUiOnModeChanged(lv_event_t *e);
static void App_LvglUiOnFieldClicked(lv_event_t *e);
static void App_LvglUiOnDigitClicked(lv_event_t *e);
static void App_LvglUiSelectField(AppUiEditField field);
static void App_LvglUiOnDecClicked(lv_event_t *e);
static void App_LvglUiOnIncClicked(lv_event_t *e);
static void App_LvglUiOnPresetSaveClicked(lv_event_t *e);
static void App_LvglUiOnPresetRecallClicked(lv_event_t *e);
static void App_LvglUiOnSweepClicked(lv_event_t *e);
static void App_LvglUiOnSweepTimeClicked(lv_event_t *e);
static void App_LvglUiOnSweepStartClicked(lv_event_t *e);
static void App_LvglUiOnSweepStopClicked(lv_event_t *e);
static void App_LvglUiOnApplyClicked(lv_event_t *e);
static void App_LvglUiOnRunClicked(lv_event_t *e);
static void App_LvglUiOnDebugClicked(lv_event_t *e);
static void App_LvglUiOnContinuousClicked(lv_event_t *e);
static void App_LvglUiOnSweepSectionClicked(lv_event_t *e);
static void App_LvglUiOnModClicked(lv_event_t *e);
static void App_LvglUiOnSystemClicked(lv_event_t *e);
static void App_LvglUiOnKeypadDigitClicked(lv_event_t *e);
static void App_LvglUiOnKeypadBackClicked(lv_event_t *e);
static void App_LvglUiOnKeypadCancelClicked(lv_event_t *e);
static void App_LvglUiOnKeypadOkClicked(lv_event_t *e);
static void App_LvglUiOnKeypadDeleteClicked(lv_event_t *e);
static const char *App_LvglUiModeText(AppDacWavegenMode mode);
static int App_LvglUiSendCurrentModeToWinner(void);
static uint32_t App_LvglUiGetWinnerLoFrequencyHz(void);
static uint8_t App_LvglUiIsDebugLocked(void);
static uint32_t App_LvglUiGetFskNextFrequencyHz(void);

#define APP_UI_WINNER_QG_DEFAULT      980U
#define APP_UI_WINNER_QP_DEFAULT        0
#define APP_UI_WINNER_IO_DEFAULT      (-50)
#define APP_UI_WINNER_QO_DEFAULT      100
#define APP_UI_WINNER_LO_AMP_DEFAULT  512U

void App_LvglUiInit(void)
{
  lv_obj_t *screen = lv_screen_active();
  lv_obj_t *panel;
  AppTxControlSnapshot snapshot;
  AppDacWavegenConfig desired_config;
  uint32_t desired_freq_hz = 0U;

  lv_obj_set_style_bg_color(screen, lv_color_hex(APP_UI_COLOR_BG), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

  App_TxControl_Init();
  (void)App_TxControl_SetSweepEnabled(0U);
  (void)App_TxControl_SetMode(APP_DAC_WAVE_MODE_CW);
  (void)App_TxControl_Stop();
  App_WinnerBridge_InitAsync();
  (void)App_TxControl_GetConfig(&desired_config, &desired_freq_hz);
  App_TxControl_GetSnapshot(&snapshot);
  g_ui_config = desired_config;
  g_lo_freq_hz = desired_freq_hz;
  g_preset_freq_hz = App_TxControl_GetPresetFrequencyHz();
  g_sweep_period_ms = App_TxControl_GetSweepPeriodMs();
  g_sweep_start_hz = App_TxControl_GetSweepStartHz();
  g_sweep_stop_hz = App_TxControl_GetSweepStopHz();
  g_system_lo_error_hz = App_TxControl_GetLoErrorHz();
  g_active_section = APP_UI_SECTION_SINGLE;

  panel = App_LvglUiCreatePanel(screen);
  App_LvglUiCreateTitle(panel);
  App_LvglUiCreateSectionButtons(panel);
  App_LvglUiCreateDebugBadge(panel);
  App_LvglUiCreateSelfTestBadge(panel);
  App_LvglUiCreatePresetBadge(panel);
  App_LvglUiCreateSweepTimeBadge(panel);
  App_LvglUiCreateSweepRangeBadge(panel);
  App_LvglUiCreateValueRow(panel, "LO freq", 74, &g_lo_name_label, &g_lo_value_label);
  App_LvglUiCreateModeRow(panel, g_ui_config.mode, 116);
  App_LvglUiCreateValueRow(panel, "Amp", 160, &g_vpp_name_label, &g_vpp_value_label);
  App_LvglUiCreateValueRow(panel, "RF out", 214, &g_rf_name_label, &g_rf_value_label);
  App_LvglUiCreateValueRow(panel, "Rate", 206, &g_rate_name_label, &g_rate_value_label);
  App_LvglUiCreateValueRow(panel, "Param", 252, &g_param_name_label, &g_param_value_label);
  App_LvglUiCreateButtons(panel);
  App_LvglUiCreateKeypad(panel);

  App_LvglUiEnsureEditableField();
  App_LvglUiRefreshValues();
  App_LvglUiRefreshEditState();
  App_LvglUiRefreshState();
}

void App_LvglUiPoll(void)
{
  AppTxControlSnapshot snapshot;
  AppDacWavegenConfig desired_config;
  uint32_t desired_freq_hz = 0U;
  uint32_t now_ms = lv_tick_get();
  uint8_t ui_changed = 0U;

  App_LvglUiProcessPendingAction();
  (void)App_TxControl_GetConfig(&desired_config, &desired_freq_hz);
  App_TxControl_GetSnapshot(&snapshot);
  if (memcmp(&g_ui_config, &desired_config, sizeof(g_ui_config)) != 0)
  {
    if (g_mode_dirty == 0U)
    {
      g_ui_config = desired_config;
      g_lo_freq_dirty = 0U;
      ui_changed = 1U;
    }
  }

  if (desired_freq_hz != g_lo_freq_hz)
  {
    if (g_lo_freq_dirty == 0U)
    {
      g_lo_freq_hz = desired_freq_hz;
      ui_changed = 1U;
    }
  }

  if (g_sweep_period_ms != App_TxControl_GetSweepPeriodMs())
  {
    g_sweep_period_ms = App_TxControl_GetSweepPeriodMs();
    ui_changed = 1U;
  }

  if ((g_sweep_start_hz != App_TxControl_GetSweepStartHz()) ||
      (g_sweep_stop_hz != App_TxControl_GetSweepStopHz()))
  {
    g_sweep_start_hz = App_TxControl_GetSweepStartHz();
    g_sweep_stop_hz = App_TxControl_GetSweepStopHz();
    ui_changed = 1U;
  }

  if (ui_changed != 0U)
  {
    if (g_mode_dropdown != NULL)
    {
      lv_dropdown_set_selected(g_mode_dropdown,
                               (uint16_t)((g_ui_config.mode <= APP_DAC_WAVE_MODE_2FSK) ? g_ui_config.mode : APP_DAC_WAVE_MODE_AM));
    }
    App_LvglUiRefreshValues();
    App_LvglUiEnsureEditableField();
    App_LvglUiRefreshModeVisibility();
    App_LvglUiRefreshState();
    g_state_refresh_last_tick_ms = now_ms;
    App_LvglUiRefreshEditState();
  }
  else
  {
    if ((g_state_refresh_last_tick_ms == 0U) ||
        ((now_ms - g_state_refresh_last_tick_ms) >= APP_UI_STATE_REFRESH_PERIOD_MS))
    {
      g_state_refresh_last_tick_ms = now_ms;
      App_LvglUiRefreshState();
    }
  }

}

static uint32_t App_LvglUiInternalToAmpRmsMv(uint16_t internal_mv)
{
  uint32_t amp_rms_mv = ((uint32_t)internal_mv + (APP_UI_AMP_INTERNAL_SCALE / 2U)) / APP_UI_AMP_INTERNAL_SCALE;

  if (amp_rms_mv < APP_UI_AMP_RMS_MIN_MV)
  {
    amp_rms_mv = APP_UI_AMP_RMS_MIN_MV;
  }

  if (amp_rms_mv > APP_UI_AMP_RMS_MAX_MV)
  {
    amp_rms_mv = APP_UI_AMP_RMS_MAX_MV;
  }

  return amp_rms_mv;
}

static uint16_t App_LvglUiAmpRmsMvToInternal(uint32_t amp_rms_mv)
{
  if (amp_rms_mv < APP_UI_AMP_RMS_MIN_MV)
  {
    amp_rms_mv = APP_UI_AMP_RMS_MIN_MV;
  }

  if (amp_rms_mv > APP_UI_AMP_RMS_MAX_MV)
  {
    amp_rms_mv = APP_UI_AMP_RMS_MAX_MV;
  }

  return (uint16_t)(amp_rms_mv * APP_UI_AMP_INTERNAL_SCALE);
}

static lv_obj_t *App_LvglUiCreatePanel(lv_obj_t *parent)
{
  lv_obj_t *panel = lv_obj_create(parent);

  lv_obj_set_size(panel, APP_UI_PANEL_W, APP_UI_PANEL_H);
  lv_obj_center(panel);
  lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_color(panel, lv_color_hex(APP_UI_COLOR_PANEL), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(APP_UI_COLOR_PANEL_BORDER), 0);
  lv_obj_set_style_border_width(panel, 2, 0);
  lv_obj_set_style_radius(panel, 8, 0);
  lv_obj_set_style_shadow_width(panel, 0, 0);

  return panel;
}

static void App_LvglUiCreateTitle(lv_obj_t *parent)
{
  lv_obj_t *title = lv_label_create(parent);
  lv_obj_t *subtitle = lv_label_create(parent);

  lv_label_set_text(title, "IQ Modulator");
  lv_obj_set_style_text_color(title, lv_color_hex(APP_UI_COLOR_TEXT_MAIN), 0);
  lv_obj_align(title, LV_ALIGN_TOP_RIGHT, -26, 18);

  lv_label_set_text(subtitle, "DAC1_OUT1 = I / DAC1_OUT2 = Q");
  lv_obj_set_style_text_color(subtitle, lv_color_hex(APP_UI_COLOR_TEXT_SUB), 0);
  lv_obj_align_to(subtitle, title, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 8);

  g_section_title_label = lv_label_create(parent);
  lv_obj_set_style_text_color(g_section_title_label, lv_color_hex(APP_UI_COLOR_TEXT_MAIN), 0);
  lv_obj_align(g_section_title_label, LV_ALIGN_TOP_LEFT, 250, 88);
}

static void App_LvglUiCreateSectionButtons(lv_obj_t *parent)
{
  g_continuous_button = App_LvglUiCreateSectionButton(parent, "Single", 92, App_LvglUiOnContinuousClicked);
  g_sweep_section_button = App_LvglUiCreateSectionButton(parent, "Sweep", 156, App_LvglUiOnSweepSectionClicked);
  g_mod_button = App_LvglUiCreateSectionButton(parent, "Mod", 220, App_LvglUiOnModClicked);
  g_system_button = App_LvglUiCreateSectionButton(parent, "System", 284, App_LvglUiOnSystemClicked);
}

static lv_obj_t *App_LvglUiCreateSectionButton(lv_obj_t *parent,
                                               const char *text,
                                               int32_t y,
                                               lv_event_cb_t event_cb)
{
  lv_obj_t *button = lv_button_create(parent);
  lv_obj_t *label = lv_label_create(button);

  lv_obj_set_size(button, APP_UI_SECTION_BUTTON_W, APP_UI_SECTION_BUTTON_H);
  lv_obj_align(button, LV_ALIGN_TOP_LEFT, APP_UI_SECTION_BUTTON_X, y);
  lv_obj_add_event_cb(button, event_cb, LV_EVENT_CLICKED, NULL);
  lv_label_set_text(label, text);
  lv_obj_set_style_radius(button, 16, 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(APP_UI_COLOR_BUTTON_IDLE), 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_set_style_border_width(button, 0, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(APP_UI_COLOR_TEXT_MAIN), 0);
  lv_obj_center(label);

  return button;
}

static void App_LvglUiCreateModeRow(lv_obj_t *parent, AppDacWavegenMode mode, int32_t y)
{
  g_mode_name_label = lv_label_create(parent);

  lv_label_set_text(g_mode_name_label, "Mode");
  lv_obj_set_width(g_mode_name_label, APP_UI_LABEL_W);
  lv_obj_set_style_text_color(g_mode_name_label, lv_color_hex(APP_UI_COLOR_TEXT_MAIN), 0);
  lv_obj_align(g_mode_name_label, LV_ALIGN_TOP_LEFT, APP_UI_CONTENT_LABEL_X, y + 10);

  g_mode_dropdown = lv_dropdown_create(parent);
  lv_obj_set_width(g_mode_dropdown, 170);
  lv_dropdown_set_options(g_mode_dropdown, "AM\nFM\n2ASK\n2PSK\n2FSK");
  lv_dropdown_set_selected(g_mode_dropdown,
                           (uint16_t)((mode <= APP_DAC_WAVE_MODE_2FSK) ? mode : APP_DAC_WAVE_MODE_AM));
  lv_obj_align(g_mode_dropdown, LV_ALIGN_TOP_LEFT, APP_UI_CONTENT_VALUE_X, y);
  lv_obj_add_event_cb(g_mode_dropdown, App_LvglUiOnModeChanged, LV_EVENT_VALUE_CHANGED, NULL);
}

static void App_LvglUiCreateValueRow(lv_obj_t *parent,
                                     const char *name,
                                     int32_t y,
                                     lv_obj_t **name_label,
                                     lv_obj_t **value_label)
{
  *name_label = lv_label_create(parent);
  lv_label_set_text(*name_label, name);
  lv_obj_set_width(*name_label, APP_UI_LABEL_W);
  lv_obj_set_style_text_color(*name_label, lv_color_hex(APP_UI_COLOR_TEXT_MAIN), 0);
  lv_obj_align(*name_label, LV_ALIGN_TOP_LEFT, APP_UI_CONTENT_LABEL_X, y);

  *value_label = lv_label_create(parent);
  lv_obj_set_width(*value_label, 220);
  lv_obj_set_style_text_color(*value_label, lv_color_hex(APP_UI_COLOR_TEXT_VALUE), 0);
  lv_obj_align(*value_label, LV_ALIGN_TOP_LEFT, APP_UI_CONTENT_VALUE_X, y);
}

static void App_LvglUiCreatePresetBadge(lv_obj_t *parent)
{
  g_preset_name_label = lv_label_create(parent);
  lv_label_set_text(g_preset_name_label, "Preset");
  lv_obj_set_style_text_color(g_preset_name_label, lv_color_hex(APP_UI_COLOR_TEXT_SUB), 0);
  lv_obj_align(g_preset_name_label, LV_ALIGN_TOP_RIGHT, -180, 26);

  g_preset_value_label = lv_label_create(parent);
  lv_obj_set_width(g_preset_value_label, 150);
  lv_obj_set_style_text_align(g_preset_value_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_color(g_preset_value_label, lv_color_hex(APP_UI_COLOR_TEXT_VALUE), 0);
  lv_obj_align(g_preset_value_label, LV_ALIGN_TOP_RIGHT, -24, 26);
}

static void App_LvglUiCreateDebugBadge(lv_obj_t *parent)
{
  g_debug_status_label = lv_label_create(parent);
  lv_obj_set_style_text_color(g_debug_status_label, lv_color_hex(APP_UI_COLOR_TEXT_SUB), 0);
  lv_obj_align(g_debug_status_label, LV_ALIGN_TOP_RIGHT, -24, 58);

  g_debug_button = lv_button_create(parent);
  lv_obj_set_size(g_debug_button, 120, 34);
  lv_obj_align(g_debug_button, LV_ALIGN_TOP_RIGHT, -24, 84);
  lv_obj_add_event_cb(g_debug_button, App_LvglUiOnDebugClicked, LV_EVENT_CLICKED, NULL);

  g_debug_button_label = lv_label_create(g_debug_button);
  lv_obj_center(g_debug_button_label);
}

static void App_LvglUiCreateSelfTestBadge(lv_obj_t *parent)
{
  g_ad9959_status_label = lv_label_create(parent);
  lv_obj_set_style_text_color(g_ad9959_status_label, lv_color_hex(APP_UI_COLOR_TEXT_VALUE), 0);
  lv_obj_align(g_ad9959_status_label, LV_ALIGN_TOP_RIGHT, -24, 124);

  g_f429_status_label = lv_label_create(parent);
  lv_obj_set_style_text_color(g_f429_status_label, lv_color_hex(APP_UI_COLOR_TEXT_VALUE), 0);
  lv_obj_align(g_f429_status_label, LV_ALIGN_TOP_RIGHT, -24, 148);
}

static void App_LvglUiCreateSweepTimeBadge(lv_obj_t *parent)
{
  g_sweep_time_name_label = lv_label_create(parent);
  lv_label_set_text(g_sweep_time_name_label, "Sweep");
  lv_obj_set_style_text_color(g_sweep_time_name_label, lv_color_hex(APP_UI_COLOR_TEXT_SUB), 0);
  lv_obj_align(g_sweep_time_name_label, LV_ALIGN_TOP_RIGHT, -180, 136);

  g_sweep_time_value_label = lv_label_create(parent);
  lv_obj_set_width(g_sweep_time_value_label, 150);
  lv_obj_set_style_text_align(g_sweep_time_value_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_color(g_sweep_time_value_label, lv_color_hex(APP_UI_COLOR_TEXT_VALUE), 0);
  lv_obj_align(g_sweep_time_value_label, LV_ALIGN_TOP_RIGHT, -24, 136);
}

static void App_LvglUiCreateSweepRangeBadge(lv_obj_t *parent)
{
  g_sweep_start_name_label = lv_label_create(parent);
  lv_label_set_text(g_sweep_start_name_label, "S-Start");
  lv_obj_set_style_text_color(g_sweep_start_name_label, lv_color_hex(APP_UI_COLOR_TEXT_SUB), 0);
  lv_obj_align(g_sweep_start_name_label, LV_ALIGN_TOP_RIGHT, -180, 162);

  g_sweep_start_value_label = lv_label_create(parent);
  lv_obj_set_width(g_sweep_start_value_label, 150);
  lv_obj_set_style_text_align(g_sweep_start_value_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_color(g_sweep_start_value_label, lv_color_hex(APP_UI_COLOR_TEXT_VALUE), 0);
  lv_obj_align(g_sweep_start_value_label, LV_ALIGN_TOP_RIGHT, -24, 162);

  g_sweep_stop_name_label = lv_label_create(parent);
  lv_label_set_text(g_sweep_stop_name_label, "S-Stop");
  lv_obj_set_style_text_color(g_sweep_stop_name_label, lv_color_hex(APP_UI_COLOR_TEXT_SUB), 0);
  lv_obj_align(g_sweep_stop_name_label, LV_ALIGN_TOP_RIGHT, -180, 188);

  g_sweep_stop_value_label = lv_label_create(parent);
  lv_obj_set_width(g_sweep_stop_value_label, 150);
  lv_obj_set_style_text_align(g_sweep_stop_value_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_color(g_sweep_stop_value_label, lv_color_hex(APP_UI_COLOR_TEXT_VALUE), 0);
  lv_obj_align(g_sweep_stop_value_label, LV_ALIGN_TOP_RIGHT, -24, 188);
}

static void App_LvglUiCreateButtons(lv_obj_t *parent)
{
  g_field_button = lv_button_create(parent);
  {
    lv_obj_t *field_label = lv_label_create(g_field_button);
    lv_obj_set_size(g_field_button, 88, 44);
    lv_obj_align(g_field_button, LV_ALIGN_BOTTOM_LEFT, 214, -24);
    lv_obj_add_event_cb(g_field_button, App_LvglUiOnFieldClicked, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(field_label, "FREQ");
    lv_obj_center(field_label);
  }
  g_digit_button = lv_button_create(parent);
  {
    lv_obj_t *digit_label = lv_label_create(g_digit_button);
    lv_obj_set_size(g_digit_button, 88, 44);
    lv_obj_align(g_digit_button, LV_ALIGN_BOTTOM_LEFT, 314, -24);
    lv_obj_add_event_cb(g_digit_button, App_LvglUiOnDigitClicked, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(digit_label, "AMP");
    lv_obj_center(digit_label);
  }
  g_dec_button = lv_button_create(parent);
  {
    lv_obj_t *dec_label = lv_label_create(g_dec_button);
    lv_obj_set_size(g_dec_button, 76, 44);
    lv_obj_align(g_dec_button, LV_ALIGN_BOTTOM_LEFT, 414, -24);
    lv_obj_add_event_cb(g_dec_button, App_LvglUiOnDecClicked, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(dec_label, "-");
    lv_obj_center(dec_label);
  }
  g_inc_button = lv_button_create(parent);
  {
    lv_obj_t *inc_label = lv_label_create(g_inc_button);
    lv_obj_set_size(g_inc_button, 76, 44);
    lv_obj_align(g_inc_button, LV_ALIGN_BOTTOM_LEFT, 502, -24);
    lv_obj_add_event_cb(g_inc_button, App_LvglUiOnIncClicked, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(inc_label, "+");
    lv_obj_center(inc_label);
  }
  lv_obj_t *preset_save_label;
  lv_obj_t *preset_recall_label;
  lv_obj_t *sweep_time_label;
  lv_obj_t *sweep_start_label;
  lv_obj_t *sweep_stop_label;

  g_edit_label = lv_label_create(parent);
  lv_obj_set_width(g_edit_label, 360);
  lv_obj_set_style_text_color(g_edit_label, lv_color_hex(APP_UI_COLOR_TEXT_SUB), 0);
  lv_obj_align(g_edit_label, LV_ALIGN_BOTTOM_LEFT, 244, -86);

  g_state_label = lv_label_create(parent);
  lv_obj_set_width(g_state_label, 220);
  lv_obj_set_style_text_color(g_state_label, lv_color_hex(APP_UI_COLOR_TEXT_SUB), 0);
  lv_obj_align(g_state_label, LV_ALIGN_BOTTOM_RIGHT, -44, -86);

  g_sweep_button = lv_button_create(parent);
  lv_obj_set_size(g_sweep_button, 92, 34);
  lv_obj_align(g_sweep_button, LV_ALIGN_TOP_RIGHT, -24, 238);
  lv_obj_add_event_cb(g_sweep_button, App_LvglUiOnSweepClicked, LV_EVENT_CLICKED, NULL);
  g_sweep_button_label = lv_label_create(g_sweep_button);
  lv_obj_center(g_sweep_button_label);

  g_sweep_time_button = lv_button_create(parent);
  sweep_time_label = lv_label_create(g_sweep_time_button);
  lv_obj_set_size(g_sweep_time_button, 92, 34);
  lv_obj_align(g_sweep_time_button, LV_ALIGN_TOP_RIGHT, -124, 238);
  lv_obj_add_event_cb(g_sweep_time_button, App_LvglUiOnSweepTimeClicked, LV_EVENT_CLICKED, NULL);
  lv_label_set_text(sweep_time_label, "Time");
  lv_obj_center(sweep_time_label);

  g_sweep_start_button = lv_button_create(parent);
  sweep_start_label = lv_label_create(g_sweep_start_button);
  lv_obj_set_size(g_sweep_start_button, 92, 34);
  lv_obj_align(g_sweep_start_button, LV_ALIGN_TOP_RIGHT, -24, 280);
  lv_obj_add_event_cb(g_sweep_start_button, App_LvglUiOnSweepStartClicked, LV_EVENT_CLICKED, NULL);
  lv_label_set_text(sweep_start_label, "SetStart");
  lv_obj_center(sweep_start_label);

  g_sweep_stop_button = lv_button_create(parent);
  sweep_stop_label = lv_label_create(g_sweep_stop_button);
  lv_obj_set_size(g_sweep_stop_button, 92, 34);
  lv_obj_align(g_sweep_stop_button, LV_ALIGN_TOP_RIGHT, -124, 280);
  lv_obj_add_event_cb(g_sweep_stop_button, App_LvglUiOnSweepStopClicked, LV_EVENT_CLICKED, NULL);
  lv_label_set_text(sweep_stop_label, "SetStop");
  lv_obj_center(sweep_stop_label);

  g_preset_save_button = lv_button_create(parent);
  preset_save_label = lv_label_create(g_preset_save_button);
  lv_obj_set_size(g_preset_save_button, 82, 44);
  lv_obj_align(g_preset_save_button, LV_ALIGN_BOTTOM_LEFT, 590, -24);
  lv_obj_add_event_cb(g_preset_save_button, App_LvglUiOnPresetSaveClicked, LV_EVENT_CLICKED, NULL);
  lv_label_set_text(preset_save_label, "Save");
  lv_obj_center(preset_save_label);

  g_preset_recall_button = lv_button_create(parent);
  preset_recall_label = lv_label_create(g_preset_recall_button);
  lv_obj_set_size(g_preset_recall_button, 82, 44);
  lv_obj_align(g_preset_recall_button, LV_ALIGN_BOTTOM_LEFT, 684, -24);
  lv_obj_add_event_cb(g_preset_recall_button, App_LvglUiOnPresetRecallClicked, LV_EVENT_CLICKED, NULL);
  lv_label_set_text(preset_recall_label, "Recall");
  lv_obj_center(preset_recall_label);

  g_apply_button = lv_button_create(parent);
  {
    lv_obj_t *apply_label = lv_label_create(g_apply_button);
    lv_obj_set_size(g_apply_button, 88, 44);
    lv_obj_align(g_apply_button, LV_ALIGN_BOTTOM_LEFT, 590, -24);
    lv_obj_add_event_cb(g_apply_button, App_LvglUiOnApplyClicked, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(apply_label, "APPLY");
    lv_obj_center(apply_label);
  }

  g_run_button = lv_button_create(parent);
  lv_obj_set_size(g_run_button, 86, 44);
  lv_obj_align(g_run_button, LV_ALIGN_BOTTOM_LEFT, 688, -24);
  lv_obj_add_event_cb(g_run_button, App_LvglUiOnRunClicked, LV_EVENT_CLICKED, NULL);
  g_run_button_label = lv_label_create(g_run_button);
  lv_obj_center(g_run_button_label);
}

static lv_obj_t *App_LvglUiCreateActionButton(lv_obj_t *parent,
                                              const char *text,
                                              int32_t x,
                                              int32_t y,
                                              int32_t w,
                                              int32_t h,
                                              lv_event_cb_t event_cb,
                                              void *user_data)
{
  lv_obj_t *button = lv_button_create(parent);
  lv_obj_t *label = lv_label_create(button);

  lv_obj_set_size(button, w, h);
  lv_obj_align(button, LV_ALIGN_TOP_LEFT, x, y);
  lv_obj_add_event_cb(button, event_cb, LV_EVENT_CLICKED, user_data);
  lv_obj_set_style_radius(button, 12, 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(APP_UI_COLOR_BUTTON_IDLE), 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(button, 1, 0);
  lv_obj_set_style_border_color(button, lv_color_hex(APP_UI_COLOR_PANEL_BORDER), 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_hex(APP_UI_COLOR_TEXT_MAIN), 0);
  lv_obj_center(label);

  return button;
}

static void App_LvglUiCreateKeypad(lv_obj_t *parent)
{
  static const char *digit_texts[] = {"7", "8", "9", "4", "5", "6", "1", "2", "3", "0", "."};
  int32_t i;

  g_keypad_overlay = lv_obj_create(parent);
  lv_obj_set_size(g_keypad_overlay, APP_UI_PANEL_W, APP_UI_PANEL_H);
  lv_obj_align(g_keypad_overlay, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(g_keypad_overlay, lv_color_hex(0x0C131D), 0);
  lv_obj_set_style_bg_opa(g_keypad_overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_keypad_overlay, 0, 0);
  lv_obj_set_style_radius(g_keypad_overlay, 8, 0);
  lv_obj_set_style_shadow_width(g_keypad_overlay, 0, 0);
  lv_obj_remove_flag(g_keypad_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(g_keypad_overlay, LV_OBJ_FLAG_HIDDEN);

  g_keypad_title_label = lv_label_create(g_keypad_overlay);
  lv_obj_set_style_text_color(g_keypad_title_label, lv_color_hex(APP_UI_COLOR_TEXT_MAIN), 0);
  lv_obj_align(g_keypad_title_label, LV_ALIGN_TOP_LEFT, 34, 24);

  g_keypad_unit_label = lv_label_create(g_keypad_overlay);
  lv_obj_set_style_text_color(g_keypad_unit_label, lv_color_hex(APP_UI_COLOR_TEXT_SUB), 0);
  lv_obj_align(g_keypad_unit_label, LV_ALIGN_TOP_RIGHT, -34, 24);

  g_keypad_current_label = lv_label_create(g_keypad_overlay);
  lv_obj_set_width(g_keypad_current_label, 520);
  lv_obj_set_style_text_color(g_keypad_current_label, lv_color_hex(APP_UI_COLOR_TEXT_SUB), 0);
  lv_obj_align(g_keypad_current_label, LV_ALIGN_TOP_LEFT, 34, 56);

  g_keypad_value_label = lv_label_create(g_keypad_overlay);
  lv_obj_set_width(g_keypad_value_label, LV_SIZE_CONTENT);
  lv_obj_set_style_text_color(g_keypad_value_label, lv_color_hex(APP_UI_COLOR_ACCENT), 0);
  lv_obj_set_style_text_align(g_keypad_value_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_align(g_keypad_value_label, LV_ALIGN_TOP_LEFT, 150, 98);

  g_keypad_panel = g_keypad_overlay;

  for (i = 0; i < 11; ++i)
  {
    int32_t col;
    int32_t row;
    int32_t x;
    int32_t y;

    if (i < 9)
    {
      row = i / 3;
      col = i % 3;
    }
    else
    {
      row = 3;
      col = (i == 9) ? 1 : 2;
    }

    x = 132 + (col * 98);
    y = 148 + (row * 64);
    if (i == 10)
    {
      g_keypad_dot_button = App_LvglUiCreateActionButton(g_keypad_panel,
                                                         digit_texts[i],
                                                         x,
                                                         y,
                                                         82,
                                                         54,
                                                         App_LvglUiOnKeypadDigitClicked,
                                                         (void *)digit_texts[i]);
    }
    else
    {
      (void)App_LvglUiCreateActionButton(g_keypad_panel,
                                         digit_texts[i],
                                         x,
                                         y,
                                         82,
                                         54,
                                         App_LvglUiOnKeypadDigitClicked,
                                         (void *)digit_texts[i]);
    }
  }

  (void)App_LvglUiCreateActionButton(g_keypad_panel, "Back",   464, 148, 156, 54, App_LvglUiOnKeypadBackClicked, NULL);
  (void)App_LvglUiCreateActionButton(g_keypad_panel, "Cancel", 464, 212, 156, 54, App_LvglUiOnKeypadCancelClicked, NULL);
  (void)App_LvglUiCreateActionButton(g_keypad_panel, "OK",     464, 276, 156, 54, App_LvglUiOnKeypadOkClicked, NULL);
  (void)App_LvglUiCreateActionButton(g_keypad_panel, "Del",    464, 340, 156, 54, App_LvglUiOnKeypadDeleteClicked, NULL);
}

static void App_LvglUiResetModeConfig(AppDacWavegenMode mode)
{
  AppDacWavegenConfig defaults;
  uint16_t keep_vpp = g_ui_config.vpp_mv;

  App_DacWavegenSetDefaultConfig(&defaults);
  defaults.mode = mode;
  defaults.vpp_mv = keep_vpp;
  g_ui_config = defaults;
}

static uint8_t App_LvglUiIsCwMode(AppDacWavegenMode mode)
{
  return (mode == APP_DAC_WAVE_MODE_CW) ? 1U : 0U;
}

static void App_LvglUiEnsureEditableField(void)
{
  uint32_t tries = 0U;

  while ((tries < APP_UI_FIELD_COUNT) && (App_LvglUiIsFieldEditable(g_selected_field) == 0U))
  {
    g_selected_field = (AppUiEditField)((g_selected_field + 1U) % APP_UI_FIELD_COUNT);
    tries++;
  }

  if (g_digit_index >= App_LvglUiGetFieldStepCount(g_selected_field))
  {
    g_digit_index = 0U;
  }
}

static void App_LvglUiSelectField(AppUiEditField field)
{
  g_selected_field = field;

  if (g_digit_index >= App_LvglUiGetFieldStepCount(g_selected_field))
  {
    g_digit_index = 0U;
  }

  App_LvglUiEnsureEditableField();
  App_LvglUiRefreshValues();
  App_LvglUiRefreshEditState();
}

static void App_LvglUiRefreshValues(void)
{
  lv_color_t normal_name = lv_color_hex(APP_UI_COLOR_TEXT_MAIN);
  lv_color_t normal_value = lv_color_hex(APP_UI_COLOR_TEXT_VALUE);
  lv_color_t selected = lv_color_hex(APP_UI_COLOR_ACCENT);
  AppDacWavegenMode mode = g_ui_config.mode;

  App_LvglUiRefreshModeVisibility();
  App_LvglUiRefreshSectionVisibility();
  if (g_section_title_label != NULL)
  {
    lv_label_set_text(g_section_title_label, App_LvglUiSectionTitle());
  }
  if (g_active_section == APP_UI_SECTION_SYSTEM)
  {
    lv_label_set_text(g_lo_name_label, "Cal freq");
    lv_label_set_text(g_lo_value_label, "120.000000 MHz");
    lv_label_set_text(g_vpp_name_label, "CH1 amp");
    lv_label_set_text(g_vpp_value_label, "1023");
    lv_label_set_text(g_rf_name_label, "Error freq");
    lv_label_set_text_fmt(g_rf_value_label, "%ld Hz", (long)g_system_lo_error_hz);
    lv_label_set_text(g_rate_name_label, "Step");
    lv_label_set_text_fmt(g_rate_value_label, "%lu Hz", (unsigned long)App_LvglUiGetSystemErrorStepHz());
    lv_obj_set_style_text_color(g_lo_name_label, normal_name, 0);
    lv_obj_set_style_text_color(g_lo_value_label, normal_value, 0);
    lv_obj_set_style_text_color(g_vpp_name_label, normal_name, 0);
    lv_obj_set_style_text_color(g_vpp_value_label, normal_value, 0);
    lv_obj_set_style_text_color(g_rf_name_label, selected, 0);
    lv_obj_set_style_text_color(g_rf_value_label, selected, 0);
    lv_obj_set_style_text_color(g_rate_name_label, normal_name, 0);
    lv_obj_set_style_text_color(g_rate_value_label, normal_value, 0);
    return;
  }
  if (g_mode_dropdown != NULL)
  {
    lv_dropdown_set_selected(g_mode_dropdown,
                             (uint16_t)((mode <= APP_DAC_WAVE_MODE_2FSK) ? mode : APP_DAC_WAVE_MODE_AM));
  }

  lv_label_set_text_fmt(g_lo_value_label, "%lu.%06lu MHz",
                        (unsigned long)(g_lo_freq_hz / 1000000UL),
                        (unsigned long)(g_lo_freq_hz % 1000000UL));
  lv_label_set_text_fmt(g_preset_value_label, "%lu.%06lu MHz",
                        (unsigned long)(g_preset_freq_hz / 1000000UL),
                        (unsigned long)(g_preset_freq_hz % 1000000UL));
  lv_label_set_text_fmt(g_sweep_time_value_label, "%lu.%01lu s",
                        (unsigned long)(g_sweep_period_ms / 1000UL),
                        (unsigned long)((g_sweep_period_ms % 1000UL) / 100UL));
  lv_label_set_text_fmt(g_sweep_start_value_label, "%lu.%06lu MHz",
                        (unsigned long)(g_sweep_start_hz / 1000000UL),
                        (unsigned long)(g_sweep_start_hz % 1000000UL));
  lv_label_set_text_fmt(g_sweep_stop_value_label, "%lu.%06lu MHz",
                        (unsigned long)(g_sweep_stop_hz / 1000000UL),
                        (unsigned long)(g_sweep_stop_hz % 1000000UL));
  lv_label_set_text(g_sweep_start_name_label, "Start freq");
  lv_label_set_text(g_sweep_stop_name_label, "Stop freq");
  lv_label_set_text(g_sweep_time_name_label, "Time");
  lv_label_set_text(g_lo_name_label, (mode == APP_DAC_WAVE_MODE_2FSK) ? "Init freq" : "FC");
  if (App_LvglUiIsCwMode(mode) != 0U)
  {
    lv_label_set_text(g_vpp_name_label,
                      (g_active_section == APP_UI_SECTION_SWEEP) ? "Target RMS" : "Amp");
    lv_label_set_text_fmt(g_vpp_value_label, "%u mVrms",
                          (unsigned)App_LvglUiInternalToAmpRmsMv(g_ui_config.vpp_mv));
    lv_label_set_text(g_rate_name_label, "Rate");
    lv_label_set_text(g_rate_value_label, "N/A");
    lv_label_set_text(g_param_name_label, "Param");
    lv_label_set_text(g_param_value_label, "N/A");
  }
  else
  {
    lv_label_set_text_fmt(g_vpp_value_label, "%u mVrms",
                          (unsigned)App_LvglUiInternalToAmpRmsMv(g_ui_config.vpp_mv));

    if ((mode == APP_DAC_WAVE_MODE_AM) || (mode == APP_DAC_WAVE_MODE_FM))
    {
      lv_label_set_text(g_rate_name_label, "Mod freq");
      lv_label_set_text_fmt(g_rate_value_label, "%lu Hz", (unsigned long)g_ui_config.mod_freq_hz);
    }
    else
    {
      lv_label_set_text(g_rate_name_label, "Bit rate");
      lv_label_set_text_fmt(g_rate_value_label, "%lu bps", (unsigned long)g_ui_config.symbol_rate_bps);
    }

    switch (mode)
    {
      case APP_DAC_WAVE_MODE_AM:
        lv_label_set_text(g_param_name_label, "Mod depth");
        lv_label_set_text_fmt(g_param_value_label, "%u %%", (unsigned)g_ui_config.am_depth_percent);
        break;
      case APP_DAC_WAVE_MODE_FM:
        lv_label_set_text(g_param_name_label, "Deviation");
        lv_label_set_text_fmt(g_param_value_label, "%lu Hz", (unsigned long)g_ui_config.fm_deviation_hz);
        break;
      case APP_DAC_WAVE_MODE_2ASK:
        lv_label_set_text(g_param_name_label, "Low amp");
        lv_label_set_text(g_param_value_label, "0");
        break;
      case APP_DAC_WAVE_MODE_2PSK:
        lv_label_set_text(g_param_name_label, "Phase");
        lv_label_set_text(g_param_value_label, "180 deg");
        break;
      case APP_DAC_WAVE_MODE_2FSK:
      default:
        lv_label_set_text(g_param_name_label, "Hop freq");
        lv_label_set_text_fmt(g_param_value_label, "%lu.%06lu MHz",
                              (unsigned long)(App_LvglUiGetFskNextFrequencyHz() / 1000000UL),
                              (unsigned long)(App_LvglUiGetFskNextFrequencyHz() % 1000000UL));
        break;
    }
  }

  lv_obj_set_style_text_color(g_lo_name_label, normal_name, 0);
  lv_obj_set_style_text_color(g_lo_value_label, normal_value, 0);
  lv_obj_set_style_text_color(g_rf_name_label, normal_name, 0);
  lv_obj_set_style_text_color(g_rf_value_label, normal_value, 0);
  lv_obj_set_style_text_color(g_preset_name_label, normal_name, 0);
  lv_obj_set_style_text_color(g_preset_value_label, normal_value, 0);
  lv_obj_set_style_text_color(g_sweep_time_name_label, normal_name, 0);
  lv_obj_set_style_text_color(g_sweep_time_value_label, normal_value, 0);
  lv_obj_set_style_text_color(g_sweep_start_name_label, normal_name, 0);
  lv_obj_set_style_text_color(g_sweep_start_value_label, normal_value, 0);
  lv_obj_set_style_text_color(g_sweep_stop_name_label, normal_name, 0);
  lv_obj_set_style_text_color(g_sweep_stop_value_label, normal_value, 0);
  lv_obj_set_style_text_color(g_vpp_name_label, normal_name, 0);
  lv_obj_set_style_text_color(g_vpp_value_label, normal_value, 0);
  lv_obj_set_style_text_color(g_rate_name_label, (App_LvglUiIsCwMode(mode) != 0U) ? lv_color_hex(APP_UI_COLOR_TEXT_MUTED) : normal_name, 0);
  lv_obj_set_style_text_color(g_rate_value_label, (App_LvglUiIsCwMode(mode) != 0U) ? lv_color_hex(APP_UI_COLOR_TEXT_MUTED) : normal_value, 0);
  lv_obj_set_style_text_color(g_param_name_label, (App_LvglUiIsCwMode(mode) != 0U) ? lv_color_hex(APP_UI_COLOR_TEXT_MUTED) : normal_name, 0);
  lv_obj_set_style_text_color(g_param_value_label, (App_LvglUiIsCwMode(mode) != 0U) ? lv_color_hex(APP_UI_COLOR_TEXT_MUTED) : normal_value, 0);

  switch (g_selected_field)
  {
    case APP_UI_FIELD_LO_FREQ:
      lv_obj_set_style_text_color(g_lo_name_label, selected, 0);
      lv_obj_set_style_text_color(g_lo_value_label, selected, 0);
      break;
    case APP_UI_FIELD_VPP:
      lv_obj_set_style_text_color(g_vpp_name_label, selected, 0);
      lv_obj_set_style_text_color(g_vpp_value_label, selected, 0);
      break;
    case APP_UI_FIELD_RATE:
      lv_obj_set_style_text_color(g_rate_name_label, selected, 0);
      lv_obj_set_style_text_color(g_rate_value_label, selected, 0);
      break;
    case APP_UI_FIELD_PARAM:
      lv_obj_set_style_text_color(g_param_name_label, selected, 0);
      lv_obj_set_style_text_color(g_param_value_label, selected, 0);
      break;
    default:
      break;
  }
}

static void App_LvglUiRefreshEditState(void)
{
  uint32_t step = App_LvglUiGetFieldStepValue(g_selected_field, g_digit_index);

  if (g_active_section == APP_UI_SECTION_SYSTEM)
  {
    lv_label_set_text_fmt(g_edit_label, "Cal step %lu Hz", (unsigned long)App_LvglUiGetSystemErrorStepHz());
    return;
  }

  if (App_LvglUiIsCwMode(g_ui_config.mode) != 0U)
  {
    if (g_selected_field == APP_UI_FIELD_LO_FREQ)
    {
      lv_label_set_text_fmt(g_edit_label, "Edit: %s / Step %lu.%06lu MHz",
                            App_LvglUiFieldName(g_selected_field),
                            (unsigned long)(step / 1000000UL),
                            (unsigned long)(step % 1000000UL));
    }
    else if (g_selected_field == APP_UI_FIELD_VPP)
    {
      lv_label_set_text_fmt(g_edit_label, "Edit: %s / Step %lu mVrms",
                            App_LvglUiFieldName(g_selected_field),
                            (unsigned long)step);
    }
    else
    {
      lv_label_set_text_fmt(g_edit_label, "Edit: %s",
                            App_LvglUiFieldName(g_selected_field));
    }
    return;
  }

  if (g_selected_field == APP_UI_FIELD_LO_FREQ)
  {
    lv_label_set_text_fmt(g_edit_label, "Edit: %s / Step %lu.%06lu MHz",
                          App_LvglUiFieldName(g_selected_field),
                          (unsigned long)(step / 1000000UL),
                          (unsigned long)(step % 1000000UL));
  }
  else if (g_selected_field == APP_UI_FIELD_VPP)
  {
    lv_label_set_text_fmt(g_edit_label, "Edit: %s / Step %lu mVrms",
                          App_LvglUiFieldName(g_selected_field),
                          (unsigned long)step);
  }
  else if (g_selected_field == APP_UI_FIELD_PARAM)
  {
    if (g_ui_config.mode == APP_DAC_WAVE_MODE_AM)
    {
      lv_label_set_text_fmt(g_edit_label, "Edit: %s / Step %lu %%",
                            App_LvglUiFieldName(g_selected_field),
                            (unsigned long)step);
    }
    else if (g_ui_config.mode == APP_DAC_WAVE_MODE_2FSK)
    {
      lv_label_set_text_fmt(g_edit_label, "Edit: %s / Step %lu.%06lu MHz",
                            App_LvglUiFieldName(g_selected_field),
                            (unsigned long)(step / 1000000UL),
                            (unsigned long)(step % 1000000UL));
    }
    else
    {
      lv_label_set_text_fmt(g_edit_label, "Edit: %s / Step %lu Hz",
                            App_LvglUiFieldName(g_selected_field),
                            (unsigned long)step);
    }
  }
  else if (g_selected_field == APP_UI_FIELD_RATE)
  {
    if ((g_ui_config.mode == APP_DAC_WAVE_MODE_AM) || (g_ui_config.mode == APP_DAC_WAVE_MODE_FM))
    {
      lv_label_set_text_fmt(g_edit_label, "Edit: %s / Step %lu Hz",
                            App_LvglUiFieldName(g_selected_field),
                            (unsigned long)step);
    }
    else
    {
      lv_label_set_text_fmt(g_edit_label, "Edit: %s / Step %lu bps",
                            App_LvglUiFieldName(g_selected_field),
                            (unsigned long)step);
    }
  }
  else
  {
    lv_label_set_text_fmt(g_edit_label, "Edit: %s / Step %lu",
                          App_LvglUiFieldName(g_selected_field),
                          (unsigned long)step);
  }
}

static void App_LvglUiRefreshState(void)
{
  AppTxControlSnapshot snapshot;
  uint8_t debug_locked;
  const AppDdsStatus *dds_status;

  App_TxControl_GetSnapshot(&snapshot);
  if (g_pending_tx_on_valid != 0U)
  {
    snapshot.basic.tx_on = g_pending_tx_on;
  }
  if (g_pending_sweep_on_valid != 0U)
  {
    snapshot.basic.sweep_on = g_pending_sweep_on;
  }
  debug_locked = App_LvglUiIsDebugLocked();
  dds_status = AppDDS_GetStatus();
  App_LvglUiRefreshSectionButtons(&snapshot, debug_locked);

  if (g_active_section == APP_UI_SECTION_SYSTEM)
  {
    if (g_run_button_label != NULL)
    {
      lv_label_set_text(g_run_button_label,
                        (((g_pending_calibration_on_valid != 0U) ? g_pending_calibration_on :
                           App_TxControl_GetCalibrationOutputEnabled()) != 0U) ? "STOP" : "START");
    }
    lv_label_set_text(g_state_label,
                      (((g_pending_calibration_on_valid != 0U) ? g_pending_calibration_on :
                         App_TxControl_GetCalibrationOutputEnabled()) != 0U) ? "Calibration output ON" : "Calibration output OFF");
    return;
  }

  if (debug_locked != 0U)
  {
    if (snapshot.basic.sweep_on != 0U)
    {
      lv_label_set_text_fmt(g_state_label, "DEBUG %s sweep", App_LvglUiModeText(snapshot.basic.mode));
    }
    else
    {
      lv_label_set_text_fmt(g_state_label, "DEBUG %s", App_LvglUiModeText(snapshot.basic.mode));
    }

    if (g_run_button_label != NULL)
    {
      lv_label_set_text(g_run_button_label, (snapshot.basic.tx_on != 0U) ? "Stop" : "Start");
    }
  }
  else
  {
    if (snapshot.basic.sweep_on != 0U)
    {
      lv_label_set_text_fmt(g_state_label, "%s sweep", App_LvglUiModeText(snapshot.basic.mode));
    }
    else if (App_LvglUiIsCwMode(g_ui_config.mode) != 0U)
    {
      if (snapshot.basic.tx_on != 0U)
      {
        lv_label_set_text(g_state_label, "CW running");
        lv_label_set_text(g_run_button_label, "Stop");
      }
      else
      {
        lv_label_set_text(g_state_label, "CW stopped");
        lv_label_set_text(g_run_button_label, "Start");
      }
    }
    else if (snapshot.basic.tx_on != 0U)
    {
      lv_label_set_text_fmt(g_state_label, "%s running", App_LvglUiModeText(snapshot.basic.mode));
      lv_label_set_text(g_run_button_label, "Stop");
    }
    else
    {
      lv_label_set_text(g_state_label, "Stopped");
      lv_label_set_text(g_run_button_label, "Start");
    }
  }

  if ((g_active_section == APP_UI_SECTION_SINGLE) && (g_run_button_label != NULL))
  {
    lv_label_set_text(g_run_button_label, (snapshot.basic.tx_on != 0U) ? "STOP" : "START");
    lv_label_set_text(g_rf_name_label, "RF out");
    lv_label_set_text(g_rf_value_label, (snapshot.basic.tx_on != 0U) ? "ON" : "OFF");
  }
  else if ((g_active_section == APP_UI_SECTION_SWEEP) && (g_run_button_label != NULL))
  {
    lv_label_set_text(g_run_button_label, "SWEEP OUT");
    lv_label_set_text(g_rf_name_label, "Sweep out");
    lv_label_set_text(g_rf_value_label, (snapshot.basic.sweep_on != 0U) ? "START" : "OFF");
  }
  else if ((g_active_section == APP_UI_SECTION_MOD) && (g_run_button_label != NULL))
  {
    lv_label_set_text(g_run_button_label, "MOD OUT");
    lv_label_set_text(g_rf_name_label, "Mod out");
    lv_label_set_text(g_rf_value_label, (snapshot.basic.tx_on != 0U) ? "START" : "OFF");
  }

  if (g_sweep_button_label != NULL)
  {
    lv_label_set_text(g_sweep_button_label, (snapshot.basic.sweep_on != 0U) ? "SweepOff" : "SweepOn");
  }

  if (g_debug_status_label != NULL)
  {
    lv_label_set_text_fmt(g_debug_status_label,
                          "Debug: %s",
                          (debug_locked != 0U) ? "ON" : "OFF");
  }

  if (g_debug_button_label != NULL)
  {
    lv_label_set_text(g_debug_button_label, (debug_locked != 0U) ? "DebugOff" : "DebugOn");
  }

  if (g_ad9959_status_label != NULL)
  {
    lv_label_set_text_fmt(g_ad9959_status_label,
                          "AD9959: %s",
                          ((dds_status != NULL) && (dds_status->hw_ready != 0U)) ? "OK" : "FAIL");
  }

  if (g_f429_status_label != NULL)
  {
    lv_label_set_text_fmt(g_f429_status_label,
                          "F429: %s",
                          (App_WinnerBridge_GetLastSelfTestOk() != 0U) ? "OK" : "FAIL");
  }
}

static void App_LvglUiRefreshSectionButtons(const AppTxControlSnapshot *snapshot, uint8_t debug_locked)
{
  lv_obj_t *active_button = NULL;
  lv_obj_t *buttons[] = {
    g_continuous_button,
    g_sweep_section_button,
    g_mod_button,
    g_system_button
  };
  uint32_t i;

  (void)snapshot;
  (void)debug_locked;

  switch (g_active_section)
  {
    case APP_UI_SECTION_SINGLE:
      active_button = g_continuous_button;
      break;
    case APP_UI_SECTION_SWEEP:
      active_button = g_sweep_section_button;
      break;
    case APP_UI_SECTION_MOD:
      active_button = g_mod_button;
      break;
    case APP_UI_SECTION_SYSTEM:
    default:
      active_button = g_system_button;
      break;
  }

  for (i = 0U; i < (uint32_t)(sizeof(buttons) / sizeof(buttons[0])); i++)
  {
    if (buttons[i] == NULL)
    {
      continue;
    }

    if (buttons[i] == active_button)
    {
      lv_obj_set_style_bg_color(buttons[i], lv_color_hex(APP_UI_COLOR_BUTTON_ON), 0);
      lv_obj_set_style_border_color(buttons[i], lv_color_hex(APP_UI_COLOR_ACCENT), 0);
      lv_obj_set_style_border_width(buttons[i], 2, 0);
    }
    else
    {
      lv_obj_set_style_bg_color(buttons[i], lv_color_hex(APP_UI_COLOR_BUTTON_IDLE), 0);
      lv_obj_set_style_border_width(buttons[i], 0, 0);
    }
  }
}

static void App_LvglUiSetActiveSection(AppUiSection section)
{
  g_active_section = section;
}

static const char *App_LvglUiSectionTitle(void)
{
  switch (g_active_section)
  {
    case APP_UI_SECTION_SINGLE:
      return "";
    case APP_UI_SECTION_SWEEP:
      return "";
    case APP_UI_SECTION_MOD:
      return "Modulation";
    case APP_UI_SECTION_SYSTEM:
    default:
      return "Calibration";
  }
}

static void App_LvglUiProcessPendingAction(void)
{
  AppUiPendingAction action = g_pending_action;

  if (action == APP_UI_PENDING_ACTION_NONE)
  {
    return;
  }

  g_pending_action = APP_UI_PENDING_ACTION_NONE;

  switch (action)
  {
    case APP_UI_PENDING_ACTION_SINGLE_RUN:
      (void)App_TxControl_SetConfig(&g_ui_config);
      (void)App_TxControl_SetFrequencyHz(g_lo_freq_hz);
      if (g_pending_tx_on != 0U)
      {
        (void)App_TxControl_Start();
        (void)App_LvglUiSendCurrentModeToWinner();
      }
      else
      {
        (void)App_TxControl_Stop();
      }
      g_pending_tx_on_valid = 0U;
      break;
    case APP_UI_PENDING_ACTION_SWEEP_RUN:
      (void)App_TxControl_SetConfig(&g_ui_config);
      if (g_pending_sweep_on != 0U)
      {
        (void)App_TxControl_SetSweepEnabled(1U);
        (void)App_TxControl_Start();
      }
      else
      {
        (void)App_TxControl_SetSweepEnabled(0U);
        (void)App_TxControl_Stop();
      }
      g_pending_sweep_on_valid = 0U;
      break;
    case APP_UI_PENDING_ACTION_MOD_RUN:
      (void)App_TxControl_SetConfig(&g_ui_config);
      (void)App_TxControl_SetFrequencyHz(g_lo_freq_hz);
      if (g_pending_tx_on != 0U)
      {
        (void)App_TxControl_Start();
        (void)App_LvglUiSendCurrentModeToWinner();
      }
      else
      {
        (void)App_TxControl_Stop();
      }
      g_pending_tx_on_valid = 0U;
      break;
    case APP_UI_PENDING_ACTION_SYSTEM_RUN:
      if (g_pending_calibration_on != 0U)
      {
        App_LvglUiApplySystemErrorNow();
        (void)App_TxControl_StartCalibrationOutput();
      }
      else
      {
        (void)App_TxControl_StopCalibrationOutput();
      }
      g_pending_calibration_on_valid = 0U;
      break;
    default:
      break;
  }
}

static void App_LvglUiRefreshSectionVisibility(void)
{
  uint8_t show_single = (g_active_section == APP_UI_SECTION_SINGLE) ? 1U : 0U;
  uint8_t show_sweep = (g_active_section == APP_UI_SECTION_SWEEP) ? 1U : 0U;
  uint8_t show_mod = (g_active_section == APP_UI_SECTION_MOD) ? 1U : 0U;
  uint8_t show_system = (g_active_section == APP_UI_SECTION_SYSTEM) ? 1U : 0U;
  lv_obj_t *show_single_objs[] = {
    g_lo_name_label, g_lo_value_label, g_vpp_name_label, g_vpp_value_label,
    g_rf_name_label, g_rf_value_label, g_field_button, g_digit_button, g_run_button
  };
  lv_obj_t *show_sweep_objs[] = {
    g_sweep_start_name_label, g_sweep_start_value_label,
    g_sweep_stop_name_label, g_sweep_stop_value_label,
    g_sweep_time_name_label, g_sweep_time_value_label,
    g_vpp_name_label, g_vpp_value_label,
    g_rf_name_label, g_rf_value_label,
    g_field_button, g_digit_button, g_dec_button, g_inc_button, g_run_button
  };
  lv_obj_t *show_mod_objs[] = {
    g_mode_name_label, g_mode_dropdown,
    g_lo_name_label, g_lo_value_label,
    g_rate_name_label, g_rate_value_label,
    g_param_name_label, g_param_value_label,
    g_rf_name_label, g_rf_value_label,
    g_field_button, g_digit_button, g_dec_button, g_apply_button, g_run_button
  };
  lv_obj_t *show_system_objs[] = {
    g_lo_name_label, g_lo_value_label,
    g_vpp_name_label, g_vpp_value_label,
    g_rf_name_label, g_rf_value_label,
    g_rate_name_label, g_rate_value_label,
    g_field_button, g_digit_button, g_dec_button, g_run_button,
    g_edit_label
  };
  lv_obj_t *all_objs[] = {
    g_lo_name_label, g_lo_value_label, g_rf_name_label, g_rf_value_label, g_mode_name_label, g_mode_dropdown,
    g_vpp_name_label, g_vpp_value_label, g_rate_name_label, g_rate_value_label,
    g_param_name_label, g_param_value_label, g_preset_name_label, g_preset_value_label,
    g_debug_status_label, g_debug_button, g_ad9959_status_label, g_f429_status_label,
    g_sweep_time_name_label, g_sweep_time_value_label, g_sweep_start_name_label, g_sweep_start_value_label,
    g_sweep_stop_name_label, g_sweep_stop_value_label, g_edit_label, g_state_label,
    g_preset_save_button, g_preset_recall_button, g_sweep_button, g_sweep_time_button,
    g_sweep_start_button, g_sweep_stop_button, g_field_button, g_digit_button, g_dec_button,
    g_inc_button, g_apply_button, g_run_button
  };
  uint32_t i;

  for (i = 0U; i < (uint32_t)(sizeof(all_objs) / sizeof(all_objs[0])); i++)
  {
    if (all_objs[i] != NULL)
    {
      lv_obj_add_flag(all_objs[i], LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (show_single != 0U)
  {
    App_LvglUiApplySingleLayout();
    for (i = 0U; i < (uint32_t)(sizeof(show_single_objs) / sizeof(show_single_objs[0])); i++)
    {
      if (show_single_objs[i] != NULL) { lv_obj_clear_flag(show_single_objs[i], LV_OBJ_FLAG_HIDDEN); }
    }
  }

  if (show_sweep != 0U)
  {
    App_LvglUiApplySweepLayout();
    for (i = 0U; i < (uint32_t)(sizeof(show_sweep_objs) / sizeof(show_sweep_objs[0])); i++)
    {
      if (show_sweep_objs[i] != NULL) { lv_obj_clear_flag(show_sweep_objs[i], LV_OBJ_FLAG_HIDDEN); }
    }
  }

  if (show_mod != 0U)
  {
    App_LvglUiApplyModLayout();
    for (i = 0U; i < (uint32_t)(sizeof(show_mod_objs) / sizeof(show_mod_objs[0])); i++)
    {
      if (show_mod_objs[i] != NULL) { lv_obj_clear_flag(show_mod_objs[i], LV_OBJ_FLAG_HIDDEN); }
    }
  }

  if (show_system != 0U)
  {
    App_LvglUiApplySystemLayout();
    for (i = 0U; i < (uint32_t)(sizeof(show_system_objs) / sizeof(show_system_objs[0])); i++)
    {
      if (show_system_objs[i] != NULL) { lv_obj_clear_flag(show_system_objs[i], LV_OBJ_FLAG_HIDDEN); }
    }
  }
}

static void App_LvglUiApplySingleLayout(void)
{
  lv_obj_t *label;

  label = (g_field_button != NULL) ? lv_obj_get_child(g_field_button, 0) : NULL;
  if (label != NULL) { lv_label_set_text(label, "FREQ"); }
  label = (g_digit_button != NULL) ? lv_obj_get_child(g_digit_button, 0) : NULL;
  if (label != NULL) { lv_label_set_text(label, "AMP"); }

  if (g_section_title_label != NULL)
  {
    lv_obj_align(g_section_title_label, LV_ALIGN_TOP_MID, 0, 88);
  }

  if (g_lo_name_label != NULL)
  {
    lv_obj_align(g_lo_name_label, LV_ALIGN_TOP_LEFT, 248, 138);
  }
  if (g_lo_value_label != NULL)
  {
    lv_obj_align(g_lo_value_label, LV_ALIGN_TOP_LEFT, 390, 138);
  }
  if (g_vpp_name_label != NULL)
  {
    lv_obj_align(g_vpp_name_label, LV_ALIGN_TOP_LEFT, 248, 184);
  }
  if (g_vpp_value_label != NULL)
  {
    lv_obj_align(g_vpp_value_label, LV_ALIGN_TOP_LEFT, 390, 184);
  }
  if (g_rf_name_label != NULL)
  {
    lv_obj_align(g_rf_name_label, LV_ALIGN_TOP_LEFT, 248, 230);
  }
  if (g_rf_value_label != NULL)
  {
    lv_obj_align(g_rf_value_label, LV_ALIGN_TOP_LEFT, 390, 230);
  }

  if (g_field_button != NULL)
  {
    lv_obj_align(g_field_button, LV_ALIGN_BOTTOM_LEFT, 248, -30);
  }
  if (g_digit_button != NULL)
  {
    lv_obj_align(g_digit_button, LV_ALIGN_BOTTOM_LEFT, 366, -30);
  }
  if (g_run_button != NULL)
  {
    lv_obj_set_size(g_run_button, 104, 44);
    lv_obj_align(g_run_button, LV_ALIGN_BOTTOM_LEFT, 484, -30);
  }
}

static void App_LvglUiApplySweepLayout(void)
{
  lv_obj_t *label;

  label = (g_field_button != NULL) ? lv_obj_get_child(g_field_button, 0) : NULL;
  if (label != NULL) { lv_label_set_text(label, "START"); }
  label = (g_digit_button != NULL) ? lv_obj_get_child(g_digit_button, 0) : NULL;
  if (label != NULL) { lv_label_set_text(label, "STOP"); }
  label = (g_dec_button != NULL) ? lv_obj_get_child(g_dec_button, 0) : NULL;
  if (label != NULL) { lv_label_set_text(label, "TIME"); }
  label = (g_inc_button != NULL) ? lv_obj_get_child(g_inc_button, 0) : NULL;
  if (label != NULL) { lv_label_set_text(label, "RMS"); }

  if (g_sweep_start_name_label != NULL)
  {
    lv_obj_align(g_sweep_start_name_label, LV_ALIGN_TOP_LEFT, 248, 122);
  }
  if (g_sweep_start_value_label != NULL)
  {
    lv_obj_align(g_sweep_start_value_label, LV_ALIGN_TOP_LEFT, 390, 122);
  }
  if (g_sweep_stop_name_label != NULL)
  {
    lv_obj_align(g_sweep_stop_name_label, LV_ALIGN_TOP_LEFT, 248, 168);
  }
  if (g_sweep_stop_value_label != NULL)
  {
    lv_obj_align(g_sweep_stop_value_label, LV_ALIGN_TOP_LEFT, 390, 168);
  }
  if (g_sweep_time_name_label != NULL)
  {
    lv_obj_align(g_sweep_time_name_label, LV_ALIGN_TOP_LEFT, 248, 214);
  }
  if (g_sweep_time_value_label != NULL)
  {
    lv_obj_align(g_sweep_time_value_label, LV_ALIGN_TOP_LEFT, 390, 214);
  }
  if (g_vpp_name_label != NULL)
  {
    lv_obj_align(g_vpp_name_label, LV_ALIGN_TOP_LEFT, 248, 260);
  }
  if (g_vpp_value_label != NULL)
  {
    lv_obj_align(g_vpp_value_label, LV_ALIGN_TOP_LEFT, 390, 260);
  }
  if (g_rf_name_label != NULL)
  {
    lv_obj_align(g_rf_name_label, LV_ALIGN_TOP_LEFT, 248, 306);
  }
  if (g_rf_value_label != NULL)
  {
    lv_obj_align(g_rf_value_label, LV_ALIGN_TOP_LEFT, 390, 306);
  }
  if (g_field_button != NULL)
  {
    lv_obj_set_size(g_field_button, 96, 44);
    lv_obj_align(g_field_button, LV_ALIGN_BOTTOM_LEFT, 184, -30);
  }
  if (g_digit_button != NULL)
  {
    lv_obj_set_size(g_digit_button, 96, 44);
    lv_obj_align(g_digit_button, LV_ALIGN_BOTTOM_LEFT, 292, -30);
  }
  if (g_dec_button != NULL)
  {
    lv_obj_set_size(g_dec_button, 96, 44);
    lv_obj_align(g_dec_button, LV_ALIGN_BOTTOM_LEFT, 400, -30);
  }
  if (g_inc_button != NULL)
  {
    lv_obj_set_size(g_inc_button, 96, 44);
    lv_obj_align(g_inc_button, LV_ALIGN_BOTTOM_LEFT, 508, -30);
  }
  if (g_run_button != NULL)
  {
    lv_obj_set_size(g_run_button, 104, 44);
    lv_obj_align(g_run_button, LV_ALIGN_BOTTOM_LEFT, 616, -30);
  }
}

static void App_LvglUiApplyModLayout(void)
{
  lv_obj_t *label;

  label = (g_field_button != NULL) ? lv_obj_get_child(g_field_button, 0) : NULL;
  if (label != NULL) { lv_label_set_text(label, (g_ui_config.mode == APP_DAC_WAVE_MODE_2FSK) ? "F0" : "FC"); }
  label = (g_digit_button != NULL) ? lv_obj_get_child(g_digit_button, 0) : NULL;
  if (label != NULL) { lv_label_set_text(label, App_LvglUiModRateButtonText()); }
  label = (g_dec_button != NULL) ? lv_obj_get_child(g_dec_button, 0) : NULL;
  if (label != NULL) { lv_label_set_text(label, App_LvglUiModParamButtonText()); }

  if (g_section_title_label != NULL)
  {
    lv_obj_align(g_section_title_label, LV_ALIGN_TOP_MID, 0, 88);
  }
  if (g_mode_name_label != NULL)
  {
    lv_label_set_text(g_mode_name_label, "Mode");
    lv_obj_align(g_mode_name_label, LV_ALIGN_TOP_LEFT, 248, 112);
  }
  if (g_mode_dropdown != NULL)
  {
    lv_obj_set_width(g_mode_dropdown, 170);
    lv_obj_align(g_mode_dropdown, LV_ALIGN_TOP_LEFT, 390, 102);
  }
  if (g_lo_name_label != NULL)
  {
    lv_obj_align(g_lo_name_label, LV_ALIGN_TOP_LEFT, 248, 154);
  }
  if (g_lo_value_label != NULL)
  {
    lv_obj_align(g_lo_value_label, LV_ALIGN_TOP_LEFT, 390, 154);
  }
  if (g_rate_name_label != NULL)
  {
    lv_obj_align(g_rate_name_label, LV_ALIGN_TOP_LEFT, 248, 198);
  }
  if (g_rate_value_label != NULL)
  {
    lv_obj_align(g_rate_value_label, LV_ALIGN_TOP_LEFT, 390, 198);
  }
  if (g_param_name_label != NULL)
  {
    lv_obj_align(g_param_name_label, LV_ALIGN_TOP_LEFT, 248, 242);
  }
  if (g_param_value_label != NULL)
  {
    lv_obj_align(g_param_value_label, LV_ALIGN_TOP_LEFT, 390, 242);
  }
  if (g_rf_name_label != NULL)
  {
    lv_obj_align(g_rf_name_label, LV_ALIGN_TOP_LEFT, 248, 286);
  }
  if (g_rf_value_label != NULL)
  {
    lv_obj_align(g_rf_value_label, LV_ALIGN_TOP_LEFT, 390, 286);
  }
  if (g_field_button != NULL)
  {
    lv_obj_set_size(g_field_button, 96, 44);
    lv_obj_align(g_field_button, LV_ALIGN_BOTTOM_LEFT, 220, -30);
  }
  if (g_digit_button != NULL)
  {
    lv_obj_set_size(g_digit_button, 96, 44);
    lv_obj_align(g_digit_button, LV_ALIGN_BOTTOM_LEFT, 332, -30);
  }
  if (g_dec_button != NULL)
  {
    lv_obj_set_size(g_dec_button, 96, 44);
    lv_obj_align(g_dec_button, LV_ALIGN_BOTTOM_LEFT, 444, -30);
  }
  if (g_apply_button != NULL)
  {
    lv_obj_set_size(g_apply_button, 96, 44);
    lv_obj_align(g_apply_button, LV_ALIGN_BOTTOM_LEFT, 556, -30);
  }
  if (g_run_button != NULL)
  {
    lv_obj_set_size(g_run_button, 96, 44);
    lv_obj_align(g_run_button, LV_ALIGN_BOTTOM_LEFT, 556, -84);
  }
}

static void App_LvglUiApplySystemLayout(void)
{
  lv_obj_t *label;

  label = (g_field_button != NULL) ? lv_obj_get_child(g_field_button, 0) : NULL;
  if (label != NULL) { lv_label_set_text(label, "ERR-"); }
  label = (g_digit_button != NULL) ? lv_obj_get_child(g_digit_button, 0) : NULL;
  if (label != NULL) { lv_label_set_text(label, "ERR+"); }
  label = (g_dec_button != NULL) ? lv_obj_get_child(g_dec_button, 0) : NULL;
  if (label != NULL) { lv_label_set_text(label, "BASE"); }
  label = (g_run_button != NULL) ? lv_obj_get_child(g_run_button, 0) : NULL;
  if (label != NULL) { lv_label_set_text(label, "START"); }

  if (g_section_title_label != NULL)
  {
    lv_obj_align(g_section_title_label, LV_ALIGN_TOP_MID, 0, 88);
  }
  if (g_lo_name_label != NULL)
  {
    lv_obj_align(g_lo_name_label, LV_ALIGN_TOP_LEFT, 248, 138);
  }
  if (g_lo_value_label != NULL)
  {
    lv_obj_align(g_lo_value_label, LV_ALIGN_TOP_LEFT, 390, 138);
  }
  if (g_vpp_name_label != NULL)
  {
    lv_obj_align(g_vpp_name_label, LV_ALIGN_TOP_LEFT, 248, 184);
  }
  if (g_vpp_value_label != NULL)
  {
    lv_obj_align(g_vpp_value_label, LV_ALIGN_TOP_LEFT, 390, 184);
  }
  if (g_rf_name_label != NULL)
  {
    lv_obj_align(g_rf_name_label, LV_ALIGN_TOP_LEFT, 248, 230);
  }
  if (g_rf_value_label != NULL)
  {
    lv_obj_align(g_rf_value_label, LV_ALIGN_TOP_LEFT, 390, 230);
  }
  if (g_rate_name_label != NULL)
  {
    lv_obj_align(g_rate_name_label, LV_ALIGN_TOP_LEFT, 248, 276);
  }
  if (g_rate_value_label != NULL)
  {
    lv_obj_align(g_rate_value_label, LV_ALIGN_TOP_LEFT, 390, 276);
  }
  if (g_field_button != NULL)
  {
    lv_obj_set_size(g_field_button, 96, 44);
    lv_obj_align(g_field_button, LV_ALIGN_BOTTOM_LEFT, 220, -30);
  }
  if (g_digit_button != NULL)
  {
    lv_obj_set_size(g_digit_button, 96, 44);
    lv_obj_align(g_digit_button, LV_ALIGN_BOTTOM_LEFT, 328, -30);
  }
  if (g_dec_button != NULL)
  {
    lv_obj_set_size(g_dec_button, 96, 44);
    lv_obj_align(g_dec_button, LV_ALIGN_BOTTOM_LEFT, 436, -30);
  }
  if (g_run_button != NULL)
  {
    lv_obj_set_size(g_run_button, 104, 44);
    lv_obj_align(g_run_button, LV_ALIGN_BOTTOM_LEFT, 548, -30);
  }
}

static const char *App_LvglUiModRateButtonText(void)
{
  if ((g_ui_config.mode == APP_DAC_WAVE_MODE_AM) || (g_ui_config.mode == APP_DAC_WAVE_MODE_FM))
  {
    return "MFREQ";
  }

  return "BRATE";
}

static const char *App_LvglUiModParamButtonText(void)
{
  switch (g_ui_config.mode)
  {
    case APP_DAC_WAVE_MODE_AM:
      return "DEPTH";
    case APP_DAC_WAVE_MODE_FM:
      return "DEV";
    case APP_DAC_WAVE_MODE_2ASK:
      return "N/A";
    case APP_DAC_WAVE_MODE_2PSK:
      return "N/A";
    case APP_DAC_WAVE_MODE_2FSK:
      return "F1";
    default:
      return "SHIFT";
  }
}

static uint32_t App_LvglUiGetSystemErrorStepHz(void)
{
  if (g_system_error_step_index >= (uint8_t)(sizeof(s_system_error_steps_hz) / sizeof(s_system_error_steps_hz[0])))
  {
    g_system_error_step_index = 0U;
  }

  return s_system_error_steps_hz[g_system_error_step_index];
}

static void App_LvglUiAdjustSystemError(int32_t direction)
{
  int64_t next_value = (int64_t)g_system_lo_error_hz + ((int64_t)direction * (int64_t)App_LvglUiGetSystemErrorStepHz());

  if (next_value > 1000000LL)
  {
    next_value = 1000000LL;
  }
  else if (next_value < -1000000LL)
  {
    next_value = -1000000LL;
  }

  g_system_lo_error_hz = (int32_t)next_value;
}

static void App_LvglUiApplySystemErrorNow(void)
{
  (void)App_TxControl_SetLoErrorHz(g_system_lo_error_hz);
  if (App_TxControl_GetCalibrationOutputEnabled() != 0U)
  {
    (void)App_TxControl_StartCalibrationOutput();
  }
}

static void App_LvglUiRefreshModeVisibility(void)
{
  if (g_preset_name_label != NULL) { lv_obj_add_flag(g_preset_name_label, LV_OBJ_FLAG_HIDDEN); }
  if (g_preset_value_label != NULL) { lv_obj_add_flag(g_preset_value_label, LV_OBJ_FLAG_HIDDEN); }
  if (g_preset_save_button != NULL) { lv_obj_add_flag(g_preset_save_button, LV_OBJ_FLAG_HIDDEN); }
  if (g_preset_recall_button != NULL) { lv_obj_add_flag(g_preset_recall_button, LV_OBJ_FLAG_HIDDEN); }
  if (g_sweep_button != NULL) { lv_obj_add_flag(g_sweep_button, LV_OBJ_FLAG_HIDDEN); }
  if (g_sweep_time_button != NULL) { lv_obj_add_flag(g_sweep_time_button, LV_OBJ_FLAG_HIDDEN); }
  if (g_sweep_start_button != NULL) { lv_obj_add_flag(g_sweep_start_button, LV_OBJ_FLAG_HIDDEN); }
  if (g_sweep_stop_button != NULL) { lv_obj_add_flag(g_sweep_stop_button, LV_OBJ_FLAG_HIDDEN); }
  if (g_state_label != NULL) { lv_obj_add_flag(g_state_label, LV_OBJ_FLAG_HIDDEN); }
  if ((g_inc_button != NULL) && (g_active_section != APP_UI_SECTION_SWEEP))
  {
    lv_obj_add_flag(g_inc_button, LV_OBJ_FLAG_HIDDEN);
  }

  if (g_active_section == APP_UI_SECTION_SYSTEM)
  {
    if (g_edit_label != NULL) { lv_obj_clear_flag(g_edit_label, LV_OBJ_FLAG_HIDDEN); }
    if (g_mode_name_label != NULL) { lv_obj_add_flag(g_mode_name_label, LV_OBJ_FLAG_HIDDEN); }
    if (g_mode_dropdown != NULL) { lv_obj_add_flag(g_mode_dropdown, LV_OBJ_FLAG_HIDDEN); }
    if (g_debug_status_label != NULL) { lv_obj_add_flag(g_debug_status_label, LV_OBJ_FLAG_HIDDEN); }
    if (g_debug_button != NULL) { lv_obj_add_flag(g_debug_button, LV_OBJ_FLAG_HIDDEN); }
    if (g_ad9959_status_label != NULL) { lv_obj_add_flag(g_ad9959_status_label, LV_OBJ_FLAG_HIDDEN); }
    if (g_f429_status_label != NULL) { lv_obj_add_flag(g_f429_status_label, LV_OBJ_FLAG_HIDDEN); }
    return;
  }

  if (g_edit_label != NULL) { lv_obj_add_flag(g_edit_label, LV_OBJ_FLAG_HIDDEN); }
}

static uint8_t App_LvglUiIsFieldEditable(AppUiEditField field)
{
  if (App_LvglUiIsCwMode(g_ui_config.mode) != 0U)
  {
    return ((field == APP_UI_FIELD_LO_FREQ) || (field == APP_UI_FIELD_VPP)) ? 1U : 0U;
  }

  if (field != APP_UI_FIELD_PARAM)
  {
    return 1U;
  }

  if ((g_ui_config.mode == APP_DAC_WAVE_MODE_2ASK) || (g_ui_config.mode == APP_DAC_WAVE_MODE_2PSK))
  {
    return 0U;
  }

  return 1U;
}

static const char *App_LvglUiFieldName(AppUiEditField field)
{
  switch (field)
  {
    case APP_UI_FIELD_LO_FREQ:
      return (g_ui_config.mode == APP_DAC_WAVE_MODE_2FSK) ? "Init freq" : "LO freq";
    case APP_UI_FIELD_VPP:
      return "Amp";
    case APP_UI_FIELD_RATE:
      return "Rate";
    case APP_UI_FIELD_PARAM:
    default:
      return (g_ui_config.mode == APP_DAC_WAVE_MODE_2FSK) ? "Hop freq" : "Param";
  }
}

static uint32_t App_LvglUiGetFieldStepCount(AppUiEditField field)
{
  switch (field)
  {
    case APP_UI_FIELD_LO_FREQ:
      return (uint32_t)(sizeof(s_lo_steps_hz) / sizeof(s_lo_steps_hz[0]));
    case APP_UI_FIELD_VPP:
      return (uint32_t)(sizeof(s_amp_steps_mv) / sizeof(s_amp_steps_mv[0]));
    case APP_UI_FIELD_RATE:
      return (uint32_t)(sizeof(s_rate_steps_hz) / sizeof(s_rate_steps_hz[0]));
    case APP_UI_FIELD_PARAM:
      if (g_ui_config.mode == APP_DAC_WAVE_MODE_AM)
      {
        return (uint32_t)(sizeof(s_param_steps_pct) / sizeof(s_param_steps_pct[0]));
      }
      if ((g_ui_config.mode == APP_DAC_WAVE_MODE_FM) || (g_ui_config.mode == APP_DAC_WAVE_MODE_2FSK))
      {
        return (uint32_t)(sizeof(s_rate_steps_hz) / sizeof(s_rate_steps_hz[0]));
      }
      return 1U;
    default:
      return 1U;
  }
}

static uint32_t App_LvglUiGetFieldStepValue(AppUiEditField field, uint8_t index)
{
  uint32_t count = App_LvglUiGetFieldStepCount(field);

  if (count == 0U)
  {
    return 1U;
  }

  if (index >= count)
  {
    index = (uint8_t)(count - 1U);
  }

  switch (field)
  {
    case APP_UI_FIELD_LO_FREQ:
      return s_lo_steps_hz[index];
    case APP_UI_FIELD_VPP:
      return s_amp_steps_mv[index];
    case APP_UI_FIELD_RATE:
      return s_rate_steps_hz[index];
    case APP_UI_FIELD_PARAM:
      if (g_ui_config.mode == APP_DAC_WAVE_MODE_AM)
      {
        return s_param_steps_pct[index];
      }
      if ((g_ui_config.mode == APP_DAC_WAVE_MODE_FM) || (g_ui_config.mode == APP_DAC_WAVE_MODE_2FSK))
      {
        return s_rate_steps_hz[index];
      }
      return 1U;
    default:
      return 1U;
  }
}

static void App_LvglUiGetFieldRange(AppUiEditField field, uint32_t *min_value, uint32_t *max_value)
{
  switch (field)
  {
    case APP_UI_FIELD_LO_FREQ:
      *min_value = APP_TX_CONTROL_FREQ_MIN_HZ;
      *max_value = APP_TX_CONTROL_FREQ_MAX_HZ;
      break;
    case APP_UI_FIELD_VPP:
      *min_value = APP_UI_AMP_RMS_MIN_MV;
      *max_value = APP_UI_AMP_RMS_MAX_MV;
      break;
    case APP_UI_FIELD_RATE:
      if ((g_ui_config.mode == APP_DAC_WAVE_MODE_AM) || (g_ui_config.mode == APP_DAC_WAVE_MODE_FM))
      {
        *min_value = APP_DAC_WAVE_MOD_FREQ_MIN_HZ;
        *max_value = APP_DAC_WAVE_MOD_FREQ_MAX_HZ;
      }
      else
      {
        *min_value = APP_DAC_WAVE_SYMBOL_RATE_MIN_BPS;
        *max_value = APP_DAC_WAVE_SYMBOL_RATE_MAX_BPS;
      }
      break;
    case APP_UI_FIELD_PARAM:
      if (g_ui_config.mode == APP_DAC_WAVE_MODE_AM)
      {
        *min_value = APP_DAC_WAVE_AM_DEPTH_MIN_PERCENT;
        *max_value = APP_DAC_WAVE_AM_DEPTH_MAX_PERCENT;
      }
      else if (g_ui_config.mode == APP_DAC_WAVE_MODE_FM)
      {
        *min_value = APP_DAC_WAVE_FM_DEVIATION_MIN_HZ;
        *max_value = APP_DAC_WAVE_FM_DEVIATION_MAX_HZ;
      }
      else if (g_ui_config.mode == APP_DAC_WAVE_MODE_2FSK)
      {
        *min_value = g_lo_freq_hz;
        *max_value = APP_TX_CONTROL_FREQ_MAX_HZ;
      }
      else
      {
        *min_value = APP_DAC_WAVE_FSK_SHIFT_MIN_HZ;
        *max_value = APP_DAC_WAVE_FSK_SHIFT_MAX_HZ;
      }
      break;
    default:
      *min_value = 0U;
      *max_value = 0U;
      break;
  }
}

static uint32_t App_LvglUiGetFieldValue(AppUiEditField field)
{
  switch (field)
  {
    case APP_UI_FIELD_LO_FREQ:
      return g_lo_freq_hz;
    case APP_UI_FIELD_VPP:
      return App_LvglUiInternalToAmpRmsMv(g_ui_config.vpp_mv);
    case APP_UI_FIELD_RATE:
      if ((g_ui_config.mode == APP_DAC_WAVE_MODE_AM) || (g_ui_config.mode == APP_DAC_WAVE_MODE_FM))
      {
        return g_ui_config.mod_freq_hz;
      }
      return g_ui_config.symbol_rate_bps;
    case APP_UI_FIELD_PARAM:
      if (g_ui_config.mode == APP_DAC_WAVE_MODE_AM)
      {
        return g_ui_config.am_depth_percent;
      }
      if (g_ui_config.mode == APP_DAC_WAVE_MODE_FM)
      {
        return g_ui_config.fm_deviation_hz;
      }
      if (g_ui_config.mode == APP_DAC_WAVE_MODE_2FSK)
      {
        return App_LvglUiGetFskNextFrequencyHz();
      }
      return g_ui_config.fsk_shift_hz;
    default:
      return 0U;
  }
}

static void App_LvglUiSetFieldValue(AppUiEditField field, uint32_t value)
{
  switch (field)
  {
    case APP_UI_FIELD_LO_FREQ:
      g_lo_freq_hz = value;
      g_lo_freq_dirty = 1U;
      break;
    case APP_UI_FIELD_VPP:
      g_ui_config.vpp_mv = App_LvglUiAmpRmsMvToInternal(value);
      break;
    case APP_UI_FIELD_RATE:
      if ((g_ui_config.mode == APP_DAC_WAVE_MODE_AM) || (g_ui_config.mode == APP_DAC_WAVE_MODE_FM))
      {
        g_ui_config.mod_freq_hz = value;
      }
      else
      {
        g_ui_config.symbol_rate_bps = value;
      }
      break;
    case APP_UI_FIELD_PARAM:
      if (g_ui_config.mode == APP_DAC_WAVE_MODE_AM)
      {
        g_ui_config.am_depth_percent = (uint16_t)value;
      }
      else if (g_ui_config.mode == APP_DAC_WAVE_MODE_FM)
      {
        g_ui_config.fm_deviation_hz = value;
      }
      else if (g_ui_config.mode == APP_DAC_WAVE_MODE_2FSK)
      {
        g_ui_config.fsk_shift_hz = (value >= g_lo_freq_hz) ? (value - g_lo_freq_hz) : 0U;
      }
      break;
    default:
      break;
  }
}

static void App_LvglUiAdjustSelectedField(int32_t direction)
{
  uint32_t min_value;
  uint32_t max_value;
  uint32_t current_value;
  uint32_t step_value;
  uint32_t next_value;

  if (App_LvglUiIsFieldEditable(g_selected_field) == 0U)
  {
    return;
  }

  App_LvglUiGetFieldRange(g_selected_field, &min_value, &max_value);
  current_value = App_LvglUiGetFieldValue(g_selected_field);
  step_value = App_LvglUiGetFieldStepValue(g_selected_field, g_digit_index);

  if (direction > 0)
  {
    if (current_value > (max_value - step_value))
    {
      next_value = max_value;
    }
    else
    {
      next_value = current_value + step_value;
    }
  }
  else
  {
    if (current_value < (min_value + step_value))
    {
      next_value = min_value;
    }
    else
    {
      next_value = current_value - step_value;
    }
  }

  App_LvglUiSetFieldValue(g_selected_field, next_value);
}

static void App_LvglUiApplyConfig(void)
{
  AppDacWavegenConfig desired_config;
  uint32_t desired_freq_hz = 0U;

  (void)App_TxControl_SetConfig(&g_ui_config);
  (void)App_TxControl_SetFrequencyHz(g_lo_freq_hz);
  (void)App_TxControl_GetConfig(&desired_config, &desired_freq_hz);

  g_ui_config = desired_config;
  g_lo_freq_hz = desired_freq_hz;
  g_lo_freq_dirty = 0U;
  g_mode_dirty = 0U;
  g_preset_freq_hz = App_TxControl_GetPresetFrequencyHz();
  g_sweep_period_ms = App_TxControl_GetSweepPeriodMs();
  g_sweep_start_hz = App_TxControl_GetSweepStartHz();
  g_sweep_stop_hz = App_TxControl_GetSweepStopHz();
  App_LvglUiEnsureEditableField();
  App_LvglUiRefreshValues();
  App_LvglUiRefreshEditState();
  App_LvglUiRefreshState();
}

static void App_LvglUiOpenKeypad(AppUiKeypadTarget target)
{
  g_keypad_target = target;
  if (target == APP_UI_KEYPAD_LO_FREQ)
  {
    App_LvglUiSelectField(APP_UI_FIELD_LO_FREQ);
  }
  else if ((target == APP_UI_KEYPAD_AMP) || (target == APP_UI_KEYPAD_SWEEP_AMP))
  {
    App_LvglUiSelectField(APP_UI_FIELD_VPP);
  }
  else if ((target == APP_UI_KEYPAD_SWEEP_START) || (target == APP_UI_KEYPAD_SWEEP_STOP) ||
           (target == APP_UI_KEYPAD_MOD_FC))
  {
    App_LvglUiSelectField(APP_UI_FIELD_LO_FREQ);
  }
  else if (target == APP_UI_KEYPAD_MOD_FREQ)
  {
    App_LvglUiSelectField(APP_UI_FIELD_RATE);
  }
  else if (target == APP_UI_KEYPAD_MOD_DEPTH)
  {
    App_LvglUiSelectField(APP_UI_FIELD_PARAM);
  }
  g_keypad_input[0] = '\0';

  App_LvglUiRefreshKeypad();
  lv_obj_clear_flag(g_keypad_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(g_keypad_overlay);
}

static void App_LvglUiCloseKeypad(void)
{
  if (g_keypad_overlay != NULL)
  {
    lv_obj_add_flag(g_keypad_overlay, LV_OBJ_FLAG_HIDDEN);
  }
}

static void App_LvglUiRefreshKeypad(void)
{
  uint32_t current_value;

  if ((g_keypad_overlay == NULL) || (g_keypad_title_label == NULL) ||
      (g_keypad_value_label == NULL) || (g_keypad_unit_label == NULL) ||
      (g_keypad_current_label == NULL))
  {
    return;
  }

  current_value = 0U;

  if ((g_keypad_target == APP_UI_KEYPAD_LO_FREQ) || (g_keypad_target == APP_UI_KEYPAD_MOD_FC))
  {
    current_value = g_lo_freq_hz;
    if ((g_keypad_target == APP_UI_KEYPAD_MOD_FC) && (g_ui_config.mode == APP_DAC_WAVE_MODE_2FSK))
    {
      lv_label_set_text(g_keypad_title_label, "Set F0");
    }
    else
    {
      lv_label_set_text(g_keypad_title_label, "Set FREQ");
    }
    lv_label_set_text(g_keypad_unit_label, "MHz");
    lv_label_set_text_fmt(g_keypad_current_label,
                          "Current: %lu.%06lu MHz",
                          (unsigned long)(current_value / 1000000UL),
                          (unsigned long)(current_value % 1000000UL));
    if (g_keypad_dot_button != NULL)
    {
      lv_obj_clear_flag(g_keypad_dot_button, LV_OBJ_FLAG_HIDDEN);
    }
  }
  else if ((g_keypad_target == APP_UI_KEYPAD_AMP) || (g_keypad_target == APP_UI_KEYPAD_SWEEP_AMP))
  {
    current_value = App_LvglUiInternalToAmpRmsMv(g_ui_config.vpp_mv);
    lv_label_set_text(g_keypad_title_label,
                      (g_keypad_target == APP_UI_KEYPAD_SWEEP_AMP) ? "Set RMS" : "Set AMP");
    lv_label_set_text(g_keypad_unit_label, "mVrms");
    lv_label_set_text_fmt(g_keypad_current_label,
                          "Current: %lu mVrms",
                          (unsigned long)current_value);
    if (g_keypad_dot_button != NULL)
    {
      lv_obj_add_flag(g_keypad_dot_button, LV_OBJ_FLAG_HIDDEN);
    }
  }
  else if (g_keypad_target == APP_UI_KEYPAD_SWEEP_START)
  {
    current_value = g_sweep_start_hz;
    lv_label_set_text(g_keypad_title_label, "Set START");
    lv_label_set_text(g_keypad_unit_label, "MHz");
    lv_label_set_text_fmt(g_keypad_current_label,
                          "Current: %lu.%06lu MHz",
                          (unsigned long)(current_value / 1000000UL),
                          (unsigned long)(current_value % 1000000UL));
    if (g_keypad_dot_button != NULL)
    {
      lv_obj_clear_flag(g_keypad_dot_button, LV_OBJ_FLAG_HIDDEN);
    }
  }
  else if (g_keypad_target == APP_UI_KEYPAD_SWEEP_STOP)
  {
    current_value = g_sweep_stop_hz;
    lv_label_set_text(g_keypad_title_label, "Set STOP");
    lv_label_set_text(g_keypad_unit_label, "MHz");
    lv_label_set_text_fmt(g_keypad_current_label,
                          "Current: %lu.%06lu MHz",
                          (unsigned long)(current_value / 1000000UL),
                          (unsigned long)(current_value % 1000000UL));
    if (g_keypad_dot_button != NULL)
    {
      lv_obj_clear_flag(g_keypad_dot_button, LV_OBJ_FLAG_HIDDEN);
    }
  }
  else
  {
    if (g_keypad_target == APP_UI_KEYPAD_SWEEP_TIME)
    {
      current_value = g_sweep_period_ms;
      lv_label_set_text(g_keypad_title_label, "Set TIME");
      lv_label_set_text(g_keypad_unit_label, "s");
      lv_label_set_text_fmt(g_keypad_current_label,
                            "Current: %lu.%01lu s",
                            (unsigned long)(current_value / 1000UL),
                            (unsigned long)((current_value % 1000UL) / 100UL));
      if (g_keypad_dot_button != NULL)
      {
        lv_obj_clear_flag(g_keypad_dot_button, LV_OBJ_FLAG_HIDDEN);
      }
    }
    else if (g_keypad_target == APP_UI_KEYPAD_MOD_FREQ)
    {
      if ((g_ui_config.mode == APP_DAC_WAVE_MODE_AM) || (g_ui_config.mode == APP_DAC_WAVE_MODE_FM))
      {
        current_value = g_ui_config.mod_freq_hz;
        lv_label_set_text(g_keypad_title_label, "Set MFREQ");
        lv_label_set_text(g_keypad_unit_label, "Hz");
        lv_label_set_text_fmt(g_keypad_current_label, "Current: %lu Hz", (unsigned long)current_value);
      }
      else
      {
        current_value = g_ui_config.symbol_rate_bps;
        lv_label_set_text(g_keypad_title_label, "Set BRATE");
        lv_label_set_text(g_keypad_unit_label, "bps");
        lv_label_set_text_fmt(g_keypad_current_label, "Current: %lu bps", (unsigned long)current_value);
      }
      if (g_keypad_dot_button != NULL)
      {
        lv_obj_add_flag(g_keypad_dot_button, LV_OBJ_FLAG_HIDDEN);
      }
    }
    else
    {
      if (g_ui_config.mode == APP_DAC_WAVE_MODE_AM)
      {
        current_value = g_ui_config.am_depth_percent;
        lv_label_set_text(g_keypad_title_label, "Set DEPTH");
        lv_label_set_text(g_keypad_unit_label, "%");
        lv_label_set_text_fmt(g_keypad_current_label, "Current: %lu %%", (unsigned long)current_value);
        if (g_keypad_dot_button != NULL)
        {
          lv_obj_add_flag(g_keypad_dot_button, LV_OBJ_FLAG_HIDDEN);
        }
      }
      else if (g_ui_config.mode == APP_DAC_WAVE_MODE_FM)
      {
        current_value = g_ui_config.fm_deviation_hz;
        lv_label_set_text(g_keypad_title_label, "Set DEV");
        lv_label_set_text(g_keypad_unit_label, "Hz");
        lv_label_set_text_fmt(g_keypad_current_label, "Current: %lu Hz", (unsigned long)current_value);
        if (g_keypad_dot_button != NULL)
        {
          lv_obj_add_flag(g_keypad_dot_button, LV_OBJ_FLAG_HIDDEN);
        }
      }
      else if (g_ui_config.mode == APP_DAC_WAVE_MODE_2FSK)
      {
        current_value = App_LvglUiGetFskNextFrequencyHz();
        lv_label_set_text(g_keypad_title_label, "Set F1");
        lv_label_set_text(g_keypad_unit_label, "MHz");
        lv_label_set_text_fmt(g_keypad_current_label,
                              "Current: %lu.%06lu MHz",
                              (unsigned long)(current_value / 1000000UL),
                              (unsigned long)(current_value % 1000000UL));
        if (g_keypad_dot_button != NULL)
        {
          lv_obj_clear_flag(g_keypad_dot_button, LV_OBJ_FLAG_HIDDEN);
        }
      }
      else
      {
        current_value = g_ui_config.fsk_shift_hz;
        lv_label_set_text(g_keypad_title_label, "Set SHIFT");
        lv_label_set_text(g_keypad_unit_label, "Hz");
        lv_label_set_text_fmt(g_keypad_current_label, "Current: %lu Hz", (unsigned long)current_value);
        if (g_keypad_dot_button != NULL)
        {
          lv_obj_add_flag(g_keypad_dot_button, LV_OBJ_FLAG_HIDDEN);
        }
      }
    }
  }

  if (g_keypad_input[0] == '\0')
  {
    lv_label_set_text(g_keypad_value_label, "_");
  }
  else
  {
    lv_label_set_text(g_keypad_value_label, g_keypad_input);
  }

  lv_obj_align_to(g_keypad_unit_label, g_keypad_value_label, LV_ALIGN_OUT_RIGHT_MID, 12, 0);
}

static uint8_t App_LvglUiTryParseKeypadValue(AppUiKeypadTarget target, const char *text, uint32_t *value_out)
{
  uint32_t min_value = 0U;
  uint32_t max_value = 0U;
  uint64_t parsed_value = 0ULL;
  uint32_t i;

  if ((text == NULL) || (text[0] == '\0') || (value_out == NULL))
  {
    return 0U;
  }

  if ((target == APP_UI_KEYPAD_LO_FREQ) || (target == APP_UI_KEYPAD_SWEEP_START) ||
      (target == APP_UI_KEYPAD_SWEEP_STOP) || (target == APP_UI_KEYPAD_MOD_FC) ||
      ((target == APP_UI_KEYPAD_MOD_DEPTH) && (g_ui_config.mode == APP_DAC_WAVE_MODE_2FSK)))
  {
    min_value = ((target == APP_UI_KEYPAD_MOD_DEPTH) && (g_ui_config.mode == APP_DAC_WAVE_MODE_2FSK)) ?
                g_lo_freq_hz : APP_TX_CONTROL_FREQ_MIN_HZ;
    max_value = APP_TX_CONTROL_FREQ_MAX_HZ;
    uint64_t int_part = 0ULL;
    uint64_t frac_part = 0ULL;
    uint32_t frac_digits = 0U;
    uint8_t seen_dot = 0U;
    uint8_t has_digit = 0U;

    for (i = 0U; text[i] != '\0'; ++i)
    {
      char c = text[i];
      if ((c >= '0') && (c <= '9'))
      {
        has_digit = 1U;
        if (seen_dot == 0U)
        {
          int_part = (int_part * 10ULL) + (uint64_t)(c - '0');
        }
        else
        {
          if (frac_digits >= 6U)
          {
            return 0U;
          }
          frac_part = (frac_part * 10ULL) + (uint64_t)(c - '0');
          frac_digits++;
        }
      }
      else if ((c == '.') && (seen_dot == 0U))
      {
        seen_dot = 1U;
      }
      else
      {
        return 0U;
      }
    }

    if (has_digit == 0U)
    {
      return 0U;
    }

    while (frac_digits < 6U)
    {
      frac_part *= 10ULL;
      frac_digits++;
    }

    parsed_value = (int_part * 1000000ULL) + frac_part;
  }
  else if (target == APP_UI_KEYPAD_SWEEP_TIME)
  {
    uint64_t int_part = 0ULL;
    uint64_t frac_part = 0ULL;
    uint32_t frac_digits = 0U;
    uint8_t seen_dot = 0U;
    uint8_t has_digit = 0U;

    min_value = APP_TX_CONTROL_SWEEP_PERIOD_MIN_MS;
    max_value = APP_TX_CONTROL_SWEEP_PERIOD_MAX_MS;

    for (i = 0U; text[i] != '\0'; ++i)
    {
      char c = text[i];
      if ((c >= '0') && (c <= '9'))
      {
        has_digit = 1U;
        if (seen_dot == 0U)
        {
          int_part = (int_part * 10ULL) + (uint64_t)(c - '0');
        }
        else
        {
          if (frac_digits >= 3U)
          {
            return 0U;
          }
          frac_part = (frac_part * 10ULL) + (uint64_t)(c - '0');
          frac_digits++;
        }
      }
      else if ((c == '.') && (seen_dot == 0U))
      {
        seen_dot = 1U;
      }
      else
      {
        return 0U;
      }
    }

    if (has_digit == 0U)
    {
      return 0U;
    }

    while (frac_digits < 3U)
    {
      frac_part *= 10ULL;
      frac_digits++;
    }

    parsed_value = (int_part * 1000ULL) + frac_part;
  }
  else
  {
    if ((target == APP_UI_KEYPAD_AMP) || (target == APP_UI_KEYPAD_SWEEP_AMP))
    {
      min_value = APP_UI_AMP_RMS_MIN_MV;
      max_value = APP_UI_AMP_RMS_MAX_MV;
    }
    else if (target == APP_UI_KEYPAD_MOD_FREQ)
    {
      if ((g_ui_config.mode == APP_DAC_WAVE_MODE_AM) || (g_ui_config.mode == APP_DAC_WAVE_MODE_FM))
      {
        min_value = APP_DAC_WAVE_MOD_FREQ_MIN_HZ;
        max_value = APP_DAC_WAVE_MOD_FREQ_MAX_HZ;
      }
      else
      {
        min_value = APP_DAC_WAVE_SYMBOL_RATE_MIN_BPS;
        max_value = APP_DAC_WAVE_SYMBOL_RATE_MAX_BPS;
      }
    }
    else
    {
      if (g_ui_config.mode == APP_DAC_WAVE_MODE_AM)
      {
        min_value = APP_DAC_WAVE_AM_DEPTH_MIN_PERCENT;
        max_value = APP_DAC_WAVE_AM_DEPTH_MAX_PERCENT;
      }
      else if (g_ui_config.mode == APP_DAC_WAVE_MODE_FM)
      {
        min_value = APP_DAC_WAVE_FM_DEVIATION_MIN_HZ;
        max_value = APP_DAC_WAVE_FM_DEVIATION_MAX_HZ;
      }
      else
      {
        min_value = APP_DAC_WAVE_FSK_SHIFT_MIN_HZ;
        max_value = APP_DAC_WAVE_FSK_SHIFT_MAX_HZ;
      }
    }
    for (i = 0U; text[i] != '\0'; ++i)
    {
      char c = text[i];
      if ((c < '0') || (c > '9'))
      {
        return 0U;
      }
      parsed_value = (parsed_value * 10ULL) + (uint64_t)(c - '0');
    }
  }

  if (parsed_value < (uint64_t)min_value)
  {
    parsed_value = (uint64_t)min_value;
  }

  if (parsed_value > (uint64_t)max_value)
  {
    parsed_value = (uint64_t)max_value;
  }

  *value_out = (uint32_t)parsed_value;
  return 1U;
}

static void App_LvglUiOnModeChanged(lv_event_t *e)
{
  AppDacWavegenMode mode;

  (void)e;

  if (App_LvglUiIsDebugLocked() != 0U)
  {
    App_LvglUiRefreshState();
    return;
  }

  mode = (AppDacWavegenMode)lv_dropdown_get_selected(g_mode_dropdown);
  App_LvglUiResetModeConfig(mode);
  (void)App_TxControl_SetMode(mode);
  App_LvglUiSetActiveSection((mode == APP_DAC_WAVE_MODE_CW) ? APP_UI_SECTION_SINGLE : APP_UI_SECTION_MOD);
  g_mode_dirty = 1U;
  g_selected_field = APP_UI_DEFAULT_FIELD;
  g_digit_index = APP_UI_DEFAULT_DIGIT_INDEX;
  App_LvglUiEnsureEditableField();
  App_LvglUiRefreshValues();
  App_LvglUiRefreshEditState();
  App_LvglUiRefreshState();
}

static void App_LvglUiOnFieldClicked(lv_event_t *e)
{
  (void)e;
  if (App_LvglUiIsDebugLocked() != 0U)
  {
    App_LvglUiRefreshState();
    return;
  }
  if (g_active_section == APP_UI_SECTION_SYSTEM)
  {
    App_LvglUiAdjustSystemError(-1);
    App_LvglUiApplySystemErrorNow();
    App_LvglUiRefreshValues();
    App_LvglUiRefreshEditState();
    App_LvglUiRefreshState();
    return;
  }
  if (g_active_section == APP_UI_SECTION_SWEEP)
  {
    App_LvglUiOpenKeypad(APP_UI_KEYPAD_SWEEP_START);
    return;
  }
  if (g_active_section == APP_UI_SECTION_MOD)
  {
    App_LvglUiOpenKeypad(APP_UI_KEYPAD_MOD_FC);
    return;
  }
  App_LvglUiOpenKeypad(APP_UI_KEYPAD_LO_FREQ);
}

static void App_LvglUiOnDigitClicked(lv_event_t *e)
{
  (void)e;
  if (App_LvglUiIsDebugLocked() != 0U)
  {
    App_LvglUiRefreshState();
    return;
  }
  if (g_active_section == APP_UI_SECTION_SYSTEM)
  {
    App_LvglUiAdjustSystemError(1);
    App_LvglUiApplySystemErrorNow();
    App_LvglUiRefreshValues();
    App_LvglUiRefreshEditState();
    App_LvglUiRefreshState();
    return;
  }
  if (g_active_section == APP_UI_SECTION_SWEEP)
  {
    App_LvglUiOpenKeypad(APP_UI_KEYPAD_SWEEP_STOP);
    return;
  }
  if (g_active_section == APP_UI_SECTION_MOD)
  {
    App_LvglUiOpenKeypad(APP_UI_KEYPAD_MOD_FREQ);
    return;
  }
  App_LvglUiOpenKeypad(APP_UI_KEYPAD_AMP);
}

static void App_LvglUiOnDecClicked(lv_event_t *e)
{
  (void)e;
  if (App_LvglUiIsDebugLocked() != 0U)
  {
    App_LvglUiRefreshState();
    return;
  }
  if (g_active_section == APP_UI_SECTION_SYSTEM)
  {
    g_system_error_step_index = (uint8_t)((g_system_error_step_index + 1U) %
                                          (uint8_t)(sizeof(s_system_error_steps_hz) / sizeof(s_system_error_steps_hz[0])));
    App_LvglUiApplySystemErrorNow();
    App_LvglUiRefreshValues();
    App_LvglUiRefreshEditState();
    App_LvglUiRefreshState();
    return;
  }
  if (g_active_section == APP_UI_SECTION_SWEEP)
  {
    App_LvglUiOpenKeypad(APP_UI_KEYPAD_SWEEP_TIME);
    return;
  }
  if (g_active_section == APP_UI_SECTION_MOD)
  {
    if (App_LvglUiIsFieldEditable(APP_UI_FIELD_PARAM) == 0U)
    {
      App_LvglUiRefreshState();
      return;
    }
    App_LvglUiOpenKeypad(APP_UI_KEYPAD_MOD_DEPTH);
    return;
  }
  App_LvglUiAdjustSelectedField(-1);
  App_LvglUiRefreshValues();
  App_LvglUiRefreshEditState();
}

static void App_LvglUiOnIncClicked(lv_event_t *e)
{
  (void)e;
  if (App_LvglUiIsDebugLocked() != 0U)
  {
    App_LvglUiRefreshState();
    return;
  }
  if (g_active_section == APP_UI_SECTION_SWEEP)
  {
    App_LvglUiOpenKeypad(APP_UI_KEYPAD_SWEEP_AMP);
    return;
  }
  App_LvglUiAdjustSelectedField(1);
  App_LvglUiRefreshValues();
  App_LvglUiRefreshEditState();
}

static void App_LvglUiOnApplyClicked(lv_event_t *e)
{
  (void)e;
  if (App_LvglUiIsDebugLocked() != 0U)
  {
    App_LvglUiRefreshState();
    return;
  }
  if (g_active_section != APP_UI_SECTION_MOD)
  {
    App_LvglUiRefreshState();
    return;
  }
  (void)App_LvglUiSendCurrentModeToWinner();
}

static void App_LvglUiOnPresetSaveClicked(lv_event_t *e)
{
  (void)e;
  if (App_LvglUiIsDebugLocked() != 0U)
  {
    App_LvglUiRefreshState();
    return;
  }
  (void)App_TxControl_SavePresetFrequencyHz(g_lo_freq_hz);
  g_preset_freq_hz = App_TxControl_GetPresetFrequencyHz();
  App_LvglUiRefreshValues();
}

static void App_LvglUiOnPresetRecallClicked(lv_event_t *e)
{
  AppTxControlSnapshot snapshot;

  (void)e;
  if (App_LvglUiIsDebugLocked() != 0U)
  {
    App_LvglUiRefreshState();
    return;
  }
  (void)App_TxControl_RecallPresetFrequencyHz(&g_lo_freq_hz);
  (void)App_TxControl_SetConfig(&g_ui_config);
  (void)App_TxControl_Apply();
  App_TxControl_GetSnapshot(&snapshot);
  g_ui_config = snapshot.dac_status.config;
  g_lo_freq_hz = snapshot.basic.freq_hz;
  g_lo_freq_dirty = 0U;
  g_mode_dirty = 0U;
  g_preset_freq_hz = App_TxControl_GetPresetFrequencyHz();
  App_LvglUiRefreshValues();
  App_LvglUiRefreshEditState();
  App_LvglUiRefreshState();
}

static void App_LvglUiOnSweepClicked(lv_event_t *e)
{
  AppTxControlSnapshot snapshot;

  (void)e;
  if (App_LvglUiIsDebugLocked() != 0U)
  {
    App_LvglUiRefreshState();
    return;
  }
  App_TxControl_GetSnapshot(&snapshot);
  (void)App_TxControl_SetConfig(&g_ui_config);
  if (snapshot.basic.sweep_on != 0U)
  {
    (void)App_TxControl_SetSweepEnabled(0U);
    (void)App_TxControl_Stop();
  }
  else
  {
    (void)App_TxControl_SetSweepEnabled(1U);
    (void)App_TxControl_Start();
  }

  App_Pe4302_SetHalfDbSteps(APP_PE4302_MAX_HALF_DB_STEPS);
  App_TxControl_GetSnapshot(&snapshot);
  g_lo_freq_hz = snapshot.basic.freq_hz;
  g_lo_freq_dirty = 0U;
  g_mode_dirty = 0U;
  App_LvglUiSetActiveSection(APP_UI_SECTION_SWEEP);
  g_sweep_period_ms = App_TxControl_GetSweepPeriodMs();
  g_sweep_start_hz = App_TxControl_GetSweepStartHz();
  g_sweep_stop_hz = App_TxControl_GetSweepStopHz();
  App_LvglUiRefreshValues();
  App_LvglUiRefreshEditState();
  App_LvglUiRefreshState();
}

static void App_LvglUiOnSweepStartClicked(lv_event_t *e)
{
  (void)e;
  if (App_LvglUiIsDebugLocked() != 0U)
  {
    App_LvglUiRefreshState();
    return;
  }
  g_sweep_start_hz = g_lo_freq_hz;
  (void)App_TxControl_SetSweepRangeHz(g_sweep_start_hz, g_sweep_stop_hz);
  g_sweep_start_hz = App_TxControl_GetSweepStartHz();
  g_sweep_stop_hz = App_TxControl_GetSweepStopHz();
  App_LvglUiRefreshValues();
  App_LvglUiRefreshState();
}

static void App_LvglUiOnSweepStopClicked(lv_event_t *e)
{
  (void)e;
  if (App_LvglUiIsDebugLocked() != 0U)
  {
    App_LvglUiRefreshState();
    return;
  }
  g_sweep_stop_hz = g_lo_freq_hz;
  (void)App_TxControl_SetSweepRangeHz(g_sweep_start_hz, g_sweep_stop_hz);
  g_sweep_start_hz = App_TxControl_GetSweepStartHz();
  g_sweep_stop_hz = App_TxControl_GetSweepStopHz();
  App_LvglUiRefreshValues();
  App_LvglUiRefreshState();
}

static void App_LvglUiOnSweepTimeClicked(lv_event_t *e)
{
  (void)e;
  if (App_LvglUiIsDebugLocked() != 0U)
  {
    App_LvglUiRefreshState();
    return;
  }

  g_sweep_period_ms += APP_TX_CONTROL_SWEEP_PERIOD_STEP_MS;
  if (g_sweep_period_ms > APP_TX_CONTROL_SWEEP_PERIOD_MAX_MS)
  {
    g_sweep_period_ms = APP_TX_CONTROL_SWEEP_PERIOD_MIN_MS;
  }

  (void)App_TxControl_SetSweepPeriodMs(g_sweep_period_ms);
  g_sweep_period_ms = App_TxControl_GetSweepPeriodMs();
  App_LvglUiRefreshValues();
  App_LvglUiRefreshState();
}

static void App_LvglUiOnRunClicked(lv_event_t *e)
{
  AppTxControlSnapshot snapshot;

  (void)e;

  if (App_LvglUiIsDebugLocked() != 0U)
  {
    App_LvglUiRefreshState();
    return;
  }

  if (g_active_section == APP_UI_SECTION_SYSTEM)
  {
    if (App_TxControl_GetCalibrationOutputEnabled() != 0U)
    {
      (void)App_TxControl_StopCalibrationOutput();
    }
    else
    {
      App_LvglUiApplySystemErrorNow();
      (void)App_TxControl_StartCalibrationOutput();
    }
    App_LvglUiRefreshValues();
    App_LvglUiRefreshEditState();
    App_LvglUiRefreshState();
    return;
  }

  App_TxControl_GetSnapshot(&snapshot);

  if (g_active_section == APP_UI_SECTION_SWEEP)
  {
    (void)App_TxControl_SetConfig(&g_ui_config);
    if (snapshot.basic.sweep_on != 0U)
    {
      (void)App_TxControl_SetSweepEnabled(0U);
      (void)App_TxControl_Stop();
    }
    else
    {
      (void)App_TxControl_SetSweepEnabled(1U);
      (void)App_TxControl_Start();
    }
    App_LvglUiRefreshState();
    return;
  }
  if (g_active_section == APP_UI_SECTION_MOD)
  {
    (void)App_TxControl_SetConfig(&g_ui_config);
    (void)App_TxControl_SetFrequencyHz(g_lo_freq_hz);
    if (snapshot.basic.tx_on != 0U)
    {
      (void)App_TxControl_Stop();
    }
    else
    {
      (void)App_TxControl_Start();
    }
    App_LvglUiRefreshState();
    return;
  }

  (void)App_TxControl_SetConfig(&g_ui_config);
  (void)App_TxControl_SetFrequencyHz(g_lo_freq_hz);
  if (snapshot.basic.tx_on != 0U)
  {
    (void)App_TxControl_Stop();
  }
  else
  {
    (void)App_TxControl_Start();
  }
  App_LvglUiRefreshState();
}

static void App_LvglUiOnDebugClicked(lv_event_t *e)
{
  uint8_t enable;

  (void)e;

  enable = (App_TxControl_GetDebugModeEnabled() != 0U) ? 0U : 1U;
  (void)App_TxControl_SetDebugModeEnabled(enable);
  App_LvglUiRefreshState();
}

static void App_LvglUiOnContinuousClicked(lv_event_t *e)
{
  (void)e;

  if (App_LvglUiIsDebugLocked() != 0U)
  {
    App_LvglUiRefreshState();
    return;
  }

  App_LvglUiResetModeConfig(APP_DAC_WAVE_MODE_CW);
  (void)App_TxControl_SetMode(APP_DAC_WAVE_MODE_CW);
  (void)App_TxControl_SetSweepEnabled(0U);
  (void)App_TxControl_Stop();
  App_LvglUiSetActiveSection(APP_UI_SECTION_SINGLE);
  g_mode_dirty = 1U;
  g_selected_field = APP_UI_DEFAULT_FIELD;
  g_digit_index = APP_UI_DEFAULT_DIGIT_INDEX;
  App_LvglUiEnsureEditableField();
  App_LvglUiRefreshValues();
  App_LvglUiRefreshEditState();
  App_LvglUiRefreshState();
}

static void App_LvglUiOnSweepSectionClicked(lv_event_t *e)
{
  (void)e;

  if (App_LvglUiIsDebugLocked() != 0U)
  {
    App_LvglUiRefreshState();
    return;
  }

  App_LvglUiResetModeConfig(APP_DAC_WAVE_MODE_CW);
  (void)App_TxControl_SetMode(APP_DAC_WAVE_MODE_CW);
  (void)App_TxControl_SetSweepEnabled(0U);
  (void)App_TxControl_Stop();
  App_LvglUiSetActiveSection(APP_UI_SECTION_SWEEP);
  g_mode_dirty = 1U;
  App_LvglUiEnsureEditableField();
  App_LvglUiRefreshValues();
  App_LvglUiRefreshEditState();
  App_LvglUiRefreshState();
}

static void App_LvglUiOnModClicked(lv_event_t *e)
{
  (void)e;

  if (App_LvglUiIsDebugLocked() != 0U)
  {
    App_LvglUiRefreshState();
    return;
  }

  App_LvglUiResetModeConfig(APP_DAC_WAVE_MODE_AM);
  (void)App_TxControl_SetMode(APP_DAC_WAVE_MODE_AM);
  g_mode_dirty = 1U;
  g_selected_field = APP_UI_DEFAULT_FIELD;
  g_digit_index = APP_UI_DEFAULT_DIGIT_INDEX;

  (void)App_TxControl_SetSweepEnabled(0U);
  (void)App_TxControl_Stop();
  App_LvglUiSetActiveSection(APP_UI_SECTION_MOD);
  App_LvglUiEnsureEditableField();
  App_LvglUiRefreshValues();
  App_LvglUiRefreshEditState();
  App_LvglUiRefreshState();
}

static void App_LvglUiOnSystemClicked(lv_event_t *e)
{
  (void)e;
  (void)App_TxControl_SetSweepEnabled(0U);
  (void)App_TxControl_Stop();
  g_system_lo_error_hz = App_TxControl_GetLoErrorHz();
  App_LvglUiSetActiveSection(APP_UI_SECTION_SYSTEM);
  (void)App_TxControl_StartCalibrationOutput();
  App_LvglUiRefreshValues();
  App_LvglUiRefreshEditState();
  App_LvglUiRefreshState();
}

static void App_LvglUiOnKeypadDigitClicked(lv_event_t *e)
{
  const char *text = (const char *)lv_event_get_user_data(e);
  size_t len;

  if ((text == NULL) || (text[0] == '\0'))
  {
    return;
  }

  len = strlen(g_keypad_input);
  if ((len + 1U) >= sizeof(g_keypad_input))
  {
    return;
  }

  if ((text[0] == '.') &&
      (g_keypad_target != APP_UI_KEYPAD_LO_FREQ) &&
      (g_keypad_target != APP_UI_KEYPAD_MOD_FC) &&
      (g_keypad_target != APP_UI_KEYPAD_SWEEP_START) &&
      (g_keypad_target != APP_UI_KEYPAD_SWEEP_STOP) &&
      (g_keypad_target != APP_UI_KEYPAD_SWEEP_TIME))
  {
    return;
  }

  if ((text[0] == '.') && (strchr(g_keypad_input, '.') != NULL))
  {
    return;
  }

  if ((text[0] == '.') && (len == 0U))
  {
    g_keypad_input[len++] = '0';
  }

  g_keypad_input[len] = text[0];
  g_keypad_input[len + 1U] = '\0';
  App_LvglUiRefreshKeypad();
}

static void App_LvglUiOnKeypadBackClicked(lv_event_t *e)
{
  size_t len;

  (void)e;
  len = strlen(g_keypad_input);
  if (len == 0U)
  {
    return;
  }

  g_keypad_input[len - 1U] = '\0';
  App_LvglUiRefreshKeypad();
}

static void App_LvglUiOnKeypadCancelClicked(lv_event_t *e)
{
  (void)e;
  App_LvglUiCloseKeypad();
}

static void App_LvglUiOnKeypadOkClicked(lv_event_t *e)
{
  uint32_t value = 0U;

  (void)e;
  if (App_LvglUiTryParseKeypadValue(g_keypad_target, g_keypad_input, &value) == 0U)
  {
    App_LvglUiRefreshKeypad();
    return;
  }

  if ((g_keypad_target == APP_UI_KEYPAD_LO_FREQ) || (g_keypad_target == APP_UI_KEYPAD_MOD_FC))
  {
    App_LvglUiSetFieldValue(APP_UI_FIELD_LO_FREQ, value);
    App_LvglUiCloseKeypad();
    App_LvglUiApplyConfig();
    return;
  }
  if ((g_keypad_target == APP_UI_KEYPAD_AMP) || (g_keypad_target == APP_UI_KEYPAD_SWEEP_AMP))
  {
    App_LvglUiSetFieldValue(APP_UI_FIELD_VPP, value);
    App_LvglUiCloseKeypad();
    App_LvglUiApplyConfig();
    return;
  }
  if (g_keypad_target == APP_UI_KEYPAD_MOD_FREQ)
  {
    App_LvglUiSetFieldValue(APP_UI_FIELD_RATE, value);
    App_LvglUiCloseKeypad();
    App_LvglUiApplyConfig();
    return;
  }
  if (g_keypad_target == APP_UI_KEYPAD_MOD_DEPTH)
  {
    App_LvglUiSetFieldValue(APP_UI_FIELD_PARAM, value);
    App_LvglUiCloseKeypad();
    App_LvglUiApplyConfig();
    return;
  }
  if (g_keypad_target == APP_UI_KEYPAD_SWEEP_START)
  {
    g_sweep_start_hz = App_TxControl_ClampFrequencyHz(value);
    (void)App_TxControl_SetSweepRangeHz(g_sweep_start_hz, g_sweep_stop_hz);
    g_sweep_start_hz = App_TxControl_GetSweepStartHz();
    g_sweep_stop_hz = App_TxControl_GetSweepStopHz();
    App_LvglUiCloseKeypad();
    App_LvglUiRefreshValues();
    App_LvglUiRefreshState();
    return;
  }
  if (g_keypad_target == APP_UI_KEYPAD_SWEEP_STOP)
  {
    g_sweep_stop_hz = App_TxControl_ClampFrequencyHz(value);
    (void)App_TxControl_SetSweepRangeHz(g_sweep_start_hz, g_sweep_stop_hz);
    g_sweep_start_hz = App_TxControl_GetSweepStartHz();
    g_sweep_stop_hz = App_TxControl_GetSweepStopHz();
    App_LvglUiCloseKeypad();
    App_LvglUiRefreshValues();
    App_LvglUiRefreshState();
    return;
  }
  g_sweep_period_ms = value;
  (void)App_TxControl_SetSweepPeriodMs(g_sweep_period_ms);
  g_sweep_period_ms = App_TxControl_GetSweepPeriodMs();
  App_LvglUiCloseKeypad();
  App_LvglUiRefreshValues();
  App_LvglUiRefreshState();
}

static void App_LvglUiOnKeypadDeleteClicked(lv_event_t *e)
{
  (void)e;
  g_keypad_input[0] = '\0';
  App_LvglUiRefreshKeypad();
}

static int App_LvglUiSendCurrentModeToWinner(void)
{
  uint32_t winner_lo_freq_hz = App_LvglUiGetWinnerLoFrequencyHz();
  AppWinnerBridgeModeRequest request;

  if (App_LvglUiIsCwMode(g_ui_config.mode) != 0U)
  {
    return 0;
  }

  memset(&request, 0, sizeof(request));
  request.mode = g_ui_config.mode;
  request.lo_freq_hz = winner_lo_freq_hz;
  request.lo_amp_code = APP_UI_WINNER_LO_AMP_DEFAULT;
  request.q_gain_permille = APP_UI_WINNER_QG_DEFAULT;
  request.q_phase_deg = APP_UI_WINNER_QP_DEFAULT;
  request.i_trim = APP_UI_WINNER_IO_DEFAULT;
  request.q_trim = APP_UI_WINNER_QO_DEFAULT;
  request.offset_code = 8192U;
  request.amp_code = 1200U;

  switch (g_ui_config.mode)
  {
    case APP_DAC_WAVE_MODE_AM:
      request.rate_hz = g_ui_config.mod_freq_hz;
      request.param_u32 = (uint32_t)g_ui_config.am_depth_percent * 10U;
      return App_WinnerBridge_SendModeRequestDirect(&request);
    case APP_DAC_WAVE_MODE_FM:
      request.rate_hz = g_ui_config.mod_freq_hz;
      request.param_u32 = g_ui_config.fm_deviation_hz;
      return App_WinnerBridge_SendModeRequestDirect(&request);
    case APP_DAC_WAVE_MODE_2ASK:
      request.rate_hz = g_ui_config.symbol_rate_bps;
      request.param_u32 = 1000U;
      return App_WinnerBridge_SendModeRequestDirect(&request);
    case APP_DAC_WAVE_MODE_2FSK:
      request.rate_hz = g_ui_config.symbol_rate_bps;
      request.param_u32 = g_ui_config.fsk_shift_hz;
      return App_WinnerBridge_SendModeRequestDirect(&request);
    case APP_DAC_WAVE_MODE_2PSK:
      request.rate_hz = g_ui_config.symbol_rate_bps;
      return App_WinnerBridge_SendModeRequestDirect(&request);
    default:
      return -1;
  }
}

static uint32_t App_LvglUiGetWinnerLoFrequencyHz(void)
{
  uint64_t winner_lo_freq_hz = g_lo_freq_hz;

  if (g_ui_config.mode == APP_DAC_WAVE_MODE_2FSK)
  {
    winner_lo_freq_hz += ((uint64_t)g_ui_config.fsk_shift_hz / 2ULL);
  }

  if (winner_lo_freq_hz > 0xFFFFFFFFULL)
  {
    winner_lo_freq_hz = 0xFFFFFFFFULL;
  }

  return App_TxControl_ClampFrequencyHz((uint32_t)winner_lo_freq_hz);
}

static uint8_t App_LvglUiIsDebugLocked(void)
{
  return App_TxControl_GetDebugModeEnabled();
}

static uint32_t App_LvglUiGetFskNextFrequencyHz(void)
{
  return App_TxControl_ClampFrequencyHz(g_lo_freq_hz + g_ui_config.fsk_shift_hz);
}

static const char *App_LvglUiModeText(AppDacWavegenMode mode)
{
  switch (mode)
  {
    case APP_DAC_WAVE_MODE_AM:
      return "AM";
    case APP_DAC_WAVE_MODE_FM:
      return "FM";
    case APP_DAC_WAVE_MODE_2ASK:
      return "2ASK";
    case APP_DAC_WAVE_MODE_2PSK:
      return "2PSK";
    case APP_DAC_WAVE_MODE_2FSK:
      return "2FSK";
    case APP_DAC_WAVE_MODE_CW:
      return "CW";
    default:
      return "Unknown";
  }
}
