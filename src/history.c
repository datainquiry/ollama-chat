#include "history.h"
#include "ui.h"
#include <glib/gstdio.h>
#include <uuid/uuid.h>
#include <string.h>

static const char *HISTORY_DIR = ".local/share/ollama-chat";

// --- Private Helper Functions ---

static char *get_history_path() {
    const char *home_dir = g_get_home_dir();
    return g_build_filename(home_dir, HISTORY_DIR, NULL);
}

static char *get_chat_filepath(const char *chat_id) {
    char *history_path = get_history_path();
    char *filepath = g_build_filename(history_path, chat_id, NULL);
    g_free(history_path);
    return filepath;
}

static char *generate_uuid() {
    uuid_t b;
    uuid_generate_random(b);
    char *uuid_str = malloc(37);
    uuid_unparse_lower(b, uuid_str);
    return uuid_str;
}

// --- Sorting Helper ---
typedef struct {
    char *filename;
    GDateTime *modified_time;
} ChatHistoryEntry;

static gint compare_chat_history_entries(gconstpointer a, gconstpointer b, gpointer user_data) {
    (void)user_data;
    const ChatHistoryEntry *entry_a = a;
    const ChatHistoryEntry *entry_b = b;
    return g_date_time_compare(entry_b->modified_time, entry_a->modified_time);
}


// --- Public Functions ---

void history_init(AppData *app_data) {
    char *history_path = get_history_path();
    g_mkdir_with_parents(history_path, 0755);
    g_free(history_path);
    
    app_data->history_store = g_list_store_new(GTK_TYPE_STRING_OBJECT);
}

void history_load_chats(AppData *app_data) {
    g_list_store_remove_all(app_data->history_store);
    char *history_path = get_history_path();
    GDir *dir = g_dir_open(history_path, 0, NULL);
    if (dir) {
        const char *filename;
        GSList *chat_entries = NULL;

        while ((filename = g_dir_read_name(dir))) {
            char *filepath = g_build_filename(history_path, filename, NULL);
            GFileInfo *file_info = g_file_query_info(g_file_new_for_path(filepath),
                                                     G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                                     G_FILE_QUERY_INFO_NONE,
                                                     NULL, NULL);
            
            if (file_info) {
                GDateTime *modified_time = g_file_info_get_modification_date_time(file_info);
                ChatHistoryEntry *entry = g_new(ChatHistoryEntry, 1);
                entry->filename = g_strdup(filename);
                entry->modified_time = modified_time;
                chat_entries = g_slist_prepend(chat_entries, entry);
                g_object_unref(file_info);
            }
            g_free(filepath);
        }
        g_dir_close(dir);

        chat_entries = g_slist_sort_with_data(chat_entries, compare_chat_history_entries, NULL);

        for (GSList *l = chat_entries; l != NULL; l = l->next) {
            ChatHistoryEntry *entry = l->data;
            GtkStringObject *str_obj = gtk_string_object_new(entry->filename);
            g_list_store_append(app_data->history_store, str_obj);
            g_free(entry->filename);
            g_date_time_unref(entry->modified_time);
            g_free(entry);
        }
        g_slist_free(chat_entries);
    }
    g_free(history_path);
}

void history_save_chat(AppData *app_data) {
    if (!app_data->current_chat_id || !app_data->messages_array) return;

    char *filepath = get_chat_filepath(app_data->current_chat_id);
    const char *json_str = json_object_to_json_string_ext(app_data->messages_array, JSON_C_TO_STRING_PRETTY);
    g_file_set_contents(filepath, json_str, -1, NULL);
    g_free(filepath);
}

void history_start_new_chat(AppData *app_data) {
    if (app_data->current_chat_id && app_data->messages_array && json_object_array_length(app_data->messages_array) > 0) {
        history_save_chat(app_data);
    }

    if (app_data->current_chat_id) {
        g_free(app_data->current_chat_id);
    }
    app_data->current_chat_id = generate_uuid();
    
    if (app_data->messages_array) {
        json_object_put(app_data->messages_array);
    }
    app_data->messages_array = json_object_new_array();
    
    ui_clear_chat_view(app_data);
    
    GtkStringObject *str_obj = gtk_string_object_new(app_data->current_chat_id);
    g_list_store_insert(app_data->history_store, 0, str_obj);
    history_save_chat(app_data);
}

void history_load_selected_chat(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    (void)box;
    AppData *app_data = (AppData *)user_data;
    if (!row) return;

    const char *chat_id = gtk_label_get_text(GTK_LABEL(gtk_list_box_row_get_child(row)));
    
    if (app_data->current_chat_id && strcmp(app_data->current_chat_id, chat_id) == 0) {
        return; // Already loaded
    }

    history_save_chat(app_data);

    char *filepath = get_chat_filepath(chat_id);
    json_object *messages = json_object_from_file(filepath);
    g_free(filepath);

    if (messages) {
        if (app_data->messages_array) {
            json_object_put(app_data->messages_array);
        }
        app_data->messages_array = messages;
        
        if (app_data->current_chat_id) {
            g_free(app_data->current_chat_id);
        }
        app_data->current_chat_id = g_strdup(chat_id);
        
        ui_clear_chat_view(app_data);
        ui_redisplay_chat_history(app_data);
    }
}

void history_delete_chat(AppData *app_data, const char *chat_id) {
    char *filepath = get_chat_filepath(chat_id);
    g_remove(filepath);
    g_free(filepath);
    
    guint n_items = g_list_model_get_n_items(G_LIST_MODEL(app_data->history_store));
    for (guint i = 0; i < n_items; i++) {
        GtkStringObject *str_obj = g_list_model_get_item(G_LIST_MODEL(app_data->history_store), i);
        const char* id_in_store = gtk_string_object_get_string(str_obj);
        if (strcmp(id_in_store, chat_id) == 0) {
            g_list_store_remove(app_data->history_store, i);
            break;
        }
    }

    if (app_data->current_chat_id && strcmp(app_data->current_chat_id, chat_id) == 0) {
        history_start_new_chat(app_data);
    }
}

void history_rename_chat(AppData *app_data, const char *chat_id, const char *new_title) {
    char *old_filepath = get_chat_filepath(chat_id);
    char *new_filepath = get_chat_filepath(new_title);

    if (g_rename(old_filepath, new_filepath) == 0) {
        guint n_items = g_list_model_get_n_items(G_LIST_MODEL(app_data->history_store));
        for (guint i = 0; i < n_items; i++) {
            GtkStringObject *str_obj = g_list_model_get_item(G_LIST_MODEL(app_data->history_store), i);
            const char* id_in_store = gtk_string_object_get_string(str_obj);
            if (strcmp(id_in_store, chat_id) == 0) {
                g_list_store_remove(app_data->history_store, i);
                GtkStringObject *new_str_obj = gtk_string_object_new(new_title);
                g_list_store_insert(app_data->history_store, i, new_str_obj);
                g_object_unref(new_str_obj);
                break;
            }
        }
        if (app_data->current_chat_id && strcmp(app_data->current_chat_id, chat_id) == 0) {
            g_free(app_data->current_chat_id);
            app_data->current_chat_id = g_strdup(new_title);
        }
    }
    g_free(old_filepath);
    g_free(new_filepath);
}
