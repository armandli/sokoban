# puzzle play
add_executable(puzzle_play puzzle_play.cpp)

set_property(TARGET puzzle_play PROPERTY CXX_STANDARD 20)

target_link_libraries(puzzle_play util)
target_link_libraries(puzzle_play game)
target_link_libraries(puzzle_play ${CURSES_LIBRARY})

# puzzle bfs search
add_executable(puzzle_bfs_search puzzle_bfs_search.cpp)

set_property(TARGET puzzle_bfs_search PROPERTY CXX_STANDARD 20)

target_link_libraries(puzzle_bfs_search util)
target_link_libraries(puzzle_bfs_search game)

# puzzle search
add_executable(puzzle_search puzzle_search.cpp)

set_property(TARGET puzzle_search PROPERTY CXX_STANDARD 20)

target_link_libraries(puzzle_search util)
target_link_libraries(puzzle_search game)
