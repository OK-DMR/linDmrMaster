cmake_minimum_required (VERSION 3.13.4)
project(linDMR)

file(GLOB linDMR_sources "*.h" "*.c")

add_executable(linDMR ${linDMR_sources} users.h scheduler.h dmr.h hyteraDecode.h)

target_link_libraries(linDMR LINK_PUBLIC m pthread sqlite3 curl)