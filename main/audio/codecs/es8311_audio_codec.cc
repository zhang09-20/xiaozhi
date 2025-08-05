#include "es8311_audio_codec.h"
// extern "C" {
// #include <driver/i2s_std.h>
//     }
#include <esp_log.h>

#include <driver/i2c_master.h>
#include <driver/i2s_tdm.h>

// =====================================================
#include <esp_timer.h>
#include <math.h>
#include <string.h>

#define TAG "Es8311AudioCodec"

Es8311AudioCodec::Es8311AudioCodec( void* i2c_master_handle, i2c_port_t i2c_port, 
                                    int input_sample_rate, int output_sample_rate,
                                    gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
                                    gpio_num_t pa_pin, uint8_t es8311_addr, uint8_t es7210_addr,
                                    bool use_mclk, bool pa_inverted) 
{
    duplex_ = true; // 是否双工
    input_reference_ = false; // 是否使用参考输入，实现回声消除
    input_channels_ = 1; // 输入通道数
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;
    pa_pin_ = pa_pin;
    pa_inverted_ = pa_inverted;

    assert(input_sample_rate_ == output_sample_rate_);
    CreateDuplexChannels(mclk, bclk, ws, dout, din);


    
    // Do initialize of related interface: data_if, ctrl_if and gpio_if
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = rx_handle_,
        .tx_handle = tx_handle_,
    };
    data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
    assert(data_if_ != NULL);

    // Output
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = i2c_port,
        .addr = es8311_addr,
        .bus_handle = i2c_master_handle,
    };
    ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(ctrl_if_ != NULL);

    gpio_if_ = audio_codec_new_gpio();
    assert(gpio_if_ != NULL);

    es8311_codec_cfg_t es8311_cfg = {};
    es8311_cfg.ctrl_if = ctrl_if_;
    es8311_cfg.gpio_if = gpio_if_;
    es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC; // 同时负责采集+播放 ESP_CODEC_DEV_WORK_MODE_BOTH;
    es8311_cfg.pa_pin = pa_pin;
    es8311_cfg.use_mclk = use_mclk;
    es8311_cfg.hw_gain.pa_voltage = 5.0;
    es8311_cfg.hw_gain.codec_dac_voltage = 3.3;
    es8311_cfg.pa_reverted = pa_inverted_;
    codec_if_ = es8311_codec_new(&es8311_cfg);
    assert(codec_if_ != NULL);


    // =====================================================
    i2c_cfg.addr = es7210_addr;    
    ctrl_if_7210 = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(ctrl_if_7210);

    es7210_codec_cfg_t es7210_cfg = {};
    es7210_cfg.ctrl_if = ctrl_if_7210;
    es7210_cfg.mic_selected = EXAMPLE_ES7210_MIC_SELECTED;    
    codec_if_7210 = es7210_codec_new(&es7210_cfg);
    assert(codec_if_7210);

    // =======================================================

    ESP_LOGI(TAG, "Es8311、Es7210 AudioCodec initialized");
}

Es8311AudioCodec::~Es8311AudioCodec() {
    esp_codec_dev_delete(dev_);

    audio_codec_delete_codec_if(codec_if_);
    audio_codec_delete_ctrl_if(ctrl_if_);
    audio_codec_delete_gpio_if(gpio_if_);
    audio_codec_delete_data_if(data_if_);

    // =========================================
    audio_codec_delete_codec_if(codec_if_7210);
    audio_codec_delete_ctrl_if(ctrl_if_7210);
    //audio_codec_delete_gpio_if(gpio_if_);
    audio_codec_delete_data_if(data_if_7210);
    
    esp_codec_dev_delete(dev_7210);
    // =========================================
}

void Es8311AudioCodec::UpdateDeviceState() {
    if (output_enabled_ && dev_ == nullptr) {
        esp_codec_dev_cfg_t dev_cfg = {
            .dev_type = ESP_CODEC_DEV_TYPE_OUT,
            .codec_if = codec_if_,
            .data_if = data_if_,
        };
        dev_ = esp_codec_dev_new(&dev_cfg);
        assert(dev_ != NULL);

        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = I2S_DATA_BIT_WIDTH_16BIT,
            .channel = 2,
            .channel_mask = 0x03,
            .sample_rate = (uint32_t)output_sample_rate_,
            //.mclk_multiple = 0,
        };
        ESP_ERROR_CHECK(esp_codec_dev_open(dev_, &fs));
        //ESP_ERROR_CHECK(esp_codec_dev_set_in_gain(dev_, AUDIO_CODEC_DEFAULT_MIC_GAIN));
        ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(dev_, output_volume_));
    } else if (!output_enabled_ && dev_ != nullptr) {
        esp_codec_dev_close(dev_);
        dev_ = nullptr;
    }

    // ==============================================================
    if (input_enabled_  && dev_7210 == nullptr) {
        esp_codec_dev_cfg_t dev_cfg = {
            .dev_type = ESP_CODEC_DEV_TYPE_IN,
            .codec_if = codec_if_7210,
            .data_if = data_if_7210,
        };
        dev_7210 = esp_codec_dev_new(&dev_cfg);
        assert(dev_7210 != NULL);

        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 2,
            .channel_mask = EXAMPLE_ES7210_MIC_SELECTED,
            .sample_rate = (uint32_t)input_sample_rate_,
            .mclk_multiple = 0,
        };
        ESP_ERROR_CHECK(esp_codec_dev_open(dev_7210, &fs));
        ESP_ERROR_CHECK(esp_codec_dev_set_in_gain(dev_7210, AUDIO_CODEC_DEFAULT_MIC_GAIN));
        //ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(dev_7210, output_volume_));
    } else if (!input_enabled_ && dev_7210 != nullptr) {
        esp_codec_dev_close(dev_7210);
        dev_7210 = nullptr;
    }

    // ==============================================================


    if (pa_pin_ != GPIO_NUM_NC) {
        int level = output_enabled_ ? 1 : 0;
        gpio_set_level(pa_pin_, pa_inverted_ ? !level : level);
    }
}

void Es8311AudioCodec::CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din) {
    assert(input_sample_rate_ == output_sample_rate_);

    // 创建i2s上行、下行通道
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM,
        .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));


    // ========================================================
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)output_sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true, 
            .left_align = true,
            .big_endian = false,
            .bit_order_lsb = false
        },
        .gpio_cfg = {
            .mclk = mclk,
            .bclk = bclk,
            .ws = ws,
            .dout = dout,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };  
    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)input_sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            .bclk_div = 8,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = i2s_tdm_slot_mask_t(I2S_TDM_SLOT0 | I2S_TDM_SLOT1),
            .ws_width = I2S_TDM_AUTO_WS_WIDTH,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = false,
            .big_endian = false,
            .bit_order_lsb = false,
            .skip_mask = false,
            .total_slot = I2S_TDM_AUTO_SLOT_NUM
        },
        .gpio_cfg = {
            .mclk = mclk,
            .bclk = bclk,
            .ws = ws,
            .dout = I2S_GPIO_UNUSED,
            .din = din,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_tdm_mode(rx_handle_, &tdm_cfg));
    ESP_LOGI(TAG, "Duplex channels created");
}

void Es8311AudioCodec::SetOutputVolume(int volume) {
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(dev_, volume));
    AudioCodec::SetOutputVolume(volume);
}

void Es8311AudioCodec::EnableInput(bool enable) {
    if (enable == input_enabled_) {
        return;
    }
    AudioCodec::EnableInput(enable);
    UpdateDeviceState();
}

void Es8311AudioCodec::EnableOutput(bool enable) {
    if (enable == output_enabled_) {
        return;
    }
    AudioCodec::EnableOutput(enable);
    UpdateDeviceState();
}

// int Es8311AudioCodec::Read(int16_t* dest, int samples) {
//     if (input_enabled_ && dev_7210 != nullptr) {
//         ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_read(dev_7210, (void*)dest, samples * sizeof(int16_t)));
//     }else{
//         ESP_LOGI(TAG,"input_enabled_ is false，或者 dev_7210 is nullptr");
//     }
//     return samples;
// }
int Es8311AudioCodec::Read(int16_t* dest, int samples) {
    if (input_enabled_ && dev_7210 != nullptr) {
        // 读取立体声数据
        int16_t stereo_buffer[samples * 2];  // 立体声需要2倍空间
        
        // 添加设备状态检查
        esp_err_t ret = esp_codec_dev_read(dev_7210, stereo_buffer, samples * 2 * sizeof(int16_t));
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "esp_codec_dev_read failed: %s", esp_err_to_name(ret));
            // 如果读取失败，返回静音数据
            memset(dest, 0, samples * sizeof(int16_t));
            return samples;
        }
        
    //     // 转换为单声道 (左右声道平均)
    //     for (int i = 0; i < samples; i++) {
    //         dest[i] = (stereo_buffer[i*2] + stereo_buffer[i*2+1]) / 2;
    //     }
    // }

    // ========================================================
        // 麦克风质量检测 (可选)
        #if ENABLE_MIC_DETECTION
        DetectMicQuality(stereo_buffer, stereo_buffer + samples, samples);
        #endif
        
        // 根据麦克风状态选择转换策略
        if (mic1_working_ && mic2_working_) {
            // 两个麦克风都正常，使用加权平均
            for (int i = 0; i < samples; i++) {
                dest[i] = (stereo_buffer[i*2] * 3 + stereo_buffer[i*2+1] * 2) / 5;
            }
        } else if (mic1_working_ && !mic2_working_) {
            // 只用左声道 (MIC1)
            for (int i = 0; i < samples; i++) {
                dest[i] = stereo_buffer[i*2];
            }
        } else if (!mic1_working_ && mic2_working_) {
            // 只用右声道 (MIC2)
            for (int i = 0; i < samples; i++) {
                dest[i] = stereo_buffer[i*2+1];
            }
        } else {
            // 两个麦克风都有问题，使用简单平均
            for (int i = 0; i < samples; i++) {
                dest[i] = (stereo_buffer[i*2] + stereo_buffer[i*2+1]) / 2;
            }
        }
    }
    // =======================================================
    return samples;
}

int Es8311AudioCodec::Write(const int16_t* data, int samples) {
    if (output_enabled_) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_write(dev_, (void*)data, samples * sizeof(int16_t)));
    }
    return samples;
}




// =================================================================

// ==================== 麦克风检测方法实现 ====================

bool Es8311AudioCodec::CheckMicSignal(int16_t* channel_data, int samples) {
    if (samples <= 0) return false;
    
    // 计算信号强度 (RMS)
    int64_t sum_squares = 0;
    for (int i = 0; i < samples; i++) {
        sum_squares += (int64_t)channel_data[i] * channel_data[i];
    }
    
    double rms = sqrt((double)sum_squares / samples);
    
    // 检查信号强度是否超过阈值
    bool has_signal = (rms > MIC_SIGNAL_THRESHOLD);
    
    ESP_LOGD(TAG, "Mic signal RMS: %.2f, threshold: %d, has_signal: %s", 
             rms, MIC_SIGNAL_THRESHOLD, has_signal ? "true" : "false");
    
    return has_signal;
}

bool Es8311AudioCodec::DetectMicQuality(int16_t* left_channel, int16_t* right_channel, int samples) {
    int64_t current_time = esp_timer_get_time() / 1000; // 转换为毫秒
    
    // 每5秒检测一次，避免频繁检测
    if (current_time - last_mic_check_time_ < MIC_CHECK_INTERVAL_MS) {
        return true; // 跳过检测
    }
    
    last_mic_check_time_ = current_time;
    
    // 检测左声道 (MIC1)
    bool mic1_signal = CheckMicSignal(left_channel, samples);
    if (mic1_signal) {
        mic_success_count_[0]++;
        mic_failure_count_[0] = 0;
    } else {
        mic_failure_count_[0]++;
        mic_success_count_[0] = 0;
    }
    
    // 检测右声道 (MIC2)
    bool mic2_signal = CheckMicSignal(right_channel, samples);
    if (mic2_signal) {
        mic_success_count_[1]++;
        mic_failure_count_[1] = 0;
    } else {
        mic_failure_count_[1]++;
        mic_success_count_[1] = 0;
    }
    
    // 添加调试信息
    ESP_LOGD(TAG, "Mic1 signal: %s, Mic2 signal: %s", 
             mic1_signal ? "true" : "false", mic2_signal ? "true" : "false");
    
    // 更新麦克风状态
    UpdateMicStatus();
    
    ESP_LOGI(TAG, "Mic1: working=%s, success=%d, failure=%d", 
             mic1_working_ ? "true" : "false", mic_success_count_[0], mic_failure_count_[0]);
    ESP_LOGI(TAG, "Mic2: working=%s, success=%d, failure=%d", 
             mic2_working_ ? "true" : "false", mic_success_count_[1], mic_failure_count_[1]);
    
    return true;
}

void Es8311AudioCodec::UpdateMicStatus() {
    // 更新MIC1状态
    if (mic_failure_count_[0] >= MIC_FAILURE_THRESHOLD) {
        if (mic1_working_) {
            ESP_LOGW(TAG, "MIC1 detected as failed");
            mic1_working_ = false;
        }
    } else if (mic_success_count_[0] >= MIC_SUCCESS_THRESHOLD) {
        if (!mic1_working_) {
            ESP_LOGI(TAG, "MIC1 detected as working");
            mic1_working_ = true;
        }
    }
    
    // 更新MIC2状态
    if (mic_failure_count_[1] >= MIC_FAILURE_THRESHOLD) {
        if (mic2_working_) {
            ESP_LOGW(TAG, "MIC2 detected as failed");
            mic2_working_ = false;
        }
    } else if (mic_success_count_[1] >= MIC_SUCCESS_THRESHOLD) {
        if (!mic2_working_) {
            ESP_LOGI(TAG, "MIC2 detected as working");
            mic2_working_ = true;
        }
    }
}

void Es8311AudioCodec::SwitchToSingleMic(int mic_index) {
    if (mic_index == 0) {
        // 切换到MIC1
        mic1_working_ = true;
        mic2_working_ = false;
        ESP_LOGI(TAG, "Switched to MIC1 only");
    } else if (mic_index == 1) {
        // 切换到MIC2
        mic1_working_ = false;
        mic2_working_ = true;
        ESP_LOGI(TAG, "Switched to MIC2 only");
    }
}

int Es8311AudioCodec::GetActiveMicCount() const {
    int count = 0;
    if (mic1_working_) count++;
    if (mic2_working_) count++;
    return count;
}

void Es8311AudioCodec::ForceMicCheck() {
    last_mic_check_time_ = 0; // 强制下次检测
    ESP_LOGI(TAG, "Forced mic check requested");
}