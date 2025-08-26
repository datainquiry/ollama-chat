#include "ui_header.h"
#include "history.h"
#include "ollama_api.h"
#include "ui_dialogs.h"

static void on_new_chat_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    history_start_new_chat((AppData *)user_data);
}

static void on_toggle_history_panel_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    AppData *app_data = (AppData *)user_data;
    app_data->history_panel_visible = !app_data->history_panel_visible;
    gtk_revealer_set_reveal_child(app_data->history_revealer, app_data->history_panel_visible);
}

static void on_refresh_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    api_get_models((AppData *)user_data);
}

static void on_preferences_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    show_preferences_dialog((AppData *)user_data);
}

static void on_about_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;
    show_about_dialog((AppData *)user_data);
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

static void on_new_chat_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;
    history_start_new_chat((AppData *)user_data);
}

static void on_refresh_models_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;
    api_get_models((AppData *)user_data);
}

static void on_preferences_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;
    show_preferences_dialog((AppData *)user_data);
}

static void on_toggle_history_panel_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;
    AppData *app_data = (AppData *)user_data;
    app_data->history_panel_visible = !app_data->history_panel_visible;
    gtk_revealer_set_reveal_child(app_data->history_revealer, app_data->history_panel_visible);
}


static void on_quit_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;
    GApplication *app = G_APPLICATION(user_data);
    g_application_quit(app);
}

GtkWidget *create_header_bar(AppData *app_data) {
    const GActionEntry app_entries[] = {
        { "new-chat", on_new_chat_action, NULL, NULL, NULL, {0} },
        { "refresh-models", on_refresh_models_action, NULL, NULL, NULL, {0} },
        { "preferences", on_preferences_action, NULL, NULL, NULL, {0} },
        { "toggle-history", on_toggle_history_panel_action, NULL, NULL, NULL, {0} },
        { "about", on_about_action, NULL, NULL, NULL, {0} },
        { "rename-chat", on_rename_chat_action, NULL, NULL, NULL, {0} },
        { "delete-chat", on_delete_chat_action, NULL, NULL, NULL, {0} },
        { "quit", on_quit_action, NULL, NULL, NULL, {0} }
    };
    g_action_map_add_action_entries(G_ACTION_MAP(app_data->app), app_entries, G_N_ELEMENTS(app_entries), app_data);

    const char *accels_for_new_chat[] = { "<Control>n", NULL };
    const char *accels_for_refresh_models[] = { "<Control>r", NULL };
    const char *accels_for_preferences[] = { "<Control>comma", NULL };
    const char *accels_for_toggle_history[] = { "<Control>h", NULL };
    const char *accels_for_quit[] = { "<Control>q", NULL };
    gtk_application_set_accels_for_action(app_data->app, "app.new-chat", accels_for_new_chat);
    gtk_application_set_accels_for_action(app_data->app, "app.refresh-models", accels_for_refresh_models);
    gtk_application_set_accels_for_action(app_data->app, "app.preferences", accels_for_preferences);
    gtk_application_set_accels_for_action(app_data->app, "app.toggle-history", accels_for_toggle_history);
    gtk_application_set_accels_for_action(app_data->app, "app.quit", accels_for_quit);

    GtkWidget *header = gtk_header_bar_new();

    GtkWidget *toggle_history_btn = gtk_button_new_from_icon_name("sidebar-show-symbolic");
    gtk_widget_set_tooltip_text(toggle_history_btn, "Toggle History Panel");
    g_signal_connect(toggle_history_btn, "clicked", G_CALLBACK(on_toggle_history_panel_clicked), app_data);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), toggle_history_btn);

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

    GtkWidget *prefs_btn = gtk_button_new_from_icon_name("emblem-system-symbolic");
    gtk_widget_set_tooltip_text(prefs_btn, "Preferences");
    g_signal_connect(prefs_btn, "clicked", G_CALLBACK(on_preferences_clicked), app_data);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), prefs_btn);

    GMenu *menu = g_menu_new();
    g_menu_append(menu, "About", "app.about");
    GtkWidget *menu_button = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_button), "open-menu-symbolic");
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_button), G_MENU_MODEL(menu));
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), menu_button);
    g_object_unref(menu);

    app_data->status_label = GTK_LABEL(gtk_label_new("Disconnected"));
    gtk_widget_add_css_class(GTK_WIDGET(app_data->status_label), "caption");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), GTK_WIDGET(app_data->status_label));

    return header;
}
