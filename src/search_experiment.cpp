/*
* Author: Hoa T. Vu
* Created: December 1, 2024
* 
* Copyright (c) 2024 Hoa T. Vu
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to use,
* copy, modify, merge, publish, and distribute copies of the Software for 
* **non-commercial purposes only**, provided that the following conditions are met:
* 
* 1. The above copyright notice and this permission notice shall be included in
*    all copies or substantial portions of the Software.
* 2. Any use of this Software for commercial purposes **requires prior written
*    permission from the author, Hoa T. Vu**.
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
#include <string>
#include <vector>
#include <algorithm>
#include <omp.h> 
#include <chrono>
#include <stdlib.h>
#include <cmath>
#include <filesystem>
#include <mutex>
#include "nnue.hpp"
#include "parameters.hpp"
#include "../lib/fathom/src/tbprobe.h"

using namespace chess;

typedef std::uint64_t U64;

const int maxThreadsID = 50; // Maximum number of threads

/*-------------------------------------------------------------------------------------------- 
    Initialize the NNUE evaluation function.
    Utility function to convert board to pieces array for fast evaluation.
--------------------------------------------------------------------------------------------*/
Network evalNetwork;

void initializeNNUE(std::string path) {
    std::cout << "Initializing NNUE from: " << path << std::endl;
    loadNetwork(path, evalNetwork);
}

std::vector<Accumulator> whiteAccumulator (maxThreadsID);
std::vector<Accumulator> blackAccumulator (maxThreadsID);

/*-------------------------------------------------------------------------------------------- 
    Initialize and look up endgame tablebases.
--------------------------------------------------------------------------------------------*/
void initializeTB(std::string path) {
    std::cout << "Initializing endgame table at path: " << path << std::endl;
    if (!tb_init(path.c_str())) {
        std::cerr << "Failed to initialize endgame table." << std::endl;
    } else {
        std::cout << "Endgame table initialized successfully!" << std::endl;
    }
}

bool probeSyzygy(const Board& board, Move& suggestedMove, int& wdl) {
    // Convert the board to bitboard representation
    U64 white = board.us(Color::WHITE).getBits();
    U64 black = board.us(Color::BLACK).getBits();
    U64 kings = board.pieces(PieceType::KING).getBits();
    U64 queens = board.pieces(PieceType::QUEEN).getBits();
    U64 rooks = board.pieces(PieceType::ROOK).getBits();
    U64 bishops = board.pieces(PieceType::BISHOP).getBits();
    U64 knights = board.pieces(PieceType::KNIGHT).getBits();
    U64 pawns = board.pieces(PieceType::PAWN).getBits();

    unsigned rule50 = board.halfMoveClock() / 2;
    unsigned castling = board.castlingRights().hashIndex();
    unsigned ep = (board.enpassantSq() != Square::underlying::NO_SQ) ? board.enpassantSq().index() : 0;
    bool turn = (board.sideToMove() == Color::WHITE);

    // Create structure to store root move suggestions
    TbRootMoves results;

    int probeSuccess = tb_probe_root_dtz(
        white, black, kings, queens, rooks, bishops, knights, pawns,
        rule50, castling, ep, turn, 
        true, true, &results
    );

    // Handle probe failure
    if (!probeSuccess) {
        probeSuccess = tb_probe_root_wdl(
            white, black, kings, queens, rooks, bishops, knights, pawns,
            rule50, castling, ep, turn, true, &results
        );

        if (!probeSuccess) {
            return false;
        }
    }

    if (results.size > 0) {
        TbRootMove *bestMove = std::max_element(results.moves, results.moves + results.size, 
            [](const TbRootMove &a, const TbRootMove &b) {
                return a.tbRank < b.tbRank; // Higher rank is better
            });

        unsigned from = TB_MOVE_FROM(bestMove->move);
        unsigned to = TB_MOVE_TO(bestMove->move);
        unsigned promotes = TB_MOVE_PROMOTES(bestMove->move);

        int fromIndex = from;
        int toIndex = to;

        if (promotes) {
            switch (promotes) {
                case TB_PROMOTES_QUEEN:
                    suggestedMove = Move::make<Move::PROMOTION>(Square(fromIndex), Square(toIndex), PieceType::QUEEN);
                    break;
                case TB_PROMOTES_ROOK:
                    suggestedMove = Move::make<Move::PROMOTION>(Square(fromIndex), Square(toIndex), PieceType::ROOK);
                    break;
                case TB_PROMOTES_BISHOP:
                    suggestedMove = Move::make<Move::PROMOTION>(Square(fromIndex), Square(toIndex), PieceType::BISHOP);
                    break;
                case TB_PROMOTES_KNIGHT:
                    suggestedMove = Move::make<Move::PROMOTION>(Square(fromIndex), Square(toIndex), PieceType::KNIGHT);
                    break;
            }
            
        } else {
            suggestedMove = Move::make<Move::NORMAL>(Square(fromIndex), Square(toIndex));
        }

        if (bestMove->tbScore > 0) {
            wdl = 1;
        } else if (bestMove->tbScore < 0) {
            wdl = -1;
        } else {
            wdl = 0;
        }

        return true;
    } else {
        return false;
    }
}

/*-------------------------------------------------------------------------------------------- 
    Late move reduction tables.
--------------------------------------------------------------------------------------------*/
std::vector<std::vector<int>> lmrTable1; // LRM for quiet moves

void precomputeLRM(int maxDepth, int maxI) {
    static bool isPrecomputed = false;
    if (isPrecomputed) return;

    lmrTable1.resize(100 + 1, std::vector<int>(maxI + 1));

    for (int depth = maxDepth; depth >= 1; --depth) {
        for (int i = maxI; i >= 1; --i) {
            lmrTable1[depth][i] =  static_cast<int>(lmrC0 + lmrC1 * log(depth) * log(i));
        }
    }

    isPrecomputed = true;
}

/*-------------------------------------------------------------------------------------------- 
    Transposition table lookup and insert.
--------------------------------------------------------------------------------------------*/
int tableSize = 8388608; // Maximum size of the transposition table
int globalMaxDepth = 0; // Maximum depth of current search
int ENGINE_DEPTH = 99; // Maximum search depth for the current engine version

enum EntryType {
    EXACT,
    LOWERBOUND,
    UPPERBOUND
};  

struct TableEntry {
    U64 hash;
    int eval;
    int depth;
    Move bestMove;
    EntryType type;
};

struct LockedTableEntry {
    std::mutex mtx;
    TableEntry entry;
};

std::vector<LockedTableEntry> ttTable(tableSize); 

bool tableLookUp(Board& board, 
    int& depth, 
    int& eval, 
    Move& bestMove, 
    EntryType& type,
    std::vector<LockedTableEntry>& table) {    
    U64 hash = board.hash();
    U64 index = hash % table.size();

    LockedTableEntry& lockedEntry = table[index];
    std::lock_guard<std::mutex> lock(lockedEntry.mtx);  

    if (lockedEntry.entry.hash == hash) {
        depth = lockedEntry.entry.depth;
        eval = lockedEntry.entry.eval;
        bestMove = lockedEntry.entry.bestMove;
        type = lockedEntry.entry.type;
        return true;
    }

    return false;
}

void tableInsert(Board& board, 
    int depth, 
    int eval, 
    Move bestMove, 
    EntryType type,
    std::vector<LockedTableEntry>& table) {

    U64 hash = board.hash();
    U64 index = hash % table.size();
    LockedTableEntry& lockedEntry = table[index];

    std::lock_guard<std::mutex> lock(lockedEntry.mtx); 
    lockedEntry.entry = {hash, eval, depth, bestMove, type}; 
}

/*-------------------------------------------------------------------------------------------- 
    Other global variables.
--------------------------------------------------------------------------------------------*/
std::chrono::time_point<std::chrono::high_resolution_clock> hardDeadline; 
std::vector<Move> rootMoves (2 * ENGINE_DEPTH + 1, Move());

std::vector<U64> nodeCount (maxThreadsID); // Node count for each thread
std::vector<U64> tableHit (maxThreadsID); // Table hit count for each thread
std::vector<Move> previousPV; // Principal variation from the previous iteration

// Killer moves for each thread and ply
std::vector<std::vector<std::vector<Move>>> killer(maxThreadsID, std::vector<std::vector<Move>> 
    (ENGINE_DEPTH + 1, std::vector<Move>(1, Move::NO_MOVE))); 

// History table for move ordering (threadID, side to move, move index)
std::vector<std::vector<std::vector<int>>> history(maxThreadsID, std::vector<std::vector<int>>(2, std::vector<int>(64 * 64, 0)));
std::vector<std::vector<std::vector<int>>> captureHistory(maxThreadsID, std::vector<std::vector<int>>(2, std::vector<int>(64 * 64, 0)));


//std::vector<std::vector<int>> moveStack(maxThreadsID, std::vector<Move>(ENGINE_DEPTH + 1, 0));

// Evaluations along the current path
std::vector<std::vector<int>> evalPath(maxThreadsID, std::vector<int>(ENGINE_DEPTH + 1, 0)); 

// Basic piece values for move ordering
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
    Compute the index of a move to be used in history table and others
--------------------------------------------------------------------------------------------*/
inline U64 moveIndex(const Move& move) {
    return move.from().index() * 64 + move.to().index();
}
 
/*-------------------------------------------------------------------------------------------- 
    Check if the move is a promotion.
--------------------------------------------------------------------------------------------*/
inline bool isPromotion(const Move& move) {
    if (move.typeOf() & Move::PROMOTION) {
        if (move.promotionType() == PieceType::QUEEN) {
            return true;
        } 
    } 
    return false;
}

/*-------------------------------------------------------------------------------------------- 
    Update the killer moves. Currently using only 1 slot per ply.
--------------------------------------------------------------------------------------------*/
inline void updateKillerMoves(const Move& move, int ply, int threadID) {
    killer[threadID][ply][0] = killer[threadID][ply][1];
    killer[threadID][ply][1] = move;
}

/*-------------------------------------------------------------------------------------------- 
    SEE (Static Exchange Evaluation) function.
 -------------------------------------------------------------------------------------------*/
int see(Board& board, Move move, int threadID) {

    nodeCount[threadID]++;
    int to = move.to().index();
    
    // Get victim and attacker piece values
    auto victim = board.at<Piece>(move.to());
    int victimValue = pieceValues[static_cast<int>(victim.type())];

    board.makeMove(move);
    Movelist subsequentCaptures;
    movegen::legalmoves<movegen::MoveGenType::CAPTURE>(subsequentCaptures, board);

    int opponentGain = 0;
    
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

    // Find the maximum gain for the opponent
    for (const Move& nextCapture : attackers) {
        opponentGain = std::max(opponentGain, see(board, nextCapture, threadID));
    }

    board.unmakeMove(move);
    return victimValue - opponentGain;
}

/*--------------------------------------------------------------------------------------------
    Late move reduction. 
--------------------------------------------------------------------------------------------*/
int lateMoveReduction(Board& board, 
                      Move move, 
                      int i, 
                      int depth, 
                      int ply, 
                      bool isPV, 
                      bool leftMost, 
                      int threadID) 
{
    if (isMopUpPhase(board))
        return depth - 1;

    bool stm = board.sideToMove() == Color::WHITE;

    if (i <= 1 || depth <= 3) {
        return depth - 1;
    } else {
        bool improving = (ply >= 2 && evalPath[threadID][ply - 2] < evalPath[threadID][ply]);
        bool isCapture = board.isCapture(move);
        board.makeMove(move);
        bool giveCheck = board.inCheck();
        board.unmakeMove(move);

        int R = lmrTable1[depth][i];
        if (improving || board.inCheck() || giveCheck || isPV) {
            R--;
        }

        if (!isCapture) {
            int histScore = history[threadID][stm][moveIndex(move)];
            R -= histScore / historyLMR;
        }

        return std::min(depth - R, depth - 1);
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
    bool leftMost,
    Move lastMove,
    int threadID,
    bool& hashMoveFound) {

    Movelist moves;
    movegen::legalmoves(moves, board);

    std::vector<std::pair<Move, int>> primary;
    std::vector<std::pair<Move, int>> quiet;

    primary.reserve(moves.size());
    quiet.reserve(moves.size());

    bool stm = board.sideToMove() == Color::WHITE;
    Color color = board.sideToMove();
    U64 hash = board.hash();

    for (const auto& move : moves) {
        int priority = 0;
        bool secondary = false;
        int ply = globalMaxDepth - depth;
        bool hashMove = false;

        // Previous PV move >= hash moves > captures/killer moves > checks > quiet moves
        Move tableMove;
        int tableEval;
        int tableDepth;
        EntryType tableType;
        TableEntry entry;

        if (tableLookUp(board, tableDepth, tableEval, tableMove, tableType, ttTable)) {
            // Hash move from the PV transposition table should be searched first 
            if (tableMove == move && tableType == EntryType::EXACT) {
                priority = 19000 + tableEval;
                primary.push_back({tableMove, priority});
                hashMove = true;
                hashMoveFound = true;
            } else if (tableMove == move && tableType == EntryType::LOWERBOUND) {
                priority = 18000 + tableEval;
                primary.push_back({tableMove, priority});
                hashMove = true;
                hashMoveFound = true;
            }
        } 
      
        if (hashMove) continue;
        
        if (previousPV.size() > ply && leftMost) {
            if (previousPV[ply] == move) {
                priority = 20000; // PV move
                hashMoveFound = true;
            }
        } else if (isPromotion(move)) {
            priority = 16000; 
        } else if (board.isCapture(move)) { 
            int victimValue = pieceValues[static_cast<int>(board.at<Piece>(move.to()).type())];
            int attackerValue = pieceValues[static_cast<int>(board.at<Piece>(move.from()).type())];
            int score = captureHistory[threadID][stm][moveIndex(move)];
            priority = 4000 + victimValue + score;
        } else if (std::find(killer[threadID][ply].begin(), killer[threadID][ply].end(), move) != killer[threadID][ply].end()) {
            priority = 4000;
        } else {
            secondary = true;
            U64 mvIndex = moveIndex(move);
            priority = history[threadID][stm][mvIndex];
        } 

        if (!secondary) {
            primary.push_back({move, priority});
        } else {
            quiet.push_back({move, priority});
        }
    }

    // Sort capture, promotion, checks by priority
    std::sort(primary.begin(), primary.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    std::sort(quiet.begin(), quiet.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    for (const auto& move : quiet) {
        primary.push_back(move);
    }

    return primary;
}

/*-------------------------------------------------------------------------------------------- 
    Quiescence search for captures only.
--------------------------------------------------------------------------------------------*/
int quiescence(Board& board, int alpha, int beta, int ply, int threadID) {
    
    nodeCount[threadID]++;
    int color = (board.sideToMove() == Color::WHITE) ? 1 : -1;
    int standPat = 0;
    bool mopUp = isMopUpPhase(board);

    // Probe Syzygy tablebases
    Move syzygyMove = Move::NO_MOVE;
    int wdl = 0;
    if (probeSyzygy(board, syzygyMove, wdl)) {
        int score = 0;
        if (wdl == 1) {
            // get the fastest path to known win by subtracting the ply
            score = SZYZYGY_INF - ply; 
        } else if (wdl == -1) {
            // delay the loss by adding the ply
            score = -SZYZYGY_INF + ply; 
        } else if (wdl == 0) {
            score = 0;
        }
        return score;
    }

    Movelist moves;
    movegen::legalmoves<movegen::MoveGenType::CAPTURE>(moves, board);

    if (isMopUpPhase(board)) {
        standPat = color * mopUpScore(board);
    } else {
        if (color == 1) {
            standPat = evalNetwork.evaluate(whiteAccumulator[threadID], blackAccumulator[threadID]);
        } else {
            standPat = evalNetwork.evaluate(blackAccumulator[threadID], whiteAccumulator[threadID]);
        }
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
        int priority = see(board, move, threadID);
        candidateMoves.push_back({move, priority});
    }

    std::sort(candidateMoves.begin(), candidateMoves.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    for (auto& [move, priority] : candidateMoves) {
        addAccumulators(board, move, whiteAccumulator[threadID], blackAccumulator[threadID], evalNetwork);
        board.makeMove(move);
        
        int score = 0;
        score = -quiescence(board, -beta, -alpha, ply + 1, threadID);

        subtractAccumulators(board, move, whiteAccumulator[threadID], blackAccumulator[threadID], evalNetwork);
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
int negamax(Board& board, int depth, int alpha, int beta, std::vector<Move>& PV, NodeInfo& nodeInfo) {

    // Stop the search if hard deadline is reached
    auto currentTime = std::chrono::high_resolution_clock::now();
    if (currentTime >= hardDeadline) {
        return 0;
    }

    int threadID = nodeInfo.threadID;
    bool leftMost = nodeInfo.leftMost;
    int ply = nodeInfo.ply;

    // Extract extension information from nodeInfo
    int checkExtensions = nodeInfo.checkExtensions;
    int singularExtensions = nodeInfo.singularExtensions;
    int oneMoveExtensions = nodeInfo.oneMoveExtensions;

    // Extract whether we can do singular search and NMP
    bool doSingularSearch = nodeInfo.doSingularSearch;
    bool doNMP = nodeInfo.doNMP;

    // Extract node type and last move from nodeInfo
    NodeType nodeType = nodeInfo.nodeType;
    Move lastMove = nodeInfo.lastMove;

    nodeCount[threadID]++;
    bool mopUp = isMopUpPhase(board);
    bool endGameFlag = gamePhase(board) <= 12;
    int color = (board.sideToMove() == Color::WHITE) ? 1 : -1;
    bool isPV = (alpha < beta - 1);
    int alpha0 = alpha;
    bool stm = (board.sideToMove() == Color::WHITE);
    
    // Check if the game is over. 
    auto gameOverResult = board.isGameOver();
    if (gameOverResult.first != GameResultReason::NONE) {
        if (gameOverResult.first == GameResultReason::CHECKMATE) {
            return -(INF/2 - ply); // Mate distance pruning.
        }
        return 0;
    }

    // Avoid searching the same position multiple times in the same path
    if (board.isRepetition(1)) {
        return 0;
    }

    // Probe Syzygy tablebases
    Move syzygyMove = Move::NO_MOVE;
    int wdl = 0;
    if (probeSyzygy(board, syzygyMove, wdl)) { 
        int score = 0;
        if (wdl == 1) {
            // get the fastest path to known win by subtracting the ply
            score = SZYZYGY_INF - ply; 
        } else if (wdl == -1) {
            // delay the loss by adding the ply
            score = -SZYZYGY_INF + ply; 
        } else if (wdl == 0) {
            score = 0;
        }
        return score;
    }

    // Probe the transposition table
    bool found = false;
    int tableEval, tableDepth;
    Move tableMove;
    EntryType tableType;
    TableEntry entry;

    /*-------------------------------------------------------------------------------------------- 
        Transposition table lookup.
    --------------------------------------------------------------------------------------------*/
    if (tableLookUp(board, tableDepth, tableEval, tableMove, tableType, ttTable)) {
        tableHit[threadID]++;
        if (tableDepth >= depth) found = true;
    }

    // if (found && tableType == EntryType::EXACT) {
    //     return tableEval;
    // }  
    
    if (found && tableEval >= beta && (tableType == EXACT || tableType == EntryType::LOWERBOUND)) {
        return tableEval;
    }  

    if (found && tableEval <= alpha && !isPV && (tableType == EntryType::UPPERBOUND || tableType == EntryType::EXACT)) {
        return tableEval;
    }
    
    if (depth <= 0 && !board.inCheck()) {
        return quiescence(board, alpha, beta, ply + 1, threadID);
    } else if (depth <= 0) {
        depth++;
        return negamax(board, depth, alpha, beta, PV, nodeInfo);
    }

    int standPat = 0;
    if (stm == 1) {
        standPat = evalNetwork.evaluate(whiteAccumulator[threadID], blackAccumulator[threadID]);
    } else {
        standPat = evalNetwork.evaluate(blackAccumulator[threadID], whiteAccumulator[threadID]);
    }
    
    evalPath[threadID][ply] = standPat; // store the evaluation along the path
    bool improving = (ply >= 2 && evalPath[threadID][ply - 2] < evalPath[threadID][ply]) && !board.inCheck();

    /*--------------------------------------------------------------------------------------------
        Reverse futility pruning.
    ------------------------------------------------------------- -------------------------------*/
    bool rfpCondition = !board.inCheck() 
                            && !endGameFlag 
                            && !isPV
                            && !mopUp
                            && doSingularSearch
                            && abs(alpha) < 8000
                            && abs(beta) < 8000;
    int rfpMargin = rfpScale * depth + (!improving ? 0 : rfpImproving);
    if (depth <= rfpDepth && rfpCondition) {
        if (standPat - rfpMargin > beta) {
            return (standPat + beta)  / 2;
        } 
    } 

    /*-------------------------------------------------------------------------------------------- 
        Null move pruning. Avoid null move pruning in the endgame phase.
    --------------------------------------------------------------------------------------------*/
    const int nullDepth = 4; 

    if (depth >= nullDepth 
        && !endGameFlag 
        && !leftMost 
        && !board.inCheck() 
        && !mopUp 
        && standPat >= beta
        && doNMP
    ) {
        std::vector<Move> nullPV; // dummy PV
        int nullEval;
        int reduction = depth > 6 ? 4 : 3;

        NodeInfo nullNodeInfo = {ply + 1, 
                                false, 
                                checkExtensions,
                                singularExtensions,
                                oneMoveExtensions,
                                false, // turn off NMP for this path
                                doSingularSearch,
                                Move::NULL_MOVE,
                                NodeType::ALL, // expected all node
                                threadID};

        //moveStack[threadID][ply] = Move::NULL_MOVE; 
        board.makeNullMove();
        nullEval = -negamax(board, depth - reduction, -beta, -(beta - 1), nullPV, nullNodeInfo);
        board.unmakeNullMove();

        if (nullEval >= beta) return beta;
    }

    bool hashMoveFound = false;
    killer[threadID][ply + 1] = {Move::NO_MOVE, Move::NO_MOVE}; 
    std::vector<std::pair<Move, int>> moves = orderedMoves(board, 
                                                        depth, 
                                                        ply, 
                                                        previousPV, 
                                                        leftMost, 
                                                        lastMove, 
                                                        threadID,
                                                        hashMoveFound);

    int bestEval = -INF;

    /*--------------------------------------------------------------------------------------------
        Simplified version of IID.
        Reduce the depth to facilitate the search if no hash move found.
        Restricted to expected cut nodes and depth > 3.
    --------------------------------------------------------------------------------------------*/
    if (!hashMoveFound && depth >= 4 && (nodeType == NodeType::CUT || nodeType == NodeType::PV) && doSingularSearch) {
        depth = depth - 1;
    }

    /*--------------------------------------------------------------------------------------------
        Singular extension.
    --------------------------------------------------------------------------------------------*/
    if (hashMoveFound && tableDepth >= depth - singularTableReduce
                        && depth >= singularDepth
                        && (tableType == EntryType::EXACT || tableType == EntryType::LOWERBOUND)
                        && isPV
                        && standPat >= beta
                        && doSingularSearch) {

            int singularEval = 0;
            bool singular = true;

            for (int i = 1; i < moves.size(); i++) {
                
                //moveStack[threadID][ply] = moves[i].first; 
                addAccumulators(board, moves[i].first, whiteAccumulator[threadID], blackAccumulator[threadID], evalNetwork);
                board.makeMove(moves[i].first);

                NodeInfo childNodeInfo = {ply + 1, 
                                            leftMost, 
                                            checkExtensions,
                                            singularExtensions,
                                            oneMoveExtensions,
                                            false, // turn off NMP for this path
                                            false, // turn off singular search for this path
                                            moves[i].first,
                                            NodeType::PV,
                                            threadID};

                singularEval = -negamax(board, depth / singularReduceFactor, -beta, -alpha, PV, childNodeInfo);

                subtractAccumulators(board, moves[i].first, whiteAccumulator[threadID], blackAccumulator[threadID], evalNetwork);
                board.unmakeMove(moves[i].first);

                if (singularEval < tableEval) {
                    singular = false;
                    break;
                }

                if (standPat >= beta && tableEval >= beta && singularEval >= beta) {
                    // Simplified multicut: if static eval, table eval of the hash move, and the second move 
                    // are all >= beta, we can assume a beta cutoff.
                    return (standPat + beta) / 2;
                }
            }
            
            if (singularExtensions && singular) {
                depth++;
                singularExtensions--;
            }

    }

    /*--------------------------------------------------------------------------------------------
        Evaluate moves.
    --------------------------------------------------------------------------------------------*/
    int numCaptures = 0;
    int numQuiet = 0;

    for (int i = 0; i < moves.size(); i++) {

        Move move = moves[i].first;
        std::vector<Move> childPV;
        
        bool isPromo = isPromotion(move);
        bool inCheck = board.inCheck();
        bool isCapture = board.isCapture(move);
        bool isQuiet = !isCapture && !isPromo; 
        board.makeMove(move);
        bool giveCheck = board.inCheck();
        board.unmakeMove(move);

        if (i > 0) leftMost = false;
        int eval = 0;
        int nextDepth = lateMoveReduction(board, move, i, depth, ply, isPV, leftMost, threadID); 

        /*--------------------------------------------------------------------------------------------
            Late move pruning
        --------------------------------------------------------------------------------------------*/
        bool lmpCondition = !isPromo && !inCheck && !isPV && doSingularSearch;

        int lmpValue = (lmpC0 + lmpC1 * depth + lmpC2 * depth * depth) / (lmpC3 + !improving);
        if (lmpCondition && isQuiet && nextDepth <= lmpDepth && i >= std::max(1, lmpValue)) {
            continue; 
        }

        /*--------------------------------------------------------------------------------------------
            History pruning
        --------------------------------------------------------------------------------------------*/       
        bool hpCondition = !isPromo && !inCheck && !isPV && doSingularSearch && isQuiet;
        if (i > 0 && hpCondition) {
            int mvIndex = moveIndex(move);
            if (history[threadID][stm][mvIndex] < -histC0 - histC1 * nextDepth) {
                continue;
            } 
        }

        /*--------------------------------------------------------------------------------------------
            SEE pruning
        --------------------------------------------------------------------------------------------*/
        if (isCapture && i > 0 && doSingularSearch && nextDepth <= seeDepth) {
            int seeScore = see(board, move, threadID);
            if (isCapture && seeScore < -seeC1 * nextDepth) {
                continue;
            } 
        }

        /*--------------------------------------------------------------------------------------------
            Futility pruning: prune if there is no hope of raising alpha.
            For tactical stability, we only do this for quiet moves.
        --------------------------------------------------------------------------------------------*/
        bool fpCondition = !isPromo 
                            && !inCheck 
                            && !isPV 
                            && doSingularSearch 
                            && !isCapture 
                            && doSingularSearch
                            && !giveCheck
                            && abs(alpha) < 8000
                            && abs(beta) < 8000;

        if (nextDepth <= fpDepth && fpCondition && i > 0) {
            int margin = (fpC0 + fpC1 * depth + fpImprovingC * improving);
            if (standPat + margin < alpha) {
                continue;
            } 
        }

        /*--------------------------------------------------------------------------------------------
            PVS search: 
            Full window & full depth for the first node or during mop up.
            After the first node, search other nodes with a null window and potentially reduced depth.
            Then, if alpha is raised, re-search with a full window & full depth. 
        --------------------------------------------------------------------------------------------*/
        addAccumulators(board, move, whiteAccumulator[threadID], blackAccumulator[threadID], evalNetwork);
        //moveStack[threadID][ply] = move; 
        board.makeMove(move);

        bool nullWindow = false;

        NodeInfo childNodeInfo = {ply + 1, 
                                leftMost, 
                                checkExtensions,
                                singularExtensions,
                                oneMoveExtensions,
                                doNMP,
                                doSingularSearch,
                                move,
                                NodeType::PV,
                                threadID};

        int rank = move.to().index() / 8;

        if (checkExtensions && board.inCheck()) { 
            nextDepth++;
            childNodeInfo.checkExtensions--;
        }  
        
        if (oneMoveExtensions && moves.size() == 1) { 
            nextDepth++;
            childNodeInfo.oneMoveExtensions--;
        }
        
        if (i == 0) {
            // full window & full depth search for the first node
            
            /*--------------------------------------------------------------------------------------------
                In PVS search, in the first child:
                1. if the node is a cut node, the child is an ALL node
                2. if the node is an ALL node, the child is a CUT node
                2. if the node is a PV node, the child is a PV node
            --------------------------------------------------------------------------------------------*/
            NodeType childNodeType;
            if (!isPV && nodeType == NodeType::CUT) {
                childNodeType = NodeType::ALL;
            } else if (!isPV && nodeType == NodeType::ALL) {
                childNodeType = NodeType::CUT;
            } else if (isPV) {
                childNodeType = NodeType::PV;
            } 
            childNodeInfo.nodeType = childNodeType;

            eval = -negamax(board, nextDepth, -beta, -alpha, childPV, childNodeInfo);
        } else {
            // null window and potential reduced depth for the rest
            nullWindow = true;

            /*--------------------------------------------------------------------------------------------
                In PVS search, in the subsequent children:
                1. if the node is a PV node, any child after the first is a CUT node
                2. if the node is an ALL node, the child is a CUT node
                2. if the node is a CUT node, the child is an ALL node
            --------------------------------------------------------------------------------------------*/
            NodeType childNodeType;
            if (isPV) {
                childNodeType = NodeType::ALL;
            } else if (!isPV && nodeType == NodeType::ALL) {
                childNodeType = NodeType::CUT;
            } else if (!isPV && nodeType == NodeType::CUT) {
                childNodeType = NodeType::ALL;
            }
            childNodeInfo.nodeType = childNodeType;

            eval = -negamax(board, nextDepth, -(alpha + 1), -alpha, childPV, childNodeInfo);
        }
        
        subtractAccumulators(board, move, whiteAccumulator[threadID], blackAccumulator[threadID], evalNetwork);
        board.unmakeMove(move);

        bool alphaRaised = eval > alpha;
        bool reducedDepth = nextDepth < depth - 1;

        if (alphaRaised && (nullWindow || reducedDepth)  && isPV) {


            /*--------------------------------------------------------------------------------------------
                Since now we are in a full window search, the child node is a PV node.
            --------------------------------------------------------------------------------------------*/
            childNodeInfo.nodeType = NodeType::PV;

            // If alpha is raised, research with full window & full depth (we don't do this for i = 0)
            //moveStack[threadID][ply] = move;
            addAccumulators(board, move, whiteAccumulator[threadID], blackAccumulator[threadID], evalNetwork);
            board.makeMove(move);

            eval = -negamax(board, depth - 1, -beta, -alpha, childPV, childNodeInfo);

            subtractAccumulators(board, move, whiteAccumulator[threadID], blackAccumulator[threadID], evalNetwork);
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

            float delta = deltaC2  * depth * depth + deltaC1 * depth + deltaC0;

            if (!board.isCapture(move)) {
                updateKillerMoves(move, ply, threadID);
                
                // tapered change new score = old score - (1 - old score / maxHistory) * depth * depth
                // the closer the old score is to maxHistory, the less change is applied.
                int mvIndex = moveIndex(move);
                int currentScore = history[threadID][stm][mvIndex];
                int change = (1.0 - static_cast<float>(std::abs(currentScore)) / static_cast<float>(maxHistory)) * delta;

                history[threadID][stm][mvIndex] += change;

                updateKillerMoves(move, ply, threadID);
            } else {
                int mvIndex = moveIndex(move);
                int currentScore = captureHistory[threadID][stm][mvIndex];
                int change = (1.0 - static_cast<float>(std::abs(currentScore)) / static_cast<float>(maxCaptureHistory)) * delta;

                captureHistory[threadID][stm][mvIndex] += change;
            }

            for (int j = 0; j < i; j++) {
                int mvIndex = moveIndex(moves[j].first);
                bool isCapture = board.isCapture(moves[j].first);
            
                int currentHistScore = isCapture ? 
                                        captureHistory[threadID][stm][mvIndex] : 
                                        history[threadID][stm][mvIndex];
            
                int change = 0;

                if (isCapture) {
                    change = (1.0 - static_cast<float>(std::abs(currentHistScore)) / static_cast<float>(maxCaptureHistory)) * delta;
                    captureHistory[threadID][stm][mvIndex] -= change;
                } else {
                    change = (1.0 - static_cast<float>(std::abs(currentHistScore)) / static_cast<float>(maxHistory)) * delta;
                    history[threadID][stm][mvIndex] -= change;
                }
            }
 
            break;
        } 
    }

    if (isPV) {
        EntryType type;

        if (bestEval >= alpha0 && bestEval < beta) {
            type = EXACT;
        } else if (bestEval < alpha0) {
            type = UPPERBOUND;
        } else {
            type = LOWERBOUND;
        } 

        if (PV.size() > 0) {
            tableInsert(board, depth, bestEval, PV[0], type, ttTable);
        } else {
            tableInsert(board, depth, bestEval, Move::NO_MOVE, type, ttTable);
        }

    } else {
        
        if (bestEval > alpha0) {
            EntryType type = LOWERBOUND;
            if (PV.size() > 0) {
                tableInsert(board, depth, bestEval, PV[0], type, ttTable);
            } else {
                tableInsert(board, depth, bestEval, Move::NO_MOVE, type, ttTable);
            }  
        } 

    }
    
    return bestEval;
}

/*-------------------------------------------------------------------------------------------- 
    Main search function to communicate with UCI interface. This is the root node.
    Time control: 
    Hard deadline: 2x time limit

    - Case 1: As long as we are within the time limit, we search as deep as we can.
    - Case 2: Stop if we reach the hard deadline or certain depth.
--------------------------------------------------------------------------------------------*/
Move findBestMove(Board& board, int numThreads = 4, int maxDepth = 30, int timeLimit = 15000) {

    /*--------------------------------------------------------------------------------------------
        Set up ttTable, threads, time limits, history scores, killer moves and accumulators.
    --------------------------------------------------------------------------------------------*/
    omp_set_num_threads(numThreads);
    auto startTime = std::chrono::high_resolution_clock::now();
    hardDeadline = startTime + 2 * std::chrono::milliseconds(timeLimit);
    bool timeLimitExceeded = false;
    rootMoves = {};
    
    precomputeLRM(100, 500); // Precompute late move reduction table

    if (ttTable.size() != tableSize) {
        // Update if the size for the transposition table changes.
        ttTable = std::vector<LockedTableEntry>(tableSize);
    }

    // Reset history scores 
    for (int i = 0; i < maxThreadsID; i++) {
        for (int j = 0; j < 64 * 64; j++) {
            history[i][0][j] = 0;
            history[i][1][j] = 0;

            captureHistory[i][0][j] = 0;
            captureHistory[i][1][j] = 0;
        }

        nodeCount[i] = 0;
        tableHit[i] = 0;
    }

    for (int i = 0; i < maxThreadsID; i++) {
        for (int j = 0; j < ENGINE_DEPTH; j++) {
            killer[i][j] = {Move::NO_MOVE, Move::NO_MOVE};
        }
    }

    for (int i = 0; i < maxThreadsID; i++) {
        makeAccumulators(board, whiteAccumulator[i], blackAccumulator[i], evalNetwork);
    }

    Move bestMove = Move(); 
    int bestEval = -INF;
    int color = board.sideToMove() == Color::WHITE ? 1 : -1;

    std::vector<std::pair<Move, int>> moves;
    std::vector<int> evals (2 * ENGINE_DEPTH + 1, 0);
    
    /*--------------------------------------------------------------------------------------------
        Check if the move position is in the endgame tablebase.
    --------------------------------------------------------------------------------------------*/
    Move syzygyMove;
    int wdl = 0;

    if (probeSyzygy(board, syzygyMove, wdl)) {

        int score = 0;
        if (wdl == 1) {
            score = SZYZYGY_INF;
        } else if (wdl == -1) {
            score = -SZYZYGY_INF;
        }
        std::cout << "info depth 0 score cp " << score << " nodes 0 time 0  pv " << uci::moveToUci(syzygyMove) << std::endl;
        
        
        if (syzygyMove != Move::NO_MOVE) {
            try {
                Board boardCopy = board;
                boardCopy.makeMove(syzygyMove);
                return syzygyMove;  // Valid move, return it
            } catch (const std::exception&) {
                // In case somehow the move is invalid, continue with the search
            }
        }
    }
    
    /*--------------------------------------------------------------------------------------------
        Start the search.
    --------------------------------------------------------------------------------------------*/

    int standPat = evalNetwork.evaluate(whiteAccumulator[0], blackAccumulator[0]);
    int depth = 0;

    while (depth <= maxDepth) {
        globalMaxDepth = depth;

        // Track the best move for the current depth
        Move currentBestMove = Move();
        int currentBestEval = -INF;
        bool hashMoveFound = false;

        bool stopNow = false;
        int alpha = (depth > 6) ? evals[depth - 1] - 150 : -INF;
        int beta  = (depth > 6) ? evals[depth - 1] + 150 : INF;

        std::vector<std::pair<Move, int>> newMoves;
        std::vector<Move> PV; 
        
        if (depth == 0) {
            moves = orderedMoves(board, depth, 0, previousPV, false, Move::NO_MOVE, 0, hashMoveFound);
        }

        while (true) {
            currentBestEval = -INF;

            #pragma omp parallel for schedule(dynamic, 1)
            for (int i = 0; i < 3 * moves.size(); i++) {

                if (stopNow) continue;
                
                bool leftMost = (i == 0);
                Move move = moves[i % moves.size()].first;
                
                std::vector<Move> childPV; 
                Board localBoard = board;
                evalPath[omp_get_thread_num()][0] = standPat;

                int ply = 0;
                bool newBestFlag = false;  
                int threadID = omp_get_thread_num();
                int nextDepth = lateMoveReduction(localBoard, move, i % moves.size(), depth, 0, true, leftMost, threadID);
                int eval = -INF;

                NodeInfo childNodeInfo = {1, 
                                        leftMost, 
                                        checkExtensions,
                                        singularExtensions,
                                        oneMoveExtensions,
                                        true,
                                        true,
                                        move,
                                        NodeType::PV,
                                        threadID};
                
                addAccumulators(localBoard, 
                                move, 
                                whiteAccumulator[threadID], 
                                blackAccumulator[threadID], 
                                evalNetwork);
                localBoard.makeMove(move);

                eval = -negamax(localBoard, nextDepth, -beta, -alpha, childPV, childNodeInfo);

                subtractAccumulators(localBoard, 
                                    move, 
                                    whiteAccumulator[threadID], 
                                    blackAccumulator[threadID],
                                    evalNetwork);
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

                    addAccumulators(localBoard, 
                                    move, 
                                    whiteAccumulator[threadID], 
                                    blackAccumulator[threadID], 
                                    evalNetwork);
                    localBoard.makeMove(move);

                    eval = -negamax(localBoard, depth - 1, -beta, -alpha, childPV, childNodeInfo);

                    subtractAccumulators(localBoard, 
                                        move, 
                                        whiteAccumulator[threadID], 
                                        blackAccumulator[threadID], 
                                        evalNetwork);
                    localBoard.unmakeMove(move);

                    // Check if the time limit has been exceeded, if so the search 
                    // has not finished. Return the best move so far.
                    if (std::chrono::high_resolution_clock::now() >= hardDeadline) {
                        stopNow = true;
                    }
                }

                if (stopNow) continue;

                #pragma omp critical
                {
                    bool computed = false;
                    for (auto& [mv, mvEval] : newMoves) {
                        if (mv == move) {
                            mvEval = eval;
                            computed = true;
                        }
                    }
                    if (!computed) {
                        newMoves.push_back({move, eval});
                    }
                }
                
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
                    } else if (eval == currentBestEval) {
                        // This is mostly for Syzygy tablebase.
                        // Prefer the move that is a capture or a pawn move.
                        if (localBoard.isCapture(move) || localBoard.at<Piece>(move.from()).type() == PieceType::PAWN) {
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
            }

            if (stopNow) break;

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

        tableInsert(board, depth, bestEval, bestMove, EntryType::EXACT, ttTable);

        moves = newMoves;
        previousPV = PV;

        U64 totalNodeCount = 0, totalTableHit = 0;
        for (int i = 0; i < maxThreadsID; i++) {
            totalNodeCount += nodeCount[i];
            totalTableHit += tableHit[i];
        }

        std::string analysis = formatAnalysis(depth, bestEval, totalNodeCount, totalTableHit, startTime, PV, board);
        std::cout << analysis << std::endl;
        
        if (moves.size() == 1) {
            return moves[0].first; // If there is only one move, return it immediately.
        }

        auto currentTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();

        timeLimitExceeded = duration > timeLimit;
        bool spendTooMuchTime = currentTime >= hardDeadline;

        evals[depth] = bestEval;
        rootMoves[depth] = bestMove; 
        
        if (!timeLimitExceeded) {
            // If the time limit is not exceeded, we can search deeper.
            depth++;// =  std::max(depth + 1, static_cast<int>(PV.size()) + 1);
        } else {
            // If we go beyond the hard limit or stabilize. Else, we can still search deeper.
            if (spendTooMuchTime || (depth >= 1 && rootMoves[depth] == rootMoves[depth - 1])) break;
            depth++;
        }
    }

    return bestMove; 
}