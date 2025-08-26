#ifndef UI_H
#define UI_H

#include "app_data.h"

void ui_build(GtkApplication *app, AppData *app_data);

// Thread-safe UI update functions
void ui_schedule_update_response_label(AppData *app_data, char *text);
void ui_schedule_finalize_generation(AppData *app_data);
void ui_schedule_update_models_dropdown(AppData *app_data);
void ui_schedule_reset_send_button(AppData *app_data);
void ui_schedule_scroll_to_bottom(AppData *app_data);

// Chat view manipulation
void ui_clear_chat_view(AppData *app_data);
void ui_redisplay_chat_history(AppData *app_data);


#endif // UI_H
