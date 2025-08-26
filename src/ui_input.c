#include "ui_input.h"
#include "ollama_api.h"
#include "web_search.h"
#include "history.h"
#include "ui_chat_view.h"

static gboolean is_binary_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) return FALSE;

    unsigned char buffer[1024];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);
    fclose(file);

    for (size_t i = 0; i < bytes_read; i++) {
        if (buffer[i] == 0) {
            return TRUE;
        }
    }
    return FALSE;
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

        if (app_data->web_search_enabled) {
            char *url = find_url(stripped_text);
            if (url) {
                char *url_content = fetch_url_content(url);
                if (url_content) {
                    g_string_append_printf(prepended_content, "Content from URL %s:\n\n%s\n\n---\n\n", url, url_content);
                    g_free(url_content);
                }
                g_free(url);
            }
        }

        GRegex *regex = g_regex_new("@[\\w\\d\\._-]+", 0, 0, NULL);
        GMatchInfo *match_info;
        if (g_regex_match(regex, stripped_text, 0, &match_info)) {
            while (g_match_info_matches(match_info)) {
                char *match = g_match_info_fetch(match_info, 0);
                if (match) {
                    char *filename = match + 1;
                    if (is_binary_file(filename)) {
                        g_string_append_printf(prepended_content, "Content from binary file %s was not included.\n\n", filename);
                    } else {
                        char *file_content = NULL;
                        GError *error = NULL;
                        if (g_file_get_contents(filename, &file_content, NULL, &error)) {
                            g_string_append_printf(prepended_content, "Content from file %s:\n\n%s\n\n---\n\n", filename, file_content);
                            g_free(file_content);
                        } else {
                            fprintf(stderr, "Error reading file %s: %s\n", filename, error->message);
                            g_error_free(error);
                        }
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
        app_data->request_cancelled = FALSE;
        gtk_widget_set_sensitive(GTK_WIDGET(app_data->send_btn), TRUE);
        gtk_button_set_icon_name(app_data->send_btn, "media-playback-stop-symbolic");
        gtk_widget_set_tooltip_text(GTK_WIDGET(app_data->send_btn), "Cancel Request");
        gtk_widget_set_visible(GTK_WIDGET(app_data->spinner), TRUE);
        gtk_spinner_start(app_data->spinner);

        api_send_chat(app_data, final_text_to_send);
    }
    g_free(text);
}

static void on_send_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    AppData *app_data = (AppData *)user_data;
    if (app_data->is_generating) {
        app_data->request_cancelled = TRUE;
    } else {
        send_message(app_data);
    }
}

static void on_open_file_dialog_finish(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    AppData *app_data = (AppData *)user_data;
    GFile *file = gtk_file_dialog_open_finish(dialog, res, NULL);
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

static void open_file_dialog(AppData *app_data) {
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_open(dialog, GTK_WINDOW(app_data->window), NULL, on_open_file_dialog_finish, app_data);
    g_object_unref(dialog);
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

GtkWidget *create_input_area(AppData *app_data) {
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
    
    app_data->send_btn = GTK_BUTTON(gtk_button_new_from_icon_name("document-send-symbolic"));
    gtk_widget_set_tooltip_text(GTK_WIDGET(app_data->send_btn), "Send Message");
    gtk_widget_add_css_class(GTK_WIDGET(app_data->send_btn), "suggested-action");
    gtk_widget_set_sensitive(GTK_WIDGET(app_data->send_btn), FALSE);
    g_signal_connect(app_data->send_btn, "clicked", G_CALLBACK(on_send_clicked), app_data);

    app_data->spinner = GTK_SPINNER(gtk_spinner_new());
    gtk_widget_set_visible(GTK_WIDGET(app_data->spinner), FALSE);
    
    gtk_box_append(GTK_BOX(input_box), text_scroll);
    gtk_box_append(GTK_BOX(input_box), GTK_WIDGET(app_data->spinner));
    gtk_box_append(GTK_BOX(input_box), GTK_WIDGET(app_data->send_btn));
    gtk_frame_set_child(GTK_FRAME(input_frame), input_box);
    
    GtkEventController *key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_key_pressed), app_data);
    gtk_widget_add_controller(GTK_WIDGET(app_data->text_view), key_controller);

    return input_frame;
}