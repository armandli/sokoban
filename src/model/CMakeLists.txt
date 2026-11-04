add_library(model sokoban_model.cpp)

target_include_directories(model PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(model SYSTEM PRIVATE ${TORCH_INCLUDE_DIRS})
target_link_libraries(model PUBLIC  util)
target_link_libraries(model PUBLIC  game)
target_link_libraries(model PUBLIC  ${TORCH_LIBRARIES})
target_compile_options(model PRIVATE ${TORCH_CXX_FLAGS})
set_property(TARGET model PROPERTY CXX_STANDARD 20)
