idf_component_register(
    SRCS "main.cpp" "wifi.cpp" "feed.cpp" "json_stream.cpp"
         "display.cpp" "gfx.cpp" "radar.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_wifi esp_netif esp_event nvs_flash esp_http_client
                  mbedtls esp_lcd esp_driver_spi esp_driver_gpio esp_timer)
