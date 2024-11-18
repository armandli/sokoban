#ifndef SOKOBAN_BOARD
#define SOKOBAN_BOARD

#include <type_alias.h>
#include <game_base.h>
#include <sokoban_state.h>

#include <cassert>
#include <stdexcept>
#include <format>
#include <vector>
#include <filesystem>
#include <fstream>

namespace s = std;
namespace fs = std::filesystem;

namespace sokoban {


using History = s::vector<BoardState>;

class SokobanBoard {
  uset<Pt>   mSpace;
  uset<Pt>   mWalls;
  uset<Pt>   mGoals;
  BoardState mState;
  Pt         mMaxPt;
  History&   mHistory;
public:
  SokobanBoard(const fs::path& bc_path, History& record): mMaxPt(0,0), mHistory(record) {
    s::ifstream source_file(bc_path.c_str());
    if (not source_file.is_open()) throw s::runtime_error(s::format("cannot open file {}", bc_path.native()));

    s::string line;
    sint row_id = 0;
    while (getline(source_file, line)){
      sint col_id = 0;
      for (char c : line){
        switch (c){
        // empty space
        break; case '.': {
          mSpace.insert(Pt(row_id, col_id));
        }
        // wall
        break; case 'W': {
          mWalls.insert(Pt(row_id, col_id));
        }
        // player
        break; case 'P': {
          mSpace.insert(Pt(row_id, col_id));
          mState.player = Pt(row_id, col_id);
        }
        // goal
        break; case 'G': {
          mSpace.insert(Pt(row_id, col_id));
          mGoals.insert(Pt(row_id, col_id));
        }
        // box
        break; case 'B': {
          mSpace.insert(Pt(row_id, col_id));
          mState.boxes.insert(Pt(row_id, col_id));
        }
        // player on goal
        break; case 'Q': {
          mSpace.insert(Pt(row_id, col_id));
          mState.player = Pt(row_id, col_id);
          mGoals.insert(Pt(row_id, col_id));
        }
        // box on goal
        break; case 'C': {
          mSpace.insert(Pt(row_id, col_id));
          mState.boxes.insert(Pt(row_id, col_id));
          mGoals.insert(Pt(row_id, col_id));
        }
        // not defined
        break; case ' ':;
        break; default: assert(false);
        }

        col_id++;
      }
      mMaxPt.y = s::max(col_id, mMaxPt.y);
      row_id++;
    }
    mMaxPt.x = row_id;
  }

  Pt size() const {
    return mMaxPt;
  }

  bool play(const Dr& dr){
    Pt np = mState.player + dr;
    if (mSpace.find(np) == mSpace.end() or mWalls.find(np) != mWalls.end())
      return false;
    if (mState.boxes.find(np) == mState.boxes.end()){
      mHistory.push_back(mState);
      mState.player = np;
      return true;
    } else {
      Pt nnp = np + dr;
      if (mSpace.find(nnp) == mSpace.end() or mWalls.find(nnp) != mWalls.end() or mState.boxes.find(nnp) != mState.boxes.end())
        return false;
      mHistory.push_back(mState);
      mState.player = np;
      mState.boxes.erase(np);
      mState.boxes.insert(nnp);
      return true;
    }
  }

  bool is_terminal() const {
    for (const Pt& pt : mGoals)
      if (mState.boxes.find(pt) == mState.boxes.end())
        return false;
    return true;
  }

  void backtrack(int steps){
    assert(steps >= 0 and steps < mHistory.size());

    if (steps == 0) return;

    mState = mHistory[mHistory.size() - steps];
    for (int i = 0; i < steps; ++i)
      mHistory.pop_back();
  }

  void set_state(const BoardState& st){
    mState = st;
  }

  void print(s::ostream& out) const {
    for (sint r = 0; r < mMaxPt.x; ++r){
      for (sint c = 0; c < mMaxPt.y; ++c){
        Pt pt(r, c);
        if (mWalls.find(pt) != mWalls.end())                    out << 'W';
        else if (mGoals.find(pt) != mGoals.end()){
          if (mState.player == pt)                              out << 'Q';
          else if (mState.boxes.find(pt) != mState.boxes.end()) out << 'C';
          else                                                  out << 'G';
        } else {
          if (mState.player == pt)                              out << 'P';
          else if (mState.boxes.find(pt) != mState.boxes.end()) out << 'B';
          else if (mSpace.find(pt) != mSpace.end())             out << '.';
          else                                                  out << ' ';
        }
      }
      out << '\n';
    }
  }
};

} // sokoban

#endif//SOKOBAN_BOARD
