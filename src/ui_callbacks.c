#include "ui_callbacks.h"
#include "history.h"
#include "ui.h"
#include "markdown.h"
#include "ui_chat_view.h"

void on_model_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data) {
    (void)pspec;
    AppData *app_data = (AppData *)user_data;
    guint selected = gtk_drop_down_get_selected(dropdown);
    if (selected != GTK_INVALID_LIST_POSITION && selected < (guint)app_data->model_count) {
        if (app_data->current_model) {
            g_free(app_data->current_model);
        }
        app_data->current_model = g_strdup(app_data->models[selected]);
    }
}


typedef struct {
    AppData *app_data;
    char *text;
} UpdateResponseData;

static gboolean update_response_label_cb(gpointer data) {
    UpdateResponseData *update_data = (UpdateResponseData *)data;
    AppData *app_data = update_data->app_data;
    char *text = update_data->text;

    if (app_data->current_response_label) {
        g_string_append(app_data->response_buffer, text);
        char *pango_markup = markdown_to_pango(app_data->response_buffer->str);
        gtk_label_set_markup(GTK_LABEL(app_data->current_response_label), pango_markup);
        g_free(pango_markup);
    }
    g_free(text);
    g_free(update_data);
    return G_SOURCE_REMOVE;
}

static gboolean finalize_generation_cb(gpointer data) {
    AppData *app_data = (AppData *)data;
    app_data->is_generating = FALSE;
    gtk_widget_set_sensitive(GTK_WIDGET(app_data->send_btn), TRUE);
    gtk_button_set_icon_name(app_data->send_btn, "document-send-symbolic");
    gtk_widget_set_tooltip_text(GTK_WIDGET(app_data->send_btn), "Send Message");
    gtk_spinner_stop(app_data->spinner);
    gtk_widget_set_visible(GTK_WIDGET(app_data->spinner), FALSE);

    if (app_data->current_response_widget) {
        const char *final_text = app_data->response_buffer->str;

        // Save to history
        json_object *assistant_msg_json = json_object_new_object();
        json_object_object_add(assistant_msg_json, "role", json_object_new_string("assistant"));
        json_object_object_add(assistant_msg_json, "content", json_object_new_string(final_text));
        json_object_array_add(app_data->messages_array, assistant_msg_json);
        history_save_chat(app_data);

        // Rerender the widget with the final content
        rerender_message_widget(app_data->current_response_widget, final_text);

        app_data->current_response_widget = NULL;
        app_data->current_response_label = NULL;
        
        // Clear the buffer for the next response
        g_string_assign(app_data->response_buffer, "");
    }

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

        gulong handler_id = (gulong)g_object_get_data(G_OBJECT(app_data->model_dropdown), "model-changed-handler-id");
        if (handler_id > 0) {
            g_signal_handler_disconnect(app_data->model_dropdown, handler_id);
        }

        gtk_drop_down_set_model(app_data->model_dropdown, G_LIST_MODEL(string_list));
        g_object_unref(string_list);

        guint selected_index = 0;
        gboolean model_found = FALSE;
        if (app_data->current_model) {
            for (int i = 0; i < app_data->model_count; i++) {
                if (g_strcmp0(app_data->current_model, app_data->models[i]) == 0) {
                    selected_index = i;
                    model_found = TRUE;
                    break;
                }
            }
        }

        if (!model_found) {
            if (app_data->current_model) g_free(app_data->current_model);
            app_data->current_model = g_strdup(app_data->models[0]);
        }
        gtk_drop_down_set_selected(app_data->model_dropdown, selected_index);

        if (handler_id > 0) {
            g_signal_connect(app_data->model_dropdown, "notify::selected", G_CALLBACK(on_model_changed), app_data);
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
    gtk_widget_set_sensitive(GTK_WIDGET(app_data->send_btn), TRUE);
    gtk_button_set_icon_name(app_data->send_btn, "document-send-symbolic");
    gtk_widget_set_tooltip_text(GTK_WIDGET(app_data->send_btn), "Send Message");
    gtk_spinner_stop(app_data->spinner);
    gtk_widget_set_visible(GTK_WIDGET(app_data->spinner), FALSE);
    return G_SOURCE_REMOVE;
}

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

typedef struct {
    AppData *app_data;
    char *status;
    char *css_class;
} UpdateStatusData;

static gboolean update_status_label_cb(gpointer data) {
    UpdateStatusData *update_data = (UpdateStatusData *)data;
    gtk_label_set_text(update_data->app_data->status_label, update_data->status);
    gtk_widget_remove_css_class(GTK_WIDGET(update_data->app_data->status_label), "success");
    gtk_widget_remove_css_class(GTK_WIDGET(update_data->app_data->status_label), "error");
    if (update_data->css_class) {
        gtk_widget_add_css_class(GTK_WIDGET(update_data->app_data->status_label), update_data->css_class);
    }
    if (g_strcmp0(update_data->status, "Connected") == 0) {
        gtk_widget_set_sensitive(GTK_WIDGET(update_data->app_data->send_btn), TRUE);
    }
    g_free(update_data->status);
    g_free(update_data->css_class);
    g_free(update_data);
    return G_SOURCE_REMOVE;
}

void ui_schedule_update_status_label(AppData *app_data, const char *status, const char *css_class) {
    UpdateStatusData *update_data = g_malloc(sizeof(UpdateStatusData));
    update_data->app_data = app_data;
    update_data->status = g_strdup(status);
    update_data->css_class = g_strdup(css_class);
    g_idle_add(update_status_label_cb, update_data);
}
