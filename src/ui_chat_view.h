#ifndef UI_CHAT_VIEW_H
#define UI_CHAT_VIEW_H

#include "app_data.h"

GtkWidget *create_chat_view(AppData *app_data);
void ui_clear_chat_view(AppData *app_data);
void ui_redisplay_chat_history(AppData *app_data);
GtkWidget *add_message_to_chat(AppData *app_data, const ChatMessage *message);
void rerender_message_widget(GtkWidget *widget, const char *new_content);

#endif // UI_CHAT_VIEW_H
