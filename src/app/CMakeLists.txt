# puzzle play
add_executable(puzzle_play puzzle_play.cpp)

set_property(TARGET puzzle_play PROPERTY CXX_STANDARD 23)

target_link_libraries(puzzle_play util)
target_link_libraries(puzzle_play game)
target_link_libraries(puzzle_play ${CURSES_LIBRARY})

# puzzle bfs search
add_executable(puzzle_bfs_search puzzle_bfs_search.cpp)

set_property(TARGET puzzle_bfs_search PROPERTY CXX_STANDARD 23)

target_link_libraries(puzzle_bfs_search util)
target_link_libraries(puzzle_bfs_search game)

# puzzle bfs search 2
add_executable(puzzle_bfs_search2 puzzle_bfs_search2.cpp)

set_property(TARGET puzzle_bfs_search2 PROPERTY CXX_STANDARD 23)

target_link_libraries(puzzle_bfs_search2 util)
target_link_libraries(puzzle_bfs_search2 game)

# puzzle a* search
add_executable(puzzle_astar_search puzzle_astar_search.cpp)

set_property(TARGET puzzle_astar_search PROPERTY CXX_STANDARD 23)

target_link_libraries(puzzle_astar_search util)
target_link_libraries(puzzle_astar_search game)

# puzzle a* search 2
add_executable(puzzle_astar_search2 puzzle_astar_search2.cpp)

set_property(TARGET puzzle_astar_search2 PROPERTY CXX_STANDARD 23)

target_link_libraries(puzzle_astar_search2 util)
target_link_libraries(puzzle_astar_search2 game)
