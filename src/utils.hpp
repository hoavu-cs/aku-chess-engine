#pragma once

#include "chess.hpp"
#include "evaluation.hpp"

using namespace chess;

int gamePhase(const Board& board);

void bitBoardVisualize(const Bitboard& board);

Bitboard generateFileMask(int file);

constexpr Bitboard generateHalfMask(int startRank, int endRank);

bool isPassedPawn(int sqIndex, Color color, const Bitboard& theirPawns);

int manhattanDistance(const Square& sq1, const Square& sq2);

int minDistance(const Square& sq, const Square& sq2);

int mopUpScore(const Board& board);
