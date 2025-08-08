#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 16000



// 音频编解码器 引脚定义
//#define AUDIO_CODEC_ES8311_RX_TX          // 如果使用 es8311 RX + TX，不用处理，否则注释掉本行
#define AUDIO_CODEC_ES7210_RX_ES8311_TX   // 如果使用 es7210-RX + es8311-TX，不用处理，否则注释掉本行

#ifdef AUDIO_CODEC_ES8311_RX_TX
// ES8311 RX + TX  引脚定义 
#define AUDIO_CODEC_I2C_NUM         (0)
#define AUDIO_CODEC_ES8311_ADDR     (0x30)  // es8311地址
#define AUDIO_CODEC_I2C_SDA_PIN     GPIO_NUM_1
#define AUDIO_CODEC_I2C_SCL_PIN     GPIO_NUM_2

#define AUDIO_CODEC_MCLK_PIN        GPIO_NUM_42
#define AUDIO_CODEC_I2S_DO_PIN      GPIO_NUM_39
#define AUDIO_CODEC_I2S_DI_PIN      GPIO_NUM_38
#define AUDIO_CODEC_I2S_SCLK_PIN    GPIO_NUM_40
#define AUDIO_CODEC_I2S_LRCK_PIN    GPIO_NUM_41
#define AUDIO_CODEC_NS4150_PIN      GPIO_NUM_21

#elif defined(AUDIO_CODEC_ES7210_RX_ES8311_TX)
// es8311-TX + es7210-RX  引脚定义
#define AUDIO_CODEC_I2C_NUM         (0)
#define AUDIO_CODEC_ES8311_ADDR     (0x30)  // es8311地址
#define AUDIO_CODEC_ES7210_I2C_ADDR (0x82)  // es7210地址
#define AUDIO_CODEC_I2C_SDA_PIN     GPIO_NUM_1
#define AUDIO_CODEC_I2C_SCL_PIN     GPIO_NUM_2

#define AUDIO_CODEC_MCLK_PIN        GPIO_NUM_18
#define AUDIO_CODEC_I2S_DO_PIN      GPIO_NUM_38
#define AUDIO_CODEC_I2S_DI_PIN      GPIO_NUM_21
#define AUDIO_CODEC_I2S_SCLK_PIN    GPIO_NUM_20
#define AUDIO_CODEC_I2S_LRCK_PIN    GPIO_NUM_19
#define AUDIO_CODEC_NS4150_PIN      GPIO_NUM_46
#endif



// // 如果使用 Duplex I2S 模式，请注释下面一行
// #define AUDIO_I2S_METHOD_SIMPLEX

// #ifdef AUDIO_I2S_METHOD_SIMPLEX
// #define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
// #define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
// #define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6

// #define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
// #define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
// #define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16

// #else
// #define AUDIO_I2S_GPIO_WS   GPIO_NUM_4
// #define AUDIO_I2S_GPIO_BCLK GPIO_NUM_5
// #define AUDIO_I2S_GPIO_DIN  GPIO_NUM_6
// #define AUDIO_I2S_GPIO_DOUT GPIO_NUM_7
// #endif



// 按键相关 引脚定义
#define BUILTIN_LED_GPIO        GPIO_NUM_40
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define TOUCH_BUTTON_GPIO       GPIO_NUM_NC
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC     // 音量加 ++++++ 引脚暂用 20 ++++++
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC     // 音量减 ------ 引脚暂用 19 ------



// sd卡 引脚定义
// 采用 sd模式，(SD Mode)，否则注释下面一行
#define SD_CARD_SD_MODE

#ifdef SD_CARD_SD_MODE
// SD Mode
#define SD_CARD_PIN_CMD     GPIO_NUM_3
#define SD_CARD_PIN_CLK     GPIO_NUM_4
#define SD_CARD_PIN_D0      GPIO_NUM_5

#else
#endif



// //Camera Config
#define CAMERA_PIN_D0 GPIO_NUM_11  // Y2 (Pin 20)
#define CAMERA_PIN_D1 GPIO_NUM_9   // Y3 (Pin 22) 
#define CAMERA_PIN_D2 GPIO_NUM_8   // Y4 (Pin 23)
#define CAMERA_PIN_D3 GPIO_NUM_10  // Y5 (Pin 21)
#define CAMERA_PIN_D4 GPIO_NUM_12  // Y6 (Pin 19)
#define CAMERA_PIN_D5 GPIO_NUM_14  // Y7 (Pin 17)
#define CAMERA_PIN_D6 GPIO_NUM_17  // Y8 (Pin 15)
#define CAMERA_PIN_D7 GPIO_NUM_16  // Y9 (Pin 13)
#define CAMERA_PIN_XCLK GPIO_NUM_15 // XCLK (Pin 14)
#define CAMERA_PIN_PCLK GPIO_NUM_13 // PCLK (Pin 18)
#define CAMERA_PIN_VSYNC GPIO_NUM_6 // VSYNC (Pin 8)
#define CAMERA_PIN_HREF GPIO_NUM_7  // HREF (Pin 10)
#define CAMERA_PIN_SIOC GPIO_NUM_2  // SIOC (Pin 6) - I2C SCL
#define CAMERA_PIN_SIOD GPIO_NUM_1  // SIOD (Pin 3) - I2C SDA
#define CAMERA_PIN_PWDN GPIO_NUM_NC // PWDN (Pin 9) - 通过R6上拉
#define CAMERA_PIN_RESET GPIO_NUM_NC // RESET (Pin 7) - 通过R5上拉
#define XCLK_FREQ_HZ 20000000



// 液晶屏 引脚定义
//#define LCD_TYPE_ST7789_SPI_240X320_my      // 使用st7789 lcd屏幕，不用处理，否则注释掉本行
#define OLED_TYPE_SSD1306_I2C_128X64_test     // 使用ssd1306 oled屏幕，不用处理，否则注释掉本行

#ifdef LCD_TYPE_ST7789_SPI_240X320_my
// ST7789液晶屏 240*320 引脚定义

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_42
#define DISPLAY_MOSI_PIN      GPIO_NUM_48
#define DISPLAY_CLK_PIN       GPIO_NUM_47
#define DISPLAY_DC_PIN        GPIO_NUM_40
#define DISPLAY_RST_PIN       GPIO_NUM_39
#define DISPLAY_CS_PIN        GPIO_NUM_41
#elif defined(OLED_TYPE_SSD1306_I2C_128X64_test)
// ssd1306液晶屏 128*64 引脚定义
#define DISPLAY_I2C_NUM     (1)
#define DISPLAY_ADDR        (0x3C)      // ssd1306地址
#define DISPLAY_SDA_PIN     GPIO_NUM_47
#define DISPLAY_SCL_PIN     GPIO_NUM_48

#define DISPLAY_WIDTH       128
#define DISPLAY_HEIGHT      64
#define DISPLAY_MIRROR_X    true
#define DISPLAY_MIRROR_Y    true

#else
// ST7789液晶屏 240*320 引脚定义 ==== es8311
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_18
#define DISPLAY_MOSI_PIN      GPIO_NUM_10
#define DISPLAY_CLK_PIN       GPIO_NUM_11
#define DISPLAY_DC_PIN        GPIO_NUM_9
#define DISPLAY_RST_PIN       GPIO_NUM_3
#define DISPLAY_CS_PIN        GPIO_NUM_8

#endif



#if defined(LCD_TYPE_ST7789_SPI_240X320_my)
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  320
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 0
#endif


#endif // _BOARD_CONFIG_H_
