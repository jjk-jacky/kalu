/**
 * kalu - Copyright (C) 2012-2014 Olivier Brunel
 *
 * news.c
 * Copyright (C) 2012-2014 Olivier Brunel <jjk@jjacky.com>
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

#include <config.h>

/* C */
#include <string.h>

#ifndef DISABLE_GUI
/* gtk */
#include <gtk/gtk.h>
#else
/* glib */
#include <glib.h>
#endif

/* kalu */
#include "kalu.h"
#include "news.h"
#include "curl.h"
#include "util.h"
#ifndef DISABLE_GUI
#include "util-gtk.h"
#include "gui.h" /* show_notif() */
#endif

enum {
    LIST_TITLES_ALL = 0,
    LIST_TITLES_SHOWN,
    LIST_TITLES_READ,
    NB_LISTS
};

typedef struct _parse_updates_data_t {
    gboolean     is_last_reached;
    alpm_list_t *titles;
} parse_updates_data_t;

#ifndef DISABLE_GUI

#define HTML_MAN_PAGE       DOCDIR "/html/index.html"
#define HISTORY             DOCDIR "/HISTORY"

typedef struct _parse_news_data_t {
    gboolean         only_updates;
    GtkTextView     *textview;
    GtkTextBuffer   *buffer;
    PangoAttrList   *attr_list;

    gboolean         is_last_reached;
    alpm_list_t    **lists;
} parse_news_data_t;

typedef void (*GMP_text_fn) (GMarkupParseContext *context,
                             const gchar         *text,
                             gsize                text_len,
                             gpointer             user_data,
                             GError             **error);

static void
create_tags (GtkTextBuffer *buffer);

static void
xml_parser_news_text (GMarkupParseContext *context,
                      const gchar         *text,
                      gsize                text_len,
                      parse_news_data_t   *parse_news_data,
                      GError             **error _UNUSED_);


/* TRUE when hovering over a link */
static gboolean hovering_link = FALSE;
/* standard & hover-link cursors */
static GdkCursor *cursor_std = NULL;
static GdkCursor *cursor_link = NULL;
/* nb of windows open */
static gint nb_windows = 0;


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
    if (!list->next || !streq ("item", list->next->data))
    {
        return;
    }

    if (streq ("title", list->data))
    {
        /* is this the last item from last check? */
        if (NULL != config->news_last && streq (config->news_last, text))
        {
            parse_updates_data->is_last_reached = TRUE;
            return;
        }

        /* was this item already read? */
        FOR_LIST (i, config->news_read)
        {
            if (streq (i->data, text))
            {
                return;
            }
        }

        /* add title to the new news */
        parse_updates_data->titles = alpm_list_add (parse_updates_data->titles,
                strdup (text));
    }
}

#endif /* DISABLE_GUI */

static gboolean
parse_xml (gchar *xml, gboolean for_updates, gpointer data_out, GError **error)
{
    GMarkupParseContext *context;
    GMarkupParser        parser;
    GError              *local_err = NULL;

    zero (parser);
    if (for_updates)
    {
#ifndef DISABLE_GUI
        parser.text = (GMP_text_fn) xml_parser_updates_text;
#endif
    }
    else
    {
#ifdef DISABLE_GUI
        return FALSE;
#else
        parse_news_data_t   *data;
        GtkTextBuffer       *buffer;

        parser.text = (GMP_text_fn) xml_parser_news_text;
        data = data_out;
        buffer = data->buffer;

        create_tags (buffer);

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
#endif

    }
    context = g_markup_parse_context_new (&parser, G_MARKUP_TREAT_CDATA_AS_TEXT,
            data_out, NULL);

    if (!g_markup_parse_context_parse (context,
                xml,
                (gssize) strlen (xml),
                &local_err))
    {
        g_markup_parse_context_free (context);
#ifndef DISABLE_GUI
        if (!for_updates && ((parse_news_data_t *)data_out)->only_updates)
        {
            pango_attr_list_unref (((parse_news_data_t *)data_out)->attr_list);
        }
#endif
        g_propagate_error (error, local_err);
        return FALSE;
    }
    g_markup_parse_context_free (context);
#ifndef DISABLE_GUI
    if (!for_updates && ((parse_news_data_t *)data_out)->only_updates)
    {
        pango_attr_list_unref (((parse_news_data_t *)data_out)->attr_list);
    }
#endif
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

    zero (data);
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

/*******************   EVERYTHING BELOW IS NOT DISABLE_GUI *******************/

#ifndef DISABLE_GUI

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

#define insert_text_with_tags() do {                                        \
    gtk_text_buffer_move_mark (buffer, mark, &iter);                        \
    gtk_text_buffer_insert (buffer, &iter, ss, -1);                         \
    gtk_text_buffer_get_iter_at_mark (buffer, &iter2, mark);                \
    FOR_LIST (i, tags)                                                      \
    {                                                                       \
        gtk_text_buffer_apply_tag_by_name (buffer, i->data, &iter, &iter2); \
    }                                                                       \
} while (0)
static void
parse_to_buffer (GtkTextBuffer *buffer, const gchar *text, gsize text_len)
{
    GtkTextIter  iter, iter2;
    GtkTextMark *mark;
    GtkTextTag  *tag;
    alpm_list_t *i, *tags = NULL;
    gchar       *s, *ss, *start, *end;
    gchar        buf[10];
    gint         c, margin;
    gint         in_ordered_list = -1;
    GdkRGBA      color;
    gchar       *link = NULL;

    s = new (gchar, text_len + 2);
    snprintf (s, text_len + 1, "%s", text);

    /* color used for links */
    gdk_rgba_parse (&color, "rgb(0,119,187)");

    /* inside <code> blocs, \n must not be converted to space */
    start = s;
    while ((start = strstr (start, "<code>")))
    {
        if (!(end = strstr (start, "</code>")))
        {
            break;
        }
        /* for now, we turn them into \r */
        while ((ss = strchr (start, '\n')) && ss < end)
        {
            *ss = '\r';
        }
        start = end;
    }
    /* \n replaced by space */
    while ((ss = strchr (s, '\n')))
    {
        *ss = ' ';
    }
    /* turn the \r from <code> blocks into \n */
    while ((ss = strchr (s, '\r')))
    {
        *ss = '\n';
    }
    /* now turn <br> into \n */
    while ((ss = strstr (s, "<br")))
    {
        if (!(end = strchr (ss, '>')))
        {
            break;
        }
        *ss = '\n';
        start = end + 1;
        memmove (++ss, start, strlen (start) + 1);
    }
    /* convert some HTML stuff */
    ss = s;
    while ((ss = strchr (ss, '&')))
    {
        end = strchr (++ss, ';');
        if (!end)
        {
            break;
        }
        *end = '\0';
        if (streq (ss, "minus"))
        {
            *--ss = '-';
            ++end;
            memmove (++ss, end, strlen (end) + 1);
        }
        else if (streq (ss, "lsquo"))
        {
            *--ss = '`';
            ++end;
            memmove (++ss, end, strlen (end) + 1);
        }
        else if (streq (ss, "rsquo"))
        {
            *--ss = '\'';
            ++end;
            memmove (++ss, end, strlen (end) + 1);
        }
        else if (streq (ss, "quot"))
        {
            *--ss = '"';
            ++end;
            memmove (++ss, end, strlen (end) + 1);
        }
        else if (streq (ss, "amp"))
        {
            *--ss = '&';
            ++end;
            memmove (++ss, end, strlen (end) + 1);
        }
        else if (streq (ss, "lt"))
        {
            *--ss = '<';
            *end = '>';
        }
        else if (streq (ss, "gt"))
        {
            *--ss = '<';
            *end = '>';
        }
        else
        {
            *end = ';';
            ss = end + 1;
        }
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
        if (*(start + 1) == 'p' && (*(start + 2) == '\0'
                    || *(start + 2) == ' '))
        {
            *start = '\0';
            insert_text_with_tags ();
            gtk_text_buffer_insert (buffer, &iter, "\n", -1);

            /* look for the margin-left style, and create a corresponding tag.
             * This is useful when showing the (HTML) man page */
            ++start;
            if ((ss = strstr (start, "margin-left:")))
            {
                ss += 12; /* 12 = strlen ("margin-left:") */
                for (c = 0, margin = 0; *ss >= '0' && *ss <= '9'; ++ss, ++c)
                {
                    margin = margin * 10 + (*ss - '0');
                }
                snprintf (buf, 10, "margin%d", margin);
                if (!gtk_text_tag_table_lookup (
                            gtk_text_buffer_get_tag_table (buffer),
                            buf))
                {
                    gtk_text_buffer_create_tag (buffer, buf,
                            "left-margin",      margin,
                            NULL);
                }
                tags = alpm_list_add (tags, (void *) buf);
            }
        }
        else if (streq (start + 1, "/p"))
        {
            *start = '\0';
            insert_text_with_tags ();
            gtk_text_buffer_insert (buffer, &iter, "\n", -1);

            /* when showing the (HTML) man page, <p> tags (might) have a margin,
             * that should be closed here. we assume proper HTML, no recursion
             * and whatnot, but that should be the case
             * Go through tags from last to first (first->prev is last) and
             * remove the first margin found */
            for (i = tags;
                    i && ((i->prev == tags && !i->next) || i->prev != tags);
                    i = i->prev)
            {
                if (strncmp (i->data, "margin", 6) == 0)
                {
                    tags = alpm_list_remove_str (tags, i->data, NULL);
                    break;
                }
            }

        }
        else if (streq (start + 1, "b"))
        {
            *start = '\0';
            insert_text_with_tags ();
            tags = alpm_list_add (tags, (void *) "bold");
        }
        else if (streq (start + 1, "/b"))
        {
            *start = '\0';
            insert_text_with_tags ();
            tags = alpm_list_remove_str (tags, "bold", NULL);
        }
        else if (streq (start + 1, "code"))
        {
            *start = '\0';
            insert_text_with_tags ();
            tags = alpm_list_add (tags, (void *) "code");
        }
        else if (streq (start + 1, "/code"))
        {
            *start = '\0';
            insert_text_with_tags ();
            tags = alpm_list_remove_str (tags, "code", NULL);
        }
        else if (streq (start + 1, "pre"))
        {
            *start = '\0';
            insert_text_with_tags ();
            tags = alpm_list_add (tags, (void *) "pre");
        }
        else if (streq (start + 1, "/pre"))
        {
            *start = '\0';
            insert_text_with_tags ();
            tags = alpm_list_remove_str (tags, "pre", NULL);
        }
        else if (streq (start + 1, "h2"))
        {
            *start = '\0';
            insert_text_with_tags ();
            gtk_text_buffer_insert (buffer, &iter, "\n", -1);
            tags = alpm_list_add (tags, (void *) "title");
        }
        else if (streq (start + 1, "/h2"))
        {
            *start = '\0';
            insert_text_with_tags ();
            gtk_text_buffer_insert (buffer, &iter, "\n", -1);
            tags = alpm_list_remove_str (tags, "title", NULL);
        }
        else if (streq (start + 1, "i"))
        {
            *start = '\0';
            insert_text_with_tags ();
            tags = alpm_list_add (tags, (void *) "italic");
        }
        else if (streq (start + 1, "/i"))
        {
            *start = '\0';
            insert_text_with_tags ();
            tags = alpm_list_remove_str (tags, "italic", NULL);
        }
        else if (streq (start + 1, "ul"))
        {
            *start = '\0';
            insert_text_with_tags ();
            gtk_text_buffer_insert (buffer, &iter, "\n", -1);
        }
        else if (streq (start + 1, "ol"))
        {
            *start = '\0';
            insert_text_with_tags ();
            gtk_text_buffer_insert (buffer, &iter, "\n", -1);
            in_ordered_list = 0;
        }
        else if (streq (start + 1, "li"))
        {
            *start = '\0';
            insert_text_with_tags ();
            gtk_text_buffer_insert (buffer, &iter, "\n", -1);
            tags = alpm_list_add (tags, (void *) "listitem");
            if (in_ordered_list == -1)
            {
                ss = (gchar *) "• ";
            }
            else
            {
                ++in_ordered_list;
                snprintf (buf, 10, "%d. ", in_ordered_list);
                ss = buf;
            }
            insert_text_with_tags ();
        }
        else if (streq (start + 1, "/li"))
        {
            *start = '\0';
            insert_text_with_tags ();
            gtk_text_buffer_insert (buffer, &iter, "\n", -1);
            tags = alpm_list_remove_str (tags, "listitem", NULL);
        }
        else if (streq (start + 1, "/ol"))
        {
            *start = '\0';
            insert_text_with_tags ();
            in_ordered_list = -1;
        }
        else if (streq (start + 1, "lt"))
        {
            *start = '\0';
            insert_text_with_tags ();
            ss = (gchar *) "<";
            insert_text_with_tags ();
        }
        else if (streq (start + 1, "gt"))
        {
            *start = '\0';
            insert_text_with_tags ();
            ss = (gchar *) ">";
            insert_text_with_tags ();
        }
        else if (strncmp (start + 1, "a ", 2) == 0)
        {
            /* add the text */
            *start = '\0';
            insert_text_with_tags ();
            /* get URL */
            if ((link = strstr (start + 1, "href"))
                    && (link = strchr (link, '"')))
            {
                /* got one, end it properly */
                ss = strchr (++link, '"');
                *ss = '\0';
            }
        }
        else if (streq (start + 1, "/a"))
        {
            *start = '\0';
            insert_text_with_tags ();
            if (link)
            {
                /* create a new tag, so we can set the link to it */
                tag = gtk_text_buffer_create_tag (buffer, NULL,
                        "foreground-rgba",  &color,
                        "underline",        PANGO_UNDERLINE_SINGLE,
                        NULL);
                /* links on Arch's website don't always include the http:// part */
                if (link[0] == '/')
                {
                    ss = new (gchar, strlen (link) + 25);
                    /* TODO: get domain from NEWS_RSS_URL */
                    sprintf (ss, "http://www.archlinux.org%s", link);
                    link = ss;
                }
                else
                {
                    link = strdup (link);
                }
                /* set link, and apply it */
                g_object_set_data_full (G_OBJECT (tag), "link", link, free);
                gtk_text_buffer_apply_tag (buffer, tag, &iter, &iter2);
                /* done */
                link = NULL;
            }
        }
        else
        {
            /* unknown tag - just skip it */
            *start = '\0';
            insert_text_with_tags ();
        }
        ss = end + 1;
    }
    insert_text_with_tags ();

    gtk_text_buffer_delete_mark (buffer, mark);
    free (s);
}
#undef insert_text_with_tags

static void
xml_parser_news_text (GMarkupParseContext *context,
                      const gchar         *text,
                      gsize                text_len,
                      parse_news_data_t   *parse_news_data,
                      GError             **error _UNUSED_)
{
    GtkTextBuffer   *buffer = parse_news_data->buffer;
    GtkTextIter     iter;
    gchar           *s = NULL;
    const GSList    *list;
    alpm_list_t     *i;
    gboolean        is_title = FALSE;
    static gboolean skip_next_description = FALSE;
    alpm_list_t   **lists = NULL;

    /* have we already reached the last unread item */
    if (parse_news_data->only_updates && parse_news_data->is_last_reached)
    {
        return;
    }

    /* is this a tag (title, description, ...) inside an item? */
    list = g_markup_parse_context_get_element_stack (context);
    if (!list->next || !streq ("item", list->next->data))
    {
        return;
    }

    /* gather whether this is title or description, don't go any further if it's
     * anything else*/
    if (streq ("title", list->data))
    {
        is_title = TRUE;
    }
    else if (!streq ("description", list->data))
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
            if (NULL != config->news_last && streq (config->news_last, text))
            {
                parse_news_data->is_last_reached = TRUE;
                return;
            }

            /* was this item already read? */
            FOR_LIST (i, config->news_read)
            {
                if (streq (i->data, text))
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

        if (parse_news_data->only_updates && lists)
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
            gtk_label_set_attributes (GTK_LABEL (label),
                    parse_news_data->attr_list);
            gtk_container_add (GTK_CONTAINER (check), label);
            gtk_widget_show (label);
            gtk_text_view_add_child_at_anchor (parse_news_data->textview,
                    check,
                    anchor);
        }
        else
        {
            gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                    text, -1, "title", NULL);
        }
        gtk_text_buffer_insert (buffer, &iter, "\n", -1);
    }
    else
    {
        parse_to_buffer (buffer, text, text_len);
    }
}

static void
create_tags (GtkTextBuffer *buffer)
{
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

    gtk_text_buffer_create_tag (buffer, "italic",
            "style",            PANGO_STYLE_ITALIC,
            NULL);

    gtk_text_buffer_create_tag (buffer, "listitem",
            "left-margin",      15,
            NULL);
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

    FOR_LIST (i, titles_all)
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

            FOR_LIST (i, news_read)
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

            /* we go and change the last_notifs. if nb_unread = 0 we can
             * simply remove it, else we change it to ask to run the checks again
             * to be up to date */
            FOR_LIST (i, config->last_notifs)
            {
                notif_t *notif = i->data;
                if (notif->type & CHECK_NEWS)
                {
                    if (nb_unread == 0)
                    {
                        config->last_notifs = alpm_list_remove_item (
                                config->last_notifs,
                                i);
                        free_notif (notif);
                    }
                    else
                    {
                        free (notif->data);
                        notif->data = NULL;
                        free (notif->text);
                        notif->text = strdup (_("Read news have changed, "
                                    "you need to run the checks again to be up-to-date."));
                    }
                    break;
                }
            }
            set_kalpm_nb (CHECK_NEWS, nb_unread, TRUE);
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
        show_error (_("Unable to save changes to disk"), file,
                GTK_WINDOW (window));
    }
}

static void
window_destroy_cb (GtkWidget *window, gpointer data _UNUSED_)
{
    alpm_list_t **lists;
    int i;

    if (--nb_windows == 0)
    {
        g_object_unref (cursor_link);
        g_object_unref (cursor_std);
    }

    /* will be present if this was a only_updates window */
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

    /* remove from list of open windows */
    remove_open_window (window);
}

static gboolean
motion_notify_event_cb (GtkTextView *textview, GdkEventMotion *event)
{
    gint x, y;
    GSList *tags = NULL, *t;
    GtkTextIter iter;
    gboolean hovering = FALSE;

    gtk_text_view_window_to_buffer_coords (textview, GTK_TEXT_WINDOW_WIDGET,
            (gint) event->x, (gint) event->y,
            &x, &y);
    gtk_text_view_get_iter_at_location (textview, &iter, x, y);

    tags = gtk_text_iter_get_tags (&iter);
    for (t = tags; t; t = t->next)
    {
        GtkTextTag *tag = t->data;
        gchar *link = g_object_get_data (G_OBJECT (tag), "link");
        if (link)
        {
            hovering = TRUE;
            break;
        }
    }
    if (tags)
    {
        g_slist_free (tags);
    }

    /* need to update cursor? */
    if (hovering != hovering_link)
    {
        hovering_link = hovering;

        if (hovering_link)
        {
            gdk_window_set_cursor (gtk_text_view_get_window (textview,
                        GTK_TEXT_WINDOW_TEXT),
                    cursor_link);
        }
        else
        {
            gdk_window_set_cursor (gtk_text_view_get_window (textview,
                        GTK_TEXT_WINDOW_TEXT),
                    cursor_std);
        }
    }

    return FALSE;
}

static gboolean
event_after_cb (GtkTextView *textview, GdkEvent *ev, GtkWidget *window)
{
    gint x, y;
    GSList *tags = NULL, *t;
    GtkTextIter start, end, iter;
    GtkTextBuffer *buffer;
    GdkEventButton *event;

    if (ev->type != GDK_BUTTON_RELEASE)
    {
        return FALSE;
    }

    event = (GdkEventButton *) ev;
    if (event->button != GDK_BUTTON_PRIMARY)
    {
        return FALSE;
    }

    buffer = gtk_text_view_get_buffer (textview);

    /* do nothing if the user is selecting something */
    gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
    if (gtk_text_iter_get_offset (&start) != gtk_text_iter_get_offset (&end))
    {
        return FALSE;
    }

    gtk_text_view_window_to_buffer_coords (textview, GTK_TEXT_WINDOW_WIDGET,
            (gint) event->x, (gint) event->y,
            &x, &y);
    gtk_text_view_get_iter_at_location (textview, &iter, x, y);

    tags = gtk_text_iter_get_tags (&iter);
    for (t = tags; t != NULL; t = t->next)
    {
        GtkTextTag *tag = t->data;
        gchar *link = g_object_get_data (G_OBJECT (tag), "link");
        if (link)
        {
            GError *error = NULL;
            gchar buf[1024], *b, *s, *ss;
            size_t len = strlen (link);

            debug ("click on link: %s", link);

            if (len + strlen (config->cmdline_link) >= 1024)
            {
                b = new (gchar, len + strlen (config->cmdline_link));
            }
            else
            {
                b = buf;
            }

            for (s = config->cmdline_link, ss = b; s && *s != '\0'; ++s, ++ss)
            {
                if (*s == '$' && *(s + 1) == 'U' && *(s + 2) == 'R'
                        && *(s + 3) == 'L')
                {
                    memcpy (ss, link, len);
                    ss += len - 1;
                    s += 3;
                }
                else
                {
                    *ss = *s;
                }
            }
            *ss = '\0';
            debug ("cmdline: %s", buf);

            if (!g_spawn_command_line_async (buf, &error))
            {
                show_error (_("Unable to open link"), error->message,
                        GTK_WINDOW (window));
                g_clear_error (&error);
            }

            if (b != buf)
            {
                free (b);
            }
            break;
        }
    }
    if (tags)
    {
        g_slist_free (tags);
    }

    return FALSE;
}

static gboolean
query_tooltip_cb (GtkTextView *textview, gint wx, gint wy,
                  gboolean keyboard _UNUSED_, GtkTooltip *tooltip,
                  gpointer data _UNUSED_)
{
    gint x, y;
    GtkTextIter iter;
    GSList *tags = NULL, *t;

    gtk_text_view_window_to_buffer_coords (textview, GTK_TEXT_WINDOW_WIDGET,
            wx, wy,
            &x, &y);
    gtk_text_view_get_iter_at_location (textview, &iter, x, y);

    tags = gtk_text_iter_get_tags (&iter);
    for (t = tags; t; t = t->next)
    {
        GtkTextTag *tag = t->data;
        gchar *link = g_object_get_data (G_OBJECT (tag), "link");
        if (link)
        {
            gtk_tooltip_set_text (tooltip, link);
            g_slist_free (tags);
            return TRUE;
        }
    }
    if (tags)
    {
        g_slist_free (tags);
    }

    return FALSE;
}

static void
new_window (gboolean only_updates, GtkWidget **window, GtkWidget **textview)
{
    ++nb_windows;

    /* window */
    *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (*window), (only_updates)
            ? _("Arch Linux Unread News - kalu")
            : _("Arch Linux News - kalu"));
    gtk_window_set_default_size (GTK_WINDOW (*window), 600, 230);
    gtk_container_set_border_width (GTK_CONTAINER (*window), 0);
    gtk_window_set_has_resize_grip (GTK_WINDOW (*window), FALSE);
    g_signal_connect (G_OBJECT (*window), "destroy",
            G_CALLBACK (window_destroy_cb), NULL);
    /* add to list of open windows */
    add_open_window (*window);
    /* icon */
    gtk_window_set_icon_name (GTK_WINDOW (*window), "kalu");

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

    /* if this is the only window, create cursors */
    if (nb_windows == 1)
    {
        cursor_std = gdk_cursor_new (GDK_XTERM);
        cursor_link = gdk_cursor_new (GDK_HAND2);
    }
    /* signals for links (change cursor; handle click) */
    g_signal_connect (*textview, "motion-notify-event",
            G_CALLBACK (motion_notify_event_cb), NULL);
    g_signal_connect (*textview, "event-after",
            G_CALLBACK (event_after_cb), (gpointer) *window);
    /* to provide URL in tooltip for links */
    g_object_set (G_OBJECT (*textview), "has-tooltip", TRUE, NULL);
    g_signal_connect (*textview, "query-tooltip",
            G_CALLBACK (query_tooltip_cb), NULL);

    /* scrolled window for the textview */
    GtkWidget *scrolled;
    scrolled = gtk_scrolled_window_new (
            gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (*textview)),
            gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (*textview)));
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
        alpm_list_t **lists = new0 (alpm_list_t *, NB_LISTS);
        g_object_set_data (G_OBJECT (*window), "lists", lists);

        /* Mark read */
        button = gtk_button_new_with_mnemonic (_("_Mark as read"));
        image = gtk_image_new_from_icon_name ("gtk-apply", GTK_ICON_SIZE_MENU);
        gtk_button_set_image (GTK_BUTTON (button), image);
        gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 4);
        gtk_widget_set_tooltip_text (button, _("Mark checked news as read"));
        g_signal_connect (G_OBJECT (button), "clicked",
                G_CALLBACK (btn_mark_cb), (gpointer) *window);
        gtk_button_set_always_show_image ((GtkButton *) button, TRUE);
        gtk_widget_show (button);
    }

    /* Close */
    button = gtk_button_new_with_mnemonic (_("_Close"));
    image = gtk_image_new_from_icon_name ("window-close", GTK_ICON_SIZE_MENU);
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 2);
    g_signal_connect (G_OBJECT (button), "clicked",
            G_CALLBACK (btn_close_cb), (gpointer) *window);
    gtk_button_set_always_show_image ((GtkButton *) button, TRUE);
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

    zero (data);
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
        gtk_widget_destroy (window);
        set_kalpm_busy (FALSE);
        return FALSE;
    }
    if (is_xml_ours)
    {
        free (xml_news);
    }

    /* if we were only showing updates, but there are none to show (i.e. from
     * the menu "Show unread news") then just show a notif about it */
    if (only_updates && data.lists && data.lists[LIST_TITLES_SHOWN] == NULL)
    {
        gtk_widget_destroy (window);
        notify_error (_("No unread news"),
                _("There are no unread Arch Linux news."));
    }
    else
    {
        gtk_widget_show (window);
    }

    set_kalpm_busy (FALSE);
    return TRUE;
}

gboolean
show_help (GError **error)
{
    GError        *local_err = NULL;
    GtkWidget     *window;
    GtkWidget     *textview;
    GtkTextBuffer *buffer;
    gchar         *text, *t, *s;

    new_window (FALSE, &window, &textview);
    gtk_window_set_title (GTK_WINDOW (window), _("Help - kalu"));
    gtk_window_set_default_size (GTK_WINDOW (window), 600, 420);
    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));

    if (!g_file_get_contents (HTML_MAN_PAGE, &text, NULL, &local_err))
    {
        g_propagate_error (error, local_err);
        gtk_widget_destroy (window);
        return FALSE;
    }

    t = text;
    /* skip HTML headers, style & TOC */
    if ((s = strstr (text, "<hr>")))
    {
        text = s + 4;
    }

    create_tags (buffer);
    parse_to_buffer (buffer, text, (gsize) strlen (text));
    g_free (t);
    gtk_widget_show (window);
    return TRUE;
}

gboolean
show_history (GError **error)
{
    GError        *local_err = NULL;
    GtkWidget     *window;
    GtkWidget     *textview;
    GtkTextBuffer *buffer;
    gchar         *text, *s;

    new_window (FALSE, &window, &textview);
    gtk_window_set_title (GTK_WINDOW (window), _("History - kalu"));
    gtk_window_set_default_size (GTK_WINDOW (window), 600, 420);
    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));

    if (!g_file_get_contents (HISTORY, &text, NULL, &local_err))
    {
        g_propagate_error (error, local_err);
        gtk_widget_destroy (window);
        return FALSE;
    }

    /* to preserves '<' */
    s = strreplace (text, "<", " <lt>");
    g_free (text);
    text = s;

    /* to preserves '>' */
    s = strreplace (text, ">", " <gt>");
    g_free (text);
    text = s;

    /* to preserve LF-s */
    s = strreplace (text, "\n\n", " <br>");
    g_free (text);
    text = s;

    /* add empty line before each new line (change) */
    s = strreplace (text, "<br>-", "<br><br>-");
    g_free (text);
    text = s;

    /* to turn date/version number into titles (w/ some styling) */
    s = strreplace (text, "\n# ", "<br><h2>");
    g_free (text);
    text = s;
    while ((s = strstr (s, "<h2>")))
    {
        s = strstr (s, " <br>");
        if (s)
        {
            *(s + 0) = '<';
            *(s + 1) = '/';
            *(s + 2) = 'h';
            *(s + 3) = '2';
            *(s + 4) = '>';
        }
    }

    create_tags (buffer);
    parse_to_buffer (buffer, text, (gsize) strlen (text));
    g_free (text);
    gtk_widget_show (window);
    return TRUE;
}

void
show_pacman_conflict ()
{
    GtkWidget     *window;
    GtkWidget     *textview;
    GtkTextBuffer *buffer;
    const gchar   *text = _("<h2>Possible pacman/kalu conflict</h2>"
        "<p>The pending system upgrade is likely to fail due to kalu's dependency "
        "on the current version of pacman. This is because the new pacman introduces "
        "API changes in libalpm (on which kalu relies).</p>"
        "<h2>How to upgrade?</h2>"
        "<p>In order to upgrade your system, you will need to :"
        "<br> <b>1.</b> Remove kalu (<pre>pacman -R kalu</pre>) This will <b>not</b> "
        "remove your preferences, watched lists, etc"
        "<br> <b>2.</b> Upgrade your system (<pre>pacman -Syu</pre>)"
        "<br> <b>3.</b> Install a new version of kalu, compatible with the new "
        "version of pacman.</p>"
        "<p>If a new version of kalu for the new pacman isn't available on the "
        "AUR yet, make sure to flag it as out-of-date.</p>")
        ;

    new_window (FALSE, &window, &textview);
    gtk_window_set_title (GTK_WINDOW (window),
            _("Possible pacman/kalu conflict - kalu"));
    gtk_window_set_default_size (GTK_WINDOW (window), 600, 230);
    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
    create_tags (buffer);
    parse_to_buffer (buffer, text, (gsize) strlen (text));
    gtk_widget_show (window);
}

#endif /* DISABLE_GUI */
