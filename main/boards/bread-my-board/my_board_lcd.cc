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
#include <driver/i2c_master.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/spi_common.h>
#include <driver/ledc.h>

#include "assets/lang_config.h"



// *******************************************************
//#include <dirent.h>

#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include <inttypes.h> // for PRIu64
#include <esp_log.h>

extern "C" {
    #include "driver/sdmmc_host.h"
}
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
//#include "driver/sdmmc_host.h"

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
#define SD_CARD_MOUNT_POINT         "/sdcard"   // 挂载点

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
    // 音乐列表
    std::vector<std::string> music_list_;
    int current_music_index_ = -1;

    void ClearMusicList(){
        music_list_.clear();
        current_music_index_ = -1;
    }

    void LoadMusicFromDirectory(const char* directory) {
        DIR* dir = opendir(directory);
        if (!dir){
            ESP_LOGI(TAG,"Failed to open directory: %s", directory)
        }

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr){
            music_list_.push_back(entry->d_name);
        }

        closedir(dir);
        ESP_LOGI(TAG,"Loaded %zu music files from %s", music_list_.size(), directory);

        // 排序音乐列表
        for (const auto& file : music_list_){
            ESP_LOGI(TAG,"  %s", file.c_str());
        }
    }

    // 初始化音乐列表
    void InitializeMusicList() {
        ESP_LOGI(TAG, "Initializing music list...");
        
        // 清空现有列表
        ClearMusicList();
        
        // 从SD卡加载音乐文件
        LoadMusicFromDirectory("/sdcard/music");

        if (music_list_.empty()) {
            ESP_LOGW(TAG, "No music files found in any storage");
        } else {
            ESP_LOGI(TAG, "Music list initialized with %zu files", music_list_.size());

        }
    }


    
    void InitializeSDCard(){
        esp_err_t ret;
        sdmmc_card_t* card;
        const char mount_point[] = SD_CARD_MOUNT_POINT;

        // 1、挂载配置
        // Options for mounting the filesystem.
        // If format_if_mount_failed is set to true, SD card will be partitioned and
        // formatted in case when mounting fails.
        esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
            .format_if_mount_failed = true,
#else
            .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
            .max_files = 5,
            .allocation_unit_size = 16 * 1024
        };
        ESP_LOGI(TAG, "Initializing SD card");

        // Use settings defined above to initialize SD card and mount FAT filesystem.
        // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
        // Please check its source code and implement error recovery when developing
        // production applications.
        ESP_LOGI(TAG, "Using SDMMC peripheral");
    
        // 2、主机配置
        // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
        // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 40MHz for SDMMC)
        // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
#if CONFIG_EXAMPLE_SDMMC_SPEED_HS
        host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
#elif CONFIG_EXAMPLE_SDMMC_SPEED_UHS_I_SDR50
        host.slot = SDMMC_HOST_SLOT_0;
        host.max_freq_khz = SDMMC_FREQ_SDR50;
        host.flags &= ~SDMMC_HOST_FLAG_DDR;
#elif CONFIG_EXAMPLE_SDMMC_SPEED_UHS_I_DDR50
        host.slot = SDMMC_HOST_SLOT_0;
        host.max_freq_khz = SDMMC_FREQ_DDR50;
#elif CONFIG_EXAMPLE_SDMMC_SPEED_UHS_I_SDR104
        host.slot = SDMMC_HOST_SLOT_0;
        host.max_freq_khz = SDMMC_FREQ_SDR104;
        host.flags &= ~SDMMC_HOST_FLAG_DDR;
#endif
    
        // 3、电源配置（可选）
        // For SoCs where the SD power can be supplied both via an internal or external (e.g. on-board LDO) power supply.
        // When using specific IO pins (which can be used for ultra high-speed SDMMC) to connect to the SD card
        // and the internal LDO power supply, we need to initialize the power supply first.
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
    
        // 4、插槽配置
        // This initializes the slot without card detect (CD) and write protect (WP) signals.
        // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
#if EXAMPLE_IS_UHS1
        slot_config.flags |= SDMMC_SLOT_FLAG_UHS1;
#endif
        // Set bus width to use:
#ifdef CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
        slot_config.width = 4;
#else
        slot_config.width = 1;
#endif
        // On chips where the GPIOs used for SD card can be configured, set them in
        // the slot_config structure:
#ifdef CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
        slot_config.clk = SD_CARD_PIN_CLK;
        slot_config.cmd = SD_CARD_PIN_CMD;
        slot_config.d0 = SD_CARD_PIN_D0;
#ifdef CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
        slot_config.d1 = CONFIG_EXAMPLE_PIN_D1;
        slot_config.d2 = CONFIG_EXAMPLE_PIN_D2;
        slot_config.d3 = CONFIG_EXAMPLE_PIN_D3;
#endif  // CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
#endif  // CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
    
        // 5、启用内部上拉
        // Enable internal pullups on enabled pins. The internal pullups
        // are insufficient however, please make sure 10k external pullups are
        // connected on the bus. This is for debug / example purpose only.
        slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    
        // 6、挂载文件系统
        ESP_LOGI(TAG, "Mounting filesystem");   
        ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

        // 7、检查挂载结果
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
        ESP_LOGI(TAG, "Filesystem mounted");
    
        // 8、打印卡属性
        // Card has been initialized, print its properties
        sdmmc_card_print_info(stdout, card);
    }



    bool PlayLocalMusic(){
        ESP_LOGI(TAG, "Playing local music");
        
        // 检查音乐列表是否为空
        if (music_list_.empty()) {
            ESP_LOGE(TAG, "No music files available");
            return false;
        }
        
        // 首位连接循环播放
        if (current_music_index_ < 0) {
            current_music_index_ = music_list_.size() - 1;
        }else if(current_music_index_ >= music_list_.size()){
            current_music_index_ = 0;
        }
        
        // 获取当前要播放的音乐文件
        std::string file_path = music_list_[current_music_index_];
        ESP_LOGI(TAG, "Playing music: %s (%d/%zu)", file_path.c_str(), current_music_index_ + 1, music_list_.size());
        
        
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
        
        ESP_LOGI(TAG, "P3 file size: %ld bytes", file_size);
        
        // 读取整个P3文件到内存
        std::vector<char> p3_data(file_size);
        size_t bytes_read = fread(p3_data.data(), 1, file_size, file);
        fclose(file);
        
        if (bytes_read != file_size) {
            ESP_LOGE(TAG, "Failed to read complete file: %zu/%ld bytes", bytes_read, file_size);
            return false;
        }
        
        ESP_LOGI(TAG, "Successfully loaded P3 file: %zu bytes", bytes_read);
        
        // 获取音频服务实例
        auto& app = Application::GetInstance();
        auto& audio_service = app.GetAudioService();
        
        // 确保音频服务处于空闲状态
        if (!audio_service.IsIdle()) {
            ESP_LOGW(TAG, "Audio service is not idle, resetting decoder");
            audio_service.ResetDecoder();
            
            // 等待音频服务变为空闲状态
            int wait_count = 0;
            const int max_wait_count = 30; // 最多等待3秒
            while (!audio_service.IsIdle() && wait_count < max_wait_count) {
                vTaskDelay(pdMS_TO_TICKS(100));
                wait_count++;
            }
            
            if (wait_count >= max_wait_count) {
                ESP_LOGW(TAG, "Audio service did not become idle within timeout, proceeding anyway");
            } else {
                ESP_LOGI(TAG, "Audio service became idle after %d ms", wait_count * 100);
            }
        }
        
        // // 确保音频输出是启用的
        // auto codec = Board::GetInstance().GetAudioCodec();
        // if (codec) {
        //     codec->EnableOutput(true);
        //     ESP_LOGI(TAG, "Enabled audio output");
        // }
        
        // 通知云端设备正在播放本地音乐，避免云端大模型一直说话
        app.SendMcpMessage("{\"jsonrpc\":\"2.0\",\"method\":\"notification\",\"params\":{\"type\":\"local_music_start\",\"message\":\"Playing local music\"}}");
        app.SendMcpMessage("{\"jsonrpc\":\"2.0\",\"method\":\"notification\",\"params\":{\"type\":\"system\",\"status\":\"local_music_active\"}}");
        ESP_LOGI(TAG, "Notified cloud about local music playback");
        
        // // 设置设备状态为speaking，这样OnIncomingAudio回调才会处理音频
        // app.SetDeviceState(kDeviceStateSpeaking);
        
        // 使用PlaySound函数播放P3数据
        std::string_view sound_data(p3_data.data(), p3_data.size());
        audio_service.PlaySound(sound_data);
        
        ESP_LOGI(TAG, "Started playing P3 file using PlaySound");
        
        // 创建监控任务，检测播放完成或被中断
        xTaskCreate([](void* arg) {
            auto& app = Application::GetInstance();
            auto& audio_service = app.GetAudioService();
            
            // 等待播放完成（音频服务变为空闲状态）或超时
            int wait_count = 0;
            const int max_wait_count = 600; // 最多等待60秒（假设音乐最长60秒）
            bool playback_finished = false;
            
            while (!audio_service.IsIdle() && wait_count < max_wait_count) {
                vTaskDelay(pdMS_TO_TICKS(100));  // 每100ms检查一次
                wait_count++;
                
                // 检查设备状态，如果被中断则退出
                if (app.GetDeviceState() != kDeviceStateSpeaking) {
                    ESP_LOGI(TAG, "Music playback was interrupted, stopping monitor");
                    playback_finished = true;
                    break;
                }
            }
            
            if (wait_count >= max_wait_count) {
                ESP_LOGW(TAG, "Music playback timeout after %d seconds", max_wait_count / 10);
            } else if (!playback_finished) {
                ESP_LOGI(TAG, "Music playback finished normally after %d ms", wait_count * 100);
            }
            
            // 只有在正常播放完成时才发送结束通知
            if (!playback_finished) {
                // 发送应用层协议：通知MCP工具本地音乐结束
                app.SendMcpMessage("{\"jsonrpc\":\"2.0\",\"method\":\"notification\",\"params\":{\"type\":\"local_music_end\",\"message\":\"Local music playback finished\"}}");
                app.SendMcpMessage("{\"jsonrpc\":\"2.0\",\"method\":\"notification\",\"params\":{\"type\":\"system\",\"status\":\"ready_for_commands\"}}");
                ESP_LOGI(TAG, "Auto-sent application layer protocol: MCP notifications");
                
                // 恢复设备状态
                app.SetDeviceState(kDeviceStateIdle);
                ESP_LOGI(TAG, "Auto-restored device state to idle");
            } else {
                // 如果是被中断的，需要发送两个协议
                // 发送应用层协议：通知MCP工具本地音乐结束
                app.SendMcpMessage("{\"jsonrpc\":\"2.0\",\"method\":\"notification\",\"params\":{\"type\":\"local_music_end\",\"message\":\"Local music playback interrupted\"}}");
                app.SendMcpMessage("{\"jsonrpc\":\"2.0\",\"method\":\"notification\",\"params\":{\"type\":\"system\",\"status\":\"ready_for_commands\"}}");
                ESP_LOGI(TAG, "Auto-sent application layer protocol: MCP notifications for interruption");
                
                // 发送协议层协议：通知云端会话中断
                app.AbortSpeaking(kAbortReasonNone);
                ESP_LOGI(TAG, "Auto-sent protocol layer protocol: abort speaking");
            }
            
            vTaskDelete(NULL);
        }, "music_monitor", 2048, nullptr, 5, nullptr);
        
        return true;
    }

    bool StopMusic() {
        ESP_LOGI(TAG, "Stopping music");
        
        // 获取音频服务实例
        auto& app = Application::GetInstance();
        auto& audio_service = app.GetAudioService();
        
        // 检查音频服务是否正在播放
        if (audio_service.IsIdle()) {
            ESP_LOGW(TAG, "Audio service is already idle, no need to stop");
            return true;
        }
        
        // 发送应用层协议：通知MCP工具本地音乐结束
        app.SendMcpMessage("{\"jsonrpc\":\"2.0\",\"method\":\"notification\",\"params\":{\"type\":\"local_music_end\",\"message\":\"Local music playback finished\"}}");
        app.SendMcpMessage("{\"jsonrpc\":\"2.0\",\"method\":\"notification\",\"params\":{\"type\":\"system\",\"status\":\"ready_for_commands\"}}");
        ESP_LOGI(TAG, "Sent application layer protocol: MCP notifications");
        
        // 发送协议层协议：通知云端会话中断
        app.AbortSpeaking(kAbortReasonNone);
        ESP_LOGI(TAG, "Sent protocol layer protocol: abort speaking");
        
        ESP_LOGI(TAG, "Notified cloud about local music end and session abort");
        
        // 等待一小段时间确保通知发送完成
        vTaskDelay(pdMS_TO_TICKS(50));
        
        // 清空解码队列和重置解码器状态
        audio_service.ResetDecoder();
        ESP_LOGI(TAG, "Reset decoder and cleared all audio queues");
        
        // 等待音频服务变为空闲状态
        int wait_count = 0;
        const int max_wait_count = 50; // 最多等待5秒
        while (!audio_service.IsIdle() && wait_count < max_wait_count) {
            vTaskDelay(pdMS_TO_TICKS(100));
            wait_count++;
        }
        
        if (wait_count >= max_wait_count) {
            ESP_LOGW(TAG, "Audio service did not become idle within timeout");
        } else {
            ESP_LOGI(TAG, "Audio service became idle after %d ms", wait_count * 100);
        }
        
        // 关闭音频输出
        auto codec = Board::GetInstance().GetAudioCodec();
        if (codec) {
            codec->EnableOutput(false);
            ESP_LOGI(TAG, "Disabled audio output");
        }
        
        // 恢复设备状态为正常
        app.SetDeviceState(kDeviceStateIdle);
        ESP_LOGI(TAG, "Restored device state to idle");
        
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
        PlayLocalMusic();
        return true;
    }

    bool PlayDesLocalMusic(const std::string& file_name) {
       
        return true;
    }


    bool test_print(){
        ESP_LOGI(TAG,"************** test_print ******************");
        return true;
    }

    // 创建自定义工具，用于读取sd卡数据，播放音乐
    void CreateCustomTool() {
        auto& mcp_server = McpServer::GetInstance();
        
        //"打印一行预设好的用于测试的，提示信息\n"
        mcp_server.AddTool("self.print_info",
            "打印",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                return test_print();
            });

        // 添加播放音乐工具
        mcp_server.AddTool("self.audio.play_local_music",
            "播放本地音乐",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                return PlayLocalMusic();
            });
        // 添加停止播放音乐工具
        mcp_server.AddTool("self.audio.stop_music",
            "结束本地音乐",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                return StopMusic();
            });
        // 添加播放下一首音乐工具
        mcp_server.AddTool("self.audio.play_next",
            "下一首，换一首",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                return SwitchMusic("next");
            });
        // 添加播放上一首音乐工具
        mcp_server.AddTool("self.audio.play_previous",
            "上一首",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                return SwitchMusic("previous");
            });

        // 添加获取音乐列表信息工具
        mcp_server.AddTool("self.audio.get_music_info",
            "获取音乐列表信息",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                std::string info = "Music List Info:\n";
                info += "Total files: " + std::to_string(GetMusicCount()) + "\n";
                info += "Current index: " + std::to_string(GetCurrentIndex()) + "\n";
                info += "Current file: " + GetCurrentMusic() + "\n";
                ESP_LOGI(TAG, "%s", info.c_str());
                return true;
            });
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

    // //======================================================================
    // // 验证与ES8311的通信，读取寄存器的值
    // bool verify_es8311_communication() {
    //     ESP_LOGI(TAG, "验证与ES8311的通信...");
        
    //     // 创建ES8311设备句柄
    //     i2c_device_config_t es8311_dev_cfg = {
    //         .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    //         .device_address = 0x41,
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
        
    //     for (size_t i = 0x00; i <= 0x4C; i++) {
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
    //     // printf("\n");
    //     // for (size_t i = 0x7A; i <= 0x7F; i++) {
    //     //     uint8_t reg_addr = i;
    //     //     uint8_t reg_val = 0;            
    //     //     ret = i2c_master_transmit_receive(es8311_dev, &reg_addr, 1, &reg_val, 1, 1000);            
    //     //     if (ret == ESP_OK) {
    //     //         ESP_LOGI(TAG, "读取寄存器0x%02X成功: 0x%02X", reg_addr, reg_val);
    //     //     } else {
    //     //         ESP_LOGE(TAG, "读取寄存器0x%02X失败: %s", reg_addr, esp_err_to_name(ret));
    //     //     }
    //     // }
   
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
    // //=======================================================================================


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

        CreateCustomTool();

        // 初始化音乐列表
        InitializeMusicList();

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

    virtual Camera* GetCamera() override {
        return camera_;
    }

};



uint8_t MyWifiBoardLCD::test_flag = 0;

DECLARE_BOARD(MyWifiBoardLCD);
