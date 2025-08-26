#ifndef APP_DATA_H
#define APP_DATA_H

#include <gtk/gtk.h>
#include <json-c/json.h>

#define MAX_MESSAGE_LEN 8192
#define MAX_MODELS 50

typedef struct {
    char *data;
    size_t size;
} HttpResponse;

typedef struct {
    char content[MAX_MESSAGE_LEN];
    gboolean is_user;
} ChatMessage;

typedef struct AppData {
    char *base_url;
    GtkApplication *app;
    GtkWindow *window;
    GtkDropDown *model_dropdown;
    GtkLabel *status_label;
    GtkBox *chat_box;
    GtkTextView *text_view;
    GtkTextBuffer *text_buffer;
    GtkButton *send_btn;
    GtkSpinner *spinner;
    GtkScrolledWindow *chat_scroll;
    
    char **models;
    int model_count;
    char *current_model;
    gboolean is_generating;
    gboolean request_cancelled;
    
    json_object *messages_array;
    GtkWidget *current_response_widget;
    GtkLabel *current_response_label;

    // Chat History
    GtkListBox *history_list_box;
    GListStore *history_store;
    GtkRevealer *history_revealer;
    char *current_chat_id;

    // Configuration
    int window_width;
    int window_height;
    int pane_position;
    gboolean history_panel_visible;
    int ollama_context_size;
    char *theme;
    gboolean web_search_enabled;

    // Ollama Model Parameters
    double temperature;
    double top_p;
    int top_k;
    int seed;
    char *system_prompt;
} AppData;

#endif // APP_DATA_H
