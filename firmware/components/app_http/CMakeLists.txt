idf_component_register(
  SRCS tf_http.c http_cmd.c
  INCLUDE_DIRS include
  PRIV_REQUIRES esp_http_client json mbedtls cmd_proc
)
