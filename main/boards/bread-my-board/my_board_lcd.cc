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
//#define I2C_MASTER_NUM              0
//#define I2C_MASTER_FREQ_HZ          100000  // 200kHz
#define I2C_TIMEOUT_MS              1000


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

    void InitializeI2c() {
        ESP_LOGI(TAG, "初始化I2C总线...");
        
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
        vTaskDelay(pdMS_TO_TICKS(300));  // 等待100ms
    }
    

    void i2c_scan_devices() {

        ESP_LOGI(TAG, "开始扫描I2C设备...");
    
        int devices_found = 0;
        
        // 直接为0x18创建设备句柄
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = 0x18,
            .scl_speed_hz = 100000,
        };
        i2c_master_dev_handle_t dev = NULL;
        
        if (i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &dev) == ESP_OK) {
            // 尝试读取一个寄存器
            uint8_t reg = 0xFD;  // ID寄存器
            uint8_t val;
            if (i2c_master_transmit_receive(dev, &reg, 1, &val, 1, 1000) == ESP_OK) {
                ESP_LOGI(TAG, "发现ES8311设备，地址: 0x18，ID: 0x%02x", val);
                devices_found++;
            }
            i2c_master_bus_rm_device(dev);
        }
        
        if (devices_found == 0) {
            ESP_LOGW(TAG, "未检测到任何I2C设备！请检查连接");
        } else {
            ESP_LOGI(TAG, "共发现 %d 个I2C设备", devices_found);
        }

    }

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

    //======================================================================

    bool verify_es8311_communication() {
        ESP_LOGI(TAG, "验证与ES8311的通信...");
        
        // 创建ES8311设备句柄
        i2c_device_config_t es8311_dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = AUDIO_CODEC_ES8311_ADDR,
            .scl_speed_hz = 100000,  // 100kHz
        };
        
        i2c_master_dev_handle_t es8311_dev = NULL;
        esp_err_t ret = i2c_master_bus_add_device(i2c_bus_, &es8311_dev_cfg, &es8311_dev);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "创建ES8311设备句柄失败: %s", esp_err_to_name(ret));
            return false;
        }
        
        // 读取几个寄存器
        const uint8_t regs_to_read[] = {0x00, 0x01, 0x02, 0xFD};
        
        for (size_t i = 0; i < sizeof(regs_to_read); i++) {
            uint8_t reg_addr = regs_to_read[i];
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

    // ES8311音频编解码器诊断函数
    void DiagnoseES8311Audio() {
        ESP_LOGI(TAG, "=== ES8311音频编解码器诊断开始 ===");
        
        // 获取音频编解码器实例
        auto codec = Board::GetInstance().GetAudioCodec();
        Es8311AudioCodec* es8311_codec = dynamic_cast<Es8311AudioCodec*>(codec);
        
        // 检查音频编解码器类型
        if (!es8311_codec) {
            ESP_LOGE(TAG, "当前音频编解码器不是ES8311类型!");
            return;
        }
        
        // 1. 检查I2C通信状态
        ESP_LOGI(TAG, "1. 检查I2C通信状态...");
        i2c_device_config_t es8311_dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = AUDIO_CODEC_ES8311_ADDR,
            .scl_speed_hz = 100000,  // 100kHz
        };
        
        i2c_master_dev_handle_t es8311_dev = NULL;
        esp_err_t ret = i2c_master_bus_add_device(i2c_bus_, &es8311_dev_cfg, &es8311_dev);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "创建ES8311设备句柄失败: %s", esp_err_to_name(ret));
            return;
        }
        
        // 2. 读取芯片ID和关键寄存器
        ESP_LOGI(TAG, "2. 读取芯片ID和关键寄存器...");
        
        // 定义需要读取的重要寄存器
        const struct {
            uint8_t addr;
            const char* name;
        } registers[] = {
            {0xFD, "芯片ID寄存器"},
            {0x00, "复位寄存器"},
            {0x01, "时钟管理寄存器1"},
            {0x02, "时钟管理寄存器2"},
            {0x03, "时钟管理寄存器3"},
            {0x17, "ADC控制寄存器"},
            {0x32, "DAC音量寄存器"},
            {0x31, "DAC静音寄存器"},
            {0x44, "GPIO控制寄存器"}
        };
        
        bool id_ok = false;
        for (const auto& reg : registers) {
            uint8_t reg_addr = reg.addr;
            uint8_t reg_val = 0;
            
            ret = i2c_master_transmit_receive(es8311_dev, &reg_addr, 1, &reg_val, 1, 1000);
            
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "  %s (0x%02X): 0x%02X", reg.name, reg.addr, reg_val);
                // 验证芯片ID (0xFD寄存器应为0x83)
                if (reg.addr == 0xFD && reg_val == 0x83) {
                    id_ok = true;
                }
            } else {
                ESP_LOGE(TAG, "  读取寄存器0x%02X失败: %s", reg.addr, esp_err_to_name(ret));
            }
        }
        
        if (!id_ok) {
            ESP_LOGW(TAG, "芯片ID验证失败！可能不是ES8311或芯片有问题");
        }
        
        // 3. 尝试强制激活音频输出
        ESP_LOGI(TAG, "3. 尝试激活音频输出...");
        codec->EnableOutput(true);
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // 4. 验证PA控制引脚状态
        ESP_LOGI(TAG, "4. 验证PA控制引脚状态...");
        gpio_num_t pa_pin = AUDIO_CODEC_PA_PIN; // 假设定义了此宏，请根据实际情况修改
        if (pa_pin != GPIO_NUM_NC) {
            int level = gpio_get_level(pa_pin);
            ESP_LOGI(TAG, "  功放控制引脚 (GPIO %d) 电平: %d", pa_pin, level);
        } else {
            ESP_LOGI(TAG, "  功放控制引脚未配置");
        }
        
        // 5. 尝试写入DAC音量寄存器提高音量
        ESP_LOGI(TAG, "5. 尝试直接设置DAC音量...");
        uint8_t write_buf[2] = {0x32, 0xC0}; // DAC音量寄存器，较高音量
        ret = i2c_master_transmit(es8311_dev, write_buf, 2, 1000);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  设置DAC音量成功");
        } else {
            ESP_LOGE(TAG, "  设置DAC音量失败: %s", esp_err_to_name(ret));
        }
        
        // 6. 清理资源
        i2c_master_bus_rm_device(es8311_dev);
        
        ESP_LOGI(TAG, "=== ES8311音频编解码器诊断完成 ===");
        
        // 7. 尝试输出一些音频数据
        ESP_LOGI(TAG, "7. 尝试播放测试音...");
        // 创建一个简单的正弦波测试音
        const int sample_count = 480; // 30ms @ 16kHz
        int16_t test_tone[sample_count];
        
        // 生成1kHz正弦波
        for (int i = 0; i < sample_count; i++) {
            test_tone[i] = 16000 * sin(2 * M_PI * 1000 * i / 16000.0);
        }
        
        // 尝试播放3次
        for (int i = 0; i < 3; i++) {
            ESP_LOGI(TAG, "  播放测试音 #%d", i+1);
            codec->Write(test_tone, sample_count);
            vTaskDelay(pdMS_TO_TICKS(500)); // 等待500ms
        }
        
        ESP_LOGI(TAG, "诊断程序结束");
    }

    //=======================================================================================



    //紧凑型 wifi 板，lcd板，构造函数
    MyWifiBoardLCD() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        InitializeIot();

        // ********************* i2c 总线初始化 ****************************
        //check_gpio_status();
        //vTaskDelay(pdMS_TO_TICKS(100));


        //InitializeMclk();
        //ns4150_ctrl_enable();
        InitializeI2c();
        vTaskDelay(pdMS_TO_TICKS(100));
        
        i2c_scan_devices();         
        DiagnoseES8311Audio();
        
        //verify_es8311_communication();
        //diagnose_es8311_issue();
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
        // 2. 实例化 ES8311 编解码器
        static Es8311AudioCodec audio_codec(
            i2c_bus_,                // I2C 句柄
            I2C_NUM_0,                    // I2C 端口号
            AUDIO_INPUT_SAMPLE_RATE,      // 输入采样率
            AUDIO_OUTPUT_SAMPLE_RATE,     // 输出采样率
            AUDIO_CODEC_MCLK_PIN,     // MCLK
            //GPIO_NUM_NC,
            AUDIO_CODEC_I2S_SCLK_PIN,     // BCLK (SCLK)
            AUDIO_CODEC_I2S_LRCK_PIN,     // WS (LRCK)
            AUDIO_CODEC_I2S_ASDOUT_PIN,   // DOUT
            AUDIO_CODEC_I2S_DSDIN_PIN,    // DIN
            AUDIO_CODEC_NS4150_PIN,                  // PA_PIN（如有功放控制脚，否则用 GPIO_NUM_NC）
            AUDIO_CODEC_ES8311_ADDR       // ES8311 I2C 地址
        );
        return &audio_codec;
    }
    
// ****************** 此处决定调用哪一个音频编、解码器 ***********************************
    


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
