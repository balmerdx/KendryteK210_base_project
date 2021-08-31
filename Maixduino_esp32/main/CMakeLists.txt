
idf_component_register(SRCS "sketch.ino.cpp" "CommandHandler.cpp"
                    INCLUDE_DIRS "../arduino/wesp32"
                    REQUIRES bt wesp32 SPIS WiFi esp32 mbedtls wpa_supplicant nvs_flash
                    )
