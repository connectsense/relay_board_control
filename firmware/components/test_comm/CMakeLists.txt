idf_component_register(
  SRCS test_comm.c
  INCLUDE_DIRS include
  REQUIRES esp_driver_uart
  PRIV_REQUIRES esp_timer json watchdog
)
