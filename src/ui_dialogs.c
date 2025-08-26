#include "ui_dialogs.h"
#include "config.h"
#include "history.h"

typedef struct {
    GtkSpinButton *temperature_spin;
    GtkSpinButton *top_p_spin;
    GtkSpinButton *top_k_spin;
    GtkSpinButton *seed_spin;
    GtkSpinButton *context_length_spin;
    GtkTextView *system_prompt_view;
    AppData *app_data;
} PrefsWidgets;

static void on_prefs_dialog_response(GtkDialog *dialog, int response_id, gpointer user_data) {
    PrefsWidgets *prefs_widgets = (PrefsWidgets *)user_data;
    if (response_id == GTK_RESPONSE_ACCEPT) {
        AppData *app_data = prefs_widgets->app_data;
        app_data->temperature = gtk_spin_button_get_value(prefs_widgets->temperature_spin);
        app_data->top_p = gtk_spin_button_get_value(prefs_widgets->top_p_spin);
        app_data->top_k = (int)gtk_spin_button_get_value(prefs_widgets->top_k_spin);
        app_data->seed = (int)gtk_spin_button_get_value(prefs_widgets->seed_spin);
        app_data->ollama_context_size = (int)gtk_spin_button_get_value(prefs_widgets->context_length_spin);

        GtkTextBuffer *buffer = gtk_text_view_get_buffer(prefs_widgets->system_prompt_view);
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        char *system_prompt_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
        if (app_data->system_prompt) g_free(app_data->system_prompt);
        app_data->system_prompt = g_strdup(system_prompt_text);
        g_free(system_prompt_text);

        config_save(app_data);
    }
    g_free(prefs_widgets);
    gtk_window_destroy(GTK_WINDOW(dialog));
}

void show_preferences_dialog(AppData *app_data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Preferences",
                                                    GTK_WINDOW(app_data->window),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    "_Save", GTK_RESPONSE_ACCEPT,
                                                    NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *grid = gtk_grid_new();
    gtk_widget_set_margin_start(grid, 12);
    gtk_widget_set_margin_end(grid, 12);
    gtk_widget_set_margin_top(grid, 12);
    gtk_widget_set_margin_bottom(grid, 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_box_append(GTK_BOX(content_area), grid);

    PrefsWidgets *prefs_widgets = g_malloc(sizeof(PrefsWidgets));
    prefs_widgets->app_data = app_data;

    int row = 0;

    // Temperature
    GtkWidget *temp_label = gtk_label_new("Temperature:");
    gtk_widget_set_halign(temp_label, GTK_ALIGN_START);
    prefs_widgets->temperature_spin = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0.0, 2.0, 0.1));
    gtk_spin_button_set_value(prefs_widgets->temperature_spin, app_data->temperature);
    gtk_grid_attach(GTK_GRID(grid), temp_label, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(prefs_widgets->temperature_spin), 1, row++, 1, 1);

    // Top P
    GtkWidget *top_p_label = gtk_label_new("Top P:");
    gtk_widget_set_halign(top_p_label, GTK_ALIGN_START);
    prefs_widgets->top_p_spin = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0.0, 1.0, 0.1));
    gtk_spin_button_set_value(prefs_widgets->top_p_spin, app_data->top_p);
    gtk_grid_attach(GTK_GRID(grid), top_p_label, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(prefs_widgets->top_p_spin), 1, row++, 1, 1);

    // Top K
    GtkWidget *top_k_label = gtk_label_new("Top K:");
    gtk_widget_set_halign(top_k_label, GTK_ALIGN_START);
    prefs_widgets->top_k_spin = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0, 100, 1));
    gtk_spin_button_set_value(prefs_widgets->top_k_spin, app_data->top_k);
    gtk_grid_attach(GTK_GRID(grid), top_k_label, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(prefs_widgets->top_k_spin), 1, row++, 1, 1);

    // Seed
    GtkWidget *seed_label = gtk_label_new("Seed (0 for random):");
    gtk_widget_set_halign(seed_label, GTK_ALIGN_START);
    prefs_widgets->seed_spin = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0, G_MAXINT, 1));
    gtk_spin_button_set_value(prefs_widgets->seed_spin, app_data->seed);
    gtk_grid_attach(GTK_GRID(grid), seed_label, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(prefs_widgets->seed_spin), 1, row++, 1, 1);

    // Context Length
    GtkWidget *context_label = gtk_label_new("Context Length:");
    gtk_widget_set_halign(context_label, GTK_ALIGN_START);
    prefs_widgets->context_length_spin = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0, 16384, 1024));
    gtk_spin_button_set_value(prefs_widgets->context_length_spin, app_data->ollama_context_size);
    gtk_grid_attach(GTK_GRID(grid), context_label, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(prefs_widgets->context_length_spin), 1, row++, 1, 1);

    // System Prompt
    GtkWidget *system_prompt_label = gtk_label_new("System Prompt:");
    gtk_widget_set_halign(system_prompt_label, GTK_ALIGN_START);
    gtk_widget_set_valign(system_prompt_label, GTK_ALIGN_START);
    prefs_widgets->system_prompt_view = GTK_TEXT_VIEW(gtk_text_view_new());
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(prefs_widgets->system_prompt_view);
    gtk_text_buffer_set_text(buffer, app_data->system_prompt ? app_data->system_prompt : "", -1);
    gtk_text_view_set_wrap_mode(prefs_widgets->system_prompt_view, GTK_WRAP_WORD_CHAR);
    GtkWidget *scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), GTK_WIDGET(prefs_widgets->system_prompt_view));
    gtk_widget_set_size_request(scrolled_window, -1, 100);
    gtk_grid_attach(GTK_GRID(grid), system_prompt_label, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), scrolled_window, 1, row++, 1, 1);

    g_signal_connect(dialog, "response", G_CALLBACK(on_prefs_dialog_response), prefs_widgets);
    gtk_window_present(GTK_WINDOW(dialog));
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

void show_rename_dialog(AppData *app_data, const char *old_name) {
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

void show_about_dialog(AppData *app_data) {
    const char *authors[] = {"Data Inquiry Consulting LLC", NULL};

    gtk_show_about_dialog(GTK_WINDOW(app_data->window),
                          "program-name", "Ollama Chat",
                          "version", "1.0.0",
                          "copyright", "© 2025, Data Inquiry Consulting LLC",
                          "authors", authors,
                          "logo-icon-name", "com.example.ollama-chat",
                          NULL);
}
