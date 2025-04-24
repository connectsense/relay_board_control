# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS watchdog.c
    INCLUDE_DIRS include
    PRIV_INCLUDE_DIRS
    REQUIRES
    PRIV_REQUIRES esp_timer
)
