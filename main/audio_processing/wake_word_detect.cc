#include "wake_word_detect.h"
#include "application.h"

#include <esp_log.h>
#include <model_path.h>
#include <arpa/inet.h>
#include <sstream>

#define DETECTION_RUNNING_EVENT 1

static const char* TAG = "WakeWordDetect";

//========= 1、构造函数 ======================
WakeWordDetect::WakeWordDetect()
    : afe_data_(nullptr),
      wake_word_pcm_(),
      wake_word_opus_() {

    event_group_ = xEventGroupCreate();
}

//========= 2、析构函数 ======================
WakeWordDetect::~WakeWordDetect() {
    if (afe_data_ != nullptr) {
        afe_iface_->destroy(afe_data_);
    }

    if (wake_word_encode_task_stack_ != nullptr) {
        heap_caps_free(wake_word_encode_task_stack_);
    }

    vEventGroupDelete(event_group_);
}

//========= 3、初始化 ======================
void WakeWordDetect::Initialize(AudioCodec* codec) {
    codec_ = codec;
    int ref_num = codec_->input_reference() ? 1 : 0;

    srmodel_list_t *models = esp_srmodel_init("model");
    for (int i = 0; i < models->num; i++) {
        ESP_LOGI(TAG, "Model %d: %s", i, models->model_name[i]);
        if (strstr(models->model_name[i], ESP_WN_PREFIX) != NULL) {
            wakenet_model_ = models->model_name[i];
            auto words = esp_srmodel_get_wake_words(models, wakenet_model_);
            // split by ";" to get all wake words
            std::stringstream ss(words);
            std::string word;
            while (std::getline(ss, word, ';')) {
                wake_words_.push_back(word);
            }
        }
    }

    std::string input_format;
    for (int i = 0; i < codec_->input_channels() - ref_num; i++) {
        input_format.push_back('M');
    }
    for (int i = 0; i < ref_num; i++) {
        input_format.push_back('R');
    }
    afe_config_t* afe_config = afe_config_init(input_format.c_str(), models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    afe_config->aec_init = codec_->input_reference();
    afe_config->aec_mode = AEC_MODE_SR_HIGH_PERF;
    afe_config->afe_perferred_core = 1;
    afe_config->afe_perferred_priority = 1;
    afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;  // ******************************
    //afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_INTERNAL;
    
    afe_iface_ = esp_afe_handle_from_config(afe_config);
    afe_data_ = afe_iface_->create_from_config(afe_config);

    xTaskCreate([](void* arg) {
        auto this_ = (WakeWordDetect*)arg;
        this_->AudioDetectionTask();
        vTaskDelete(NULL);
    }, "audio_detection", 4096, this, 3, nullptr);
}

//========= 4、设置唤醒词检测回调 ======================
void WakeWordDetect::OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) {
    wake_word_detected_callback_ = callback;
}

//========= 5、开始检测 ======================
void WakeWordDetect::StartDetection() {
    xEventGroupSetBits(event_group_, DETECTION_RUNNING_EVENT);
}

//========= 6、停止检测 ======================
void WakeWordDetect::StopDetection() {
    xEventGroupClearBits(event_group_, DETECTION_RUNNING_EVENT);
    if (afe_data_ != nullptr) {
        afe_iface_->reset_buffer(afe_data_);
    }
}

//========= 7、检测是否正在运行 ======================
bool WakeWordDetect::IsDetectionRunning() {
    return xEventGroupGetBits(event_group_) & DETECTION_RUNNING_EVENT;
}

//========= 8、喂入音频数据 ======================
void WakeWordDetect::Feed(const std::vector<int16_t>& data) {
    if (afe_data_ == nullptr) {
        return;
    }
    afe_iface_->feed(afe_data_, data.data());
}

//========= 9、获取喂入音频数据大小 ======================
size_t WakeWordDetect::GetFeedSize() {
    if (afe_data_ == nullptr) {
        return 0;
    }
    return afe_iface_->get_feed_chunksize(afe_data_) * codec_->input_channels();
}

//========= 10、音频检测任务 ======================
void WakeWordDetect::AudioDetectionTask() {
    auto fetch_size = afe_iface_->get_fetch_chunksize(afe_data_);
    auto feed_size = afe_iface_->get_feed_chunksize(afe_data_);
    ESP_LOGI(TAG, "Audio detection task started, feed size: %d fetch size: %d",
        feed_size, fetch_size);

    while (true) {
        xEventGroupWaitBits(event_group_, DETECTION_RUNNING_EVENT, pdFALSE, pdTRUE, portMAX_DELAY);

        auto res = afe_iface_->fetch_with_delay(afe_data_, portMAX_DELAY);
        if (res == nullptr || res->ret_value == ESP_FAIL) {
            continue;;
        }

        // Store the wake word data for voice recognition, like who is speaking
        StoreWakeWordData((uint16_t*)res->data, res->data_size / sizeof(uint16_t));

        if (res->wakeup_state == WAKENET_DETECTED) {
            StopDetection();
            last_detected_wake_word_ = wake_words_[res->wake_word_index - 1];

            if (wake_word_detected_callback_) {
                wake_word_detected_callback_(last_detected_wake_word_);
            }
        }
    }
}

//========= 11、存储唤醒词数据 ======================
void WakeWordDetect::StoreWakeWordData(uint16_t* data, size_t samples) {
    // store audio data to wake_word_pcm_
    wake_word_pcm_.emplace_back(std::vector<int16_t>(data, data + samples));
    // keep about 2 seconds of data, detect duration is 32ms (sample_rate == 16000, chunksize == 512)
    while (wake_word_pcm_.size() > 2000 / 32) {
        wake_word_pcm_.pop_front();
    }
}

//========= 12、编码唤醒词数据 ======================
void WakeWordDetect::EncodeWakeWordData() {
    // 清空唤醒词数据
    wake_word_opus_.clear();
    // 如果唤醒词编码任务栈为空，则分配内存
    if (wake_word_encode_task_stack_ == nullptr) {
        wake_word_encode_task_stack_ = (StackType_t*)heap_caps_malloc(4096 * 8, MALLOC_CAP_SPIRAM);   // ******************************
        //wake_word_encode_task_stack_ = (StackType_t*)heap_caps_malloc(4096 * 4, MALLOC_CAP_INTERNAL);
    }
    // 创建唤醒词编码任务
    wake_word_encode_task_ = xTaskCreateStatic([](void* arg) {
        auto this_ = (WakeWordDetect*)arg;  // 获取当前对象
        {
            auto start_time = esp_timer_get_time();  // 获取开始时间
            auto encoder = std::make_unique<OpusEncoderWrapper>(16000, 1, OPUS_FRAME_DURATION_MS);  // 创建Opus编码器
            encoder->SetComplexity(0); // 0 is the fastest  // 设置编码复杂度

            for (auto& pcm: this_->wake_word_pcm_) {  // 遍历唤醒词数据
                encoder->Encode(std::move(pcm), [this_](std::vector<uint8_t>&& opus) {  // 编码唤醒词数据
                    std::lock_guard<std::mutex> lock(this_->wake_word_mutex_);  // 锁定唤醒词互斥锁
                    this_->wake_word_opus_.emplace_back(std::move(opus));  // 添加编码后的唤醒词数据
                    this_->wake_word_cv_.notify_all();  // 通知唤醒词条件变量
                });
            }
            this_->wake_word_pcm_.clear();  // 清空唤醒词数据

            auto end_time = esp_timer_get_time();  // 获取结束时间
            ESP_LOGI(TAG, "Encode wake word opus %zu packets in %lld ms",
                this_->wake_word_opus_.size(), (end_time - start_time) / 1000);  // 打印编码后的唤醒词数据大小和时间

            std::lock_guard<std::mutex> lock(this_->wake_word_mutex_);  // 锁定唤醒词互斥锁
            this_->wake_word_opus_.push_back(std::vector<uint8_t>());  // 添加编码后的唤醒词数据
            this_->wake_word_cv_.notify_all();  // 通知唤醒词条件变量
        }
        vTaskDelete(NULL);  // 删除任务
    }, "encode_detect_packets", 4096 * 8, this, 2, wake_word_encode_task_stack_, &wake_word_encode_task_buffer_);  // 创建唤醒词编码任务
}

//========= 13、获取唤醒词数据 ======================
bool WakeWordDetect::GetWakeWordOpus(std::vector<uint8_t>& opus) {
    std::unique_lock<std::mutex> lock(wake_word_mutex_);
    wake_word_cv_.wait(lock, [this]() {
        return !wake_word_opus_.empty();
    });
    opus.swap(wake_word_opus_.front());
    wake_word_opus_.pop_front();
    return !opus.empty();
}
