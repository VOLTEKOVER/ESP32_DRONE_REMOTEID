idf_component_register(
    SRCS 
        "id_open_esp32.cpp"
        "id_open.cpp"
        "id_open_beacon.cpp"
        "alt_unix_time.c"
    INCLUDE_DIRS 
        "."
        "${CMAKE_CURRENT_SOURCE_DIR}/../../id_open"
    PUBLIC_INCLUDE_DIRS 
        "."
)

# Link C++ standard library
target_link_libraries(${COMPONENT_LIB} PUBLIC stdc++)
