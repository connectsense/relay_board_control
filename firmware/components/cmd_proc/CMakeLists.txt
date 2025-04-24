idf_component_register(
  SRCS cmd_proc.c
  INCLUDE_DIRS include
  REQUIRES json test_comm
  PRIV_REQUIRES esp_timer
)
