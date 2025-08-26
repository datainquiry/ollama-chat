#ifndef UI_CALLBACKS_H
#define UI_CALLBACKS_H

#include "app_data.h"

void ui_schedule_update_response_label(AppData *app_data, char *text);
void ui_schedule_finalize_generation(AppData *app_data);
void ui_schedule_update_models_dropdown(AppData *app_data);
void ui_schedule_reset_send_button(AppData *app_data);
void ui_schedule_scroll_to_bottom(AppData *app_data);

#endif // UI_CALLBACKS_H
