#include "ui.h"
#include "ollama_api.h"
#include <pthread.h>

// --- UI update callbacks for g_idle_add ---

typedef struct {
    AppData *app_data;
    char *text;
} UpdateResponseData;

static gboolean update_response_label_cb(gpointer data) {
    UpdateResponseData *update_data = (UpdateResponseData *)data;
    AppData *app_data = update_data->app_data;
    char *text = update_data->text;

    if (app_data->current_response_label) {
        const char *current = gtk_label_get_text(app_data->current_response_label);
        char *new_text = g_strconcat(current, text, NULL);
        gtk_label_set_text(app_data->current_response_label, new_text);
        g_free(new_text);
    }
    g_free(text);
    g_free(update_data);
    return G_SOURCE_REMOVE;
}

static gboolean finalize_generation_cb(gpointer data) {
    AppData *app_data = (AppData *)data;
    app_data->is_generating = FALSE;
    gtk_button_set_label(app_data->send_btn, "Send");
    gtk_widget_set_sensitive(GTK_WIDGET(app_data->send_btn), TRUE);

    if (app_data->current_response_label) {
        const char *final_text = gtk_label_get_text(app_data->current_response_label);
        json_object *assistant_msg = json_object_new_object();
        json_object_object_add(assistant_msg, "role", json_object_new_string("assistant"));
        json_object_object_add(assistant_msg, "content", json_object_new_string(final_text));
        json_object_array_add(app_data->messages_array, assistant_msg);
    }

    app_data->current_response_widget = NULL;
    app_data->current_response_label = NULL;
    return G_SOURCE_REMOVE;
}

static gboolean scroll_to_bottom_cb(gpointer data) {
    AppData *app_data = (AppData *)data;
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(app_data->chat_scroll);
    gtk_adjustment_set_value(vadj, gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj));
    return G_SOURCE_REMOVE;
}

static gboolean update_models_dropdown_cb(gpointer data) {
    AppData *app_data = (AppData *)data;
    if (app_data->model_count > 0) {
        GtkStringList *string_list = gtk_string_list_new(NULL);
        for (int i = 0; i < app_data->model_count; i++) {
            gtk_string_list_append(string_list, app_data->models[i]);
        }
        gtk_drop_down_set_model(app_data->model_dropdown, G_LIST_MODEL(string_list));
        g_object_unref(string_list);

        if (!app_data->current_model) {
            app_data->current_model = g_strdup(app_data->models[0]);
            gtk_drop_down_set_selected(app_data->model_dropdown, 0);
        }

        gtk_widget_set_sensitive(GTK_WIDGET(app_data->send_btn), TRUE);
        gtk_label_set_text(app_data->status_label, "Connected");
        gtk_widget_remove_css_class(GTK_WIDGET(app_data->status_label), "error");
        gtk_widget_add_css_class(GTK_WIDGET(app_data->status_label), "success");
    } else {
        gtk_label_set_text(app_data->status_label, "No models found");
        gtk_widget_remove_css_class(GTK_WIDGET(app_data->status_label), "success");
        gtk_widget_add_css_class(GTK_WIDGET(app_data->status_label), "error");
    }
    return G_SOURCE_REMOVE;
}

static gboolean reset_send_button_cb(gpointer data) {
    AppData *app_data = (AppData *)data;
    app_data->is_generating = FALSE;
    gtk_button_set_label(app_data->send_btn, "Send");
    gtk_widget_set_sensitive(GTK_WIDGET(app_data->send_btn), TRUE);
    return G_SOURCE_REMOVE;
}

// --- Public thread-safe UI update functions ---

void ui_schedule_update_response_label(AppData *app_data, char *text) {
    UpdateResponseData *update_data = g_malloc(sizeof(UpdateResponseData));
    update_data->app_data = app_data;
    update_data->text = text;
    g_idle_add(update_response_label_cb, update_data);
}

void ui_schedule_finalize_generation(AppData *app_data) {
    g_idle_add(finalize_generation_cb, app_data);
}

void ui_schedule_update_models_dropdown(AppData *app_data) {
    g_idle_add(update_models_dropdown_cb, app_data);
}

void ui_schedule_reset_send_button(AppData *app_data) {
    g_idle_add(reset_send_button_cb, app_data);
}

void ui_schedule_scroll_to_bottom(AppData *app_data) {
    g_idle_add(scroll_to_bottom_cb, app_data);
}

// --- UI building and event handling ---

static GtkWidget *create_message_widget(const ChatMessage *message) {
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(main_box, 12);
    gtk_widget_set_margin_end(main_box, 12);
    gtk_widget_set_margin_top(main_box, 6);
    gtk_widget_set_margin_bottom(main_box, 6);
    
    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_widget_add_css_class(frame, "card");
    
    GtkWidget *message_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(message_box, 12);
    gtk_widget_set_margin_end(message_box, 12);
    gtk_widget_set_margin_top(message_box, 8);
    gtk_widget_set_margin_bottom(message_box, 8);
    
    GtkWidget *sender_label = gtk_label_new(NULL);
    char *markup = g_markup_printf_escaped("<b>%s</b>", message->is_user ? "You" : "Assistant");
    gtk_label_set_markup(GTK_LABEL(sender_label), markup);
    g_free(markup);
    gtk_widget_set_halign(sender_label, GTK_ALIGN_START);
    gtk_widget_add_css_class(sender_label, "caption");
    
    GtkWidget *content_label = gtk_label_new(message->content);
    gtk_label_set_wrap(GTK_LABEL(content_label), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(content_label), PANGO_WRAP_WORD_CHAR);
    gtk_widget_set_halign(content_label, GTK_ALIGN_START);
    gtk_label_set_selectable(GTK_LABEL(content_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(content_label), 0);
    
    gtk_box_append(GTK_BOX(message_box), sender_label);
    gtk_box_append(GTK_BOX(message_box), content_label);
    gtk_frame_set_child(GTK_FRAME(frame), message_box);
    
    if (message->is_user) {
        gtk_widget_set_halign(main_box, GTK_ALIGN_END);
        gtk_widget_add_css_class(frame, "user-message");
    } else {
        gtk_widget_set_halign(main_box, GTK_ALIGN_START);
        gtk_widget_add_css_class(frame, "assistant-message");
    }
    
    gtk_box_append(GTK_BOX(main_box), frame);
    g_object_set_data(G_OBJECT(main_box), "content_label", content_label);
    return main_box;
}

static GtkWidget *add_message_to_chat(AppData *app_data, const ChatMessage *message) {
    GtkWidget *widget = create_message_widget(message);
    gtk_box_append(app_data->chat_box, widget);
    ui_schedule_scroll_to_bottom(app_data);
    return widget;
}

static void on_refresh_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    api_get_models((AppData *)user_data);
}

static void on_model_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data) {
    (void)pspec;
    AppData *app_data = (AppData *)user_data;
    guint selected = gtk_drop_down_get_selected(dropdown);
    if (selected != GTK_INVALID_LIST_POSITION && selected < (guint)app_data->model_count) {
        if (app_data->current_model) {
            free(app_data->current_model);
        }
        app_data->current_model = g_strdup(app_data->models[selected]);
    }
}

static void send_message(AppData *app_data) {
    if (app_data->is_generating) return;

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(app_data->text_buffer, &start, &end);
    char *text = gtk_text_buffer_get_text(app_data->text_buffer, &start, &end, FALSE);
    
    if (text && strlen(g_strstrip(text)) > 0) {
        ChatMessage user_msg = {.is_user = TRUE};
        strncpy(user_msg.content, text, MAX_MESSAGE_LEN - 1);
        add_message_to_chat(app_data, &user_msg);
        
        json_object *user_json = json_object_new_object();
        json_object_object_add(user_json, "role", json_object_new_string("user"));
        json_object_object_add(user_json, "content", json_object_new_string(text));
        json_object_array_add(app_data->messages_array, user_json);
        
        gtk_text_buffer_set_text(app_data->text_buffer, "", -1);
        
        ChatMessage assistant_msg = {.is_user = FALSE, .content = ""};
        app_data->current_response_widget = add_message_to_chat(app_data, &assistant_msg);
        app_data->current_response_label = GTK_LABEL(g_object_get_data(G_OBJECT(app_data->current_response_widget), "content_label"));
        
        app_data->is_generating = TRUE;
        gtk_button_set_label(app_data->send_btn, "Generating...");
        gtk_widget_set_sensitive(GTK_WIDGET(app_data->send_btn), FALSE);
        
        api_send_chat(app_data, g_strdup(text));
    }
    
    g_free(text);
}

static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    (void)controller;
    (void)keycode;
    if (keyval == GDK_KEY_Return && !(state & GDK_SHIFT_MASK)) {
        send_message((AppData *)user_data);
        return TRUE;
    }
    return FALSE;
}

static void on_send_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    send_message((AppData *)user_data);
}

void ui_build(GtkApplication *app, AppData *app_data) {
    app_data->window = GTK_WINDOW(gtk_application_window_new(app));
    gtk_window_set_title(app_data->window, "Ollama Chat");
    gtk_window_set_default_size(app_data->window, 800, 600);

    GtkWidget *header = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(app_data->window), header);

    app_data->model_dropdown = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    gtk_widget_set_tooltip_text(GTK_WIDGET(app_data->model_dropdown), "Select AI Model");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), GTK_WIDGET(app_data->model_dropdown));

    GtkWidget *refresh_btn = gtk_button_new_from_icon_name("view-refresh-symbolic");
    gtk_widget_set_tooltip_text(refresh_btn, "Refresh Models");
    g_signal_connect(refresh_btn, "clicked", G_CALLBACK(on_refresh_clicked), app_data);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), refresh_btn);

    app_data->status_label = GTK_LABEL(gtk_label_new("Disconnected"));
    gtk_widget_add_css_class(GTK_WIDGET(app_data->status_label), "caption");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), GTK_WIDGET(app_data->status_label));

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(app_data->window), main_box);

    app_data->chat_scroll = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new());
    gtk_scrolled_window_set_policy(app_data->chat_scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(GTK_WIDGET(app_data->chat_scroll), TRUE);
    app_data->chat_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_scrolled_window_set_child(app_data->chat_scroll, GTK_WIDGET(app_data->chat_box));
    gtk_box_append(GTK_BOX(main_box), GTK_WIDGET(app_data->chat_scroll));

    GtkWidget *input_frame = gtk_frame_new(NULL);
    gtk_widget_add_css_class(input_frame, "toolbar");
    GtkWidget *input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(input_box, 12);
    gtk_widget_set_margin_end(input_box, 12);
    gtk_widget_set_margin_top(input_box, 8);
    gtk_widget_set_margin_bottom(input_box, 8);
    
    app_data->text_buffer = gtk_text_buffer_new(NULL);
    app_data->text_view = GTK_TEXT_VIEW(gtk_text_view_new_with_buffer(app_data->text_buffer));
    gtk_text_view_set_wrap_mode(app_data->text_view, GTK_WRAP_WORD);
    gtk_widget_add_css_class(GTK_WIDGET(app_data->text_view), "entry");
    
    GtkWidget *text_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(text_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(text_scroll), 100);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(text_scroll), GTK_WIDGET(app_data->text_view));
    gtk_widget_set_hexpand(text_scroll, TRUE);
    
    app_data->send_btn = GTK_BUTTON(gtk_button_new_with_label("Send"));
    gtk_widget_add_css_class(GTK_WIDGET(app_data->send_btn), "suggested-action");
    gtk_widget_set_sensitive(GTK_WIDGET(app_data->send_btn), FALSE);
    g_signal_connect(app_data->send_btn, "clicked", G_CALLBACK(on_send_clicked), app_data);
    
    gtk_box_append(GTK_BOX(input_box), text_scroll);
    gtk_box_append(GTK_BOX(input_box), GTK_WIDGET(app_data->send_btn));
    gtk_frame_set_child(GTK_FRAME(input_frame), input_box);
    gtk_box_append(GTK_BOX(main_box), input_frame);
    
    GtkEventController *key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_key_pressed), app_data);
    gtk_widget_add_controller(GTK_WIDGET(app_data->text_view), key_controller);
    
    g_signal_connect(app_data->model_dropdown, "notify::selected", G_CALLBACK(on_model_changed), app_data);
    
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const char *css = ".user-message { background: alpha(@accent_color, 0.1); }\n"
                     ".assistant-message { background: alpha(@theme_fg_color, 0.05); }\n"
                     ".success { color: @success_color; }\n"
                     ".error { color: @error_color; }";
    gtk_css_provider_load_from_string(css_provider, css);
    gtk_style_context_add_provider_for_display(gtk_widget_get_display(GTK_WIDGET(app_data->window)),
                                              GTK_STYLE_PROVIDER(css_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    gtk_window_present(app_data->window);
    gtk_widget_grab_focus(GTK_WIDGET(app_data->text_view));
}