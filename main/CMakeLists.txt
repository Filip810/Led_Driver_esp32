idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES led_control wifi_manager http_server log
)
