#include "boards/common/board.h"
#include "TFT_eSPI.h"

Board& Board::GetInstance()
{
    static Board* instance = static_cast<Board*>(create_board());
    return *instance;
}

bool Board::RefreshDisplay()
{
    GetDisplay().update();
    return true;
}
