Este es un pequeño programa desarrollado con el ESP32 el cual consiste en prender y apagar un led utilizando el protocolo CoAP (https://en.wikipedia.org/wiki/Constrained_Application_Protocol).

antes de compilar es necesario establecer las variables de configuracion de wifi en el archivo "coap_server_main.c"

 ```c
#define EXAMPLE_WIFI_SSID            "wifo_ssid"
#define EXAMPLE_WIFI_PASS            "wifi_passwd"
```

Para compilar el programa en el esp32 es necesario instalar el sdk oficial (https://github.com/espressif/esp-idf), y en la carpeta "esp_coap_server" ejecutar el comando:

```bash
make flash
```

Esto compilara e instalara el programa el esp32.

Con el comando siguente se podra inspeccionar la direccion ip asignada por el router en de consola.

```bash
make monitor
```
 La salida es algo como lo que sigue:

```bash
I (2069) event: sta ip: 192.168.10.6, mask: 255.255.255.0, gw: 192.168.10.1
I (2069) CoAP_server: Connected to AP
```

Para probar el servidor en la carpeta "node_coap_client" se encuentra un pequeño cliente en node js con el cual se podra enviar la solicitud de prender o apagar el led, antes de usarlo es necesario configurar la direccion ip en el archivo client.js.

para preder el led se ejecuta el comando

```bash
node client.js ledh
```

![alt text](https://github.com/rquiroga83/esp32_CoAP_led/blob/master/images/on.png)

para apagarlo se ejecuta el comando

```bash
node client.js ledl
```
![alt text](https://github.com/rquiroga83/esp32_CoAP_led/blob/master/images/off.png)
