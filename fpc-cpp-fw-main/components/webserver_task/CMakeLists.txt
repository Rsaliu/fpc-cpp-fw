idf_component_register(
    SRCS "src/webserver_task.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common webserver freertos
)
