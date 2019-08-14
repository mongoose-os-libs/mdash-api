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
#include "mgos.h"

#include "mgos_mdash_api.h"

/*
 * mos.yml:
config_schema:
  - ["mdash.device_id", "s", "", {}]
  - ["mdash.api_key", "s", "", {}]

 */

/* mdash widget types */
enum mdash_widget_type {
  /* Toggle: switch things on/off */
  /* {"type": "toggle", "title": "Switch LED on/off", "key": "led"} */
  MDASH_WIDGET_TOGGLE,

  /* Value: show any state.reported.KEY shadow value */
  /* {"type": "value", "title": "free RAM:", "key": "ram"} */
  MDASH_WIDGET_VALUE,

  /* Button: call any RPC function */
  /* {"type": "button", "title": "reboot", "icon": "fa-power-off", "method":
     "Sys.Reboot", "params": {}} */
  MDASH_WIDGET_BUTTON,

  /* Input: show and change any state.reported.KEY shadow value */
  /* {"type": "input", "icon": "fa-save", title: "Set LED pin", "key": "pin"} */
  MDASH_WIDGET_INPUT,
};

struct mgos_mdash_widget {
  enum mdash_widget_type type;
  char *title;
  union {
    struct toggle {
      char *key;
    } toggle;
    struct value {
      char *key;
    } value;
    struct button {
      char *icon;
      char *method;
      char *params;
    } button;
    struct input {
      char *key;
    } input;
  };
};

struct mgos_mdash_widgets {
  struct mgos_mdash_widget **widgets;
  size_t max_count;
  size_t count;
};

static void mdash_api_handler(struct mg_connection *nc, int ev, void *evd,
                              void *cb_arg) {
  const char *caller_func = (const char *) cb_arg;
  switch (ev) {
    case MG_EV_CONNECT: {
      int err = *(int *) evd;
      if (err != 0) {
        LOG(LL_INFO, ("%s - caller: %s - MG_EV_CONNECT error: %d (%s)",
                      __FUNCTION__, caller_func, err, strerror(err)));
      }
      break;
    }
    case MG_EV_HTTP_REPLY: {
      const struct http_message *msg = (const struct http_message *) (evd);
      if (msg->resp_code != 200) {
        int code = 0;
        char *message = NULL;
        if (json_scanf(msg->body.p, msg->body.len,
                       "{error:{code:%d,message:%Q}}", &code, &message) == 2) {
          LOG(LL_INFO,
              ("%s - caller: %s - MG_EV_HTTP_REPLY error: %d, message: %s",
               __FUNCTION__, caller_func, code, message));
          free(message);
        }
      }
      nc->flags |= MG_F_CLOSE_IMMEDIATELY;
      break;
    }
  }
}

static void mdash_connect(const char *mdash_device_id,
                          const char *mdash_api_key, const char *post_data,
                          const char *caller_func) {
  char url[128];
  snprintf(url, sizeof(url),
           "https://mdash.net/api/v2/devices/%s?access_token=%s",
           mdash_device_id, mdash_api_key);
  const char *extra_headers = "Content-Type: application/json\r\n";
  struct mg_mgr *mgr = mgos_get_mgr();
  mg_connect_http(mgr, mdash_api_handler, (void *) caller_func, url,
                  extra_headers, post_data);
}

typedef void (*mdash_format_func)(struct mbuf *jsmb, void *arg);

static void mdash_api_post(mdash_format_func cb, void *cb_arg,
                           const char *caller_func) {
  const char *mdash_device_id = mgos_sys_config_get_mdash_device_id();
  const char *mdash_api_key = mgos_sys_config_get_mdash_api_key();
  if ((mdash_api_key == NULL) || (mdash_device_id == NULL)) {
    LOG(LL_ERROR,
        ("%s - mdash_api_key (%p) and/or mdash_device_id (%p) is NULL",
         __FUNCTION__, mdash_api_key, mdash_device_id));
    return;
  }

  struct mbuf jsmb;
  cb(&jsmb, cb_arg);
  mdash_connect(mdash_device_id, mdash_api_key, jsmb.buf, caller_func);
  mbuf_free(&jsmb);
}

static void set_label_format(struct mbuf *jsmb, void *arg) {
  const char *label = (const char *) arg;
  if (label == NULL) {
    label = mgos_sys_config_get_device_id();
  }

  struct json_out jsout = JSON_OUT_MBUF(jsmb);
  mbuf_init(jsmb, 0);
  json_printf(&jsout, "{shadow:{tags:{labels:%Q}}}", label);
  // ensure null terminating string
  mbuf_append(jsmb, "", 1);
}

/*
 * if label is NULL, device.id is used
 */
void mgos_mdash_set_label(const char *label) {
  mdash_api_post(set_label_format, (void *) label, __FUNCTION__);
}

static void format_widget(struct json_out *jsout,
                          const struct mgos_mdash_widget *widget) {
  switch (widget->type) {
    case MDASH_WIDGET_TOGGLE:
      json_printf(jsout, "{type: %Q, title: %Q, key: %Q}", "toggle",
                  widget->title, widget->toggle.key);
      break;
    case MDASH_WIDGET_VALUE:
      json_printf(jsout, "{type: %Q, title: %Q, key: %Q}", "value",
                  widget->title, widget->value.key);
      break;
    case MDASH_WIDGET_BUTTON:
      json_printf(
          jsout, "{type: %Q, title: %Q, icon: %Q, method: %Q, params:{%s}}",
          "button", widget->title, widget->button.icon, widget->button.method,
          (widget->button.params != NULL) ? widget->button.params : "");
      break;
    case MDASH_WIDGET_INPUT:
      json_printf(jsout, "{type: %Q, icon: %Q, title: %Q, key: %Q}", "input",
                  "fa-save", widget->title, widget->input.key);
      break;
  }
}

static void create_ui_format(struct mbuf *jsmb, void *arg) {
  struct json_out jsout = JSON_OUT_MBUF(jsmb);
  mbuf_init(jsmb, 0);

  const struct mgos_mdash_widgets *widgets =
      (const struct mgos_mdash_widgets *) arg;
  json_printf(&jsout, "{shadow:{tags:{ui:{widgets:[");
  for (size_t i = 0; i < widgets->count; ++i) {
    if (widgets->widgets[i] != NULL) {
      if (i != 0) {
        mbuf_append(jsmb, ", ", 2);
      }
      format_widget(&jsout, widgets->widgets[i]);
    }
  }

  json_printf(&jsout, "]}}}}");
  mbuf_append(jsmb, "", 1);
  LOG(LL_INFO, ("%s - %s", __FUNCTION__, jsmb->buf));
}

/* Toggle: switch things on/off */
/* {"type": "toggle", "title": "Switch LED on/off", "key": "led"} */
struct mgos_mdash_widget *mgos_mdash_widget_toggle_create(const char *title,
                                                          const char *key) {
  if ((title == NULL) || (key == NULL)) {
    LOG(LL_ERROR, ("title and key must be NOT null!"));
    return NULL;
  }
  struct mgos_mdash_widget *p = calloc(1, sizeof(*p));
  p->type = MDASH_WIDGET_TOGGLE;
  p->title = strdup(title);
  p->toggle.key = strdup(key);
  return p;
}

/* Value: show any state.reported.KEY shadow value */
/* {"type": "value", "title": "free RAM:", "key": "ram"} */
struct mgos_mdash_widget *mgos_mdash_widget_value_create(const char *title,
                                                         const char *key) {
  if ((title == NULL) || (key == NULL)) {
    LOG(LL_ERROR, ("title and key must be NOT null!"));
    return NULL;
  }
  struct mgos_mdash_widget *p = calloc(1, sizeof(*p));
  p->type = MDASH_WIDGET_VALUE;
  p->title = strdup(title);
  p->value.key = strdup(key);
  return p;
}

/* Button: call any RPC function */
/* {"type": "button", "title": "reboot", "icon": "fa-power-off", "method":
   "Sys.Reboot", "params": {}} */
struct mgos_mdash_widget *mgos_mdash_widget_button_create(const char *title,
                                                          const char *method,
                                                          const char *params,
                                                          const char *icon) {
  if ((title == NULL) || (method == NULL) /*|| (icon == NULL)*/) {
    LOG(LL_ERROR, ("title, method and icon must be NOT null!"));
    return NULL;
  }
  struct mgos_mdash_widget *p = calloc(1, sizeof(*p));
  p->type = MDASH_WIDGET_BUTTON;
  p->title = strdup(title);
  p->button.method = strdup(method);
  p->button.params = strdup(params ? params : "");
  p->button.icon = (icon != NULL) ? strdup(icon) : NULL;
  return p;
}

/* Input: show and change any state.reported.KEY shadow value */
/* {"type": "input", "icon": "fa-save", title: "Set LED pin", "key": "pin"} */
struct mgos_mdash_widget *mgos_mdash_widget_input_create(const char *title,
                                                         const char *key) {
  if ((title == NULL) || (key == NULL)) {
    LOG(LL_ERROR, ("title and key must be NOT null!"));
    return NULL;
  }
  struct mgos_mdash_widget *p = calloc(1, sizeof(*p));
  p->type = MDASH_WIDGET_INPUT;
  p->title = strdup(title);
  p->input.key = strdup(key);
  return p;
}

void mgos_mdash_widget_free(struct mgos_mdash_widget **widget) {
  if ((widget == NULL) || (*widget == NULL)) {
    return;
  }
  struct mgos_mdash_widget *p = *widget;
  switch (p->type) {
    case MDASH_WIDGET_TOGGLE:
      if (p->toggle.key != NULL) {
        free(p->toggle.key);
      }
      break;
    case MDASH_WIDGET_VALUE:
      if (p->value.key != NULL) {
        free(p->value.key);
      }
      break;
    case MDASH_WIDGET_BUTTON:
      if (p->button.icon != NULL) {
        free(p->button.icon);
      }
      if (p->button.params != NULL) {
        free(p->button.params);
      }
      if (p->button.method != NULL) {
        free(p->button.method);
      }
      break;
    case MDASH_WIDGET_INPUT:
      if (p->input.key != NULL) {
        free(p->input.key);
      }
      break;
  }
  if (p->title != NULL) {
    free(p->title);
  }
  free(p);
  *widget = NULL;
}

bool mgos_mdash_create_ui(const struct mgos_mdash_widgets *widgets) {
  if ((widgets == NULL) || (widgets->count == 0)) {
    LOG(LL_ERROR, ("Invalid values for widgets (%p) and/or widgets->count (%d)",
                   widgets, (int) widgets->count));
    return false;
  }

  mdash_api_post(create_ui_format, (void *) widgets, __FUNCTION__);

  return true;
}

struct mgos_mdash_widgets *mgos_mdash_widgets_create(size_t max_count) {
  struct mgos_mdash_widgets *widgets =
      (struct mgos_mdash_widgets *) calloc(1, sizeof(*widgets));
  widgets->max_count = max_count;
  widgets->widgets = (struct mgos_mdash_widget **) calloc(
      max_count, sizeof(widgets->widgets[0]));
  return widgets;
}

void mgos_mdash_widgets_add_widget(struct mgos_mdash_widgets *widgets,
                                   struct mgos_mdash_widget *widget) {
  if ((widgets == NULL) || (widget == NULL)) {
    LOG(LL_ERROR,
        ("widgets (%p) and/or widget (%p) is NULL!", widgets, widget));
    return;
  }
  if (widgets->count == widgets->max_count) {
    /* increase max_count */
    widgets->max_count += 2;
    /* realloc */
    widgets->widgets = (struct mgos_mdash_widget **) realloc(
        widgets->widgets, widgets->max_count * sizeof(widgets->widgets[0]));
  }
  widgets->count++;
  widgets->widgets[widgets->count - 1] = widget;
}

void mgos_mdash_widgets_free(struct mgos_mdash_widgets *widgets) {
  if ((widgets == NULL) || (widgets->widgets == NULL)) {
    return;
  }
  for (size_t i = 0; i < widgets->count; ++i) {
    mgos_mdash_widget_free(&widgets->widgets[i]);
  }
  free(widgets->widgets);
}

/*
struct mgos_config_mdash_value {
  const char * title;
  const char * key;
};

struct mgos_config_mdash_input {
  const char * title;
  const char * key;
};

struct mgos_config_mdash_toggle {
  const char * title;
  const char * key;
};

struct mgos_config_mdash_button {
  const char * title;
  const char * method;
  const char * params;
  const char * icon;
};

struct mgos_config_mdash {
  const char * device_id;
  const char * api_key;
  struct mgos_config_mdash_value value;
  struct mgos_config_mdash_value value1;
  struct mgos_config_mdash_value value2;
  struct mgos_config_mdash_value value3;
  struct mgos_config_mdash_value value4;
  struct mgos_config_mdash_input input;
  struct mgos_config_mdash_input input1;
  struct mgos_config_mdash_input input2;
  struct mgos_config_mdash_input input3;
  struct mgos_config_mdash_input input4;
  struct mgos_config_mdash_toggle toggle;
  struct mgos_config_mdash_toggle toggle1;
  struct mgos_config_mdash_toggle toggle2;
  struct mgos_config_mdash_button button;
  struct mgos_config_mdash_button button1;
  struct mgos_config_mdash_button button2;
  struct mgos_config_mdash_button button3;
};

 */

static const struct mgos_config_mdash_value *mdash_get_value(
    const struct mgos_config_mdash *cfg, int index) {
  if (cfg == NULL) {
    return NULL;
  }
  switch (index) {
    case 0:
      return &cfg->value;
    case 1:
      return &cfg->value1;
    case 2:
      return &cfg->value2;
    case 3:
      return &cfg->value3;
    case 4:
      return &cfg->value4;
    case 5:
      return &cfg->value5;
    case 6:
      return &cfg->value6;
    case 7:
      return &cfg->value7;
    case 8:
      return &cfg->value8;
    case 9:
      return &cfg->value9;
  }
  return NULL;
}

static const struct mgos_config_mdash_input *mdash_get_input(
    const struct mgos_config_mdash *cfg, int index) {
  if (cfg == NULL) {
    return NULL;
  }
  switch (index) {
    case 0:
      return &cfg->input;
    case 1:
      return &cfg->input1;
    case 2:
      return &cfg->input2;
    case 3:
      return &cfg->input3;
    case 4:
      return &cfg->input4;
    case 5:
      return &cfg->input5;
    case 6:
      return &cfg->input6;
    case 7:
      return &cfg->input7;
    case 8:
      return &cfg->input8;
    case 9:
      return &cfg->input9;
  }
  return NULL;
}

static const struct mgos_config_mdash_toggle *mdash_get_toggle(
    const struct mgos_config_mdash *cfg, int index) {
  if (cfg == NULL) {
    return NULL;
  }
  switch (index) {
    case 0:
      return &cfg->toggle;
    case 1:
      return &cfg->toggle1;
    case 2:
      return &cfg->toggle2;
  }
  return NULL;
}

static const struct mgos_config_mdash_button *mdash_get_button(
    const struct mgos_config_mdash *cfg, int index) {
  if (cfg == NULL) {
    return NULL;
  }
  switch (index) {
    case 0:
      return &cfg->button;
    case 1:
      return &cfg->button1;
    case 2:
      return &cfg->button2;
  }
  return NULL;
}

static void mdash_process_value(const struct mgos_config_mdash *cfg,
                                struct mgos_mdash_widgets *widgets) {
  for (int i = 0; i < 10; ++i) {
    const struct mgos_config_mdash_value *p = mdash_get_value(cfg, i);
    if (p->enable) {
      struct mgos_mdash_widget *w =
          mgos_mdash_widget_value_create(p->title, p->key);
      if (w != NULL) {
        mgos_mdash_widgets_add_widget(widgets, w);
      }
    }
  }
}

static void mdash_process_input(const struct mgos_config_mdash *cfg,
                                struct mgos_mdash_widgets *widgets) {
  for (int i = 0; i < 10; ++i) {
    const struct mgos_config_mdash_input *p = mdash_get_input(cfg, i);
    if (p->enable) {
      struct mgos_mdash_widget *w =
          mgos_mdash_widget_input_create(p->title, p->key);
      if (w != NULL) {
        mgos_mdash_widgets_add_widget(widgets, w);
      }
    }
  }
}

static void mdash_process_toggle(const struct mgos_config_mdash *cfg,
                                 struct mgos_mdash_widgets *widgets) {
  for (int i = 0; i < 3; ++i) {
    const struct mgos_config_mdash_toggle *p = mdash_get_toggle(cfg, i);
    if (p->enable) {
      struct mgos_mdash_widget *w =
          mgos_mdash_widget_toggle_create(p->title, p->key);
      if (w != NULL) {
        mgos_mdash_widgets_add_widget(widgets, w);
      }
    }
  }
}

static void mdash_process_button(const struct mgos_config_mdash *cfg,
                                 struct mgos_mdash_widgets *widgets) {
  for (int i = 0; i < 3; ++i) {
    const struct mgos_config_mdash_button *p = mdash_get_button(cfg, i);
    if (p->enable) {
      struct mgos_mdash_widget *w = mgos_mdash_widget_button_create(
          p->title, p->method, p->params, p->icon);
      if (w != NULL) {
        mgos_mdash_widgets_add_widget(widgets, w);
      }
    }
  }
}

bool mgos_mdash_create_widgets_from_config(
    struct mgos_mdash_widgets **pwidgets) {
  if (pwidgets == NULL) {
    return false;
  }

  *pwidgets = mgos_mdash_widgets_create(0);
  const struct mgos_config_mdash *cfg = mgos_sys_config_get_mdash();

  mdash_process_value(cfg, *pwidgets);
  mdash_process_input(cfg, *pwidgets);
  mdash_process_toggle(cfg, *pwidgets);
  mdash_process_button(cfg, *pwidgets);

  return true;
}

bool mgos_mdash_api_init(void) {
  return true;
}
