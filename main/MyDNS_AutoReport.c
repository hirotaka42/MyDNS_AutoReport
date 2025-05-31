#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/ip4_addr.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

#define WIFI_SSID "ssid_text"
#define WIFI_PASS "password_text"
#define SLEEP_SEC 1800   // 30分
#define MYDNS_IPV6_URL "http://ipv6.mydns.jp/login.html"
#define MYDNS_IPV4_URL "http://ipv4.mydns.jp/login.html"
#define MYDNS_USERNAME "username_text"
#define MYDNS_PASSWORD "password_text"
#define MYDNS_IPV6_DNS "2001:4860:4860::8888" // Google Public DNS IPv6

static const char *TAG = "MYDNS";

static void wifi_init_sta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "Connecting to WiFi...");
    esp_wifi_connect();

    // 待機
    int retry = 0;
    while (retry < 10 && esp_wifi_connect() != ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        retry++;
    }
    
    // ★ここを追加
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_create_ip6_linklocal(netif);
    }
}

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            printf("%.*s\n", evt->data_len, (char*)evt->data);
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void update_ddns(const char *url)
{
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler,
        .auth_type = HTTP_AUTH_TYPE_BASIC,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .skip_cert_common_name_check = true, // 検証しない（開発用）
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_username(client, MYDNS_USERNAME);
    esp_http_client_set_password(client, MYDNS_PASSWORD);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "[HTTP] Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

void app_main(void)
{
    nvs_flash_init();
    wifi_init_sta();

    ESP_LOGI(TAG, "Waiting for IP...");
    vTaskDelay(pdMS_TO_TICKS(5000));  // IP取得待ち

    // IPv6アドレス取得状況をログ出力
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_ip6_addr_t ip6;
        if (esp_netif_get_ip6_linklocal(netif, &ip6) == ESP_OK) {
            ESP_LOGI(TAG, "IPv6 Link-local: %x:%x:%x:%x:%x:%x:%x:%x", 
                IP6_ADDR_BLOCK1(&ip6), IP6_ADDR_BLOCK2(&ip6), IP6_ADDR_BLOCK3(&ip6), IP6_ADDR_BLOCK4(&ip6),
                IP6_ADDR_BLOCK5(&ip6), IP6_ADDR_BLOCK6(&ip6), IP6_ADDR_BLOCK7(&ip6), IP6_ADDR_BLOCK8(&ip6));
        } else {
            ESP_LOGW(TAG, "IPv6 Link-local address not assigned");
        }
        // グローバルIPv6アドレスも確認
        esp_ip6_addr_t ip6g;
        if (esp_netif_get_ip6_global(netif, &ip6g) == ESP_OK && !ip6_addr_islinklocal(&ip6g)) {
            ESP_LOGI(TAG, "IPv6 Global: %x:%x:%x:%x:%x:%x:%x:%x",
                IP6_ADDR_BLOCK1(&ip6g), IP6_ADDR_BLOCK2(&ip6g), IP6_ADDR_BLOCK3(&ip6g), IP6_ADDR_BLOCK4(&ip6g),
                IP6_ADDR_BLOCK5(&ip6g), IP6_ADDR_BLOCK6(&ip6g), IP6_ADDR_BLOCK7(&ip6g), IP6_ADDR_BLOCK8(&ip6g));
        }
        // DNSサーバ情報をログ出力
        esp_netif_dns_info_t dns_info;
        if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info) == ESP_OK) {
            if (dns_info.ip.type == ESP_IPADDR_TYPE_V6) {
                ESP_LOGI(TAG, "DNSv6: %x:%x:%x:%x:%x:%x:%x:%x",
                    IP6_ADDR_BLOCK1(&dns_info.ip.u_addr.ip6), IP6_ADDR_BLOCK2(&dns_info.ip.u_addr.ip6),
                    IP6_ADDR_BLOCK3(&dns_info.ip.u_addr.ip6), IP6_ADDR_BLOCK4(&dns_info.ip.u_addr.ip6),
                    IP6_ADDR_BLOCK5(&dns_info.ip.u_addr.ip6), IP6_ADDR_BLOCK6(&dns_info.ip.u_addr.ip6),
                    IP6_ADDR_BLOCK7(&dns_info.ip.u_addr.ip6), IP6_ADDR_BLOCK8(&dns_info.ip.u_addr.ip6));
            } else if (dns_info.ip.type == ESP_IPADDR_TYPE_V4) {
                ESP_LOGI(TAG, "DNSv4: %s", ipaddr_ntoa((const ip_addr_t*)&dns_info.ip.u_addr.ip4));
            }
        }
        // DNSサーバが未設定ならGoogle Public DNSを手動設定
        if (dns_info.ip.type != ESP_IPADDR_TYPE_V6 ||
            (dns_info.ip.u_addr.ip6.addr[0] == 0 && dns_info.ip.u_addr.ip6.addr[1] == 0 &&
             dns_info.ip.u_addr.ip6.addr[2] == 0 && dns_info.ip.u_addr.ip6.addr[3] == 0)) {
            esp_netif_dns_info_t manual_dns;
            manual_dns.ip.type = ESP_IPADDR_TYPE_V6;
            inet_pton(AF_INET6, MYDNS_IPV6_DNS, &manual_dns.ip.u_addr.ip6);
            esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &manual_dns);
            ESP_LOGI(TAG, "Set Google IPv6 DNS: %s", MYDNS_IPV6_DNS);
            vTaskDelay(pdMS_TO_TICKS(1000)); // 1秒待機
            // 設定後のDNSサーバ情報を再取得してログ出力
            esp_netif_dns_info_t check_dns;
            if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &check_dns) == ESP_OK) {
                if (check_dns.ip.type == ESP_IPADDR_TYPE_V6) {
                    ESP_LOGI(TAG, "After set: DNSv6: %x:%x:%x:%x:%x:%x:%x:%x",
                        IP6_ADDR_BLOCK1(&check_dns.ip.u_addr.ip6), IP6_ADDR_BLOCK2(&check_dns.ip.u_addr.ip6),
                        IP6_ADDR_BLOCK3(&check_dns.ip.u_addr.ip6), IP6_ADDR_BLOCK4(&check_dns.ip.u_addr.ip6),
                        IP6_ADDR_BLOCK5(&check_dns.ip.u_addr.ip6), IP6_ADDR_BLOCK6(&check_dns.ip.u_addr.ip6),
                        IP6_ADDR_BLOCK7(&check_dns.ip.u_addr.ip6), IP6_ADDR_BLOCK8(&check_dns.ip.u_addr.ip6));
                }
            }
        }
    } else {
        ESP_LOGE(TAG, "Failed to get netif handle");
    }

    // IPv6アクセスでデバッグ
    ESP_LOGI(TAG, "[CHECK] Try IPv6 myDNS access...");
    update_ddns(MYDNS_IPV6_URL);
    ESP_LOGI(TAG, "[CHECK] IPv6 myDNS access finished.");

    // IPv4アクセスでデバッグ
    ESP_LOGI(TAG, "[CHECK] Try IPv4 myDNS access...");
    update_ddns(MYDNS_IPV4_URL);
    ESP_LOGI(TAG, "[CHECK] IPv4 myDNS access finished.");

    ESP_LOGI(TAG, "Going to deep sleep...");
    esp_deep_sleep(SLEEP_SEC * 1000000ULL);
}
