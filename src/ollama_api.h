#ifndef OLLAMA_API_H
#define OLLAMA_API_H

#include <stddef.h>

// Forward declaration
typedef struct AppData AppData;

typedef struct {
    char *data;
    size_t size;
} HttpResponse;

void api_init(void);
void api_cleanup(void);
void api_get_models(AppData *app_data);
void api_send_chat(AppData *app_data, char *message);
void api_check_connection(AppData *app_data);

#endif // OLLAMA_API_H
