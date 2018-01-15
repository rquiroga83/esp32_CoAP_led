/* CoAP Led server 

   This code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <sys/socket.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

#include "driver/gpio.h"

#include "nvs_flash.h"

#include "coap.h"

/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_WIFI_SSID            CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS            CONFIG_WIFI_PASSWORD

#define COAP_DEFAULT_TIME_SEC 5
#define COAP_DEFAULT_TIME_USEC 0

#define BLINK_GPIO 2

static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const static int CONNECTED_BIT = BIT0;

const static char *TAG = "CoAP_server";

//static coap_async_state_t *async = NULL;

static coap_async_state_t *led_h_async = NULL;
static coap_async_state_t *led_l_async = NULL;

static void send_async_response(coap_context_t *ctx, const coap_endpoint_t *local_if, coap_async_state_t **async, char* response_data )
{
    coap_pdu_t *response;
    unsigned char buf[3];
    size_t size = sizeof(coap_hdr_t) + 100; // Adjust message leng
    ESP_LOGI(TAG, "PDU Size %d", size);

    response = coap_pdu_init((*async)->flags & COAP_MESSAGE_CON, COAP_RESPONSE_CODE(205), 0, size);
    response->hdr->id = coap_new_message_id(ctx);
    if ((*async)->tokenlen)
        coap_add_token(response, (*async)->tokenlen, (*async)->token);
    coap_add_option(response, COAP_OPTION_CONTENT_TYPE, coap_encode_var_bytes(buf, COAP_MEDIATYPE_APPLICATION_JSON), buf);
    coap_add_data  (response, strlen(response_data), (unsigned char *)response_data);

    if (coap_send(ctx, local_if, &(*async)->peer, response) == COAP_INVALID_TID) {

    }
    coap_delete_pdu(response);
    coap_async_state_t *tmp;
    coap_remove_async(ctx, (*async)->id, &tmp);
    coap_free_async((*async));
    (*async) = NULL;
}

/*
 * The resource handlers
 */
static void led_h_async_handler(coap_context_t *ctx, struct coap_resource_t *resource,
              const coap_endpoint_t *local_interface, coap_address_t *peer,
              coap_pdu_t *request, str *token, coap_pdu_t *response)
{
    led_h_async = coap_register_async(ctx, peer, request, COAP_ASYNC_SEPARATE | COAP_ASYNC_CONFIRM, (void*)"no data");
}

static void led_l_async_handler(coap_context_t *ctx, struct coap_resource_t *resource,
              const coap_endpoint_t *local_interface, coap_address_t *peer,
              coap_pdu_t *request, str *token, coap_pdu_t *response)
{
    led_l_async = coap_register_async(ctx, peer, request, COAP_ASYNC_SEPARATE | COAP_ASYNC_CONFIRM, (void*)"no data");
}




static void coap_example_thread(void *p)
{
    coap_context_t*  ctx = NULL;
    coap_address_t   serv_addr;
    coap_resource_t* led_h = NULL;
    coap_resource_t* led_l = NULL;
    char* response_data;
    //resource

    fd_set           readfds;
    struct timeval tv;
    int flags = 0;

    while (1) {
        /* Wait for the callback to set the CONNECTED_BIT in the
           event group.
        */
        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                            false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Connected to AP");

        /* Prepare the CoAP server socket */
        coap_address_init(&serv_addr);
        serv_addr.addr.sin.sin_family      = AF_INET;
        serv_addr.addr.sin.sin_addr.s_addr = INADDR_ANY;
        serv_addr.addr.sin.sin_port        = htons(COAP_DEFAULT_PORT);
        ctx                                = coap_new_context(&serv_addr);
        if (ctx) {
            flags = fcntl(ctx->sockfd, F_GETFL, 0);
            fcntl(ctx->sockfd, F_SETFL, flags|O_NONBLOCK);

            tv.tv_usec = COAP_DEFAULT_TIME_USEC;
            tv.tv_sec = COAP_DEFAULT_TIME_SEC;
            /* Initialize the resource */
            led_h = coap_resource_init((unsigned char *)"ledh", 4, 0);
            led_l = coap_resource_init((unsigned char *)"ledl", 4, 0);

            if (led_h){
                coap_register_handler(led_h, COAP_REQUEST_GET, led_h_async_handler);
                coap_add_resource(ctx, led_h);
            }

            if (led_l){
                coap_register_handler(led_l, COAP_REQUEST_GET, led_l_async_handler);
                coap_add_resource(ctx, led_l);
            }

             /*For incoming connections*/
            for (;;) {
                FD_ZERO(&readfds);
                FD_CLR( ctx->sockfd, &readfds);
                FD_SET( ctx->sockfd, &readfds);

                int result = select( ctx->sockfd+1, &readfds, 0, 0, &tv );
                if (result > 0){
                    if (FD_ISSET( ctx->sockfd, &readfds ))
                        coap_read(ctx);
                    ESP_LOGI(TAG, "Message receibe!");
                } else if (result < 0){
                    ESP_LOGW(TAG, "Break");
                    break;
                } else {
                    ESP_LOGW(TAG, "select timeout");
                }

                if (led_h_async) {
                    response_data     = "{'response':'ON'}";
                    send_async_response(ctx, ctx->endpoint, &led_h_async, response_data);
                    /* Blink on (output high) */
                    gpio_set_level(BLINK_GPIO, 1);
                    ESP_LOGI(TAG, "ON");
                }

                if (led_l_async) {
                    response_data     = "{'response':'OFF'}";
                    send_async_response(ctx, ctx->endpoint, &led_l_async, response_data);
                    /* Blink off (output low) */
                    gpio_set_level(BLINK_GPIO, 0);
                    ESP_LOGI(TAG, "OFF");
                }
            }
            
            coap_free_context(ctx);
        }
    }

    vTaskDelete(NULL);
}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void wifi_conn_init(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}


void gpio_init(void){
    /* Configure the IOMUX register for pad BLINK_GPIO (some pads are
       muxed to GPIO on reset already, but some default to other
       functions and need to be switched to GPIO. Consult the
       Technical Reference for a list of pads and their default
       functions.)
    */
    gpio_pad_select_gpio(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

void app_main(void)
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    gpio_init();
    wifi_conn_init();

    xTaskCreate(coap_example_thread, "coap", 2048, NULL, 5, NULL);
}
