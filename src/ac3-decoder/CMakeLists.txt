# Set the target name
set(TARGET_NAME ac3-decoder)

# Add source files
file(GLOB AC3_DECODER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ac3rf-lib/ac3/*.cpp
)

add_executable(${TARGET_NAME} ${AC3_DECODER_SOURCES})

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ac3rf-lib
    ${CMAKE_CURRENT_SOURCE_DIR}/ac3rf-lib/ac3
    ${CMAKE_CURRENT_SOURCE_DIR}/ac3rf-lib/rs
)

install(TARGETS ${TARGET_NAME})
