#include <gtk/gtk.h>
#include <glib.h>
#include <stdio.h>

static GtkWidget *window = NULL;
static GtkWidget *search_entry = NULL;
static GtkWidget *list = NULL;
// TODO: Optimize this. There shouldn't be a need to copy the whole buffer everytime it's modified
static gchar *search_text = NULL;

static GtkLabel* get_list_item_title(GtkListBoxRow *row) {
    GtkWidget *child_widget = GTK_WIDGET(gtk_container_get_children(GTK_CONTAINER(row))->data);
    return GTK_LABEL(gtk_grid_get_child_at(GTK_GRID(child_widget), 0, 0));
}

static gboolean focus_out_callback(GtkWidget *widget, GdkEvent *event, gpointer userdata) {
    gtk_widget_destroy(window);
    return FALSE;
}

static gboolean filter_func(GtkListBoxRow *row, gpointer userdata) {
    GtkLabel *row_title_label = get_list_item_title(row);
    return !search_text || strlen(search_text) == 0 || strstr(gtk_label_get_text(row_title_label), search_text) != NULL;
}

static void list_move_select(GtkListBox *list, gint direction) {
    GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(list));
    if(!row)
        return;

    gint current_row_index = gtk_list_box_row_get_index(row);
    GtkListBoxRow *new_row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(list), current_row_index + direction);
    if(new_row)
        gtk_list_box_select_row(GTK_LIST_BOX(list), new_row);
}

static gboolean keypress_callback(GtkWidget *widget, GdkEventKey *event, gpointer userdata) {
    if(event->keyval == GDK_KEY_BackSpace) {
        gtk_editable_delete_text(GTK_EDITABLE(search_entry), 0, -1);
    } else if(event->keyval == GDK_KEY_Return) {
        GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(list));
        if(row) {
            GtkLabel *row_title_label = get_list_item_title(row);
            puts(gtk_label_get_text(row_title_label));
        }
        gtk_widget_destroy(window);
        return FALSE;
    } else if(event->keyval == GDK_KEY_Up) {
        list_move_select(GTK_LIST_BOX(list), -1);
        return FALSE;
    } else if(event->keyval == GDK_KEY_Down) {
        list_move_select(GTK_LIST_BOX(list), 1);
        return FALSE;
    } else {
        gint position = -1;
        gtk_editable_insert_text(GTK_EDITABLE(search_entry), event->string, -1, &position);
        printf("key press %d\n", event->keyval);
    }

    g_free(search_text);
    search_text = gtk_editable_get_chars(GTK_EDITABLE(search_entry), 0, -1);
    gtk_list_box_invalidate_filter(GTK_LIST_BOX(list));
    return TRUE;
}

static GtkWidget* create_entry(const char *text, const char *description) {
    GtkWidget *entry = gtk_grid_new();
    GtkWidget *label_widget = gtk_label_new(text);
    gtk_grid_attach(GTK_GRID(entry), label_widget, 0, 0, 1, 1);
    GtkWidget *description_widget = gtk_label_new(description);
    gtk_grid_attach(GTK_GRID(entry), description_widget, 1, 0, 1, 1);
    return entry;
}

static void activate(GtkApplication *app, gpointer userdata) {
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "QuickMedia");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 300);

    GtkWidget *grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(window), grid);

    search_entry = gtk_search_entry_new();
    gtk_widget_set_sensitive(search_entry, FALSE);
    GdkRGBA text_color;
    gdk_rgba_parse(&text_color, "black");
    gtk_widget_override_color(search_entry, GTK_STATE_FLAG_INSENSITIVE, &text_color);
    gtk_widget_set_hexpand(search_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), search_entry, 0, 0, 1, 1);

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_grid_attach(GTK_GRID(grid), scrolled_window, 0, 1, 1, 1);

    list = gtk_list_box_new();
    gtk_widget_set_hexpand(list, TRUE);
    gtk_widget_set_vexpand(list, TRUE);
    gtk_list_box_set_filter_func(GTK_LIST_BOX(list), filter_func, NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled_window), list);

    for(int i = 0; i < 100; ++i) {
        GtkWidget *item = create_entry("hello, world!", "asdas");
        gtk_list_box_insert(GTK_LIST_BOX(list), item, i);
    }

    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ALWAYS);
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_widget_show_all(window);


    gtk_widget_add_events(window, GDK_KEY_PRESS_MASK | GDK_FOCUS_CHANGE_MASK);
    g_signal_connect(G_OBJECT(window), "key-press-event", G_CALLBACK(keypress_callback), NULL);
    g_signal_connect(G_OBJECT(window), "focus-out-event", G_CALLBACK(focus_out_callback), NULL);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("org.dec05eba.quickmedia", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}

