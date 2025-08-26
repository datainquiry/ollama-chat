#include "ui_history.h"
#include "history.h"

static GtkWidget* create_history_row(gpointer item, gpointer user_data) {
    (void)user_data;
    GtkStringObject *str_obj = GTK_STRING_OBJECT(item);
    const char *id = gtk_string_object_get_string(str_obj);
    GtkWidget *label = gtk_label_new(id);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    return label;
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

GtkWidget *create_history_panel(AppData *app_data) {
    app_data->history_revealer = GTK_REVEALER(gtk_revealer_new());
    gtk_revealer_set_transition_type(app_data->history_revealer, GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
    gtk_revealer_set_reveal_child(app_data->history_revealer, app_data->history_panel_visible);

    GtkWidget *history_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(history_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(history_scroll, 250, -1);
    app_data->history_list_box = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(app_data->history_list_box, GTK_SELECTION_SINGLE);
    
    gtk_list_box_bind_model(app_data->history_list_box, G_LIST_MODEL(app_data->history_store), create_history_row, NULL, NULL);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(history_scroll), GTK_WIDGET(app_data->history_list_box));
    gtk_revealer_set_child(app_data->history_revealer, history_scroll);
    
    g_signal_connect(app_data->history_list_box, "row-activated", G_CALLBACK(history_load_selected_chat), app_data);

    GtkGesture *context_gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(context_gesture), GDK_BUTTON_SECONDARY);
    g_signal_connect(context_gesture, "pressed", G_CALLBACK(on_history_context_menu), app_data);
    gtk_widget_add_controller(GTK_WIDGET(app_data->history_list_box), GTK_EVENT_CONTROLLER(context_gesture));

    return GTK_WIDGET(app_data->history_revealer);
}
