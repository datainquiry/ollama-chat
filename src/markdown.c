#include "markdown.h"
#include <string.h>
#include <glib.h>

char *markdown_to_pango(const char *markdown) {
    GString *pango = g_string_new("");
    const char *p = markdown;

    while (*p) {
        // Code block: ```lang\ncode\n```
        if (strncmp(p, "```", 3) == 0) {
            const char *end = strstr(p + 3, "```");
            if (end) {
                const char *code_start = p + 3;
                const char *first_newline = strchr(code_start, '\n');
                if (first_newline && first_newline < end) {
                    code_start = first_newline + 1;
                }

                char *code = g_strndup(code_start, end - code_start);
                char *escaped_code = g_markup_escape_text(code, -1);
                
                g_string_append(pango, "<tt>");
                g_string_append(pango, escaped_code);
                g_string_append(pango, "</tt>");
                
                g_free(code);
                g_free(escaped_code);
                p = end + 3;
                continue;
            }
        }

        // Bold and Italic
        if (strncmp(p, "**", 2) == 0) {
            const char *end = strstr(p + 2, "**");
            if (end) {
                char *text = g_strndup(p + 2, end - (p + 2));
                char *inner_pango = markdown_to_pango(text);
                g_string_append(pango, "<b>");
                g_string_append(pango, inner_pango);
                g_string_append(pango, "</b>");
                g_free(text);
                g_free(inner_pango);
                p = end + 2;
                continue;
            }
        }

        if (*p == '*') {
            const char *end = strchr(p + 1, '*');
            if (end) {
                char *text = g_strndup(p + 1, end - (p + 1));
                char *inner_pango = markdown_to_pango(text);
                g_string_append(pango, "<i>");
                g_string_append(pango, inner_pango);
                g_string_append(pango, "</i>");
                g_free(text);
                g_free(inner_pango);
                p = end + 1;
                continue;
            }
        }

        // Code: `text`
        if (*p == '`') {
            const char *end = strchr(p + 1, '`');
            if (end) {
                char *code = g_strndup(p + 1, end - (p + 1));
                char *escaped_code = g_markup_escape_text(code, -1);
                g_string_append(pango, "<tt>");
                g_string_append(pango, escaped_code);
                g_string_append(pango, "</tt>");
                g_free(code);
                g_free(escaped_code);
                p = end + 1;
                continue;
            }
        }

        // Escape XML special characters
        if (*p == '<') {
            g_string_append(pango, "&lt;");
        } else if (*p == '>') {
            g_string_append(pango, "&gt;");
        } else if (*p == '&') {
            g_string_append(pango, "&amp;");
        } else {
            g_string_append_c(pango, *p);
        }
        p++;
    }

    return g_string_free(pango, FALSE);
}