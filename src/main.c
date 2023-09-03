#include <stdio.h>
#include <string.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <esp_netif.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <esp_task_wdt.h>
#include <nvs_flash.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>

#include "client_handle.h"

#define SSID_MY_NETWORK "FRED_LUCAS"
#define LEN_SSID_MY_NETWORK strlen(SSID_MY_NETWORK)
#define PASS_MY_NETWORK "fred22458811278200"
#define SERVER_KEY "Teste"
#define SERVER_PING "PING-SERVER"
#define SERVER_ADDR "192.168.2.101"
#define WIFI_CONNECTED_BIT BIT0
#define MSG_BUFFER_SIZE 100
#define BUTTON_PIN GPIO_NUM_4

TaskHandle_t server_running;
wifi_ap_record_t target_acess_point = {0};
esp_netif_t* my_netif = NULL;
esp_event_handler_instance_t got_my_ip_instance;
esp_event_handler_instance_t started_wifi_sta_instance;
TaskHandle_t client_run_handle;
volatile bool connect_to_wifi = false;
static EventGroupHandle_t wifi_connected_handle;

void client_run (void* arg) {

    struct sockaddr_in server = {
        .sin_family = AF_INET,
        .sin_port = htons(1234)
    };
    // addr.sin_addr.s_addr = INADDR_ANY;
    inet_pton(AF_INET, SERVER_ADDR, &server.sin_addr);

    puts("CLIENT SETUP");

    fd_set all_set;

    int client, max_fd;
    bool connected = true;
    
    struct timeval timeout = {
        .tv_sec = 5,
        .tv_usec = 0
    };
    while (true)
    {
        client = create_client((char *)&timeout, sizeof(struct timeval));
        printf("CLIENT: %d\n", client);

        FD_ZERO(&all_set);
        FD_SET(client, &all_set);
        max_fd = client;

        if (connect(client, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) < 0) {
            puts("Couldn't connect");
        }
        else {

            // connected = authorize_connect(client, SERVER_KEY);
            
            while(connected) {
                handle_connection(client, &all_set, max_fd, SERVER_PING, SERVER_KEY);
            }
        }
        closesocket(client);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void started_wifi_sta(void *esp_netif, esp_event_base_t base, int32_t event_id, void *data) {
    if (connect_to_wifi)
        esp_wifi_connect();
}

#define TARGET_STATIC_IP_OCT 192, 168, 2, 78
void got_my_ip(void *esp_netif, esp_event_base_t base, int32_t event_id, void *data) {
    puts("GOT AN IP");

    ip_event_got_ip_t * info = (ip_event_got_ip_t*)data;
    printf("IP THAT I GOT = " IPSTR "\n", IP2STR(&info->ip_info.ip));

    esp_ip4_addr_t target_ip;
    esp_netif_set_ip4_addr(&target_ip, TARGET_STATIC_IP_OCT);
    if (info->ip_info.ip.addr != target_ip.addr) {
        esp_netif_dhcp_status_t status;
        ESP_ERROR_CHECK(esp_netif_dhcpc_get_status(my_netif, &status));
        if (status != ESP_NETIF_DHCP_STOPPED) {
            ESP_ERROR_CHECK(esp_netif_dhcpc_stop(my_netif));
        }
        esp_netif_set_ip4_addr(&(info->ip_info.ip), TARGET_STATIC_IP_OCT);
        ESP_ERROR_CHECK(esp_netif_set_ip_info(my_netif, &(info->ip_info)));
    } //else xTaskCreate(server_run, "Server Run", 40096, NULL, 10, &server_running);
    else xEventGroupSetBits(wifi_connected_handle, WIFI_CONNECTED_BIT);
}


void setup_events() {
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &got_my_ip,
        NULL,
        &got_my_ip_instance
    ));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        WIFI_EVENT_STA_START,
        &started_wifi_sta,
        NULL,
        &started_wifi_sta_instance
    ));
}

bool scann() {
    wifi_scan_config_t config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = true
    };

    ESP_ERROR_CHECK(esp_wifi_scan_start(&config, 1));

    uint16_t ap_amount = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_amount));
    printf("# of AP: %u\n", ap_amount);

    wifi_ap_record_t *records = malloc(sizeof(wifi_ap_config_t)*ap_amount);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_amount, records));

    for (int i = 0; i < ap_amount; i++){
        if (strncmp((char*)records[i].ssid, SSID_MY_NETWORK, LEN_SSID_MY_NETWORK) == 0) {
            puts("SSID Found");
            ESP_ERROR_CHECK(esp_wifi_clear_ap_list());
            free(records);
            return true;
        }
    }
    ESP_ERROR_CHECK(esp_wifi_clear_ap_list());
    free(records);

    puts("SSID Not Found");
    return false;
}

void app_main() {

    wifi_connected_handle = xEventGroupCreate();

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    my_netif = esp_netif_create_default_wifi_sta(); //esp_netif_create_default_wifi_ap()
    wifi_init_config_t default_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&default_config));

    setup_events();

    wifi_config_t my_network_config = {
        .sta = {
            .ssid = SSID_MY_NETWORK,
            .password = PASS_MY_NETWORK
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &my_network_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    connect_to_wifi = scann();

    if (connect_to_wifi){
        esp_wifi_connect();
    }

    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT); // pinmode(BUTTON_PIN, INPUT_PULLUP);
    //gpio_pullup_en(BUTTON_PIN);
    //gpio_pulldown_dis(BUTTON_PIN);
    gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY);//

    EventBits_t bits = xEventGroupWaitBits(wifi_connected_handle, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        xTaskCreate(client_run, "Client Run", 4068, NULL, 0, &client_run_handle);
    }
    else {
        puts("Couldn't wait for wifi connection \n Trying to restart ESP32");
        esp_restart();
    }

    while(true) vTaskDelay(10 / portTICK_PERIOD_MS);
}
