#include "i2s_es7210_audio_codec.h"

#include <esp_log.h>

static const char *TAG = "Es7210AudioCodec" ;

Es7210AudioCodec::Es7210AudioCodec ( void* i2c_master_handle, i2c_port_t i2c_port, int input_sample_rate,
                                    gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout,  
                                    uint8_t es7210_addr, bool use_mclk = true, bool pa_inverted = false)
{
    duplex_ = true; // 是否双工
    input_reference_ = false; // 是否使用参考输入，实现回声消除
    input_channels_ = 2; // 输入通道数
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;
    //pa_pin_ = pa_pin;
    //pa_inverted_ = pa_inverted;

    assert(input_sample_rate_ == output_sample_rate_);
    //CreateDuplexChannels(mclk, bclk, ws, dout, din);///////////////

    const audio_codec_if_t* codec_if_ = nullptr;
    const audio_codec_data_if_t* data_if_ = nullptr;
    const audio_codec_ctrl_if_t* ctrl_if_ = nullptr;
    const audio_codec_gpio_if_t* gpio_if_ = nullptr;


    // Do initialize of related interface: <data_if>, <ctrl_if> and <gpio_if>
    // 定义 I2S 数据接口
    audio_codec_i2s_cfg_t i2s_cfg = {
    .port = I2S_NUM_0,
    .rx_handle = rx_handle_,
    .tx_handle = tx_handle_,
    };
    data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
    assert(data_if_ != NULL);
    ESP_LOGI(TAG, "I2S数据接口创建成功");


    // 定义 I2C 控制接口
    audio_codec_i2c_cfg_t i2c_cfg = {
    .port = i2c_port,
    .addr = es8311_addr,
    .bus_handle = i2c_master_handle,
    };
    ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(ctrl_if_ != NULL);
    ESP_LOGI(TAG, "I2C控制接口创建成功");


    // 定义 GPIO 接口
    gpio_if_ = audio_codec_new_gpio();
    assert(gpio_if_ != NULL);
    ESP_LOGI(TAG, "GPIO接口创建成功");


    // 创建 es8311 音频编码器
    es8311_codec_cfg_t es8311_cfg = {};
    es8311_cfg.ctrl_if = ctrl_if_;
    es8311_cfg.gpio_if = gpio_if_;
    es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH;
    es8311_cfg.pa_pin = pa_pin;
    es8311_cfg.use_mclk = use_mclk;
    es8311_cfg.hw_gain.pa_voltage = 5.0;
    es8311_cfg.hw_gain.codec_dac_voltage = 3.3;
    es8311_cfg.pa_reverted = pa_inverted_;
    codec_if_ = es8311_codec_new(&es8311_cfg);
    assert(codec_if_ != NULL);

    ESP_LOGI(TAG, "Es8311AudioCodec initialized");
}






// 初始化 es7210 的 i2s 通道
void Es7210AudioCodec::CreateRXChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t din)
{
    ESP_LOGI(TAG, "Create I2S receive channel");
    i2s_chan_config_t i2s_rx_conf = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&i2s_rx_conf, NULL, &rx_handle_));

    
    ESP_LOGI(TAG, "Configure I2S receive channel to TDM mode");

    i2s_tdm_config_t i2s_tdm_rx_conf = {
        // es7210 driver is default to use philips format in esp_codec_dev component
        .slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(
            EXAMPLE_I2S_SAMPLE_BITS,
            I2S_SLOT_MODE_STEREO,
            EXAMPLE_I2S_TDM_SLOT_MASK),
        .clk_cfg  = {
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .sample_rate_hz = EXAMPLE_I2S_SAMPLE_RATE,
            .mclk_multiple = EXAMPLE_I2S_MCLK_MULTIPLE
        },
        .gpio_cfg = {
            .mclk = mclk,
            .bclk = bclk,
            .ws   = ws,
            .dout = -1, // ES7210 only has ADC capability
            .din  = din
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_tdm_mode(rx_handle_, &i2s_tdm_rx_conf));
}


// 初始化 es7210
static esp_codec_dev_handle_t es7210_codec_init(i2s_chan_handle_t i2s_rx_chan)
{

    /* Create control interface with I2C bus handle */
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = EXAMPLE_I2C_NUM,
        .addr = ES7210_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_bus_handle,
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(ctrl_if);

    /* Create data interface with I2S bus handle */
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = EXAMPLE_I2S_NUM,
        .rx_handle = i2s_rx_chan,
        .tx_handle = NULL,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);
    assert(data_if);

    /* Create ES7210 interface handle */
    ESP_LOGI(TAG, "Configure ES7210 codec parameters");
    es7210_codec_cfg_t es7210_cfg = {
        .ctrl_if = ctrl_if,
        .master_mode = false,
        .mic_selected = EXAMPLE_ES7210_MIC_SELECTED,
        .mclk_src = ES7210_MCLK_FROM_PAD,
        .mclk_div = EXAMPLE_I2S_MCLK_MULTIPLE,
    };
    const audio_codec_if_t *es7210_if = es7210_codec_new(&es7210_cfg);
    assert(es7210_if);

    
    /* Create the top codec handle with ES7210 interface handle and data interface */
    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = es7210_if,
        .data_if = data_if,
    };
    esp_codec_dev_handle_t codec_handle = esp_codec_dev_new(&dev_cfg);
    assert(codec_handle);

    /* Specify the sample configurations and open the device */
    esp_codec_dev_sample_info_t sample_cfg = {
        .bits_per_sample = EXAMPLE_I2S_SAMPLE_BITS,
        .channel = EXAMPLE_I2S_CHAN_NUM,
        .channel_mask = EXAMPLE_ES7210_MIC_SELECTED,
        .sample_rate = EXAMPLE_I2S_SAMPLE_RATE,
    };
    if (esp_codec_dev_open(codec_handle, &sample_cfg) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Open codec device failed");
        assert(false);
    }

    /* Set the initial gain */
    if (esp_codec_dev_set_in_gain(codec_handle, EXAMPLE_ES7210_MIC_GAIN) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "set input gain failed");
        assert(false);
    }

    return codec_handle;
}
