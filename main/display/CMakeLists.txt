idf_component_register(
    SRCS
        "lcd_display.cc"
        "oled_display.cc"
        "ssd1309_display.cc"
        "spi_lcd_display.cc"
        "rgb_lcd_display.cc"
        "mipi_lcd_display.cc"
        "no_display.cc"
    INCLUDE_DIRS
        "."
    REQUIRES
        esp_lcd
        lvgl
        esp_lvgl_port
) 