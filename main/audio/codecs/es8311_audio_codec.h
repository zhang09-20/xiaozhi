#ifndef _ES8311_AUDIO_CODEC_H
#define _ES8311_AUDIO_CODEC_H

#include "audio_codec.h"

#include <driver/i2c_master.h>
#include <driver/gpio.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>

/* I2S configurations */
#define EXAMPLE_I2S_MCLK_MULTIPLE  (I2S_MCLK_MULTIPLE_256)
#define EXAMPLE_I2S_SAMPLE_BITS    (I2S_DATA_BIT_WIDTH_16BIT)
#define EXAMPLE_I2S_TDM_SLOT_MASK  (I2S_TDM_SLOT0 | I2S_TDM_SLOT1)

/* ES7210 configurations */
//#define EXAMPLE_ES7210_I2C_ADDR    (0x40)
#define EXAMPLE_ES7210_MIC_GAIN    (30)  // 30db
#define EXAMPLE_ES7210_MIC_SELECTED (ES7120_SEL_MIC1 | ES7120_SEL_MIC2)

class Es8311AudioCodec : public AudioCodec {
private:
    const audio_codec_data_if_t* data_if_ = nullptr;
    const audio_codec_ctrl_if_t* ctrl_if_ = nullptr;
    const audio_codec_if_t* codec_if_ = nullptr;
    const audio_codec_gpio_if_t* gpio_if_ = nullptr;

    // =====================================================
    const audio_codec_data_if_t* data_if_7210 = nullptr;
    const audio_codec_ctrl_if_t* ctrl_if_7210 = nullptr;
    const audio_codec_if_t* codec_if_7210 = nullptr;
    //const audio_codec_gpio_if_t* gpio_if_a = nullptr;
    
    esp_codec_dev_handle_t dev_7210 = nullptr;
    // =====================================================
    

    esp_codec_dev_handle_t dev_ = nullptr;
    gpio_num_t pa_pin_ = GPIO_NUM_NC;
    bool pa_inverted_ = false;

    void CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din);
    void UpdateDeviceState();

    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;

public:
    Es8311AudioCodec(   void* i2c_master_handle, i2c_port_t i2c_port, 
                        int input_sample_rate, int output_sample_rate,
                        gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
                        gpio_num_t pa_pin, uint8_t es8311_addr, uint8_t es7210_addr, 
                        bool use_mclk = true, bool pa_inverted = false);
    virtual ~Es8311AudioCodec();

    virtual void SetOutputVolume(int volume) override;
    virtual void EnableInput(bool enable) override;
    virtual void EnableOutput(bool enable) override;
};

#endif // _ES8311_AUDIO_CODEC_H