#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <glib.h>
#include "web_search.h"

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

// Helper function to extract substring between two delimiters
static char *extract_substring(const char *source, const char *start_delim, const char *end_delim) {
    const char *start = strstr(source, start_delim);
    if (!start) return NULL;
    start += strlen(start_delim);

    const char *end = strstr(start, end_delim);
    if (!end) return NULL;

    size_t length = end - start;
    char *substring = g_malloc(length + 1);
    strncpy(substring, start, length);
    substring[length] = '\0';
    return substring;
}

char *perform_web_search(const char *query) {
    CURL *curl_handle;
    CURLcode res;

    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();

    char *encoded_query = curl_easy_escape(curl_handle, query, 0);
    char url[512];
    snprintf(url, sizeof(url), "https://html.duckduckgo.com/html/?q=%s", encoded_query);
    curl_free(encoded_query);

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36");

    res = curl_easy_perform(curl_handle);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        free(chunk.memory);
        curl_easy_cleanup(curl_handle);
        curl_global_cleanup();
        return NULL;
    }

    GString *result_gstring = g_string_new("");
    const char *html_body = chunk.memory;
    int count = 0;

    // Find the container for all results
    const char *results_container = strstr(html_body, "<div id=\"links\"");
    if (results_container) {
        html_body = results_container;
    }

    while (count < 3 && (html_body = strstr(html_body, "result__title"))) {
        char *title = extract_substring(html_body, "result__a\">", "</a>");
        char *url_part = extract_substring(html_body, "href=\"", "\">");
        char *snippet = extract_substring(html_body, "result__snippet\">", "</a>");

        if (title && url_part && snippet) {
            char *decoded_url = curl_easy_unescape(curl_handle, url_part, 0, NULL);
            if (decoded_url) {
                // The URL from DDG is a redirect, let's clean it up.
                char *actual_url = strstr(decoded_url, "uddg=");
                if (actual_url) {
                    actual_url += 5; // Move past "uddg="
                    char *end_url = strstr(actual_url, "&");
                    if (end_url) {
                        *end_url = '\0';
                    }
                    // URL Decode the final URL
                    char *final_url = curl_easy_unescape(curl_handle, actual_url, 0, NULL);
                    g_string_append_printf(result_gstring, "Title: %s\nURL: %s\nSnippet: %s\n\n", title, final_url, snippet);
                    curl_free(final_url);
                } else {
                    g_string_append_printf(result_gstring, "Title: %s\nURL: %s\nSnippet: %s\n\n", title, decoded_url, snippet);
                }
                count++;
            }
            curl_free(decoded_url);
        }

        g_free(title);
        g_free(url_part);
        g_free(snippet);

        html_body++; // Move past the current result to find the next one
    }

    curl_easy_cleanup(curl_handle);
    free(chunk.memory);
    curl_global_cleanup();

    if (result_gstring->len > 0) {
        return g_string_free(result_gstring, FALSE);
    } else {
        g_string_free(result_gstring, TRUE);
        return strdup("No results found.");
    }
}

// Function to strip HTML tags from a string (simple version)
static char *strip_html(const char *html) {
    GString *text = g_string_new("");
    gboolean in_tag = FALSE;
    for (int i = 0; html[i] != '\0'; i++) {
        if (html[i] == '<') {
            in_tag = TRUE;
        } else if (html[i] == '>') {
            in_tag = FALSE;
        } else if (!in_tag) {
            g_string_append_c(text, html[i]);
        }
    }
    return g_string_free(text, FALSE);
}

char *fetch_url_content(const char *url) {
    CURL *curl_handle;
    CURLcode res;
    struct MemoryStruct chunk;

    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36");
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects

    res = curl_easy_perform(curl_handle);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        free(chunk.memory);
        curl_easy_cleanup(curl_handle);
        curl_global_cleanup();
        return NULL;
    }

    char *text_content = strip_html(chunk.memory);

    curl_easy_cleanup(curl_handle);
    free(chunk.memory);
    curl_global_cleanup();

    return text_content;
}

char *find_url(const char *text) {
    GRegex *regex;
    GMatchInfo *match_info;
    char *url = NULL;

    // Regex to find URLs
    regex = g_regex_new("(https?://[\\w\\d\\.\\-/:\\?=&%#]+)", 0, 0, NULL);
    if (g_regex_match(regex, text, 0, &match_info)) {
        url = g_match_info_fetch(match_info, 0);
    }

    if (match_info) {
        g_match_info_free(match_info);
    }
    if (regex) {
        g_regex_unref(regex);
    }

    return url;
}