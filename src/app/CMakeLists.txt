# puzzle play
add_executable(puzzle_play puzzle_play.cpp)

set_property(TARGET puzzle_play PROPERTY CXX_STANDARD 26)

target_link_libraries(puzzle_play util)
target_link_libraries(puzzle_play game)
target_link_libraries(puzzle_play ftxui::screen ftxui::dom ftxui::component)

# puzzle bfs search
add_executable(puzzle_bfs_search puzzle_bfs_search.cpp)

set_property(TARGET puzzle_bfs_search PROPERTY CXX_STANDARD 26)

target_link_libraries(puzzle_bfs_search util)
target_link_libraries(puzzle_bfs_search game)

# puzzle bfs search 2
add_executable(puzzle_bfs_search2 puzzle_bfs_search2.cpp)

set_property(TARGET puzzle_bfs_search2 PROPERTY CXX_STANDARD 26)

target_link_libraries(puzzle_bfs_search2 util)
target_link_libraries(puzzle_bfs_search2 game)

# puzzle a* search
add_executable(puzzle_astar_search puzzle_astar_search.cpp)

set_property(TARGET puzzle_astar_search PROPERTY CXX_STANDARD 26)

target_link_libraries(puzzle_astar_search util)
target_link_libraries(puzzle_astar_search game)

# puzzle a* search 2
add_executable(puzzle_astar_search2 puzzle_astar_search2.cpp)

set_property(TARGET puzzle_astar_search2 PROPERTY CXX_STANDARD 26)

target_link_libraries(puzzle_astar_search2 util)
target_link_libraries(puzzle_astar_search2 game)

# neural network training
add_executable(puzzle_train puzzle_train.cpp)

set_property(TARGET puzzle_train PROPERTY CXX_STANDARD 20)

target_include_directories(puzzle_train SYSTEM PRIVATE ${TORCH_INCLUDE_DIRS})
target_link_libraries(puzzle_train model)
target_link_libraries(puzzle_train training)
target_link_libraries(puzzle_train ${TORCH_LIBRARIES})
target_compile_options(puzzle_train PRIVATE ${TORCH_CXX_FLAGS})

# neural heuristic a* search
add_executable(puzzle_astar_nn puzzle_astar_nn.cpp)

set_property(TARGET puzzle_astar_nn PROPERTY CXX_STANDARD 20)

target_include_directories(puzzle_astar_nn SYSTEM PRIVATE ${TORCH_INCLUDE_DIRS})
target_link_libraries(puzzle_astar_nn model)
target_link_libraries(puzzle_astar_nn ${TORCH_LIBRARIES})
target_compile_options(puzzle_astar_nn PRIVATE ${TORCH_CXX_FLAGS})
