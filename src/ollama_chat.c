// ollama_chat.c - Ollama GTK Chat Application in C
#include "app_data.h"
#include "ui.h"
#include "ollama_api.h"
#include "history.h"
#include "config.h"

#include <stdlib.h>

static AppData *app_data = NULL;

static void on_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    app_data = g_malloc0(sizeof(AppData));
    app_data->app = app;
    
    // Set the application icon
    GtkIconTheme *icon_theme = gtk_icon_theme_get_for_display(gdk_display_get_default());
    gtk_icon_theme_add_search_path(icon_theme, "/usr/share/icons/hicolor/scalable/apps");
    
    config_init(app_data);
    history_init(app_data);
    
    ui_build(app, app_data);
    
    history_load_chats(app_data);

    if (g_list_model_get_n_items(G_LIST_MODEL(app_data->history_store)) == 0) {
        history_start_new_chat(app_data);
    } else {
        // Load the first chat in the list
        GtkListBoxRow *first_row = gtk_list_box_get_row_at_index(app_data->history_list_box, 0);
        if (first_row) {
            history_load_selected_chat(app_data->history_list_box, first_row, app_data);
            gtk_list_box_select_row(app_data->history_list_box, first_row);
        }
    }
    
    api_get_models(app_data);
}

int main(int argc, char *argv[]) {
    api_init();
    
    GtkApplication *app = gtk_application_new(
        "com.example.ollama-chat", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    
    if (app_data) {
        config_save(app_data);
        history_save_chat(app_data);
        if (app_data->models) {
            for (int i = 0; i < app_data->model_count; i++) {
                g_free(app_data->models[i]);
            }
            free(app_data->models);
        }
        if (app_data->current_model) {
            g_free(app_data->current_model);
        }
        if (app_data->system_prompt) {
            g_free(app_data->system_prompt);
        }
        if (app_data->messages_array) {
            json_object_put(app_data->messages_array);
        }
        if (app_data->current_chat_id) {
            g_free(app_data->current_chat_id);
        }
        if (app_data->history_store) {
            g_object_unref(app_data->history_store);
        }
        if (app_data->theme) {
            g_free(app_data->theme);
        }
        g_free(app_data);
    }
    
    g_object_unref(app);
    api_cleanup();
    
    return status;
}