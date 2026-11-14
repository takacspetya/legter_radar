#include "display.h"

#include <algorithm>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace {

constexpr spi_host_device_t kHost = SPI2_HOST;
constexpr int kStripLines = 40;

esp_lcd_panel_handle_t panel;
SemaphoreHandle_t transferDone;

bool onTransferDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *) {
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR(transferDone, &woken);
  return woken == pdTRUE;
}

}  // namespace

void displayBegin() {
  transferDone = xSemaphoreCreateCounting(kLcdSize / kStripLines + 1, 0);

  if (CONFIG_RADAR_LCD_BL >= 0) {
    gpio_set_direction((gpio_num_t)CONFIG_RADAR_LCD_BL, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)CONFIG_RADAR_LCD_BL, 0);
  }

  spi_bus_config_t bus = {};
  bus.sclk_io_num = CONFIG_RADAR_LCD_SCK;
  bus.mosi_io_num = CONFIG_RADAR_LCD_MOSI;
  bus.miso_io_num = -1;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = kLcdSize * kStripLines * sizeof(uint16_t);
  ESP_ERROR_CHECK(spi_bus_initialize(kHost, &bus, SPI_DMA_CH_AUTO));

  esp_lcd_panel_io_spi_config_t ioCfg = {};
  ioCfg.cs_gpio_num = CONFIG_RADAR_LCD_CS;
  ioCfg.dc_gpio_num = CONFIG_RADAR_LCD_DC;
  ioCfg.spi_mode = 0;
  ioCfg.pclk_hz = CONFIG_RADAR_LCD_SPI_MHZ * 1000 * 1000;
  ioCfg.trans_queue_depth = 10;
  ioCfg.on_color_trans_done = onTransferDone;
  ioCfg.lcd_cmd_bits = 8;
  ioCfg.lcd_param_bits = 8;
  esp_lcd_panel_io_handle_t io;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)kHost, &ioCfg, &io));

  esp_lcd_panel_dev_config_t panelCfg = {};
  panelCfg.reset_gpio_num = CONFIG_RADAR_LCD_RST;
#ifdef CONFIG_RADAR_LCD_BGR
  panelCfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
#else
  panelCfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
#endif
  panelCfg.bits_per_pixel = 16;
  ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io, &panelCfg, &panel));

  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
#ifdef CONFIG_RADAR_LCD_INVERT
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
#endif
#ifdef CONFIG_RADAR_LCD_MIRROR_X
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, true, false));
#endif
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
}

void displayFlush(const uint16_t *fb) {
  int strips = 0;
  for (int y = 0; y < kLcdSize; y += kStripLines) {
    int h = std::min(kStripLines, kLcdSize - y);
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, y, kLcdSize, y + h, fb + y * kLcdSize));
    strips++;
  }
  while (strips--) xSemaphoreTake(transferDone, portMAX_DELAY);

  // Háttérvilágítás csak az első kép után, hogy ne villanjon szemét
  static bool backlightOn = false;
  if (!backlightOn && CONFIG_RADAR_LCD_BL >= 0) {
    gpio_set_level((gpio_num_t)CONFIG_RADAR_LCD_BL, 1);
    backlightOn = true;
  }
}
