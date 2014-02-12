/**
 * kalu - Copyright (C) 2012-2014 Olivier Brunel
 *
 * closures.h
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

#ifndef __g_cclosure_user_marshal_MARSHAL_H__
#define __g_cclosure_user_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:STRING,UINT,UINT (closures.def:1) */
extern void g_cclosure_user_marshal_VOID__STRING_UINT_UINT (GClosure     *closure,
                                                            GValue       *return_value,
                                                            guint         n_param_values,
                                                            const GValue *param_values,
                                                            gpointer      invocation_hint,
                                                            gpointer      marshal_data);

/* VOID:INT,STRING (closures.def:2) */
extern void g_cclosure_user_marshal_VOID__INT_STRING (GClosure     *closure,
                                                      GValue       *return_value,
                                                      guint         n_param_values,
                                                      const GValue *param_values,
                                                      gpointer      invocation_hint,
                                                      gpointer      marshal_data);

/* BOOLEAN:STRING (closures.def:3) */
extern void g_cclosure_user_marshal_BOOLEAN__STRING (GClosure     *closure,
                                                     GValue       *return_value,
                                                     guint         n_param_values,
                                                     const GValue *param_values,
                                                     gpointer      invocation_hint,
                                                     gpointer      marshal_data);

/* BOOLEAN:STRING,STRING,STRING,STRING (closures.def:4) */
extern void g_cclosure_user_marshal_BOOLEAN__STRING_STRING_STRING_STRING (GClosure     *closure,
                                                                          GValue       *return_value,
                                                                          guint         n_param_values,
                                                                          const GValue *param_values,
                                                                          gpointer      invocation_hint,
                                                                          gpointer      marshal_data);

/* BOOLEAN:STRING,STRING,STRING (closures.def:5) */
extern void g_cclosure_user_marshal_BOOLEAN__STRING_STRING_STRING (GClosure     *closure,
                                                                   GValue       *return_value,
                                                                   guint         n_param_values,
                                                                   const GValue *param_values,
                                                                   gpointer      invocation_hint,
                                                                   gpointer      marshal_data);

/* BOOLEAN:POINTER (closures.def:6) */
extern void g_cclosure_user_marshal_BOOLEAN__POINTER (GClosure     *closure,
                                                      GValue       *return_value,
                                                      guint         n_param_values,
                                                      const GValue *param_values,
                                                      gpointer      invocation_hint,
                                                      gpointer      marshal_data);

/* INT:STRING,POINTER (closures.def:7) */
extern void g_cclosure_user_marshal_INT__STRING_POINTER (GClosure     *closure,
                                                         GValue       *return_value,
                                                         guint         n_param_values,
                                                         const GValue *param_values,
                                                         gpointer      invocation_hint,
                                                         gpointer      marshal_data);

/* BOOLEAN:STRING,STRING (closures.def:8) */
extern void g_cclosure_user_marshal_BOOLEAN__STRING_STRING (GClosure     *closure,
                                                            GValue       *return_value,
                                                            guint         n_param_values,
                                                            const GValue *param_values,
                                                            gpointer      invocation_hint,
                                                            gpointer      marshal_data);

/* VOID:STRING,STRING,POINTER (closures.def:9) */
extern void g_cclosure_user_marshal_VOID__STRING_STRING_POINTER (GClosure     *closure,
                                                                 GValue       *return_value,
                                                                 guint         n_param_values,
                                                                 const GValue *param_values,
                                                                 gpointer      invocation_hint,
                                                                 gpointer      marshal_data);

/* VOID:STRING,STRING (closures.def:10) */
extern void g_cclosure_user_marshal_VOID__STRING_STRING (GClosure     *closure,
                                                         GValue       *return_value,
                                                         guint         n_param_values,
                                                         const GValue *param_values,
                                                         gpointer      invocation_hint,
                                                         gpointer      marshal_data);

/* VOID:STRING,STRING,STRING,POINTER (closures.def:11) */
extern void g_cclosure_user_marshal_VOID__STRING_STRING_STRING_POINTER (GClosure     *closure,
                                                                        GValue       *return_value,
                                                                        guint         n_param_values,
                                                                        const GValue *param_values,
                                                                        gpointer      invocation_hint,
                                                                        gpointer      marshal_data);

/* VOID:INT,STRING,INT,UINT,UINT (closures.def:12) */
extern void g_cclosure_user_marshal_VOID__INT_STRING_INT_UINT_UINT (GClosure     *closure,
                                                                    GValue       *return_value,
                                                                    guint         n_param_values,
                                                                    const GValue *param_values,
                                                                    gpointer      invocation_hint,
                                                                    gpointer      marshal_data);

G_END_DECLS

#endif /* __g_cclosure_user_marshal_MARSHAL_H__ */
