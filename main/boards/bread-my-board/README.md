# My Board Configuration

这是一个支持ST7789显示屏和ES8311音频编解码器的ESP32板子配置。

## 功能特性

- **ST7789显示屏**: 支持多种分辨率的SPI显示屏
- **ES8311音频编解码器**: 支持双工音频（同时录音和播放）
- **WiFi连接**: 支持WiFi网络连接
- **按钮控制**: 支持Boot按钮控制
- **LED指示**: 内置LED状态指示
- **背光控制**: 显示屏背光PWM控制

## 引脚定义

### 音频引脚 (ES8311)
- `AUDIO_I2S_GPIO_MCLK`: GPIO 0 - 主时钟
- `AUDIO_I2S_GPIO_BCLK`: GPIO 1 - 位时钟
- `AUDIO_I2S_GPIO_WS`: GPIO 2 - 字选择
- `AUDIO_I2S_GPIO_DOUT`: GPIO 3 - 音频输出
- `AUDIO_I2S_GPIO_DIN`: GPIO 4 - 音频输入

### I2C引脚 (ES8311控制)
- `AUDIO_CODEC_I2C_SDA_PIN`: GPIO 5 - I2C数据线
- `AUDIO_CODEC_I2C_SCL_PIN`: GPIO 6 - I2C时钟线
- `AUDIO_CODEC_PA_PIN`: GPIO 7 - 功放使能

### 显示屏引脚 (ST7789)
- `DISPLAY_MOSI_PIN`: GPIO 10 - SPI数据线
- `DISPLAY_CLK_PIN`: GPIO 11 - SPI时钟线
- `DISPLAY_DC_PIN`: GPIO 12 - 数据/命令选择
- `DISPLAY_RST_PIN`: GPIO 13 - 复位信号
- `DISPLAY_CS_PIN`: GPIO 14 - 片选信号
- `DISPLAY_BACKLIGHT_PIN`: GPIO 15 - 背光控制

### 控制引脚
- `BUILTIN_LED_GPIO`: GPIO 8 - 状态LED
- `BOOT_BUTTON_GPIO`: GPIO 9 - 启动按钮

## 配置说明

### 显示屏配置

默认配置为240x240分辨率的ST7789显示屏。如需更改分辨率，请修改`config.h`中的相关定义：

```c
// 当前配置 (240x240)
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_SPI_MODE 0
```

### 音频配置

ES8311配置为双工模式，同时支持录音和播放：

```c
#define AUDIO_INPUT_SAMPLE_RATE  16000   // 输入采样率
#define AUDIO_OUTPUT_SAMPLE_RATE 24000   // 输出采样率
```

## 使用方法

1. 将`my_board.cc`和`config.h`文件放在`main/boards/bread-my-board/`目录下
2. 根据实际硬件连接修改`config.h`中的引脚定义
3. 在编译时选择此板子配置
4. 编译并烧录到ESP32

## 自定义引脚

如需修改引脚定义，请编辑`config.h`文件中的相应宏定义。所有引脚都通过宏定义配置，便于移植到不同的硬件平台。

## 注意事项

1. 确保I2S引脚连接正确，特别是MCLK、BCLK、WS、DOUT、DIN
2. ES8311的I2C地址默认为0x18，如需修改请更改`AUDIO_CODEC_ES8311_ADDR`
3. 显示屏的SPI模式默认为0，某些显示屏可能需要设置为其他模式
4. 背光控制使用PWM，确保GPIO支持PWM功能

## 故障排除

1. **显示屏不显示**: 检查SPI引脚连接和复位信号
2. **音频无输出**: 检查I2S引脚连接和功放使能信号
3. **I2C通信失败**: 检查I2C引脚连接和上拉电阻
4. **编译错误**: 确保所有依赖的头文件路径正确 