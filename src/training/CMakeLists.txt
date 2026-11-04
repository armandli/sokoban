add_library(training data_generator.cpp)

target_include_directories(training PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(training PUBLIC util)
target_link_libraries(training PUBLIC game)
set_property(TARGET training PROPERTY CXX_STANDARD 20)
