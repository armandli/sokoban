#include <type_alias.h>
#include <game_base.h>
#include <sokoban_board.h>

#include <filesystem>
#include <format>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>

#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

namespace s = std;
namespace fs = std::filesystem;
namespace g = sokoban;
using namespace ftxui;

static Element render_board(g::SokobanBoard& game) {
  s::stringstream ss;
  game.print(ss);

  s::vector<Element> rows;
  s::string line;
  while (s::getline(ss, line)) {
    s::vector<Element> cells;
    for (char c : line) {
      Element cell;
      switch (c) {
      case 'W': cell = text("W") | color(Color::GrayLight);            break;
      case '.': cell = text(".");                                       break;
      case 'P': cell = text("P") | color(Color::Green)  | bold;        break;
      case 'G': cell = text("G") | color(Color::Yellow);               break;
      case 'B': cell = text("B") | color(Color::Blue);                 break;
      case 'Q': cell = text("Q") | color(Color::GreenLight) | bold;    break;
      case 'C': cell = text("C") | color(Color::Green);                break;
      default:  cell = text(" ");                                       break;
      }
      cells.push_back(cell);
    }
    rows.push_back(hbox(cells));
  }
  return vbox(rows);
}

int main(int argc, char* argv[]){
  if (argc != 2){
    s::cout << s::format("expect {} <puzzle_file_path>", argv[0]) << s::endl;
    return 1;
  }

  fs::path source(argv[1]);
  g::SokobanBoard game(source);
  int move_count = 0;

  auto screen = App::Fullscreen();

  auto component = Renderer([&] {
    if (game.is_terminal()) {
      return vbox({
        text("Sokoban") | bold | center,
        separator(),
        render_board(game) | border | center,
        separator(),
        text("Puzzle solved!") | bold | color(Color::Green) | center,
        text("Press Q to exit") | dim | center,
      });
    }
    return vbox({
      text("Sokoban") | bold | center,
      separator(),
      render_board(game) | border | center,
      separator(),
      text("WASD / arrows: move   B: undo   Q: quit") | dim | center,
    });
  });

  component |= CatchEvent([&](Event event) -> bool {
    if (event == Event::Character('q') || event == Event::Character('Q')) {
      screen.ExitLoopClosure()();
      return true;
    }
    if (game.is_terminal()) return false;

    if (event == Event::Character('w') || event == Event::ArrowUp) {
      if (game.play(g::NORTH)) move_count++;
      return true;
    }
    if (event == Event::Character('s') || event == Event::ArrowDown) {
      if (game.play(g::SOUTH)) move_count++;
      return true;
    }
    if (event == Event::Character('a') || event == Event::ArrowLeft) {
      if (game.play(g::WEST)) move_count++;
      return true;
    }
    if (event == Event::Character('d') || event == Event::ArrowRight) {
      if (game.play(g::EAST)) move_count++;
      return true;
    }
    if (event == Event::Character('b') && move_count > 0) {
      game.backtrack(1);
      move_count--;
      return true;
    }
    return false;
  });

  screen.Loop(component);
  return 0;
}
