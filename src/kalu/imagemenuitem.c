/**
 * kalu - Copyright (C) 2012-2018 Olivier Brunel
 *
 * imagemenuitem.c
 * Copyright (C) 2014 Olivier Brunel <jjk@jjacky.com>
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

/**
 * imagemenuitem.c from Donnatella -- http://jjacky.com/donnatella
 */

#include <config.h>

#include <gtk/gtk.h>
#include "imagemenuitem.h"
#include "kalu.h"

enum
{
    PROP_0,

    PROP_IMAGE,
    PROP_IMAGE_SELECTED,
    PROP_IMAGE_SPECIAL,
    PROP_IS_ACTIVE,
    PROP_IS_INCONSISTENT,
    PROP_IS_COMBINED,
    PROP_IS_COMBINED_SENSITIVE,
    PROP_IS_LABEL_BOLD,
    PROP_IS_LABEL_MARKUP,

    NB_PROPS
};

enum
{
    SIGNAL_LOAD_SUBMENU,
    NB_SIGNALS
};

struct _DonnaImageMenuItemPrivate
{
    GtkWidget   *image;
    GtkWidget   *image_org;
    GtkWidget   *image_sel;
    guint        image_special          : 2;
    guint        is_active              : 1;
    guint        is_inconsistent        : 1;
    guint        is_combined            : 1;
    guint        is_combined_sensitive  : 1;
    guint        is_label_bold          : 1;
    guint        is_label_markup        : 1;
    gint         toggle_size;

    gdouble      item_width;
    gulong       sid_button_release;
    gulong       sid_parent_button_release;
    guint        sid_timeout;
};

static GParamSpec * donna_image_menu_item_props[NB_PROPS] = { NULL, };
static guint        donna_image_menu_item_signals[NB_SIGNALS] = { 0, };


static gboolean menu_item_release_cb            (GtkWidget             *widget,
                                                 GdkEventButton        *event,
                                                 DonnaImageMenuItem    *item);


static void donna_image_menu_item_destroy               (GtkWidget      *widget);
static void donna_image_menu_item_get_preferred_width   (GtkWidget      *widget,
                                                         gint           *minimum,
                                                         gint           *natural);
static void donna_image_menu_item_get_preferred_height  (GtkWidget      *widget,
                                                         gint           *minimum,
                                                         gint           *natural);
static void donna_image_menu_item_get_preferred_height_for_width (
                                                         GtkWidget      *widget,
                                                         gint            width,
                                                         gint           *minimum,
                                                         gint           *natural);
static void donna_image_menu_item_size_allocate         (GtkWidget      *widget,
                                                         GtkAllocation  *allocation);
static void donna_image_menu_item_map                   (GtkWidget      *widget);
static void donna_image_menu_item_parent_set            (GtkWidget      *widget,
                                                         GtkWidget      *old_parent);
static gboolean donna_image_menu_item_draw              (GtkWidget      *widget,
                                                         cairo_t        *cr);

static void donna_image_menu_item_toggle_size_request   (GtkMenuItem    *menu_item,
                                                         gint           *requisition);
static void donna_image_menu_item_toggle_size_allocate  (GtkMenuItem    *menu_item,
                                                         gint            allocation);
static void donna_image_menu_item_select                (GtkMenuItem    *item);
static void donna_image_menu_item_deselect              (GtkMenuItem    *item);
static void donna_image_menu_item_set_label             (GtkMenuItem    *item,
                                                         const gchar    *label);
static const gchar * donna_image_menu_item_get_label    (GtkMenuItem    *item);

static void donna_image_menu_item_forall                (GtkContainer   *container,
                                                         gboolean        include_internals,
                                                         GtkCallback     callback,
                                                         gpointer        data);
static void donna_image_menu_item_remove                (GtkContainer   *container,
                                                         GtkWidget      *child);

static void donna_image_menu_item_finalize              (GObject        *object);
static void donna_image_menu_item_set_property          (GObject        *object,
                                                         guint           prop_id,
                                                         const GValue   *value,
                                                         GParamSpec     *pspec);
static void donna_image_menu_item_get_property          (GObject        *object,
                                                         guint           prop_id,
                                                         GValue         *value,
                                                         GParamSpec     *pspec);

G_DEFINE_TYPE (DonnaImageMenuItem, donna_image_menu_item,
        GTK_TYPE_MENU_ITEM)


static void
donna_image_menu_item_class_init (DonnaImageMenuItemClass *klass)
{
    GObjectClass *o_class;
    GtkWidgetClass *w_class;
    GtkContainerClass *c_class;
    GtkMenuItemClass *mi_class;

    o_class = (GObjectClass *) klass;
    o_class->finalize       = donna_image_menu_item_finalize;
    o_class->set_property   = donna_image_menu_item_set_property;
    o_class->get_property   = donna_image_menu_item_get_property;

    w_class = (GtkWidgetClass *) klass;
    w_class->destroy                = donna_image_menu_item_destroy;
    w_class->get_preferred_width    = donna_image_menu_item_get_preferred_width;
    w_class->get_preferred_height   = donna_image_menu_item_get_preferred_height;
    w_class->get_preferred_height_for_width = donna_image_menu_item_get_preferred_height_for_width;
    w_class->size_allocate          = donna_image_menu_item_size_allocate;
    w_class->map                    = donna_image_menu_item_map;
    w_class->parent_set             = donna_image_menu_item_parent_set;
    w_class->draw                   = donna_image_menu_item_draw;

    c_class = (GtkContainerClass *) klass;
    c_class->forall = donna_image_menu_item_forall;
    c_class->remove = donna_image_menu_item_remove;

    mi_class = (GtkMenuItemClass *) klass;
    mi_class->toggle_size_request   = donna_image_menu_item_toggle_size_request;
    mi_class->toggle_size_allocate  = donna_image_menu_item_toggle_size_allocate;
    mi_class->select                = donna_image_menu_item_select;
    mi_class->deselect              = donna_image_menu_item_deselect;
    mi_class->set_label             = donna_image_menu_item_set_label;
    mi_class->get_label             = donna_image_menu_item_get_label;

    /**
     * DonnaImageMenuItem:image:
     *
     * Child widget to appear next to the menu text.
     */
    donna_image_menu_item_props[PROP_IMAGE] =
        g_param_spec_object ("image", "Image widget",
                "Child widget to appear next to the menu text",
                GTK_TYPE_WIDGET,
                G_PARAM_READWRITE);

    /**
     * DonnaImageMenuItem:image-selected:
     *
     * Child widget to appear next to the menu text when item is selected.
     */
    donna_image_menu_item_props[PROP_IMAGE_SELECTED] =
        g_param_spec_object ("image-selected", "Selected image widget",
                "Child widget to appear next to the menu text when item is selected",
                GTK_TYPE_WIDGET,
                G_PARAM_READWRITE);

    /**
     * DonnaImageMenuItem:image-special:
     *
     * Special image to use in the menu item. This allows to use, instead of an
     * image, a check box or radio option.
     *
     * If set, it will take precedence over :image or :image-selected
     */
    donna_image_menu_item_props[PROP_IMAGE_SPECIAL] =
        g_param_spec_uint ("image-special", "Special image",
                "Special image used in the menu item",
                DONNA_IMAGE_MENU_ITEM_IS_IMAGE, /* minimum */
                _DONNA_IMAGE_MENU_ITEM_NB_IMAGE_SPECIAL - 1, /* maximum */
                DONNA_IMAGE_MENU_ITEM_IS_IMAGE, /* default */
                G_PARAM_READWRITE);

    /**
     * DonnaImageMenuItem:is-active:
     *
     * Whether the check box or radio option is active/enabled or not.
     *
     * This only applies if :image-special is %DONNA_IMAGE_MENU_ITEM_IS_CHECK
     * or %DONNA_IMAGE_MENU_ITEM_IS_RADIO
     */
    donna_image_menu_item_props[PROP_IS_ACTIVE] =
        g_param_spec_boolean ("is-active", "is-active",
                "Whether the check box/radio option is active/checked",
                FALSE, /* default */
                G_PARAM_READWRITE);

    /**
     * DonnaImageMenuItem:is-inconsistent:
     *
     * Whether the check box is in an inconsistent state or not (i.e. mixed of
     * active/selected and not)
     *
     * This only applies if :image-special is %DONNA_IMAGE_MENU_ITEM_IS_CHECK
     */
    donna_image_menu_item_props[PROP_IS_INCONSISTENT] =
        g_param_spec_boolean ("is-inconsistent", "is-inconsistent",
                "Whether the check box option is inconsistent",
                FALSE, /* default */
                G_PARAM_READWRITE);

    /**
     * DonnaImageMenuItem:is-combined:
     *
     * Whether or not this item is a combined action and submenu
     */
    donna_image_menu_item_props[PROP_IS_COMBINED] =
        g_param_spec_boolean ("is-combined", "is-combined",
                "Whether or not this item is a combined action and submenu",
                FALSE,   /* default */
                G_PARAM_READWRITE);

    /**
     * DonnaImageMenuItem:is-combined-sensitive:
     *
     * Whether or not the item part of the combne item is sensitive
     */
    donna_image_menu_item_props[PROP_IS_COMBINED_SENSITIVE] =
        g_param_spec_boolean ("is-combined-sensitive", "is-combined-sensitive",
                "Whether or not the item part of the combine item is sensitive",
                TRUE,   /* default */
                G_PARAM_READWRITE);

    /**
     * DonnaImageMenuItem:is-label-bold:
     *
     * Whether or not the label is shown in bold
     */
    donna_image_menu_item_props[PROP_IS_LABEL_BOLD] =
        g_param_spec_boolean ("is-label-bold", "is-label-bold",
                "Whether or not the label is shown in bold",
                FALSE,  /* default */
                G_PARAM_READWRITE);

    /**
     * DonnaImageMenuItem:is-label-markup:
     *
     * Whether or not the label is parsed as markup
     */
    donna_image_menu_item_props[PROP_IS_LABEL_MARKUP] =
        g_param_spec_boolean ("is-label-markup", "is-label-markup",
                "Whether or not the label is parsed as markup",
                FALSE,  /* default */
                G_PARAM_READWRITE);

    g_object_class_install_properties (o_class, NB_PROPS,
            donna_image_menu_item_props);

    /**
     * DonnaImageMenuItem:indicator-size:
     *
     * Size of the check box/radio option, when applicable (See :image-special)
     */
    gtk_widget_class_install_style_property (w_class,
            g_param_spec_int ("indicator-size", "Indicator size",
                "Size of check box or radio option image",
                0,
                G_MAXINT,
                16,
                G_PARAM_READWRITE));

    /**
     * DonnaImageMenuItem::load-submenu:
     *
     * Emitted when the submenu for the item should be loaded
     */
    donna_image_menu_item_signals[SIGNAL_LOAD_SUBMENU] =
        g_signal_new ("load-submenu",
                DONNA_TYPE_IMAGE_MENU_ITEM,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (DonnaImageMenuItemClass, load_submenu),
                NULL,
                NULL,
                g_cclosure_marshal_VOID__BOOLEAN,
                G_TYPE_NONE,
                1,
                G_TYPE_BOOLEAN);

    g_type_class_add_private (klass, sizeof (DonnaImageMenuItemPrivate));
}

static void
donna_image_menu_item_init (DonnaImageMenuItem *item)
{
    item->priv = G_TYPE_INSTANCE_GET_PRIVATE (item,
            DONNA_TYPE_IMAGE_MENU_ITEM, DonnaImageMenuItemPrivate);

    /* this will be the first handler called, so we can block everything else */
    item->priv->sid_button_release = g_signal_connect (item,
            "button-release-event", (GCallback) menu_item_release_cb, item);

    item->priv->is_combined_sensitive = TRUE;
}

static void
donna_image_menu_item_finalize (GObject *object)
{
    DonnaImageMenuItemPrivate *priv = ((DonnaImageMenuItem *) object)->priv;

    if (priv->sid_parent_button_release)
        g_signal_handler_disconnect (
                gtk_widget_get_parent ((GtkWidget *) object),
                priv->sid_parent_button_release);
    if (priv->image_org)
        g_object_unref (priv->image_org);
    if (priv->image_sel)
        g_object_unref (priv->image_sel);

    ((GObjectClass *) donna_image_menu_item_parent_class)->finalize (object);
}

static void
donna_image_menu_item_set_property (GObject        *object,
                                    guint           prop_id,
                                    const GValue   *value,
                                    GParamSpec     *pspec)
{
    DonnaImageMenuItem *item = (DonnaImageMenuItem *) object;

    switch (prop_id)
    {
        case PROP_IMAGE:
            donna_image_menu_item_set_image (item,
                    (GtkWidget *) g_value_get_object (value));
            break;

        case PROP_IMAGE_SELECTED:
            donna_image_menu_item_set_image_selected (item,
                    (GtkWidget *) g_value_get_object (value));
            break;

        case PROP_IMAGE_SPECIAL:
            donna_image_menu_item_set_image_special (item,
                    g_value_get_uint (value));
            break;

        case PROP_IS_ACTIVE:
            donna_image_menu_item_set_is_active (item,
                    g_value_get_boolean (value));
            break;

        case PROP_IS_INCONSISTENT:
            donna_image_menu_item_set_is_inconsistent (item,
                    g_value_get_boolean (value));
            break;

        case PROP_IS_COMBINED:
            donna_image_menu_item_set_is_combined (item,
                    g_value_get_boolean (value));
            break;

        case PROP_IS_COMBINED_SENSITIVE:
            donna_image_menu_item_set_is_combined_sensitive (item,
                    g_value_get_boolean (value));
            break;

        case PROP_IS_LABEL_BOLD:
            donna_image_menu_item_set_is_label_bold (item,
                    g_value_get_boolean (value));
            break;

        case PROP_IS_LABEL_MARKUP:
            donna_image_menu_item_set_is_label_markup (item,
                    g_value_get_boolean (value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
donna_image_menu_item_get_property (GObject        *object,
                                    guint           prop_id,
                                    GValue         *value,
                                    GParamSpec     *pspec)
{
    DonnaImageMenuItemPrivate *priv = ((DonnaImageMenuItem *) object)->priv;

    switch (prop_id)
    {
        case PROP_IMAGE:
            g_value_set_object (value, priv->image);
            break;

        case PROP_IMAGE_SELECTED:
            g_value_set_object (value, priv->image_sel);
            break;

        case PROP_IMAGE_SPECIAL:
            g_value_set_uint (value, priv->image_special);

        case PROP_IS_ACTIVE:
            g_value_set_boolean (value, priv->is_active);
            break;

        case PROP_IS_INCONSISTENT:
            g_value_set_boolean (value, priv->is_inconsistent);
            break;

        case PROP_IS_COMBINED:
            g_value_set_boolean (value, priv->is_combined);
            break;

        case PROP_IS_COMBINED_SENSITIVE:
            g_value_set_boolean (value, priv->is_combined_sensitive);
            break;

        case PROP_IS_LABEL_BOLD:
            g_value_set_boolean (value, priv->is_label_bold);
            break;

        case PROP_IS_LABEL_MARKUP:
            g_value_set_boolean (value, priv->is_label_markup);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
donna_image_menu_item_map (GtkWidget *widget)
{
    DonnaImageMenuItemPrivate *priv = ((DonnaImageMenuItem *) widget)->priv;

    if (priv->image_special == DONNA_IMAGE_MENU_ITEM_IS_IMAGE && priv->image)
        g_object_set (priv->image, "visible", TRUE, NULL);

    ((GtkWidgetClass *) donna_image_menu_item_parent_class)->map (widget);
}

static void
donna_image_menu_item_destroy (GtkWidget *widget)
{
    DonnaImageMenuItemPrivate *priv = ((DonnaImageMenuItem *) widget)->priv;

    if (priv->image)
        gtk_container_remove ((GtkContainer *) widget, priv->image);

    ((GtkWidgetClass *) donna_image_menu_item_parent_class)->destroy (widget);
}

static void
donna_image_menu_item_toggle_size_request (GtkMenuItem *menu_item,
                                           gint        *requisition)
{
    DonnaImageMenuItemPrivate *priv = ((DonnaImageMenuItem *) menu_item)->priv;

    *requisition = 0;

    if (priv->image_special == DONNA_IMAGE_MENU_ITEM_IS_IMAGE)
    {
        if (priv->image)
        {
            GtkWidget *widget = (GtkWidget *) menu_item;
            GtkWidget *parent;
            GtkPackDirection pack_dir;
            GtkRequisition image_requisition;
            gint toggle_spacing;

            parent = gtk_widget_get_parent (widget);
            if (GTK_IS_MENU_BAR (parent))
                pack_dir = gtk_menu_bar_get_child_pack_direction ((GtkMenuBar *) parent);
            else
                pack_dir = GTK_PACK_DIRECTION_LTR;

            gtk_widget_get_preferred_size (priv->image, &image_requisition, NULL);
            gtk_widget_style_get (widget, "toggle-spacing", &toggle_spacing, NULL);

            if (pack_dir == GTK_PACK_DIRECTION_LTR || pack_dir == GTK_PACK_DIRECTION_RTL)
            {
                if (image_requisition.width > 0)
                    *requisition = image_requisition.width + toggle_spacing;
            }
            else
            {
                if (image_requisition.height > 0)
                    *requisition = image_requisition.height + toggle_spacing;
            }
        }
    }
    else
    {
        gint toggle_spacing;
        gint indicator_size;

        gtk_widget_style_get ((GtkWidget *) menu_item,
                "toggle-spacing",   &toggle_spacing,
                "indicator-size",   &indicator_size,
                NULL);

        *requisition = indicator_size + toggle_spacing;
    }
}

static void
donna_image_menu_item_toggle_size_allocate (GtkMenuItem    *menu_item,
                                            gint            allocation)
{
    ((DonnaImageMenuItem *) menu_item)->priv->toggle_size = allocation;
    ((GtkMenuItemClass *) donna_image_menu_item_parent_class)->
        toggle_size_allocate (menu_item, allocation);
}

static void
donna_image_menu_item_get_preferred_width (GtkWidget        *widget,
                                           gint             *minimum,
                                           gint             *natural)
{
    DonnaImageMenuItemPrivate *priv = ((DonnaImageMenuItem *) widget)->priv;

    ((GtkWidgetClass *) donna_image_menu_item_parent_class)->
        get_preferred_width (widget, minimum, natural);

    if (priv->image_special == DONNA_IMAGE_MENU_ITEM_IS_IMAGE && priv->image)
    {
        GtkPackDirection pack_dir;
        GtkWidget *parent;

        parent = gtk_widget_get_parent (widget);
        if (GTK_IS_MENU_BAR (parent))
            pack_dir = gtk_menu_bar_get_child_pack_direction ((GtkMenuBar *) parent);
        else
            pack_dir = GTK_PACK_DIRECTION_LTR;

        if (pack_dir == GTK_PACK_DIRECTION_TTB || pack_dir == GTK_PACK_DIRECTION_BTT)
        {
            gint child_minimum, child_natural;

            gtk_widget_get_preferred_width (priv->image, &child_minimum, &child_natural);
            *minimum = MAX (*minimum, child_minimum);
            *natural = MAX (*natural, child_natural);
        }
    }
}

static void
donna_image_menu_item_get_preferred_height (GtkWidget        *widget,
                                            gint             *minimum,
                                            gint             *natural)
{
    DonnaImageMenuItemPrivate *priv = ((DonnaImageMenuItem *) widget)->priv;

    ((GtkWidgetClass *) donna_image_menu_item_parent_class)->
        get_preferred_height (widget, minimum, natural);

    if (priv->image_special == DONNA_IMAGE_MENU_ITEM_IS_IMAGE && priv->image)
    {
        GtkPackDirection pack_dir;
        GtkWidget *parent;

        parent = gtk_widget_get_parent (widget);
        if (GTK_IS_MENU_BAR (parent))
            pack_dir = gtk_menu_bar_get_child_pack_direction ((GtkMenuBar *) parent);
        else
            pack_dir = GTK_PACK_DIRECTION_LTR;

        if (pack_dir == GTK_PACK_DIRECTION_RTL || pack_dir == GTK_PACK_DIRECTION_LTR)
        {
            GtkRequisition child_requisition;

            gtk_widget_get_preferred_size (priv->image, &child_requisition, NULL);
            *minimum = MAX (*minimum, child_requisition.height);
            *natural = MAX (*natural, child_requisition.height);
        }
    }
}

static void
donna_image_menu_item_get_preferred_height_for_width (GtkWidget        *widget,
                                                      gint              width,
                                                      gint             *minimum,
                                                      gint             *natural)
{
    DonnaImageMenuItemPrivate *priv = ((DonnaImageMenuItem *) widget)->priv;

    ((GtkWidgetClass *) donna_image_menu_item_parent_class)->
        get_preferred_height_for_width (widget, width, minimum, natural);

    if (priv->image_special == DONNA_IMAGE_MENU_ITEM_IS_IMAGE && priv->image)
    {
        GtkPackDirection pack_dir;
        GtkWidget *parent;

        parent = gtk_widget_get_parent (widget);
        if (GTK_IS_MENU_BAR (parent))
            pack_dir = gtk_menu_bar_get_child_pack_direction ((GtkMenuBar *) parent);
        else
            pack_dir = GTK_PACK_DIRECTION_LTR;

        if (pack_dir == GTK_PACK_DIRECTION_RTL || pack_dir == GTK_PACK_DIRECTION_LTR)
        {
            GtkRequisition child_requisition;

            gtk_widget_get_preferred_size (priv->image, &child_requisition, NULL);
            *minimum = MAX (*minimum, child_requisition.height);
            *natural = MAX (*natural, child_requisition.height);
        }
    }
}


static void
donna_image_menu_item_size_allocate (GtkWidget     *widget,
                                     GtkAllocation *allocation)
{
    DonnaImageMenuItemPrivate *priv = ((DonnaImageMenuItem *) widget)->priv;

    ((GtkWidgetClass *) donna_image_menu_item_parent_class)->
        size_allocate (widget, allocation);

    if (priv->image_special == DONNA_IMAGE_MENU_ITEM_IS_IMAGE && priv->image)
    {
        GtkAllocation widget_allocation;
        GtkPackDirection pack_dir;
        GtkWidget *parent;
        gint x, y, offset;
        GtkStyleContext *context;
        GtkStateFlags state;
        GtkBorder padding;
        GtkRequisition child_requisition;
        GtkAllocation child_allocation;
        gint horizontal_padding, toggle_spacing;

        parent = gtk_widget_get_parent (widget);
        if (GTK_IS_MENU_BAR (parent))
            pack_dir = gtk_menu_bar_get_child_pack_direction ((GtkMenuBar *) parent);
        else
            pack_dir = GTK_PACK_DIRECTION_LTR;

        gtk_widget_style_get (widget,
                "horizontal-padding",   &horizontal_padding,
                "toggle-spacing",       &toggle_spacing,
                NULL);

        gtk_widget_get_preferred_size (priv->image, &child_requisition, NULL);

        gtk_widget_get_allocation (widget, &widget_allocation);

        context = gtk_widget_get_style_context (widget);
        state = gtk_widget_get_state_flags (widget);
        gtk_style_context_get_padding (context, state, &padding);
        offset = (gint) gtk_container_get_border_width ((GtkContainer *) widget);

        if (pack_dir == GTK_PACK_DIRECTION_LTR || pack_dir == GTK_PACK_DIRECTION_RTL)
        {
            if ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) ==
                    (pack_dir == GTK_PACK_DIRECTION_LTR))
                x = offset + horizontal_padding + padding.left
                    + (priv->toggle_size - toggle_spacing - child_requisition.width) / 2;
            else
                x = widget_allocation.width - offset - horizontal_padding
                    - padding.right - priv->toggle_size + toggle_spacing
                    + (priv->toggle_size - toggle_spacing - child_requisition.width) / 2;

            y = (widget_allocation.height - child_requisition.height) / 2;
        }
        else
        {
            if ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) ==
                    (pack_dir == GTK_PACK_DIRECTION_TTB))
                y = offset + horizontal_padding + padding.top
                    + (priv->toggle_size - toggle_spacing - child_requisition.height) / 2;
            else
                y = widget_allocation.height - offset - horizontal_padding
                    - padding.bottom - priv->toggle_size + toggle_spacing
                    + (priv->toggle_size - toggle_spacing - child_requisition.height) / 2;

            x = (widget_allocation.width - child_requisition.width) / 2;
        }

        child_allocation.width = child_requisition.width;
        child_allocation.height = child_requisition.height;
        child_allocation.x = widget_allocation.x + MAX (x, 0);
        child_allocation.y = widget_allocation.y + MAX (y, 0);

        gtk_widget_size_allocate (priv->image, &child_allocation);
    }
}

static void
donna_image_menu_item_forall (GtkContainer   *container,
                              gboolean        include_internals,
                              GtkCallback     callback,
                              gpointer        data)
{
    DonnaImageMenuItemPrivate *priv = ((DonnaImageMenuItem *) container)->priv;

    ((GtkContainerClass *) donna_image_menu_item_parent_class)->
        forall (container, include_internals, callback, data);

    if (include_internals && priv->image_special == DONNA_IMAGE_MENU_ITEM_IS_IMAGE
            && priv->image)
        (*callback) (priv->image, data);
}

static void
donna_image_menu_item_remove (GtkContainer *container,
                              GtkWidget    *child)
{
    DonnaImageMenuItemPrivate *priv = ((DonnaImageMenuItem *) container)->priv;

    if (child == priv->image)
    {
        gtk_widget_unparent (child);
        priv->image = NULL;

        if (gtk_widget_get_visible ((GtkWidget *) container))
            gtk_widget_queue_resize ((GtkWidget *) container);

        g_object_notify ((GObject *) container, "image");
    }
    else
        ((GtkContainerClass *) donna_image_menu_item_parent_class)->
            remove (container, child);
}

/* ***************** */

static void
popdown_menu (GtkWidget *item)
{
    GtkWidget *parent = gtk_widget_get_parent (item);

    /* locate top parent shell, e.g. main/first menu that was poped up */
    for (;;)
    {
        GtkWidget *w = gtk_menu_shell_get_parent_shell ((GtkMenuShell *) parent);
        if (w)
            parent = w;
        else
            break;
    }

    /* this will hide all menus */
    gtk_menu_popdown ((GtkMenu *) parent);
    /* we use this signal to unref the menu when it wasn't packed nowhere */
    g_signal_emit_by_name (parent, "deactivate");
}

static gboolean
menu_item_release_cb (GtkWidget             *widget,
                      GdkEventButton        *event,
                      DonnaImageMenuItem    *item)
{
    DonnaImageMenuItemPrivate *priv = item->priv;

    /* doesn't get triggered if there's a submenu, so we know we don't have one
     * yet */

    if (!priv->is_combined || !priv->is_combined_sensitive)
        return FALSE;

    if (event->x <= priv->item_width)
    {
        /* on the item part, make sure an event gets out & close the menu. We
         * "emit" the event blocking this handler instead of just returning
         * FALSE/letting it go through because our popdown_menu() will must
         * likely destroy the menu, and we need the item to still exists so the
         * click can be processed */
        g_signal_handler_block (widget, priv->sid_button_release);
        gtk_widget_event (widget, (GdkEvent *) event);
        g_signal_handler_unblock (widget, priv->sid_button_release);
        popdown_menu (widget);
        return TRUE;
    }
    else
        g_signal_emit (item, donna_image_menu_item_signals[SIGNAL_LOAD_SUBMENU], 0,
                TRUE);

    return TRUE;
}

static gboolean
parent_button_release_cb (GtkWidget             *parent _UNUSED_,
                          GdkEventButton        *event,
                          DonnaImageMenuItem    *item)
{
    DonnaImageMenuItemPrivate *priv = item->priv;
    GtkWidget *w;

    if (!priv->is_combined || !priv->is_combined_sensitive)
        return FALSE;

    w = gtk_get_event_widget ((GdkEvent *) event);
    while (w && !GTK_IS_MENU_ITEM (w))
        w = gtk_widget_get_parent (w);

    if (!w || w != (GtkWidget *) item)
        return FALSE;

    if (event->x <= priv->item_width
            && gtk_menu_item_get_submenu ((GtkMenuItem *) item))
    {
        /* on the item part while a submenu was attached. So we close the
         * clicked menu, and make sure an event gets out */
        g_signal_handler_block (w, priv->sid_button_release);
        gtk_widget_event (w, (GdkEvent *) event);
        g_signal_handler_unblock (w, priv->sid_button_release);
        popdown_menu (w);
    }

    return FALSE;
}

static void
donna_image_menu_item_parent_set (GtkWidget          *widget,
                                  GtkWidget          *old_parent)
{
    DonnaImageMenuItemPrivate *priv = ((DonnaImageMenuItem *) widget)->priv;
    GtkWidget *parent;

    if (priv->sid_parent_button_release)
        g_signal_handler_disconnect (old_parent, priv->sid_parent_button_release);

    parent = gtk_widget_get_parent (widget);
    if (G_LIKELY (parent && GTK_IS_MENU_SHELL (parent)))
        priv->sid_parent_button_release = g_signal_connect (parent,
                "button-release-event",
                (GCallback) parent_button_release_cb, widget);
    else
        priv->sid_parent_button_release = 0;

    ((GtkWidgetClass *) donna_image_menu_item_parent_class)
        ->parent_set (widget, old_parent);
}

static gboolean
delayed_emit (DonnaImageMenuItem *item)
{
    DonnaImageMenuItemPrivate *priv = item->priv;

    g_signal_emit (item, donna_image_menu_item_signals[SIGNAL_LOAD_SUBMENU], 0,
            FALSE);

    priv->sid_timeout = 0;
    return FALSE;
}

static void
donna_image_menu_item_select (GtkMenuItem        *menuitem)
{
    DonnaImageMenuItemPrivate *priv = ((DonnaImageMenuItem *) menuitem)->priv;

    if (priv->image_special == DONNA_IMAGE_MENU_ITEM_IS_IMAGE && priv->image_sel)
    {
        if (priv->image)
            priv->image_org = g_object_ref (priv->image);
        donna_image_menu_item_set_image ((DonnaImageMenuItem *) menuitem, priv->image_sel);
    }

    if (!priv->is_combined || gtk_menu_item_get_submenu (menuitem))
        goto chain;

    if (!priv->sid_timeout)
    {
        gint delay;

        g_object_get (gtk_widget_get_settings ((GtkWidget *) menuitem),
                "gtk-menu-popup-delay", &delay,
                NULL);
        if (delay > 0)
            priv->sid_timeout = g_timeout_add ((guint) delay,
                    (GSourceFunc) delayed_emit, (GtkWidget *) menuitem);
        else
            g_signal_emit (menuitem,
                    donna_image_menu_item_signals[SIGNAL_LOAD_SUBMENU], 0,
                    FALSE);
    }

chain:
    ((GtkMenuItemClass *) donna_image_menu_item_parent_class)->select (menuitem);
}

static void
donna_image_menu_item_deselect (GtkMenuItem          *menuitem)
{
    DonnaImageMenuItemPrivate *priv = ((DonnaImageMenuItem *) menuitem)->priv;

    if (priv->image_special == DONNA_IMAGE_MENU_ITEM_IS_IMAGE && priv->image_sel)
    {
        donna_image_menu_item_set_image ((DonnaImageMenuItem *) menuitem, priv->image_org);
        g_object_unref (priv->image_org);
        priv->image_org = NULL;
    }

    if (priv->sid_timeout)
    {
        g_source_remove (priv->sid_timeout);
        priv->sid_timeout = 0;
    }

    ((GtkMenuItemClass *) donna_image_menu_item_parent_class)->deselect (menuitem);
}

static void
donna_image_menu_item_set_label (GtkMenuItem    *item,
                                 const gchar    *label)
{
    GtkWidget *child;

    child = gtk_bin_get_child ((GtkBin *) item);
    if (!child)
    {
        child = gtk_label_new (NULL);
        gtk_widget_set_halign (child, GTK_ALIGN_START);
        gtk_widget_set_valign (child, GTK_ALIGN_CENTER);
        gtk_container_add ((GtkContainer *) item, child);
        gtk_widget_show (child);
    }

    if (((DonnaImageMenuItem *) item)->priv->is_label_markup)
        gtk_label_set_markup ((GtkLabel *) child, (label) ? label : "");
    else
        gtk_label_set_label ((GtkLabel *) child, (label) ? label : "");
    g_object_notify ((GObject *) item, "label");
}

static const gchar * donna_image_menu_item_get_label (GtkMenuItem    *item)
{
    GtkWidget *child;

    child = gtk_bin_get_child ((GtkBin *) item);
    return (child) ? gtk_label_get_label ((GtkLabel *) child) : NULL;
}

/* the following is a copy/paste from gtkmenuitem.c
 *
 * gtkmenuitem.c
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Modified by the GTK+ Team and others 1997-2000.
 * */
static void
get_arrow_size (GtkWidget *widget,
                GtkWidget *child,
                gint      *size,
                gint      *spacing)
{
    PangoContext     *context;
    PangoFontMetrics *metrics;
    gfloat            arrow_scaling;
    gint              arrow_spacing;

    g_assert (size);

    gtk_widget_style_get (widget,
            "arrow-scaling", &arrow_scaling,
            "arrow-spacing", &arrow_spacing,
            NULL);

    if (spacing != NULL)
        *spacing = arrow_spacing;

    context = gtk_widget_get_pango_context (child);

    metrics = pango_context_get_metrics (context,
            pango_context_get_font_description (context),
            pango_context_get_language (context));

    *size = (PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) +
                pango_font_metrics_get_descent (metrics)));

    pango_font_metrics_unref (metrics);

    *size = (gint) ((gfloat) *size * arrow_scaling);
}

struct draw
{
    GtkContainer *container;
    cairo_t *cr;
    gboolean is_sensitive;
};

static void
draw_child (GtkWidget *widget, struct draw *data)
{
    gtk_widget_set_sensitive (widget, data->is_sensitive);
    gtk_container_propagate_draw (data->container, widget, data->cr);
}

/* the following is pretty much a copy/paste from gtkmenuitem.c, except for the
 * is_combined bit, and check/radio indicator.
 *
 * gtkmenuitem.c
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Modified by the GTK+ Team and others 1997-2000.
 * */
static gboolean
donna_image_menu_item_draw (GtkWidget          *widget,
                            cairo_t            *cr)
{
    DonnaImageMenuItemPrivate *priv = ((DonnaImageMenuItem *) widget)->priv;
    GtkStateFlags state;
    GtkStyleContext *context;
    GtkBorder padding;
    GtkWidget *child, *parent;
    gdouble x, y, w, h, width, height;
    guint border_width;
    gint arrow_size;
    struct draw data;

    border_width = gtk_container_get_border_width ((GtkContainer *) widget);
    state = gtk_widget_get_state_flags (widget);
    context = gtk_widget_get_style_context (widget);
    width = gtk_widget_get_allocated_width (widget);
    height = gtk_widget_get_allocated_height (widget);

    x = border_width;
    y = border_width;
    w = width - border_width * 2;
    h = height - border_width * 2;

    child = gtk_bin_get_child (GTK_BIN (widget));
    parent = gtk_widget_get_parent (widget);

    gtk_style_context_get_padding (context, state, &padding);

    if (priv->is_combined)
    {
        get_arrow_size (widget, child, &arrow_size, NULL);

        /* item width == the first highlight rectangle, which excludes the arrow
         * and some padding: 1 padding right when the arrow is drawn, 1 padding
         * left we add before its own rectangle, and another 1 padding right
         * used as non-highlighted separation between the two */
        priv->item_width = w - arrow_size - 2 * padding.right - padding.left;

        if (priv->is_combined_sensitive)
        {
            gtk_render_background (context, cr, x, y, priv->item_width, h);
            gtk_render_frame (context, cr, x, y, priv->item_width, h);
        }

        gtk_render_background (context, cr,
                x + priv->item_width + padding.right, y,
                padding.left + arrow_size + padding.right, h);
        gtk_render_frame (context, cr,
                x + priv->item_width + padding.right, y,
                padding.left + arrow_size + padding.right, h);
    }
    else
    {
        gtk_render_background (context, cr, x, y, w, h);
        gtk_render_frame (context, cr, x, y, w, h);
    }

    if (priv->is_combined
            || (gtk_menu_item_get_submenu ((GtkMenuItem *) widget)
                && !GTK_IS_MENU_BAR (parent)))
    {
        gdouble arrow_x, arrow_y;
        GtkTextDirection direction;
        gdouble angle;

        if (!priv->is_combined)
            get_arrow_size (widget, child, &arrow_size, NULL);

        direction = gtk_widget_get_direction (widget);

        if (direction == GTK_TEXT_DIR_LTR)
        {
            arrow_x = x + w - arrow_size - padding.right;
            angle = G_PI / 2;
        }
        else
        {
            arrow_x = x + padding.left;
            angle = (3 * G_PI) / 2;
        }

        arrow_y = y + (h - arrow_size) / 2;

        gtk_render_arrow (context, cr, angle, arrow_x, arrow_y, arrow_size);
    }
    else if (!child)
    {
        gboolean wide_separators;
        gint     separator_height;

        gtk_widget_style_get (widget,
                "wide-separators",    &wide_separators,
                "separator-height",   &separator_height,
                NULL);
        if (wide_separators)
            gtk_render_frame (context, cr,
                    x + padding.left,
                    y + padding.top,
                    w - padding.left - padding.right,
                    separator_height);
        else
            gtk_render_line (context, cr,
                    x + padding.left,
                    y + padding.top,
                    x + w - padding.right - 1,
                    y + padding.top);
    }

    if (priv->image_special != DONNA_IMAGE_MENU_ITEM_IS_IMAGE)
    {
        guint toggle_spacing;
        guint horizontal_padding;
        guint indicator_size;
        guint offset;

        gtk_widget_style_get (widget,
                "toggle-spacing",       &toggle_spacing,
                "horizontal-padding",   &horizontal_padding,
                "indicator-size",       &indicator_size,
                NULL);

        offset = border_width + (guint16) padding.left + 2;

        if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
            x = offset + horizontal_padding
                + ((guint) priv->toggle_size - toggle_spacing - indicator_size) / 2;
        else
            x = width - offset - horizontal_padding - priv->toggle_size + toggle_spacing
                + ((guint) priv->toggle_size - toggle_spacing - indicator_size) / 2;
        y = (height - indicator_size) / 2;

        gtk_style_context_save (context);

        if (priv->image_special == DONNA_IMAGE_MENU_ITEM_IS_CHECK
                && priv->is_inconsistent)
            state |= GTK_STATE_FLAG_INCONSISTENT;
        else if (priv->is_active)
            state |= GTK_STATE_FLAG_ACTIVE;

        gtk_style_context_set_state (context, state);

        if (priv->image_special == DONNA_IMAGE_MENU_ITEM_IS_CHECK)
        {
            gtk_style_context_add_class (context, GTK_STYLE_CLASS_CHECK);
            gtk_render_check (context, cr, x, y, indicator_size, indicator_size);
        }
        else
        {
            gtk_style_context_add_class (context, GTK_STYLE_CLASS_RADIO);
            gtk_render_option (context, cr, x, y, indicator_size, indicator_size);
        }

        gtk_style_context_restore (context);
    }

    /* we don't chain up because then the background is done over ours, so we
     * just call propagate_draw on all children */
    data.container = (GtkContainer *) widget;
    data.cr = cr;
    data.is_sensitive = priv->is_combined_sensitive;
    gtk_container_forall ((GtkContainer *) widget, (GtkCallback) draw_child, &data);

    return FALSE;
}

/* ***************** */

/**
 * donna_image_menu_item_new_with_label:
 * @label: Text of the menu item
 *
 * Creates a new #DonnaImageMenuItem using @label
 *
 * Returns: a new #DonnaImageMenuItem
 */
GtkWidget *
donna_image_menu_item_new_with_label (const gchar *label)
{
    return g_object_new (DONNA_TYPE_IMAGE_MENU_ITEM, "label", label, NULL);
}

/**
 * donna_image_menu_item_set_image:
 * @item: #DonnaImageMenuItem to set the image of
 * @image: (allow-none): a #GtkWidget to set as the image for the menu item
 *
 * Sets the image of @item to the given widget
 *
 * Note that is will not be used if :image-special was set to something other
 * than %DONNA_IMAGE_MENU_ITEM_IS_IMAGE; see
 * donna_image_menu_item_set_image_special()
 */
void
donna_image_menu_item_set_image (DonnaImageMenuItem *item,
                                 GtkWidget          *image)
{
    DonnaImageMenuItemPrivate *priv;

    g_return_if_fail (DONNA_IS_IMAGE_MENU_ITEM (item));
    priv = item->priv;

    if (image == priv->image)
        return;

    if (priv->image)
        gtk_container_remove ((GtkContainer *) item, priv->image);

    priv->image = image;

    if (image != NULL)
    {
        gtk_widget_set_parent (image, (GtkWidget *) item);
        g_object_set (image, "visible", TRUE, "no-show-all", TRUE, NULL);
    }

    g_object_notify ((GObject *) item, "image");
}

/**
 * donna_image_menu_item_get_image:
 * @item: a #DonnaImageMenuItem
 *
 * Gets the widget that is currently set as the image of @item
 * See donna_image_menu_item_set_image()
 *
 * Returns: (transfer none): the widget set as image of @item
 **/
GtkWidget *
donna_image_menu_item_get_image (DonnaImageMenuItem *item)
{
    g_return_val_if_fail (DONNA_IS_IMAGE_MENU_ITEM (item), NULL);
    return item->priv->image;
}

/**
 * donna_image_menu_item_set_image_selected:
 * @item: #DonnaImageMenuItem to set the image-selected of
 * @image: (allow-none): a #GtkWidget to set as the image-selected for the menu
 * item
 *
 * Sets the image shown when @item is selected to the given widget
 *
 * Note that is will not be used if :image-special was set to something other
 * than %DONNA_IMAGE_MENU_ITEM_IS_IMAGE; see
 * donna_image_menu_item_set_image_special()
 */
void
donna_image_menu_item_set_image_selected (DonnaImageMenuItem *item,
                                          GtkWidget          *image)
{
    DonnaImageMenuItemPrivate *priv;

    g_return_if_fail (DONNA_IS_IMAGE_MENU_ITEM (item));
    priv = item->priv;

    if (image == priv->image_sel)
        return;

    if (priv->image_sel)
        g_object_unref (priv->image_sel);

    priv->image_sel = image;

    if (image != NULL)
        g_object_ref_sink (image);

    g_object_notify ((GObject *) item, "image-selected");
}

/**
 * donna_image_menu_item_get_image_selected:
 * @item: a #DonnaImageMenuItem
 *
 * Gets the widget that is currently set as the image of @item when it is
 * selected. See donna_image_menu_item_set_image_selected()
 *
 * Returns: (transfer none): the widget set as image-selected of @item
 **/
GtkWidget *
donna_image_menu_item_get_image_selected (DonnaImageMenuItem *item)
{
    g_return_val_if_fail (DONNA_IS_IMAGE_MENU_ITEM (item), NULL);
    return item->priv->image_sel;
}

/**
 * donna_image_menu_item_set_image_special:
 * @item: #DonnaImageMenuItem to set the image-selected of
 * @image: Which type of image will be used on the menu item
 *
 * Sets which type of image will be used on the menu item. It can be a special
 * rendering for check box or radio option, or whatever image was set using
 * donna_image_menu_item_set_image()
 */
void
donna_image_menu_item_set_image_special (DonnaImageMenuItem             *item,
                                         DonnaImageMenuItemImageSpecial  image)
{
    g_return_if_fail (DONNA_IS_IMAGE_MENU_ITEM (item));
    g_return_if_fail (image >= DONNA_IMAGE_MENU_ITEM_IS_IMAGE
            && image < _DONNA_IMAGE_MENU_ITEM_NB_IMAGE_SPECIAL);

    item->priv->image_special = image;
    g_object_notify ((GObject *) item, "image-special");
}

/**
 * donna_image_menu_item_get_image_special:
 * @item: a #DonnaImageMenuItem
 *
 * Returns which type of image is used on the menu item. Note that
 * %DONNA_IMAGE_MENU_ITEM_IS_IMAGE doesn't mean an image will be featured, see
 * donna_image_menu_item_get_image() to see which, if any, will actually be
 * used.
 *
 * Returns: Which type of image is used
 **/
DonnaImageMenuItemImageSpecial
donna_image_menu_item_get_image_special (DonnaImageMenuItem *item)
{
    g_return_val_if_fail (DONNA_IS_IMAGE_MENU_ITEM (item), 0);
    return item->priv->image_special;
}

/**
 * donna_image_menu_item_set_is_active:
 * @item: a #DonnaImageMenuItem
 * @is_active: Whether @item is active/checked or not
 *
 * Sets whether the menu item will show an active/checked check box/radio option
 * or not. This obviously only applies if :image-special is set to either
 * %DONNA_IMAGE_MENU_ITEM_IS_CHECK or %DONNA_IMAGE_MENU_ITEM_IS_RADIO
 */
void
donna_image_menu_item_set_is_active (DonnaImageMenuItem *item,
                                     gboolean            is_active)
{
    DonnaImageMenuItemPrivate *priv;

    g_return_if_fail (DONNA_IS_IMAGE_MENU_ITEM (item));
    priv = item->priv;

    if (priv->is_active != is_active)
    {
        priv->is_active = (is_active) ? 1 : 0;
        g_object_notify ((GObject *) item, "is-active");
    }
}

/**
 * donna_image_menu_item_get_is_active:
 * @item: a #DonnaImageMenuItem
 *
 * This only applies if :image-special is set to either
 * %DONNA_IMAGE_MENU_ITEM_IS_CHECK or %DONNA_IMAGE_MENU_ITEM_IS_RADIO
 *
 * Returns: Whether @item is active/checked or not
 */
gboolean
donna_image_menu_item_get_is_active (DonnaImageMenuItem *item)
{
    g_return_val_if_fail (DONNA_IS_IMAGE_MENU_ITEM (item), FALSE);
    return item->priv->is_active;
}

/**
 * donna_image_menu_item_set_is_inconsistent:
 * @item: a #DonnaImageMenuItem
 * @is_inconsistent: Whether the check box of @item is in an inconsistent state
 * or not
 *
 * Sets whether the menu item will show a check box in an inconsistent state or
 * not. This takes precedence over :is-active and will obviously only apply if
 * :image-special is set to either %DONNA_IMAGE_MENU_ITEM_IS_CHECK or
 * %DONNA_IMAGE_MENU_ITEM_IS_RADIO
 */
void
donna_image_menu_item_set_is_inconsistent (DonnaImageMenuItem *item,
                                           gboolean            is_inconsistent)
{
    DonnaImageMenuItemPrivate *priv;

    g_return_if_fail (DONNA_IS_IMAGE_MENU_ITEM (item));
    priv = item->priv;

    if (priv->is_inconsistent != is_inconsistent)
    {
        priv->is_inconsistent = (is_inconsistent) ? 1 : 0;
        g_object_notify ((GObject *) item, "is-inconsistent");
    }
}

/**
 * donna_image_menu_item_get_is_inconsistent:
 * @item: a #DonnaImageMenuItem
 *
 * This only applies if :image-special is set to %DONNA_IMAGE_MENU_ITEM_IS_CHECK
 *
 * Returns: Whether the check box of @item is in an inconsistent state or not
 */
gboolean
donna_image_menu_item_get_is_inconsistent (DonnaImageMenuItem *item)
{
    g_return_val_if_fail (DONNA_IS_IMAGE_MENU_ITEM (item), FALSE);
    return item->priv->is_inconsistent;
}

/**
 * donna_image_menu_item_set_is_combined:
 * @item: a #DonnaImageMenuItem
 * @is_combined: Whether @item should combine an item and a submenu or not
 *
 * Sets whether the menu item will be both an item (that can be clicked) as well
 * as a submenu, or not (i.e. either one or the other)
 */
void
donna_image_menu_item_set_is_combined (DonnaImageMenuItem *item,
                                       gboolean            is_combined)
{
    DonnaImageMenuItemPrivate *priv;

    g_return_if_fail (DONNA_IS_IMAGE_MENU_ITEM (item));
    priv = item->priv;

    if (priv->is_combined != is_combined)
    {
        priv->is_combined = (is_combined) ? 1 : 0;
        gtk_menu_item_set_reserve_indicator ((GtkMenuItem *) item, priv->is_combined);
        g_object_notify ((GObject *) item, "is-combined");
    }
}

/**
 * donna_image_menu_item_get_is_combined:
 * @item: a #DonnaImageMenuItem
 *
 * Returns: Whether @item combines an item and a submenu or not
 */
gboolean
donna_image_menu_item_get_is_combined (DonnaImageMenuItem *item)
{
    g_return_val_if_fail (DONNA_IS_IMAGE_MENU_ITEM (item), FALSE);
    return item->priv->is_combined;
}

/**
 * donna_image_menu_item_set_is_combined_sensitive:
 * @item: a #DonnaImageMenuItem
 * @is_combined_sensitive: Whether the action part of @item is sensitive or not
 *
 * Sets whether the item (i.e. clickable) part of the item is sensitive or not.
 * This allows to have it non-sensitive (i.e. disabled) while keeping the
 * submenu functionnal.
 * Obviously, only works if @item combines both an item & a submenu, see
 * donna_image_menu_item_set_is_combined()
 */
void
donna_image_menu_item_set_is_combined_sensitive (DonnaImageMenuItem *item,
                                                 gboolean            is_combined_sensitive)
{
    DonnaImageMenuItemPrivate *priv;

    g_return_if_fail (DONNA_IS_IMAGE_MENU_ITEM (item));
    priv = item->priv;

    if (priv->is_combined_sensitive != is_combined_sensitive)
    {
        priv->is_combined_sensitive = (is_combined_sensitive) ? 1 : 0;
        g_object_notify ((GObject *) item, "is-combined-sensitive");
    }
}

/**
 * donna_image_menu_item_get_is_combined_sensitive:
 * @item: a #DonnaImageMenuItem
 *
 * Returns whether the item/clickable part of @item is sensitive or not
 *
 * Returns: %TRUE if @item can be clicked, else %FALSE
 */
gboolean
donna_image_menu_item_get_is_combined_sensitive (DonnaImageMenuItem *item)
{
    g_return_val_if_fail (DONNA_IS_IMAGE_MENU_ITEM (item), FALSE);
    return item->priv->is_combined_sensitive;
}

/**
 * donna_image_menu_item_set_is_label_bold:
 * @item: a #DonnaImageMenuItem
 * @is_bold: Whether the label should be bold or not
 *
 * Sets whether the label of @item should be written in bold or not
 */
void
donna_image_menu_item_set_is_label_bold (DonnaImageMenuItem *item,
                                         gboolean            is_bold)
{
    DonnaImageMenuItemPrivate *priv;

    g_return_if_fail (DONNA_IS_IMAGE_MENU_ITEM (item));
    priv = item->priv;

    if (priv->is_label_bold != is_bold)
    {
        GtkWidget *child;

        priv->is_label_bold = (is_bold) ? 1 : 0;
        child = gtk_bin_get_child ((GtkBin *) item);
        if (GTK_IS_LABEL (child))
        {
            PangoAttrList *attrs;

            attrs = pango_attr_list_new ();
            if (is_bold)
                pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
            gtk_label_set_attributes ((GtkLabel *) child, attrs);
            pango_attr_list_unref (attrs);
        }
        else
            g_warning ("ImageMenuItem: Cannot set label bold, child isn't a GtkLabel");

        g_object_notify ((GObject *) item, "is-label-bold");
    }
}

/**
 * donna_image_menu_item_get_is_label_bold:
 * @item: a #DonnaImageMenuItem
 *
 * Returns whether the label of @item is written in bold or not
 *
 * Returns: %TRUE is @item's label is in bold, else %FALSE
 */
gboolean
donna_image_menu_item_get_is_label_bold (DonnaImageMenuItem *item)
{
    g_return_val_if_fail (DONNA_IS_IMAGE_MENU_ITEM (item), FALSE);
    return item->priv->is_label_bold;
}

/**
 * donna_image_menu_item_set_is_label_markup:
 * @item: a #DonnaImageMenuItem
 * @is_markup: Whether the label should be parsed as markup or not
 *
 * Sets whether the label of @item should be parsed as markup or not
 */
void
donna_image_menu_item_set_is_label_markup (DonnaImageMenuItem *item,
                                           gboolean            is_markup)
{
    DonnaImageMenuItemPrivate *priv;

    g_return_if_fail (DONNA_IS_IMAGE_MENU_ITEM (item));
    priv = item->priv;

    if (priv->is_label_markup != is_markup)
    {
        GtkLabel *label;

        priv->is_label_markup = (is_markup) ? 1 : 0;
        label = (GtkLabel *) gtk_bin_get_child ((GtkBin *) item);
        if (GTK_IS_LABEL (label))
        {
            if (is_markup)
                gtk_label_set_markup (label, gtk_label_get_label (label));
            else
                gtk_label_set_text (label, gtk_label_get_label (label));
        }
        else
            g_warning ("ImageMenuItem: Cannot set label markup, child isn't a GtkLabel");

        g_object_notify ((GObject *) item, "is-label-markup");
    }
}

/**
 * donna_image_menu_item_get_is_label_markup:
 * @item: a #DonnaImageMenuItem
 *
 * Returns whether the label of @item is parsed as markup or not
 *
 * Returns: %TRUE is @item's label is parsed as markup, else %FALSE
 */
gboolean
donna_image_menu_item_get_is_label_markup (DonnaImageMenuItem *item)
{
    g_return_val_if_fail (DONNA_IS_IMAGE_MENU_ITEM (item), FALSE);
    return item->priv->is_label_markup;
}

/**
 * donna_image_menu_item_set_loading_submenu:
 * @item: a #DonnaImageMenuItem
 * @label: (allow-none): Label for the item in the submenu of @item
 *
 * Creates a new submenu for @item, containing only one non-sensitive item with
 * @label as label (or "Please wait..." if %NULL).
 * This is an helper function to be used if/when loading the actual submenu is a
 * slow operation, to give some feedback to the user.
 */
void
donna_image_menu_item_set_loading_submenu (DonnaImageMenuItem   *item,
                                           const gchar          *label)
{
    GtkMenu *menu;
    GtkWidget *w;

    g_return_if_fail (DONNA_IS_IMAGE_MENU_ITEM (item));
    g_return_if_fail (gtk_menu_item_get_submenu ((GtkMenuItem *) item) == NULL);

    menu = (GtkMenu *) gtk_menu_new ();
    w = donna_image_menu_item_new_with_label ((label) ? label : "Please wait...");
    gtk_widget_set_sensitive (w, FALSE);
    gtk_menu_attach (menu, w, 0, 1, 0, 1);
    gtk_widget_show (w);
    gtk_menu_item_set_submenu ((GtkMenuItem *) item, (GtkWidget *) menu);
    if ((GtkWidget *) item == gtk_menu_shell_get_selected_item (
                (GtkMenuShell *) gtk_widget_get_parent ((GtkWidget *) item)))
        gtk_menu_item_select ((GtkMenuItem *) item);
}
