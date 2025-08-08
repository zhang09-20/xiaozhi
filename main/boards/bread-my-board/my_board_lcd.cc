#include "wifi_board.h"
#include "audio/codecs/no_audio_codec.h"
//
#include "audio/codecs/box_audio_codec.h"
#include "audio/codecs/es8311_audio_codec.h"
//
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



// *******************************************************
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
//#include "sd_test_io.h"
#if SOC_SDMMC_IO_POWER_EXTERNAL
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#endif

#include <dirent.h>      // 用于 opendir, readdir
#include <algorithm>     // 用于 std::sort, std::transform
#include <cctype>        // 用于 ::tolower
#include "mcp_server.h"
#include "lamp_controller.h"
#include <esp_lcd_panel_sh1106.h>

#include "esp32_camera.h"
//#include "audio/codecs/i2s_es7210_audio_codec.h"
#include <math.h>
    // ... 其他 include
//#include <reg52.h>
// #include <board.h>

// // 函数声明
// static void mclk_task(void *arg);


// I2C配置
//#define I2C_MASTER_NUM              0
//#define I2C_MASTER_FREQ_HZ          100000  // 200kHz
#define I2C_TIMEOUT_MS              1000
#define SD_CARD_MOUNT_POINT         "sd_card"   // 挂载点

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


    // ********************************************************
    void InitializeSDCard(){
        esp_err_t ret;
        sdmmc_card_t* card;

        // 1. fat文件系统 挂载配置
        esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = false,    // 如果格式化失败，不进行格式化
            .max_files = 5,
            .allocation_unit_size = 16 * 1024,
        };

        // 2. 主机配置（默认）
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();

#if CONFIG_EXAMPLE_SDMMC_SPEED_HS   // 根据配置选择速度模式：
        host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
#elif CONFIG_EXAMPLE_SDMMC_SPEED_UHS_I_SDR50
        host.max_freq_khz = SDMMC_FREQ_SDR50;
        host.flags &= ~SDMMC_HOST_FLAG_DDR;
#elif CONFIG_EXAMPLE_SDMMC_SPEED_UHS_I_DDR50
        host.max_freq_khz = SDMMC_FREQ_DDR50;
#elif CONFIG_EXAMPLE_SDMMC_SPEED_UHS_I_SDR104
        host.max_freq_khz = SDMMC_FREQ_SDR104;
        host.flags &= ~SDMMC_HOST_FLAG_DDR;
#endif

        // 3. 电源控制初始化（可选）
#if CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
        sd_pwr_ctrl_ldo_config_t ldo_config = {
            .ldo_chan_id = CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_IO_ID,
        };
        sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;
        
        ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create a new on-chip LDO power control driver");
            return;
        }
        host.pwr_ctrl_handle = pwr_ctrl_handle;
#endif

        //sdmmc_card_t* card;
        // esp_err_t ret = esp_vfs_fat_sdmmc_mount(SD_CARD_MOUNT_POINT, &host, &mount_config, &card);
        // if (ret != ESP_OK) {
        //     ESP_LOGE(TAG, "SD卡挂载失败");
        // }
        // ESP_LOGI(TAG, "SD卡挂载成功");

        // 4. 插槽配置
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

#if EXAMPLE_IS_UHS1 // 设置UHS1标志（如果需要）
        slot_config.flags |= SDMMC_SLOT_FLAG_UHS1;
#endif

#ifdef CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4 // 设置总线宽度
        slot_config.width = 4;
#else
        slot_config.width = 1;
#endif

#ifdef CONFIG_SOC_SDMMC_USE_GPIO_MATRIX     // 配置GPIO引脚（如果支持GPIO矩阵）
        slot_config.clk = CONFIG_EXAMPLE_PIN_CLK;
        slot_config.cmd = CONFIG_EXAMPLE_PIN_CMD;
        slot_config.d0 = CONFIG_EXAMPLE_PIN_D0;
#ifdef CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
        slot_config.d1 = CONFIG_EXAMPLE_PIN_D1;
        slot_config.d2 = CONFIG_EXAMPLE_PIN_D2;
        slot_config.d3 = CONFIG_EXAMPLE_PIN_D3;
#endif
#endif
        slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;   // 启用内部上拉电阻

        // 5. 挂载文件系统
        ESP_LOGI(TAG, "Mounting filesystem");
        ret = esp_vfs_fat_sdmmc_mount(SD_CARD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);    //mount_point


        // 6. 错误处理
        if (ret != ESP_OK) {
            if (ret == ESP_FAIL) {
                ESP_LOGE(TAG, "Failed to mount filesystem. "
                         "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
            } else {
                ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                         "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));

#ifdef CONFIG_EXAMPLE_DEBUG_PIN_CONNECTIONS
                check_sd_card_pins(&config, pin_count);
#endif
            }
            return;
        }

        // 7. 成功后的操作
        ESP_LOGI(TAG, "Filesystem mounted");        
        sdmmc_card_print_info(stdout, card);    // 打印SD卡信息
    }



    bool PlayLocalMusic(const std::string& file_name) {
        ESP_LOGI(TAG, "Playing local music: %s", file_name.c_str());
        
        // 构建完整的文件路径
        std::string file_path = "/sdcard/" + file_name;
        
        // 检查文件是否存在
        struct stat st;
        if (stat(file_path.c_str(), &st) != 0) {
            ESP_LOGE(TAG, "File not found: %s", file_path.c_str());
            return false;
        }
        
        // 打开文件
        FILE* file = fopen(file_path.c_str(), "rb");
        if (!file) {
            ESP_LOGE(TAG, "Failed to open file: %s", file_path.c_str());
            return false;
        }
        
        // 获取文件大小
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        ESP_LOGI(TAG, "File size: %ld bytes", file_size);
        
        // 读取文件内容并推送到解码队列
        const size_t buffer_size = 1024;
        uint8_t buffer[buffer_size];
        size_t bytes_read;
        
        while ((bytes_read = fread(buffer, 1, buffer_size, file)) > 0) {
            // 创建音频流包
            auto packet = std::make_unique<AudioStreamPacket>();
            packet->sample_rate = 16000;  // 假设16kHz采样率
            packet->frame_duration = 60;   // 60ms帧长度
            packet->payload.resize(bytes_read);
            memcpy(packet->payload.data(), buffer, bytes_read);
            
            // 推送到音频解码队列
            auto& app = Application::GetInstance();
            bool success = app.GetAudioService().PushPacketToDecodeQueue(std::move(packet), false);
            
            if (!success) {
                // 队列满了，等待一下再继续
                ESP_LOGW(TAG, "Decode queue is full, waiting...");
                vTaskDelay(pdMS_TO_TICKS(50));  // 等待50ms
                continue;  // 重新尝试推送当前数据包
            }
      
            ESP_LOGI(TAG, "Pushed %zu bytes to decode queue", bytes_read);
            
            // 添加适当延迟，控制推送速度
            // 1024字节在16kHz采样率下大约对应32ms的音频
            vTaskDelay(pdMS_TO_TICKS(30));  // 延迟30ms
        }
        
        fclose(file);
        ESP_LOGI(TAG, "Finished playing: %s", file_name.c_str());
        return true;
    }

    bool StopMusic() {
        ESP_LOGI(TAG, "Stopping music");
        
        // 获取音频服务实例
        auto& app = Application::GetInstance();
        auto& audio_service = app.GetAudioService();
        
        // 清空解码队列
        audio_service.ResetDecoder();
        ESP_LOGI(TAG, "Cleared decode queue");
        
        // 关闭音频输出
        auto codec = Board::GetInstance().GetAudioCodec();
        if (codec) {
            codec->EnableOutput(false);
            ESP_LOGI(TAG, "Disabled audio output");
        }
        return true;
    }

    bool SwitchMusic(const std::string& direction) {
        ESP_LOGI(TAG, "Switching music: %s", direction.c_str());
        
        // 停止当前播放
        StopMusic();
        
        // 扫描SD卡中的音乐文件
        std::vector<std::string> music_files;
        DIR* dir = opendir("/sdcard");
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string filename = entry->d_name;
                // 检查是否是音乐文件（简单检查扩展名）
                if (filename.length() > 4) {
                    std::string ext = filename.substr(filename.length() - 4);
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".mp3" || ext == ".wav" || ext == ".p3") {
                        music_files.push_back(filename);
                    }
                }
            }
            closedir(dir);
        }
        
        if (music_files.empty()) {
            ESP_LOGE(TAG, "No music files found on SD card");
            return false;
        }
        
        // 排序文件列表
        std::sort(music_files.begin(), music_files.end());
        
        // 查找当前播放的文件
        static size_t current_index = 0;
        
        if (direction == "next") {
            current_index = (current_index + 1) % music_files.size();
        } else if (direction == "previous") {
            if (current_index == 0) {
                current_index = music_files.size() - 1;
            } else {
                current_index--;
            }
        }
        
        // 播放选中的文件
        std::string next_file = music_files[current_index];
        ESP_LOGI(TAG, "Playing next file: %s (index: %zu/%zu)", 
                 next_file.c_str(), current_index + 1, music_files.size());
        
        // 重新启用音频输出
        auto codec = Board::GetInstance().GetAudioCodec();
        if (codec) {
            codec->EnableOutput(true);
        }
        
        // 播放新文件
        PlayLocalMusic(next_file);
        return true;
    }

    bool PlayDesLocalMusic(const std::string& file_name) {
        ESP_LOGI(TAG, "Playing designated local music: %s", file_name.c_str());
        
        // 先停止当前播放
        StopMusic();
        
        // 构建完整的文件路径
        std::string file_path = "/sdcard/" + file_name;
        
        // 检查文件是否存在
        struct stat st;
        if (stat(file_path.c_str(), &st) != 0) {
            ESP_LOGE(TAG, "File not found: %s", file_path.c_str());
            return false;
        }
        
        // 重新启用音频输出
        auto codec = Board::GetInstance().GetAudioCodec();
        if (codec) {
            codec->EnableOutput(true);
        }
        
        // 打开文件
        FILE* file = fopen(file_path.c_str(), "rb");
        if (!file) {
            ESP_LOGE(TAG, "Failed to open file: %s", file_path.c_str());
            return false;
        }
        
        // 获取文件大小
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        ESP_LOGI(TAG, "File size: %ld bytes", file_size);
        
        // 读取文件内容并推送到解码队列
        const size_t buffer_size = 1024;
        uint8_t buffer[buffer_size];
        size_t bytes_read;
        
        while ((bytes_read = fread(buffer, 1, buffer_size, file)) > 0) {
            // 创建音频流包
            auto packet = std::make_unique<AudioStreamPacket>();
            packet->sample_rate = 16000;  // 假设16kHz采样率
            packet->frame_duration = 60;   // 60ms帧长度
            packet->payload.resize(bytes_read);
            memcpy(packet->payload.data(), buffer, bytes_read);
            
            // 推送到音频解码队列
            auto& app = Application::GetInstance();
            bool success = app.GetAudioService().PushPacketToDecodeQueue(std::move(packet), false);
            
            if (!success) {
                // 队列满了，等待一下再继续
                ESP_LOGW(TAG, "Decode queue is full, waiting...");
                vTaskDelay(pdMS_TO_TICKS(50));  // 等待50ms
                continue;  // 重新尝试推送当前数据包
            }
            
            ESP_LOGI(TAG, "Pushed %zu bytes to decode queue", bytes_read);
            
            // 添加适当延迟，控制推送速度
            // 1024字节在16kHz采样率下大约对应32ms的音频
            vTaskDelay(pdMS_TO_TICKS(30));  // 延迟30ms
        }
        
        fclose(file);
        ESP_LOGI(TAG, "Finished playing designated music: %s", file_name.c_str());
        return true;
    }
    


    // 创建自定义工具，用于读取sd卡数据，播放音乐
    void CreateCustomTool() {
        // 添加播放音乐工具
        McpServer::GetInstance().AddTool("self.audio.play_local_music",
            "播放SD卡中的音乐文件。支持播放指定文件名的音乐。\n"
            "Use this tool when the user wants to play music from the local SD card.\n"
            "Args:\n"
            "  `file_name`: The name of the music file to play (e.g. 'song1.mp3', 'music.wav')",
            PropertyList({
                Property("file_name", kPropertyTypeString)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                auto file_name = properties["file_name"].value<std::string>();
                return PlayLocalMusic(file_name);
            });

        // 添加停止播放音乐工具
        McpServer::GetInstance().AddTool("self.audio.stop_music",
            "停止当前正在播放的音乐。\n"
            "Use this tool when the user wants to stop the currently playing music.",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                return StopMusic();
            });

        // // 添加切换音乐工具
        // McpServer::GetInstance().AddTool("self.audio.switch_music",
        //     "切换到下一个或上一个音乐文件。\n"
        //     "Use this tool when the user wants to play the next or previous song.\n"
        //     "Args:\n"
        //     "  `direction`: 'next' to play next song, 'previous' to play previous song",
        //     PropertyList({
        //         Property("direction", kPropertyTypeString)
        //     }),
        //     [this](const PropertyList& properties) -> ReturnValue {
        //         auto direction = properties["direction"].value<std::string>();
        //         return SwitchMusic(direction);
        //     });

        // // 添加播放指定音乐工具
        // McpServer::GetInstance().AddTool("self.audio.play_local_des_music",
        //     "播放SD卡中的音乐文件。支持播放指定文件名的音乐。\n"
        //     "Use this tool when the user wants to play music from the local SD card.\n"
        //     "Args:\n"
        //     "  `file_name`: The name of the music file to play (e.g. 'song1.mp3', 'music.wav')",
        //     PropertyList({
        //         Property("file_name", kPropertyTypeString)
        //     }),
        //     [this](const PropertyList& properties) -> ReturnValue {
        //         auto file_name = properties["file_name"].value<std::string>();
        //         return PlayDesLocalMusic(file_name);
        //     });
    }


    // codec的 i2c总线 初始化
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
        config.pin_sccb_sda = -1;  
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
        //ESP_LOGI(TAG,"开始创建摄像头实例111");
        camera_ = new Esp32Camera(config);
        //ESP_LOGI(TAG,"开始创建摄像头实例222");
        camera_->SetHMirror(false);
        //ESP_LOGI(TAG,"开始创建摄像头实例333");
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
    // // 验证与ES8311的通信，读取寄存器的值
    // bool verify_es8311_communication() {
    //     ESP_LOGI(TAG, "验证与ES8311的通信...");
        
    //     // 创建ES8311设备句柄
    //     i2c_device_config_t es8311_dev_cfg = {
    //         .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    //         .device_address = 0x18,
    //         .scl_speed_hz = 100000,  // 100kHz
    //     };
        
    //     i2c_master_dev_handle_t es8311_dev = NULL;
    //     esp_err_t ret = i2c_master_bus_add_device(i2c_bus_, &es8311_dev_cfg, &es8311_dev);
    //     if (ret != ESP_OK) {
    //         ESP_LOGE(TAG, "创建ES8311设备句柄失败: %s", esp_err_to_name(ret));
    //         return false;
    //     }
        
    //     // 读取几个寄存器
    //     //const uint8_t regs_to_read[] = {0x00, 0x01, 0x02, 0xFD};
        
    //     for (size_t i = 0x00; i <= 0x45; i++) {
    //         uint8_t reg_addr = i;
    //         uint8_t reg_val = 0;            
    //         ret = i2c_master_transmit_receive(es8311_dev, &reg_addr, 1, &reg_val, 1, 1000);            
    //         if (ret == ESP_OK) {
    //             ESP_LOGI(TAG, "读取寄存器0x%02X成功: 0x%02X", reg_addr, reg_val);
    //         } else {
    //             ESP_LOGE(TAG, "读取寄存器0x%02X失败: %s", reg_addr, esp_err_to_name(ret));
    //         }
    //         if((i%8)==0){
    //             printf("\n");
    //         }
    //     }
    //     printf("\n");
    //     for (size_t i = 0xFA; i <= 0xff; i++) {
    //         uint8_t reg_addr = i;
    //         uint8_t reg_val = 0;            
    //         ret = i2c_master_transmit_receive(es8311_dev, &reg_addr, 1, &reg_val, 1, 1000);            
    //         if (ret == ESP_OK) {
    //             ESP_LOGI(TAG, "读取寄存器0x%02X成功: 0x%02X", reg_addr, reg_val);
    //         } else {
    //             ESP_LOGE(TAG, "读取寄存器0x%02X失败: %s", reg_addr, esp_err_to_name(ret));
    //         }
    //     }
   
    //     // 清理设备句柄
    //     i2c_master_bus_rm_device(es8311_dev);
        
    //     // 如果至少有一个寄存器能读取成功，说明通信正常
    //     if (ret == ESP_OK) {
    //         ESP_LOGI(TAG, "ES8311通信验证成功\n");
    //         return true;
    //     } else {
    //         ESP_LOGW(TAG, "ES8311通信验证失败\n");
    //         return false;
    //     }
    // }
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

        // ********************* audio_i2c、camera、sd_scard ****************************
        InitializeI2c();

        InitializeCamera();

        InitializeSDCard();

        //CreateCustomTool();

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
        static BoxAudioCodec audio_codec(
            i2c_bus_,                   // I2C 句柄
            //I2C_NUM_0,                  // I2C 端口号
            AUDIO_INPUT_SAMPLE_RATE,    // 输入采样率
            AUDIO_OUTPUT_SAMPLE_RATE,   // 输出采样率

            AUDIO_CODEC_MCLK_PIN,       // MCLK
            AUDIO_CODEC_I2S_SCLK_PIN,   // BCLK (SCLK)
            AUDIO_CODEC_I2S_LRCK_PIN,   // WS (LRCK)
            AUDIO_CODEC_I2S_DO_PIN,     // DSDIN
            AUDIO_CODEC_I2S_DI_PIN,     // ASDOUT

            AUDIO_CODEC_NS4150_PIN,         // PA_PIN（如有功放控制脚，否则用 GPIO_NUM_NC)        
            AUDIO_CODEC_ES8311_ADDR,        // ES8311 I2C 地址
            AUDIO_CODEC_ES7210_I2C_ADDR,     // ES7210 I2C 地址
            true
        );

        // if (!test_flag) {
        //     verify_es8311_communication();
        //     test_flag ++;
        // }
        
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

    virtual Camera* GetCamera() override {
        return camera_;
    }

};



uint8_t MyWifiBoardLCD::test_flag = 1;

DECLARE_BOARD(MyWifiBoardLCD);
