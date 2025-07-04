#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/spi_common.h>
#include <driver/ledc.h>

// *******************************************************
#include "audio_codecs/es8311_audio_codec.h"
    // ... 其他 include

// // 函数声明
// static void mclk_task(void *arg);


// I2C配置
#define I2C_MASTER_NUM              0
#define I2C_MASTER_FREQ_HZ          400000  // 400kHz
#define I2C_TIMEOUT_MS              1000


// // I2C初始化函数
// static esp_err_t i2c_master_init(void)
// {
//     // 新版I2C Master配置
//     i2c_master_bus_config_t i2c_mst_config = {
//         .clk_source = I2C_CLK_SRC_DEFAULT,
//         .i2c_port = I2C_MASTER_NUM,
//         .scl_io_num = AUDIO_CODEC_I2C_SDC_PIN,
//         .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
//         .glitch_ignore_cnt = 7,
//         .flags.enable_internal_pullup = true,
//     };
    
//     // 创建I2C Master总线
//     esp_err_t ret = i2c_new_master_bus(&i2c_mst_config, &bus_handle);
//     if (ret != ESP_OK) {
//         ESP_LOGE("I2C", "I2C总线创建失败: %s", esp_err_to_name(ret));
//         return ret;
//     }
    
//     // 配置MCLK引脚
//     gpio_config_t io_conf = {
//         .pin_bit_mask = (1ULL << AUDIO_CODEC_I2C_MCLK_PIN),
//         .mode = GPIO_MODE_OUTPUT,
//         .pull_up_en = GPIO_PULLUP_DISABLE,
//         .pull_down_en = GPIO_PULLDOWN_DISABLE,
//         .intr_type = GPIO_INTR_DISABLE,
//     };
//     gpio_config(&io_conf);
    
//     // 启动MCLK输出任务
//     xTaskCreate(mclk_task, "mclk_task", 2048, NULL, 5, NULL);
    
//     ESP_LOGI("I2C", "I2C主机初始化成功");
//     return ESP_OK;
// }

// // MCLK输出任务
// static void mclk_task(void *arg)
// {
//     ESP_LOGI("MCLK", "开始输出MCLK信号");
    
//     // 使用硬件时钟输出代替软件模拟
//     // 配置MCLK为时钟输出
//     // 注意：ESP32-S3可以使用LEDC外设产生时钟信号
    
//     // 配置LEDC定时器
//     ledc_timer_config_t ledc_timer = {
//         .speed_mode = LEDC_LOW_SPEED_MODE,
//         .duty_resolution = LEDC_TIMER_1_BIT, // 设置为1位分辨率，产生50%占空比
//         .timer_num = LEDC_TIMER_0,
//         .freq_hz = 12000000, // 12MHz MCLK for ES8311
//         .clk_cfg = LEDC_AUTO_CLK,
//     };
//     ledc_timer_config(&ledc_timer);
    
//     // 配置LEDC通道
//     ledc_channel_config_t ledc_channel = {
//         .gpio_num = AUDIO_CODEC_I2C_MCLK_PIN,
//         .speed_mode = LEDC_LOW_SPEED_MODE,
//         .channel = LEDC_CHANNEL_0,
//         .timer_sel = LEDC_TIMER_0,
//         .duty = 1, // 50%占空比 (对于1位分辨率，值为1表示50%)
//         .hpoint = 0,
//     };
//     ledc_channel_config(&ledc_channel);
    
//     ESP_LOGI("MCLK", "MCLK硬件时钟配置完成");
    
//     // 任务完成后删除自己
//     vTaskDelete(NULL);
// }

// // 扫描I2C总线上的所有设备
// static void i2c_scan_devices(void)
// {
//     ESP_LOGI("I2C", "开始扫描I2C设备...");
//     uint8_t devices_found = 0;
    
//     for (uint8_t i = 1; i < 128; i++) {
//         // 直接使用i2c_master_probe函数检测设备
//         esp_err_t ret = i2c_master_probe(bus_handle, i, I2C_TIMEOUT_MS);
        
//         if (ret == ESP_OK) {
//             ESP_LOGI("I2C", "检测到设备: 0x%02x", i);
//             devices_found++;
            
//             // 如果是ES8311地址，特别标记
//             if (i == AUDIO_CODEC_ES8311_ADDR) {
//                 ESP_LOGI("I2C", "找到ES8311编解码器! (地址: 0x%02x)", i);
//             }
//         }
        
//         // 添加短暂延时，避免I2C总线过载
//         vTaskDelay(pdMS_TO_TICKS(10));
//     }
    
//     if (devices_found == 0) {
//         ESP_LOGW("I2C", "未检测到任何I2C设备！请检查连接");
//     } else {
//         ESP_LOGI("I2C", "扫描完成, 共发现 %d 个设备", devices_found);
//     }
// }




#define TAG "my_board_lcd"

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);

// 面板板，wifi板，lcd板，紧凑型
class MyWifiBoardLCD : public WifiBoard {
private:
 
    Button boot_button_;
    LcdDisplay* display_;

    // // 全局I2C总线句柄 *****************************************************

    i2c_master_bus_handle_t i2c_bus_ = nullptr;  // 实例变量而非静态变量
    
    void InitializeMclk() {
        ESP_LOGI(TAG, "开始配置MCLK...");
        
        gpio_num_t mclk_pin = AUDIO_CODEC_I2C_MCLK_PIN;
        uint32_t freq = 24000000;  // 24MHz
        
        ledc_timer_config_t ledc_timer = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = LEDC_TIMER_1_BIT,
            .timer_num = LEDC_TIMER_0,
            .freq_hz = freq,
            .clk_cfg = LEDC_AUTO_CLK
        };
        ledc_timer_config(&ledc_timer);
        
        ledc_channel_config_t ledc_channel = {
            .gpio_num = mclk_pin,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_0,
            .timer_sel = LEDC_TIMER_0,
            .duty = 1,  // 50%占空比
            .hpoint = 0
        };
        ledc_channel_config(&ledc_channel);
        
        ESP_LOGI(TAG, "MCLK配置完成\n");
    }
    
    void InitializeI2c() {
        ESP_LOGI(TAG, "初始化I2C总线...");
        
        i2c_master_bus_config_t i2c_mst_config = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SDC_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = true
            }
        };
        
        esp_err_t ret = i2c_new_master_bus(&i2c_mst_config, &i2c_bus_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2C总线初始化失败: %s", esp_err_to_name(ret));
            return;
        }
        
        ESP_LOGI(TAG, "I2C总线初始化成功\n");
        
        // 扫描I2C设备
        //i2c_scan_devices();
    }
    
    void i2c_scan_devices() {
        ESP_LOGI(TAG, "开始扫描I2C设备...");
        int devices_found = 0;
        
        for (uint8_t i = 0x03; i < 0x78; i++) {
            esp_err_t ret = i2c_master_probe(i2c_bus_, i, I2C_TIMEOUT_MS);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "找到I2C设备，地址: 0x%02x", i);
                devices_found++;
            }
        }
        
        if (devices_found == 0) {
            ESP_LOGW(TAG, "未检测到任何I2C设备！请检查连接");
        } else {
            ESP_LOGI(TAG, "共发现 %d 个I2C设备", devices_found);
        }

        // 扫描到设备后，添加延迟再进行通信
        if (devices_found > 0) {
            ESP_LOGI(TAG, "等待ES8311芯片稳定...");
            vTaskDelay(pdMS_TO_TICKS(100));  // 等待100ms
        }
    }




    // static i2c_master_bus_handle_t bus_handle = nullptr;

    // //初始化 i2c 总线
    // void InitializeI2c() {
    //     // 新版I2C Master配置
    //     i2c_master_bus_config_t i2c_mst_config = {
    //         .clk_source = I2C_CLK_SRC_DEFAULT,
    //         .i2c_port = I2C_MASTER_NUM,
    //         .scl_io_num = AUDIO_CODEC_I2C_SDC_PIN,
    //         .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
    //         .glitch_ignore_cnt = 7,
    //         .flags.enable_internal_pullup = true,
    //     };
        
    //     // 创建I2C Master总线
    //     esp_err_t ret = i2c_new_master_bus(&i2c_mst_config, &bus_handle);
    //     if (ret != ESP_OK) {
    //         ESP_LOGE("I2C", "I2C总线创建失败: %s", esp_err_to_name(ret));
    //         //return ret;
    //     }
        
    //     // 配置MCLK引脚
    //     gpio_config_t io_conf = {
    //         .pin_bit_mask = (1ULL << AUDIO_CODEC_I2C_MCLK_PIN),
    //         .mode = GPIO_MODE_OUTPUT,
    //         .pull_up_en = GPIO_PULLUP_DISABLE,
    //         .pull_down_en = GPIO_PULLDOWN_DISABLE,
    //         .intr_type = GPIO_INTR_DISABLE,
    //     };
    //     gpio_config(&io_conf);
        
    //     ESP_LOGI("I2C", "I2C主机初始化成功");   

    //     //========================= 输出 MCLK 信号 ====================================

    //     ESP_LOGI("MCLK", "开始输出MCLK信号");
        
    //     // 使用硬件时钟输出代替软件模拟
    //     // 配置MCLK为时钟输出
    //     // 注意：ESP32-S3可以使用LEDC外设产生时钟信号
        
    //     // 配置LEDC定时器
    //     ledc_timer_config_t ledc_timer = {
    //         .speed_mode = LEDC_LOW_SPEED_MODE,
    //         .duty_resolution = LEDC_TIMER_1_BIT, // 设置为1位分辨率，产生50%占空比
    //         .timer_num = LEDC_TIMER_0,
    //         .freq_hz = 12000000, // 12MHz MCLK for ES8311
    //         .clk_cfg = LEDC_AUTO_CLK,
    //     };
    //     ledc_timer_config(&ledc_timer);
        
    //     // 配置LEDC通道
    //     ledc_channel_config_t ledc_channel = {
    //         .gpio_num = AUDIO_CODEC_I2C_MCLK_PIN,
    //         .speed_mode = LEDC_LOW_SPEED_MODE,
    //         .channel = LEDC_CHANNEL_0,
    //         .timer_sel = LEDC_TIMER_0,
    //         .duty = 1, // 50%占空比 (对于1位分辨率，值为1表示50%)
    //         .hpoint = 0,
    //     };
    //     ledc_channel_config(&ledc_channel);
        
    //     ESP_LOGI("MCLK", "MCLK硬件时钟配置完成");
    // }


    // // 扫描I2C总线上的所有设备
    // void i2c_scan_devices(void) {
    //     ESP_LOGI("I2C", "开始扫描I2C设备...");
    //     uint8_t devices_found = 0;
        
    //     for (uint8_t i = 1; i < 128; i++) {
    //         // 直接使用i2c_master_probe函数检测设备
    //         esp_err_t ret = i2c_master_probe(bus_handle, i, I2C_TIMEOUT_MS);
            
    //         if (ret == ESP_OK) {
    //             ESP_LOGI("I2C", "检测到设备: 0x%02x", i);
    //             devices_found++;
                
    //             // 如果是ES8311地址，特别标记
    //             if (i == AUDIO_CODEC_ES8311_ADDR) {
    //                 ESP_LOGI("I2C", "找到ES8311编解码器! (地址: 0x%02x)", i);
    //             }
    //         }
            
    //         // 添加短暂延时，避免I2C总线过载
    //         vTaskDelay(pdMS_TO_TICKS(10));
    //     }
        
    //     if (devices_found == 0) {
    //         ESP_LOGW("I2C", "未检测到任何I2C设备！请检查连接");
    //     } else {
    //         ESP_LOGI("I2C", "扫描完成, 共发现 %d 个设备", devices_found);
    //     }
    // }

    // // ********************************************************************

    //初始化 spi 总线
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    //初始化液晶屏
    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
#if defined(LCD_TYPE_ILI9341_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
#elif defined(LCD_TYPE_GC9A01_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(panel_io, &panel_config, &panel));
        gc9a01_vendor_config_t gc9107_vendor_config = {
            .init_cmds = gc9107_lcd_init_cmds,
            .init_cmds_size = sizeof(gc9107_lcd_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t),
        };        
#else
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
#endif
        
        esp_lcd_panel_reset(panel);
 

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
#ifdef  LCD_TYPE_GC9A01_SERIAL
        panel_config.vendor_config = &gc9107_vendor_config;
#endif
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_16_4,
                                        .icon_font = &font_awesome_16_4,
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
                                        .emoji_font = font_emoji_32_init(),
#else
                                        .emoji_font = DISPLAY_HEIGHT >= 240 ? font_emoji_64_init() : font_emoji_32_init(),
#endif
                                    });
    }


    //初始化 开机按钮，点击开机按钮，重置 wifi 配置，并进入聊天状态
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Screen"));
        thing_manager.AddThing(iot::CreateThing("Lamp"));
    }

public:
    //检查 GPIO 42 状态 ======================================================================
    void check_gpio_status() {
        ESP_LOGI(TAG, "检查GPIO 42状态...");
        
        // 检查GPIO状态
        gpio_config_t io_conf;
        esp_err_t ret = gpio_get_config(GPIO_NUM_42, &io_conf);
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "GPIO 42配置: mode=%d, pull_up=%d, pull_down=%d, intr_type=%d", 
                    io_conf.mode, io_conf.pull_up_en, io_conf.pull_down_en, io_conf.intr_type);
        } else {
            ESP_LOGI(TAG, "无法获取GPIO 42配置: %s", esp_err_to_name(ret));
        }
        
        // 尝试重置GPIO
        gpio_reset_pin(GPIO_NUM_42);
        ESP_LOGI(TAG, "已重置GPIO 42");
    }
    //=======================================================================================
    //紧凑型 wifi 板，lcd板，构造函数
    MyWifiBoardLCD() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        InitializeIot();

        // ********************* i2c 总线初始化 ****************************
        check_gpio_status();
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        InitializeMclk();
        InitializeI2c();
        i2c_scan_devices();
        // ****************************************************************

        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            GetBacklight()->RestoreBrightness();
        }
        
    }

    //获取 led 灯
    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }


// ****************** 此处决定调用哪一个音频编、解码器 ***********************************

    //获取 音频编码器


//     virtual AudioCodec* GetAudioCodec() override {
// #ifdef AUDIO_I2S_METHOD_SIMPLEX
//         static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
//             AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, 
//             AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
// #else
//         static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
//             AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
// #endif
//         return &audio_codec;
//     }


    virtual AudioCodec* GetAudioCodec() override {

        // ESP_LOGI(TAG, "******************************* GetAudioCodec my_board_lcd ********************************");
        // // 1. 初始化 I2C 总线（如有需要，通常在板子构造函数里做）
        // static i2c_master_bus_handle_t codec_i2c_bus = nullptr;
        // if (!codec_i2c_bus) {
        //     i2c_master_bus_config_t i2c_bus_cfg = {
        //         .i2c_port = I2C_NUM_0,
        //         .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
        //         .scl_io_num = AUDIO_CODEC_I2C_SDC_PIN,
        //         .clk_source = I2C_CLK_SRC_DEFAULT,
        //         .glitch_ignore_cnt = 7,
        //         .intr_priority = 0,
        //         .trans_queue_depth = 0,
        //         .flags = {
        //         .enable_internal_pullup = 1,
        //         },
        //     };
        //     ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus));
        // }

        // 2. 实例化 ES8311 编解码器
        static Es8311AudioCodec audio_codec(
            i2c_bus_,                // I2C 句柄
            I2C_NUM_0,                    // I2C 端口号
            AUDIO_INPUT_SAMPLE_RATE,      // 输入采样率
            AUDIO_OUTPUT_SAMPLE_RATE,     // 输出采样率
            AUDIO_CODEC_I2C_MCLK_PIN,     // MCLK
            AUDIO_CODEC_I2S_SCLK_PIN,     // BCLK (SCLK)
            AUDIO_CODEC_I2S_LRCK_PIN,     // WS (LRCK)
            AUDIO_CODEC_I2S_ASDOUT_PIN,   // DOUT
            AUDIO_CODEC_I2S_DSDIN_PIN,    // DIN
            GPIO_NUM_NC,                  // PA_PIN（如有功放控制脚，否则用 GPIO_NUM_NC）
            AUDIO_CODEC_ES8311_ADDR,      // ES8311 I2C 地址
            true   //false                // use_mclk, 可选参数
        );
        return &audio_codec;
    }
    

    //获取 液晶屏
    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
        return nullptr;
    }
};

DECLARE_BOARD(MyWifiBoardLCD);
