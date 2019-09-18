#include <Arduino.h>

#define configUSE_PREEMPTION 1
#include <FreeRTOS.h>

#include "color.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"

#define PxMATRIX_SPI_SPEED 20000000
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

TaskHandle_t debugTask;
float fps = 0;
long writeTime = 0;
int droppedFrames = 0;
int displayCount = 0;

bool debug = true;

void setup() {
  Serial.begin(115200);

  display.begin(SCAN_RATE);
  display.setDriverChip(FM6126A);
  display.setCursor(0,0);
  display.print("LED Panel Driver v0.1");
  display.setBrightness(255);
  display.setFastUpdate(true);

  xTaskCreatePinnedToCore(
    update,
    "update",
    24000,
    NULL,
    4,
    &updateTask,
    0
  );

  xTaskCreatePinnedToCore(
    [](void *arg){
      for (;;) {
        vTaskDelay(8000);
        Serial.printf("Cycles:\t%d\n", displayCount / 8);
        Serial.printf("FPS:\t%f\n", fps);
        Serial.printf("Write Time:\t%d\n", writeTime);
        Serial.printf("Dropped Frames:\t%d\n", droppedFrames / 8);
        droppedFrames = 0;
        displayCount = 0;
      }
    },
    "debug",
    8000,
    NULL,
    5,
    &debugTask,
    0
  );
}

void loop() {
  display.display(0);
  displayCount++;
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

long lastUpdate = 0;

void update(void *arg) {

  // updateReady = xSemaphoreCreateBinary();
  
  hspi.begin(P_HSPI_SO, P_HSPI_SD, P_HSPI_CLK, P_HSPI_CS,
    DISPLAY_BUFFER_SIZE, 
    spiRXCallback);

  long then = micros();

  for (;;) {
    // https://github.com/espressif/arduino-esp32/issues/922#issuecomment-413829864
    TIMERG0.wdt_wprotect=TIMG_WDT_WKEY_VALUE;
    TIMERG0.wdt_feed=1;
    TIMERG0.wdt_wprotect=0;

    then = micros();

    uint32_t frameCount = 0;
    if ( xTaskNotifyWait(pdFALSE, ULONG_MAX, &frameCount, 100) == pdFALSE) continue;

    if (frameCount > 1) {
      droppedFrames += frameCount - 1;
    }

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

    long now = micros();
    writeTime = (now - then + writeTime) / 2;

    fps = 1000000. / (float)(now - lastUpdate);
    lastUpdate = now;

    if ( display.showBuffer() == true ) {
      Serial.println("Show buffer already pending");
    }
  }

}