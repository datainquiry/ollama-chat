// ollama_chat.c - Ollama GTK Chat Application in C
#include "app_data.h"
#include "ui.h"
#include "ollama_api.h"

#include <stdlib.h>

static AppData *app_data = NULL;

static void on_activate(GtkApplication *app, gpointer user_data) {
    app_data = g_malloc0(sizeof(AppData));
    app_data->app = app;
    app_data->messages_array = json_object_new_array();
    
    ui_build(app, app_data);
    
    // Load models
    api_get_models(app_data);
}

int main(int argc, char *argv[]) {
    api_init();
    
    GtkApplication *app = gtk_application_new(
        "com.example.ollama-chat", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    
    if (app_data) {
        if (app_data->models) {
            for (int i = 0; i < app_data->model_count; i++) {
                free(app_data->models[i]);
            }
            free(app_data->models);
        }
        if (app_data->current_model) {
            free(app_data->current_model);
        }
        if (app_data->messages_array) {
            json_object_put(app_data->messages_array);
        }
        g_free(app_data);
    }
    
    g_object_unref(app);
    api_cleanup();
    
    return status;
}