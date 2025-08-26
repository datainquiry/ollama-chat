#include "ollama_api.h"
#include "ui.h"

#include <curl/curl.h>
#include <json-c/json.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <ctype.h>

// HTTP response callback
static size_t write_callback(void *contents, size_t size, size_t nmemb, HttpResponse *response) {
    size_t real_size = size * nmemb;
    char *ptr = realloc(response->data, response->size + real_size + 1);
    
    if (!ptr) {
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }
    
    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, real_size);
    response->size += real_size;
    response->data[response->size] = 0;
    
    return real_size;
}

// Streaming callback for chat responses
#define STREAM_BUFFER_SIZE 8192
typedef struct {
    AppData *app_data;
    char buffer[STREAM_BUFFER_SIZE];
    size_t buffer_pos;
} StreamData;

static size_t stream_callback(void *contents, size_t size, size_t nmemb, StreamData *stream_data) {
    size_t real_size = size * nmemb;

    if (stream_data->buffer_pos + real_size >= STREAM_BUFFER_SIZE) {
        fprintf(stderr, "Stream buffer overflow. Increase STREAM_BUFFER_SIZE.\n");
        // Clear buffer and hope for the best
        stream_data->buffer_pos = 0;
        return real_size; // Consume data to not get stuck in a loop
    }

    memcpy(&stream_data->buffer[stream_data->buffer_pos], contents, real_size);
    stream_data->buffer_pos += real_size;
    stream_data->buffer[stream_data->buffer_pos] = '\0';

    char *line_start = stream_data->buffer;
    char *line_end;

    while ((line_end = strchr(line_start, '\n')) != NULL) {
        *line_end = '\0';

        *line_end = '\0';

        gboolean has_content = FALSE;
        for (char *p = line_start; *p; p++) {
            if (!isspace((unsigned char)*p)) {
                has_content = TRUE;
                break;
            }
        }

        if (has_content) {
            json_object *json_obj = json_tokener_parse(line_start);
            if (json_obj) {
                json_object *message_obj, *content_obj, *done_obj;

                if (json_object_object_get_ex(json_obj, "message", &message_obj)) {
                    if (json_object_object_get_ex(message_obj, "content", &content_obj)) {
                        const char *content = json_object_get_string(content_obj);

                        if (content) { // No need to check strlen, send even empty strings from API
                            char *content_copy = g_strdup(content);
                            ui_schedule_update_response_label(stream_data->app_data, content_copy);
                        }
                    }
                }

                if (json_object_object_get_ex(json_obj, "done", &done_obj)) {
                    if (json_object_get_boolean(done_obj)) {
                        ui_schedule_finalize_generation(stream_data->app_data);
                    }
                }

                json_object_put(json_obj);
            }
        }

        line_start = line_end + 1;
    }

    // Move the remaining partial line to the beginning of the buffer
    if (line_start < &stream_data->buffer[stream_data->buffer_pos]) {
        size_t remaining_len = &stream_data->buffer[stream_data->buffer_pos] - line_start;
        memmove(stream_data->buffer, line_start, remaining_len);
        stream_data->buffer_pos = remaining_len;
    } else {
        stream_data->buffer_pos = 0;
    }

    return real_size;
}

static void *get_models_thread(void *arg) {
    AppData *app_data = (AppData *)arg;
    CURL *curl;
    CURLcode res;
    HttpResponse response = {0};
    
    curl = curl_easy_init();
    if (curl) {
        char url[256];
        snprintf(url, sizeof(url), "%s/api/tags", BASE_URL);
        
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        if (res == CURLE_OK && response.data) {
            json_object *json_obj = json_tokener_parse(response.data);
            if (json_obj) {
                json_object *models_array_obj;
                if (json_object_object_get_ex(json_obj, "models", &models_array_obj) && json_object_is_type(models_array_obj, json_type_array)) {
                    int array_len = json_object_array_length(models_array_obj);
                    char **new_models = malloc(array_len * sizeof(char*));
                    int new_model_count = 0;

                    if (new_models) {
                        for (int i = 0; i < array_len; i++) {
                            json_object *model_obj = json_object_array_get_idx(models_array_obj, i);
                            json_object *name_obj;
                            if (json_object_object_get_ex(model_obj, "name", &name_obj)) {
                                const char *name = json_object_get_string(name_obj);
                                new_models[i] = g_strdup(name);
                                if (new_models[i]) {
                                    new_model_count++;
                                }
                            }
                        }

                        if (new_model_count == array_len) {
                            // Free old models
                            if (app_data->models) {
                                for (int i = 0; i < app_data->model_count; i++) {
                                    g_free(app_data->models[i]);
                                }
                                free(app_data->models);
                            }
                            app_data->models = new_models;
                            app_data->model_count = new_model_count;
                        } else {
                            // Cleanup new_models if allocation failed
                            for (int i = 0; i < new_model_count; i++) {
                                g_free(new_models[i]);
                            }
                            free(new_models);
                        }
                    }
                    
                    ui_schedule_update_models_dropdown(app_data);
                }
                json_object_put(json_obj);
            }
        }
        
        if (response.data) {
            free(response.data);
        }
    }
    
    return NULL;
}

typedef struct {
    AppData *app_data;
    char *message;
} ChatThreadData;

static void *send_chat_thread(void *arg) {
    ChatThreadData *thread_data = (ChatThreadData *)arg;
    AppData *app_data = thread_data->app_data;
    char *message = thread_data->message;

    CURL *curl;
    CURLcode res;
    
    curl = curl_easy_init();
    if (curl) {
        char url[256];
        snprintf(url, sizeof(url), "%s/api/chat", BASE_URL);
        
        json_object *payload = json_object_new_object();
        json_object_object_add(payload, "model", json_object_new_string(app_data->current_model));
        json_object_object_add(payload, "messages", json_object_get(app_data->messages_array));
        json_object_object_add(payload, "stream", json_object_new_boolean(TRUE));
        
        const char *json_string = json_object_to_json_string(payload);
        
        StreamData stream_data = {.app_data = app_data, .buffer_pos = 0};
        
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_string);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stream_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream_data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        res = curl_easy_perform(curl);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        json_object_put(payload);
        
        if (res != CURLE_OK) {
            ui_schedule_reset_send_button(app_data);
        }
    }
    
    free(message);
    free(thread_data);
    return NULL;
}

void api_init(void) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void api_cleanup(void) {
    curl_global_cleanup();
}

void api_get_models(AppData *app_data) {
    pthread_t thread;
    pthread_create(&thread, NULL, get_models_thread, app_data);
    pthread_detach(thread);
}

void api_send_chat(AppData *app_data, char *message) {
    ChatThreadData *thread_data = malloc(sizeof(ChatThreadData));
    thread_data->app_data = app_data;
    thread_data->message = message; // already a copy
    pthread_t thread;
    pthread_create(&thread, NULL, send_chat_thread, thread_data);
    pthread_detach(thread);
}
