#ifndef CONFIG_H
#define CONFIG_H

#include "app_data.h"

void config_init(AppData *app_data);
void config_load(AppData *app_data);
void config_save(AppData *app_data);

#endif // CONFIG_H
