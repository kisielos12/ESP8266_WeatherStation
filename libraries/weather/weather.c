/*
 * Author: Jakub Kisiel
 */

#include <stdlib.h>
#include <stdint.h>
#include "weather.h"
#include "lwip/ip_addr.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/tcp.h"

#include <stdint.h>
#include <string.h>
#include "c_types.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

//static SemaphoreHandle_t weatherSemp;

static newData_clbk_f notifyClbk = NULL;
static struct tcp_pcb* pcb = NULL;

static void foundIpByHostname(const char *name, ip_addr_t *ipaddr, void *callback_arg);

static void parseWeatherFromJson(char *jsonMsg)
{
    char *out;
    char *msg;
    cJSON *root;
    cJSON *mainTemp;
    cJSON *mainValues;
    weather_struct_t newData;
    int temp;
    int humidity;
    char *city;

    memset(&newData, 0x0, sizeof(newData));

    msg = strstr(jsonMsg, "{\"coord\":");
    if(msg == NULL)
    {
        newData.error_code = ERR_BUF; 
        return;
    }

    root = cJSON_Parse(msg);

    if (!root) 
    {
        printf("Error before: [%s]\n", cJSON_GetErrorPtr());
        newData.error_code = ERR_VAL;
    }
    else 
    {
        mainValues = cJSON_GetObjectItem(root, "main");
        temp = cJSON_GetObjectItem(mainValues, "temp")->valueint;
        humidity = cJSON_GetObjectItem(mainValues, "humidity")->valueint;
        city =  cJSON_GetObjectItem(root, "name")->valuestring;

        cJSON_Delete(root);
        free(out);
        newData.error_code = ERR_OK;
        newData.temp = temp;
        newData.humidity = humidity;
        strcpy(newData.city, city);

    }
    if(notifyClbk != NULL)
        notifyClbk(&newData);
}


static err_t tcpRecvCallback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (p == NULL) {
        tcp_close(tpcb);
        return ERR_ABRT;
    } else {
        parseWeatherFromJson((char *)p->payload);
    }
    
    return 0;
}

static uint32_t tcp_send_packet(void)
{
    char *string = "GET /data/2.5/weather?id=2172797&appid=b6907d289e10d714a6e88b30761fae22\r\n\r\n ";
    uint32_t len = strlen(string);
    err_t error;
    /* push to buffer */
    error = tcp_write(pcb, string, strlen(string), TCP_WRITE_FLAG_COPY);
    
    if (error) {
        return 1;
    }
    
    /* now send */
    error = tcp_output(pcb);
    if (error) {
        return 1;
    }
    return 0;
}

/* connection established callback, err is unused and only return 0 */
static err_t connectCallback(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    tcp_send_packet();
    return 0;
}

static void tcp_setup(ip_addr_t ipaddr)
{
    uint32_t data = 0xdeadbeef;

    /* create the control block */
    pcb = tcp_new();    //testpcb is a global struct tcp_pcb
                            // as defined by lwIP

    /* dummy data to pass to callbacks*/

    tcp_arg(pcb, &data);

    /* register callbacks with the pcb */
    tcp_recv(pcb, tcpRecvCallback);

    /* now connect */
    tcp_connect(pcb, &ipaddr, 80, connectCallback);
}

static void sendApiRequest()
{
   ip_addr_t server_address;
   err_t err;

    err = dns_gethostbyname("api.openweathermap.org", &server_address, foundIpByHostname, NULL);
    if (err == ERR_INPROGRESS) 
    {
      /* DNS request sent, wait for sntp_dns_found being called */
//      printf("sntp_request: Waiting for server address to be resolved.\r\n");
      return;
    } else if (err == ERR_OK) 
    {
        tcp_setup(server_address);
    }
}

static void foundIpByHostname(const char *name, ip_addr_t *ipaddr, void *callback_arg)
{
    //Now is sending request cause in some case there is a risk of incorrect resolved name,
    //so run one time more resolving to be sure
     
    sendApiRequest();
}

void WEATHER_RegisterDataNotify(newData_clbk_f fun)
{
    notifyClbk = fun;
}

void WEATHER_Update()
{
    sendApiRequest();
}

