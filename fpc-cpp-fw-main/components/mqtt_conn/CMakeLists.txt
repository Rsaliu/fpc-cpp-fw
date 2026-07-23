idf_component_register(
    SRCS "src/mqtt_conn.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common mqtt freertos
)
