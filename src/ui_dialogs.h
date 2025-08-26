#ifndef UI_DIALOGS_H
#define UI_DIALOGS_H

#include "app_data.h"

void show_preferences_dialog(AppData *app_data);
void show_rename_dialog(AppData *app_data, const char *old_name);
void show_about_dialog(AppData *app_data);

#endif // UI_DIALOGS_H
