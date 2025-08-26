#include "markdown.h"
#include <string.h>
#include <glib.h>

char *markdown_to_pango(const char *markdown) {
    GString *pango = g_string_new("");
    const char *p = markdown;
    GList *stack = NULL;

    while (*p) {
        // Bold: **text** -> <b>text</b>
        if (*p == '*' && *(p + 1) == '*') {
            if (g_list_find(stack, "<b>")) {
                g_string_append(pango, "</b>");
                stack = g_list_remove(stack, "<b>");
            } else {
                g_string_append(pango, "<b>");
                stack = g_list_prepend(stack, "<b>");
            }
            p += 2;
            continue;
        }

        // Italic: *text* -> <i>text</i>
        if (*p == '*') {
            if (g_list_find(stack, "<i>")) {
                g_string_append(pango, "</i>");
                stack = g_list_remove(stack, "<i>");
            } else {
                g_string_append(pango, "<i>");
                stack = g_list_prepend(stack, "<i>");
            }
            p++;
            continue;
        }

        // Code: `text` -> <tt>text</tt>
        if (*p == '`') {
            if (g_list_find(stack, "<tt>")) {
                g_string_append(pango, "</tt>");
                stack = g_list_remove(stack, "<tt>");
            } else {
                g_string_append(pango, "<tt>");
                stack = g_list_prepend(stack, "<tt>");
            }
            p++;
            continue;
        }

        // Escape XML special characters
        if (g_list_find(stack, "<tt>")) {
            char *escaped = g_markup_escape_text(p, 1);
            g_string_append(pango, escaped);
            g_free(escaped);
        } else {
            if (*p == '<') {
                g_string_append(pango, "&lt;");
            } else if (*p == '>') {
                g_string_append(pango, "&gt;");
            } else if (*p == '&') {
                g_string_append(pango, "&amp;");
            } else {
                g_string_append_c(pango, *p);
            }
        }
        p++;
    }

    // Close any open tags
    for (GList *l = stack; l != NULL; l = l->next) {
        if (strcmp(l->data, "<b>") == 0) {
            g_string_append(pango, "</b>");
        } else if (strcmp(l->data, "<i>") == 0) {
            g_string_append(pango, "</i>");
        } else if (strcmp(l->data, "<tt>") == 0) {
            g_string_append(pango, "</tt>");
        }
    }
    g_list_free(stack);

    return g_string_free(pango, FALSE);
}
