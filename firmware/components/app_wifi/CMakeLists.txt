# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS wifi_ctrl.c wifi_scan.c wifi_cmd.c
    INCLUDE_DIRS include
    PRIV_INCLUDE_DIRS
    REQUIRES cmd_proc esp_wifi
    PRIV_REQUIRES
)
