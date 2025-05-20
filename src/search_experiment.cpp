#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <omp.h> 
#include <chrono>
#include <stdlib.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <mutex>

#include "nnue.hpp"
#include "../lib/fathom/src/tbprobe.h"
#include "search.hpp"
#include "chess_utils.hpp"
#include "utils.hpp"
#include "syzygy.hpp"
#include "chess.hpp"
#include "params.hpp"

using namespace chess;

// Aliases, constants, and engine parameters
typedef std::uint64_t U64;
constexpr int MAX_THREADS = 12;  // Maximum number of threads supported by the engine
constexpr int ENGINE_DEPTH = 99; // Maximum search depth supported by the engine
constexpr int MAX_ASPIRATION_SZ = 300;
constexpr int MAX_HIST = 9000;
constexpr int MAX_CAP_HIST = 3000;

int tableSize = 4194304; // Maximum size of the transposition table (default 256MB)
bool stopSearch = false; // To signal if the search should stop once the main thread is done

// Initalize NNUE, black and white accumulators
Network nnue;
std::vector<Accumulator> wAccumulator (MAX_THREADS); 
std::vector<Accumulator> bAccumulator (MAX_THREADS);

// Timer and statistics
std::chrono::time_point<std::chrono::high_resolution_clock> hardDeadline; 
std::vector<U64> nodeCount (MAX_THREADS); // Node count for each thread
std::vector<U64> tableHit (MAX_THREADS); // Table hit count for each thread

void initializeNNUE(std::string path) {
    std::cout << "Initializing NNUE from: " << path << std::endl;
    load_network(path, nnue);
}


std::vector<std::vector<std::vector<int>>> history(MAX_THREADS, std::vector<std::vector<int>>(2, std::vector<int>(64 * 64, 0)));
std::vector<std::vector<std::vector<int>>> captureHistory(MAX_THREADS, std::vector<std::vector<int>>(2, std::vector<int>(64 * 64, 0)));

// Evaluations along the current path
std::vector<std::vector<int>> staticEval(MAX_THREADS, std::vector<int>(ENGINE_DEPTH + 1, 0)); 

// complete[i] is set to true if the search at depth i is complete by one of the threads
std::vector<bool> completeDepth(ENGINE_DEPTH + 1, false); 

// Killer moves for each thread and ply
std::vector<std::vector<std::vector<Move>>> killer(MAX_THREADS, std::vector<std::vector<Move>> 
    (ENGINE_DEPTH + 1, std::vector<Move>(1, Move::NO_MOVE))); 

// Move stack for each thread
std::vector<std::vector<int>> moveStack(MAX_THREADS, std::vector<int>(ENGINE_DEPTH + 1, 0));

// LMR table 
std::vector<std::vector<int>> lmr_table; 

// Random seeds for LMR
std::vector<uint32_t> seeds(MAX_THREADS);

// tt entry definition
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

std::vector<LockedTableEntry> tt_table(tableSize); 

// Helper function declarations
void precompute_lmr(int max_depth, int max_i);
inline void update_history(Board&, Move, int, int);
bool tableLookUp(Board&, int&, int&, bool&, Move&, EntryType&, std::vector<LockedTableEntry>&);
void tableInsert(Board&, int, int, bool, Move, EntryType, std::vector<LockedTableEntry>&);
inline void updateKillerMoves(const Move&, int, int);
int see(Board&, Move, int);
int late_move_reduction(Board&, Move, int, int, int, bool, NodeType, int);
std::vector<std::pair<Move, int>> orderedMoves(Board&, int, std::vector<Move>&, bool, Move, int, bool&);
int quiescence(Board&, int, int, int, int);
inline void update_history(Board&, Move, int);

// Function definitions
void precompute_lmr(int max_depth, int max_i) {
    static bool is_precomputed = false;
    if (is_precomputed) return;

    lmr_table.resize(100 + 1, std::vector<int>(max_i + 1));

    for (int depth = max_depth; depth >= 1; --depth) {
        for (int i = max_i; i >= 1; --i) {
            lmr_table[depth][i] =  static_cast<int>(lmr1 + lmr2 * log(depth) * log(i));
        }
    }

    is_precomputed = true;
}

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

// Update killer moves 
inline void updateKillerMoves(const Move& move, int ply, int threadID) {
    killer[threadID][ply][0] = killer[threadID][ply][1];
    killer[threadID][ply][1] = move;
}

// Static exchange evaluation (SEE) function
int see(Board& board, Move move, int threadID) {
    int to = move.to().index();

    auto victim = board.at<Piece>(move.to());
    int victimValue = piece_type_value(victim.type());

    std::vector<int> values;
    values.push_back(victimValue);

    std::vector<Move> exchangesStack;
    exchangesStack.push_back(move);

    int depth = 0;
    Color currentSide = board.sideToMove();

    Board copy = board;
    while (!exchangesStack.empty()) {
        Move currentMove = exchangesStack.back();
        exchangesStack.pop_back();

        copy.makeMove(currentMove); // Make the capture
        Movelist captures;
        movegen::legalmoves<movegen::MoveGenType::CAPTURE>(captures, copy);

        std::vector<Move> nextCaptures;
        Move bestNextCapture = Move::NO_MOVE;
        int bestValue = INF;

        for (const Move& nextCapture : captures) {
            if (nextCapture.to().index() != to) continue; 
            
            int value = piece_type_value(copy.at<Piece>(nextCapture.from()).type());
            if (value < bestValue) {
                bestValue = value;
                bestNextCapture = nextCapture;
            }
        }

        if (bestNextCapture == Move::NO_MOVE) break;
        
        values.push_back(bestValue);
        exchangesStack.push_back(bestNextCapture);
    }

    int n = values.size();
    if (n == 0) return 0;

    int score = values[n - 1];
    for (int i = n - 2; i >= 0; --i) {
        score = values[i] - std::max(0, score);
    }

    return score;
}

// Late move reduction 
int late_move_reduction(Board& board, 
        Move move, 
        int i, 
        int depth, 
        int ply, 
        bool is_pv, 
        NodeType node_type,
        int thread_id) {

    if (is_mopup(board)) {
        return depth - 1;
    }

    bool stm = board.sideToMove() == Color::WHITE;
    bool isPromThreat = promotion_threat(board, move);

    if (i <= 1 || depth <= 3 || isPromThreat) {
        return depth - 1;
    } else {
        bool improving = ply >= 2 && staticEval[thread_id][ply - 2] < staticEval[thread_id][ply] && !board.inCheck();
        bool is_capture = board.isCapture(move);
        
        int R = lmr_table[depth][i];
        int tt_eval, tt_depth, history_score = history[thread_id][stm][move_index(move)];
        bool tt_is_pv, past_pv = false;
        EntryType tt_type;
        Move tt_move;
        
        if (tableLookUp(board, tt_depth, tt_eval, tt_is_pv, tt_move, tt_type, tt_table)) {
            past_pv = tt_is_pv; 
        }

        if (improving || is_pv  || past_pv || is_capture) {
            R--;
        }

        if (board.inCheck()) {
            R--;
        }

        if (history_score < -8000) R++;
        return std::min(depth - R, depth - 1);
    }
}

// generate ordered moves for the current position
std::vector<std::pair<Move, int>> orderedMoves(Board& board, int ply, int threadID,bool& hashMoveFound) {

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
        Move ttMove;
        EntryType ttType;
        TableEntry entry;
        int ttEval, ttDepth, priority = 0;
        bool ttIsPV;
        bool secondary = false;
        bool hashMove = false;

        if (tableLookUp(board, ttDepth, ttEval, ttIsPV, ttMove, ttType, tt_table)) {
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

        int previousMvIndex = ply > 0 ? moveStack[threadID][ply - 1] : -1;
        int currentMvIndex = move_index(move);

        if (is_promotion(move)) {                   
            priority = 16000; 
        } else if (board.isCapture(move)) { 
            int victimValue = piece_type_value(board.at<Piece>(move.to()).type());
            int attackerValue = piece_type_value(board.at<Piece>(move.from()).type());
            int score = captureHistory[threadID][stm][move_index(move)];
            priority = 4000 + victimValue + score;
        } else if (std::find(killer[threadID][ply].begin(), killer[threadID][ply].end(), move) != killer[threadID][ply].end()) {
            priority = 4000; // killer move
        } else {
            secondary = true;
            U64 mvIndex = move_index(move);
            priority = history[threadID][stm][mvIndex];
        } 

        if (!secondary) {
            primary.push_back({move, priority});
        } else {
            quiet.push_back({move, priority});
        }
    }

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

    // Stop the search if hard deadline is reached
    auto currentTime = std::chrono::high_resolution_clock::now();
    if (currentTime >= hardDeadline || stopSearch) {
        stopSearch = true;
        return 0;
    }

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
    bool mopUp = is_mopup(board);

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

    if (is_mopup(board)) {
        int color = (board.sideToMove() == Color::WHITE) ? 1 : -1;
        standPat = color * mopup_score(board);
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
        int seeScore = see(board, move, threadID);
        candidateMoves.push_back({move, seeScore});
    }

    std::sort(candidateMoves.begin(), candidateMoves.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    for (auto& [move, priority] : candidateMoves) {
        add_accumulators(board, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
        board.makeMove(move);
        
        int score = 0;
        score = -quiescence(board, -beta, -alpha, ply + 1, threadID);

        subtract_accumulators(board, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
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
int negamax(Board& board, int depth, int alpha, int beta, std::vector<Move>& PV, NodeData& data) {

    // Stop the search if hard deadline is reached
    auto currentTime = std::chrono::high_resolution_clock::now();
    if (currentTime >= hardDeadline || stopSearch) {
        stopSearch = true;
        return 0;
    }

    int threadID = data.threadID;
    int ply = data.ply;
    int rootDepth = data.rootDepth;

    std::vector<Move> badQuiets; // quiet moves that fail to raise alpha
    std::vector<Move> badCaptures; // bad tacticals (captures/promos) that fail to raise alpha

    // Extract whether we can do singular search and NMP
    bool nmpOk = data.nmpOk;

    // Extract node type and last move from nodeInfo
    NodeType nodeType = data.nodeType;

    nodeCount[threadID]++;
    bool mopUp = is_mopup(board);
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
    int ttEval, ttDepth, extensions = 0;
    bool ttIsPV = false;
    bool improving = ply >= 2 && staticEval[threadID][ply - 2] < staticEval[threadID][ply] && !board.inCheck();

    Move ttMove;
    EntryType ttType;
    TableEntry entry;
    bool ttHit = false;

    if (tableLookUp(board, ttDepth, ttEval, ttIsPV, ttMove, ttType, tt_table)) {
        tableHit[threadID]++;
        if (ttDepth >= depth) found = true;
        ttHit = true;
    }

    if (found && !isPV) {
        if (ttType == EntryType::EXACT
            || (ttType == EntryType::LOWERBOUND && ttEval >= beta)
            || (ttType == EntryType::UPPERBOUND && ttEval <= alpha)) {
            
            return ttEval;
        } 
    }
    
    if (found && isPV) {
        if ((ttType == EntryType::EXACT  || ttType == EntryType::LOWERBOUND) && ttEval >= beta) {
            return ttEval;
        } 
    }
    
    if (depth <= 0 && !board.inCheck()) {
        int qEval = quiescence(board, alpha, beta, ply + 1, threadID);
        eval_adjust(qEval);
        return qEval;
    } else if (depth <= 0) {
        return negamax(board, 1, alpha, beta, PV, data);
    }

    
    int standPat = 0;
    if (stm == 1) {
        standPat = nnue.evaluate(wAccumulator[threadID], bAccumulator[threadID]);
    } else {
        standPat = nnue.evaluate(bAccumulator[threadID], wAccumulator[threadID]);
    }

    // Use hash table's evaluation instead of NNUE if the position is found
    if (ttHit) {
        if (ttType == EntryType::EXACT 
            || (ttType == EntryType::LOWERBOUND && ttEval > standPat)
            || (ttType == EntryType::UPPERBOUND && ttEval < standPat)) {
            standPat = ttEval;
        }
    } 

    if (ttHit && ttDepth >= depth - 2 && (ttType == EntryType::EXACT || ttType == EntryType::LOWERBOUND)) {
        if (ttEval >= beta) {
            depth--;
        } 
    }
    
    staticEval[threadID][ply] = standPat; // store the evaluation along the path
    bool hashMoveFound = false;
    killer[threadID][ply + 1] = {Move::NO_MOVE, Move::NO_MOVE}; 
    
    std::vector<std::pair<Move, int>> moves = orderedMoves(board, 
                                                        ply, 
                                                        threadID,
                                                        hashMoveFound);
    
    seeds[threadID] = fast_rand(seeds[threadID]); 
    int R1 = seeds[threadID] % moves.size(); 
    seeds[threadID] = fast_rand(seeds[threadID]);
    int R2 = seeds[threadID] % moves.size();

    //bool captureTTMove = found && ttMove != Move::NO_MOVE && board.isCapture(ttMove);
    
    // Reverse futility pruning (RFP)
    bool rfpCondition = (depth <= rfpDepth) 
                        && !board.inCheck() 
                        && !isPV 
                        && !ttIsPV 
                        && abs(beta) < 10000
                        && !board.isCapture(ttMove);
    if (rfpCondition) {
        int rfpMargin = rfpC1 + rfpC2 * depth + rfpC3 * (1 - improving);
        if (standPat >= beta + rfpMargin) {
            return (standPat + beta) / 2;
        }
    }

    // Null move pruning. Side to move must have non-pawn material.
    const int nullDepth = 4; 
    bool nmpCondition = (depth >= nullDepth 
        && non_pawn_material(board) 
        && !board.inCheck() 
        && !mopUp 
        && !isPV
        && standPat >= beta
        && nmpOk);

    if (nmpCondition) {
            
        std::vector<Move> nullPV; // dummy PV
        int nullEval;
        int reduction = 3;

        NodeData nullData = {ply + 1, 
                                false, 
                                rootDepth,
                                NodeType::ALL, // expected all node
                                threadID};
        moveStack[threadID][ply] = -1;
        board.makeNullMove();
        nullPV.push_back(Move::NULL_MOVE);
        nullEval = -negamax(board, depth - reduction, -beta, -(beta - 1), nullPV, nullData);
        eval_adjust(nullEval);
        board.unmakeNullMove();

        if (nullEval >= beta) {
            return beta;
        } else if (nullEval < -INF/2 + 5) {
            // threat extensions
            return beta - 1;
        }
    }

    int bestEval = -INF;

    // Simplified version of IID. Reduce the depth to facilitate the search if no hash move found.
    if (!hashMoveFound && depth >= 4) {
        depth = depth - 1;
    }

    // Singular extension. If the hash move is stronger than all others, extend the search.
    if (hashMoveFound && ttDepth >= depth - 3
        && depth > 4
        && ttType != EntryType::UPPERBOUND
        && abs(ttEval) < INF/2 - 100) {

        int sEval = -INF;
        int sBeta = ttEval - singularC1 * depth - singularC2; 

        for (int i = 0; i < moves.size(); i++) {
            if (moves[i].first == ttMove) 
                continue; 

            add_accumulators(board, moves[i].first, wAccumulator[threadID], bAccumulator[threadID], nnue);
            moveStack[threadID][ply] = move_index(moves[i].first);
            board.makeMove(moves[i].first);

            NodeData childNodeData = {ply + 1, 
                                    false, 
                                    rootDepth,
                                    NodeType::PV,
                                    threadID};


            sEval = std::max(sEval, -negamax(board, (depth - 1) / 2, -sBeta, -sBeta + 1, PV, childNodeData));
            eval_adjust(sEval);

            subtract_accumulators(board, moves[i].first, wAccumulator[threadID], bAccumulator[threadID], nnue);
            board.unmakeMove(moves[i].first);

            if (sEval >= sBeta) {
                break;
            } 
        }

        if (sEval < sBeta) {
            extensions++; // If one move is much better than the others, we extend the search.
            if (sEval < sBeta - 40) {
                extensions++; // Double extension
            }
        } else if (sBeta >= beta && !isPV) {
            return sBeta; // Multicut
        }
    }

    if (board.inCheck() && std::abs(standPat) > 75) {
        extensions++;
    }

    if (moves.size() == 1) {
        extensions++;
    }
    
    extensions = std::clamp(extensions, 0, 2); // limit extensions to 2 per ply

    // Evaluate moves
    for (int i = 0; i < moves.size(); i++) {

        Move move = moves[i].first;
        std::vector<Move> childPV;
        
        bool isPromo = is_promotion(move);
        bool inCheck = board.inCheck();
        bool isCapture = board.isCapture(move);
        bool isPromoThreat = promotion_threat(board, move);

        bool isPawnPush = board.at<Piece>(move.from()).type() == PieceType::PAWN;

        board.makeMove(move);
        bool giveCheck = board.inCheck();
        board.unmakeMove(move);

        int eval = 0;
        int nextDepth = late_move_reduction(board, move, i, depth, ply, isPV, nodeType, threadID); 

        nextDepth = std::min(nextDepth + extensions, (rootDepth) - ply - 1);

        // common conditions for pruning
        bool canPrune = !inCheck && !isPawnPush && i > 0;

        // Futility  pruning
        bool fpCondition = canPrune && !isCapture && !giveCheck && !isPV && nextDepth <= fpDepth && !ttIsPV;
        if (fpCondition) {
            int margin = fpC1 + fpC2 * nextDepth + fpC3 * improving;
            if (standPat + margin < alpha) {
                continue;
            }
        }

        // Further reduction for quiet moves
        bool lmpCondition = canPrune && !isPV && !isCapture && nextDepth <= lmpDepth;
        if (lmpCondition) {
            int divisor = improving ? 1 : 2;
            if (i >= (lmpC1 + nextDepth * nextDepth) / divisor) {
                nextDepth--;
            }
        }

        // Further reduction for quiet moves with lower history scores
        bool hpCondition = canPrune && !isPV && !isCapture  && nextDepth <= hpDepth && !ttIsPV;
        if (hpCondition) {
            int margin = -(hpC1 + hpC2 * nextDepth + hpC3 * improving);
            int historyScore = history[threadID][stm][move_index(move)];
            if (historyScore < margin) {
                continue;
            }
        }
    
        add_accumulators(board, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
        moveStack[threadID][ply] = move_index(move);
        board.makeMove(move);
        
        bool nullWindow = false;
        bool reducedDepth = nextDepth < depth - 1;

        NodeData childNodeData = {ply + 1, 
                                nmpOk,
                                rootDepth,
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
            childNodeData.nodeType = childNodeType;
            eval = -negamax(board, nextDepth, -beta, -alpha, childPV, childNodeData);
            eval_adjust(eval);
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
            childNodeData.nodeType = childNodeType;

            eval = -negamax(board, nextDepth, -(alpha + 1), -alpha, childPV, childNodeData);
            eval_adjust(eval);
        }
        
        subtract_accumulators(board, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
        board.unmakeMove(move);
    
        // If we raised alpha in a null window search or reduced depth search, re-search with full window and full depth.
        // We don't need to do this for non-PV nodes because when beta = alpha + 1, the full window is the same as the null window.
        // Furthermore, if we are in a non-PV node and a reduced depth search raised alpha, then we will need to 
        // re-search with full window and full depth in some ancestor node anyway so there is no need to do it here. 
        if ((eval > alpha) && (nullWindow || reducedDepth) && isPV) {

            // Now this child becomes a PV node.
            childNodeData.nodeType = NodeType::PV;

            add_accumulators(board, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
            moveStack[threadID][ply] = move_index(move);
            board.makeMove(move);

            eval = -negamax(board, depth - 1, -beta, -alpha, childPV, childNodeData);
            eval_adjust(eval);

            subtract_accumulators(board, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
            board.unmakeMove(move);
        }

        if (eval > bestEval) {
            bestEval = eval;
            if (bestEval > alpha) {
                alpha = bestEval;
                update_pv(PV, move, childPV);
            }
        }

        if (eval <= alpha) {
            if (isCapture) {
                badCaptures.push_back(move);
            } else {
                badQuiets.push_back(move);
            }
        }

        // Beta cutoff.
        if (beta <= alpha) {
            


            int mvIndex = move_index(move);
            int currentScore = isCapture ? 
                                captureHistory[threadID][stm][mvIndex] : 
                                history[threadID][stm][mvIndex];
            
            int limit = isCapture ? MAX_CAP_HIST : MAX_HIST;
            int delta = (1.0 - static_cast<float>(std::abs(currentScore)) / static_cast<float>(limit)) * depth * depth;

            // Update history scores for the move that caused the cutoff and the previous moves that failed to cutoffs.
            if (!isCapture) {
                updateKillerMoves(move, ply, threadID);
                history[threadID][stm][mvIndex] += delta;
                history[threadID][stm][mvIndex] = std::clamp(history[threadID][stm][mvIndex], -MAX_HIST, MAX_HIST);

                // penalize bad quiet moves
                for (auto& badMv : badQuiets) {
                    int badMvIndex = move_index(badMv);
                    history[threadID][stm][badMvIndex] -= delta;
                    history[threadID][stm][badMvIndex] = std::clamp(history[threadID][stm][badMvIndex], -MAX_HIST, MAX_HIST);
                }
            } else {
                captureHistory[threadID][stm][mvIndex] += delta;
                captureHistory[threadID][stm][mvIndex] = std::clamp(captureHistory[threadID][stm][mvIndex], -MAX_CAP_HIST, MAX_CAP_HIST);
            }

            for (auto& badCap : badCaptures) {
                int badMvIndex = move_index(badCap);
                captureHistory[threadID][stm][badMvIndex] -= delta;
                captureHistory[threadID][stm][badMvIndex] = std::clamp(captureHistory[threadID][stm][badMvIndex], -MAX_CAP_HIST, MAX_CAP_HIST);
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

        if (bestEval > alpha0 && bestEval < beta) {
            type = EXACT;
        } else if (bestEval <= alpha0) {
            type = UPPERBOUND;
        } else {
            type = LOWERBOUND;
        } 

        if (PV.size() > 0) {
            tableInsert(board, depth, bestEval, true, PV[0], type, tt_table);
        } else {
            tableInsert(board, depth, bestEval, true, Move::NO_MOVE, type, tt_table);
        }

    } else {
        // For non-PV nodes:
        // alpha is artifical for CUT nodes and beta is artifical for ALL nodes.
        // In CUT nodes, we have a fake alpha. We can only tell if bestEval is a LOWERBOUND if we have a beta cutoff.
        // IN ALL nodess, we have a fake beta. Similarly, we can only tell if bestEval is a UPPERBOUND if we have an alpha cutoff.
        if (bestEval >= beta) {
            EntryType type = LOWERBOUND;
            if (PV.size() > 0) {
                tableInsert(board, depth, bestEval, false, PV[0], type, tt_table);
            } else {
                tableInsert(board, depth, bestEval, false, Move::NO_MOVE, type, tt_table);
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
std::tuple<Move, int, int, std::vector<Move>> rootSearch(Board& board, int maxDepth = 30, int timeLimit = 15000, int threadID = 0) {

    // Time management variables
    auto startTime = std::chrono::high_resolution_clock::now();
    hardDeadline = startTime + 2 * std::chrono::milliseconds(timeLimit);
    bool timeLimitExceeded = false;

    int bestEval = -INF;
    int color = board.sideToMove() == Color::WHITE ? 1 : -1;

    std::vector<Move> rootMoves (ENGINE_DEPTH + 1, Move::NO_MOVE);
    std::vector<int> evals (2 * ENGINE_DEPTH + 1, 0);
    std::vector<std::pair<Move, int>> moves;

    Move bestMove = Move(); 
    Move syzygyMove;

    // Syzygy tablebase probe
    int wdl = 0;
    if (syzygy::probeSyzygy(board, syzygyMove, wdl)) {
        int score = 0;
        if (wdl == 1) {
            score = SZYZYGY_INF;
        } else if (wdl == -1) {
            score = -SZYZYGY_INF;
        }

        if (syzygyMove != Move::NO_MOVE && threadID == 0) {
            std::cout << "info depth 0 score " << score  << " cp "
                        << " nodes 0 time 0  pv " << uci::moveToUci(syzygyMove) << std::endl;
        }
        
        if (syzygyMove != Move::NO_MOVE) {
            try {
                Board boardCopy = board;
                boardCopy.makeMove(syzygyMove);
                return {syzygyMove, 0, score, {syzygyMove}};
            } catch (const std::exception&) {
                // In case somehow the move is invalid, continue with the search
            }
        }
    }
    
    // Start the search
    int standPat = nnue.evaluate(wAccumulator[threadID], bAccumulator[threadID]);
    int depth = 0;
    std::vector<Move> PV; 

    while (depth <= std::min(ENGINE_DEPTH, maxDepth)) {
        Move currBestMove = Move(); // Track the best move for the current depth
        int currBestEval = -INF;
        bool hashMoveFound = false;

        // Aspiration window
        int window = 150;
        int alpha = (depth > 6) ? evals[depth - 1] - window : -INF;
        int beta  = (depth > 6) ? evals[depth - 1] + window : INF;
        
        std::vector<std::pair<Move, int>> newMoves;
        
        
        if (depth == 0) {
            moves = orderedMoves(board, 0, 0, hashMoveFound);
        }

        while (true) {
            currBestEval = -INF;
            int alpha0 = alpha;
            newMoves.clear();
            std::vector<Move> currPV;
            
            for (int i = 0; i < moves.size(); i++) {

                Move move = moves[i].first;
                std::vector<Move> childPV; 
                Board localBoard = board;
                staticEval[threadID][0] = standPat;

                int ply = 0;
                int nextDepth = late_move_reduction(localBoard, move, i, depth, 0, true, NodeType::PV, threadID);
                int eval = -INF;

                NodeData childNodeData = {1, // ply of child node
                                        true, // NMP ok
                                        depth, // root depth
                                        NodeType::PV, // child of a root node is a PV node
                                        threadID};
                
                add_accumulators(localBoard, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
                moveStack[threadID][ply] = move_index(move);
                localBoard.makeMove(move);

                eval = -negamax(localBoard, nextDepth, -beta, -alpha, childPV, childNodeData);
                eval_adjust(eval);

                subtract_accumulators(localBoard, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
                localBoard.unmakeMove(move);

                // Check for stop search flag
                if (stopSearch) {
                    return {bestMove, depth - 1, bestEval, PV};
                }

                if (eval > currBestEval && nextDepth < depth - 1) {
                    // Re-search with full depth if we have a new best move
                    add_accumulators(localBoard, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
                    moveStack[threadID][ply] = move_index(move);
                    localBoard.makeMove(move);

                    eval = -negamax(localBoard, depth - 1, -beta, -alpha, childPV, childNodeData);
                    eval_adjust(eval);

                    subtract_accumulators(localBoard, move, wAccumulator[threadID], bAccumulator[threadID], nnue);
                    localBoard.unmakeMove(move);

                    if (stopSearch) {
                        return {bestMove, depth - 1, bestEval, PV};
                    }
                }

                newMoves.push_back({move, eval}); // Store the move and its evaluation

                // If found the new best move
                if (eval > currBestEval) {
                    currBestEval = eval;
                    currBestMove = move;
                    alpha = std::max(alpha, currBestEval);
                    update_pv(currPV, move, childPV);
                } 
                
                if (alpha >= beta) {
                    break;
                }
            }

            if (currBestEval <= alpha0 || currBestEval >= beta) {
                alpha = -INF;
                beta = INF;
                newMoves.clear();
            } else {
                PV = currPV;
                break;
            }
        }
        
        // Update the global best move and evaluation after this depth if the time limit is not exceeded
        bestMove = currBestMove;
        bestEval = currBestEval;
        

        // Sort the moves by evaluation for the next iteration
        std::sort(newMoves.begin(), newMoves.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });

        tableInsert(board, depth, bestEval, true, bestMove, EntryType::EXACT, tt_table);

        moves = newMoves;

        U64 totalNodeCount = 0, totalTableHit = 0;
        for (int i = 0; i < MAX_THREADS; i++) {
            totalNodeCount += nodeCount[i];
            totalTableHit += tableHit[i];
        }
    
        if (threadID == 0){
            // Only print the analysis for the first thread to avoid clutter 
            std::string analysis = format_analysis(depth, bestEval, totalNodeCount, totalTableHit, startTime, PV, board);
            std::cout << analysis << std::endl;
        }

        
        if (moves.size() == 1) {
            return {moves[0].first, 0, standPat, {moves[0].first}}; // If there is only one move, return it immediately.
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
            break; // If two consecutive depths found mate, stop searching.
        }
        
        if (!timeLimitExceeded) {
            depth++; // If the time limit is not exceeded, we can search deeper.
        } else {
            if (spendTooMuchTime || (depth >= 1 && rootMoves[depth] == rootMoves[depth - 1] && depth >= 14)) {
                break; // If we go beyond the hard limit or stabilize.
            } 
            depth++; 
        }
    }

    return {bestMove, depth, bestEval, PV};
}

Move lazysmpRootSearch(Board &board, int numThreads, int maxDepth, int timeLimit) {
    
    precompute_lmr(100, 500);  // Precompute late move reduction table
    omp_set_num_threads(numThreads); // Set the number of threads for OpenMP
    Move bestMove = Move(); 

    stopSearch = false;
    auto startTime = std::chrono::high_resolution_clock::now();

    // Update if the size for the transposition table changes.
    if (tt_table.size() != tableSize) {
        tt_table = std::vector<LockedTableEntry>(tableSize);
    }
    
    for (int i = 0; i < MAX_THREADS; i++) {
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
        seeds[i] = rand();

        // Make accumulators for each thread
        make_accumulators(board, wAccumulator[i], bAccumulator[i], nnue);
    }

    int depth = -1;
    int eval = -INF;
    std::vector<Move> PV;

    #pragma omp parallel for schedule (static, 1)
    for (int i = 0; i < numThreads; i++) {
        Board localBoard = board;
        auto [threadMove, threadDepth, threadEval, threadPV] = rootSearch(localBoard, maxDepth, timeLimit, i);
        if (i == 0) { 
            // Get the result from the thread with the highest depth without extensions
            depth = threadDepth;
            bestMove = threadMove; 
            eval = threadEval; 
            PV = threadPV; 
            stopSearch = true; // Stop all threads
        }
    }

    // Print the final analysis
    int totalNodeCount = 0;
    int totalTableHit = 0;
    for (int i = 0; i < numThreads; i++) {
        totalNodeCount += nodeCount[i];
        totalTableHit += tableHit[i];
    }
    
    std::string analysis = format_analysis(depth, eval, totalNodeCount, totalTableHit, startTime, PV, board);
    std::cout << analysis << std::endl;
    return bestMove; 
}