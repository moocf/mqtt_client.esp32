#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <mqtt_client.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include "macros.h"


static const char *MQTT_URI = "mqtt://10.2.28.74:1883";
static int mqtt_pokeball_sub = 0;

static esp_err_t nvs_init() {
  printf("- Initialize NVS\n");
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ERET( nvs_flash_erase() );
    ERET( nvs_flash_init() );
  }
  ERET( ret );
  return ESP_OK;
}


static void on_mqtt(void *arg, esp_event_base_t base, int32_t id, void *data) {
  esp_mqtt_event_handle_t d = (esp_mqtt_event_handle_t) data;
  esp_mqtt_client_handle_t h = d->client;
  d->topic = d->topic? d->topic : "";
  d->data = d->data? d->data : "";
  // d->data[d->data_len] = '\0';
  int msg;
  switch (d->event_id) {
    case MQTT_EVENT_CONNECTED:
    printf("- Connected to MQTT broker\n");
    msg = esp_mqtt_client_subscribe(h, "/pokedex", 0);
    printf(": subscribe to '/pokedex': msg=%d\n", msg);
    msg = esp_mqtt_client_subscribe(h, "/pokeball", 0);
    printf(": subscribe to '/pokeball': msg=%d\n", msg);
    mqtt_pokeball_sub = msg;
    msg = esp_mqtt_client_publish(h, "/pokemon", "charmender", 0, 1, 0);
    printf(": send 'charmender' on '/pokemon': msg=%d\n", msg);
    msg = esp_mqtt_client_publish(h, "/pokemon", "squirtle", 0, 1, 0);
    printf(": send 'squirtle' on '/pokemon': msg=%d\n", msg);
    msg = esp_mqtt_client_publish(h, "/pokemon", "bulbasaur", 0, 1, 0);
    printf(": send 'bulbasaur' on '/pokemon': msg=%d\n", msg);
    break;
    case MQTT_EVENT_DISCONNECTED:
    printf("- Disconnected from MQTT broker\n");
    break;
    case MQTT_EVENT_SUBSCRIBED:
    printf("- Subscribed to topic: msg=%d\n", d->msg_id);
    if (d->msg_id == mqtt_pokeball_sub) {
      msg = esp_mqtt_client_unsubscribe(h, "/pokeball");
      printf(": unsubscribe to '/pokeball': msg=%d\n", msg);
    }
    break;
    case MQTT_EVENT_UNSUBSCRIBED:
    printf("- Unsubscribed from topic: msg=%d\n", d->msg_id);
    break;
    case MQTT_EVENT_PUBLISHED:
    printf("- Published data to topic: msg=%d\n", d->msg_id);
    break;
    case MQTT_EVENT_DATA:
    printf("- Data recieved on topic: msg=%d\n", d->msg_id);
    break;
    case MQTT_EVENT_ERROR:
    printf("- MQTT error ocurred\n");
    break;
    default:
    printf("- Other event id: %d\n", d->event_id);
    break;
  }
  (void)(msg);
}


static esp_err_t mqtt_init(const char *uri) {
  printf("- Init MQTT client\n");
  esp_mqtt_client_config_t c = {
    .uri = uri,
  };
  esp_mqtt_client_handle_t h = esp_mqtt_client_init(&c);
  ERET( esp_mqtt_client_register_event(h, ESP_EVENT_ANY_ID, on_mqtt, h) );
  ERET( esp_mqtt_client_start(h) );
  return ESP_OK;
}


static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data) {
  if (id == WIFI_EVENT_STA_START) {
    printf("- WiFi station started, connecting ...\n");
    ERETV( esp_wifi_connect() );
  } else if (id == WIFI_EVENT_STA_CONNECTED) {
    wifi_event_sta_connected_t *d = (wifi_event_sta_connected_t*) data;
    printf("- Connected to '%s' on channel %d\n", d->ssid, d->channel);
  } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
    wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t*) data;
    printf("- Disconnected from '%s', reconnecting ...\n", d->ssid);
    ERETV( esp_wifi_connect() );
  }
}


static void on_ip(void *arg, esp_event_base_t base, int32_t id, void *data) {
  if (id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *d = (ip_event_got_ip_t*) data;
    printf("- Got IP=%s\n", ip4addr_ntoa(&d->ip_info.ip));
    ERETV( mqtt_init(MQTT_URI) );
  }
}


static esp_err_t wifi_sta() {
  printf("- Initialize TCP/IP adapter\n");
  tcpip_adapter_init();
  printf("- Create default event loop\n");
  ERET( esp_event_loop_create_default() );
  printf("- Initialize WiFi with default config\n");
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ERET( esp_wifi_init(&cfg) );
  printf("- Register WiFi event handler\n");
  ERET( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi, NULL) );
  ERET( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_ip, NULL) );
  printf("- Set WiFi mode as station\n");
  ERET( esp_wifi_set_mode(WIFI_MODE_STA) );
  printf("- Set WiFi configuration\n");
  wifi_config_t wifi_config = {.sta = {
    .ssid = "belkin.a58",
    .password = "ceca966f",
    .scan_method = WIFI_ALL_CHANNEL_SCAN,
    .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
    .threshold.rssi = -127,
    .threshold.authmode = WIFI_AUTH_OPEN,
  }};
  ERET( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
  printf("- Start WiFi\n");
  ERET( esp_wifi_start() );
  return ESP_OK;
}


void app_main() {
  ESP_ERROR_CHECK( nvs_init() );
  ESP_ERROR_CHECK( wifi_sta() );
}
