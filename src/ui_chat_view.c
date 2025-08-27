#include <gtksourceview/gtksource.h>
#include "ui_chat_view.h"
#include "ui_callbacks.h"
#include "markdown.h"

static gboolean revert_copy_icon(gpointer user_data) {
    gtk_button_set_icon_name(GTK_BUTTON(user_data), "edit-copy-symbolic");
    return G_SOURCE_REMOVE;
}

/**
 * Data received by this function has been duplicated. Therefore,
 * it is deallocated here.
 */
static void on_copy_clicked(GtkButton *button, gpointer user_data) {
    const char* text = (const char*) user_data;
    gdk_clipboard_set_text(
            gdk_display_get_clipboard(
                gtk_widget_get_display(GTK_WIDGET(button))), text);
    gtk_button_set_icon_name(button, "object-select-symbolic");
    g_timeout_add(1000, revert_copy_icon, button);
    g_free(user_data);
}

static GtkWidget *create_chat_bubble_header(const ChatMessage *message) {
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *sender_label = gtk_label_new(NULL);
    char *markup = g_markup_printf_escaped("<b>%s</b>", message->is_user ? "You" : "Assistant");
    gtk_label_set_markup(GTK_LABEL(sender_label), markup);
    g_free(markup);
    gtk_widget_set_halign(sender_label, GTK_ALIGN_START);
    gtk_widget_add_css_class(sender_label, "caption");
    gtk_box_append(GTK_BOX(header_box), sender_label);
    return header_box;
}

static GtkWidget *create_text_label(const char *pango_markup) {
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), pango_markup);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    return label;
}

static GtkWidget *create_code_block(const char *code, const char *lang) {
    GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default();
    GtkSourceLanguage *language = lang ? gtk_source_language_manager_get_language(lm, lang) : NULL;

    GtkSourceBuffer *buffer = gtk_source_buffer_new(NULL);
    gtk_source_buffer_set_language(buffer, language);
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), code, -1);

    GtkWidget *source_view = gtk_source_view_new_with_buffer(buffer);
    gtk_widget_set_hexpand(source_view, TRUE);
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(source_view), TRUE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(source_view), FALSE);
    gtk_widget_add_css_class(source_view, "code-block");

    GtkWidget *scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), source_view);
    
    return scrolled_window;
}

static void parse_and_display_message(GtkBox *message_box, const char *content) {
    const char *p = content;
    while (*p) {
        const char *code_start = strstr(p, "```");
        if (code_start) {
            if (code_start > p) {
                char *text = g_strndup(p, code_start - p);
                char *pango_markup = markdown_to_pango(text);
                GtkWidget *label = create_text_label(pango_markup);
                gtk_box_append(message_box, label);
                g_free(text);
                g_free(pango_markup);
            }

            const char *code_end = strstr(code_start + 3, "```");
            if (code_end) {
                const char *lang_end = strchr(code_start + 3, '\n');
                char *lang = NULL;
                if (lang_end && lang_end < code_end) {
                    lang = g_strndup(code_start + 3, lang_end - (code_start + 3));
                }

                char *code = g_strndup(lang_end ? lang_end + 1 : code_start + 3, code_end - (lang_end ? lang_end + 1 : code_start + 3));
                GtkWidget *code_widget = create_code_block(code, lang);
                gtk_box_append(message_box, code_widget);

                g_free(lang);
                g_free(code);
                p = code_end + 3;
            } else {
                p = code_start + 3;
            }
        } else {
            char *pango_markup = markdown_to_pango(p);
            GtkWidget *label = create_text_label(pango_markup);
            gtk_box_append(message_box, label);
            g_free(pango_markup);
            break;
        }
    }
}

static void add_assistant_message_actions(GtkBox *header_box, const char *content) {
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(header_box, spacer);

    GtkWidget *copy_btn = gtk_button_new_from_icon_name("edit-copy-symbolic");
    gtk_widget_add_css_class(copy_btn, "copy-button");
    // `content` is duplicated. The callback should deallocate the buffer.
    g_signal_connect(copy_btn, "clicked", G_CALLBACK(on_copy_clicked), g_strdup(content));
    gtk_box_append(header_box, copy_btn);
}

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
    gtk_widget_set_hexpand(message_box, TRUE);

    GtkWidget *header_box = create_chat_bubble_header(message);
    gtk_box_append(GTK_BOX(message_box), header_box);

    if (strlen(message->content) > 0) {
        parse_and_display_message(GTK_BOX(message_box), message->content);
    }

    gtk_frame_set_child(GTK_FRAME(frame), message_box);
    
    if (message->is_user) {
        gtk_widget_add_css_class(frame, "user-message");
    } else {
        gtk_widget_add_css_class(frame, "assistant-message");
        add_assistant_message_actions(GTK_BOX(header_box), message->content);
    }
    
    gtk_box_append(GTK_BOX(main_box), frame);

    GtkWidget *content_label = NULL;
    if (strlen(message->content) == 0 && !message->is_user) {
        content_label = gtk_label_new(NULL);
        gtk_label_set_wrap(GTK_LABEL(content_label), TRUE);
        gtk_label_set_wrap_mode(GTK_LABEL(content_label), PANGO_WRAP_WORD_CHAR);
        gtk_widget_set_halign(content_label, GTK_ALIGN_START);
        gtk_label_set_selectable(GTK_LABEL(content_label), TRUE);
        gtk_label_set_xalign(GTK_LABEL(content_label), 0);
        gtk_box_append(GTK_BOX(message_box), content_label);
    }

    g_object_set_data(G_OBJECT(main_box), "content_label", content_label);
    return main_box;
}

GtkWidget *add_message_to_chat(AppData *app_data, const ChatMessage *message) {
    GtkWidget *widget = create_message_widget(message);
    gtk_box_append(app_data->chat_box, widget);
    ui_schedule_scroll_to_bottom(app_data);
    return widget;
}

void ui_clear_chat_view(AppData *app_data) {
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(GTK_WIDGET(app_data->chat_box))) != NULL) {
        gtk_box_remove(app_data->chat_box, child);
    }
}

static void chat_message_from_json(json_object *msg_obj, ChatMessage *msg) {
    json_object *role_obj, *content_obj;
    if (json_object_object_get_ex(msg_obj, "role", &role_obj) &&
        json_object_object_get_ex(msg_obj, "content", &content_obj)) {
        
        const char *role = json_object_get_string(role_obj);
        const char *content = json_object_get_string(content_obj);
        
        msg->is_user = (strcmp(role, "user") == 0);
        strncpy(msg->content, content, MAX_MESSAGE_LEN - 1);
        msg->content[MAX_MESSAGE_LEN - 1] = '\0';
    }
}

void ui_redisplay_chat_history(AppData *app_data) {
    if (!app_data->messages_array) return;
    int len = json_object_array_length(app_data->messages_array);
    for (int i = 0; i < len; i++) {
        json_object *msg_obj = json_object_array_get_idx(app_data->messages_array, i);
        ChatMessage msg = {0};
        chat_message_from_json(msg_obj, &msg);
        if (strlen(msg.content) > 0) {
            add_message_to_chat(app_data, &msg);
        }
    }
}

GtkWidget *create_chat_view(AppData *app_data) {
    GtkWidget *chat_area_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(chat_area_box, TRUE);

    app_data->chat_scroll = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new());
    gtk_scrolled_window_set_policy(app_data->chat_scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(GTK_WIDGET(app_data->chat_scroll), TRUE);
    app_data->chat_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_scrolled_window_set_child(app_data->chat_scroll, GTK_WIDGET(app_data->chat_box));
    gtk_box_append(GTK_BOX(chat_area_box), GTK_WIDGET(app_data->chat_scroll));

    return chat_area_box;
}

void rerender_message_widget(GtkWidget *widget, const char *new_content) {
    GtkWidget *frame = gtk_widget_get_first_child(widget);
    if (!frame) return;
    GtkWidget *message_box = gtk_widget_get_first_child(frame);
    if (!message_box) return;

    // Remove all children except the header
    GtkWidget *header = gtk_widget_get_first_child(message_box);
    GtkWidget *child;
    while ((child = gtk_widget_get_next_sibling(header)) != NULL) {
        gtk_box_remove(GTK_BOX(message_box), child);
    }

    // Reparse and add the new content
    parse_and_display_message(GTK_BOX(message_box), new_content);
}
