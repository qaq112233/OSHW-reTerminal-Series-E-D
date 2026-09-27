// Native host contract for the PhotoPainter cloud/display profile.
// c++ -std=c++11 -Isrc tests/photopainter_registry.cpp -o /tmp/photopainter_registry
#define BOARD_WAVESHARE_PHOTOPAINTER
#define BOARD_SCREEN_COMBO 521
#include "boards/board_registry.h"
#include <cassert>
#include <cstring>

int main() {
    const auto& board = board_registry::current_board();
    const auto& screen = board_registry::current_board_screen();
    assert(board.model == board_registry::BoardModel::PhotoPainter);
    assert(screen.model == board_registry::BoardModel::PhotoPainter);
    assert(screen.combo_id == 521);
    assert(screen.width == 800 && screen.height == 480);
    assert(screen.color == board_registry::ScreenColor::Chromatic);
    assert(std::strcmp(screen.board_info_type, "xiao_diy_ee04") == 0);
    assert(std::strcmp(screen.board_info_screen_type, "7_3_color_800_480") == 0);
    assert(std::strcmp(screen.resolution, "800x480") == 0);
}
