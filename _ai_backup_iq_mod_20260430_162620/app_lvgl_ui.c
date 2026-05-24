#include "app_lvgl_ui.h"

#include "app_dac_wavegen.h"

#include "lvgl.h"

#define APP_UI_PANEL_W 620
#define APP_UI_PANEL_H 360
#define APP_UI_LABEL_W 96
#define APP_UI_VALUE_W 120
#define APP_UI_SLIDER_W 360

static lv_obj_t *g_freq_slider = NULL;
static lv_obj_t *g_low_slider = NULL;
static lv_obj_t *g_high_slider = NULL;
static lv_obj_t *g_freq_value_label = NULL;
static lv_obj_t *g_low_value_label = NULL;
static lv_obj_t *g_high_value_label = NULL;
static lv_obj_t *g_state_label = NULL;
static lv_obj_t *g_run_button_label = NULL;

static lv_obj_t *App_LvglUiCreatePanel(lv_obj_t *parent);
static void App_LvglUiCreateTitle(lv_obj_t *parent);
static void App_LvglUiCreateSliderRow(lv_obj_t *parent,
                                      const char *name,
                                      int32_t min,
                                      int32_t max,
                                      int32_t value,
                                      int32_t y,
                                      lv_obj_t **slider,
                                      lv_obj_t **value_label);
static void App_LvglUiCreateButtons(lv_obj_t *parent);
static void App_LvglUiRefreshLabels(void);
static void App_LvglUiRefreshState(void);
static void App_LvglUiReadConfig(AppDacWavegenConfig *config);
static void App_LvglUiApplyConfig(void);
static void App_LvglUiOnSliderChanged(lv_event_t *e);
static void App_LvglUiOnApplyClicked(lv_event_t *e);
static void App_LvglUiOnRunClicked(lv_event_t *e);

void App_LvglUiInit(void)
{
  lv_obj_t *screen = lv_screen_active();
  lv_obj_t *panel = NULL;
  AppDacWavegenStatus status;

  lv_obj_set_style_bg_color(screen, lv_color_hex(0x111318), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  panel = App_LvglUiCreatePanel(screen);
  App_LvglUiCreateTitle(panel);

  App_DacWavegenGetStatus(&status);

  App_LvglUiCreateSliderRow(panel,
                            "Freq",
                            (int32_t)APP_DAC_WAVE_FREQ_MIN_HZ,
                            (int32_t)APP_DAC_WAVE_FREQ_MAX_HZ,
                            (int32_t)status.config.freq_hz,
                            88,
                            &g_freq_slider,
                            &g_freq_value_label);
  App_LvglUiCreateSliderRow(panel,
                            "Low",
                            (int32_t)APP_DAC_WAVE_MV_MIN,
                            (int32_t)APP_DAC_WAVE_MV_MAX,
                            (int32_t)status.config.low_mv,
                            148,
                            &g_low_slider,
                            &g_low_value_label);
  App_LvglUiCreateSliderRow(panel,
                            "High",
                            (int32_t)APP_DAC_WAVE_MV_MIN,
                            (int32_t)APP_DAC_WAVE_MV_MAX,
                            (int32_t)status.config.high_mv,
                            208,
                            &g_high_slider,
                            &g_high_value_label);
  App_LvglUiCreateButtons(panel);

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

  lv_label_set_text(title, "DAC Sine Output");
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 22);

  lv_label_set_text(subtitle, "2 MHz sample rate / dual channel");
  lv_obj_set_style_text_color(subtitle, lv_color_hex(0xBFC7D5), 0);
  lv_obj_align_to(subtitle, title, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
}

static void App_LvglUiCreateSliderRow(lv_obj_t *parent,
                                      const char *name,
                                      int32_t min,
                                      int32_t max,
                                      int32_t value,
                                      int32_t y,
                                      lv_obj_t **slider,
                                      lv_obj_t **value_label)
{
  lv_obj_t *name_label = lv_label_create(parent);

  lv_label_set_text(name_label, name);
  lv_obj_set_width(name_label, APP_UI_LABEL_W);
  lv_obj_set_style_text_color(name_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(name_label, LV_ALIGN_TOP_LEFT, 36, y);

  *value_label = lv_label_create(parent);
  lv_obj_set_width(*value_label, APP_UI_VALUE_W);
  lv_obj_set_style_text_color(*value_label, lv_color_hex(0xDDE6F3), 0);
  lv_obj_align(*value_label, LV_ALIGN_TOP_LEFT, 128, y);

  *slider = lv_slider_create(parent);
  lv_obj_set_width(*slider, APP_UI_SLIDER_W);
  lv_slider_set_range(*slider, min, max);
  lv_slider_set_value(*slider, value, LV_ANIM_OFF);
  lv_obj_align(*slider, LV_ALIGN_TOP_LEFT, 220, y + 2);
  lv_obj_add_event_cb(*slider, App_LvglUiOnSliderChanged, LV_EVENT_VALUE_CHANGED, NULL);
}

static void App_LvglUiCreateButtons(lv_obj_t *parent)
{
  lv_obj_t *apply_button = lv_button_create(parent);
  lv_obj_t *apply_label = lv_label_create(apply_button);
  lv_obj_t *run_button = lv_button_create(parent);

  lv_obj_set_size(apply_button, 132, 44);
  lv_obj_align(apply_button, LV_ALIGN_BOTTOM_LEFT, 210, -28);
  lv_obj_add_event_cb(apply_button, App_LvglUiOnApplyClicked, LV_EVENT_CLICKED, NULL);
  lv_label_set_text(apply_label, "Apply");
  lv_obj_center(apply_label);

  lv_obj_set_size(run_button, 132, 44);
  lv_obj_align(run_button, LV_ALIGN_BOTTOM_LEFT, 360, -28);
  lv_obj_add_event_cb(run_button, App_LvglUiOnRunClicked, LV_EVENT_CLICKED, NULL);
  g_run_button_label = lv_label_create(run_button);
  lv_label_set_text(g_run_button_label, "Stop");
  lv_obj_center(g_run_button_label);

  g_state_label = lv_label_create(parent);
  lv_obj_set_width(g_state_label, 170);
  lv_obj_set_style_text_color(g_state_label, lv_color_hex(0xBFC7D5), 0);
  lv_obj_align(g_state_label, LV_ALIGN_BOTTOM_LEFT, 36, -38);
}

static void App_LvglUiRefreshLabels(void)
{
  int32_t freq = lv_slider_get_value(g_freq_slider);
  int32_t low = lv_slider_get_value(g_low_slider);
  int32_t high = lv_slider_get_value(g_high_slider);

  lv_label_set_text_fmt(g_freq_value_label, "%ld Hz", (long)freq);
  lv_label_set_text_fmt(g_low_value_label, "%ld mV", (long)low);
  lv_label_set_text_fmt(g_high_value_label, "%ld mV", (long)high);
}

static void App_LvglUiRefreshState(void)
{
  AppDacWavegenStatus status;

  App_DacWavegenGetStatus(&status);

  if (status.running != 0U)
  {
    lv_label_set_text(g_state_label, "Running");
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
  uint16_t swap;

  config->freq_hz = (uint32_t)lv_slider_get_value(g_freq_slider);
  config->low_mv = (uint16_t)lv_slider_get_value(g_low_slider);
  config->high_mv = (uint16_t)lv_slider_get_value(g_high_slider);

  if (config->low_mv > config->high_mv)
  {
    swap = config->low_mv;
    config->low_mv = config->high_mv;
    config->high_mv = swap;
    lv_slider_set_value(g_low_slider, config->low_mv, LV_ANIM_OFF);
    lv_slider_set_value(g_high_slider, config->high_mv, LV_ANIM_OFF);
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
