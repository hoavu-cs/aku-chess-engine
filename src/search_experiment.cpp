
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
#include "chess_utils.hpp"
#include "utils.hpp"
#include "syzygy.hpp"
#include "chess.hpp"

using namespace chess;

typedef std::uint64_t U64;
const int maxThreadsID = 50; 


// Initalize NNUE
Network nnue;

void initializeNNUE(std::string path) {
    std::cout << "Initializing NNUE from: " << path << std::endl;
    loadNetwork(path, nnue);
}

std::vector<Accumulator> wAccumulator (maxThreadsID);
std::vector<Accumulator> bAccumulator (maxThreadsID);

// Precompute the LMR table
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

// Transposition table lookup and insert.
int tableSize = 4194304; // Maximum size of the transposition table (default 256MB)
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
    bool pv; // this flag is used to check if the position is or was a PV node
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
    bool& pv,
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
        pv = lockedEntry.entry.pv;
        bestMove = lockedEntry.entry.bestMove;
        type = lockedEntry.entry.type;
        return true;
    }

    return false;
}

void tableInsert(Board& board, 
    int depth, 
    int eval, 
    bool pv,
    Move bestMove, 
    EntryType type,
    std::vector<LockedTableEntry>& table) {

    U64 hash = board.hash();
    U64 index = hash % table.size();
    LockedTableEntry& lockedEntry = table[index];

    if (lockedEntry.entry.hash == hash && lockedEntry.entry.pv) {
        pv = true; // don't overwrite the pv node if it was set
    }
        
    std::lock_guard<std::mutex> lock(lockedEntry.mtx); 
    lockedEntry.entry = {hash, eval, depth, pv, bestMove, type}; 
}

// Other global variables.
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

// Inspired by Glaurung:
// We avoid pruning quiet moves that have more historical successes than failures.
// - A "success" is counted when a quiet move causes a beta cutoff.
// - A "failure" is counted when the move is tried but fails to cause a cutoff,
//   and a later move in the list does cause one.
std::vector<std::vector<std::vector<int>>> success(maxThreadsID, std::vector<std::vector<int>>(2, std::vector<int>(64 * 64, 0)));
std::vector<std::vector<std::vector<int>>> failure(maxThreadsID, std::vector<std::vector<int>>(2, std::vector<int>(64 * 64, 0)));

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

// Update killer moves 
inline void updateKillerMoves(const Move& move, int ply, int threadID) {
    killer[threadID][ply][0] = killer[threadID][ply][1];
    killer[threadID][ply][1] = move;
}

// Static exchange evaluation (SEE) function
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

// Late move reduction 
int lrm(Board& board, 
                      Move move, 
                      int i, 
                      int depth, 
                      int ply, 
                      bool isPV, 
                      bool leftMost, 
                      int threadID) {

    if (isMopUpPhase(board)) {
        return depth - 1;
    }

    bool stm = board.sideToMove() == Color::WHITE;

    if (i <= 1 || depth <= 3) {
        return depth - 1;
    } else {
        bool improving = (ply >= 2 && staticEval[threadID][ply - 2] < staticEval[threadID][ply]);
        bool isCapture = board.isCapture(move);
    
        bool isKiller = std::find(killer[threadID][ply].begin(), killer[threadID][ply].end(), move) != killer[threadID][ply].end();
        
        int ttEval, ttDepth;
        bool ttIsPV, hashMoveFound, pastPV = false;
        EntryType ttType;
        Move ttMove;
        TableEntry entry;

        if (tableLookUp(board, ttDepth, ttEval, ttIsPV, ttMove, ttType, ttTable)) {
            pastPV = ttIsPV;
        }

        int R = lmrTable1[depth][i];

        if (!isCapture) {
            int histScore = history[threadID][stm][moveIndex(move)];
            if (histScore < historyLMR) {
                R++;
            }
        }

        if (improving || board.inCheck() || isPV || isKiller || isCapture || pastPV) {
            R--;
        }

        return std::min(depth - R, depth - 1);
    }
}

// generate ordered moves for the current position
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
        int ttEval, ttDepth;
        bool ttIsPV;
        EntryType ttType;
        TableEntry entry;

        if (tableLookUp(board, ttDepth, ttEval, ttIsPV, ttMove, ttType, ttTable)) {
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

// Quiescence search 
int quiescence(Board& board, int alpha, int beta, int ply, int threadID) {

    // Check if the game is over. 
    auto gameOverResult = board.isGameOver();
    if (gameOverResult.first != GameResultReason::NONE) {
        if (gameOverResult.first == GameResultReason::CHECKMATE) {
            return -INF/2; 
        }
        return 0;
    }
    
    nodeCount[threadID]++;
    bool stm = (board.sideToMove() == Color::WHITE);
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
        int color = (board.sideToMove() == Color::WHITE) ? 1 : -1;
        standPat = color * mopUpScore(board);
    } else {
        if (stm == 1) {
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

// Negamax main search function
int negamax(Board& board, int depth, int alpha, int beta, std::vector<Move>& PV, NodeInfo& nodeInfo) {

    // Stop the search if hard deadline is reached
    auto currentTime = std::chrono::high_resolution_clock::now();
    if (currentTime >= hardDeadline) {
        return 0;
    }

    int threadID = nodeInfo.threadID;
    bool leftMost = nodeInfo.leftMost;
    int ply = nodeInfo.ply;
    int rootDepth = nodeInfo.rootDepth;

    // Extract whether we can do singular search and NMP
    bool singularSearchOk = nodeInfo.singularSearchOk;
    bool nmpOk = nodeInfo.nmpOk;

    // Extract node type and last move from nodeInfo
    NodeType nodeType = nodeInfo.nodeType;
    Move lastMove = nodeInfo.lastMove;

    nodeCount[threadID]++;
    bool mopUp = isMopUpPhase(board);
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
    bool ttIsPV = false;
    Move ttMove;
    EntryType ttType;
    TableEntry entry;

    if (tableLookUp(board, ttDepth, ttEval, ttIsPV, ttMove, ttType, ttTable)) {
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
        int qEval = quiescence(board, alpha, beta, ply + 1, threadID);
        evalAdjust(qEval);
        return qEval;
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

    // Reverse futility pruning (RFP)
    bool rfpCondition = depth <= 4 && !board.inCheck() && !isPV && !ttIsPV && abs(beta) < 10000;
    if (rfpCondition) {
        int rfpMargin = 512 + 32 * (depth - 2);
        if (standPat - rfpMargin > beta) {
            return (standPat + beta)  / 2;
        } 
    } 

    // Null move pruning. We allow recursive NMP but not 2 consecutive Null moves.
    // Side to move must have non-pawn material.
    const int nullDepth = 4; 
    bool nmpCondition = (depth >= nullDepth 
        && nonPawnMaterial(board) 
        && !leftMost 
        && !board.inCheck() 
        && !mopUp 
        && !isPV
        && standPat >= beta - 768
        && lastMove != Move::NULL_MOVE
        && nmpOk);

    if (nmpCondition) {
            
        std::vector<Move> nullPV; // dummy PV
        int nullEval;
        int reduction = 4;

        NodeInfo nullNodeInfo = {ply + 1, 
                                false, 
                                true, 
                                singularSearchOk,
                                rootDepth,
                                Move::NULL_MOVE,
                                NodeType::ALL, // expected all node
                                threadID};

        board.makeNullMove();
        nullEval = -negamax(board, depth - reduction, -beta, -(beta - 1), nullPV, nullNodeInfo);
        evalAdjust(nullEval);
        board.unmakeNullMove();

        if (nullEval >= beta) {
            return beta;
        } else if (nullEval < -INF/2 + 5) {
            // If the null move fails low, we may be faced with a threat.
            // Return beta - 1 to signal that we need to re-search
            return beta - 1;
        }
    }


    int bestEval = -INF;
    int extensions = 0;
    bool searchAllFlag = false;

    // Simplified version of IID.
    // Reduce the depth to facilitate the search if no hash move found.
    // Restricted to expected cut nodes and depth > 3.
    if (!hashMoveFound && depth >= 4 && (nodeType == NodeType::CUT || nodeType == NodeType::PV)) {
        depth = depth - 1;
    }

    // Singular extension.
    // If the hash move is stronger than all others, extend the search.
    if (hashMoveFound && ttDepth >= depth - 3
                        && depth >= singularDepth
                        && ttType != EntryType::UPPERBOUND
                        && isPV
                        && singularSearchOk
                        && abs(ttEval) < INF/2 - 100) {

        int bestSEval = -INF;
        int sBeta = ttEval - 2 * depth;

        for (int i = 0; i < moves.size(); i++) {
            
            if (moves[i].first == ttMove) 
                continue; 
            
            addAccumulators(board, moves[i].first, wAccumulator[threadID], bAccumulator[threadID], nnue);
            board.makeMove(moves[i].first);

            NodeInfo childNodeInfo = {ply + 1, 
                                        leftMost, 
                                        false, // turn off NMP for singular search
                                        false, // turn off singular search for this path
                                        rootDepth,
                                        moves[i].first,
                                        NodeType::PV,
                                        threadID};

            
            int sEval = -negamax(board, (depth - 1)/2, -sBeta, -sBeta + 1, PV, childNodeInfo);
            evalAdjust(sEval);

            subtractAccumulators(board, moves[i].first, wAccumulator[threadID], bAccumulator[threadID], nnue);
            board.unmakeMove(moves[i].first);

            bestSEval = std::max(bestSEval, sEval);
            if (bestSEval >= sBeta) break;
        }
        
        if (bestSEval < sBeta) { 
            extensions++; // singular extension
        }
    }

    if (board.inCheck()) {
        extensions++;
    }
    if (moves.size() == 1) {
        extensions++;
    }
    
    extensions = std::clamp(extensions, 0, 1); // limit extensions to 2

    // Evaluate moves
    for (int i = 0; i < moves.size(); i++) {

        Move move = moves[i].first;
        std::vector<Move> childPV;
        
        bool isPromo = isPromotion(move);
        bool inCheck = board.inCheck();
        bool isCapture = board.isCapture(move);
        bool isPromoThreat = promotionThreat(board, move);

        bool isPawnPush = board.at<Piece>(move.from()).type() == PieceType::PAWN;

        board.makeMove(move);
        bool giveCheck = board.inCheck();
        board.unmakeMove(move);

        if (i > 0) leftMost = false;
        int eval = 0;
        int nextDepth = lrm(board, move, i, depth, ply, isPV, leftMost, threadID); 

        nextDepth = std::min(nextDepth + extensions, (3 + rootDepth) - ply - 1);

        // common conditions for pruning
        bool goodHistory = success[threadID][stm][moveIndex(move)] >= failure[threadID][stm][moveIndex(move)];
        bool canPrune = !inCheck && !isPawnPush && !goodHistory;
        
        // Late move pruning
        bool lmpCondition = canPrune && extensions == 0 && !isCapture && nextDepth <= 6;
        if (i >= 5 + depth && lmpCondition) {
            continue; 
        }

        // History pruning       
        // bool hpCondition = canPrune && !isCapture && !isPV && i > 0 && nextDepth <= 4;
        // if (hpCondition) {
        //     int mvIndex = moveIndex(move);
        //     if (history[threadID][stm][mvIndex] < -histC0 - histC1 * nextDepth) {
        //         continue;
        //     } 
        // }

        // SEE pruning
        // bool seeCondition = canPrune && !isPV && isCapture && i > 0 && nextDepth <= seeDepth;
        // if (seeCondition) {
        //     int seeScore = see(board, move, threadID);
        //     if (seeScore < -seeC1 * nextDepth) {
        //         continue;
        //     }
        // }

        // Futility pruning
        bool fpCondition = canPrune &&
                   !isCapture &&
                   !giveCheck &&
                   nextDepth <= 6 &&
                   i > 0; 

        if (fpCondition) {
            //int margin = (fpC0 + fpC1 * depth + fpImprovingC * improving);
            int margin = (nextDepth == 1)
                    ? 256
                    : 512 + 32 * (nextDepth - 2);

            if (standPat + margin < alpha)
                continue; // prune move
        }


            

        addAccumulators(board, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
        board.makeMove(move);
        bool nullWindow = false;
        bool reducedDepth = nextDepth < depth - 1;

        NodeInfo childNodeInfo = {ply + 1, 
                                leftMost, 
                                nmpOk,
                                singularSearchOk,
                                rootDepth,
                                move,
                                NodeType::PV,
                                threadID};

  
        
        // PVS: Full window for the first node. 
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
                success[threadID][stm][mvIndex]++;

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
                    failure[threadID][stm][mvIndex]++;
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
            tableInsert(board, depth, bestEval, true, PV[0], type, ttTable);
        } else {
            tableInsert(board, depth, bestEval, true, Move::NO_MOVE, type, ttTable);
        }

    } else {
        // For non-PV nodes:
        // alpha is artifical for CUT nodes and beta is artifical for ALL nodes.
        // In CUT nodes, we have a fake alpha. We can only tell if bestEval is a LOWERBOUND if we have a beta cutoff.
        // IN ALL nodess, we have a fake beta. Similarly, we can only tell if bestEval is a UPPERBOUND if we have an alpha cutoff.
        if (bestEval >= beta) {
            EntryType type = LOWERBOUND;
            if (PV.size() > 0) {
                tableInsert(board, depth, bestEval, false, PV[0], type, ttTable);
            } else {
                tableInsert(board, depth, bestEval, false, Move::NO_MOVE, type, ttTable);
            }  
        } 

    }
    
    return bestEval;
}

//     Root search function to communicate with UCI interface. 
//     Time control: 
//     Hard deadline: 2x time limit
//     - Case 1: As long as we are within the time limit, we search as deep as we can.
//     - Case 2: Stop if we reach the hard deadline or certain depth.
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

            success[i][0][j] = 0;
            success[i][1][j] = 0;

            failure[i][0][j] = 0;
            failure[i][1][j] = 0;
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
            for (int i = 0; i < 6 * moves.size(); i++) {

                if (stopNow) continue; // Check if the time limit has been exceeded
                
                Move move = moves[i % moves.size()].first;
                std::vector<Move> childPV; 
                Board localBoard = board;
                staticEval[omp_get_thread_num()][0] = standPat;

                bool leftMost = (i == 0);
                int ply = 0;
                bool newBestFlag = false;  
                int threadID = omp_get_thread_num();
                int nextDepth = lrm(localBoard, move, i % moves.size(), depth, 0, true, leftMost, threadID);
                int eval = -INF;

                NodeInfo childNodeInfo = {1, // ply of child node
                                        leftMost, // left most flag
                                        true, // NMP ok
                                        true, // singular search ok
                                        nextDepth, // root depth
                                        move, 
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

            if (currentBestEval <= alpha || currentBestEval >= beta) {
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

        tableInsert(board, depth, bestEval, true, bestMove, EntryType::EXACT, ttTable);

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

        if (depth >= 6 
            && abs(evals[depth - 1]) >= INF/2 - 100 
            && abs(evals[depth]) >= INF/2 - 100) {
            break; // If we are in a forced mate sequence, we can stop the search.
        }
        
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