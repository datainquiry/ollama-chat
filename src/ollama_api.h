#ifndef OLLAMA_API_H
#define OLLAMA_API_H

#include "app_data.h"

void api_init(void);
void api_cleanup(void);
void api_get_models(AppData *app_data);
void api_send_chat(AppData *app_data, char *message);

#endif // OLLAMA_API_H
