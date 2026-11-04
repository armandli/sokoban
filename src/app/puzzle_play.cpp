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

namespace s  = std;
namespace sf = std::filesystem;
namespace so = sokoban;
namespace ft = ftxui;

static ft::Element render_board(so::SokobanBoard& game) {
  s::stringstream ss;
  game.print(ss);

  s::vector<ft::Element> rows;
  s::string line;
  while (s::getline(ss, line)) {
    s::vector<ft::Element> cells;
    for (char c : line) {
      ft::Element cell;
      switch (c) {
      case 'W': cell = ft::text("W") | ft::color(ft::Color::GrayLight);

      break; case '.': cell = ft::text(".");

      break; case 'P':
        cell = ft::text("P") | ft::color(ft::Color::Green) | ft::bold;

      break; case 'G': cell = ft::text("G") | ft::color(ft::Color::Yellow);

      break; case 'B': cell = ft::text("B") | ft::color(ft::Color::Blue);

      break; case 'Q':
        cell = ft::text("Q") | ft::color(ft::Color::GreenLight) | ft::bold;

      break; case 'C': cell = ft::text("C") | ft::color(ft::Color::Green);

      default: cell = ft::text(" ");
      }
      cells.push_back(cell);
    }
    rows.push_back(ft::hbox(cells));
  }
  return ft::vbox(rows);
}

int main(int argc, char* argv[]){
  if (argc != 2){
    s::cout << s::format("expect {} <puzzle_file_path>", argv[0]) << s::endl;
    return 1;
  }

  sf::path source(argv[1]);
  so::SokobanBoard game(source);
  int move_count = 0;

  auto screen = ft::App::Fullscreen();

  auto component = ft::Renderer([&] {
    if (game.is_terminal()) {
      return ft::vbox({
        ft::text("Sokoban") | ft::bold | ft::center,
        ft::separator(),
        render_board(game) | ft::border | ft::center,
        ft::separator(),
        ft::text("Puzzle solved!")
          | ft::bold | ft::color(ft::Color::Green) | ft::center,
        ft::text("Press Q to exit") | ft::dim | ft::center,
      });
    }
    return ft::vbox({
      ft::text("Sokoban") | ft::bold | ft::center,
      ft::separator(),
      render_board(game) | ft::border | ft::center,
      ft::separator(),
      ft::text("WASD / arrows: move   B: undo   Q: quit") | ft::dim | ft::center,
    });
  });

  component |= ft::CatchEvent([&](ft::Event event) -> bool {
    if (event == ft::Event::Character('q') or
        event == ft::Event::Character('Q')) {
      screen.ExitLoopClosure()();
      return true;
    }
    if (game.is_terminal()) return false;

    if (event == ft::Event::Character('w') or event == ft::Event::ArrowUp) {
      if (game.play(so::NORTH)) move_count++;
      return true;
    }
    if (event == ft::Event::Character('s') or event == ft::Event::ArrowDown) {
      if (game.play(so::SOUTH)) move_count++;
      return true;
    }
    if (event == ft::Event::Character('a') or event == ft::Event::ArrowLeft) {
      if (game.play(so::WEST)) move_count++;
      return true;
    }
    if (event == ft::Event::Character('d') or event == ft::Event::ArrowRight) {
      if (game.play(so::EAST)) move_count++;
      return true;
    }
    if (event == ft::Event::Character('b') and move_count > 0) {
      game.backtrack(1);
      move_count--;
      return true;
    }
    return false;
  });

  screen.Loop(component);
  return 0;
}
