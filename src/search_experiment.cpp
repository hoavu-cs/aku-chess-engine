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
#include "search.hpp"
#include "search_utils.hpp"
#include "syzygy.hpp"
#include "chess.hpp"
#include "utils.hpp"


using namespace chess;

typedef std::uint64_t U64;

const int maxThreadsID = 50; // Maximum number of threads

/*-------------------------------------------------------------------------------------------- 
    Initialize the NNUE evaluation function.
    Utility function to convert board to pieces array for fast evaluation.
--------------------------------------------------------------------------------------------*/
Network nnue;

void initializeNNUE(std::string path) {
    std::cout << "Initializing NNUE from: " << path << std::endl;
    loadNetwork(path, nnue);
}

std::vector<Accumulator> wAccumulator (maxThreadsID);
std::vector<Accumulator> bAccumulator (maxThreadsID);


/*-------------------------------------------------------------------------------------------- 
    Late move reduction tables.
--------------------------------------------------------------------------------------------*/
std::vector<std::vector<int>> lmrTable1; 

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
int tableSize = 4194304; // Maximum size of the transposition table
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
std::vector<std::vector<int>> staticEval(maxThreadsID, std::vector<int>(ENGINE_DEPTH + 1, 0)); 

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
        bool improving = (ply >= 2 && staticEval[threadID][ply - 2] < staticEval[threadID][ply]);
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
        Move ttMove;
        int ttEval;
        int ttDepth;
        EntryType ttType;
        TableEntry entry;

        if (tableLookUp(board, ttDepth, ttEval, ttMove, ttType, ttTable)) {
            // Hash move from the PV transposition table should be searched first 
            if (ttMove == move && ttType == EntryType::EXACT) {
                priority = 19000 + ttEval;
                primary.push_back({ttMove, priority});
                hashMove = true;
                hashMoveFound = true;
            } else if (ttMove == move && ttType == EntryType::LOWERBOUND) {
                priority = 18000 + ttEval;
                primary.push_back({ttMove, priority});
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
    if (syzygy::probeSyzygy(board, syzygyMove, wdl)) {
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
            standPat = nnue.evaluate(wAccumulator[threadID], bAccumulator[threadID]);
        } else {
            standPat = nnue.evaluate(bAccumulator[threadID], wAccumulator[threadID]);
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
        addAccumulators(board, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
        board.makeMove(move);
        
        int score = 0;
        score = -quiescence(board, -beta, -alpha, ply + 1, threadID);

        subtractAccumulators(board, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
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
    bool singularSearchOk = nodeInfo.singularSearchOk;
    bool nmpOK = nodeInfo.nmpOk;

    // Extract node type and last move from nodeInfo
    NodeType nodeType = nodeInfo.nodeType;
    Move lastMove = nodeInfo.lastMove;

    nodeCount[threadID]++;
    bool mopUp = isMopUpPhase(board);
    bool endGameFlag = gamePhase(board) <= 12;
    int color = (board.sideToMove() == Color::WHITE) ? 1 : -1;
    bool isPV = (alpha < beta - 1);
    int alpha0 = alpha; // Original alpha passed from the parent node
    bool stm = (board.sideToMove() == Color::WHITE);
    
    // Check if the game is over. 
    auto gameOverResult = board.isGameOver();
    if (gameOverResult.first != GameResultReason::NONE) {
        if (gameOverResult.first == GameResultReason::CHECKMATE) {
            return -INF/2; // Mate distance pruning.
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
    if (syzygy::probeSyzygy(board, syzygyMove, wdl)) { 
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
    int ttEval, ttDepth;
    Move ttMove;
    EntryType ttType;
    TableEntry entry;

    if (tableLookUp(board, ttDepth, ttEval, ttMove, ttType, ttTable)) {
        tableHit[threadID]++;
        if (ttDepth >= depth) found = true;
    }

    if (found && !isPV) {
        if (ttType == EntryType::EXACT
            || (ttType == EntryType::LOWERBOUND && ttEval >= beta)
            || (ttType == EntryType::UPPERBOUND && ttEval <= alpha)) {
            return ttEval;
        } 
    }

    if (found && isPV) {
        if (ttType == EntryType::EXACT 
            || (ttType == EntryType::LOWERBOUND && ttEval >= beta)) {
            return ttEval;
        } 
    }
    
    
    if (depth <= 0 && !board.inCheck()) {
        return quiescence(board, alpha, beta, ply + 1, threadID);
    } else if (depth <= 0) {
        return negamax(board, 1, alpha, beta, PV, nodeInfo);
    }

    int standPat = 0;
    if (stm == 1) {
        standPat = nnue.evaluate(wAccumulator[threadID], bAccumulator[threadID]);
    } else {
        standPat = nnue.evaluate(bAccumulator[threadID], wAccumulator[threadID]);
    }
    
    staticEval[threadID][ply] = standPat; // store the evaluation along the path
    bool improving = (ply >= 2 && staticEval[threadID][ply - 2] < staticEval[threadID][ply]) && !board.inCheck();

    // Reverse futility pruning (RFP)
    bool rfpCondition = !board.inCheck() 
                            && !endGameFlag 
                            && !isPV
                            && !mopUp
                            && singularSearchOk;
    int rfpMargin = rfpScale * depth + (!improving ? 0 : rfpImproving);
    if (depth <= rfpDepth && rfpCondition) {
        if (standPat - rfpMargin > beta) {
            return (standPat + beta)  / 2;
        } 
    } 

    // Null move pruning
    const int nullDepth = 4; 

    if (depth >= nullDepth 
        && !endGameFlag // avoid null move pruning in endgame
        && !leftMost 
        && !board.inCheck() 
        && !mopUp 
        && standPat >= beta
        && nmpOK
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
                                singularSearchOk,
                                Move::NULL_MOVE,
                                NodeType::ALL, // expected all node
                                threadID};

        //moveStack[threadID][ply] = Move::NULL_MOVE; 
        board.makeNullMove();
        nullEval = -negamax(board, depth - reduction, -beta, -(beta - 1), nullPV, nullNodeInfo);
        evalAdjust(nullEval);
        board.unmakeNullMove();

        if (nullEval >= beta) {
            return beta;
        } else if (nullEval <= -INF/2 + 2) {
            // If we don't make a move and get mated, then we should return beta + 1 to triger a
            // re-search for this threat.
            return beta + 1;
        }
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
    bool searchAllFlag = false;

    // Simplified version of IID.
    // Reduce the depth to facilitate the search if no hash move found.
    // Restricted to expected cut nodes and depth > 3.
    if (!hashMoveFound && depth >= 4 && (nodeType == NodeType::CUT || nodeType == NodeType::PV)) {
        depth = depth - 1;
    }

    //Singular extension.
    if (hashMoveFound && ttDepth >= depth - singularTableReduce
                        && depth >= singularDepth
                        && (ttType == EntryType::EXACT || ttType == EntryType::LOWERBOUND)
                        && isPV
                        && standPat >= beta
                        && singularSearchOk) {

            int singularEval = 0;
            bool singular = true;

            for (int i = 1; i < moves.size(); i++) {
                
                //moveStack[threadID][ply] = moves[i].first; 
                addAccumulators(board, moves[i].first, wAccumulator[threadID], bAccumulator[threadID], nnue);
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
                evalAdjust(singularEval);

                subtractAccumulators(board, moves[i].first, wAccumulator[threadID], bAccumulator[threadID], nnue);
                board.unmakeMove(moves[i].first);

                if (singularEval < ttEval) {
                    singular = false;
                    break;
                }
            }
            
            if (singularExtensions && singular) {
                depth++;
                singularExtensions--;
            }
    }

    // Evaluate moves.
    int numCaptures = 0;
    int numQuiet = 0;

    for (int i = 0; i < moves.size(); i++) {

        Move move = moves[i].first;
        std::vector<Move> childPV;
        
        bool isPromo = isPromotion(move);
        bool inCheck = board.inCheck();
        bool isCapture = board.isCapture(move);
        bool isQuiet = !isCapture && !isPromo; 
        bool isPromoThreat = promotionThreat(board, move);

        if (i > 0) leftMost = false;
        int eval = 0;
        int nextDepth = lateMoveReduction(board, move, i, depth, ply, isPV, leftMost, threadID); 

        // Late move pruning
        bool lmpCondition = isQuiet && !inCheck && !isPV && singularSearchOk;
        int lmpValue = (lmpC0 + lmpC1 * depth + lmpC2 * depth * depth) / (lmpC3 + !improving);
        if (lmpCondition && isQuiet && nextDepth <= lmpDepth && i >= std::max(1, lmpValue)) {
            continue; 
        }

        // History pruning       
        bool hpCondition = !isPromo && !isPromoThreat && !inCheck && !isPV && singularSearchOk && isQuiet;
        if (i > 0 && hpCondition) {
            int mvIndex = moveIndex(move);
            if (history[threadID][stm][mvIndex] < -histC0 - histC1 * nextDepth) {
                continue;
            } 
        }

        // SEE pruning
        if (isCapture && i > 0 && singularSearchOk && nextDepth <= seeDepth) {
            int seeScore = see(board, move, threadID);
            if (seeScore < -seeC1 * nextDepth) {
                continue;
            }
        }

        // Futility pruning
        bool fpCondition = !isPromo 
                            && !isPromoThreat
                            && !inCheck 
                            && !isCapture 
                            && !isPV 
                            && singularSearchOk;

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
        addAccumulators(board, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
        board.makeMove(move);
        bool nullWindow = false;
        bool reducedDepth = nextDepth < depth - 1;

        NodeInfo childNodeInfo = {ply + 1, 
                                leftMost, 
                                checkExtensions,
                                singularExtensions,
                                oneMoveExtensions,
                                nmpOK,
                                singularSearchOk,
                                move,
                                NodeType::PV,
                                threadID};

        if (checkExtensions && board.inCheck()) { 
            nextDepth++;
            childNodeInfo.checkExtensions--;
        }  
        
        if (oneMoveExtensions && moves.size() == 1) { 
            nextDepth++;
            childNodeInfo.oneMoveExtensions--;
        }
        
        // Full window for the first node. 
        // Once alpha is raised, we search with null window until alpha is raised again.
        // If alpha is raised on a null window or reduced depth, we search with full window and full depth.
        if (i == 0) {
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
            evalAdjust(eval);
        } else {
            // If we are in a PV node and search the next child on a null window, we expect
            // the child to be a CUT node. 
            // If we are in a CUT node, we expect the child to be an ALL node.
            // If we are in an ALL node, we expect the child to be a CUT node.
            nullWindow = true;
            NodeType childNodeType;
            if (isPV) {
                childNodeType = NodeType::CUT;
            } else if (!isPV && nodeType == NodeType::ALL) {
                childNodeType = NodeType::CUT;
            } else if (!isPV && nodeType == NodeType::CUT) {
                childNodeType = NodeType::ALL;
            }
            childNodeInfo.nodeType = childNodeType;

            eval = -negamax(board, nextDepth, -(alpha + 1), -alpha, childPV, childNodeInfo);
            evalAdjust(eval);
        }
        
        subtractAccumulators(board, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
        board.unmakeMove(move);
    
        // If we raised alpha in a null window search or reduced depth search, re-search with full window and full depth.
        // We don't need to do this for non-PV nodes because when beta = alpha + 1, the full window is the same as the null window.
        // Furthermore, if we are in a non-PV node and a reduced depth search raised alpha, then we will need to 
        // re-search with full window and full depth in some ancestor node anyway so there is no need to do it here. 
        if ((eval > alpha) && (nullWindow || reducedDepth) && isPV) {

            // Now this child becomes a PV node.
            childNodeInfo.nodeType = NodeType::PV;

            addAccumulators(board, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
            board.makeMove(move);

            eval = -negamax(board, depth - 1, -beta, -alpha, childPV, childNodeInfo);
            evalAdjust(eval);

            subtractAccumulators(board, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
            board.unmakeMove(move);
        }

        if (eval > alpha) {
            updatePV(PV, move, childPV);
        } 

        bestEval = std::max(bestEval, eval);
        alpha = std::max(alpha, eval);

        // Beta cutoff.
        if (beta <= alpha) {

            float delta = deltaC2  * depth * depth + deltaC1 * depth + deltaC0;

            // Update history scores for the move that caused the cutoff and the previous moves that failed to cutoffs.
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
        // If the bestEval is in (alpha0, beta), then bestEval is EXACT.
        // If the bestEval <= alpha0, then bestEval is UPPERBOUND because this is caused by one of the children's beta-cutoff.
        // If the bestEval >= beta then we quit the loop early, then we know that this is a LOWERBOUND. 
        // This ccould be exact if the cut off is at the last child, but it's not too important to handle this case separately.
        // If the bestEval is in (alpha0, beta), then we know that this is an exact score.
        // This is nice in the sense that if a node is non-PV, the bounds are always UPPERBOUND and LOWERBOUND.
        // EXACT flag only happens at PV nodes.
        // For non-PV nodes, the bounds are artifical so we can't say for sure.
        EntryType type;

        if (bestEval > alpha0 && bestEval < beta && searchAllFlag) {
            type = EXACT;
        } else if (bestEval <= alpha0) {
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
        // For non-PV nodes:
        // alpha is artifical for CUT nodes and beta is artifical for ALL nodes.
        // In CUT nodes, we have a fake alpha. We can only tell if bestEval is a LOWERBOUND if we have a beta cutoff.
        // IN ALL nodess, we have a fake beta. Similarly, we can only tell if bestEval is a UPPERBOUND if we have an alpha cutoff.
        if (bestEval >= beta) {
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

    omp_set_num_threads(numThreads);

    // Time management variables
    auto startTime = std::chrono::high_resolution_clock::now();
    hardDeadline = startTime + 2 * std::chrono::milliseconds(timeLimit);
    bool timeLimitExceeded = false;

    Move bestMove = Move(); 
    int bestEval = -INF;
    int color = board.sideToMove() == Color::WHITE ? 1 : -1;
    rootMoves = {};
    
    // Precompute late move reduction table
    precomputeLRM(100, 500); 

    // Update if the size for the transposition table changes.
    if (ttTable.size() != tableSize) {
        ttTable = std::vector<LockedTableEntry>(tableSize);
    }

    
    for (int i = 0; i < maxThreadsID; i++) {
        // Reset history scores 
        for (int j = 0; j < 64 * 64; j++) {
            history[i][0][j] = 0;
            history[i][1][j] = 0;

            captureHistory[i][0][j] = 0;
            captureHistory[i][1][j] = 0;
        }

        // Reset killer moves
        for (int j = 0; j < ENGINE_DEPTH; j++) {
            killer[i][j] = {Move::NO_MOVE, Move::NO_MOVE};
        }

        nodeCount[i] = 0;
        tableHit[i] = 0;

        // Make accumulators for each thread
        makeAccumulators(board, wAccumulator[i], bAccumulator[i], nnue);
    }

    std::vector<std::pair<Move, int>> moves;
    std::vector<int> evals (2 * ENGINE_DEPTH + 1, 0);
    
    // Syzygy tablebase probe
    Move syzygyMove;
    int wdl = 0;

    if (syzygy::probeSyzygy(board, syzygyMove, wdl)) {
        int score = 0;
        if (wdl == 1) {
            score = SZYZYGY_INF;
        } else if (wdl == -1) {
            score = -SZYZYGY_INF;
        }

        if (syzygyMove != Move::NO_MOVE) {
            std::cout << "info depth 0 score cp " << score 
                        << " nodes 0 time 0  pv " << uci::moveToUci(syzygyMove) << std::endl;
        }
        
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
    
    // Start the search
    int standPat = nnue.evaluate(wAccumulator[0], bAccumulator[0]);
    int depth = 0;

    while (depth <= std::min(ENGINE_DEPTH, maxDepth)) {
        globalMaxDepth = depth;

        // Track the best move for the current depth
        Move currentBestMove = Move();
        int currentBestEval = -INF;
        bool hashMoveFound = false;

        bool stopNow = false;
        int alpha = (depth > 6) ? evals[depth - 1] - 100 : -INF;
        int beta  = (depth > 6) ? evals[depth - 1] + 100 : INF;

        std::vector<std::pair<Move, int>> newMoves;
        std::vector<Move> PV; 
        
        if (depth == 0) {
            moves = orderedMoves(board, depth, 0, previousPV, false, Move::NO_MOVE, 0, hashMoveFound);
        }

        while (true) {
            currentBestEval = -INF;

            #pragma omp parallel for schedule(dynamic, 1)
            for (int i = 0; i < 3 * moves.size(); i++) {

                if (stopNow) continue; // Check if the time limit has been exceeded
                
                Move move = moves[i % moves.size()].first;
                std::vector<Move> childPV; 
                Board localBoard = board;
                staticEval[omp_get_thread_num()][0] = standPat;

                bool leftMost = (i == 0);
                int ply = 0;
                bool newBestFlag = false;  
                int threadID = omp_get_thread_num();
                int nextDepth = lateMoveReduction(localBoard, move, i % moves.size(), depth, 0, true, leftMost, threadID);
                int eval = -INF;

                NodeInfo childNodeInfo = {1, // ply of child node
                                        leftMost, // left most flag
                                        checkExtensions,
                                        singularExtensions,
                                        oneMoveExtensions,
                                        true, // NMP ok
                                        true, // singular search ok
                                        move, // no last move
                                        NodeType::PV, // root node is always a PV node
                                        threadID};
                
                addAccumulators(localBoard, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
                localBoard.makeMove(move);

                eval = -negamax(localBoard, nextDepth, -beta, -alpha, childPV, childNodeInfo);
                evalAdjust(eval);

                subtractAccumulators(localBoard, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
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

                if (newBestFlag && depth > 8 && nextDepth < depth - 1) {

                    addAccumulators(localBoard, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
                    localBoard.makeMove(move);

                    eval = -negamax(localBoard, depth - 1, -beta, -alpha, childPV, childNodeInfo);
                    evalAdjust(eval);

                    subtractAccumulators(localBoard, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
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
                        updatePV(PV, move, childPV);
                    } else if (eval == currentBestEval) {
                        // This is mostly for Syzygy tablebase.
                        // Prefer the move that is a capture or a pawn move.
                        if (localBoard.isCapture(move) || localBoard.at<Piece>(move.from()).type() == PieceType::PAWN) {
                            currentBestEval = eval;
                            currentBestMove = move;
                            updatePV(PV, move, childPV);
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
            depth++;
        } else {
            if (spendTooMuchTime || (depth >= 1 && rootMoves[depth] == rootMoves[depth - 1])) {
                break; // If we go beyond the hard limit or stabilize
            } 
            depth++; // Else, we can still search deeper
        }
    }

    return bestMove; 
}