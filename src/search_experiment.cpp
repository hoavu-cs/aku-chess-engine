/*
* Author: Hoa T. Vu
* Created: December 1, 2024
* 
* Copyright (c) 2024 Hoa T. Vu
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/

#include "search.hpp"
#include "chess.hpp"
#include "utils.hpp"
#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include <omp.h> // Include OpenMP header
#include <chrono>
#include <stdlib.h>
#include <cmath>
#include <unordered_set>
#include <queue>
#include "../lib/stockfish_nnue_probe/probe.h"


using namespace chess;
using namespace Stockfish;

typedef std::uint64_t U64;


U64 trainingCount = 0;


/*-------------------------------------------------------------------------------------------- 
    Initialize the NNUE evaluation function.
--------------------------------------------------------------------------------------------*/
void initializeNNUE() {
    std::cout << "Initializing NNUE." << std::endl;

    Stockfish::Probe::init("nn-b1a57edbea57.nnue", "nn-b1a57edbea57.nnue");
}

/*-------------------------------------------------------------------------------------------- 
    Constants and global variables.
--------------------------------------------------------------------------------------------*/

// Transposition table 
const int maxTableSize = 13e6; // Maximum size of the transposition table

struct tableEntry {
    U64 hash;
    int eval;
    int depth;
    Move bestMove;
};

std::vector<tableEntry> ttTable(maxTableSize); 
std::vector<tableEntry> ttTableNonPV(maxTableSize); 

std::unordered_map<U64, U64> historyTable; // History heuristic table

std::chrono::time_point<std::chrono::high_resolution_clock> hardDeadline; // Search hardDeadline
std::chrono::time_point<std::chrono::high_resolution_clock> softDeadline;

U64 nodeCount; // Node count for each thread
U64 tableHit;
std::vector<Move> previousPV; // Principal variation from the previous iteration
std::vector<std::vector<Move>> killerMoves(1000); // Killer moves

int globalMaxDepth = 0; // Maximum depth of current search
const int ENGINE_DEPTH = 30; // Maximum search depth for the current engine version

// Basic piece values for move ordering, detection of sacrafices, etc.
const int pieceValues[] = {
    0,    // No piece
    100,  // Pawn
    320,  // Knight
    330,  // Bishop
    500,  // Rook
    900,  // Queen
    20000 // King
};

/*-------------------------------------------------------------------------------------------- 
    Transposition table lookup and insert.
--------------------------------------------------------------------------------------------*/
bool tableLookUp(Board& board, int& depth, int& eval, Move& bestMove, std::vector<tableEntry>& table) {    
    U64 hash = board.hash();
    U64 index = hash % maxTableSize;

    tableEntry entry = table[index];

    if (entry.hash == hash) {
        depth = entry.depth;
        eval = entry.eval;
        bestMove = entry.bestMove;
        return true;
    }

    return false;
}

void tableInsert(Board& board, int depth, int eval, Move bestMove, std::vector<tableEntry>& table) {
    U64 hash = board.hash();
    U64 index = hash % maxTableSize;

    tableEntry entry = {hash, eval, depth, bestMove};
    table[index] = entry;
}
 
/*-------------------------------------------------------------------------------------------- 
    Check if the move is a queen promotion.
--------------------------------------------------------------------------------------------*/
bool isPromotion(const Move& move) {
    if (move.typeOf() & Move::PROMOTION) {
        return true;
    } 
    return false;
}

/*-------------------------------------------------------------------------------------------- 
    Update the killer moves.
--------------------------------------------------------------------------------------------*/
void updateKillerMoves(const Move& move, int ply) {
    if (killerMoves[ply].size() < 2) {
        killerMoves[ply].push_back(move);
    } else {
        killerMoves[ply][1] = killerMoves[ply][0];
        killerMoves[ply][0] = move;
    }
}

/*-------------------------------------------------------------------------------------------- 
    Check if the move involves a passed pawn push.
--------------------------------------------------------------------------------------------*/
bool promotionThreatMove(Board& board, Move move) {
    Color color = board.sideToMove();
    PieceType type = board.at<Piece>(move.from()).type();

    if (type == PieceType::PAWN) {
        int destinationIndex = move.to().index();
        int rank = destinationIndex / 8;
        Bitboard theirPawns = board.pieces(PieceType::PAWN, !color);

        bool isPassedPawnFlag = isPassedPawn(destinationIndex, color, theirPawns);

        if (isPassedPawnFlag) {
            if ((color == Color::WHITE && rank > 3) || 
                (color == Color::BLACK && rank < 4)) {
                return true;
            }
        }
    }

    return false;
}

/*-------------------------------------------------------------------------------------------- 
  SEE (Static Exchange Evaluation) function.
 -------------------------------------------------------------------------------------------*/
int see(Board& board, Move move) {

    #pragma omp critical
    {
        nodeCount++;
    }

    int to = move.to().index();
    
    // Get victim and attacker piece values
    auto victim = board.at<Piece>(move.to());
    auto attacker = board.at<Piece>(move.from());
    
    int victimValue = pieceValues[static_cast<int>(victim.type())];
    int attackerValue = pieceValues[static_cast<int>(attacker.type())];

    // Material gain from the first capture
    int materialGain = victimValue - attackerValue;

    board.makeMove(move);
    Movelist subsequentCaptures;
    movegen::legalmoves<movegen::MoveGenType::CAPTURE>(subsequentCaptures, board);
    int maxSubsequentGain = 0;
    
    // Store attackers sorted by increasing value (weakest first)
    std::vector<Move> attackers;
    for (int i = 0; i < subsequentCaptures.size(); i++) {
        if (subsequentCaptures[i].to() == to) {
            attackers.push_back(subsequentCaptures[i]);
        }
    }

    // Sort attackers by piece value (weakest attacker moves first)
    std::sort(attackers.begin(), attackers.end(), [&](const Move& a, const Move& b) {
        return pieceValues[static_cast<int>(board.at<Piece>(a.from()).type())] < 
               pieceValues[static_cast<int>(board.at<Piece>(b.from()).type())];
    });

    // Recursively evaluate each attacker
    for (const Move& nextCapture : attackers) {
        maxSubsequentGain = -std::max(maxSubsequentGain, see(board, nextCapture));
    }

    // Undo the move before returning
    board.unmakeMove(move);
    return materialGain + maxSubsequentGain;
}

/*--------------------------------------------------------------------------------------------
    Late move reduction. 
--------------------------------------------------------------------------------------------*/
int lateMoveReduction(Board& board, Move move, int i, int depth, int ply, bool isPV, int quietCount, bool leftMost) {

    if (isMopUpPhase(board)) {
        // Search more thoroughly in mop-up phase
        return depth - 1;
    }

    if (i <= 2 || depth <= 2) { 
        return depth - 1;
    } else {
        int R = log (depth) + log (i);
        return depth - std::max(1, R);
    }
}

/*-------------------------------------------------------------------------------------------- 
    Returns a list of candidate moves ordered by priority.
--------------------------------------------------------------------------------------------*/
std::vector<std::pair<Move, int>> orderedMoves(
    Board& board, 
    int depth, 
    int ply,
    std::vector<Move>& previousPV, 
    bool leftMost) {

    Movelist moves;
    movegen::legalmoves(moves, board);

    std::vector<std::pair<Move, int>> candidatesPrimary;
    std::vector<std::pair<Move, int>> candidatesSecondary;

    candidatesPrimary.reserve(moves.size());
    candidatesSecondary.reserve(moves.size());

    bool whiteTurn = board.sideToMove() == Color::WHITE;
    Color color = board.sideToMove();
    U64 hash = board.hash();

    // Move tableMove;
    // Move tableMoveNonPV;
    // int tableEval;
    // int tableEvalNonPV;
    // int tableDepth;
    // int tableDepthNonPV;

    // #pragma omp critical
    // {
    //     Move tableMove;
    //     tableLookUp(board, tableDepth, tableEval, tableMove, ttTable);
    //     tableLookUp(board, tableDepthNonPV, tableEvalNonPV, tableMoveNonPV, ttTableNonPV);
    // }

    // Move ordering 1. promotion 2. captures 3. killer moves 4. hash 5. checks 6. quiet moves
    for (const auto& move : moves) {
        int priority = 0;
        bool secondary = false;
        int moveIndex = move.from().index() * 64 + move.to().index();
        int ply = globalMaxDepth - depth;
        bool hashMove = false;

        // Previous PV move > hash moves > captures/killer moves > checks > quiet moves
        #pragma omp critical
        {
            Move tableMove;
            int tableEval;
            int tableDepth;
            if (tableLookUp(board, tableDepth, tableEval, tableMove, ttTable)) {
                if (tableMove == move) {
                    tableHit++;
                    priority = 8000 + tableDepth;
                    candidatesPrimary.push_back({tableMove, priority});
                    hashMove = true;
                }
            } else if (tableLookUp(board, tableDepth, tableEval, tableMove, ttTableNonPV)) {
                if (tableMove == move) {
                    tableHit++;
                    priority = 7000 + tableDepth;
                    candidatesPrimary.push_back({tableMove, priority});
                    hashMove = true;
                }
            }
        }
      
        if (hashMove) continue;
        
        if (previousPV.size() > ply && leftMost) {
            if (previousPV[ply] == move) {
                priority = 10000; // PV move
            }
        } else if (std::find(killerMoves[ply].begin(), killerMoves[ply].end(), move) != killerMoves[ply].end()) {
            priority = 4000; // Killer moves
        } else if (isPromotion(move)) {
            priority = 6000; 
        } else if (board.isCapture(move)) { 
            int seeScore = see(board, move);
            priority = 4000 + seeScore;
        } else {
            board.makeMove(move);
            bool isCheck = board.inCheck();
            board.unmakeMove(move);

            if (isCheck) {
                priority = 4000;
            } else {
                secondary = true;
                U64 moveIndex = move.from().index() * 64 + move.to().index();
                #pragma omp critical
                {
                    if (historyTable.count(moveIndex)) {
                        priority = 1000 + historyTable[moveIndex];
                    } else {
                        priority = moveScoreByTable(board, move);
                    }
                                            
                }
            }
        } 

        if (!secondary) {
            candidatesPrimary.push_back({move, priority});
        } else {
            candidatesSecondary.push_back({move, priority});
        }
    }

    // Sort capture, promotion, checks by priority
    std::sort(candidatesPrimary.begin(), candidatesPrimary.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    std::sort(candidatesSecondary.begin(), candidatesSecondary.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    for (const auto& move : candidatesSecondary) {
        candidatesPrimary.push_back(move);
    }

    return candidatesPrimary;
}

/*-------------------------------------------------------------------------------------------- 
    Quiescence search for captures only.
--------------------------------------------------------------------------------------------*/
int quiescence(Board& board, int alpha, int beta) {
    
    #pragma omp critical
    nodeCount++;

    if (knownDraw(board)) {
        return 0;
    }

    Movelist moves;
    movegen::legalmoves<movegen::MoveGenType::CAPTURE>(moves, board);

    int color = board.sideToMove() == Color::WHITE ? 1 : -1;
    int standPat = 0;

    bool mopUp = isMopUpPhase(board);

    if (isMopUpPhase(board)) {
        standPat = color * mopUpScore(board);
    } else {
        standPat = Probe::eval(board.getFen().c_str());
    }

    int bestScore = standPat;
    if (standPat >= beta) {
        return beta;
    }

    alpha = std::max(alpha, standPat);

    std::vector<std::pair<Move, int>> candidateMoves;
    candidateMoves.reserve(moves.size());

    for (const auto& move : moves) {
        auto victim = board.at<Piece>(move.to());
        auto attacker = board.at<Piece>(move.from());
        int victimValue = pieceValues[static_cast<int>(victim.type())];
        int attackerValue = pieceValues[static_cast<int>(attacker.type())];

        int priority = see(board, move);
        candidateMoves.push_back({move, priority});
        
    }

    std::sort(candidateMoves.begin(), candidateMoves.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    for (const auto& [move, priority] : candidateMoves) {
        board.makeMove(move);
        int score = 0;
        score = -quiescence(board, -beta, -alpha);
        board.unmakeMove(move);

        bestScore = std::max(bestScore, score);
        alpha = std::max(alpha, score);

        if (alpha >= beta) { 
            return beta;
        }
    }

    return bestScore;
}

/*-------------------------------------------------------------------------------------------- 
    Negamax with alpha-beta pruning.
--------------------------------------------------------------------------------------------*/
int negamax(Board& board, 
            int depth, 
            int alpha, 
            int beta, 
            std::vector<Move>& PV,
            bool leftMost,
            int ply) {

    auto currentTime = std::chrono::high_resolution_clock::now();
    if (currentTime >= hardDeadline) {
        return 0;
    }

    #pragma omp critical
    nodeCount++;

    bool mopUp = isMopUpPhase(board);

    bool whiteTurn = board.sideToMove() == Color::WHITE;
    bool endGameFlag = gamePhase(board) <= 12;
    int color = whiteTurn ? 1 : -1;
    bool isPV = (alpha < beta - 1); // Principal variation node flag
    
    // Check if the game is over
    auto gameOverResult = board.isGameOver();
    if (gameOverResult.first != GameResultReason::NONE) {
        if (gameOverResult.first == GameResultReason::CHECKMATE) {
            int ply = globalMaxDepth - depth;
            return -(INF/2 - ply); 
        }
        return 0;
    }

    if (board.isRepetition(1)) {
        return 0;
    }

    // Probe the transposition table
    bool found = false;
    Move tableMove;
    int tableEval;
    int tableDepth;
    
    #pragma omp critical
    {
        if (tableLookUp(board, tableDepth, tableEval, tableMove, ttTable)) {
            tableHit++;
            if (tableDepth >= depth) {
                found = true;
            }
        }
    }

    if (found && tableEval >= beta) {
        return tableEval;
    } 

    #pragma omp critical
    {
        if (tableLookUp(board, tableDepth, tableEval, tableMove, ttTableNonPV)) {
            tableHit++;
            if (tableDepth >= depth) {
                found = true;
            }
        }
    }

    if (found && tableEval >= beta) {
        return tableEval;
    }
    

    if (depth <= 0) {
        int quiescenceEval = quiescence(board, alpha, beta);
        return quiescenceEval;
    }

    int standPat = Probe::eval(board.getFen().c_str());

    bool pruningCondition = !board.inCheck() 
                            && !endGameFlag 
                            && alpha < 2000 
                            && alpha > -2000 
                            && beta < 2000
                            && beta > -2000
                            && !leftMost
                            && !mopUp;

    /*--------------------------------------------------------------------------------------------
        Reverse futility pruning: We skip the search if the position is too good for us.
        Avoid pruning in the endgame phase, when alpha is close to the mate score (to avoid missing 
        checkmates). We also not do this in PV nodes.
    --------------------------------------------------------------------------------------------*/
    if (depth <= 8 && pruningCondition) {
        int margin = depth * 350;
        if (standPat - margin > beta) {
            return standPat - margin;
        } 
    }

    /*-------------------------------------------------------------------------------------------- 
        Null move pruning. Avoid null move pruning in the endgame phase.
    --------------------------------------------------------------------------------------------*/
    const int nullDepth = 4; 

    if (depth >= nullDepth && !endGameFlag && !leftMost && !board.inCheck() && !mopUp) {
        std::vector<Move> nullPV;
        int nullEval;
        int reduction = 3;

        if (depth >= 6) {
            reduction = 4;
        }

        board.makeNullMove();
        nullEval = -negamax(board, depth - reduction, -beta, -(beta - 1), nullPV, false, ply + 1);
        board.unmakeNullMove();

        int margin = 0;
        if (isPV) {
            margin = 100;
        }

        if (nullEval >= beta + margin) { 
            return beta;
        } 
    }

    std::vector<std::pair<Move, int>> moves = orderedMoves(board, depth, ply, previousPV, leftMost);
    int bestEval = -INF;
    int quietCount = 0;

    /*--------------------------------------------------------------------------------------------
        Singular extension: If the hash move is much better than the other moves, extend the search.
    --------------------------------------------------------------------------------------------*/
    if (found && depth >= 10 && ply <= globalMaxDepth - 1 && !mopUp) {
        bool singularExtension = true;
        int singularBeta = tableEval - 50; // 80 - 80 * (!isPV) * depth / 60;
        int singularDepth = depth / 2;
        int singularEval = -INF; 
        int bestSingularEval = -INF;

        for (int i = 0; i < moves.size(); i++) {
            if (moves[i].first == tableMove) {
                continue;
            }
            board.makeMove(moves[i].first);
            singularEval = -negamax(board, singularDepth, -(singularBeta + 1), -singularBeta, PV, leftMost, ply + 1);
            board.unmakeMove(moves[i].first);
            bestSingularEval = std::max(bestSingularEval, singularEval);
            if (bestSingularEval >= singularBeta) {
                singularExtension = false;
                break;
            }
        }
        if (singularExtension) {
            depth++;
        }
    }

    /*--------------------------------------------------------------------------------------------
        Simple multicut: look up in the table for the first few moves. If all moves lead to a
        fail high, this is likely a cut node.
        Current not doing well.
    --------------------------------------------------------------------------------------------*/
    // if (depth >= 8 && pruningCondition) {
    //     int failHighCount = 0;
    //     for (int i = 0; i < moves.size(); i++) {
    //         Move move = moves[i].first;

    //         int tableDepth;
    //         int tableEval;
    //         Move tableMove;

    //         board.makeMove(move);
    //         if (tableLookUp(board, tableDepth, tableEval, tableMove, ttTable)) {
    //             if (tableEval >= beta && tableDepth >= depth - 2) {
    //                 failHighCount++;
    //             }
    //         } else if (tableLookUp(board, tableDepth, tableEval, tableMove, ttTableNonPV)) {
    //             if (tableEval >= beta && tableDepth >= depth - 2) {
    //                 failHighCount++;
    //             }
    //         }
    //         board.unmakeMove(move);

    //         if (failHighCount >= 5) {
    //             return beta;
    //         }
    //     }
    // }

    for (int i = 0; i < moves.size(); i++) {

        Move move = moves[i].first;
        std::vector<Move> childPV;

        bool isCapture = board.isCapture(move);
        bool inCheck = board.inCheck();
        bool isPromo = isPromotion(move);
        board.makeMove(move);
        bool isCheck = board.inCheck();
        board.unmakeMove(move);
        bool isPromoThreat = promotionThreatMove(board, move);

        bool quiet = !isCapture && !isCheck && !isPromo && !inCheck && !isPromoThreat;
        if (quiet) {
            quietCount++;
        }

        /*--------------------------------------------------------------------------------------------
            Futility pruning: prune if there is no hope of raising alpha.
            For tactical stability, we only do this for quiet moves.
        --------------------------------------------------------------------------------------------*/
        if (depth <= 2 && quiet && pruningCondition) {
            int margin = 350 * depth;
            if (standPat + margin < alpha) {
                return alpha;
            } 
        }

        if (i > 0) {
            leftMost = false;
        }

        int eval = 0;
        int nextDepth = lateMoveReduction(board, move, i, depth, ply, isPV, quietCount, leftMost); 
        
        /*--------------------------------------------------------------------------------------------
            PVS search: 
            Full window & full depth for the first node or during mop up.

            After the first node, search other nodes with a null window and potentially reduced depth.
            - If the depth is reduced and alpha is raised, research with full depth but still 
            with a null window.
            - Then, if alpha is raised, re-search with a full window & full depth. 

        --------------------------------------------------------------------------------------------*/

        board.makeMove(move);
        bool nullWindow = false;

        if (i == 0) {
            // full window & full depth search for the first node
            eval = -negamax(board, nextDepth, -beta, -alpha, childPV, leftMost, ply + 1);
        } else {
            // null window and potential reduced depth for the rest
            nullWindow = true;
            eval = -negamax(board, nextDepth, -(alpha + 1), -alpha, childPV, leftMost, ply + 1);
        }

        
        board.unmakeMove(move);
        bool alphaRaised = eval > alpha;
        bool reducedDepth = nextDepth < depth - 1;

        if (alphaRaised && reducedDepth && nullWindow) {
            // If alpha is raised and we reduced the depth, research with full depth but still with a null window
            board.makeMove(move);
            eval = -negamax(board, depth - 1, -(alpha + 1), -alpha, childPV, leftMost, ply + 1);
            board.unmakeMove(move);
        } 

        // After this, check if we have raised alpha
        alphaRaised = eval > alpha;

        if (alphaRaised && nullWindow) {
            // If alpha is raised, research with full window & full depth (we don't do this for i = 0)
            board.makeMove(move);
            eval = -negamax(board, depth - 1, -beta, -alpha, childPV, leftMost, ply + 1);
            board.unmakeMove(move);
        }

        if (eval > alpha) {
            PV.clear();
            PV.push_back(move);
            for (auto& move : childPV) {
                PV.push_back(move);
            }
        } 

        bestEval = std::max(bestEval, eval);
        alpha = std::max(alpha, eval);

        if (beta <= alpha) {
            if (!board.isCapture(move) && !isCheck) {
                U64 moveIndex = move.from().index() * 64 + move.to().index();
                #pragma omp critical
                {
                    updateKillerMoves(move, ply);
                    historyTable[moveIndex] += depth * depth;
                }
            }
            break;
        }
    }

    #pragma omp critical
    {
        if (PV.size() > 0) {
            if (isPV) {
                tableInsert(board, depth, bestEval, PV[0], ttTable);
            } else {
                tableInsert(board, depth, bestEval, PV[0], ttTableNonPV);
            }
        }
    }

    return bestEval;
}

/*-------------------------------------------------------------------------------------------- 
    Main search function to communicate with UCI interface.
    Time control: 
    Soft deadline: 2x time limit
    Hard deadline: 3x time limit

    - Case 1: As long as we are within the time limit, we search as deep as we can.
    - Case 2: If we have used more than the time limit:
        Case 2.1: If the search has stabilized, return the best move.
        Case 2.2: If the search has not stabilized and we used less than the soft deadline, 
                  continue searching.
    - Case 3: If we are past the hard deadline, stop the search and return the best move.
--------------------------------------------------------------------------------------------*/
Move findBestMove(Board& board, 
                int numThreads = 4, 
                int maxDepth = 8, 
                int timeLimit = 15000,
                bool quiet = false) {


    auto startTime = std::chrono::high_resolution_clock::now();
    hardDeadline = startTime + 3 * std::chrono::milliseconds(timeLimit);
    softDeadline = startTime + 2 * std::chrono::milliseconds(timeLimit);
    bool timeLimitExceeded = false;

    historyTable.clear();
    killerMoves.clear();

    Move bestMove = Move(); 
    int bestEval = -INF;
    int color = board.sideToMove() == Color::WHITE ? 1 : -1;

    std::vector<std::pair<Move, int>> moves;
    std::vector<Move> globalPV (maxDepth);

    omp_set_num_threads(numThreads);

    const int baseDepth = 1;
    int depth = baseDepth;
    std::vector<int> evals (2 * ENGINE_DEPTH + 1, 0);
    std::vector<Move> candidateMove (2 * ENGINE_DEPTH + 1, Move());

    while (depth <= maxDepth) {
        nodeCount = 0;
        globalMaxDepth = depth;
        tableHit = 0;
        
        // Track the best move for the current depth
        Move currentBestMove = Move();
        int currentBestEval = -INF;
        std::vector<std::pair<Move, int>> newMoves;
        std::vector<Move> PV; // Principal variation

        if (depth == baseDepth) {
            moves = orderedMoves(board, depth, 0, previousPV, false);
        }
        auto iterationStartTime = std::chrono::high_resolution_clock::now();

        bool stopNow = false;
        int quietCount = 0;
        int aspiration, alpha, beta;

        alpha = -INF;
        beta = INF;

        if (depth > 6) {
            aspiration = evals[depth - 1];
            alpha = aspiration - 100;
            beta = aspiration + 100;
        }

        while (true) {

            currentBestEval = -INF;

            #pragma omp parallel for schedule(dynamic, 1)
            for (int i = 0; i < moves.size(); i++) {

                if (stopNow) {
                    continue;
                }

                bool leftMost = (i == 0);

                Move move = moves[i].first;
                std::vector<Move> childPV; 
            
                Board localBoard = board;

                bool isCapture = localBoard.isCapture(move);
                bool inCheck = localBoard.inCheck();
                bool isPromo = isPromotion(move);
                localBoard.makeMove(move);
                bool isCheck = localBoard.inCheck();
                localBoard.unmakeMove(move);
                bool isPromoThreat = promotionThreatMove(localBoard, move);
                int ply = 0;
        
                bool quiet = !isCapture && !isCheck && !isPromo && !inCheck && !isPromoThreat;
                if (quiet) {
                    quietCount++;
                }


                bool newBestFlag = false;  
                int nextDepth = lateMoveReduction(localBoard, move, i, depth, 0, true, quietCount, leftMost);
                int eval = -INF;

                localBoard.makeMove(move);
                eval = -negamax(localBoard, nextDepth, -beta, -alpha, childPV, leftMost, ply + 1);
                localBoard.unmakeMove(move);

                // Check if the time limit has been exceeded, if so the search 
                // has not finished. Return the best move so far.
                if (std::chrono::high_resolution_clock::now() >= hardDeadline) {
                    stopNow = true;
                }

                if (stopNow) continue;

                #pragma omp critical
                {
                    if (eval > currentBestEval) {
                        newBestFlag = true;
                    }
                }

                if (newBestFlag && nextDepth < depth - 1) {
                    localBoard.makeMove(move);
                    eval = -negamax(localBoard, depth - 1, -beta, -alpha, childPV, leftMost, ply + 1);
                    localBoard.unmakeMove(move);

                    // Check if the time limit has been exceeded, if so the search 
                    // has not finished. Return the best move so far.
                    if (std::chrono::high_resolution_clock::now() >= hardDeadline) {
                        stopNow = true;
                    }
                }

                if (stopNow) continue;

                #pragma omp critical
                newMoves.push_back({move, eval});

                #pragma omp critical
                {
                    if (eval > currentBestEval) {
                        currentBestEval = eval;
                        currentBestMove = move;

                        PV.clear();
                        PV.push_back(move);
                        for (auto& move : childPV) {
                            PV.push_back(move);
                        }
                    }
                }
            }

            if (stopNow) {
                break;
            }

            if (currentBestEval < alpha + 1 || currentBestEval > beta - 1) {
                alpha = -INF;
                beta = INF;
                newMoves.clear();
            } else {
                break;
            }
        }
        
        
        if (stopNow) {
            break;
        }

        // Update the global best move and evaluation after this depth if the time limit is not exceeded
        bestMove = currentBestMove;
        bestEval = currentBestEval;

        // Sort the moves by evaluation for the next iteration
        std::sort(newMoves.begin(), newMoves.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });

        #pragma omp critical
        {
            tableInsert(board, depth, bestEval, bestMove, ttTable);
        }

        moves = newMoves;
        previousPV = PV;

        std::string depthStr = "depth " +  std::to_string(std::max(size_t(depth), PV.size()));
        std::string scoreStr = "score cp " + std::to_string(bestEval);
        std::string nodeStr = "nodes " + std::to_string(nodeCount);
        std::string tableHitStr = "tableHit " + std::to_string(static_cast<double>(tableHit) / nodeCount);

        auto iterationEndTime = std::chrono::high_resolution_clock::now();
        std::string timeStr = "time " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(iterationEndTime - iterationStartTime).count());


        std::string pvStr = "pv ";
        for (const auto& move : PV) {
            pvStr += uci::moveToUci(move) + " ";
        }

        std::string analysis = "info " + depthStr + " " + scoreStr + " " +  nodeStr + " " + timeStr + " " + pvStr;

        if (!quiet) {
            std::cout << analysis << std::endl;
        }

        if (moves.size() == 1) {
            return moves[0].first;
        }


        auto currentTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();

        timeLimitExceeded = duration > timeLimit;
        bool spendTooMuchTime = currentTime >= softDeadline;

        evals[depth] = bestEval;
        candidateMove[depth] = bestMove; 

        // Check for stable evaluation
        bool stableEval = true;
        if ((depth > 3 && std::abs(evals[depth] - evals[depth - 2]) > 50) ||
            (depth > 3 && std::abs(evals[depth] - evals[depth - 1]) > 50) ||
            (depth > 3 && candidateMove[depth] != candidateMove[depth - 1])){
            stableEval = false;
        }

        // Break out of the loop if the time limit is exceeded and the evaluation is stable.
        if (!timeLimitExceeded) {
            depth++;
        } else if (stableEval) {
            break;
        } else {
            if (depth > ENGINE_DEPTH || spendTooMuchTime) {
                break;
            } 
            depth++;
        }
    }

    return bestMove; 
}