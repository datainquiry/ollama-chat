#include "ui.h"
#include "ollama_api.h"
#include "web_search.h"
#include "history.h"
#include <pthread.h>

// --- Forward Declarations ---
static void send_message(AppData *app_data);
static GtkWidget *create_message_widget(const ChatMessage *message);
static GtkWidget *add_message_to_chat(AppData *app_data, const ChatMessage *message);

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
        history_save_chat(app_data);
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

// --- Chat View Manipulation ---

void ui_clear_chat_view(AppData *app_data) {
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(GTK_WIDGET(app_data->chat_box))) != NULL) {
        gtk_box_remove(app_data->chat_box, child);
    }
}

void ui_redisplay_chat_history(AppData *app_data) {
    if (!app_data->messages_array) return;
    int len = json_object_array_length(app_data->messages_array);
    for (int i = 0; i < len; i++) {
        json_object *msg_obj = json_object_array_get_idx(app_data->messages_array, i);
        json_object *role_obj, *content_obj;
        if (json_object_object_get_ex(msg_obj, "role", &role_obj) &&
            json_object_object_get_ex(msg_obj, "content", &content_obj)) {
            
            const char *role = json_object_get_string(role_obj);
            const char *content = json_object_get_string(content_obj);
            
            ChatMessage msg;
            msg.is_user = (strcmp(role, "user") == 0);
            strncpy(msg.content, content, MAX_MESSAGE_LEN - 1);
            msg.content[MAX_MESSAGE_LEN - 1] = '\0';
            
            add_message_to_chat(app_data, &msg);
        }
    }
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

static void on_new_chat_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    history_start_new_chat((AppData *)user_data);
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
    char *stripped_text = g_strstrip(text);

    if (stripped_text && strlen(stripped_text) > 0) {
        ChatMessage user_msg = {.is_user = TRUE};
        strncpy(user_msg.content, stripped_text, MAX_MESSAGE_LEN - 1);
        add_message_to_chat(app_data, &user_msg);

        char *final_text_to_send = NULL;
        GString *prepended_content = g_string_new("");

        char *url = find_url(stripped_text);
        if (url) {
            char *url_content = fetch_url_content(url);
            if (url_content) {
                g_string_append_printf(prepended_content, "Content from URL %s:\n\n%s\n\n---\n\n", url, url_content);
                g_free(url_content);
            }
            g_free(url);
        }

        GRegex *regex = g_regex_new("@[\\w\\d\\._-]+", 0, 0, NULL);
        GMatchInfo *match_info;
        if (g_regex_match(regex, stripped_text, 0, &match_info)) {
            while (g_match_info_matches(match_info)) {
                char *match = g_match_info_fetch(match_info, 0);
                if (match) {
                    char *filename = match + 1;
                    char *file_content = NULL;
                    GError *error = NULL;
                    if (g_file_get_contents(filename, &file_content, NULL, &error)) {
                        g_string_append_printf(prepended_content, "Content from file %s:\n\n%s\n\n---\n\n", filename, file_content);
                        g_free(file_content);
                    } else {
                        fprintf(stderr, "Error reading file %s: %s\n", filename, error->message);
                        g_error_free(error);
                    }
                    g_free(match);
                }
                g_match_info_next(match_info, NULL);
            }
        }
        if (match_info) g_match_info_free(match_info);
        if (regex) g_regex_unref(regex);

        if (prepended_content->len > 0) {
            final_text_to_send = g_strconcat(prepended_content->str, "User message: ", stripped_text, NULL);
        } else {
            final_text_to_send = g_strdup(stripped_text);
        }
        g_string_free(prepended_content, TRUE);

        json_object *user_json = json_object_new_object();
        json_object_object_add(user_json, "role", json_object_new_string("user"));
        json_object_object_add(user_json, "content", json_object_new_string(final_text_to_send));
        json_object_array_add(app_data->messages_array, user_json);
        history_save_chat(app_data);

        gtk_text_buffer_set_text(app_data->text_buffer, "", -1);

        ChatMessage assistant_msg = {.is_user = FALSE, .content = ""};
        app_data->current_response_widget = add_message_to_chat(app_data, &assistant_msg);
        app_data->current_response_label = GTK_LABEL(g_object_get_data(G_OBJECT(app_data->current_response_widget), "content_label"));

        app_data->is_generating = TRUE;
        gtk_button_set_label(app_data->send_btn, "Generating...");
        gtk_widget_set_sensitive(GTK_WIDGET(app_data->send_btn), FALSE);

        api_send_chat(app_data, final_text_to_send);
    }
    g_free(text);
}

static void on_file_chooser_response(GtkNativeDialog *native, int response_id, gpointer user_data) {
    if (response_id == GTK_RESPONSE_ACCEPT) {
        AppData *app_data = (AppData *)user_data;
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(native);
        GFile *file = gtk_file_chooser_get_file(chooser);
        if (file) {
            char *filename = g_file_get_basename(file);
            if (filename) {
                GtkTextIter iter;
                gtk_text_buffer_get_iter_at_mark(app_data->text_buffer, &iter, gtk_text_buffer_get_insert(app_data->text_buffer));
                gunichar prev_char = gtk_text_iter_get_char(&iter);
                if (prev_char != '@' && gtk_text_iter_backward_char(&iter)) {
                    prev_char = gtk_text_iter_get_char(&iter);
                }
                if (prev_char == '@') {
                     GtkTextIter end_iter = iter;
                     gtk_text_iter_forward_char(&end_iter);
                     gtk_text_buffer_delete(app_data->text_buffer, &iter, &end_iter);
                }
                gtk_text_buffer_insert(app_data->text_buffer, &iter, "@", 1);
                gtk_text_buffer_insert(app_data->text_buffer, &iter, filename, -1);
                g_free(filename);
            }
            g_object_unref(file);
        }
    }
    g_object_unref(native);
}

static void open_file_dialog(AppData *app_data) {
    GtkFileChooserNative *native = gtk_file_chooser_native_new("Open File", GTK_WINDOW(app_data->window), GTK_FILE_CHOOSER_ACTION_OPEN, "_Open", "_Cancel");
    g_signal_connect(native, "response", G_CALLBACK(on_file_chooser_response), app_data);
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(native));
}

static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    (void)controller; (void)keycode;
    if (keyval == GDK_KEY_Return && !(state & GDK_SHIFT_MASK)) {
        send_message((AppData *)user_data);
        return TRUE;
    } else if (keyval == GDK_KEY_at) {
        open_file_dialog((AppData *)user_data);
        return TRUE;
    }
    return FALSE;
}

static void on_send_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    send_message((AppData *)user_data);
}

static void on_history_context_menu(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    GtkListBoxRow *row = gtk_list_box_get_row_at_y(app_data->history_list_box, y);

    if (n_press == 1 && gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture)) == GDK_BUTTON_SECONDARY && row) {
        gtk_list_box_select_row(app_data->history_list_box, row);

        GMenu *menu = g_menu_new();
        g_menu_append(menu, "Rename", "app.rename-chat");
        g_menu_append(menu, "Delete", "app.delete-chat");

        GtkWidget *popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
        gtk_widget_set_parent(popover, GTK_WIDGET(app_data->history_list_box));

        GtkAllocation allocation;
        gtk_widget_get_allocation(GTK_WIDGET(row), &allocation);
        GdkRectangle rect = { .x = allocation.x, .y = allocation.y, .width = allocation.width, .height = allocation.height };

        gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
        gtk_popover_popup(GTK_POPOVER(popover));
        g_object_unref(menu);
    }
}

static void on_rename_dialog_response(GtkDialog *dialog, int response_id, gpointer user_data) {
    if (response_id == GTK_RESPONSE_ACCEPT) {
        AppData *app_data = (AppData *)user_data;
        GtkWidget *content_area = gtk_dialog_get_content_area(dialog);
        GtkWidget *entry = gtk_widget_get_first_child(content_area);
        const char *new_name = gtk_editable_get_text(GTK_EDITABLE(entry));
        
        GtkListBoxRow *row = gtk_list_box_get_selected_row(app_data->history_list_box);
        if (row && strlen(new_name) > 0) {
            const char *old_name = gtk_label_get_text(GTK_LABEL(gtk_list_box_row_get_child(row)));
            history_rename_chat(app_data, old_name, new_name);
        }
    }
    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void show_rename_dialog(AppData *app_data, const char *old_name) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Rename Chat",
                                                    GTK_WINDOW(app_data->window),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    "_Rename", GTK_RESPONSE_ACCEPT,
                                                    NULL);
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(entry), old_name);
    gtk_box_append(GTK_BOX(content_area), entry);

    g_signal_connect(dialog, "response", G_CALLBACK(on_rename_dialog_response), app_data);
    gtk_window_present(GTK_WINDOW(dialog));
}

static void on_rename_chat_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action; (void)parameter;
    AppData *app_data = (AppData *)user_data;
    GtkListBoxRow *row = gtk_list_box_get_selected_row(app_data->history_list_box);
    if (row) {
        const char *chat_id = gtk_label_get_text(GTK_LABEL(gtk_list_box_row_get_child(row)));
        show_rename_dialog(app_data, chat_id);
    }
}

static void on_delete_chat_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action; (void)parameter;
    AppData *app_data = (AppData *)user_data;
    GtkListBoxRow *row = gtk_list_box_get_selected_row(app_data->history_list_box);
    if (row) {
        const char *chat_id = gtk_label_get_text(GTK_LABEL(gtk_list_box_row_get_child(row)));
        history_delete_chat(app_data, chat_id);
    }
}

static GtkWidget* create_history_row(gpointer item, gpointer user_data) {
    (void)user_data;
    GtkStringObject *str_obj = GTK_STRING_OBJECT(item);
    const char *id = gtk_string_object_get_string(str_obj);
    GtkWidget *label = gtk_label_new(id);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    return label;
}

void ui_build(GtkApplication *app, AppData *app_data) {
    app_data->window = GTK_WINDOW(gtk_application_window_new(app));
    gtk_window_set_title(app_data->window, "Ollama Chat");
    gtk_window_set_default_size(app_data->window, 1024, 768);

    // --- Actions for context menu ---
    const GActionEntry app_entries[] = {
        { "rename-chat", on_rename_chat_action, NULL, NULL, NULL },
        { "delete-chat", on_delete_chat_action, NULL, NULL, NULL }
    };
    g_action_map_add_action_entries(G_ACTION_MAP(app), app_entries, G_N_ELEMENTS(app_entries), app_data);


    GtkWidget *header = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(app_data->window), header);

    GtkWidget *new_chat_btn = gtk_button_new_from_icon_name("document-new-symbolic");
    gtk_widget_set_tooltip_text(new_chat_btn, "New Chat");
    g_signal_connect(new_chat_btn, "clicked", G_CALLBACK(on_new_chat_clicked), app_data);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), new_chat_btn);

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

    GtkWidget *main_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_window_set_child(GTK_WINDOW(app_data->window), main_paned);

    // --- Left Panel: History ---
    GtkWidget *history_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(history_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    app_data->history_list_box = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(app_data->history_list_box, GTK_SELECTION_SINGLE);
    
    gtk_list_box_bind_model(app_data->history_list_box, G_LIST_MODEL(app_data->history_store), create_history_row, NULL, NULL);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(history_scroll), GTK_WIDGET(app_data->history_list_box));
    gtk_paned_set_start_child(GTK_PANED(main_paned), history_scroll);
    gtk_paned_set_position(GTK_PANED(main_paned), 1024 * 0.25);
    
    g_signal_connect(app_data->history_list_box, "row-activated", G_CALLBACK(history_load_selected_chat), app_data);

    GtkGesture *context_gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(context_gesture), GDK_BUTTON_SECONDARY);
    g_signal_connect(context_gesture, "pressed", G_CALLBACK(on_history_context_menu), app_data);
    gtk_widget_add_controller(GTK_WIDGET(app_data->history_list_box), GTK_EVENT_CONTROLLER(context_gesture));


    // --- Right Panel: Chat View ---
    GtkWidget *chat_area_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_paned_set_end_child(GTK_PANED(main_paned), chat_area_box);
    gtk_paned_set_resize_end_child(GTK_PANED(main_paned), TRUE);

    app_data->chat_scroll = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new());
    gtk_scrolled_window_set_policy(app_data->chat_scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(GTK_WIDGET(app_data->chat_scroll), TRUE);
    app_data->chat_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_scrolled_window_set_child(app_data->chat_scroll, GTK_WIDGET(app_data->chat_box));
    gtk_box_append(GTK_BOX(chat_area_box), GTK_WIDGET(app_data->chat_scroll));

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
    gtk_box_append(GTK_BOX(chat_area_box), input_frame);
    
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