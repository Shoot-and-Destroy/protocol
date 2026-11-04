/*
 * Reading our server list: GET /v1/servers over HTTPS with libcurl.
 *
 * Prints one line per server. The JSON here is read with a deliberately
 * small scanner: enough for a list row, not a general parser. In a real
 * client use a JSON library.
 *
 * Build with `make master` (needs libcurl).
 *
 * MIT licence.
 */
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MASTER_URL "https://master.shootanddestroy.obex.pw/v1/servers"
#define LIST_MAX (8u * 1024u * 1024u)

typedef struct {
    char *data;
    size_t size;
    size_t cap;
    int too_big;
} buffer;

static size_t on_body(void *contents, size_t size, size_t nmemb, void *user)
{
    buffer *b = (buffer *) user;
    size_t n = size * nmemb;
    if (b->size + n + 1 > LIST_MAX) {
        b->too_big = 1;
        return 0; /* abort the transfer */
    }
    if (b->size + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 65536;
        while (cap < b->size + n + 1)
            cap *= 2;
        char *p = realloc(b->data, cap);
        if (p == NULL)
            return 0;
        b->data = p;
        b->cap = cap;
    }
    memcpy(b->data + b->size, contents, n);
    b->size += n;
    b->data[b->size] = '\0';
    return n;
}

/* decode four hex digits after \u and write UTF-8; returns bytes written */
static size_t put_unicode(const char *hex, char *out)
{
    unsigned cp = 0;
    for (int i = 0; i < 4; i++) {
        char c = hex[i];
        unsigned v;
        if (c >= '0' && c <= '9')
            v = (unsigned) (c - '0');
        else if (c >= 'a' && c <= 'f')
            v = (unsigned) (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            v = (unsigned) (c - 'A' + 10);
        else
            return 0;
        cp = cp << 4 | v;
    }
    if (cp < 0x80) {
        out[0] = (char) cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char) (0xC0 | (cp >> 6));
        out[1] = (char) (0x80 | (cp & 0x3F));
        return 2;
    }
    out[0] = (char) (0xE0 | (cp >> 12));
    out[1] = (char) (0x80 | ((cp >> 6) & 0x3F));
    out[2] = (char) (0x80 | (cp & 0x3F));
    return 3;
}

/* find "key": <value> inside `row` and copy the value.
   Handles a JSON string (with the common escapes) and a number/bool. */
static int json_field(const char *row, const char *key, char *out, size_t out_size)
{
    char pattern[64];
    snprintf(pattern, sizeof pattern, "\"%s\"", key);
    const char *p = strstr(row, pattern);
    if (p == NULL) {
        out[0] = '\0';
        return 0;
    }
    p += strlen(pattern);
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p != ':')
        return 0;
    p++;
    while (*p == ' ' || *p == '\t')
        p++;
    size_t at = 0;
    if (*p == '"') {
        p++;
        while (*p && *p != '"' && at + 1 < out_size) {
            if (*p == '\\' && p[1]) {
                p++;
                switch (*p) {
                case 'n': out[at++] = '\n'; break;
                case 't': out[at++] = '\t'; break;
                case 'r': out[at++] = '\r'; break;
                case '"': out[at++] = '"'; break;
                case '\\': out[at++] = '\\'; break;
                case 'u':
                    if (at + 4 < out_size) {
                        size_t k = put_unicode(p + 1, out + at);
                        if (k) {
                            at += k;
                            p += 4;
                        } else {
                            out[at++] = '?';
                        }
                    }
                    break;
                default: out[at++] = *p; break;
                }
                p++;
            } else {
                out[at++] = *p++;
            }
        }
    } else {
        while (*p && *p != ',' && *p != '}' && *p != ']' && at + 1 < out_size)
            out[at++] = *p++;
        while (at > 0 && (out[at - 1] == ' ' || out[at - 1] == '\n'))
            at--;
    }
    out[at] = '\0';
    return 1;
}

/* split the top-level array into row slices; returns the row count */
static int json_rows(const char *json, const char *rows[], int max_rows)
{
    int depth = 0, n = 0;
    int in_string = 0, escape = 0;
    const char *start = NULL;
    for (const char *p = json; *p; p++) {
        char c = *p;
        if (in_string) {
            if (escape)
                escape = 0;
            else if (c == '\\')
                escape = 1;
            else if (c == '"')
                in_string = 0;
            continue;
        }
        if (c == '"') {
            in_string = 1;
        } else if (c == '{') {
            if (depth == 1 && start == NULL)
                start = p;
            depth++;
        } else if (c == '}') {
            depth--;
            if (depth == 1 && start != NULL && n < max_rows) {
                rows[n++] = start;
                start = NULL;
            }
        } else if (c == '[') {
            depth++;
        } else if (c == ']') {
            depth--;
        }
    }
    return n;
}

/* the matching '}' of a row that starts at '{'; NULL when the JSON is cut */
static const char *json_row_end(const char *start)
{
    int depth = 0, in_string = 0, escape = 0;
    for (const char *p = start; *p; p++) {
        char c = *p;
        if (in_string) {
            if (escape)
                escape = 0;
            else if (c == '\\')
                escape = 1;
            else if (c == '"')
                in_string = 0;
            continue;
        }
        if (c == '"')
            in_string = 1;
        else if (c == '{')
            depth++;
        else if (c == '}') {
            depth--;
            if (depth == 0)
                return p;
        }
    }
    return NULL;
}

int main(void)
{
    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        fprintf(stderr, "curl_easy_init failed\n");
        return 1;
    }

    buffer body;
    memset(&body, 0, sizeof body);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, MASTER_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "protocol-example/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    /* a CA bundle: the system's on a desktop, or the one your build ships */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

    CURLcode rc = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    if (rc != CURLE_OK) {
        fprintf(stderr, "list unreachable: %s\n", curl_easy_strerror(rc));
    } else if (status != 200 || body.data == NULL) {
        fprintf(stderr, "list answered HTTP %ld\n", status);
    } else {
        const char *rows[512];
        int n = json_rows(body.data, rows, 512);
        printf("%d server(s)\n", n);
        for (int i = 0; i < n; i++) {
            char name[96], ident[256], map[64], mode[32], version[16], players[32], max[32];
            /* a row slice is not NUL terminated: copy it */
            char row[65536];
            const char *end = json_row_end(rows[i]);
            size_t len = end != NULL ? (size_t) (end - rows[i]) + 1 : strlen(rows[i]);
            if (len >= sizeof row)
                len = sizeof row - 1;
            memcpy(row, rows[i], len);
            row[len] = '\0';

            json_field(row, "name", name, sizeof name);
            json_field(row, "identifier", ident, sizeof ident);
            json_field(row, "map", map, sizeof map);
            json_field(row, "game_mode", mode, sizeof mode);
            json_field(row, "game_version", version, sizeof version);
            json_field(row, "players_current", players, sizeof players);
            json_field(row, "players_max", max, sizeof max);
            printf("%-24s %-32s %s/%s  %-10s %-6s %s\n", name, ident, players, max,
                   map, mode, version);
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(body.data);
    return rc == CURLE_OK ? 0 : 1;
}
