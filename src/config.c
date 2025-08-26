#include "config.h"
#include <glib/gstdio.h>
#include <json-c/json.h>

static const char *CONFIG_DIR = ".config/ollama-chat";
static const char *CONFIG_FILE = "config.json";

// --- Private Helper Functions ---

static char *get_config_filepath() {
    const char *home_dir = g_get_home_dir();
    return g_build_filename(home_dir, CONFIG_DIR, CONFIG_FILE, NULL);
}

static void ensure_config_dir_exists() {
    const char *home_dir = g_get_home_dir();
    char *config_path = g_build_filename(home_dir, CONFIG_DIR, NULL);
    g_mkdir_with_parents(config_path, 0755);
    g_free(config_path);
}

// --- Public Functions ---

void config_init(AppData *app_data) {
    // Set default values
    app_data->window_width = 1024;
    app_data->window_height = 768;
    app_data->pane_position = 256;
    app_data->ollama_context_size = 2048;
    app_data->theme = g_strdup("light");
    app_data->web_search_enabled = TRUE;

    // Ollama Model Parameters
    app_data->temperature = 0.8;
    app_data->top_p = 0.9;
    app_data->top_k = 40;
    app_data->seed = 0;
    app_data->system_prompt = g_strdup("You are a helpful assistant.");

    ensure_config_dir_exists();
    config_load(app_data); // Load config or create a default one
}

void config_load(AppData *app_data) {
    char *filepath = get_config_filepath();
    json_object *root = json_object_from_file(filepath);

    if (root) {
        json_object *val;
        if (json_object_object_get_ex(root, "selected_model", &val)) {
            if (app_data->current_model) g_free(app_data->current_model);
            app_data->current_model = g_strdup(json_object_get_string(val));
        }
        if (json_object_object_get_ex(root, "window_width", &val)) {
            app_data->window_width = json_object_get_int(val);
        }
        if (json_object_object_get_ex(root, "window_height", &val)) {
            app_data->window_height = json_object_get_int(val);
        }
        if (json_object_object_get_ex(root, "pane_position", &val)) {
            app_data->pane_position = json_object_get_int(val);
        }
        if (json_object_object_get_ex(root, "ollama_context_size", &val)) {
            app_data->ollama_context_size = json_object_get_int(val);
        }
        if (json_object_object_get_ex(root, "theme", &val)) {
            if (app_data->theme) g_free(app_data->theme);
            app_data->theme = g_strdup(json_object_get_string(val));
        }
        if (json_object_object_get_ex(root, "web_search_enabled", &val)) {
            app_data->web_search_enabled = json_object_get_boolean(val);
        }
        if (json_object_object_get_ex(root, "temperature", &val)) {
            app_data->temperature = json_object_get_double(val);
        }
        if (json_object_object_get_ex(root, "top_p", &val)) {
            app_data->top_p = json_object_get_double(val);
        }
        if (json_object_object_get_ex(root, "top_k", &val)) {
            app_data->top_k = json_object_get_int(val);
        }
        if (json_object_object_get_ex(root, "seed", &val)) {
            app_data->seed = json_object_get_int(val);
        }
        if (json_object_object_get_ex(root, "system_prompt", &val)) {
            if (app_data->system_prompt) g_free(app_data->system_prompt);
            app_data->system_prompt = g_strdup(json_object_get_string(val));
        }
        json_object_put(root);
    } else {
        // If file doesn't exist, create it with defaults
        config_save(app_data);
    }
    g_free(filepath);
}

void config_save(AppData *app_data) {
    json_object *root = json_object_new_object();

    if (app_data->current_model) {
        json_object_object_add(root, "selected_model", json_object_new_string(app_data->current_model));
    }
    json_object_object_add(root, "window_width", json_object_new_int(app_data->window_width));
    json_object_object_add(root, "window_height", json_object_new_int(app_data->window_height));
    json_object_object_add(root, "pane_position", json_object_new_int(app_data->pane_position));
    json_object_object_add(root, "ollama_context_size", json_object_new_int(app_data->ollama_context_size));
    if (app_data->theme) {
        json_object_object_add(root, "theme", json_object_new_string(app_data->theme));
    }
    json_object_object_add(root, "web_search_enabled", json_object_new_boolean(app_data->web_search_enabled));

    // Ollama Model Parameters
    json_object_object_add(root, "temperature", json_object_new_double(app_data->temperature));
    json_object_object_add(root, "top_p", json_object_new_double(app_data->top_p));
    json_object_object_add(root, "top_k", json_object_new_int(app_data->top_k));
    json_object_object_add(root, "seed", json_object_new_int(app_data->seed));
    if (app_data->system_prompt) {
        json_object_object_add(root, "system_prompt", json_object_new_string(app_data->system_prompt));
    }

    char *filepath = get_config_filepath();
    const char *json_str = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    g_file_set_contents(filepath, json_str, -1, NULL);

    g_free(filepath);
    json_object_put(root);
}
