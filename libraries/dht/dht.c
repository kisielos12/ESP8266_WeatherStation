
/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you
 * retain
 * this notice you can do whatever you want with this stuff. If we meet some
 * day,
 * and you think this stuff is worth it, you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 */
#include "esp_common.h"
#include "c_types.h"
#include "gpio.h"
#include "dht.h"

#define MAXTIMINGS 10000
#define DHT_MAXCOUNT 32000
#define BREAKTIME 32

#define DHT_PIN 14
#define printf(...) 

enum sensor_type SENSOR;

static inline float scale_humidity(int *data) {
  if (SENSOR == SENSOR_DHT11) {
    return data[0];
  } else {
    float humidity = data[0] * 256 + data[1];
    return humidity /= 10;
  }
}

static inline float scale_temperature(int *data) {
  if (SENSOR == SENSOR_DHT11) {
    return data[2];
  } else {
    float temperature = data[2] & 0x7f;
    temperature *= 256;
    temperature += data[3];
    temperature /= 10;
    if (data[2] & 0x80)
      temperature *= -1;
    return temperature;
  }
}

static inline void delay_ms(int sleep) { 
    os_delay_us(1000 * sleep); 
}

static struct sensor_reading reading = {
    .source = "DHT11", .success = 0
};


    
    
/** 
Originally from: http://harizanov.com/2014/11/esp8266-powered-web-server-led-control-dht22-temperaturehumidity-sensor-reading/
Adapted from: https://github.com/adafruit/Adafruit_Python_DHT/blob/master/source/Raspberry_Pi/pi_dht_read.c
LICENSE:
// Copyright (c) 2014 Adafruit Industries
// Author: Tony DiCola

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
*/
static  void ICACHE_FLASH_ATTR pollDHTCb(void * arg){
  int counter = 0;
  int laststate = 1;
  int i = 0;
  int bits_in = 0;

  int data[100];

  data[0] = data[1] = data[2] = data[3] = data[4] = 0;

  //disable interrupts, start of critical section
  ETS_INTR_LOCK();
  system_soft_wdt_feed();

  GPIO_OUTPUT_SET(DHT_PIN, 1);
  delay_ms(20);

  // Hold low for 20ms
  GPIO_OUTPUT_SET(DHT_PIN, 0);
  delay_ms(20);

  // High for 40ms

  GPIO_DIS_OUTPUT(DHT_PIN);
  os_delay_us(40);



  // wait for pin to drop?
  while (GPIO_INPUT_GET(DHT_PIN) == 1 && i < DHT_MAXCOUNT) {
    if (i >= DHT_MAXCOUNT) {
      goto fail;
    }
    i++;
  }


  // read data!
  for (i = 0; i < MAXTIMINGS; i++) {
    // Count high time (in approx us)
    counter = 0;
    while (GPIO_INPUT_GET(DHT_PIN) == laststate) {
      counter++;
      os_delay_us(1);
      if (counter == 1000)
        break;
    }
    laststate = GPIO_INPUT_GET(DHT_PIN);

    if (counter == 1000)
      break;

    // store data after 3 reads
    if ((i > 3) && (i % 2 == 0)) {
      // shove each bit into the storage bytes
      data[bits_in / 8] <<= 1;
      if (counter > BREAKTIME) {
        data[bits_in / 8] |= 1;
      } else {
      }
      bits_in++;
    }
  }

  //Re-enable interrupts, end of critical section
  ETS_INTR_UNLOCK();

  if (bits_in < 40) {
    goto fail;
  }
  

  int checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
  
  
  if (data[4] != checksum) {
    goto fail;
  }

  reading.temperature = scale_temperature(data);
  reading.humidity = scale_humidity(data);

  reading.success = 1;
  return;
fail:
  
  printf("Failed to get reading, dying\n");
  reading.success = 0;
}

struct sensor_reading *ICACHE_FLASH_ATTR readDHT(int force) {
    if(force){
        pollDHTCb(NULL);
    } 
    return &reading;
}

void DHTInit(enum sensor_type sensor_type, uint32_t polltime) {
  SENSOR = sensor_type;
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);
  
  printf("DHT Setup for type %d, poll interval of %d\n", sensor_type, (int)polltime);
  
  static os_timer_t dhtTimer;
  os_timer_setfn(&dhtTimer, pollDHTCb, NULL);
  os_timer_arm(&dhtTimer, polltime, 1);
}
