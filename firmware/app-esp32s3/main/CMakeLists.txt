# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS main.c version.c gpio_cmd.c nvs_cmd.c
    INCLUDE_DIRS include
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES esp_wifi esp_http_client
    PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_timer nvs_flash test_comm cmd_proc app_wifi app_http
)
