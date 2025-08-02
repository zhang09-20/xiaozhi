#include "wifi_board.h"
#include "audio/codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "display/oled_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
//#include "iot/thing_manager.h"
#include "led/single_led.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/spi_common.h>
#include <driver/ledc.h>

#include "assets/lang_config.h"



#include "mcp_server.h"
#include "lamp_controller.h"
#include <esp_lcd_panel_sh1106.h>



// *******************************************************
#include "audio/codecs/es8311_audio_codec.h"

#include "esp32_camera.h"
//#include "audio/codecs/i2s_es7210_audio_codec.h"
#include <math.h>
    // ... 其他 include

// // 函数声明
// static void mclk_task(void *arg);


// I2C配置
//#define I2C_MASTER_NUM              0
//#define I2C_MASTER_FREQ_HZ          100000  // 200kHz
#define I2C_TIMEOUT_MS              1000


#define TAG "my_board_lcd"


#ifdef LCD_TYPE_ST7789_SPI_240X320_my
LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);

#elif defined(OLED_TYPE_SSD1306_I2C_128X64_test)
LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_14_1);

#endif

// 面板板，wifi板，lcd板，紧凑型
class MyWifiBoardLCD : public WifiBoard {
private:

    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    Display* display_ = nullptr;
    Button boot_button_;

    Button touch_button_;
    Button volume_up_button_;
    Button volume_down_button_;

    // 摄像头
    Esp32Camera* camera_;


    // // 全局I2C总线句柄 *****************************************************
    i2c_master_bus_handle_t i2c_bus_ = nullptr;         // 实例变量而非静态变量
    void InitializeI2c() {
        ESP_LOGI(TAG, "初始化codec I2C总线...");     
        i2c_master_bus_config_t i2c_mst_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            //.clk_speed_hz = I2C_MASTER_FREQ_HZ,  // 设置I2C频率为100kHz
            .flags = {
                .enable_internal_pullup = true
            }
        };
        ESP_ERROR_CHECK (i2c_new_master_bus(&i2c_mst_config, &i2c_bus_)); // 创建 I2C 总线

        if(i2c_bus_ == nullptr) {
            ESP_LOGE(TAG, "I2C总线初始化失败");
            return;
        }
        
        ESP_LOGI(TAG, "I2C总线初始化成功\n");
        vTaskDelay(pdMS_TO_TICKS(100));  // 等待100ms，确保i2c从设备上电成功
    }


    void InitializeCamera() {
        camera_config_t config = {};
        config.pin_d0 = CAMERA_PIN_D0;
        config.pin_d1 = CAMERA_PIN_D1;
        config.pin_d2 = CAMERA_PIN_D2;
        config.pin_d3 = CAMERA_PIN_D3;
        config.pin_d4 = CAMERA_PIN_D4;
        config.pin_d5 = CAMERA_PIN_D5;
        config.pin_d6 = CAMERA_PIN_D6;
        config.pin_d7 = CAMERA_PIN_D7;
        config.pin_xclk = CAMERA_PIN_XCLK;
        config.pin_pclk = CAMERA_PIN_PCLK;
        config.pin_vsync = CAMERA_PIN_VSYNC;
        config.pin_href = CAMERA_PIN_HREF;
        config.pin_sccb_sda = CAMERA_PIN_SIOD;  
        config.pin_sccb_scl = CAMERA_PIN_SIOC;
        config.sccb_i2c_port = 0;
        config.pin_pwdn = CAMERA_PIN_PWDN;
        config.pin_reset = CAMERA_PIN_RESET;
        config.xclk_freq_hz = XCLK_FREQ_HZ;
        config.pixel_format = PIXFORMAT_RGB565;
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
        camera_ = new Esp32Camera(config);
        camera_->SetHMirror(false);
    }


#ifdef OLED_TYPE_SSD1306_I2C_128X64_test
    i2c_master_bus_handle_t display_i2c_bus_ = nullptr;  // 实例变量而非静态变量
    void InitializeDisplayI2c() {
        ESP_LOGI(TAG, "初始化display I2C总线...");
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)1,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));

        if(display_i2c_bus_ == nullptr) {
            ESP_LOGE(TAG, "display I2C总线初始化失败");
            return;
        }    
        ESP_LOGI(TAG, "display I2C总线初始化成功\n");
    }

    void InitializeSsd1306Display() {
        // SSD1306 config
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = DISPLAY_ADDR,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
            .scl_speed_hz = 400 * 1000,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(display_i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = -1;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

#ifdef SH1106
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh1106(panel_io_, &panel_config, &panel_));
#else
        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
#endif
        ESP_LOGI(TAG, "SSD1306 driver installed");

        // Reset the display
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }

        // Set the display to on
        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
            {&font_puhui_14_1, &font_awesome_14_1});
    }

#endif

    // // ********************************************************************

#ifdef LCD_TYPE_ST7789_SPI_240X320_my
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

#endif


    // // 初始化 音量加减按钮 =================================================
    // void InitializeButtons() {
    //     boot_button_.OnClick([this]() {
    //         auto& app = Application::GetInstance();
    //         if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
    //             ResetWifiConfiguration();
    //         }
    //         app.ToggleChatState();
    //     });
    //     touch_button_.OnPressDown([this]() {
    //         Application::GetInstance().StartListening();
    //     });
    //     touch_button_.OnPressUp([this]() {
    //         Application::GetInstance().StopListening();
    //     });

    //     //音量加按键 +10
    //     volume_up_button_.OnClick([this]() {
    //         auto codec = GetAudioCodec();
    //         auto volume = codec->output_volume() + 10;
    //         if (volume > 100) {
    //             volume = 100;
    //         }
    //         codec->SetOutputVolume(volume);
    //         GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
    //     });
    //     //长按 加按键 +> 100 最大音量
    //     volume_up_button_.OnLongPress([this]() {
    //         GetAudioCodec()->SetOutputVolume(100);
    //         GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
    //     });

    //     //音量减按键 -10
    //     volume_down_button_.OnClick([this]() {
    //         auto codec = GetAudioCodec();
    //         auto volume = codec->output_volume() - 10;
    //         if (volume < 0) {
    //             volume = 0;
    //         }
    //         codec->SetOutputVolume(volume);
    //         GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
    //     });
    //     //长按 减按键 -> 0 静音
    //     volume_down_button_.OnLongPress([this]() {
    //         GetAudioCodec()->SetOutputVolume(0);
    //         GetDisplay()->ShowNotification(Lang::Strings::MUTED);
    //     });
    // }

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

public:

    //======================================================================

    // 验证与ES8311的通信，读取寄存器的值
    bool verify_es8311_communication() {
        ESP_LOGI(TAG, "验证与ES8311的通信...");
        
        // 创建ES8311设备句柄
        i2c_device_config_t es8311_dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = 0x18,
            .scl_speed_hz = 100000,  // 100kHz
        };
        
        i2c_master_dev_handle_t es8311_dev = NULL;
        esp_err_t ret = i2c_master_bus_add_device(i2c_bus_, &es8311_dev_cfg, &es8311_dev);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "创建ES8311设备句柄失败: %s", esp_err_to_name(ret));
            return false;
        }
        
        // 读取几个寄存器
        //const uint8_t regs_to_read[] = {0x00, 0x01, 0x02, 0xFD};
        
        for (size_t i = 0x00; i <= 0x1C; i++) {
            uint8_t reg_addr = i;
            uint8_t reg_val = 0;            
            ret = i2c_master_transmit_receive(es8311_dev, &reg_addr, 1, &reg_val, 1, 1000);            
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "读取寄存器0x%02X成功: 0x%02X", reg_addr, reg_val);
            } else {
                ESP_LOGE(TAG, "读取寄存器0x%02X失败: %s", reg_addr, esp_err_to_name(ret));
            }
        }

        for (size_t i = 0x31; i <= 0x37; i++) {
            uint8_t reg_addr = i;
            uint8_t reg_val = 0;            
            ret = i2c_master_transmit_receive(es8311_dev, &reg_addr, 1, &reg_val, 1, 1000);            
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "读取寄存器0x%02X成功: 0x%02X", reg_addr, reg_val);
            } else {
                ESP_LOGE(TAG, "读取寄存器0x%02X失败: %s", reg_addr, esp_err_to_name(ret));
            }
        }

        for (size_t i = 0x44; i <= 0x45; i++) {
            uint8_t reg_addr = i;
            uint8_t reg_val = 0;            
            ret = i2c_master_transmit_receive(es8311_dev, &reg_addr, 1, &reg_val, 1, 1000);            
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "读取寄存器0x%02X成功: 0x%02X", reg_addr, reg_val);
            } else {
                ESP_LOGE(TAG, "读取寄存器0x%02X失败: %s", reg_addr, esp_err_to_name(ret));
            }
        }

        for (size_t i = 0xFD; i <= 0xFF; i++) {
            uint8_t reg_addr = i;
            uint8_t reg_val = 0;            
            ret = i2c_master_transmit_receive(es8311_dev, &reg_addr, 1, &reg_val, 1, 1000);            
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "读取寄存器0x%02X成功: 0x%02X", reg_addr, reg_val);
            } else {
                ESP_LOGE(TAG, "读取寄存器0x%02X失败: %s", reg_addr, esp_err_to_name(ret));
            }
        }
        
        // 清理设备句柄
        i2c_master_bus_rm_device(es8311_dev);
        
        // 如果至少有一个寄存器能读取成功，说明通信正常
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "ES8311通信验证成功\n");
            return true;
        } else {
            ESP_LOGW(TAG, "ES8311通信验证失败\n");
            return false;
        }
    }

    //=======================================================================================


    //面包板 wifi 板，lcd板，构造函数
    MyWifiBoardLCD() : 
        boot_button_(BOOT_BUTTON_GPIO),
        touch_button_(TOUCH_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {

#ifdef LCD_TYPE_ST7789_SPI_240X320_my
        // spi屏幕驱动相关初始化
        InitializeSpi();
        InitializeLcdDisplay();
#elif defined(OLED_TYPE_SSD1306_I2C_128X64_test)
        // i2c屏幕驱动相关初始化
        InitializeDisplayI2c();
        InitializeSsd1306Display();
#endif

        InitializeButtons();
        // ********************* audio_i2c、camera ****************************
        //InitializeCamera();

        InitializeI2c();
        //vTaskDelay(pdMS_TO_TICKS(100));   
        //verify_es8311_communication();
        // ****************************************************************

#ifdef LCD_TYPE_ST7789_SPI_240X320_my
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            GetBacklight()->RestoreBrightness();
        } 
#elif defined(OLED_TYPE_SSD1306_I2C_128X64_test)
#endif

    }

    //获取 led 灯
    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }


// ****************** 此处决定调用哪一个音频编、解码器 ***********************************

    //获取音频编解、码器 1，无音频编码器
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


    static uint8_t test_flag;
    //获取音频编、解码器 2，es8311
    virtual AudioCodec* GetAudioCodec() override {
        // 2. 实例化 ES8311 编解码器
        static Es8311AudioCodec audio_codec(
            i2c_bus_,                   // I2C 句柄
            I2C_NUM_0,                  // I2C 端口号
            AUDIO_INPUT_SAMPLE_RATE,    // 输入采样率
            AUDIO_OUTPUT_SAMPLE_RATE,   // 输出采样率

            AUDIO_CODEC_MCLK_PIN,       // MCLK
            AUDIO_CODEC_I2S_SCLK_PIN,   // BCLK (SCLK)
            AUDIO_CODEC_I2S_LRCK_PIN,   // WS (LRCK)
            AUDIO_CODEC_I2S_DO_PIN,     // DSDIN
            AUDIO_CODEC_I2S_DI_PIN,     // ASDOUT

            AUDIO_CODEC_NS4150_PIN,         // PA_PIN（如有功放控制脚，否则用 GPIO_NUM_NC)
            
            AUDIO_CODEC_ES8311_ADDR,        // ES8311 I2C 地址
            AUDIO_CODEC_ES7210_I2C_ADDR     // ES7210 I2C 地址
        );

        if (!test_flag) {
            verify_es8311_communication();
            test_flag ++;
        }
        
        return &audio_codec;
    }

// ****************** 此处决定调用哪一个音频编、解码器 ***********************************
    

    //获取 液晶屏 
    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
#ifdef LCD_TYPE_ST7789_SPI_240X320_my
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
#elif defined(OLED_TYPE_SSD1306_I2C_128X64_test)
#endif
        return nullptr;
    }
};

uint8_t MyWifiBoardLCD::test_flag = 0;

DECLARE_BOARD(MyWifiBoardLCD);
