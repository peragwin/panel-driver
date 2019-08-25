#include <Arduino.h>
#include <FreeRTOS.h>
#include "color.h"

#define PxMATRIX_SPI_SPEED 30000000
#define PxMATRIX_COLOR_DEPTH 16
#include <PxMatrix.h>

#define P_LAT 22
#define P_A 19
#define P_B 23
#define P_C 18
#define P_D 5
#define P_OE 2

#define P_HSPI_CLK GPIO_NUM_26
#define P_HSPI_SD GPIO_NUM_25
#define P_HSPI_CS GPIO_NUM_27
#define P_HSPI_SO GPIO_NUM_15

#define display_draw_time 0
#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32
#define DISPLAY_BUFFER_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT * 3 / 4)
#define SCAN_RATE 16

PxMATRIX display(DISPLAY_WIDTH, DISPLAY_HEIGHT, P_LAT, P_OE, P_A, P_B, P_C, P_D);

#define SPI_DEFAULT_MAX_BUFFER_SIZE DISPLAY_BUFFER_SIZE // 128
#include <SlaveSPI.h>
SlaveSPI hspi(HSPI_HOST);

TaskHandle_t updateTask;
void update(void *arg);
SemaphoreHandle_t updateReady;
int spiRXCallback(void);

void setup() {
  Serial.begin(115200);

  display.begin(SCAN_RATE);
  display.setCursor(0,0);
  display.print("LED Panel Driver v0.1");
  display.setBrightness(255);
  display.setFastUpdate(true);

  xTaskCreatePinnedToCore(
    update,
    "update",
    24000,
    NULL,
    0,
    &updateTask,
    0
  );
}

void loop() {
  display.display(0);
}

Color_RGB8 buffer[DISPLAY_BUFFER_SIZE / 3];
int bufferOffset = 0;

int spiRXCallback(void) {
  // hspi.readToBytes(((uint8_t*)buffer) + bufferOffset, SPI_DEFAULT_MAX_BUFFER_SIZE);
  // bufferOffset += SPI_DEFAULT_MAX_BUFFER_SIZE;
  // bufferOffset %= DISPLAY_BUFFER_SIZE;
  hspi.readToBytes(buffer, DISPLAY_BUFFER_SIZE);

  if (bufferOffset == 0) {
    xTaskNotifyGive(updateTask);
  }
  return 0;
}

void update(void *arg) {

  // updateReady = xSemaphoreCreateBinary();
  
  hspi.begin(P_HSPI_SO, P_HSPI_SD, P_HSPI_CLK, P_HSPI_CS,
    DISPLAY_BUFFER_SIZE, 
    spiRXCallback);

  for (;;) {
    // bool rx = xSemaphoreTake(updateReady, 100);
    if ( xTaskNotifyWait(0, 0, NULL, 100) == pdFALSE) continue;

    int yo = DISPLAY_HEIGHT/2;
    int xo = DISPLAY_WIDTH/2;
    for (int x = 0; x < DISPLAY_WIDTH/2; x++) {
        for (int y = 0; y < DISPLAY_HEIGHT/2; y++) {
            
            int bidx = x + y * DISPLAY_WIDTH/2;
            Color_RGB8 c = buffer[bidx];

            display.drawPixelRGB888(xo+x, yo+y, c.r, c.g, c.b);
            display.drawPixelRGB888(xo-1-x, yo+y, c.r, c.g, c.b);
            display.drawPixelRGB888(xo+x, yo-1-y, c.r, c.g, c.b);
            display.drawPixelRGB888(xo-1-x, yo-1-y, c.r, c.g, c.b);
        }
    }    
  }

}