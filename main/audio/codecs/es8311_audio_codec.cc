#include "es8311_audio_codec.h"
extern "C" {
#include <driver/i2s_std.h>
    }
#include <esp_log.h>

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
        .rx_handle = nullptr,
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
    /* Create control interface with I2C bus handle */
    audio_codec_i2c_cfg_t i2c_cfg_7210 = {
        .port = i2c_port,
        .addr = es7210_addr,
        .bus_handle = i2c_master_handle,
    };
    ctrl_if_7210 = audio_codec_new_i2c_ctrl(&i2c_cfg_7210);
    assert(ctrl_if_7210);

    /* Create data interface with I2S bus handle */
    audio_codec_i2s_cfg_t i2s_cfg_7210 = {
        .port = I2S_NUM_0,
        .rx_handle = rx_handle_,
        .tx_handle = nullptr,
    };
    data_if_7210 = audio_codec_new_i2s_data(&i2s_cfg_7210);
    assert(data_if_7210);

    /* Create ES7210 interface handle */
    ESP_LOGI(TAG, "Configure ES7210 codec parameters");
    es7210_codec_cfg_t es7210_cfg = {
        .ctrl_if = ctrl_if_7210,
        .master_mode = false,
        .mic_selected = EXAMPLE_ES7210_MIC_SELECTED,
        .mclk_src = ES7210_MCLK_FROM_PAD,
        .mclk_div = EXAMPLE_I2S_MCLK_MULTIPLE,
    };
    codec_if_7210 = es7210_codec_new(&es7210_cfg);
    assert(codec_if_7210);


    
    // /* Create the top codec handle with ES7210 interface handle and data interface */
    // esp_codec_dev_cfg_t dev_cfg = {
    //     .dev_type = ESP_CODEC_DEV_TYPE_IN,
    //     .codec_if = codec_if_7210,
    //     .data_if = data_if_7210,
    // };
    // esp_codec_dev_handle_t codec_handle = esp_codec_dev_new(&dev_cfg);
    // assert(codec_handle);

    // /* Specify the sample configurations and open the device */
    // esp_codec_dev_sample_info_t sample_cfg = {
    //     .bits_per_sample = EXAMPLE_I2S_SAMPLE_BITS,
    //     .channel = EXAMPLE_I2S_CHAN_NUM,
    //     .channel_mask = EXAMPLE_ES7210_MIC_SELECTED,
    //     .sample_rate = EXAMPLE_I2S_SAMPLE_RATE,
    // };
    // if (esp_codec_dev_open(codec_handle, &sample_cfg) != ESP_CODEC_DEV_OK) {
    //     ESP_LOGE(TAG, "Open codec device failed");
    //     assert(false);
    // }

    // /* Set the initial gain */
    // if (esp_codec_dev_set_in_gain(codec_handle, EXAMPLE_ES7210_MIC_GAIN) != ESP_CODEC_DEV_OK) {
    //     ESP_LOGE(TAG, "set input gain failed");
    //     assert(false);
    // }
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
            .bits_per_sample = 16,
            .channel = 1,
            .channel_mask = 0,
            .sample_rate = (uint32_t)output_sample_rate_,
            .mclk_multiple = 0,
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

    // 初始化i2s下行通道，esp32 -> es8311
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)output_sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
			#ifdef   I2S_HW_VERSION_2    
				.ext_clk_freq_hz = 0,
			#endif
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
            #ifdef   I2S_HW_VERSION_2   
                .left_align = true,
                .big_endian = false,
                .bit_order_lsb = false
            #endif
        },
        .gpio_cfg = {
            .mclk = mclk,
            .bclk = bclk,
            .ws = ws,
            .dout = dout,
            .din = din,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));


    // =====================================================================
    // 初始化i2s上行通道，es7210 -> esp32

    i2s_std_config_t std_cfg_7210 = {
        // es7210 driver is default to use philips format in esp_codec_dev component
        .clk_cfg  = {
            .sample_rate_hz = (uint32_t)input_sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = EXAMPLE_I2S_MCLK_MULTIPLE
        },
        .slot_cfg = {
            .data_bit_width = EXAMPLE_I2S_SAMPLE_BITS,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = EXAMPLE_I2S_SAMPLE_BITS,
            .ws_pol = false,
            .bit_shift = true,
        },
        .gpio_cfg = {
            .mclk = mclk,
            .bclk = bclk,
            .ws   = ws,
            .dout = GPIO_NUM_NC, // ES7210 only has ADC capability
            .din  = din
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg_7210));
    // ======================================================================

    //ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));
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

int Es8311AudioCodec::Read(int16_t* dest, int samples) {
    if (input_enabled_) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_read(dev_, (void*)dest, samples * sizeof(int16_t)));
    }
    return samples;
}

int Es8311AudioCodec::Write(const int16_t* data, int samples) {
    if (output_enabled_) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_write(dev_, (void*)data, samples * sizeof(int16_t)));
    }
    return samples;
}