#include "ui.h"
#include "ui_callbacks.h"
#include "ui_chat_view.h"
#include "ui_input.h"
#include "ui_history.h"
#include "ui_header.h"

static void on_window_destroy(GtkApplicationWindow *window, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    gtk_window_get_default_size(GTK_WINDOW(window), &app_data->window_width, &app_data->window_height);
}

void ui_build(GtkApplication *app, AppData *app_data) {
    app_data->window = GTK_WINDOW(gtk_application_window_new(app));
    gtk_window_set_title(app_data->window, "Ollama Chat");
    gtk_window_set_default_size(app_data->window, app_data->window_width, app_data->window_height);
    g_signal_connect(app_data->window, "destroy", G_CALLBACK(on_window_destroy), app_data);

    if (g_strcmp0(app_data->theme, "dark") == 0) {
        g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", TRUE, NULL);
    } else {
        g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", FALSE, NULL);
    }

    GtkWidget *header = create_header_bar(app_data);
    gtk_window_set_titlebar(GTK_WINDOW(app_data->window), header);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_window_set_child(GTK_WINDOW(app_data->window), main_box);

    GtkWidget *history_panel = create_history_panel(app_data);
    gtk_box_append(GTK_BOX(main_box), history_panel);

    GtkWidget *chat_area_box = create_chat_view(app_data);
    gtk_box_append(GTK_BOX(main_box), chat_area_box);

    GtkWidget *input_area = create_input_area(app_data);
    gtk_box_append(GTK_BOX(chat_area_box), input_area);

    g_signal_connect(app_data->model_dropdown, "notify::selected", G_CALLBACK(on_model_changed), app_data);
    
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const char *css = ".user-message { background: alpha(@accent_color, 0.1); }\n"
                     ".assistant-message { background: alpha(@theme_fg_color, 0.05); }\n"
                     ".success { color: @success_color; }\n"
                     ".error { color: @error_color; }\n"
                     ".copy-button { background: transparent; border: none; }";
    gtk_css_provider_load_from_string(css_provider, css);
    gtk_style_context_add_provider_for_display(gtk_widget_get_display(GTK_WIDGET(app_data->window)),
                                              GTK_STYLE_PROVIDER(css_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    gtk_window_present(app_data->window);
    gtk_widget_grab_focus(GTK_WIDGET(app_data->text_view));
}