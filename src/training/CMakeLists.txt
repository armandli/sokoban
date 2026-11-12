# header-only: level loading/splitting and batched search
add_library(training INTERFACE)

target_include_directories(training INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(training INTERFACE util)
target_link_libraries(training INTERFACE game)
