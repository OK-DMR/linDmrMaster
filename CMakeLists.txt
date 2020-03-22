cmake_minimum_required(VERSION 3.13.4)
project(linDMR)

file(GLOB linDMR_sources "*.h" "*.c")
file(GLOB voiceFiles "*.voice")
file(GLOB rdacIn "rdac.in")

file(COPY ${rdacIn} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${voiceFiles} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_executable(linDMR ${linDMR_sources})

target_link_libraries(linDMR LINK_PUBLIC m pthread sqlite3 curl)