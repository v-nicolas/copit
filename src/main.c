/*
 * Copit: Save clipboard history.
 * Copyright (C) 2022 <nicolas.vilmain[at]gmail[dot]com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#define PVERSION "1.0"
#define DATE_SIZE 24

#define HISTORY_MIN 2
#define HISTORY_MAX 1024
#define HISTORY_DEFAULT_SIZE 64

#define WINDOW_TITLE "Copit"
#define WINDOW_DEFAULT_BG_COLOR "#edbb99"

#define EV_CLICKED "clicked"
#define EV_DESTROY "destroy"
#define EV_DESTROY_EVENT "delete-event"
#define EV_OWNER_CHANGE "owner-change"

enum copit_options {
    OPT_ICONIFY_DISABLE = (1 << 0),
};

struct history {
    char *content;
    char date[DATE_SIZE];
    GtkWidget *box;

    struct history *next;
    struct history *prev;
};

void copit_load(int argc, char *argv[]);
void window_set_config(void);
void window_set_size(GdkDisplay *display);
gboolean window_delete_event_callback(GtkWidget *w, GdkEvent *ev, gpointer data);
void copit_quit(GtkWidget *widget, gpointer data);
void delete_history_entry_callback(GtkWidget *button, gpointer data);
void pastit_callback(GtkWidget *button, gpointer data);
void copit_callback(GtkClipboard *clipboard, const gchar *u1, gpointer u2);
struct history *history_add(const char *content);
void dlist_add(struct history **dlist, struct history *new);
void history_show_new_entry(struct history *h);
struct history *history_have_entry(const char *content);
void history_delete(struct history *chunk);
void history_chunk_free(struct history *chunk);
void get_date(char *date);
void xfree(void *p);
void parse_program_options(int argc, char **argv);
void usage(void);
void version(void);

unsigned int options = 0;
int orientation = GTK_ORIENTATION_VERTICAL;
int hist_size = HISTORY_DEFAULT_SIZE;
const char *window_bg_color = WINDOW_DEFAULT_BG_COLOR;
const char *pname = NULL;
struct history *history = NULL;
GtkWidget *window = NULL;
GtkWidget *hist_box = NULL;
GtkClipboard* clipboard = NULL;

int
main(int argc, char *argv[])
{
    pname = argv[0];
    parse_program_options(argc, argv);
    copit_load(argc, argv);
    return EXIT_SUCCESS;
}

void
copit_load(int argc, char *argv[])
{
    GtkWidget *scrollbar = NULL;
    
    gtk_init(&argc, &argv);
    
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), WINDOW_TITLE);
    g_signal_connect(window, EV_DESTROY, G_CALLBACK(copit_quit), NULL);
    g_signal_connect(window, EV_DESTROY_EVENT,
		     G_CALLBACK(window_delete_event_callback), NULL);

    window_set_config();

    clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    g_signal_connect(clipboard, EV_OWNER_CHANGE,
		     G_CALLBACK(copit_callback),
		     NULL);

    scrollbar = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_hexpand(scrollbar, TRUE);
    gtk_widget_set_vexpand(scrollbar, TRUE);

    hist_box = gtk_box_new(orientation, 10);

    gtk_container_add(GTK_CONTAINER(window), scrollbar);
    gtk_container_add(GTK_CONTAINER(scrollbar), hist_box);

    gtk_widget_show_all(window);
    gtk_main();
}

void
window_set_config(void)
{
    GdkDisplay *display = NULL;
    
    display = gdk_display_get_default();
    window_set_size(display);
    
#if GTK_MAJOR_VERSION >= 3 && GTK_MINOR_VERSION >= 16
    GdkScreen *screen = NULL;
    GtkCssProvider *css_provider = NULL;
    GError *err = NULL;
    char css[1024];

    snprintf(css, sizeof(css),
	     "window {"
	     " background-color: %s; "
	     "}\n"
	     "button {"
	     " font-size: 10px; "
	     " border-width: 1px; "
	     " border-radius: 8px; "
	     "}", window_bg_color);

# ifndef NDEBUG
    printf("CSS:\n%s\n", css);
# endif /* !NDEBUG */

    screen = gdk_display_get_default_screen(display);
    css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_screen(screen,
					      GTK_STYLE_PROVIDER(css_provider),
					      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(css_provider),
				    css, -1, &err);

    if (err != NULL) {
	fprintf(stderr, "error: gtk_css_provider_load_from_data: %s\n",
		err->message);
	g_error_free(err);
	return;
    }
#else
    GdkRGBA color;
    
    if (gdk_rgba_parse(&color, WINDOW_BG_COLOR)) {
	gtk_widget_override_background_color(window, GTK_STATE_NORMAL, &color);
    }
#endif /* GTK_VERSION */
}

void
window_set_size(GdkDisplay *display)
{
    GdkMonitor *monitor = NULL;
    GdkRectangle area;

    monitor = gdk_display_get_primary_monitor(display);
    gdk_monitor_get_workarea(monitor, &area);

    if (orientation == GTK_ORIENTATION_VERTICAL) {
	gtk_window_resize(GTK_WINDOW(window), 200, area.height);
    } else {
	gtk_window_resize(GTK_WINDOW(window), area.width, 200);
    }
    
#ifndef NDEBUG
    printf("Screen width:%d, height:%d\n", area.width, area.height);
#endif /* !NDEBUG */
}

gboolean
window_delete_event_callback(GtkWidget *w,
			     GdkEvent *ev __attribute__((unused)),
			     gpointer data __attribute__((unused)))
{
    gint ret;
    GtkWidget *dialog = NULL;

    dialog = gtk_message_dialog_new(GTK_WINDOW(w),
				    GTK_DIALOG_DESTROY_WITH_PARENT,
				    GTK_MESSAGE_QUESTION,
				    GTK_BUTTONS_YES_NO,
				    "Exit program ?");
    ret = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    if (ret == GTK_RESPONSE_YES) {
	return FALSE;
    }
    
    return TRUE;
}

void
copit_quit(GtkWidget *widget __attribute__((unused)),
	   gpointer data __attribute__((unused)))
{
    gtk_main_quit();
    exit(EXIT_SUCCESS);
}

void
delete_history_entry_callback(GtkWidget *button __attribute__((unused)),
			      gpointer data)
{
#ifndef NDEBUG
    printf("Delete by button: %s\n", h->content);
#endif /* NDEBUG */
    
    history_delete(data);
    gtk_widget_show_all(hist_box);
}

void
pastit_callback(GtkWidget *button, gpointer data __attribute__((unused)))
{
    const gchar *label = NULL;
    struct history *h = NULL;
    
    label = gtk_button_get_label(GTK_BUTTON(button));
    gtk_clipboard_set_text(clipboard, label, -1);

    h = history_have_entry(label);
    if (h != NULL) {
	history_delete(h);
	history_add(label);
	gtk_widget_show_all(hist_box);
    }

    if (!(options & OPT_ICONIFY_DISABLE)){
	gtk_window_iconify(GTK_WINDOW(window));
    }
}

void
copit_callback(GtkClipboard *clipboardsrc,
	       const gchar *u1 __attribute__((unused)),
	       gpointer u2 __attribute__((unused)))
{
    char *text = NULL;
    
    text = gtk_clipboard_wait_for_text(clipboardsrc);
    if (text == NULL || *text == 0) {
	return;
    }

#ifndef NDEBUG
    printf("New copy: %s\n", text);
#endif /* !NDEBUG */

    if (history_have_entry(text) != NULL) {
	return;
    }

    (void) history_add(text);
}


struct history *
history_add(const char *content)
{
    int i;
    struct history *h = NULL;

    i = 1;
    for (h = history; h != NULL && i < hist_size; h = h->next) {
	i++;
    }
    
    if (h != NULL) {
#ifndef NDEBUG
	printf("Delete latest content = %s\n", h->content);
#endif /* !NDEBUG */
	
	h->prev->next = NULL;
	history_chunk_free(h);

    }

    h = malloc(sizeof(struct history));
    if (h == NULL) {
	return NULL;
    }
    
    h->content = strdup(content);
    if (h->content == NULL) {
	xfree(h);
	return NULL;
    }

    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
	get_date(h->date);
    }

    dlist_add(&history, h);
    history_show_new_entry(h);
    gtk_widget_show_all(hist_box);
    
    return h;
}

void
dlist_add(struct history **dlist, struct history *new)
{
    new->prev = NULL;
    new->next = NULL;
    
    if (*dlist != NULL) {
	new->next = history;
	(*dlist)->prev = new;
    }
    *dlist = new;
}

void
history_show_new_entry(struct history *h)
{
    GtkWidget *scrollbar = NULL;
    GtkWidget *label = NULL;
    GtkWidget *button_text = NULL;
    GtkWidget *button_delete = NULL;
    GtkWidget *box_util = NULL;
    
    scrollbar = gtk_scrolled_window_new(NULL, NULL);

    if (orientation == GTK_ORIENTATION_VERTICAL) {
	h->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	box_util = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_size_request(scrollbar, 140, 160);
    } else {
	h->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	box_util = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_widget_set_size_request(scrollbar, 200, 50);
    }

    gtk_box_pack_end(GTK_BOX(hist_box), h->box, 0, 0, 10);
    gtk_box_pack_start(GTK_BOX(h->box), scrollbar, 1, 1, 10);
    gtk_box_pack_start(GTK_BOX(h->box), box_util, 0, 0, 0);

    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
	label = gtk_label_new(h->date);
	gtk_box_pack_start(GTK_BOX(box_util), label, 0, 0, 10);
    }

#if GTK_MAJOR_VERSION >= 3 && GTK_MINOR_VERSION >= 10
    button_delete = gtk_button_new_from_icon_name("window-close-symbolic",
						  GTK_ICON_SIZE_SMALL_TOOLBAR);
#else
    button_delete = gtk_button_new_from_stock("gtk-delete");
#endif /* GTK_VERSION */

    g_signal_connect(button_delete, EV_CLICKED,
		     G_CALLBACK(delete_history_entry_callback),
		     h);
    gtk_box_pack_start(GTK_BOX(box_util), button_delete, 0, 0, 0);
    
    button_text = gtk_button_new_with_label(h->content);
    g_signal_connect(button_text, EV_CLICKED,
		     G_CALLBACK(pastit_callback),
		     NULL);
    gtk_container_add(GTK_CONTAINER(scrollbar), button_text);
}

struct history *
history_have_entry(const char *content)
{
    struct history *p = NULL;

    for (p = history; p; p = p->next) {
	if (!strcmp(p->content, content)) {
	    return p;
	}
    }
    return NULL;
}

void
history_delete(struct history *chunk)
{
    if (history == chunk) {
	if (history->next == NULL) {
	    history_chunk_free(chunk);
	    history = NULL;
	    return;
	}
	history = history->next;
	history->prev = NULL;
    } else {
	chunk->prev->next = chunk->next;
	if (chunk->next != NULL) {
	    chunk->next->prev = chunk->prev;
	}
    }

    history_chunk_free(chunk);
}

void
history_chunk_free(struct history *chunk)
{
    gtk_container_remove(GTK_CONTAINER(hist_box), chunk->box);
    xfree(chunk->content);
    xfree(chunk);
}

void
get_date(char *date)
{
    time_t timestamp;
    struct tm *tm = NULL;
    
    timestamp = time(NULL);
    tm = localtime(&timestamp );

    strftime(date, DATE_SIZE, "%d/%m/%Y %H:%M:%S", tm);
}

void
xfree(void *p)
{
    if (p) {
	free(p);
    }
}

void
parse_program_options(int argc, char **argv)
{
    int current_arg;
    char *err;
    static struct option const opt_index[] = {
        {"help",            no_argument,       NULL, 'h'},
        {"version",         no_argument,       NULL, 'v'},
        {"horizontal",      no_argument,       NULL, 'H'},
        {"vertical",        no_argument,       NULL, 'V'},
        {"iconify-disable", no_argument,       NULL, 'i'},
        {"history-size",    required_argument, NULL, 's'},
        {"bg-color    ",    required_argument, NULL, 'c'},
        {NULL,              0,                 NULL, 0},
    };
    
    do {
        current_arg = getopt_long(argc, argv, "hvHVis:c:", opt_index, NULL);
        switch(current_arg) {
        case 'h':
            usage();
            break;
        case 'v':
            version();
            break;
        case 'H':
	    orientation = GTK_ORIENTATION_HORIZONTAL;
            break;
        case 'V':
	    orientation = GTK_ORIENTATION_VERTICAL;
            break;
	case 'i':
	    options |= OPT_ICONIFY_DISABLE;
	    break;
        case 's':
	    hist_size = (int) strtol(optarg, &err, 10);
	    if (err && *err) {
		fprintf(stderr, "Invalid argument hist-size.\n");
		exit(EXIT_FAILURE);
	    }
	    break;
	case 'c':
	    window_bg_color = optarg;
	    break;
	default:
	    break;
	}
    } while (current_arg != -1);

    if (hist_size < HISTORY_MIN || hist_size > HISTORY_MAX) {
	fprintf(stderr, "error: Invalid history size `%d' (>=%d &&& <= %d)\n",
		hist_size, HISTORY_MIN, HISTORY_MAX);
	exit(EXIT_FAILURE);
    }

#ifndef NDEBUG
    printf("\rVertical: %d\n"
	   "\rHorizontal: %d\n"
	   "Background color: %s\n"
	   "Iconify: %d\n"
	   "History size: %d\n",
	   (orientation == GTK_ORIENTATION_VERTICAL),
	   (orientation == GTK_ORIENTATION_HORIZONTAL),
	   window_bg_color,
	   !(options & OPT_ICONIFY_DISABLE),
	   hist_size);
#endif /* !NDEBUG */
}

void __attribute__((noreturn))
usage(void)
{
    printf("%s usage: %s [OPTIONS]\n\n"
	   "Options list:\n"
	   "  -h, --help             : Show usage and exit.\n"
	   "  -v, --version          : Show version and exit.\n"
	   "\n"
	   "  -H, --horizontal       : Horizontal.\n"
	   "  -V, --vertical         : Vertical.\n"
	   "\n"
	   "  -i, --iconify-disable  : Disable iconify.\n"
	   "  -s, --history-size     : Set number entry in history.\n"
	   "  -c, --bg-color         : Set background color.\n"
	   "\n",
	   pname, pname);
    exit(EXIT_SUCCESS);
}

void __attribute__((noreturn))
version(void)
{
    printf("%s version %s\n", pname, PVERSION);
    exit(EXIT_SUCCESS);
}
