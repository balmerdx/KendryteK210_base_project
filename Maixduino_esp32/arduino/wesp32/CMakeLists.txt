idf_component_register(SRCS "delay.c" "main.cpp" "WInterrupts.c" "wiring.c" "wiring_analog.c"
                    "wiring_digital.c" "WMath.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES esp32 nvs_flash
                    )
