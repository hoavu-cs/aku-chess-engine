#pragma once

#include "chess.hpp"
#include "evaluation.hpp"

using namespace chess;

const int PAWN_VALUE = 120;
const int KNIGHT_VALUE = 320;
const int BISHOP_VALUE = 330;
const int ROOK_VALUE = 500;
const int QUEEN_VALUE = 900;
const int KING_VALUE = 5000;

/*-------------------
    Helper Functions
-------------------*/

void bitBoardVisualize(const Bitboard& board);

int gamePhase(const Board& board);

int materialImbalance(const Board& board);

bool isPassedPawn(int sqIndex, Color color, const Bitboard& theirPawns);

int manhattanDistance(const Square& sq1, const Square& sq2);

int minDistance(const Square& sq, const Square& sq2);

int mopUpScore(const Board& board);
