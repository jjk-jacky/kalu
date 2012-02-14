
#ifndef _KALU_UTIL_GTK_H
#define _KALU_UTIL_GTK_H

/* gtk */
#include <gtk/gtk.h>

void
rend_size (GtkTreeViewColumn *column, GtkCellRenderer *renderer,
           GtkTreeModel *store, GtkTreeIter *iter, int col, int is_unsigned);

GtkWidget *
new_confirm (const gchar *message,
             const gchar *submessage,
             const gchar *btn_yes_label,
             const gchar *btn_yes_image,
             const gchar *btn_no_label,
             const gchar *btn_no_image,
             GtkWidget *window
             );

gboolean
confirm (const gchar *message,
         const gchar *submessage,
         const gchar *btn_yes_label,
         const gchar *btn_yes_image,
         const gchar *btn_no_label,
         const gchar *btn_no_image,
         GtkWidget *window
         );

void
show_error (const gchar *summary, const gchar *text, GtkWindow *parent);

void
notify_error (const gchar *summary, const gchar *text);

alpm_list_t *
confirm_choices (const gchar *message,
                 const gchar *submessage,
                 const gchar *btn_yes_label,
                 const gchar *btn_yes_image,
                 const gchar *btn_no_label,
                 const gchar *btn_no_image,
                 const gchar *chk_label,
                 const gchar *choice_label,
                 alpm_list_t *choices,
                 GtkWidget *window
                 );

#endif /* _KALU_UTIL_GTK_H */
