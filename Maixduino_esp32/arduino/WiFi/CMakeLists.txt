idf_component_register(SRCS "WiFi.cpp" "WiFiClient.cpp" "WiFiServer.cpp" "WiFiSSLClient.cpp" "WiFiUdp.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES esp32 wesp32
                    )
