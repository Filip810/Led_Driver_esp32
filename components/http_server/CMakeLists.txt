idf_component_register(
    SRCS "http_server.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server led_control json log
    EMBED_TXTFILES "web/index.html"
)
