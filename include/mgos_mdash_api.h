/*
 * Copyright (c) 2019 Liviu Nicolescu <nliviu@gmail.com>
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the ""License"");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ""AS IS"" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mgos_mdash_widget;
struct mgos_mdash_widgets;

/* set a device label */
void mgos_mdash_set_label(const char *label);

/* widget API */

/* Toggle: switch things on/off */
/* {"type": "toggle", "title": "Switch LED on/off", "key": "led"} */
struct mgos_mdash_widget *mgos_mdash_widget_toggle_create(const char *title,
                                                          const char *key);

/* Value: show any state.reported.KEY shadow value */
/* {"type": "value", "title": "free RAM:", "key": "ram"} */
struct mgos_mdash_widget *mgos_mdash_widget_value_create(const char *title,
                                                         const char *key);

/* Button: call any RPC function */
/* {"type": "button", "title": "reboot", "icon": "fa-power-off", "method":
   "Sys.Reboot", "params": {}} */
struct mgos_mdash_widget *mgos_mdash_widget_button_create(const char *title,
                                                          const char *method,
                                                          const char *params,
                                                          const char *icon);

/* Input: show and change any state.reported.KEY shadow value */
/* {"type": "input", "icon": "fa-save", title: "Set LED pin", "key": "pin"} */
struct mgos_mdash_widget *mgos_mdash_widget_input_create(const char *title,
                                                         const char *key);

/* free the widget */
void mgos_mdash_widget_free(struct mgos_mdash_widget **widget);

/* widgets API */

/* create the mDash UI */
bool mgos_mdash_create_ui(const struct mgos_mdash_widgets *widgets);

/* create the widgets holder */
struct mgos_mdash_widgets *mgos_mdash_widgets_create(size_t max_count);

/* add a widget */
void mgos_mdash_widgets_add_widget(struct mgos_mdash_widgets *widgets,
                                   struct mgos_mdash_widget *widget);

/* free the widgets holder */
void mgos_mdash_widgets_free(struct mgos_mdash_widgets *widgets);

/* create the widgets from config */
bool mgos_mdash_create_widgets_from_config(struct mgos_mdash_widgets **widgets);

#ifdef __cplusplus
}
#endif
