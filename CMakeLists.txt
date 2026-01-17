file(GLOB c_sources "*.c")
file(GLOB cpp_sources "*.cpp")

idf_component_register(
    SRCS ${c_sources} ${cpp_sources}
    INCLUDE_DIRS "include"
    REQUIRES esp_event esp_timer
)