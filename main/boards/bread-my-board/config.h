#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 16000

// ES8311 引脚定义 ************************************************
#define AUDIO_CODEC_ES8311_ADDR     0x18
#define AUDIO_CODEC_I2C_SDA_PIN     GPIO_NUM_1
#define AUDIO_CODEC_I2C_SCL_PIN     GPIO_NUM_2

#define AUDIO_CODEC_MCLK_PIN    GPIO_NUM_42

#define AUDIO_CODEC_I2S_ASDOUT_PIN  GPIO_NUM_38
#define AUDIO_CODEC_I2S_DSDIN_PIN   GPIO_NUM_39
#define AUDIO_CODEC_I2S_SCLK_PIN    GPIO_NUM_40
#define AUDIO_CODEC_I2S_LRCK_PIN    GPIO_NUM_41


#define AUDIO_CODEC_NS4150_PIN    GPIO_NUM_21

// ***************************************************************



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
#define BUILTIN_LED_GPIO        GPIO_NUM_48
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define TOUCH_BUTTON_GPIO       GPIO_NUM_NC
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_20     // 音量加 ++++++ 引脚暂用 20 ++++++
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_19     // 音量减 ------ 引脚暂用 19 ------


// ST7789 引脚定义
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_18

#define DISPLAY_MOSI_PIN      GPIO_NUM_10
#define DISPLAY_CLK_PIN       GPIO_NUM_11
#define DISPLAY_DC_PIN        GPIO_NUM_9
#define DISPLAY_RST_PIN       GPIO_NUM_3
#define DISPLAY_CS_PIN        GPIO_NUM_8



//#ifdef CONFIG_LCD_ST7789_240X320
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
//#endif


#endif // _BOARD_CONFIG_H_
