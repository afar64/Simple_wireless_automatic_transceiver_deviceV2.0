#include "app_lvgl_ui.h"

#include "app_tx_control.h"
#include "app_winner_bridge.h"
#include "app_dds_ctrl.h"

#include "lvgl.h"

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
static lv_obj_t *g_mode_dropdown = NULL;
static lv_obj_t *g_lo_name_label = NULL;
static lv_obj_t *g_lo_value_label = NULL;
static lv_obj_t *g_vpp_name_label = NULL;
static lv_obj_t *g_vpp_value_label = NULL;
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

static const uint32_t s_lo_steps_hz[] = {APP_TX_CONTROL_FREQ_STEP_HZ};
static const uint32_t s_amp_steps_mv[] = {APP_UI_AMP_RMS_STEP_MV};
static const uint32_t s_rate_steps_hz[] = {10000UL, 1000UL, 100UL, 10UL, 1UL};
static const uint32_t s_param_steps_pct[] = {10U, 1U};

static uint32_t App_LvglUiInternalToAmpRmsMv(uint16_t internal_mv);
static uint16_t App_LvglUiAmpRmsMvToInternal(uint32_t amp_rms_mv);

static lv_obj_t *App_LvglUiCreatePanel(lv_obj_t *parent);
static void App_LvglUiCreateTitle(lv_obj_t *parent);
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
static void App_LvglUiResetModeConfig(AppDacWavegenMode mode);
static uint8_t App_LvglUiIsCwMode(AppDacWavegenMode mode);
static void App_LvglUiEnsureEditableField(void);
static void App_LvglUiRefreshValues(void);
static void App_LvglUiRefreshEditState(void);
static void App_LvglUiRefreshState(void);
static void App_LvglUiRefreshModeVisibility(void);
static uint8_t App_LvglUiIsFieldEditable(AppUiEditField field);
static const char *App_LvglUiFieldName(AppUiEditField field);
static uint32_t App_LvglUiGetFieldStepCount(AppUiEditField field);
static uint32_t App_LvglUiGetFieldStepValue(AppUiEditField field, uint8_t index);
static void App_LvglUiGetFieldRange(AppUiEditField field, uint32_t *min_value, uint32_t *max_value);
static uint32_t App_LvglUiGetFieldValue(AppUiEditField field);
static void App_LvglUiSetFieldValue(AppUiEditField field, uint32_t value);
static void App_LvglUiCycleField(void);
static void App_LvglUiCycleDigit(void);
static void App_LvglUiAdjustSelectedField(int32_t direction);
static void App_LvglUiApplyConfig(void);
static void App_LvglUiOnModeChanged(lv_event_t *e);
static void App_LvglUiOnFieldClicked(lv_event_t *e);
static void App_LvglUiOnDigitClicked(lv_event_t *e);
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
static const char *App_LvglUiModeText(AppDacWavegenMode mode);
static int App_LvglUiSendCurrentModeToWinner(void);
static uint8_t App_LvglUiIsDebugLocked(void);

#define APP_UI_WINNER_QG_DEFAULT      980U
#define APP_UI_WINNER_QP_DEFAULT        0
#define APP_UI_WINNER_IO_DEFAULT      (-50)
#define APP_UI_WINNER_QO_DEFAULT      100
#define APP_UI_WINNER_LO_AMP_DEFAULT  512U
#define APP_UI_WINNER_FSK_LO_OFFSET_HZ 10000UL

void App_LvglUiInit(void)
{
  lv_obj_t *screen = lv_screen_active();
  lv_obj_t *panel;
  AppTxControlSnapshot snapshot;

  lv_obj_set_style_bg_color(screen, lv_color_hex(0x101216), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  App_TxControl_Init();
  App_TxControl_GetSnapshot(&snapshot);
  g_ui_config = snapshot.dac_status.config;
  g_lo_freq_hz = snapshot.basic.freq_hz;
  g_preset_freq_hz = App_TxControl_GetPresetFrequencyHz();
  g_sweep_period_ms = App_TxControl_GetSweepPeriodMs();
  g_sweep_start_hz = App_TxControl_GetSweepStartHz();
  g_sweep_stop_hz = App_TxControl_GetSweepStopHz();

  panel = App_LvglUiCreatePanel(screen);
  App_LvglUiCreateTitle(panel);
  App_LvglUiCreateDebugBadge(panel);
  App_LvglUiCreateSelfTestBadge(panel);
  App_LvglUiCreatePresetBadge(panel);
  App_LvglUiCreateSweepTimeBadge(panel);
  App_LvglUiCreateSweepRangeBadge(panel);
  App_LvglUiCreateValueRow(panel, "LO freq", 74, &g_lo_name_label, &g_lo_value_label);
  App_LvglUiCreateModeRow(panel, g_ui_config.mode, 116);
  App_LvglUiCreateValueRow(panel, "Amp", 160, &g_vpp_name_label, &g_vpp_value_label);
  App_LvglUiCreateValueRow(panel, "Rate", 206, &g_rate_name_label, &g_rate_value_label);
  App_LvglUiCreateValueRow(panel, "Param", 252, &g_param_name_label, &g_param_value_label);
  App_LvglUiCreateButtons(panel);

  App_LvglUiEnsureEditableField();
  App_LvglUiRefreshValues();
  App_LvglUiRefreshEditState();
  App_LvglUiRefreshState();
}

void App_LvglUiPoll(void)
{
  AppTxControlSnapshot snapshot;
  uint32_t now_ms = lv_tick_get();
  uint8_t ui_changed = 0U;

  App_TxControl_GetSnapshot(&snapshot);
  if (memcmp(&g_ui_config, &snapshot.dac_status.config, sizeof(g_ui_config)) != 0)
  {
    if (g_mode_dirty == 0U)
    {
      g_ui_config = snapshot.dac_status.config;
      g_lo_freq_dirty = 0U;
      ui_changed = 1U;
    }
  }

  if (snapshot.basic.freq_hz != g_lo_freq_hz)
  {
    if ((snapshot.basic.sweep_on != 0U) || (g_lo_freq_dirty == 0U))
    {
      g_lo_freq_hz = snapshot.basic.freq_hz;
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
      lv_dropdown_set_selected(g_mode_dropdown, (uint16_t)g_ui_config.mode);
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
  lv_obj_set_style_bg_color(panel, lv_color_hex(0x20242B), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(0x4E5968), 0);
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
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

  lv_label_set_text(subtitle, "DAC1_OUT1 = I / DAC1_OUT2 = Q");
  lv_obj_set_style_text_color(subtitle, lv_color_hex(0xBFC7D5), 0);
  lv_obj_align_to(subtitle, title, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
}

static void App_LvglUiCreateModeRow(lv_obj_t *parent, AppDacWavegenMode mode, int32_t y)
{
  lv_obj_t *name_label = lv_label_create(parent);

  lv_label_set_text(name_label, "Mode");
  lv_obj_set_width(name_label, APP_UI_LABEL_W);
  lv_obj_set_style_text_color(name_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(name_label, LV_ALIGN_TOP_LEFT, 36, y + 6);

  g_mode_dropdown = lv_dropdown_create(parent);
  lv_obj_set_width(g_mode_dropdown, 180);
  lv_dropdown_set_options(g_mode_dropdown, "AM\nFM\n2ASK\n2PSK\n2FSK\nCW");
  lv_dropdown_set_selected(g_mode_dropdown, (uint16_t)mode);
  lv_obj_align(g_mode_dropdown, LV_ALIGN_TOP_LEFT, 180, y);
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
  lv_obj_set_style_text_color(*name_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(*name_label, LV_ALIGN_TOP_LEFT, 36, y);

  *value_label = lv_label_create(parent);
  lv_obj_set_width(*value_label, APP_UI_VALUE_W);
  lv_obj_set_style_text_color(*value_label, lv_color_hex(0xDDE6F3), 0);
  lv_obj_align(*value_label, LV_ALIGN_TOP_LEFT, 180, y);
}

static void App_LvglUiCreatePresetBadge(lv_obj_t *parent)
{
  g_preset_name_label = lv_label_create(parent);
  lv_label_set_text(g_preset_name_label, "Preset");
  lv_obj_set_style_text_color(g_preset_name_label, lv_color_hex(0xBFC7D5), 0);
  lv_obj_align(g_preset_name_label, LV_ALIGN_TOP_RIGHT, -180, 26);

  g_preset_value_label = lv_label_create(parent);
  lv_obj_set_width(g_preset_value_label, 150);
  lv_obj_set_style_text_align(g_preset_value_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_color(g_preset_value_label, lv_color_hex(0xDDE6F3), 0);
  lv_obj_align(g_preset_value_label, LV_ALIGN_TOP_RIGHT, -24, 26);
}

static void App_LvglUiCreateDebugBadge(lv_obj_t *parent)
{
  g_debug_status_label = lv_label_create(parent);
  lv_obj_set_style_text_color(g_debug_status_label, lv_color_hex(0xBFC7D5), 0);
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
  lv_obj_set_style_text_color(g_ad9959_status_label, lv_color_hex(0xDDE6F3), 0);
  lv_obj_align(g_ad9959_status_label, LV_ALIGN_TOP_RIGHT, -24, 124);

  g_f429_status_label = lv_label_create(parent);
  lv_obj_set_style_text_color(g_f429_status_label, lv_color_hex(0xDDE6F3), 0);
  lv_obj_align(g_f429_status_label, LV_ALIGN_TOP_RIGHT, -24, 148);
}

static void App_LvglUiCreateSweepTimeBadge(lv_obj_t *parent)
{
  g_sweep_time_name_label = lv_label_create(parent);
  lv_label_set_text(g_sweep_time_name_label, "Sweep");
  lv_obj_set_style_text_color(g_sweep_time_name_label, lv_color_hex(0xBFC7D5), 0);
  lv_obj_align(g_sweep_time_name_label, LV_ALIGN_TOP_RIGHT, -180, 136);

  g_sweep_time_value_label = lv_label_create(parent);
  lv_obj_set_width(g_sweep_time_value_label, 150);
  lv_obj_set_style_text_align(g_sweep_time_value_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_color(g_sweep_time_value_label, lv_color_hex(0xDDE6F3), 0);
  lv_obj_align(g_sweep_time_value_label, LV_ALIGN_TOP_RIGHT, -24, 136);
}

static void App_LvglUiCreateSweepRangeBadge(lv_obj_t *parent)
{
  g_sweep_start_name_label = lv_label_create(parent);
  lv_label_set_text(g_sweep_start_name_label, "S-Start");
  lv_obj_set_style_text_color(g_sweep_start_name_label, lv_color_hex(0xBFC7D5), 0);
  lv_obj_align(g_sweep_start_name_label, LV_ALIGN_TOP_RIGHT, -180, 162);

  g_sweep_start_value_label = lv_label_create(parent);
  lv_obj_set_width(g_sweep_start_value_label, 150);
  lv_obj_set_style_text_align(g_sweep_start_value_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_color(g_sweep_start_value_label, lv_color_hex(0xDDE6F3), 0);
  lv_obj_align(g_sweep_start_value_label, LV_ALIGN_TOP_RIGHT, -24, 162);

  g_sweep_stop_name_label = lv_label_create(parent);
  lv_label_set_text(g_sweep_stop_name_label, "S-Stop");
  lv_obj_set_style_text_color(g_sweep_stop_name_label, lv_color_hex(0xBFC7D5), 0);
  lv_obj_align(g_sweep_stop_name_label, LV_ALIGN_TOP_RIGHT, -180, 188);

  g_sweep_stop_value_label = lv_label_create(parent);
  lv_obj_set_width(g_sweep_stop_value_label, 150);
  lv_obj_set_style_text_align(g_sweep_stop_value_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_color(g_sweep_stop_value_label, lv_color_hex(0xDDE6F3), 0);
  lv_obj_align(g_sweep_stop_value_label, LV_ALIGN_TOP_RIGHT, -24, 188);
}

static void App_LvglUiCreateButtons(lv_obj_t *parent)
{
  lv_obj_t *field_button = lv_button_create(parent);
  lv_obj_t *field_label = lv_label_create(field_button);
  lv_obj_t *digit_button = lv_button_create(parent);
  lv_obj_t *digit_label = lv_label_create(digit_button);
  lv_obj_t *dec_button = lv_button_create(parent);
  lv_obj_t *dec_label = lv_label_create(dec_button);
  lv_obj_t *inc_button = lv_button_create(parent);
  lv_obj_t *inc_label = lv_label_create(inc_button);
  lv_obj_t *preset_save_label;
  lv_obj_t *preset_recall_label;
  lv_obj_t *sweep_time_label;
  lv_obj_t *sweep_start_label;
  lv_obj_t *sweep_stop_label;
  lv_obj_t *apply_button = lv_button_create(parent);
  lv_obj_t *apply_label = lv_label_create(apply_button);
  lv_obj_t *run_button = lv_button_create(parent);

  g_edit_label = lv_label_create(parent);
  lv_obj_set_width(g_edit_label, 460);
  lv_obj_set_style_text_color(g_edit_label, lv_color_hex(0xBFC7D5), 0);
  lv_obj_align(g_edit_label, LV_ALIGN_BOTTOM_LEFT, 36, -84);

  g_state_label = lv_label_create(parent);
  lv_obj_set_width(g_state_label, 220);
  lv_obj_set_style_text_color(g_state_label, lv_color_hex(0xBFC7D5), 0);
  lv_obj_align(g_state_label, LV_ALIGN_BOTTOM_RIGHT, -36, -84);

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

  lv_obj_set_size(field_button, 92, 42);
  lv_obj_align(field_button, LV_ALIGN_BOTTOM_LEFT, 36, -24);
  lv_obj_add_event_cb(field_button, App_LvglUiOnFieldClicked, LV_EVENT_CLICKED, NULL);
  lv_label_set_text(field_label, "Field");
  lv_obj_center(field_label);

  lv_obj_set_size(digit_button, 84, 42);
  lv_obj_align(digit_button, LV_ALIGN_BOTTOM_LEFT, 140, -24);
  lv_obj_add_event_cb(digit_button, App_LvglUiOnDigitClicked, LV_EVENT_CLICKED, NULL);
  lv_label_set_text(digit_label, "Digit");
  lv_obj_center(digit_label);

  lv_obj_set_size(dec_button, 60, 42);
  lv_obj_align(dec_button, LV_ALIGN_BOTTOM_LEFT, 240, -24);
  lv_obj_add_event_cb(dec_button, App_LvglUiOnDecClicked, LV_EVENT_CLICKED, NULL);
  lv_label_set_text(dec_label, "-");
  lv_obj_center(dec_label);

  lv_obj_set_size(inc_button, 60, 42);
  lv_obj_align(inc_button, LV_ALIGN_BOTTOM_LEFT, 316, -24);
  lv_obj_add_event_cb(inc_button, App_LvglUiOnIncClicked, LV_EVENT_CLICKED, NULL);
  lv_label_set_text(inc_label, "+");
  lv_obj_center(inc_label);

  g_preset_save_button = lv_button_create(parent);
  preset_save_label = lv_label_create(g_preset_save_button);
  lv_obj_set_size(g_preset_save_button, 78, 42);
  lv_obj_align(g_preset_save_button, LV_ALIGN_BOTTOM_LEFT, 392, -24);
  lv_obj_add_event_cb(g_preset_save_button, App_LvglUiOnPresetSaveClicked, LV_EVENT_CLICKED, NULL);
  lv_label_set_text(preset_save_label, "Save");
  lv_obj_center(preset_save_label);

  g_preset_recall_button = lv_button_create(parent);
  preset_recall_label = lv_label_create(g_preset_recall_button);
  lv_obj_set_size(g_preset_recall_button, 78, 42);
  lv_obj_align(g_preset_recall_button, LV_ALIGN_BOTTOM_LEFT, 486, -24);
  lv_obj_add_event_cb(g_preset_recall_button, App_LvglUiOnPresetRecallClicked, LV_EVENT_CLICKED, NULL);
  lv_label_set_text(preset_recall_label, "Recall");
  lv_obj_center(preset_recall_label);

  lv_obj_set_size(apply_button, 66, 42);
  lv_obj_align(apply_button, LV_ALIGN_BOTTOM_LEFT, 572, -24);
  lv_obj_add_event_cb(apply_button, App_LvglUiOnApplyClicked, LV_EVENT_CLICKED, NULL);
  lv_label_set_text(apply_label, "Apply");
  lv_obj_center(apply_label);

  lv_obj_set_size(run_button, 60, 42);
  lv_obj_align(run_button, LV_ALIGN_BOTTOM_LEFT, 650, -24);
  lv_obj_add_event_cb(run_button, App_LvglUiOnRunClicked, LV_EVENT_CLICKED, NULL);
  g_run_button_label = lv_label_create(run_button);
  lv_obj_center(g_run_button_label);
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

static void App_LvglUiRefreshValues(void)
{
  lv_color_t normal_name = lv_color_hex(0xFFFFFF);
  lv_color_t normal_value = lv_color_hex(0xDDE6F3);
  lv_color_t selected = lv_color_hex(0x68D391);
  AppDacWavegenMode mode = g_ui_config.mode;

  App_LvglUiRefreshModeVisibility();

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
  if (App_LvglUiIsCwMode(mode) != 0U)
  {
    lv_label_set_text(g_vpp_name_label, "Amp");
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
        lv_label_set_text(g_param_name_label, "Depth");
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
        lv_label_set_text(g_param_name_label, "Shift");
        lv_label_set_text_fmt(g_param_value_label, "%lu Hz", (unsigned long)g_ui_config.fsk_shift_hz);
        break;
    }
  }

  lv_obj_set_style_text_color(g_lo_name_label, normal_name, 0);
  lv_obj_set_style_text_color(g_lo_value_label, normal_value, 0);
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
  lv_obj_set_style_text_color(g_rate_name_label, (App_LvglUiIsCwMode(mode) != 0U) ? lv_color_hex(0x6B7280) : normal_name, 0);
  lv_obj_set_style_text_color(g_rate_value_label, (App_LvglUiIsCwMode(mode) != 0U) ? lv_color_hex(0x6B7280) : normal_value, 0);
  lv_obj_set_style_text_color(g_param_name_label, (App_LvglUiIsCwMode(mode) != 0U) ? lv_color_hex(0x6B7280) : normal_name, 0);
  lv_obj_set_style_text_color(g_param_value_label, (App_LvglUiIsCwMode(mode) != 0U) ? lv_color_hex(0x6B7280) : normal_value, 0);

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
  debug_locked = App_LvglUiIsDebugLocked();
  dds_status = AppDDS_GetStatus();

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

static void App_LvglUiRefreshModeVisibility(void)
{
  uint8_t is_cw = App_LvglUiIsCwMode(g_ui_config.mode);

  if (g_preset_name_label != NULL)
  {
    if (is_cw != 0U)
    {
      lv_obj_clear_flag(g_preset_name_label, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(g_preset_value_label, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(g_sweep_time_name_label, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(g_sweep_time_value_label, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(g_sweep_start_name_label, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(g_sweep_start_value_label, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(g_sweep_stop_name_label, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(g_sweep_stop_value_label, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(g_preset_save_button, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(g_preset_recall_button, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(g_sweep_button, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(g_sweep_time_button, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(g_sweep_start_button, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(g_sweep_stop_button, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
      lv_obj_add_flag(g_preset_name_label, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(g_preset_value_label, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(g_sweep_time_name_label, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(g_sweep_time_value_label, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(g_sweep_start_name_label, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(g_sweep_start_value_label, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(g_sweep_stop_name_label, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(g_sweep_stop_value_label, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(g_preset_save_button, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(g_preset_recall_button, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(g_sweep_button, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(g_sweep_time_button, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(g_sweep_start_button, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(g_sweep_stop_button, LV_OBJ_FLAG_HIDDEN);
    }
  }
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
      return "LO freq";
    case APP_UI_FIELD_VPP:
      return "Amp";
    case APP_UI_FIELD_RATE:
      return "Rate";
    case APP_UI_FIELD_PARAM:
    default:
      return "Param";
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
        g_ui_config.fsk_shift_hz = value;
      }
      break;
    default:
      break;
  }
}

static void App_LvglUiCycleField(void)
{
  uint32_t tries = 0U;

  do
  {
    g_selected_field = (AppUiEditField)((g_selected_field + 1U) % APP_UI_FIELD_COUNT);
    tries++;
  } while ((tries < APP_UI_FIELD_COUNT) && (App_LvglUiIsFieldEditable(g_selected_field) == 0U));

  g_digit_index = 0U;
}

static void App_LvglUiCycleDigit(void)
{
  uint32_t count = App_LvglUiGetFieldStepCount(g_selected_field);

  if (count == 0U)
  {
    g_digit_index = 0U;
    return;
  }

  g_digit_index = (uint8_t)((g_digit_index + 1U) % count);
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
  AppTxControlSnapshot snapshot;

  (void)App_TxControl_SetConfig(&g_ui_config);
  (void)App_TxControl_SetFrequencyHz(g_lo_freq_hz);
  (void)App_TxControl_Apply();
  (void)App_LvglUiSendCurrentModeToWinner();

  App_TxControl_GetSnapshot(&snapshot);
  g_ui_config = snapshot.dac_status.config;
  g_lo_freq_hz = snapshot.basic.freq_hz;
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
  App_LvglUiCycleField();
  App_LvglUiEnsureEditableField();
  App_LvglUiRefreshValues();
  App_LvglUiRefreshEditState();
}

static void App_LvglUiOnDigitClicked(lv_event_t *e)
{
  (void)e;
  if (App_LvglUiIsDebugLocked() != 0U)
  {
    App_LvglUiRefreshState();
    return;
  }
  App_LvglUiCycleDigit();
  App_LvglUiRefreshEditState();
}

static void App_LvglUiOnDecClicked(lv_event_t *e)
{
  (void)e;
  if (App_LvglUiIsDebugLocked() != 0U)
  {
    App_LvglUiRefreshState();
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
  App_LvglUiApplyConfig();
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
  }
  else
  {
    (void)App_TxControl_SetSweepEnabled(1U);
  }

  App_TxControl_GetSnapshot(&snapshot);
  g_lo_freq_hz = snapshot.basic.freq_hz;
  g_lo_freq_dirty = 0U;
  g_mode_dirty = 0U;
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

  (void)App_TxControl_SetConfig(&g_ui_config);
  (void)App_TxControl_SetFrequencyHz(g_lo_freq_hz);
  App_TxControl_GetSnapshot(&snapshot);

  if (snapshot.basic.tx_on != 0U)
  {
    (void)App_TxControl_Stop();
  }
  else
  {
    (void)App_TxControl_Start();
    (void)App_LvglUiSendCurrentModeToWinner();
  }

  App_TxControl_GetSnapshot(&snapshot);
  g_lo_freq_hz = snapshot.basic.freq_hz;
  g_lo_freq_dirty = 0U;
  g_mode_dirty = 0U;
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

static int App_LvglUiSendCurrentModeToWinner(void)
{
  uint32_t winner_lo_freq_hz = g_lo_freq_hz;

  if (App_LvglUiIsCwMode(g_ui_config.mode) != 0U)
  {
    return 0;
  }

  if (g_ui_config.mode == APP_DAC_WAVE_MODE_2FSK)
  {
    winner_lo_freq_hz += APP_UI_WINNER_FSK_LO_OFFSET_HZ;
  }

  switch (g_ui_config.mode)
  {
    case APP_DAC_WAVE_MODE_AM:
      return App_WinnerBridge_SendAmSequence(winner_lo_freq_hz,
                                             APP_UI_WINNER_LO_AMP_DEFAULT,
                                             APP_UI_WINNER_QG_DEFAULT,
                                             APP_UI_WINNER_QP_DEFAULT,
                                             APP_UI_WINNER_IO_DEFAULT,
                                             APP_UI_WINNER_QO_DEFAULT,
                                             g_ui_config.mod_freq_hz,
                                             8192U,
                                             1200U,
                                             g_ui_config.am_depth_percent * 10U);
    case APP_DAC_WAVE_MODE_FM:
      return App_WinnerBridge_SendFmSequence(winner_lo_freq_hz,
                                             APP_UI_WINNER_LO_AMP_DEFAULT,
                                             APP_UI_WINNER_QG_DEFAULT,
                                             APP_UI_WINNER_QP_DEFAULT,
                                             APP_UI_WINNER_IO_DEFAULT,
                                             APP_UI_WINNER_QO_DEFAULT,
                                             g_ui_config.mod_freq_hz,
                                             8192U,
                                             1200U,
                                             g_ui_config.fm_deviation_hz);
    case APP_DAC_WAVE_MODE_2ASK:
      return App_WinnerBridge_SendAskSequence(winner_lo_freq_hz,
                                              APP_UI_WINNER_LO_AMP_DEFAULT,
                                              APP_UI_WINNER_QG_DEFAULT,
                                              APP_UI_WINNER_QP_DEFAULT,
                                              APP_UI_WINNER_IO_DEFAULT,
                                              APP_UI_WINNER_QO_DEFAULT,
                                              g_ui_config.symbol_rate_bps,
                                              8192U,
                                              1200U,
                                              1000U);
    case APP_DAC_WAVE_MODE_2FSK:
      return App_WinnerBridge_SendFskSequence(winner_lo_freq_hz,
                                              APP_UI_WINNER_LO_AMP_DEFAULT,
                                              APP_UI_WINNER_QG_DEFAULT,
                                              APP_UI_WINNER_QP_DEFAULT,
                                              APP_UI_WINNER_IO_DEFAULT,
                                              APP_UI_WINNER_QO_DEFAULT,
                                              g_ui_config.symbol_rate_bps,
                                              8192U,
                                              1200U,
                                              g_ui_config.fsk_shift_hz);
    case APP_DAC_WAVE_MODE_2PSK:
      return App_WinnerBridge_SendPskSequence(winner_lo_freq_hz,
                                              APP_UI_WINNER_LO_AMP_DEFAULT,
                                              APP_UI_WINNER_QG_DEFAULT,
                                              APP_UI_WINNER_QP_DEFAULT,
                                              APP_UI_WINNER_IO_DEFAULT,
                                              APP_UI_WINNER_QO_DEFAULT,
                                              g_ui_config.symbol_rate_bps,
                                              8192U,
                                              1200U);
    default:
      return -1;
  }
}

static uint8_t App_LvglUiIsDebugLocked(void)
{
  return App_TxControl_GetDebugModeEnabled();
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
