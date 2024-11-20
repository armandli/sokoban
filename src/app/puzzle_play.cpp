#include <type_alias.h>
#include <game_base.h>
#include <sokoban_board.h>

#include <filesystem>
#include <format>

#include <sstream>
#include <iostream>

#include <thread>
#include <chrono>

#include <curses.h>

namespace s = std;
namespace fs = std::filesystem;
namespace g = sokoban;

int main(int argc, char* argv[]){
  if (argc != 2){
    s::cout << s::format("expect {} <puzzle_file_path>", argv[0]) << s::endl;
    return 1;
  }

  s::cout << "press any key to start" << s::endl;

  s::this_thread::sleep_for(s::chrono::milliseconds(1000));

  fs::path source(argv[1]);
  g::SokobanBoard game(source);

  initscr(); // initialize screen
  cbreak();  // disabling of buffering of typed characters
  noecho();  // no automatic echoing

  g::Pt game_size = game.size();
  WINDOW* win = newwin(game_size.x+2, game_size.y+2, 0, 0);

  int input = getch();

  wborder(win, 0, 0, 0, 0, 0, 0, 0, 0);

  while (not game.is_terminal()){
    s::stringstream outstream;
    game.print(outstream);
    
    int row_id = 1;
    wmove(win, row_id, 1);
    for (char c : outstream.str()){
      switch (c){
      break; case '\n':
        row_id++;
        wmove(win, row_id, 1);
      break; default:
        waddch(win, c);
      }
    }
    wrefresh(win);

    do {
      input = getch();
    } while (input != 'w' and input != 's' and input != 'a' and input != 'd' and input != 'b');

    g::Dr action;
    bool is_rollback = false;
    switch (input){
    break; case 'w': action = g::NORTH;
    break; case 's': action = g::SOUTH;
    break; case 'a': action = g::WEST;
    break; case 'd': action = g::EAST;
    break; case 'b': is_rollback = true;
    break; default: assert(false);
    }

    if (is_rollback){
      game.backtrack(1);
    } else {
      game.play(action);
    }
  }

  delwin(win);

  s::cout << s::flush << "game complete" << s::endl;
}
