#ifndef APP_LVGL_UI_H
#define APP_LVGL_UI_H

typedef enum
{
  APP_UI_FIELD_LO_FREQ = 0,
  APP_UI_FIELD_VPP,
  APP_UI_FIELD_RATE,
  APP_UI_FIELD_PARAM,
  APP_UI_FIELD_COUNT
} AppUiEditField;

#define APP_UI_DEFAULT_FIELD        APP_UI_FIELD_LO_FREQ
#define APP_UI_DEFAULT_DIGIT_INDEX  0U

#ifdef __cplusplus
extern "C" {
#endif

void App_LvglUiInit(void);
void App_LvglUiPoll(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_LVGL_UI_H */
