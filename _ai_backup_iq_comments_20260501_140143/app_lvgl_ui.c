#include "app_lvgl_ui.h"

#include "app_dac_wavegen.h"

#include "lvgl.h"

#define APP_UI_PANEL_W 720
#define APP_UI_PANEL_H 400
#define APP_UI_LABEL_W 112
#define APP_UI_VALUE_W 128
#define APP_UI_CONTROL_W 390

static lv_obj_t *g_mode_dropdown = NULL;
static lv_obj_t *g_vpp_slider = NULL;
static lv_obj_t *g_rate_slider = NULL;
static lv_obj_t *g_param_slider = NULL;
static lv_obj_t *g_vpp_value_label = NULL;
static lv_obj_t *g_rate_name_label = NULL;
static lv_obj_t *g_rate_value_label = NULL;
static lv_obj_t *g_param_name_label = NULL;
static lv_obj_t *g_param_value_label = NULL;
static lv_obj_t *g_state_label = NULL;
static lv_obj_t *g_run_button_label = NULL;

static lv_obj_t *App_LvglUiCreatePanel(lv_obj_t *parent);
static void App_LvglUiCreateTitle(lv_obj_t *parent);
static void App_LvglUiCreateModeRow(lv_obj_t *parent, AppDacWavegenMode mode, int32_t y);
static void App_LvglUiCreateSliderRow(lv_obj_t *parent,
                                      const char *name,
                                      int32_t min,
                                      int32_t max,
                                      int32_t value,
                                      int32_t y,
                                      lv_obj_t **name_label,
                                      lv_obj_t **slider,
                                      lv_obj_t **value_label);
static void App_LvglUiCreateButtons(lv_obj_t *parent);
static void App_LvglUiApplyModeToControls(AppDacWavegenMode mode);
static void App_LvglUiRefreshLabels(void);
static void App_LvglUiRefreshState(void);
static void App_LvglUiReadConfig(AppDacWavegenConfig *config);
static void App_LvglUiApplyConfig(void);
static void App_LvglUiOnModeChanged(lv_event_t *e);
static void App_LvglUiOnSliderChanged(lv_event_t *e);
static void App_LvglUiOnApplyClicked(lv_event_t *e);
static void App_LvglUiOnRunClicked(lv_event_t *e);
static const char *App_LvglUiModeText(AppDacWavegenMode mode);

void App_LvglUiInit(void)
{
  lv_obj_t *screen = lv_screen_active();
  lv_obj_t *panel = NULL;
  AppDacWavegenStatus status;

  lv_obj_set_style_bg_color(screen, lv_color_hex(0x101216), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  panel = App_LvglUiCreatePanel(screen);
  App_LvglUiCreateTitle(panel);

  App_DacWavegenGetStatus(&status);

  App_LvglUiCreateModeRow(panel, status.config.mode, 84);
  App_LvglUiCreateSliderRow(panel,
                            "Vpp",
                            (int32_t)APP_DAC_WAVE_IQ_VPP_MIN_MV,
                            (int32_t)APP_DAC_WAVE_IQ_VPP_MAX_MV,
                            (int32_t)status.config.vpp_mv,
                            142,
                            NULL,
                            &g_vpp_slider,
                            &g_vpp_value_label);
  App_LvglUiCreateSliderRow(panel,
                            "Rate",
                            (int32_t)APP_DAC_WAVE_MOD_FREQ_MIN_HZ,
                            (int32_t)APP_DAC_WAVE_MOD_FREQ_MAX_HZ,
                            (int32_t)status.config.mod_freq_hz,
                            200,
                            &g_rate_name_label,
                            &g_rate_slider,
                            &g_rate_value_label);
  App_LvglUiCreateSliderRow(panel,
                            "Param",
                            (int32_t)APP_DAC_WAVE_AM_DEPTH_MIN_PERCENT,
                            (int32_t)APP_DAC_WAVE_AM_DEPTH_MAX_PERCENT,
                            (int32_t)status.config.am_depth_percent,
                            258,
                            &g_param_name_label,
                            &g_param_slider,
                            &g_param_value_label);
  App_LvglUiCreateButtons(panel);

  App_LvglUiApplyModeToControls(status.config.mode);
  App_LvglUiRefreshLabels();
  App_LvglUiRefreshState();
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

  lv_label_set_text(subtitle, "DAC1_OUT1 = I / DAC1_OUT2 = Q / offset 400 mV / Vpp <= 300 mV");
  lv_obj_set_style_text_color(subtitle, lv_color_hex(0xBFC7D5), 0);
  lv_obj_align_to(subtitle, title, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
}

static void App_LvglUiCreateModeRow(lv_obj_t *parent, AppDacWavegenMode mode, int32_t y)
{
  lv_obj_t *name_label = lv_label_create(parent);

  lv_label_set_text(name_label, "Mode");
  lv_obj_set_width(name_label, APP_UI_LABEL_W);
  lv_obj_set_style_text_color(name_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(name_label, LV_ALIGN_TOP_LEFT, 36, y + 8);

  g_mode_dropdown = lv_dropdown_create(parent);
  lv_obj_set_width(g_mode_dropdown, 190);
  lv_dropdown_set_options(g_mode_dropdown, "AM\nFM\n2ASK\n2PSK\n2FSK");
  lv_dropdown_set_selected(g_mode_dropdown, (uint16_t)mode);
  lv_obj_align(g_mode_dropdown, LV_ALIGN_TOP_LEFT, 220, y);
  lv_obj_add_event_cb(g_mode_dropdown, App_LvglUiOnModeChanged, LV_EVENT_VALUE_CHANGED, NULL);
}

static void App_LvglUiCreateSliderRow(lv_obj_t *parent,
                                      const char *name,
                                      int32_t min,
                                      int32_t max,
                                      int32_t value,
                                      int32_t y,
                                      lv_obj_t **name_label,
                                      lv_obj_t **slider,
                                      lv_obj_t **value_label)
{
  lv_obj_t *local_name_label = lv_label_create(parent);

  lv_label_set_text(local_name_label, name);
  lv_obj_set_width(local_name_label, APP_UI_LABEL_W);
  lv_obj_set_style_text_color(local_name_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(local_name_label, LV_ALIGN_TOP_LEFT, 36, y);

  *value_label = lv_label_create(parent);
  lv_obj_set_width(*value_label, APP_UI_VALUE_W);
  lv_obj_set_style_text_color(*value_label, lv_color_hex(0xDDE6F3), 0);
  lv_obj_align(*value_label, LV_ALIGN_TOP_LEFT, 132, y);

  *slider = lv_slider_create(parent);
  lv_obj_set_width(*slider, APP_UI_CONTROL_W);
  lv_slider_set_range(*slider, min, max);
  lv_slider_set_value(*slider, value, LV_ANIM_OFF);
  lv_obj_align(*slider, LV_ALIGN_TOP_LEFT, 260, y + 2);
  lv_obj_add_event_cb(*slider, App_LvglUiOnSliderChanged, LV_EVENT_VALUE_CHANGED, NULL);

  if (name_label != NULL)
  {
    *name_label = local_name_label;
  }
}

static void App_LvglUiCreateButtons(lv_obj_t *parent)
{
  lv_obj_t *apply_button = lv_button_create(parent);
  lv_obj_t *apply_label = lv_label_create(apply_button);
  lv_obj_t *run_button = lv_button_create(parent);

  lv_obj_set_size(apply_button, 132, 44);
  lv_obj_align(apply_button, LV_ALIGN_BOTTOM_LEFT, 360, -24);
  lv_obj_add_event_cb(apply_button, App_LvglUiOnApplyClicked, LV_EVENT_CLICKED, NULL);
  lv_label_set_text(apply_label, "Apply");
  lv_obj_center(apply_label);

  lv_obj_set_size(run_button, 132, 44);
  lv_obj_align(run_button, LV_ALIGN_BOTTOM_LEFT, 510, -24);
  lv_obj_add_event_cb(run_button, App_LvglUiOnRunClicked, LV_EVENT_CLICKED, NULL);
  g_run_button_label = lv_label_create(run_button);
  lv_label_set_text(g_run_button_label, "Stop");
  lv_obj_center(g_run_button_label);

  g_state_label = lv_label_create(parent);
  lv_obj_set_width(g_state_label, 300);
  lv_obj_set_style_text_color(g_state_label, lv_color_hex(0xBFC7D5), 0);
  lv_obj_align(g_state_label, LV_ALIGN_BOTTOM_LEFT, 36, -34);
}

static void App_LvglUiApplyModeToControls(AppDacWavegenMode mode)
{
  switch (mode)
  {
    case APP_DAC_WAVE_MODE_AM:
      lv_label_set_text(g_rate_name_label, "Mod freq");
      lv_slider_set_range(g_rate_slider,
                          (int32_t)APP_DAC_WAVE_MOD_FREQ_MIN_HZ,
                          (int32_t)APP_DAC_WAVE_MOD_FREQ_MAX_HZ);
      lv_slider_set_value(g_rate_slider, 10000, LV_ANIM_OFF);
      lv_label_set_text(g_param_name_label, "Depth");
      lv_slider_set_range(g_param_slider,
                          (int32_t)APP_DAC_WAVE_AM_DEPTH_MIN_PERCENT,
                          (int32_t)APP_DAC_WAVE_AM_DEPTH_MAX_PERCENT);
      lv_slider_set_value(g_param_slider, 50, LV_ANIM_OFF);
      lv_obj_clear_state(g_param_slider, LV_STATE_DISABLED);
      break;

    case APP_DAC_WAVE_MODE_FM:
      lv_label_set_text(g_rate_name_label, "Mod freq");
      lv_slider_set_range(g_rate_slider,
                          (int32_t)APP_DAC_WAVE_MOD_FREQ_MIN_HZ,
                          (int32_t)APP_DAC_WAVE_MOD_FREQ_MAX_HZ);
      lv_slider_set_value(g_rate_slider, 10000, LV_ANIM_OFF);
      lv_label_set_text(g_param_name_label, "Deviation");
      lv_slider_set_range(g_param_slider,
                          (int32_t)APP_DAC_WAVE_FM_DEVIATION_MIN_HZ,
                          (int32_t)APP_DAC_WAVE_FM_DEVIATION_MAX_HZ);
      lv_slider_set_value(g_param_slider, 75000, LV_ANIM_OFF);
      lv_obj_clear_state(g_param_slider, LV_STATE_DISABLED);
      break;

    case APP_DAC_WAVE_MODE_2ASK:
      lv_label_set_text(g_rate_name_label, "Bit rate");
      lv_slider_set_range(g_rate_slider,
                          (int32_t)APP_DAC_WAVE_SYMBOL_RATE_MIN_BPS,
                          (int32_t)APP_DAC_WAVE_SYMBOL_RATE_MAX_BPS);
      lv_slider_set_value(g_rate_slider, 10000, LV_ANIM_OFF);
      lv_label_set_text(g_param_name_label, "Low amp");
      lv_slider_set_range(g_param_slider, 0, 1);
      lv_slider_set_value(g_param_slider, 0, LV_ANIM_OFF);
      lv_obj_add_state(g_param_slider, LV_STATE_DISABLED);
      break;

    case APP_DAC_WAVE_MODE_2PSK:
      lv_label_set_text(g_rate_name_label, "Bit rate");
      lv_slider_set_range(g_rate_slider,
                          (int32_t)APP_DAC_WAVE_SYMBOL_RATE_MIN_BPS,
                          (int32_t)APP_DAC_WAVE_SYMBOL_RATE_MAX_BPS);
      lv_slider_set_value(g_rate_slider, 10000, LV_ANIM_OFF);
      lv_label_set_text(g_param_name_label, "Phase");
      lv_slider_set_range(g_param_slider, 0, 180);
      lv_slider_set_value(g_param_slider, 180, LV_ANIM_OFF);
      lv_obj_add_state(g_param_slider, LV_STATE_DISABLED);
      break;

    case APP_DAC_WAVE_MODE_2FSK:
    default:
      lv_label_set_text(g_rate_name_label, "Bit rate");
      lv_slider_set_range(g_rate_slider,
                          (int32_t)APP_DAC_WAVE_SYMBOL_RATE_MIN_BPS,
                          (int32_t)APP_DAC_WAVE_SYMBOL_RATE_MAX_BPS);
      lv_slider_set_value(g_rate_slider, 10000, LV_ANIM_OFF);
      lv_label_set_text(g_param_name_label, "Shift");
      lv_slider_set_range(g_param_slider,
                          (int32_t)APP_DAC_WAVE_FSK_SHIFT_MIN_HZ,
                          (int32_t)APP_DAC_WAVE_FSK_SHIFT_MAX_HZ);
      lv_slider_set_value(g_param_slider, 20000, LV_ANIM_OFF);
      lv_obj_clear_state(g_param_slider, LV_STATE_DISABLED);
      break;
  }
}

static void App_LvglUiRefreshLabels(void)
{
  AppDacWavegenMode mode = (AppDacWavegenMode)lv_dropdown_get_selected(g_mode_dropdown);
  int32_t vpp = lv_slider_get_value(g_vpp_slider);
  int32_t rate = lv_slider_get_value(g_rate_slider);
  int32_t param = lv_slider_get_value(g_param_slider);

  lv_label_set_text_fmt(g_vpp_value_label, "%ld mV", (long)vpp);

  if ((mode == APP_DAC_WAVE_MODE_AM) || (mode == APP_DAC_WAVE_MODE_FM))
  {
    lv_label_set_text_fmt(g_rate_value_label, "%ld Hz", (long)rate);
  }
  else
  {
    lv_label_set_text_fmt(g_rate_value_label, "%ld bps", (long)rate);
  }

  switch (mode)
  {
    case APP_DAC_WAVE_MODE_AM:
      lv_label_set_text_fmt(g_param_value_label, "%ld %%", (long)param);
      break;
    case APP_DAC_WAVE_MODE_FM:
      lv_label_set_text_fmt(g_param_value_label, "%ld Hz", (long)param);
      break;
    case APP_DAC_WAVE_MODE_2ASK:
      lv_label_set_text(g_param_value_label, "0");
      break;
    case APP_DAC_WAVE_MODE_2PSK:
      lv_label_set_text(g_param_value_label, "180 deg");
      break;
    case APP_DAC_WAVE_MODE_2FSK:
    default:
      lv_label_set_text_fmt(g_param_value_label, "%ld Hz", (long)param);
      break;
  }
}

static void App_LvglUiRefreshState(void)
{
  AppDacWavegenStatus status;

  App_DacWavegenGetStatus(&status);

  if (status.running != 0U)
  {
    lv_label_set_text_fmt(g_state_label, "%s running", App_LvglUiModeText(status.config.mode));
    lv_label_set_text(g_run_button_label, "Stop");
  }
  else
  {
    lv_label_set_text(g_state_label, "Stopped");
    lv_label_set_text(g_run_button_label, "Start");
  }
}

static void App_LvglUiReadConfig(AppDacWavegenConfig *config)
{
  App_DacWavegenSetDefaultConfig(config);

  config->mode = (AppDacWavegenMode)lv_dropdown_get_selected(g_mode_dropdown);
  config->vpp_mv = (uint16_t)lv_slider_get_value(g_vpp_slider);

  if ((config->mode == APP_DAC_WAVE_MODE_AM) || (config->mode == APP_DAC_WAVE_MODE_FM))
  {
    config->mod_freq_hz = (uint32_t)lv_slider_get_value(g_rate_slider);
  }
  else
  {
    config->symbol_rate_bps = (uint32_t)lv_slider_get_value(g_rate_slider);
  }

  switch (config->mode)
  {
    case APP_DAC_WAVE_MODE_AM:
      config->am_depth_percent = (uint16_t)lv_slider_get_value(g_param_slider);
      break;
    case APP_DAC_WAVE_MODE_FM:
      config->fm_deviation_hz = (uint32_t)lv_slider_get_value(g_param_slider);
      break;
    case APP_DAC_WAVE_MODE_2FSK:
      config->fsk_shift_hz = (uint32_t)lv_slider_get_value(g_param_slider);
      break;
    case APP_DAC_WAVE_MODE_2ASK:
    case APP_DAC_WAVE_MODE_2PSK:
    default:
      break;
  }
}

static void App_LvglUiApplyConfig(void)
{
  AppDacWavegenConfig config;

  App_LvglUiReadConfig(&config);
  (void)App_DacWavegenSetConfig(&config);
  App_LvglUiRefreshLabels();
  App_LvglUiRefreshState();
}

static void App_LvglUiOnModeChanged(lv_event_t *e)
{
  AppDacWavegenMode mode;

  (void)e;

  mode = (AppDacWavegenMode)lv_dropdown_get_selected(g_mode_dropdown);
  App_LvglUiApplyModeToControls(mode);
  App_LvglUiRefreshLabels();
}

static void App_LvglUiOnSliderChanged(lv_event_t *e)
{
  (void)e;
  App_LvglUiRefreshLabels();
}

static void App_LvglUiOnApplyClicked(lv_event_t *e)
{
  (void)e;
  App_LvglUiApplyConfig();
}

static void App_LvglUiOnRunClicked(lv_event_t *e)
{
  AppDacWavegenStatus status;

  (void)e;

  App_DacWavegenGetStatus(&status);
  if (status.running != 0U)
  {
    App_DacWavegenStop();
  }
  else
  {
    App_LvglUiApplyConfig();
    (void)App_DacWavegenStart();
  }

  App_LvglUiRefreshState();
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
    default:
      return "2FSK";
  }
}
