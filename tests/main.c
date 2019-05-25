#include <stdio.h>
#include "../include/Program.h"
#include <quickmedia/HtmlSearch.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t size;
} Buffer;

static int program_output_callback(char *data, int size, void *userdata) {
    Buffer *buf = userdata;
    size_t new_size = buf->size + size;
    buf->data = realloc(buf->data, new_size + 1);
    buf->data[new_size] = '\0';
    memcpy(buf->data + buf->size, data, size);
    buf->size = new_size;
    return 0;
}

static void html_search_callback(QuickMediaHtmlNode *node, void *userdata) {
    const char *href = quickmedia_html_node_get_attribute_value(node, "href");
    QuickMediaStringView text = quickmedia_html_node_get_text(node);
    printf("a href: %s, text: %.*s\n", href, text.size, text.data);
}

int main(int argc, char **argv) {
    Buffer buf;
    buf.data = NULL;
    buf.size = 0;
    char *args[] = { "/usr/bin/curl", "-s", "-L", "https://manganelo.com/search/naruto", NULL };
    exec_program(args, program_output_callback, &buf);
    /*printf("%s\n", buf.data);*/

    QuickMediaHtmlSearch html_search;
    if(quickmedia_html_search_init(&html_search, buf.data) != 0)
        return -1;
    if(quickmedia_html_find_nodes_xpath(&html_search, "//h3[class=\"story_name\"]//a", html_search_callback, NULL) != 0)
        return -1;
    quickmedia_html_search_deinit(&html_search);

    free(buf.data);
    return 0;
}
