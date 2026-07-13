idf_component_register(
    SRCS "src/ota_serialization.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common nlohmann_json
)
