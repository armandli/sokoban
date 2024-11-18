# puzzle play
add_executable(puzzle_play puzzle_play.cpp)

set_property(TARGET puzzle_play PROPERTY CXX_STANDARD 20)

target_link_libraries(puzzle_play util)
target_link_libraries(puzzle_play game)
target_link_libraries(puzzle_play ${CURSES_LIBRARY})
