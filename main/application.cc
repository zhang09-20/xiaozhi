#include "application.h"
#include "board.h"
#include "display.h"
#include "system_info.h"
#include "ml307_ssl_transport.h"
#include "audio_codec.h"
#include "mqtt_protocol.h"
#include "websocket_protocol.h"
#include "font_awesome_symbols.h"
#include "iot/thing_manager.h"
#include "assets/lang_config.h"

#if CONFIG_USE_AUDIO_PROCESSOR
#include "afe_audio_processor.h"
#else
#include "dummy_audio_processor.h"
#endif

#include <cstring>
#include <esp_log.h>
#include <cJSON.h>
#include <driver/gpio.h>
#include <arpa/inet.h>

#define TAG "Application"


static const char* const STATE_STRINGS[] = {
    "unknown",
    "starting",
    "configuring",
    "idle",
    "connecting",
    "listening",
    "speaking",
    "upgrading",
    "activating",
    "fatal_error",
    "invalid_state"
};
//============ 1、构造函数 =============
Application::Application() {
    event_group_ = xEventGroupCreate();
    background_task_ = new BackgroundTask(4096 * 8);

#if CONFIG_USE_AUDIO_PROCESSOR
    audio_processor_ = std::make_unique<AfeAudioProcessor>();
#else
    audio_processor_ = std::make_unique<DummyAudioProcessor>();
#endif

    esp_timer_create_args_t clock_timer_args = {
        .callback = [](void* arg) {
            Application* app = (Application*)arg;
            app->OnClockTimer();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "clock_timer",
        .skip_unhandled_events = true
    };
    esp_timer_create(&clock_timer_args, &clock_timer_handle_);
    esp_timer_start_periodic(clock_timer_handle_, 1000000);
}

//=========== 2、析构函数 =============
Application::~Application() {
    if (clock_timer_handle_ != nullptr) {
        esp_timer_stop(clock_timer_handle_);
        esp_timer_delete(clock_timer_handle_);
    }
    if (background_task_ != nullptr) {
        delete background_task_;
    }
    vEventGroupDelete(event_group_);
}

//=========== 3、检查新版本 =============
void Application::CheckNewVersion() {
    // // 禁用OTA功能，直接跳过版本检查 ******************************************
    // ESP_LOGI(TAG, "OTA disabled, skipping version check");
    // xEventGroupSetBits(event_group_, CHECK_NEW_VERSION_DONE_EVENT);
    // return;
    // //

    const int MAX_RETRY = 10;
    int retry_count = 0;
    int retry_delay = 10; // 初始重试延迟为10秒

    //while (true) {
        SetDeviceState(kDeviceStateActivating); //设置当前设备状态为激活
        auto display = Board::GetInstance().GetDisplay();
        display->SetStatus(Lang::Strings::CHECKING_NEW_VERSION);    //显示正在检查新版本
        
        // //检查新版本，如果失败则进行重测
        // if (!ota_.CheckVersion()) {
        //     retry_count++;
        //     if (retry_count >= MAX_RETRY) {
        //         ESP_LOGE(TAG, "Too many retries, exit version check");
        //         return;
        //     }

        //     //显示检查失败提示，包含重试延迟时间和检查url
        //     char buffer[128];
        //     snprintf(buffer, sizeof(buffer), Lang::Strings::CHECK_NEW_VERSION_FAILED, retry_delay, ota_.GetCheckVersionUrl().c_str());
        //     Alert(Lang::Strings::ERROR, buffer, "sad", Lang::Sounds::P3_EXCLAMATION);

        //     ESP_LOGW(TAG, "Check new version failed, retry in %d seconds (%d/%d)", retry_delay, retry_count, MAX_RETRY);
            
        //     //等待重试延迟时间
        //     for (int i = 0; i < retry_delay; i++) {
        //         vTaskDelay(pdMS_TO_TICKS(1000));
        //         if (device_state_ == kDeviceStateIdle) {
        //             break;
        //         }
        //     }
        //     retry_delay *= 2; // 每次重试后延迟时间翻倍
        //     continue;
        // }
        // retry_count = 0;
        // retry_delay = 10; // 重置重试延迟时间

        //如果发现新版本，执行升级程序
//         if (ota_.HasNewVersion()) {
//             Alert(Lang::Strings::OTA_UPGRADE, Lang::Strings::UPGRADING, "happy", Lang::Sounds::P3_UPGRADE);

//             vTaskDelay(pdMS_TO_TICKS(3000));

//             SetDeviceState(kDeviceStateUpgrading);  //设置设备状态为升级中
            
//             //显示升级信息
//             display->SetIcon(FONT_AWESOME_DOWNLOAD);
//             std::string message = std::string(Lang::Strings::NEW_VERSION) + ota_.GetFirmwareVersion();
//             display->SetChatMessage("system", message.c_str());

//             //准备升级环境
//             auto& board = Board::GetInstance();
//             board.SetPowerSaveMode(false);  //关闭省电模式
// #if CONFIG_USE_WAKE_WORD_DETECT
//             wake_word_detect_.StopDetection();
// #endif
//             // 预先关闭音频输出，避免升级过程有音频操作
//             //关闭音频相关功能
//             auto codec = board.GetAudioCodec();
//             codec->EnableInput(false);
//             codec->EnableOutput(false);
//             {
//                 std::lock_guard<std::mutex> lock(mutex_);
//                 audio_decode_queue_.clear();    //清空音频解码队列
//             }
//             background_task_->WaitForCompletion();  //等待后台任务完成
//             delete background_task_;
//             background_task_ = nullptr;
//             vTaskDelay(pdMS_TO_TICKS(1000));

//             //开始升级，显示进度
//             ota_.StartUpgrade([display](int progress, size_t speed) {
//                 char buffer[64];
//                 snprintf(buffer, sizeof(buffer), "%d%% %zuKB/s", progress, speed / 1024);
//                 display->SetChatMessage("system", buffer);
//             });

//             // If upgrade success, the device will reboot and never reach here
//             //如果升级失败，显示错误并重启
//             display->SetStatus(Lang::Strings::UPGRADE_FAILED);
//             ESP_LOGI(TAG, "Firmware upgrade failed...");
//             vTaskDelay(pdMS_TO_TICKS(3000));
//             Reboot();
//             return;
//         }

        // No new version, mark the current version as valid
        //没有新版本，标记当前版本为有效
        ota_.MarkCurrentVersionValid();
        // if (!ota_.HasActivationCode() && !ota_.HasActivationChallenge()) {
        //     xEventGroupSetBits(event_group_, CHECK_NEW_VERSION_DONE_EVENT);
        //     // Exit the loop if done checking new version
        //     break;
        // }

        //处理设备激活流程
        display->SetStatus(Lang::Strings::ACTIVATION);
        
        // Activation code is shown to the user and waiting for the user to input
        //显示激活码给用户
        if (ota_.HasActivationCode()) {
            ShowActivationCode();
        }

        // This will block the loop until the activation is done or timeout
        //尝试激活设备，最多尝试10次
        for (int i = 0; i < 10; ++i) {
            ESP_LOGI(TAG, "Activating... %d/%d", i + 1, 10);
            esp_err_t err = ota_.Activate();
            if (err == ESP_OK) {
                xEventGroupSetBits(event_group_, CHECK_NEW_VERSION_DONE_EVENT);
                break;
            } else if (err == ESP_ERR_TIMEOUT) {
                vTaskDelay(pdMS_TO_TICKS(3000));
            } else {
                vTaskDelay(pdMS_TO_TICKS(10000));
            }
            if (device_state_ == kDeviceStateIdle) {
                break;
            }
        }
    //}
}

//=========== 4、显示激活码 =============
void Application::ShowActivationCode() {
    auto& message = ota_.GetActivationMessage();
    auto& code = ota_.GetActivationCode();

    struct digit_sound {
        char digit;
        const std::string_view& sound;
    };
    static const std::array<digit_sound, 10> digit_sounds{{
        digit_sound{'0', Lang::Sounds::P3_0},
        digit_sound{'1', Lang::Sounds::P3_1}, 
        digit_sound{'2', Lang::Sounds::P3_2},
        digit_sound{'3', Lang::Sounds::P3_3},
        digit_sound{'4', Lang::Sounds::P3_4},
        digit_sound{'5', Lang::Sounds::P3_5},
        digit_sound{'6', Lang::Sounds::P3_6},
        digit_sound{'7', Lang::Sounds::P3_7},
        digit_sound{'8', Lang::Sounds::P3_8},
        digit_sound{'9', Lang::Sounds::P3_9}
    }};

    // This sentence uses 9KB of SRAM, so we need to wait for it to finish
    Alert(Lang::Strings::ACTIVATION, message.c_str(), "happy", Lang::Sounds::P3_ACTIVATION);

    for (const auto& digit : code) {
        auto it = std::find_if(digit_sounds.begin(), digit_sounds.end(),
            [digit](const digit_sound& ds) { return ds.digit == digit; });
        if (it != digit_sounds.end()) {
            PlaySound(it->sound);
        }
    }
}

//=========== 5、提示 =============
void Application::Alert(const char* status, const char* message, const char* emotion, const std::string_view& sound) {
    ESP_LOGW(TAG, "Alert %s: %s [%s]", status, message, emotion);
    auto display = Board::GetInstance().GetDisplay();
    display->SetStatus(status);
    display->SetEmotion(emotion);
    display->SetChatMessage("system", message);
    if (!sound.empty()) {
        ResetDecoder();
        PlaySound(sound);
    }
}

//=========== 6、关闭提示 =============
void Application::DismissAlert() {
    if (device_state_ == kDeviceStateIdle) {
        auto display = Board::GetInstance().GetDisplay();
        display->SetStatus(Lang::Strings::STANDBY);
        display->SetEmotion("neutral");
        display->SetChatMessage("system", "");
    }
}

//=========== 7、播放声音 =============
void Application::PlaySound(const std::string_view& sound) {
    // Wait for the previous sound to finish
    {
        std::unique_lock<std::mutex> lock(mutex_);
        audio_decode_cv_.wait(lock, [this]() {
            return audio_decode_queue_.empty();
        });
    }
    background_task_->WaitForCompletion();

    // The assets are encoded at 16000Hz, 60ms frame duration
    SetDecodeSampleRate(16000, 60);
    const char* data = sound.data();
    size_t size = sound.size();
    for (const char* p = data; p < data + size; ) {
        auto p3 = (BinaryProtocol3*)p;
        p += sizeof(BinaryProtocol3);

        auto payload_size = ntohs(p3->payload_size);
        AudioStreamPacket packet;
        packet.payload.resize(payload_size);
        memcpy(packet.payload.data(), p3->payload, payload_size);
        p += payload_size;

        std::lock_guard<std::mutex> lock(mutex_);
        audio_decode_queue_.emplace_back(std::move(packet));
    }
}

//=========== 8、切换聊天状态 ===========
void Application::ToggleChatState() {
    if (device_state_ == kDeviceStateActivating) {  //如果设备处于激活状态，则切换到空闲状态
        SetDeviceState(kDeviceStateIdle);           //切换到空闲状态
        return;
    }

    if (!protocol_) {   //如果协议没有初始化，则返回
        ESP_LOGE(TAG, "Protocol not initialized");  //打印错误日志
        return;
    }

    if (device_state_ == kDeviceStateIdle) {                //如果设备处于空闲状态============= 1
        //添加异步任务，切换到连接状态
        Schedule([this]() {
            SetDeviceState(kDeviceStateConnecting); //切换到连接状态
            if (!protocol_->OpenAudioChannel()) {   //如果打开音频通道失败，则返回
                return;
            }

            //设置监听模式，启用实时聊天模式，则设置为实时模式，否则设置为自动停止模式
            SetListeningMode(realtime_chat_enabled_ ? kListeningModeRealtime : kListeningModeAutoStop);
        });
    } else if (device_state_ == kDeviceStateSpeaking) {     //如果设备处于说话状态============= 2
        Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);    //停止说话
        });
    } else if (device_state_ == kDeviceStateListening) {    //如果设备处于监听状态============= 3
        Schedule([this]() {
            protocol_->CloseAudioChannel();     //关闭音频通道
        });
    }
}

//=========== 9、开始监听 ===========
void Application::StartListening() {
    if (device_state_ == kDeviceStateActivating) {
        SetDeviceState(kDeviceStateIdle);
        return;
    }

    if (!protocol_) {
        ESP_LOGE(TAG, "Protocol not initialized");
        return;
    }
    
    if (device_state_ == kDeviceStateIdle) {
        Schedule([this]() {
            if (!protocol_->IsAudioChannelOpened()) {
                SetDeviceState(kDeviceStateConnecting);
                if (!protocol_->OpenAudioChannel()) {
                    return;
                }
            }

            SetListeningMode(kListeningModeManualStop);
        });
    } else if (device_state_ == kDeviceStateSpeaking) {
        Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);
            SetListeningMode(kListeningModeManualStop);
        });
    }
}

//=========== 10、停止监听 ===========
void Application::StopListening() {
    const std::array<int, 3> valid_states = {
        kDeviceStateListening,
        kDeviceStateSpeaking,
        kDeviceStateIdle,
    };
    // If not valid, do nothing
    if (std::find(valid_states.begin(), valid_states.end(), device_state_) == valid_states.end()) {
        return;
    }

    Schedule([this]() {
        if (device_state_ == kDeviceStateListening) {
            protocol_->SendStopListening();
            SetDeviceState(kDeviceStateIdle);
        }
    });
}

//=========== 11、启动应用程序 ===================
/**
 * @prief 启动应用程序
 * 
 * 1、创建开发板实例
 * 2、设置设备状态为启动
 * 
 * 3、设置显示实例
 * 
 * 4、设置opus解码器实例
 * 5、设置opus编码器实例
 * 6、设置opus编码器复杂度
 * 7、设置opus编码器输入采样率
 * 8、设置音频编码器输出采样率
 * 9、创建音频处理任务
 * 
 * 10、启动网络
 */
void Application::Start() {
    //创建开发板实例
    auto& board = Board::GetInstance();
    SetDeviceState(kDeviceStateStarting);   //设备状态设为启动

    /* Setup the display */
    auto display = board.GetDisplay();  //创建开发板显示实例

    /* Setup the audio codec */
    auto codec = board.GetAudioCodec();     //创建opus编解码器实例
    //初始化opus解码器
    opus_decoder_ = std::make_unique<OpusDecoderWrapper>(codec->output_sample_rate(), 1, OPUS_FRAME_DURATION_MS);
    //初始化opus编码器
    opus_encoder_ = std::make_unique<OpusEncoderWrapper>(16000, 1, OPUS_FRAME_DURATION_MS);
    if (realtime_chat_enabled_) {   //实时聊天模式，编码复杂度设为 0
        ESP_LOGI(TAG, "Realtime chat enabled, setting opus encoder complexity to 0");
        opus_encoder_->SetComplexity(0);
    } else if (board.GetBoardType() == "ml307") {   //ml307开发板
        ESP_LOGI(TAG, "ML307 board detected, setting opus encoder complexity to 5");
        opus_encoder_->SetComplexity(5);
    } else {    //其他、默认选择
        ESP_LOGI(TAG, "WiFi board detected, setting opus encoder complexity to 3");
        opus_encoder_->SetComplexity(3);
    }

    //音频编码器 输入采样率
    if (codec->input_sample_rate() != 16000) {  //若不是 16 Khz
        input_resampler_.Configure(codec->input_sample_rate(), 16000);  //音频编码器 输入采样率
        reference_resampler_.Configure(codec->input_sample_rate(), 16000);  //参考重采样器 输入采样率
    }
    //音频 编、解码器 初始化
    codec->Start();

#if CONFIG_USE_AUDIO_PROCESSOR  //如果音频处理器 启用
    //创建音频处理 RTOS 任务，并启用音频处理循环
    xTaskCreatePinnedToCore([](void* arg) { //区别是绑定到某核心
        Application* app = (Application*)arg;
        app->AudioLoop();
        vTaskDelete(NULL);
    }, "audio_loop", 4096 * 2, this, 8, &audio_loop_task_handle_, 1);
#else
    xTaskCreate([](void* arg) { //不指定某核心
        Application* app = (Application*)arg;
        app->AudioLoop();
        vTaskDelete(NULL);
    }, "audio_loop", 4096 * 2, this, 8, &audio_loop_task_handle_);
#endif

    /* Wait for the network to be ready */
    //等待网络就绪
    board.StartNetwork();

    // Check for new firmware version or get the MQTT broker address
    //检查新版本固件或获取 mqtt 代理地址
    CheckNewVersion();

    // Initialize the protocol
    //初始化协议
    display->SetStatus(Lang::Strings::LOADING_PROTOCOL);    //更新显示设备上状态文本

    //根据ota配置选择协议类型·
    if (ota_.HasMqttConfig()) {
        protocol_ = std::make_unique<MqttProtocol>();   //使用 mqtt 协议
    } else if (ota_.HasWebsocketConfig()) {
        protocol_ = std::make_unique<WebsocketProtocol>();  //使用 websocket 协议
    } else {
        ESP_LOGW(TAG, "No protocol specified in the OTA config, using default MQTT");
        // 使用默认MQTT配置
        //Settings settings("mqtt", true);
        // if (!settings.HasKey("broker")) {
        //     // 设置默认MQTT配置
        //     settings.SetString("broker", "mqtt.tenclass.net");
        //     settings.SetInt("port", 1883);
        //     settings.SetString("username", "xiaozhi");
        //     settings.SetString("password", "xiaozhi123");
        //     settings.SetString("client_id", "");
        //     ESP_LOGI(TAG, "Set default MQTT configuration");
        // }
        protocol_ = std::make_unique<MqttProtocol>();   //默认使用 mqtt 协议
    }

    //设置网络错误回调
    protocol_->OnNetworkError([this](const std::string& message) {
        SetDeviceState(kDeviceStateIdle);
        Alert(Lang::Strings::ERROR, message.c_str(), "sad", Lang::Sounds::P3_EXCLAMATION);
    });
    //设置接收音频数据回调
    protocol_->OnIncomingAudio([this](AudioStreamPacket&& packet) {
        const int max_packets_in_queue = 600 / OPUS_FRAME_DURATION_MS;  //最大队列长度
        std::lock_guard<std::mutex> lock(mutex_);
        if (audio_decode_queue_.size() < max_packets_in_queue) {
            audio_decode_queue_.emplace_back(std::move(packet));
        }
    });
    //设置音频通道打开回调
    protocol_->OnAudioChannelOpened([this, codec, &board]() {
        board.SetPowerSaveMode(false);  //关闭省电模式
        if (protocol_->server_sample_rate() != codec->output_sample_rate()) {
            ESP_LOGW(TAG, "Server sample rate %d does not match device output sample rate %d, resampling may cause distortion",
                protocol_->server_sample_rate(), codec->output_sample_rate());
        }
        SetDecodeSampleRate(protocol_->server_sample_rate(), protocol_->server_frame_duration());
        auto& thing_manager = iot::ThingManager::GetInstance();
        protocol_->SendIotDescriptors(thing_manager.GetDescriptorsJson());
        std::string states;
        if (thing_manager.GetStatesJson(states, false)) {
            protocol_->SendIotStates(states);
        }
    });
    //设置音频通道关闭回调
    protocol_->OnAudioChannelClosed([this, &board]() {
        board.SetPowerSaveMode(true);   //关闭省电模式
        Schedule([this]() {
            auto display = Board::GetInstance().GetDisplay();
            display->SetChatMessage("system", "");
            SetDeviceState(kDeviceStateIdle);
        });
    });
    //设置接收 json 数据回调
    protocol_->OnIncomingJson([this, display](const cJSON* root) {
        // Parse JSON data  //解析 json 数据
        auto type = cJSON_GetObjectItem(root, "type");
        if (strcmp(type->valuestring, "tts") == 0) {    //处理 tts 消息，文本转语音
            auto state = cJSON_GetObjectItem(root, "state");
            if (strcmp(state->valuestring, "start") == 0) { //开始 tts
                Schedule([this]() {
                    aborted_ = false;
                    if (device_state_ == kDeviceStateIdle || device_state_ == kDeviceStateListening) {
                        SetDeviceState(kDeviceStateSpeaking);
                    }
                });
            } else if (strcmp(state->valuestring, "stop") == 0) {   //停止 tts
                Schedule([this]() {
                    background_task_->WaitForCompletion();
                    if (device_state_ == kDeviceStateSpeaking) {
                        if (listening_mode_ == kListeningModeManualStop) {
                            SetDeviceState(kDeviceStateIdle);
                        } else {
                            SetDeviceState(kDeviceStateListening);
                        }
                    }
                });
            } else if (strcmp(state->valuestring, "sentence_start") == 0) { //句子开始
                auto text = cJSON_GetObjectItem(root, "text");
                if (text != NULL) {
                    ESP_LOGI(TAG, "<< %s", text->valuestring);
                    Schedule([this, display, message = std::string(text->valuestring)]() {
                        display->SetChatMessage("assistant", message.c_str());
                    });
                }
            }
        } else if (strcmp(type->valuestring, "stt") == 0) { //处理 stt 消息，语音转文本
            auto text = cJSON_GetObjectItem(root, "text");
            if (text != NULL) {
                ESP_LOGI(TAG, ">> %s", text->valuestring);
                Schedule([this, display, message = std::string(text->valuestring)]() {
                    display->SetChatMessage("user", message.c_str());
                });
            }
        } else if (strcmp(type->valuestring, "llm") == 0) { //处理 llm 消息，情感表达
            auto emotion = cJSON_GetObjectItem(root, "emotion");
            if (emotion != NULL) {
                Schedule([this, display, emotion_str = std::string(emotion->valuestring)]() {
                    display->SetEmotion(emotion_str.c_str());
                });
            }
        } else if (strcmp(type->valuestring, "iot") == 0) { //处理 iot 消息，
            auto commands = cJSON_GetObjectItem(root, "commands");
            if (commands != NULL) {
                auto& thing_manager = iot::ThingManager::GetInstance();
                for (int i = 0; i < cJSON_GetArraySize(commands); ++i) {
                    auto command = cJSON_GetArrayItem(commands, i);
                    thing_manager.Invoke(command);
                }
            }
        } else if (strcmp(type->valuestring, "system") == 0) {  //处理系统消息
            auto command = cJSON_GetObjectItem(root, "command");
            if (command != NULL) {
                ESP_LOGI(TAG, "System command: %s", command->valuestring);
                if (strcmp(command->valuestring, "reboot") == 0) {  //重启命令
                    // Do a reboot if user requests a OTA update
                    Schedule([this]() {
                        Reboot();
                    });
                } else {
                    ESP_LOGW(TAG, "Unknown system command: %s", command->valuestring);
                }
            }
        } else if (strcmp(type->valuestring, "alert") == 0) {   //处理警告消息
            auto status = cJSON_GetObjectItem(root, "status");
            auto message = cJSON_GetObjectItem(root, "message");
            auto emotion = cJSON_GetObjectItem(root, "emotion");
            if (status != NULL && message != NULL && emotion != NULL) {
                Alert(status->valuestring, message->valuestring, emotion->valuestring, Lang::Sounds::P3_VIBRATION);
            } else {
                ESP_LOGW(TAG, "Alert command requires status, message and emotion");
            }
        }
    });

    //启动协议
    bool protocol_started = protocol_->Start();

    //初始化音频处理器
    audio_processor_->Initialize(codec);
    audio_processor_->OnOutput([this](std::vector<int16_t>&& data) {
        background_task_->Schedule([this, data = std::move(data)]() mutable {
            if (protocol_->IsAudioChannelBusy()) {
                return;
            }
            opus_encoder_->Encode(std::move(data), [this](std::vector<uint8_t>&& opus) {
                AudioStreamPacket packet;
                packet.payload = std::move(opus);
#ifdef CONFIG_USE_SERVER_AEC
                {
                    std::lock_guard<std::mutex> lock(timestamp_mutex_);
                    if (!timestamp_queue_.empty()) {
                        packet.timestamp = timestamp_queue_.front();
                        timestamp_queue_.pop_front();
                    } else {
                        packet.timestamp = 0;
                    }

                    if (timestamp_queue_.size() > 3) { // 限制队列长度3
                        timestamp_queue_.pop_front(); // 该包发送前先出队保持队列长度
                        return;
                    }
                }
#endif
                Schedule([this, packet = std::move(packet)]() {
                    protocol_->SendAudio(packet);
                });
            });
        });
    });

    //设置 vad 状态变化回调
    audio_processor_->OnVadStateChange([this](bool speaking) {
        if (device_state_ == kDeviceStateListening) {
            Schedule([this, speaking]() {
                if (speaking) {
                    voice_detected_ = true;
                } else {
                    voice_detected_ = false;
                }
                auto led = Board::GetInstance().GetLed();
                led->OnStateChanged();
            });
        }
    });

#if CONFIG_USE_WAKE_WORD_DETECT //如果 唤醒词检测 启用
    //初始 化唤醒词检测
    wake_word_detect_.Initialize(codec);
    wake_word_detect_.OnWakeWordDetected([this](const std::string& wake_word) {
        Schedule([this, &wake_word]() {
            if (device_state_ == kDeviceStateIdle) {
                SetDeviceState(kDeviceStateConnecting);
                wake_word_detect_.EncodeWakeWordData();

                if (!protocol_ || !protocol_->OpenAudioChannel()) {
                    wake_word_detect_.StartDetection();
                    return;
                }
                
                AudioStreamPacket packet;
                // Encode and send the wake word data to the server
                //编码并发送唤醒词数据到服务器
                while (wake_word_detect_.GetWakeWordOpus(packet.payload)) {
                    protocol_->SendAudio(packet);
                }
                // Set the chat state to wake word detected
                //设置聊天状态为 唤醒词检测
                protocol_->SendWakeWordDetected(wake_word);
                ESP_LOGI(TAG, "Wake word detected: %s", wake_word.c_str());
                SetListeningMode(realtime_chat_enabled_ ? kListeningModeRealtime : kListeningModeAutoStop);
            } else if (device_state_ == kDeviceStateSpeaking) {
                AbortSpeaking(kAbortReasonWakeWordDetected);
            } else if (device_state_ == kDeviceStateActivating) {
                SetDeviceState(kDeviceStateIdle);
            }
        });
    });
    wake_word_detect_.StartDetection();
#endif

    // Wait for the new version check to finish
    //等待新版本检查完成
    xEventGroupWaitBits(event_group_, CHECK_NEW_VERSION_DONE_EVENT, pdTRUE, pdFALSE, portMAX_DELAY);
    SetDeviceState(kDeviceStateIdle);

    if (protocol_started) {
        std::string message = std::string(Lang::Strings::VERSION) + ota_.GetCurrentVersion();
        display->ShowNotification(message.c_str());
        display->SetChatMessage("system", "");
        // Play the success sound to indicate the device is ready
        //播放成功音效表示设备就绪
        ResetDecoder();
        PlaySound(Lang::Sounds::P3_SUCCESS);
    }
    
    // Enter the main event loop
    //进入主事件循环
    MainEventLoop();
}

//=========== 12、时钟定时器回调函数 ==================
/**
 * @brief 时钟定时器回调函数
 * 
 */
void Application::OnClockTimer() {
    //增加时钟计数
    clock_ticks_++;

    // Print the debug info every 10 seconds
    //每十秒执行一次调试信息打印
    if (clock_ticks_ % 10 == 0) {
        // SystemInfo::PrintRealTimeStats(pdMS_TO_TICKS(1000));
        //获取系统内存使用情况
        int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGI(TAG, "Free internal: %u minimal internal: %u", free_sram, min_free_sram);

        // If we have synchronized server time, set the status to clock "HH:MM" if the device is idle
        //如果已同步服务器时间且设备处于空闲状态，则更新显示状态为当前时间
        if (ota_.HasServerTime()) {
            if (device_state_ == kDeviceStateIdle) {
                Schedule([this]() {
                    // Set status to clock "HH:MM"
                    //获取当前时间并格式化为  "HH：MM" 格式
                    time_t now = time(NULL);
                    char time_str[64];
                    strftime(time_str, sizeof(time_str), "%H:%M  ", localtime(&now));
                    //更新显示状态为当前时间
                    Board::GetInstance().GetDisplay()->SetStatus(time_str);
                });
            }
        }
    }
}

// Add a async task to MainLoop
//=========== 13、添加异步任务 ==================
void Application::Schedule(std::function<void()> callback) {
    {   
        std::lock_guard<std::mutex> lock(mutex_);       //加锁
        main_tasks_.push_back(std::move(callback));     //添加任务到任务列表
    }
    xEventGroupSetBits(event_group_, SCHEDULE_EVENT);   //设置事件组中的事件，通知主事件循环执行任务
}

// The Main Event Loop controls the chat state and websocket connection
// If other tasks need to access the websocket or chat state,
// they should use Schedule to call this function

void Application::MainEventLoop() { //主事件循环
    while (true) {
        //等待事件组中的事件
        auto bits = xEventGroupWaitBits(event_group_, SCHEDULE_EVENT, pdTRUE, pdFALSE, portMAX_DELAY);

        //如果事件组中存在 SCHEDULE_EVENT 事件，则执行任务
        if (bits & SCHEDULE_EVENT) {
            //执行任务
            std::unique_lock<std::mutex> lock(mutex_);  //加锁
            //移动 任务列表 main_tasks_ 到 tasks
            std::list<std::function<void()>> tasks = std::move(main_tasks_);
            lock.unlock();  //解锁
            
            //执行任务
            for (auto& task : tasks) {
                task();
            }
        }
    }
}

// The Audio Loop is used to input and output audio data
//=========== 12、音频处理循环 =======================
void Application::AudioLoop() {
    auto codec = Board::GetInstance().GetAudioCodec();
    while (true) {
        OnAudioInput();
        if (codec->output_enabled()) {
            OnAudioOutput();
        }
    }
}

//============ 13、音频输出 ======================
void Application::OnAudioOutput() {
    //如果正在解码音频，则返回
    if (busy_decoding_audio_) {
        return;
    }

    auto now = std::chrono::steady_clock::now();        //获取当前时间
    auto codec = Board::GetInstance().GetAudioCodec();  //获取音频编解码器
    const int max_silence_seconds = 10;                 //设置最大静默时间

    std::unique_lock<std::mutex> lock(mutex_);  //加锁==================================================
    //如果解码队列为空，则返回
    if (audio_decode_queue_.empty()) {
        // Disable the output if there is no audio data for a long time
        //如果设备状态为空闲，则禁用音频输出
        if (device_state_ == kDeviceStateIdle) {
            //计算音频输出时间
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_output_time_).count();
            //如果音频输出时间大于最大时间，则禁用音频输出
            if (duration > max_silence_seconds) {
                codec->EnableOutput(false);
            }
        }
        return;
    }

    //如果设备为监听状态，则清空解码队列
    if (device_state_ == kDeviceStateListening) {
        audio_decode_queue_.clear();
        audio_decode_cv_.notify_all();
        return;
    }

    auto packet = std::move(audio_decode_queue_.front());   //获取解码队列的第一个元素
    audio_decode_queue_.pop_front();                        //删除解码队列的第一个元素
    lock.unlock();                      //解锁，此锁用于保护解码队列======================================

    audio_decode_cv_.notify_all();      //通知解码队列，有数据可以解码了

    busy_decoding_audio_ = true;        //设置正在解码音频标志
    //添加解码任务
    background_task_->Schedule([this, codec, packet = std::move(packet)]() mutable {
        busy_decoding_audio_ = false;   
        if (aborted_) {
            return;
        }

        std::vector<int16_t> pcm;   //定义pcm数据
        //解码音频数据，失败，则返回
        if (!opus_decoder_->Decode(std::move(packet.payload), pcm)) {
            return;
        }
        // Resample if the sample rate is different
        //如果解码器采样率与输出采样率不同，则重新采样
        if (opus_decoder_->sample_rate() != codec->output_sample_rate()) {
            int target_size = output_resampler_.GetOutputSamples(pcm.size());       //重采样后数据大小
            std::vector<int16_t> resampled(target_size);                            //重采样后的数据
            output_resampler_.Process(pcm.data(), pcm.size(), resampled.data());    //重采样
            pcm = std::move(resampled);     //将重采样后的数据赋值给pcm
        }
        codec->OutputData(pcm); //输出音频数据

#ifdef CONFIG_USE_SERVER_AEC    //如果启用了服务器回声消除
            std::lock_guard<std::mutex> lock(timestamp_mutex_); //加锁
            timestamp_queue_.push_back(packet.timestamp);       //将时间戳加入到时间戳队列
            last_output_timestamp_ = packet.timestamp;          //更新最后输出时间戳
#endif
        last_output_time_ = std::chrono::steady_clock::now();   //更新最后输出时间
    });
}

//============ 14、音频输入 ======================
void Application::OnAudioInput() {
#if CONFIG_USE_WAKE_WORD_DETECT     //如果启用了唤醒词检测

    //如果唤醒词检测正在运行
    if (wake_word_detect_.IsDetectionRunning()) {

        std::vector<int16_t> data;                      //定义音频数据
        int samples = wake_word_detect_.GetFeedSize();  //获取音频数据大小

        //如果音频数据大小 >0
        if (samples > 0) {
            ReadAudio(data, 16000, samples);    //读取音频数据
            wake_word_detect_.Feed(data);       //喂入音频数据
            return;
        }
    }
#endif
    //如果音频处理器正在运行
    if (audio_processor_->IsRunning()) {

        std::vector<int16_t> data;                      //定义音频数据
        int samples = audio_processor_->GetFeedSize();  //获取音频数据大小

        //如果音频数据大小 >0
        if (samples > 0) {
            ReadAudio(data, 16000, samples);    //读取音频数据
            audio_processor_->Feed(data);       //喂入数据
            return;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(30));  //等待 30ms
}

//============ 15、读取音频数据 ================== dma
void Application::ReadAudio(std::vector<int16_t>& data, int sample_rate, int samples) {
    auto codec = Board::GetInstance().GetAudioCodec();  //获取音频编码器
    //如果音频编码器采样率与设置的采样率不同，则进行重采样
    if (codec->input_sample_rate() != sample_rate) {
        data.resize(samples * codec->input_sample_rate() / sample_rate);        //重采样，根据采样率计算数据大小
        //如果输入数据失败，则返回
        if (!codec->InputData(data)) {
            return;
        }
        //如果音频编码器输入通道为2，则进行双通道重采样，否则进行单通道重采样
        if (codec->input_channels() == 2) {
            auto mic_channel = std::vector<int16_t>(data.size() / 2);           //创建麦克风通道
            auto reference_channel = std::vector<int16_t>(data.size() / 2);     //创建参考通道
            //将数据从data中复制到麦克风通道和参考通道
            for (size_t i = 0, j = 0; i < mic_channel.size(); ++i, j += 2) {    //遍历麦克风通道
                mic_channel[i] = data[j];
                reference_channel[i] = data[j + 1];
            }
            auto resampled_mic = std::vector<int16_t>(input_resampler_.GetOutputSamples(mic_channel.size()));                   //创建重采样后的麦克风通道
            auto resampled_reference = std::vector<int16_t>(reference_resampler_.GetOutputSamples(reference_channel.size()));   //创建重采样后的参考通道
            input_resampler_.Process(mic_channel.data(), mic_channel.size(), resampled_mic.data());                             //重采样麦克风通道，将麦克风通道数据重采样到resampled_mic
            reference_resampler_.Process(reference_channel.data(), reference_channel.size(), resampled_reference.data());       //重采样参考通道
            data.resize(resampled_mic.size() + resampled_reference.size());     //重采样后的数据大小
            //将重采样后的麦克风通道数据、参考通道数据复制到data
            for (size_t i = 0, j = 0; i < resampled_mic.size(); ++i, j += 2) {
                data[j] = resampled_mic[i];             //将重采样后的麦克风通道数据复制到data     
                data[j + 1] = resampled_reference[i];   //将重采样后的参考通道数据复制到data
            }
        } else {    //单通道采样
            auto resampled = std::vector<int16_t>(input_resampler_.GetOutputSamples(data.size()));  //创建重采样后的数据
            input_resampler_.Process(data.data(), data.size(), resampled.data());                   //重采样数据
            data = std::move(resampled);                //将重采样后的数据复制到data
        }
    } else {    //
        data.resize(samples);   //重采样后数据大小
        //如果输入数据失败，则返回
        if (!codec->InputData(data)) {
            return;
        }
    }
}

//============ 16、停止说话 =======================
void Application::AbortSpeaking(AbortReason reason) {
    ESP_LOGI(TAG, "Abort speaking");    //打印停止说话日志
    aborted_ = true;                    //设置停止说话标志
    protocol_->SendAbortSpeaking(reason);   //发送停止说话指令
}

//============ 17、设置监听模式 =======================
void Application::SetListeningMode(ListeningMode mode) {
    listening_mode_ = mode; //设置监听模式
    SetDeviceState(kDeviceStateListening);  //设备切换到监听状态
}

//============ 18、设置设备状态 =======================
void Application::SetDeviceState(DeviceState state) {
    //如果设备状态没有变化，则返回
    if (device_state_ == state) {
        return;
    }
    
    clock_ticks_ = 0;   //重置时钟计数
    auto previous_state = device_state_;    //保留当前设备状态
    device_state_ = state;  //更新设备状态
    ESP_LOGI(TAG, "STATE: %s", STATE_STRINGS[device_state_]);   //打印设备状态

    // The state is changed, wait for all background tasks to finish
    background_task_->WaitForCompletion();  //state已经改变，等待所有后台任务完成

    auto& board = Board::GetInstance(); //获取开发板实例
    auto display = board.GetDisplay();  //获取显示器实例
    auto led = board.GetLed();  //获取led实例
    led->OnStateChanged();      //led状态改变，更新led状态

    //根据设备状态进行不同操作
    switch (state) {
        case kDeviceStateUnknown:       //未知状态============= 1
        case kDeviceStateIdle:          //空闲状态============= 2
            display->SetStatus(Lang::Strings::STANDBY); //设置显示状态为待机
            display->SetEmotion("neutral"); //设置表情为中性
            audio_processor_->Stop();       //停止音频处理
            
#if CONFIG_USE_WAKE_WORD_DETECT //如果启用唤醒词检测
            wake_word_detect_.StartDetection(); //启动唤醒词检测
#endif
            break;
        case kDeviceStateConnecting:    //连接状态============= 3
            display->SetStatus(Lang::Strings::CONNECTING);  //设置显示状态为连接
            display->SetEmotion("neutral");         //设置表情为中性
            display->SetChatMessage("system", "");  //设置聊天消息为空
            timestamp_queue_.clear();               //清理时间戳队列
            last_output_timestamp_ = 0;             //重置时间戳
            break;
        case kDeviceStateListening:     //监听状态============= 4
            display->SetStatus(Lang::Strings::LISTENING);   //设置显示状态为监听
            display->SetEmotion("neutral");                 //设置表情为中性
            // Update the IoT states before sending the start listening command
            UpdateIotStates();  //更新物联网状态

            // Make sure the audio processor is running
            //如果音频处理器没有运行，则启动音频处理器
            if (!audio_processor_->IsRunning()) {
                // Send the start listening command
                protocol_->SendStartListening(listening_mode_); //发送开始监听命令
                if (listening_mode_ == kListeningModeAutoStop && previous_state == kDeviceStateSpeaking) {
                    // FIXME: Wait for the speaker to empty the buffer
                    vTaskDelay(pdMS_TO_TICKS(120));     //等待120ms
                }
                opus_encoder_->ResetState();    //重置opus编码器
#if CONFIG_USE_WAKE_WORD_DETECT                     //如果启用了唤醒词检测
                wake_word_detect_.StopDetection();  //停止唤醒词检测
#endif
                audio_processor_->Start();      //启动音频处理器
            }
            break;
        case kDeviceStateSpeaking:      //说话状态============= 5
            display->SetStatus(Lang::Strings::SPEAKING);        //设置显示状态为说话状态
            if (listening_mode_ != kListeningModeRealtime) {    //如果监听模式不是实时模式
                audio_processor_->Stop();   //停止音频处理器
#if CONFIG_USE_WAKE_WORD_DETECT                     //如果启用了唤醒词检测
                wake_word_detect_.StartDetection(); //启动唤醒词检测
#endif
            }
            ResetDecoder(); //重置解码器
            break;
        default:    //默认状态============= 6
            // Do nothing
            break;
    }
}

//============ 19、重置解码器 =======================
void Application::ResetDecoder() {
    std::lock_guard<std::mutex> lock(mutex_);
    opus_decoder_->ResetState();    //重置解码器
    audio_decode_queue_.clear();    //清空解码器队列
    audio_decode_cv_.notify_all();  //通知解码器队列有数据
    last_output_time_ = std::chrono::steady_clock::now();   //更新输出时间
    auto codec = Board::GetInstance().GetAudioCodec();      //获取音频编码器
    codec->EnableOutput(true);      //启用音频输出
}

//============ 20、设置解码采样率 ====================
void Application::SetDecodeSampleRate(int sample_rate, int frame_duration) {
    //如果解码器采样率与设置的采样率相同，则返回
    if (opus_decoder_->sample_rate() == sample_rate && opus_decoder_->duration_ms() == frame_duration) {
        return;
    }

    opus_decoder_.reset();  //重置解码器
    opus_decoder_ = std::make_unique<OpusDecoderWrapper>(sample_rate, 1, frame_duration);   //创建解码器

    auto codec = Board::GetInstance().GetAudioCodec();  //获取音频编码器
    //如果音频编码器采样率与解码器采样率不同，则配置重采样器
    if (opus_decoder_->sample_rate() != codec->output_sample_rate()) {
        ESP_LOGI(TAG, "Resampling audio from %d to %d", opus_decoder_->sample_rate(), codec->output_sample_rate());
        //配置重采样器
        output_resampler_.Configure(opus_decoder_->sample_rate(), codec->output_sample_rate());
    }
}

//============ 21、更新物联网状态 =====================
void Application::UpdateIotStates() {
    auto& thing_manager = iot::ThingManager::GetInstance(); //获取物联网实例
    std::string states;                     //定义状态字符串
    if (thing_manager.GetStatesJson(states, true)) {        //获取物联网状态
        protocol_->SendIotStates(states);   //发送物联网状态
    }
}

//============ 22、重启设备 ===========================
void Application::Reboot() {
    ESP_LOGI(TAG, "Rebooting...");
    esp_restart();  //重启设备
}

//=========== 23、唤醒词触发 ====================
void Application::WakeWordInvoke(const std::string& wake_word) {
    if (device_state_ == kDeviceStateIdle) {                //如果设备处于空闲状态============= 1
        ToggleChatState();  //切换聊天状态，切换到监听状态
        Schedule([this, wake_word]() {
            if (protocol_) {
                protocol_->SendWakeWordDetected(wake_word); //发送唤醒词检测
            }   
        }); 
    } else if (device_state_ == kDeviceStateSpeaking) {     //如果设备处于说话状态============= 2
        Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);    //停止说话
        });
    } else if (device_state_ == kDeviceStateListening) {    //如果设备处于监听状态============= 3
        Schedule([this]() {
            if (protocol_) {
                protocol_->CloseAudioChannel(); //关闭音频通道
            }
        });
    }
}

//=========== 24、判断是否可以使用睡眠 ====================
bool Application::CanEnterSleepMode() {
    //如果设备不是空闲状态，则不能进入睡眠状态
    if (device_state_ != kDeviceStateIdle) {
        return false;
    }

    //如果音频通道打开，则不能进入睡眠状态
    if (protocol_ && protocol_->IsAudioChannelOpened()) {
        return false;
    }

    // Now it is safe to enter sleep mode
    return true;    //状态检测通过，可以进入睡眠状态
}
