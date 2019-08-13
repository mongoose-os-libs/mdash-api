# Use mDash API to set a device label and create the UI

## Example
```c

#include "mgos.h"

#define USE_WIFI 0

#if USE_WIFI == 1
#include "mgos_wifi.h"
#endif

#include "mgos_mdash_api.h"

static void create_mdash_ui() {
  struct mgos_mdash_widgets *widgets = mgos_mdash_widgets_create(0);
  mgos_mdash_widgets_add_widget(
      widgets, mgos_mdash_widget_toggle_create("Switch LED on/off",
                                               "state.reported.on"));
  mgos_mdash_widgets_add_widget(
      widgets,
      mgos_mdash_widget_value_create("Uptime (s):", "state.reported.uptime"));
  mgos_mdash_widgets_add_widget(
      widgets, mgos_mdash_widget_input_create(
                   "Led pin", "state.reported.led_pin", "fa-save"));
  mgos_mdash_widgets_add_widget(
      widgets, mgos_mdash_widget_button_create("Reboot", "Sys.Reboot", NULL,
                                               "fa-power-off"));

  mgos_mdash_create_ui(widgets);
  mgos_mdash_widgets_free(widgets);
}

#if USE_WIFI == 1
static void wifi_cb(int ev, void *evd, void *arg) {
  switch (ev) {
    case MGOS_WIFI_EV_STA_IP_ACQUIRED: {
      LOG(LL_INFO, ("%s", "MGOS_WIFI_EV_STA_IP_ACQUIRED"));

      mgos_mdash_set_label("my_label");
      create_mdash_ui();

      break;
    }
  }
  (void) evd;
  (void) arg;
}
#else
static void net_cb(int ev, void *evd, void *arg) {
  switch (ev) {
    case MGOS_NET_EV_IP_ACQUIRED: {
      LOG(LL_INFO, ("%s", "MGOS_NET_EV_IP_ACQUIRED"));

      mgos_mdash_set_label("my_label");
      create_mdash_ui();
      break;
    }
  }
  (void) evd;
  (void) arg;
}
#endif

enum mgos_app_init_result mgos_app_init(void) {
#if USE_WIFI == 1
  mgos_event_add_group_handler(MGOS_EVENT_GRP_WIFI, wifi_cb, NULL);
#else
  mgos_event_add_group_handler(MGOS_EVENT_GRP_NET, net_cb, NULL);
#endif

  return MGOS_APP_INIT_SUCCESS;
}
```

## Usage
- Create a device in mDash
- Create a API key
- Build and flash the application
- `mos wifi SSID PASSWORD`
- Provision the device in mDash
- `mos --port <port> config-set mdash.device_id="<device ID from mDash>" mdash.api_key="<API key from mDash>" `
