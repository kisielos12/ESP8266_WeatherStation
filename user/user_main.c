/*
* Title: Blinking LED example
* NOTE: Tested with Espressif FreeRTOS SDK v.1.4.0
* Copyright (c) 2016 Pratik Panda
* Designed for IoTBlocks Hardware
* (Website: http://www.PratikPanda.com)
*
* MIT License with additional clause:
* The author, Pratik Panda, must be notified if the source code is used
* in a commercial product or associated with any service that is not
* provided free of cost to the end user.The above copyright notice and
* this permission notice shall be included in all copies or substantial
* portions of the Software.
*
* Full License text:
* http://www.pratikpanda.com/iotblocks/iotblocks-code-license/
*/
 
#include "esp_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/api.h"
#include "lwipopts.h"
#include "lwip/sockets.h"
#include "lwip/ip_addr.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/igmp.h"
#include "lwip/udp.h"   
#include "lwip/netdb.h"
#include "lwip/tcp.h"
#include "apps/sntp.h"
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "esp_system.h"
#include "c_types.h"
#include "cJSON.h"
#include "espconn.h"
#include "user_config.h"
//#include "tcp_client.h"

/*USER INCLUDES */
#include "uart.h"
#include "i2c/i2c.h"
#include "cli/cli.h"
#include "conn_ap/conn_ap.h"
#include "ssd1306/ssd1306.h"
#include "weather/weather.h"
#include "dht/dht.h"

#include "fonts/SegoePrint16pt.h"
#include "fonts/Impact10pt.h"
#include "fonts/DejaVuSansCondensed20pt.h"
#include "fonts/dejaVuSans10pt.h"
#include "fonts/dejaVuSans14pt.h"
#include "fonts/dejaVuSans24pt.h"
#include "fonts/dejaVuSans28pt.h"
#include "fonts/dejaVuSans36pt.h"
#include "fonts/UbuntuCondensed26pt.h"

#define OLED_HEIGHT		64
#define OLED_WIDTH			128
#define OLED_BUFFER_LENGTH	((OLED_HEIGHT * OLED_WIDTH) / 8)

int I2C_Send (uint8_t SlaveAddr, uint8_t WordAddr, uint8_t *Buffer, size_t BufferLength);
void I2C_Delay(uint16_t ms);

uint8_t OLED_Buffer[OLED_BUFFER_LENGTH];

typedef void (*screenDrawFun_clbk) (void);

typedef struct
{
    screenDrawFun_clbk ScreenDrawFunction;
    int time;
}oled_draw_struct_t;


typedef struct
{
    time_t now;
    signed int indoor_temp;
    signed int indoor_humidity;
    signed int outdoor_temp;
    signed int outdoor_humidity;
}oled_data_s;

static oled_data_s displayData;

static  SSD1306_InitType display = {
        .DisplayAddress = 0x78,
        .WriteFunction = I2C_Send,
        .DelayFunction = I2C_Delay,
		.Height = OLED_HEIGHT,
		.Width = OLED_WIDTH,
		.FrameBuffer = OLED_Buffer,
		.BufferLength = sizeof(OLED_Buffer),
		.Color = 1
        };

static oled_draw_struct_t oledMenagerTab[10];
static int oledMenagerScreenCounter = 0;

void OLEDMenager_RegisterScreen(screenDrawFun_clbk fun, int time)
{
    oledMenagerTab[oledMenagerScreenCounter].ScreenDrawFunction = fun;
    oledMenagerTab[oledMenagerScreenCounter].time = time;
    oledMenagerScreenCounter++;
}

/* I2C write function callback typedef. */
int I2C_Send (uint8_t SlaveAddr, uint8_t WordAddr, uint8_t *Buffer, size_t BufferLength)
{
    uint8_t ack = 0;
    int i = 0;

    i2c_start();

    ack = i2c_write(SlaveAddr);

    ack = i2c_write(WordAddr);

    for ( i = 0; i < BufferLength; i++)
    {
        ack = i2c_write(Buffer[i]);
    }

    i2c_stop();

    return ack;
}

void I2C_Delay (uint16_t ms)
{
    os_delay_us(ms*1000);   
}

/**********************************SAMPLE CODE*****************************/

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/

uint32 user_rf_cal_sector_set(void)
{
    flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;
        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;
        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

void cli_test(uint32_t argc, char *argv[])
{
    printf("CLI TEST\r\n");
//    WEATHER_Update();
    char temp_buf[100];
    struct sensor_reading *data;

    data = readDHT(0);
    sprintf(temp_buf, "%dc %d%%", (int)(data->temperature), (int)(data->humidity));
    printf("dth11 %s\r\n", temp_buf);
}


void cli_oled_clear(uint32_t argc, char *argv[])
{
    SSD1306_Cls(&display);
    SSD1306_UpdateDisplay(&display);
}


void cli_oled_goto(uint32_t argc, char *argv[])
{  
    uint32_t x = atoi(argv[1]);  
    uint32_t y = atoi(argv[2]);

    SSD1306_Goto(&display, x , y);  
    SSD1306_UpdateDisplay(&display);
}


void cli_oled_print(uint32_t argc, char *argv[])
{
    printf("%s\r\n", argv[1]);
    SSD1306_Puts_P(&display, argv[1]);
    SSD1306_UpdateDisplay(&display);
}

void cli_rst(uint32_t argc, char *argv[])
{
    system_restart();
}

void cli()
{
    CLI_Register("clear", cli_oled_clear, 0, 0, "clear oled");
    CLI_Register("goto", cli_oled_goto, 2, 2, "go to x y on oled");
    CLI_Register("print", cli_oled_print, 1, 1, "print a string on oled");
    CLI_Register("reset", cli_rst, 0, 0, "reset esp");
    CLI_Register("test", cli_test, 0, 0, "test function");
    CLI_Init();
}

void OLED_SetDateAndTime(char *date, char *time)
{
    SSD1306_Cls(&display);
    SSD1306_SetFont(&display, &dejaVuSans_36ptFontInfo );
    SSD1306_Goto(&display, 0, 0);    
    SSD1306_Puts_P(&display, time);

    SSD1306_SetFont(&display, &dejaVuSans_14ptFontInfo );
    SSD1306_Goto(&display, 0, 51);    
    SSD1306_Puts_P(&display, date);
    SSD1306_UpdateDisplay(&display);
}

void OLED_SetHumidity(int indoor, int outdoor)
{
    char indoor_buf[10];
    char outdoor_buf[10];

    if(indoor == -1)
        sprintf(indoor_buf, " X");
    else
        sprintf(indoor_buf, "%2d%%", indoor);
    
    if(outdoor == -1)
        sprintf(outdoor_buf, " X");
    else        
        sprintf(outdoor_buf, "%2d%%", outdoor);

    if(indoor_buf && outdoor_buf)
    { 
        printf("%s\r\n%s\r\n",indoor_buf, outdoor_buf);
        SSD1306_Cls(&display);
        SSD1306_SetFont(&display, &dejaVuSans_10ptFontInfo );
        SSD1306_Goto(&display, 0, 3);    
        SSD1306_Puts_P(&display, "HUM");
        SSD1306_Goto(&display, 0, 20);    
        SSD1306_Puts_P(&display, "IN:");
        SSD1306_Goto(&display, 0, 32);    
        SSD1306_Puts_P(&display, "OUT:");
    
        SSD1306_SetFont(&display, &dejaVuSans_28ptFontInfo );
        SSD1306_Goto(&display, 50, 0);    
        SSD1306_Puts_P(&display, indoor_buf);
        SSD1306_Goto(&display, 50, 35);    
        SSD1306_Puts_P(&display, outdoor_buf);
        SSD1306_UpdateDisplay(&display);
    }
    else
    {
        printf("Error\r\n");
    }
}

void OLED_SetTemp(int indoor, int outdoor)
{
    printf("Set temp: %d %d\r\n", indoor, outdoor);

    char indoor_buf[10];
    char outdoor_buf[10];

    if(indoor == 69)
        sprintf(indoor_buf, "   X");
    else
        sprintf(indoor_buf, "% 3d*", indoor);
    
    if(outdoor == 69)
        sprintf(outdoor_buf, "   X");
    else        
        sprintf(outdoor_buf, "% 3d*", outdoor);

    if(indoor_buf && outdoor_buf)
    { 
        printf("%s\r\n%s\r\n",indoor_buf, outdoor_buf);
        SSD1306_Cls(&display);
        SSD1306_SetFont(&display, &dejaVuSans_10ptFontInfo );
        SSD1306_Goto(&display, 0, 3);    
        SSD1306_Puts_P(&display, "TEMP");
        SSD1306_Goto(&display, 0, 20);    
        SSD1306_Puts_P(&display, "IN:");
        SSD1306_Goto(&display, 0, 32);    
        SSD1306_Puts_P(&display, "OUT:");
    
        SSD1306_SetFont(&display, &dejaVuSans_28ptFontInfo );
        SSD1306_Goto(&display, 47, 0);    
        SSD1306_Puts_P(&display, indoor_buf);
        SSD1306_Goto(&display, 47, 32);    
        SSD1306_Puts_P(&display, outdoor_buf);
        SSD1306_UpdateDisplay(&display);
    }
    else
    {
        printf("Error\r\n");
    }
}

static void I2C_Scanner(void)
{
    int addr = 1;
    int ack = 0;
    printf("i2c scanner\r\n");
    for ( addr = 1; addr < 128; addr++)
    {
        i2c_start();
        ack = i2c_write(addr);
        if( ack )
            printf( "Found at: 0x%02X\r\n", addr);
        i2c_stop();
    }
    printf("i2c scanner\r\n");
}

static void newWeatherDataReceiver(weather_struct_t *newData)
{
//    printf("Temp: %d\r\nHumidity: %d\r\n", newData->temp, newData->humidity);
    displayData.outdoor_temp = newData->temp;
    displayData.outdoor_humidity = newData->humidity;
}

void dataTask ( void *pvParameters )
{
    time_t now;
    int formatCnt = 0;
    char temp_buf[100];
    struct sensor_reading *data;
  
   
    configTime(1, 1, "pool.ntp.org", NULL, NULL, 1);

    while(1)
    {
        time(&now);
        displayData.now = now;

        if(formatCnt % 5000)
        {
            data = readDHT(0);
            if(data->success)
            {
                displayData.indoor_humidity = (int)(data->humidity);
                displayData.indoor_temp = (int)(data->temperature);
            }
            else
            {
                displayData.indoor_temp = 69;
                displayData.indoor_humidity = -1;
            }
        }
 
        if(formatCnt % 30 == 0)
        {
            WEATHER_Update();
        }

        formatCnt++;
        vTaskDelay(1000 / portTICK_RATE_MS);
    }      
}

void oledTask ( void *pvParameters )
{
    SSD1306_Init(&display);
    SSD1306_Cls(&display);
    int oledCurrentPageCounter = 0;
    int oledCurrentPageShowTime = 0;

    while(1)
    {
        if(oledCurrentPageShowTime < oledMenagerTab[oledCurrentPageCounter].time)
        {
            oledMenagerTab[oledCurrentPageCounter].ScreenDrawFunction();
            oledCurrentPageShowTime++;
        }
        else
        {
            printf("Reset\r\n");
            oledCurrentPageCounter = (oledCurrentPageCounter + 1) % oledMenagerScreenCounter;
            oledCurrentPageShowTime = 0;
        }

        vTaskDelay(1000 / portTICK_RATE_MS );
    }
}

void printHumidity()
{
    OLED_SetHumidity(displayData.indoor_humidity, displayData.outdoor_humidity);
}

void printTemp()
{
    OLED_SetTemp(displayData.indoor_temp, displayData.outdoor_temp);
}

void printClock()
{
    char time_buf[64];
    char date_buf[64];
    struct tm timeinfo;
    static tick = 0;

    localtime_r(&(displayData.now), &timeinfo); 

    if( tick )
        strftime(time_buf, sizeof(time_buf), "%H:%M", &timeinfo);
    else
        strftime(time_buf, sizeof(time_buf), "%H %M", &timeinfo);
    
    tick ^= 1;
    strftime(date_buf, sizeof(date_buf), "%d.%m.%Y", &timeinfo);
    OLED_SetDateAndTime(date_buf, time_buf);

}

// User function
// All user code goes here.
// Note that the user function should exit and not block execution
void user_init(void)
{
    printf("SDK version:%s\n", system_get_sdk_version());
    cli();
    conn_ap_init();
    i2c_init();
    DHTInit(SENSOR_DHT11, 5000);

    OLEDMenager_RegisterScreen(printClock, 15);
    OLEDMenager_RegisterScreen(printTemp, 5);
    OLEDMenager_RegisterScreen(printClock, 15);
    OLEDMenager_RegisterScreen(printHumidity, 5);

    WEATHER_RegisterDataNotify(newWeatherDataReceiver);
    
    xTaskCreate(dataTask, (signed char *)"dataTask", 1024, NULL, 1, NULL);
    xTaskCreate(oledTask, (signed char *)"oledTask", 1024, NULL, 1, NULL);       
}
