#ifndef HISTORY_H
#define HISTORY_H

#include <json-c/json.h>
#include "app_data.h"

void history_init(AppData *app_data);
void history_load_chats(AppData *app_data);
void history_save_chat(AppData *app_data);
void history_start_new_chat(AppData *app_data);
void history_load_selected_chat(GtkListBox *box, GtkListBoxRow *row, gpointer user_data);
void history_delete_chat(AppData *app_data, const char *chat_id);
void history_rename_chat(AppData *app_data, const char *chat_id, const char *new_title);

#endif // HISTORY_H
