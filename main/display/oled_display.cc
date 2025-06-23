#include "oled_display.h"
#include "font_awesome_symbols.h"
#include "assets/lang_config.h"

#include <string>
#include <algorithm>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>

#define TAG "OledDisplay"

LV_FONT_DECLARE(font_awesome_30_1);

OledDisplay::OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
    int width, int height, bool mirror_x, bool mirror_y, DisplayFonts fonts)
    : panel_io_(panel_io), panel_(panel), fonts_(fonts) {
    width_ = width;
    height_ = height;

    ESP_LOGI(TAG, "Initialize LVGL");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * height_),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (height_ == 64) {
        SetupUI_128x64();
        //ESP_LOGE(TAG, "*********SetupUI_128x64()********************");
    } else {
        SetupUI_128x32();
        //ESP_LOGE(TAG, "*********SetupUI_128x32()********************");
    }
}

OledDisplay::~OledDisplay() {
    if (content_ != nullptr) {
        lv_obj_del(content_);
    }
    if (status_bar_ != nullptr) {
        lv_obj_del(status_bar_);
    }
    if (side_bar_ != nullptr) {
        lv_obj_del(side_bar_);
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
    lvgl_port_deinit();
}

bool OledDisplay::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void OledDisplay::Unlock() {
    lvgl_port_unlock();
}

void OledDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }

    // Replace all newlines with spaces
    std::string content_str = content;
    std::replace(content_str.begin(), content_str.end(), '\n', ' ');

    if (content_right_ == nullptr) {
        lv_label_set_text(chat_message_label_, content_str.c_str());
    } else {
        if (content == nullptr || content[0] == '\0') {
            lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(chat_message_label_, content_str.c_str());
            lv_obj_clear_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void OledDisplay::SetupUI_128x64() {
    DisplayLockGuard lock(this);

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, fonts_.text_font, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);

    /* Status bar */
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, 16);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_radius(status_bar_, 0, 0);

    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_flex_main_place(content_, LV_FLEX_ALIGN_CENTER, 0);

    // 创建左侧固定宽度的容器
    content_left_ = lv_obj_create(content_);
    lv_obj_set_size(content_left_, 32, LV_SIZE_CONTENT);  // 固定宽度32像素
    lv_obj_set_style_pad_all(content_left_, 0, 0);
    lv_obj_set_style_border_width(content_left_, 0, 0);

    emotion_label_ = lv_label_create(content_left_);
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_1, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);
    lv_obj_center(emotion_label_);
    lv_obj_set_style_pad_top(emotion_label_, 8, 0);

    // 创建右侧可扩展的容器
    content_right_ = lv_obj_create(content_);
    lv_obj_set_size(content_right_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(content_right_, 0, 0);
    lv_obj_set_style_border_width(content_right_, 0, 0);
    lv_obj_set_flex_grow(content_right_, 1);
    lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);

    chat_message_label_ = lv_label_create(content_right_);
    lv_label_set_text(chat_message_label_, "");
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(chat_message_label_, width_ - 32);
    lv_obj_set_style_pad_top(chat_message_label_, 14, 0);

    // 延迟一定的时间后开始滚动字幕
    static lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_delay(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);

    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);

    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, fonts_.text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(low_battery_popup_, lv_color_black(), 0);
    lv_obj_set_style_radius(low_battery_popup_, 10, 0);
    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief 设置 128x32 分辨率 OLED 显示器的 UI 布局
 * 该函数创建并配置了一个适合 128x32 分辨率 OLED 显示器的用户界面
 * 界面包含左侧图标区域和右侧状态栏及消息区域
 */
void OledDisplay::SetupUI_128x32() {
    DisplayLockGuard lock(this);                            // 获取显示锁，确保线程安全

    auto screen = lv_screen_active();                       // 获取活动屏幕对象
    lv_obj_set_style_text_font(screen, fonts_.text_font, 0);    // 设置屏幕默认字体

    /* 创建主容器 */
    container_ = lv_obj_create(screen);
    
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);    // 设置容器大小为屏幕大小
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);     // 设置容器为水平流式布局
    lv_obj_set_style_pad_all(container_, 0, 0);             // 设置容器内边距为0
    lv_obj_set_style_border_width(container_, 0, 0);        // 设置容器边框宽度为0
    lv_obj_set_style_pad_column(container_, 0, 0);          // 设置容器列间距为0

    /* 创建左侧图标区域 */
    content_ = lv_obj_create(container_);

    lv_obj_set_size(content_, 32, 32);                      // 设置图标区域大小为32x32像素
    lv_obj_set_style_pad_all(content_, 0, 0);               // 设置图标区域内边距为0
    lv_obj_set_style_border_width(content_, 0, 0);          // 设置图标区域边框宽度为0
    lv_obj_set_style_radius(content_, 0, 0);                // 设置图标区域圆角为0

    emotion_label_ = lv_label_create(content_);             // 创建表情标签
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_1, 0);    // 设置表情标签字体为30像素的图标字体
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);             // 设置表情标签文本为AI芯片图标
    lv_obj_center(emotion_label_);                          // 将表情标签居中显示

    /* 创建右侧区域 */
    side_bar_ = lv_obj_create(container_);

    lv_obj_set_size(side_bar_, width_ - 32, 32);            // 设置右侧区域宽度为屏幕宽度减去32像素
    lv_obj_set_flex_flow(side_bar_, LV_FLEX_FLOW_COLUMN);   // 设置右侧区域为垂直流式布局
    lv_obj_set_style_pad_all(side_bar_, 0, 0);              // 设置右侧区域内边距为0
    lv_obj_set_style_border_width(side_bar_, 0, 0);         // 设置右侧区域边框宽度为0
    lv_obj_set_style_radius(side_bar_, 0, 0);               // 设置右侧区域圆角为0
    lv_obj_set_style_pad_row(side_bar_, 0, 0);              // 设置右侧区域行间距为0

    /* 创建状态栏 */
    status_bar_ = lv_obj_create(side_bar_);
    
    lv_obj_set_size(status_bar_, width_ - 32, 16);          // 设置状态栏大小为右侧区域宽度x16像素
    lv_obj_set_style_radius(status_bar_, 0, 0);             // 设置状态栏圆角为0
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);    // 设置状态栏为水平流式布局
    lv_obj_set_style_pad_all(status_bar_, 0, 0);            // 设置状态栏内边距为0
    lv_obj_set_style_border_width(status_bar_, 0, 0);       // 设置状态栏边框宽度为0
    lv_obj_set_style_pad_column(status_bar_, 0, 0);         // 设置状态栏列间距为0

    status_label_ = lv_label_create(status_bar_);           // 创建状态文本标签
    lv_obj_set_flex_grow(status_label_, 1);                 // 设置状态标签可以扩展
    lv_obj_set_style_pad_left(status_label_, 2, 0);         // 设置状态标签左内边距为2像素
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);    // 设置状态标签初始文本

    notification_label_ = lv_label_create(status_bar_);     // 创建通知标签
    lv_obj_set_flex_grow(notification_label_, 1);           // 设置通知标签可以扩展
    lv_obj_set_style_pad_left(notification_label_, 2, 0);   // 设置通知标签左内边距为2像素
    lv_label_set_text(notification_label_, "");             // 设置通知标签初始文本为空
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);    // 隐藏通知标签

    mute_label_ = lv_label_create(status_bar_);             // 创建静音标签
    lv_label_set_text(mute_label_, "");                     // 设置静音标签初始文本为空
    lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);    // 设置静音标签字体为图标字体

    network_label_ = lv_label_create(status_bar_);          // 创建网络标签
    lv_label_set_text(network_label_, "");                  // 设置网络标签初始文本为空
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);    // 设置网络标签字体为图标字体

    battery_label_ = lv_label_create(status_bar_);          // 创建电池标签
    lv_label_set_text(battery_label_, "");                  // 设置电池标签初始文本为空
    lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);    // 设置电池标签字体为图标字体

    chat_message_label_ = lv_label_create(side_bar_);       // 创建聊天消息标签
    lv_obj_set_size(chat_message_label_, width_ - 32, LV_SIZE_CONTENT);    // 设置聊天消息标签宽度为右侧区域宽度
    lv_obj_set_style_pad_left(chat_message_label_, 2, 0);   // 设置聊天消息标签左内边距为2像素
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);    // 设置聊天消息标签为循环滚动模式
    lv_label_set_text(chat_message_label_, "");             // 设置聊天消息标签初始文本为空

    /* 设置滚动动画 */
    static lv_anim_t a;
    lv_anim_init(&a);                                       // 初始化动画
    lv_anim_set_delay(&a, 1000);                           // 设置动画延迟为1000毫秒
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE); // 设置动画无限重复
    lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);    // 将动画应用到聊天消息标签
    lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);    // 设置动画持续时间，根据文本长度自动调整
}

