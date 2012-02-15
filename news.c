/**
 * kalu - Copyright (C) 2012 Olivier Brunel
 *
 * news.c
 * Copyright (C) 2012 Olivier Brunel <i.am.jack.mail@gmail.com>
 * 
 * This file is part of kalu.
 *
 * kalu is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * kalu is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * kalu. If not, see http://www.gnu.org/licenses/
 */

/* C */
#include <string.h>

/* gtk */
#include <gtk/gtk.h>

/* glib */
#include <glib.h>

/* kalu */
#include "kalu.h"
#include "news.h"
#include "curl.h"
#include "util.h"
#include "util-gtk.h"

#define NEWS_RSS_URL        "http://www.archlinux.org/feeds/news/"

typedef struct _parse_updates_data_t {
    gboolean     is_last_reached;
    alpm_list_t *titles;
} parse_updates_data_t;

typedef struct _parse_news_data_t {
    gboolean         only_updates;
    GtkTextView     *textview;
    GtkTextBuffer   *buffer;
    PangoAttrList   *attr_list;
    
    gboolean         is_last_reached;
    alpm_list_t    **lists;
} parse_news_data_t;

enum {
    LIST_TITLES_ALL = 0,
    LIST_TITLES_SHOWN,
    LIST_TITLES_READ,
    NB_LISTS
};

static void
xml_parser_updates_text (GMarkupParseContext   *context,
                         const gchar           *text,
                         gsize                  text_len _UNUSED_,
                         parse_updates_data_t  *parse_updates_data,
                         GError               **error _UNUSED_)
{
    const GSList         *list;
    alpm_list_t          *i;
    
    /* have we already reached the last unread item */
    if (parse_updates_data->is_last_reached)
    {
        return;
    }
    
    /* is this a tag (title, description, ...) inside an item? */
    list = g_markup_parse_context_get_element_stack (context);
    if (!list->next || strcmp ("item", list->next->data) != 0)
    {
        return;
    }
    
    if (strcmp ("title", list->data) == 0)
    {
        /* is this the last item from last check? */
        if (NULL != config->news_last && strcmp (config->news_last, text) == 0)
        {
            parse_updates_data->is_last_reached = TRUE;
            return;
        }
        
        /* was this item already read? */
        for (i = config->news_read; i; i = alpm_list_next (i))
        {
            if (strcmp (i->data, text) == 0)
            {
                return;
            }
        }
        
        /* add title to the new news */
        parse_updates_data->titles = alpm_list_add (parse_updates_data->titles,
            strdup (text));
    }
}

static void
title_toggled_cb (GtkToggleButton *button, alpm_list_t **lists)
{
    gchar *title = g_object_get_data (G_OBJECT (button), "title");
    gboolean is_active;
    
    g_object_get (G_OBJECT (button), "active", &is_active, NULL);
    if (is_active)
    {
        lists[LIST_TITLES_READ] = alpm_list_add (lists[LIST_TITLES_READ], title);
    }
    else
    {
        lists[LIST_TITLES_READ] = alpm_list_remove_str (lists[LIST_TITLES_READ],
            title, NULL);
    }
}

#define insert_text_with_tags() do {                                            \
        gtk_text_buffer_move_mark (buffer, mark, &iter);                        \
        gtk_text_buffer_insert (buffer, &iter, ss, -1);                         \
        gtk_text_buffer_get_iter_at_mark (buffer, &iter2, mark);                \
        for (i = tags; i; i = alpm_list_next (i))                               \
        {                                                                       \
            gtk_text_buffer_apply_tag_by_name (buffer, i->data, &iter, &iter2); \
        }                                                                       \
    } while (0)
static void
xml_parser_news_text (GMarkupParseContext *context,
                      const gchar         *text,
                      gsize                text_len,
                      parse_news_data_t   *parse_news_data,
                      GError             **error _UNUSED_)
{
    GtkTextBuffer   *buffer = parse_news_data->buffer;
    GtkTextIter     iter, iter2;
    GtkTextMark     *mark;
    gchar           *s, *ss, *start, *end;
    const GSList    *list;
    alpm_list_t     *i, *tags = NULL;
    gboolean        is_title = FALSE;
    static gboolean skip_next_description = FALSE;
    alpm_list_t   **lists;
    
    /* have we already reached the last unread item */
    if (parse_news_data->only_updates && parse_news_data->is_last_reached)
    {
        return;
    }
    
    /* is this a tag (title, description, ...) inside an item? */
    list = g_markup_parse_context_get_element_stack (context);
    if (!list->next || strcmp ("item", list->next->data) != 0)
    {
        return;
    }
    
    /* gather whether this is title or description, don't go any further if it's
     * anything else*/
    if (strcmp ("title", list->data) == 0)
    {
        is_title = TRUE;
    }
    else if (strcmp ("description", list->data) != 0)
    {
        return;
    }
    
    if (parse_news_data->only_updates)
    {
        if (is_title)
        {
            /* make a copy of the title, and store it in list of all titles */
            /* it will not be free-d here. this is done on window_destroy_cb */
            s = strdup (text);
            lists = parse_news_data->lists;
            lists[LIST_TITLES_ALL] = alpm_list_add (lists[LIST_TITLES_ALL], s);
            
            /* is this the last item from last check? */
            if (NULL != config->news_last && strcmp (config->news_last, text) == 0)
            {
                parse_news_data->is_last_reached = TRUE;
                return;
            }
            
            /* was this item already read? */
            for (i = config->news_read; i; i = alpm_list_next (i))
            {
                if (strcmp (i->data, text) == 0)
                {
                    /* make a note to skip its description as well */
                    skip_next_description = TRUE;
                    return;
                }
            }
        }
        else if (skip_next_description)
        {
            skip_next_description = FALSE;
            return;
        }
    }
    
    if (is_title)
    {
        /* add a LF */
        gtk_text_buffer_get_end_iter (buffer, &iter);
        gtk_text_buffer_insert (buffer, &iter, "\n", -1);
        
        if (parse_news_data->only_updates)
        {
            GtkTextChildAnchor *anchor;
            GtkWidget *check, *label;
            
            /* store title in list of shown titles */
            lists[LIST_TITLES_SHOWN] = alpm_list_add (lists[LIST_TITLES_SHOWN], s);
            
            /* add a widget to check if the news should be marked read */
            anchor = gtk_text_buffer_create_child_anchor(buffer, &iter);
            check = gtk_check_button_new ();
            /* we set as data the title, same as in the lists above. it will be
             * used on toggled callback to be added to/removed from
             * lists[LIST_TITLES_READ] */
            g_object_set_data (G_OBJECT (check), "title", s);
            g_signal_connect (G_OBJECT (check), "toggled",
                              G_CALLBACK (title_toggled_cb), lists);
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);
            gtk_widget_show (check);
            label = gtk_label_new (text);
            gtk_label_set_attributes (GTK_LABEL (label), parse_news_data->attr_list);
            gtk_container_add (GTK_CONTAINER (check), label);
            gtk_widget_show (label);
            gtk_text_view_add_child_at_anchor (parse_news_data->textview, check, anchor);
        }
        else
        {
            gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, text, -1, "title", NULL);
        }
        gtk_text_buffer_insert (buffer, &iter, "\n", -1);
    }
    else
    {
        s = malloc ((text_len + 2) * sizeof (gchar));
        snprintf (s, text_len + 1, "%s", text);
        
        /* \n replaced by space */
        while ((ss = strchr (s, '\n')))
        {
            *ss = ' ';
        }
        
        gtk_text_buffer_get_end_iter (buffer, &iter);
        mark = gtk_text_buffer_create_mark (buffer, "mark", &iter, TRUE);
        
        ss = s;
        while ((start = strchr (ss, '<')))
        {
            if (NULL == (end = strchr (start, '>')))
            {
                break;
            }
            *end = '\0';
            if (strcmp (start + 1, "p") == 0)
            {
                *start = '\0';
                insert_text_with_tags ();
                gtk_text_buffer_insert (buffer, &iter, "\n", -1);
                ss = end + 1;
            }
            else if (strcmp (start + 1, "/p") == 0)
            {
                *start = '\0';
                insert_text_with_tags ();
                gtk_text_buffer_insert (buffer, &iter, "\n", -1);
                ss = end + 1;
            }
            else if (strcmp (start + 1, "b") == 0)
            {
                *start = '\0';
                insert_text_with_tags ();
                tags = alpm_list_add (tags, (void *) "bold");
                ss = end + 1;
            }
            else if (strcmp (start + 1, "/b") == 0)
            {
                *start = '\0';
                insert_text_with_tags ();
                tags = alpm_list_remove_str (tags, "bold", NULL);
                ss = end + 1;
            }
            else if (strcmp (start + 1, "code") == 0)
            {
                *start = '\0';
                insert_text_with_tags ();
                tags = alpm_list_add (tags, (void *) "code");
                ss = end + 1;
            }
            else if (strcmp (start + 1, "/code") == 0)
            {
                *start = '\0';
                insert_text_with_tags ();
                tags = alpm_list_remove_str (tags, "code", NULL);
                ss = end + 1;
            }
            else if (strcmp (start + 1, "pre") == 0)
            {
                *start = '\0';
                insert_text_with_tags ();
                tags = alpm_list_add (tags, (void *) "pre");
                ss = end + 1;
            }
            else if (strcmp (start + 1, "/pre") == 0)
            {
                *start = '\0';
                insert_text_with_tags ();
                tags = alpm_list_remove_str (tags, "pre", NULL);
                ss = end + 1;
            }
            else
            {
                /* unknown tag - just skip it */
                *start = '\0';
                insert_text_with_tags ();
                ss = end + 1;
            }
        }
        insert_text_with_tags ();
        
        gtk_text_buffer_delete_mark (buffer, mark);
        free (s);
    }
}
#undef insert_text_with_tags

static gboolean
parse_xml (gchar *xml, gboolean for_updates, gpointer data_out, GError **error)
{
    GMarkupParseContext *context;
    GMarkupParser        parser;
    GError              *local_err = NULL;

    
    
    memset (&parser, 0, sizeof (GMarkupParser));
    if (for_updates)
    {
        parser.text = xml_parser_updates_text;
    }
    else
    {
        parser.text = xml_parser_news_text;
        parse_news_data_t *data = data_out;
        GtkTextBuffer *buffer = data->buffer;
        
        /* create tags */
        GdkRGBA color;
        
        gdk_rgba_parse (&color, "rgb(0,119,187)");
        gtk_text_buffer_create_tag (buffer, "title",
            "size-points",      10.0,
            "weight",           800,
            "foreground-rgba",  &color,
            NULL);
        
        gtk_text_buffer_create_tag (buffer, "bold",
            "weight",           800,
            NULL);
        
        gdk_rgba_parse (&color, "rgb(255,255,221)");
        gtk_text_buffer_create_tag (buffer, "code",
            "family",           "Monospace",
            "background-rgba",  &color,
            NULL);
        
        gdk_rgba_parse (&color, "rgb(221,255,221)");
        gtk_text_buffer_create_tag (buffer, "pre",
            "family",           "Monospace",
            "background-rgba",  &color,
            NULL);
        
        if (data->only_updates)
        {
            /* create a attribute list, for labels of check-titles */
            PangoAttribute *attr;
            
            data->attr_list = pango_attr_list_new ();
            attr = pango_attr_weight_new (800);
            pango_attr_list_insert (data->attr_list, attr);
            attr = pango_attr_size_new (10 * PANGO_SCALE);
            pango_attr_list_insert (data->attr_list, attr);
            attr = pango_attr_foreground_new (0, 30583, 48059);
            pango_attr_list_insert (data->attr_list, attr);
        }

    }
    context = g_markup_parse_context_new (&parser, G_MARKUP_TREAT_CDATA_AS_TEXT,
        data_out, NULL);
    
    if (!g_markup_parse_context_parse (context, xml, (gssize) strlen (xml), &local_err))
    {
        g_markup_parse_context_free (context);
        if (!for_updates && ((parse_news_data_t *)data_out)->only_updates)
        {
            pango_attr_list_unref (((parse_news_data_t *)data_out)->attr_list);
        }
        g_propagate_error (error, local_err);
        return FALSE;
    }
    g_markup_parse_context_free (context);
    if (!for_updates && ((parse_news_data_t *)data_out)->only_updates)
    {
        pango_attr_list_unref (((parse_news_data_t *)data_out)->attr_list);
    }
    return TRUE;
}

static void
btn_close_cb (GtkWidget *button _UNUSED_, GtkWidget *window)
{
    gtk_widget_destroy (window);
}

static void
btn_mark_cb (GtkWidget *button _UNUSED_, GtkWidget *window)
{
    alpm_list_t **lists, *titles_all, *titles_shown, *titles_read, *i;
    alpm_list_t *news_read = NULL;
    char *news_last = NULL;
    gboolean is_last_set = FALSE;
    int nb_unread = 0;
    
    gtk_widget_hide (window);
    
    lists = g_object_get_data (G_OBJECT (window), "lists");
    /* reverse this one, to start with the oldest news */
    titles_all = alpm_list_reverse (lists[LIST_TITLES_ALL]);
    titles_shown = lists[LIST_TITLES_SHOWN];
    titles_read = lists[LIST_TITLES_READ];
    
    for (i = titles_all; i; i = alpm_list_next (i))
    {
        void *shown = alpm_list_find_ptr (titles_shown, i->data);
        
        /* was this news not shown, or shown and mark read? */
        if (!shown || (shown && alpm_list_find_ptr (titles_read, i->data)))
        {
            /* was last already set? */
            if (is_last_set)
            {
                /* then we add it to read */
                debug ("read:%s", (char*)i->data);
                news_read = alpm_list_add (news_read, strdup (i->data));
            }
            else
            {
                /* set the new last */
                if (news_last)
                {
                    free (news_last);
                }
                debug ("last=%s", (char*)i->data);
                news_last = strdup (i->data);
            }
        }
        /* was it shown? i.e. stays unread */
        else if (shown)
        {
            ++nb_unread;
            /* item was not read, so we can simply say that now last is set.
             * either it has been set/updated before, or it remains unchanged */
            is_last_set = TRUE;
        }
    }
    
    /* we only free this like so, because everything else (including the data
     * in titles) will be free-d when destroying the window */
    alpm_list_free (titles_all);
    
    /* save */
    FILE *fp;
    char file[MAX_PATH];
    gboolean saved = FALSE;
    
    snprintf (file, MAX_PATH - 1, "%s/.config/kalu/news.conf", g_get_home_dir ());
    if (ensure_path (file))
    {
        fp = fopen (file, "w");
        if (fp != NULL)
        {
            fputs ("Last=", fp);
            fputs (news_last, fp);
            fputs ("\n", fp);
            
            for (i = news_read; i; i = alpm_list_next (i))
            {
                fputs ("Read=", fp);
                fputs ((const char *) i->data, fp);
                fputs ("\n", fp);
            }
            fclose (fp);
            
            /* update */
            if (config->news_last)
            {
                free (config->news_last);
            }
            config->news_last = news_last;
            
            FREELIST (config->news_read);
            config->news_read = news_read;
            
            set_kalpm_nb (CHECK_NEWS, nb_unread);
            saved = TRUE;
        }
    }
    
    if (saved)
    {
        gtk_widget_destroy (window);
    }
    else
    {
        gtk_widget_show (window);
        show_error ("Unable to save changes to disk", file, GTK_WINDOW (window));
    }
}

static void
window_destroy_cb (GtkWidget *window, gpointer data _UNUSED_)
{
    alpm_list_t **lists;
    int i;
    
    /* will be present of this was a only_updates window */
    lists = g_object_get_data (G_OBJECT (window), "lists");
    if (lists)
    {
        /* lists[LIST_TITLES_ALL] is a list of all titles, we free it */
        FREELIST (lists[LIST_TITLES_ALL]);
        /* the other lists[LIST_TITLES_*] are made of the same pointers, hence
         * their data are free-d when calling FREELIST (lists[LIST_TITLES_ALL])
         * IOW alpm_list_free is enough */
        for (i = 1; i < NB_LISTS; ++i)
        {
            alpm_list_free (lists[i]);
        }
        /* this was holding the pointers, free it */
        free (lists);
    }
}

static void
new_window (gboolean only_updates, GtkWidget **window, GtkWidget **textview)
{
    /* window */
    *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (*window),
        (only_updates) ? "Arch Linux Unread News - kalu" : "Arch Linux News - kalu");
    gtk_window_set_default_size (GTK_WINDOW (*window), 600, 230);
    gtk_container_set_border_width (GTK_CONTAINER (*window), 0);
    gtk_window_set_has_resize_grip (GTK_WINDOW (*window), FALSE);
    g_signal_connect (G_OBJECT (*window), "destroy",
                      G_CALLBACK (window_destroy_cb), NULL);
    /* icon */
    GdkPixbuf *pixbuf;
    pixbuf = gtk_widget_render_icon_pixbuf (*window, "kalu-logo", GTK_ICON_SIZE_DIALOG);
    gtk_window_set_icon (GTK_WINDOW (*window), pixbuf);
    g_object_unref (pixbuf);
    
    /* everything in a vbox */
    GtkWidget *vbox;
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add (GTK_CONTAINER (*window), vbox);
    gtk_widget_show (vbox);
    
    /* textview */
    *textview = gtk_text_view_new ();
    g_object_set (G_OBJECT (*textview),
        "editable",     FALSE,
        "wrap-mode",    GTK_WRAP_WORD,
        NULL);
    
    /* scrolled window for the textview */
    GtkWidget *scrolled;
    scrolled = gtk_scrolled_window_new (
        gtk_text_view_get_hadjustment (GTK_TEXT_VIEW (*textview)),
        gtk_text_view_get_vadjustment (GTK_TEXT_VIEW (*textview)));
    gtk_box_pack_start (GTK_BOX (vbox), scrolled, TRUE, TRUE, 0);
    gtk_widget_show (scrolled);
    
    /* adding textview in scrolled */
    gtk_container_add (GTK_CONTAINER (scrolled), *textview);
    gtk_widget_show (*textview);
    
    /* button box */
    GtkWidget *hbox;
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 2);
    gtk_widget_show (hbox);
    
    GtkWidget *button, *image;
    
    if (only_updates)
    {
        alpm_list_t **lists = calloc (NB_LISTS, sizeof (*lists));
        g_object_set_data (G_OBJECT (*window), "lists", lists);
        
        /* Mark read */
        button = gtk_button_new_with_label ("Mark as read");
        image = gtk_image_new_from_stock (GTK_STOCK_APPLY, GTK_ICON_SIZE_MENU);
        gtk_button_set_image (GTK_BUTTON (button), image);
        gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 4);
        gtk_widget_set_tooltip_text (button, "Mark checked news as read");
        g_signal_connect (G_OBJECT (button), "clicked",
                          G_CALLBACK (btn_mark_cb), (gpointer) *window);
        gtk_widget_show (button);
    }
    
    /* Close */
    button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
    gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 2);
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (btn_close_cb), (gpointer) *window);
    gtk_widget_show (button);
}

gboolean
news_show (gchar *xml_news, gboolean only_updates, GError **error)
{
    GError             *local_err = NULL;
    gboolean            is_xml_ours = FALSE;
    parse_news_data_t   data;
    GtkWidget          *window;
    GtkWidget          *textview;
    
    /* if no XML was provided, download it */
    if (xml_news == NULL)
    {
        xml_news = curl_download (NEWS_RSS_URL, &local_err);
        if (local_err != NULL)
        {
            g_propagate_error (error, local_err);
            set_kalpm_busy (FALSE);
            return FALSE;
        }
        is_xml_ours = TRUE;
    }
    
    new_window (only_updates, &window, &textview);
    
    memset (&data, 0, sizeof (parse_news_data_t));
    data.only_updates = only_updates;
    data.textview = GTK_TEXT_VIEW (textview);
    data.buffer = gtk_text_view_get_buffer (data.textview);
    data.lists = g_object_get_data (G_OBJECT (window), "lists");
    
    if (!parse_xml (xml_news, FALSE, (gpointer) &data, &local_err))
    {
        if (is_xml_ours)
        {
            free (xml_news);
        }
        g_propagate_error (error, local_err);
        set_kalpm_busy (FALSE);
        return FALSE;
    }
    if (is_xml_ours)
    {
        free (xml_news);
    }    
    
    gtk_widget_show (window);
    set_kalpm_busy (FALSE);
    return TRUE;
}

gboolean
news_has_updates (alpm_list_t **titles,
                  gchar       **xml_news,
                  GError      **error)
{
    GError               *local_err = NULL;
    parse_updates_data_t  data;
    
    *xml_news = curl_download (NEWS_RSS_URL, &local_err);
    if (local_err != NULL)
    {
        g_propagate_error (error, local_err);
        return FALSE;
    }
    
    memset (&data, 0, sizeof (parse_updates_data_t));
    if (!parse_xml (*xml_news, TRUE, (gpointer) &data, &local_err))
    {
        free (*xml_news);
        g_propagate_error (error, local_err);
        return FALSE;
    }
    
    if (data.titles == NULL)
    {
        free (*xml_news);
        return FALSE;
    }
    else
    {
        *titles = data.titles;
        return TRUE;
    }
}
