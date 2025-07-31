#ifndef _ES8311_AUDIO_CODEC_H
#define _ES8311_AUDIO_CODEC_H

#include "audio_codec.h"

#include <driver/i2c_master.h>
#include <driver/gpio.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>




// =====================================================
// 麦克风检测配置
#define ENABLE_MIC_DETECTION 1  // 启用麦克风检测
#define MIC_DETECTION_SIMPLE  1  // 使用简化检测方案
// =====================================================



// 条件编译 选择要使用的 音频编、解码芯片方案
//#define ES8311_RX_TX
#define ES8311_TX_ES7210_RX

#ifdef ES8311_RX_TX

#elif defined(ES8311_TX_ES7210_RX)
/* I2S configurations */
#define EXAMPLE_I2S_MCLK_MULTIPLE  (I2S_MCLK_MULTIPLE_256)
#define EXAMPLE_I2S_SAMPLE_BITS    (I2S_DATA_BIT_WIDTH_16BIT)
#define EXAMPLE_I2S_TDM_SLOT_MASK  (I2S_TDM_SLOT0 | I2S_TDM_SLOT1)

/* ES7210 configurations */
//#define EXAMPLE_ES7210_I2C_ADDR    (0x40)
#define EXAMPLE_ES7210_MIC_GAIN    (30)  // 30db
#define EXAMPLE_ES7210_MIC_SELECTED (ES7120_SEL_MIC1 | ES7120_SEL_MIC2)

#endif




class Es8311AudioCodec : public AudioCodec {
private:
    const audio_codec_data_if_t* data_if_ = nullptr;
    const audio_codec_ctrl_if_t* ctrl_if_ = nullptr;
    const audio_codec_if_t* codec_if_ = nullptr;
    const audio_codec_gpio_if_t* gpio_if_ = nullptr;

#ifdef ES8311_RX_TX

#elif defined(ES8311_TX_ES7210_RX)
    // =====================================================
    const audio_codec_data_if_t* data_if_7210 = nullptr;
    const audio_codec_ctrl_if_t* ctrl_if_7210 = nullptr;
    const audio_codec_if_t* codec_if_7210 = nullptr;
    //const audio_codec_gpio_if_t* gpio_if_a = nullptr;
    
    esp_codec_dev_handle_t dev_7210 = nullptr;
    // =====================================================

#endif
    
    esp_codec_dev_handle_t dev_ = nullptr;
    gpio_num_t pa_pin_ = GPIO_NUM_NC;
    bool pa_inverted_ = false;

    void CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din);
    void UpdateDeviceState();



    // =====================================================
    // 麦克风检测相关
    bool mic1_working_ = true;
    bool mic2_working_ = true;
    int mic_failure_count_[2] = {0, 0};
    int mic_success_count_[2] = {0, 0};
    int64_t last_mic_check_time_ = 0;
    static const int MIC_CHECK_INTERVAL_MS = 5000;  // 5秒检测一次
    static const int MIC_FAILURE_THRESHOLD = 10;    // 连续10次失败认为麦克风故障
    static const int MIC_SUCCESS_THRESHOLD = 5;     // 连续5次成功认为麦克风正常
    static const int MIC_SIGNAL_THRESHOLD = 50;     // 降低信号强度阈值

    // 麦克风检测方法
    bool DetectMicQuality(int16_t* left_channel, int16_t* right_channel, int samples);
    bool CheckMicSignal(int16_t* channel_data, int samples);
    void UpdateMicStatus();
    void SwitchToSingleMic(int mic_index);
    // =====================================================




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


    
    // =====================================================
    // 麦克风状态查询
    bool IsMic1Working() const { return mic1_working_; }
    bool IsMic2Working() const { return mic2_working_; }
    int GetActiveMicCount() const;
    void ForceMicCheck();
    // =====================================================


};

#endif // _ES8311_AUDIO_CODEC_H