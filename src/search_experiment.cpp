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

int table_size = 4194304; // Maximum size of the transposition table (default 256MB)
bool stop_search = false; // To signal if the search should stop once the main thread is done


// Initalize NNUE, black and white accumulators
Network nnue;
std::vector<Accumulator> white_accumulator (MAX_THREADS); 
std::vector<Accumulator> black_accumulator (MAX_THREADS);

// Timer and statistics
std::chrono::time_point<std::chrono::high_resolution_clock> hard_deadline; 
std::vector<U64> node_count (MAX_THREADS); // Node count for each thread
std::vector<U64> table_hit (MAX_THREADS); // Table hit count for each thread

void initializeNNUE(std::string path) {
    std::cout << "Initializing NNUE from: " << path << std::endl;
    load_network(path, nnue);
}

// History scores for quiet moves
std::vector<std::vector<std::vector<int>>> history(MAX_THREADS, std::vector<std::vector<int>>(2, std::vector<int>(64 * 64, 0)));

// Evaluations along the current path
std::vector<std::vector<int>> static_eval(MAX_THREADS, std::vector<int>(ENGINE_DEPTH + 1, 0)); 

// Killer moves for each thread and ply
std::vector<std::vector<std::vector<Move>>> killer(MAX_THREADS, std::vector<std::vector<Move>> 
    (ENGINE_DEPTH + 1, std::vector<Move>(1, Move::NO_MOVE))); 

// Move stack for each thread
std::vector<std::vector<int>> move_stack(MAX_THREADS, std::vector<int>(ENGINE_DEPTH + 1, 0));

// LMR table 
std::vector<std::vector<int>> lmr_table; 

// Random seeds for LMR
std::vector<uint32_t> seeds(MAX_THREADS);

// Misra-Gries instead of counter moves
std::vector<MisraGriesIntInt> mg_2ply(MAX_THREADS, MisraGriesIntInt(500));  

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
    Move best_move;
    EntryType type;
};

struct LockedTableEntry {
    std::mutex mtx;
    TableEntry entry;
};

std::vector<LockedTableEntry> tt_table(table_size); 

// Helper function declarations
void precompute_lmr(int max_depth, int max_i);
inline void update_history(Board&, Move, int, int);
bool table_lookup(Board&, int&, int&, bool&, Move&, EntryType&, std::vector<LockedTableEntry>&);
void table_insert(Board&, int, int, bool, Move, EntryType, std::vector<LockedTableEntry>&);
inline void update_killers(const Move&, int, int);
int see(Board&, Move);
int late_move_reduction(Board&, Move, int, int, int, bool, NodeType, int);
std::vector<std::pair<Move, int>> order_move(Board&, int, std::vector<Move>&, bool, Move, int, bool&);
int quiescence(Board&, int, int, int, int);
inline void update_history(Board&, Move, int);

// Function definitions
void precompute_lmr(int max_depth, int max_i) {
    static bool is_precomputed = false;
    if (is_precomputed) return;

    lmr_table.resize(100 + 1, std::vector<int>(max_i + 1));

    for (int depth = max_depth; depth >= 1; --depth) {
        for (int i = max_i; i >= 1; --i) {
            lmr_table[depth][i] =  static_cast<int>(lmr_1 + lmr_2 * log(depth) * log(i));
        }
    }

    is_precomputed = true;
}

bool table_lookup(Board& board, 
    int& depth, 
    int& eval, 
    bool& pv,
    Move& best_move, 
    EntryType& type,
    std::vector<LockedTableEntry>& table) {  

    U64 hash = board.hash();
    U64 index = hash % table.size();
    LockedTableEntry& locked_entry = table[index];
    std::lock_guard<std::mutex> lock(locked_entry.mtx);  

    if (locked_entry.entry.hash == hash) {
        depth = locked_entry.entry.depth;
        eval = locked_entry.entry.eval;
        pv = locked_entry.entry.pv;
        best_move = locked_entry.entry.best_move;
        type = locked_entry.entry.type;
        return true;
    }

    return false;
}

void table_insert(Board& board, 
    int depth, 
    int eval, 
    bool pv,
    Move best_move, 
    EntryType type,
    std::vector<LockedTableEntry>& table) {

    U64 hash = board.hash();
    U64 index = hash % table.size();
    LockedTableEntry& locked_entry = table[index];

    if (locked_entry.entry.hash == hash && locked_entry.entry.pv) {
        pv = true; // don't overwrite the pv node if it was set
    }
        
    std::lock_guard<std::mutex> lock(locked_entry.mtx); 
    locked_entry.entry = {hash, eval, depth, pv, best_move, type}; 
}

// Update killer moves 
inline void update_killers(const Move& move, int ply, int thread_id) {
    killer[thread_id][ply][0] = killer[thread_id][ply][1];
    killer[thread_id][ply][1] = move;
}

// Static exchange evaluation (SEE) function
int see(Board& board, Move move) {
    int to = move.to().index();

    auto victim = board.at<Piece>(move.to());
    int victim_value = piece_type_value(victim.type());

    std::vector<int> values;
    values.push_back(victim_value);

    std::vector<Move> exchange_stack;
    exchange_stack.push_back(move);

    int depth = 0;
    Color current_side = board.sideToMove();

    Board copy = board;
    while (!exchange_stack.empty()) {
        Move current_move = exchange_stack.back();
        exchange_stack.pop_back();

        copy.makeMove(current_move); // Make the capture
        Movelist captures;
        movegen::legalmoves<movegen::MoveGenType::CAPTURE>(captures, copy);

        Move best_next_capture = Move::NO_MOVE;
        int best_value = INF;

        for (const Move& next_capture : captures) {
            if (next_capture.to().index() != to) continue; 
            
            int value = piece_type_value(copy.at<Piece>(next_capture.from()).type());
            if (value < best_value) {
                best_value = value;
                best_next_capture = next_capture;
            }
        }

        if (best_next_capture == Move::NO_MOVE) break;
        
        values.push_back(best_value);
        exchange_stack.push_back(best_next_capture);
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
    bool is_promotion_threat = promotion_threat(board, move);

    if (i <= 1 || depth <= 3 || is_promotion_threat) {
        return depth - 1;
    } else {
        bool improving = ply >= 2 && static_eval[thread_id][ply - 2] < static_eval[thread_id][ply] && !board.inCheck();
        bool is_capture = board.isCapture(move);
        
        int R = lmr_table[depth][i];
        int tt_eval, tt_depth, history_score = history[thread_id][stm][move_index(move)];
        bool tt_is_pv, past_pv = false;
        EntryType tt_type;
        Move tt_move;
        
        if (table_lookup(board, tt_depth, tt_eval, tt_is_pv, tt_move, tt_type, tt_table)) {
            past_pv = tt_is_pv; 
        }

        if (improving || is_pv  || past_pv || is_capture) {
            R--;
        }

        if (board.inCheck()) {
            R--;
        }

        return std::min(depth - R, depth - 1);
    }
}

// generate ordered moves for the current position
std::vector<std::pair<Move, int>> order_move(Board& board, int ply, int thread_id, bool& hash_move_found) {

    Movelist moves;
    movegen::legalmoves(moves, board);

    std::vector<std::pair<Move, int>> primary;
    std::vector<std::pair<Move, int>> quiet;

    primary.reserve(moves.size());
    quiet.reserve(moves.size());

    bool stm = board.sideToMove() == Color::WHITE;
    Color color = board.sideToMove();
    U64 hash = board.hash();

    // A pair is either (ply - 1, ply) or (ply - 2, ply) that caused beta cut-off
    // We try to find the best pair give it higher priority
    Move best_2ply_move = Move::NO_MOVE;
    int best_2ply_score = -INF;

    if (ply >= 2) {
        for (const auto& move : moves) {
            int move_index_2 = move_index(move_stack[thread_id][ply - 2]);
            int move_index_1 = move_index(move_stack[thread_id][ply - 1]);
            int move_index_0 = move_index(move);
            std::pair<int, int> pair_1 = {move_index_2, move_index_0};
            std::pair<int, int> pair_2 = {move_index_1, move_index_0};

            int count_2 = mg_2ply[thread_id].get_count(pair_1) + mg_2ply[thread_id].get_count(pair_2);
            if (count_2 > best_2ply_score) {
                best_2ply_score = count_2;
                best_2ply_move = move;
            }
        }
    }

    for (const auto& move : moves) {
        Move tt_move;
        EntryType tt_type;
        TableEntry entry;
        int tt_eval, tt_depth, priority = 0;
        bool tt_is_pv;
        bool secondary = false;
        bool hash_move = false;

        if (table_lookup(board, tt_depth, tt_eval, tt_is_pv, tt_move, tt_type, tt_table)) {
            // Hash move from the PV transposition table should be searched first 
            if (tt_move == move && tt_type == EntryType::EXACT) {
                priority = 19000 + tt_eval;
                primary.push_back({tt_move, priority});
                hash_move = true;
                hash_move_found = true;
            } else if (tt_move == move && tt_type == EntryType::LOWERBOUND) {
                priority = 18000 + tt_eval;
                primary.push_back({tt_move, priority});
                hash_move = true;
                hash_move_found = true;
            }
        } 
      
        if (hash_move) continue;

        if (is_promotion(move)) {                   
            priority = 16000; 
        } else if (board.isCapture(move)) { 
            int victime_value = piece_type_value(board.at<Piece>(move.to()).type());
            int see_score = see(board, move);   
            priority = 4000 + see_score;// victime_value + score;
        } else if (std::find(killer[thread_id][ply].begin(), killer[thread_id][ply].end(), move) != killer[thread_id][ply].end()) {
            priority = 4000; // killer move
        } else if (move == best_2ply_move) {
            priority = 3950;
        } else {
            secondary = true;
            U64 move_idx = move_index(move);
            priority = history[thread_id][stm][move_idx];
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
int quiescence(Board& board, int alpha, int beta, int ply, int thread_id) {

    // Stop the search if hard deadline is reached
    auto current_time = std::chrono::high_resolution_clock::now();
    if (current_time >= hard_deadline || stop_search) {
        stop_search = true;
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
    
    node_count[thread_id]++;
    bool stm = (board.sideToMove() == Color::WHITE);
    int stand_pat = 0;

    // Probe Syzygy tablebases
    Move syzygy_move = Move::NO_MOVE;
    int wdl = 0;
    if (syzygy::probe_syzygy(board, syzygy_move, wdl)) {
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
        stand_pat = color * mopup_score(board);
    } else {
        if (stm == 1) {
            stand_pat = nnue.evaluate(white_accumulator[thread_id], black_accumulator[thread_id]);
        } else {
            stand_pat = nnue.evaluate(black_accumulator[thread_id], white_accumulator[thread_id]);
        }
    }

    int best_score = stand_pat;
    if (stand_pat >= beta) {
        return beta;
    }

    alpha = std::max(alpha, stand_pat);
    std::vector<std::pair<Move, int>> candidate_moves;
    candidate_moves.reserve(moves.size());

    for (const auto& move : moves) {
        int see_score = see(board, move);
        candidate_moves.push_back({move, see_score});
    }

    std::sort(candidate_moves.begin(), candidate_moves.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    for (auto& [move, priority] : candidate_moves) {
        add_accumulators(board, move, white_accumulator[thread_id], black_accumulator[thread_id], nnue);
        board.makeMove(move);
        
        int score = 0;
        score = -quiescence(board, -beta, -alpha, ply + 1, thread_id);

        subtract_accumulators(board, move, white_accumulator[thread_id], black_accumulator[thread_id], nnue);
        board.unmakeMove(move);

        best_score = std::max(best_score, score);
        alpha = std::max(alpha, score);

        if (alpha >= beta) { 
            return beta;
        }
    }
    return best_score;
}

// Negamax main search function
int negamax(Board& board, int depth, int alpha, int beta, std::vector<Move>& PV, NodeData& data) {

    // Stop the search if hard deadline is reached
    auto current_time = std::chrono::high_resolution_clock::now();
    if (current_time >= hard_deadline || stop_search) {
        stop_search = true;
        return 0;
    }

    int thread_id = data.thread_id;
    int ply = data.ply;
    int root_depth = data.root_depth;
    Move excluded_move = data.excluded_move;

    std::vector<Move> bad_quiets; // quiet moves that fail to raise alpha
    std::vector<Move> bad_captures; // bad tacticals (captures/promos) that fail to raise alpha

    // Extract whether we can do singular search and NMP
    bool nmp_ok = data.nmp_ok;

    // Extract node type and last move from nodeInfo
    NodeType nodeType = data.node_type;

    node_count[thread_id]++;
    bool mopUp = is_mopup(board);
    bool is_pv = (alpha < beta - 1);
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
    Move syzygy_move = Move::NO_MOVE;
    int wdl = 0;
    if (syzygy::probe_syzygy(board, syzygy_move, wdl)) { 
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
    int tt_eval, tt_depth, extensions = 0;
    bool tt_is_pv = false;
    bool improving = ply >= 2 && static_eval[thread_id][ply - 2] < static_eval[thread_id][ply] && !board.inCheck();

    Move tt_move;
    EntryType tt_type;
    TableEntry entry;
    bool tt_hit = false;

    if (table_lookup(board, tt_depth, tt_eval, tt_is_pv, tt_move, tt_type, tt_table)) {
        table_hit[thread_id]++;
        if (tt_depth >= depth) found = true;
        tt_hit = true;
    }

    if (found && !is_pv) {
        if (tt_type == EntryType::EXACT
            || (tt_type == EntryType::LOWERBOUND && tt_eval >= beta)
            || (tt_type == EntryType::UPPERBOUND && tt_eval <= alpha)) {
            
            return tt_eval;
        } 
    }
    
    if (found && is_pv) {
        if ((tt_type == EntryType::EXACT  || tt_type == EntryType::LOWERBOUND) && tt_eval >= beta) {
            return tt_eval;
        } 
    }
    
    if (depth <= 0 && !board.inCheck()) {
        int qEval = quiescence(board, alpha, beta, ply + 1, thread_id);
        eval_adjust(qEval);
        return qEval;
    } else if (depth <= 0) {
        return negamax(board, 1, alpha, beta, PV, data);
    }

    
    int stand_pat = 0;
    if (stm == 1) {
        stand_pat = nnue.evaluate(white_accumulator[thread_id], black_accumulator[thread_id]);
    } else {
        stand_pat = nnue.evaluate(black_accumulator[thread_id], white_accumulator[thread_id]);
    }

    // Use hash table's evaluation instead of NNUE if the position is found
    if (tt_hit) {
        if (tt_type == EntryType::EXACT 
            || (tt_type == EntryType::LOWERBOUND && tt_eval > stand_pat)
            || (tt_type == EntryType::UPPERBOUND && tt_eval < stand_pat)) {
            stand_pat = tt_eval;
        }
    } 
    
    static_eval[thread_id][ply] = stand_pat; // store the evaluation along the path
    bool hash_move_found = false;
    killer[thread_id][ply + 1] = {Move::NO_MOVE, Move::NO_MOVE}; 
    
    std::vector<std::pair<Move, int>> moves = order_move(board, 
                                                        ply, 
                                                        thread_id,
                                                        hash_move_found);
    
    seeds[thread_id] = fast_rand(seeds[thread_id]); 
    int R1 = seeds[thread_id] % moves.size(); 
    seeds[thread_id] = fast_rand(seeds[thread_id]);
    int R2 = seeds[thread_id] % moves.size();

    bool capture_tt_move = found && tt_move != Move::NO_MOVE && board.isCapture(tt_move);
    
    // Reverse futility pruning (RFP)
    bool rfp_condition = depth <= rfp_depth
                        && !board.inCheck() 
                        && !is_pv 
                        && !tt_is_pv
                        && !capture_tt_move
                        && excluded_move == Move::NO_MOVE // No rfp during singular search
                        && abs(beta) < 10000;
    if (rfp_condition) {
        int rfp_margin = rfp_c1 * (depth - improving);
        if (stand_pat >= beta + rfp_margin) {
            return (stand_pat + beta) / 2;
        }
    }

    // Null move pruning. Side to move must have non-pawn material.
    const int null_depth = 4; 
    bool nmp_condition = (depth >= null_depth 
        && non_pawn_material(board) 
        && !board.inCheck() 
        && !mopUp 
        && !is_pv
        && stand_pat >= beta
        && nmp_ok
        && excluded_move == Move::NO_MOVE // No nmp during singular search
    );

    if (nmp_condition) {
            
        std::vector<Move> null_pv; 
        int null_eval;
        int reduction = 3;

        NodeData null_data = {ply + 1, 
                                false, 
                                root_depth,
                                NodeType::ALL, 
                                Move::NO_MOVE,
                                thread_id};
        move_stack[thread_id][ply] = -1;
        board.makeNullMove();
        null_pv.push_back(Move::NULL_MOVE);
        null_eval = -negamax(board, depth - reduction, -beta, -(beta - 1), null_pv, null_data);
        eval_adjust(null_eval);
        board.unmakeNullMove();

        if (null_eval >= beta) {
            return beta;
        } 
    }

    int best_eval = -INF;

    // Simplified version of IID. Reduce the depth to facilitate the search if no hash move found.
    if (!hash_move_found && depth >= 4) {
        depth = depth - 1;
    }

    // Singular extension
    int singular_ext = 0;
    // seeds[thread_id] = fast_rand(seeds[thread_id]);
    // if (hash_move_found && tt_depth >= depth - 3
    //     && depth >= 8
    //     && tt_type != EntryType::UPPERBOUND
    //     && abs(tt_eval) < INF/2 - 100
    //     && excluded_move == Move::NO_MOVE // No singular search within singular search
    // ) {
    //     // #pragma omp atomic
    //     // singular_search_count++;
    //     int singular_eval = -INF;
    //     int singular_beta = tt_eval - singular_c1 * depth - singular_c2; 
    //     std::vector<Move> singular_pv;
    //     NodeData singular_node_data = {ply, 
    //         false, 
    //         root_depth,
    //         NodeType::PV,
    //         tt_move,
    //         thread_id};
    //     singular_eval = negamax(board, (depth - 1) / 2, singular_beta - 1, singular_beta, singular_pv, singular_node_data);
    //     if (singular_eval < singular_beta) {
    //         singular_ext++; // singular extension
    //         if (singular_eval < singular_beta - 40) {
    //             singular_ext++; // double extension
    //         }
    //     } 
    // }

    if (board.inCheck()) {
        extensions++;
    }

    if (moves.size() == 1) {
        extensions++;
    }

    // Evaluate moves
    for (int i = 0; i < moves.size(); i++) {

        Move move = moves[i].first;
        std::vector<Move> childPV;

        if (move == excluded_move) {
            continue; // skip excluded move
        }
        
        bool is_promo = is_promotion(move);
        bool in_check = board.inCheck();
        bool is_capture = board.isCapture(move);
        bool is_promotion_threat = promotion_threat(board, move) || is_promo; 

        board.makeMove(move);
        bool give_check = board.inCheck();
        board.unmakeMove(move);

        int eval = 0;
        int next_depth = late_move_reduction(board, move, i, depth, ply, is_pv, nodeType, thread_id); 

        if (move == tt_move) {
            extensions += singular_ext;
        }

        extensions = std::clamp(extensions, 0, 2); 
        next_depth = std::min(next_depth + extensions, (3 + root_depth) - ply - 1);

        // common conditions for pruning
        bool can_prune = !in_check && !is_promotion_threat && i > 0;

        // Futility  pruning
        bool fp_condition = can_prune 
                            && !is_capture 
                            && !give_check 
                            && !is_pv 
                            && !tt_is_pv
                            && next_depth <= fp_depth 
                            && excluded_move == Move::NO_MOVE;
        if (fp_condition) {
            int margin = fp_c1 * (next_depth + improving);
            if (stand_pat + margin < alpha) {
                continue;
            }
        }

        // Further reduction for quiet moves
        // bool lmp_condition = can_prune && !is_pv && !is_capture && next_depth <= 2;
        // if (lmp_condition) {
        //     int divisor = improving ? 1 : 2;
        //     if (i >= (lmp_c1 + next_depth * next_depth) / divisor) {
        //         continue;
        //     }
        // }

        // History pruning
        // bool hp_condition = can_prune && !is_pv && !is_capture && next_depth <= 2;
        // if (hp_condition) {
        //     int margin = -hp_c1 * next_depth * next_depth; 
        //     int historyScore = history[thread_id][stm][move_index(move)];
        //     if (historyScore < margin) {
        //         continue;
        //     }
        // }
    
        add_accumulators(board, move, white_accumulator[thread_id], black_accumulator[thread_id], nnue);
        move_stack[thread_id][ply] = move_index(move);
        board.makeMove(move);
        
        bool null_window = false;
        bool reduced_depth = next_depth < depth - 1;

        NodeData child_node_data = {ply + 1, 
                                nmp_ok,
                                root_depth,
                                NodeType::PV,
                                excluded_move,
                                thread_id};

        // PVS: Full window for the first node. 
        // Once alpha is raised, we search with null window until alpha is raised again.
        // If alpha is raised on a null window or reduced depth, we search with full window and full depth.
        if (i == 0) {
            NodeType child_node_type;
            if (!is_pv && nodeType == NodeType::CUT) {
                child_node_type = NodeType::ALL;
            } else if (!is_pv && nodeType == NodeType::ALL) {
                child_node_type = NodeType::CUT;
            } else if (is_pv) {
                child_node_type = NodeType::PV;
            } 
            child_node_data.node_type = child_node_type;
            eval = -negamax(board, next_depth, -beta, -alpha, childPV, child_node_data);
            eval_adjust(eval);
        } else {
            // If we are in a PV node and search the next child on a null window, we expect
            // the child to be a CUT node. 
            // If we are in a CUT node, we expect the child to be an ALL node.
            // If we are in an ALL node, we expect the child to be a CUT node.
            null_window = true;
            NodeType child_node_type;
            if (is_pv) {
                child_node_type = NodeType::CUT;
            } else if (!is_pv && nodeType == NodeType::ALL) {
                child_node_type = NodeType::CUT;
            } else if (!is_pv && nodeType == NodeType::CUT) {
                child_node_type = NodeType::ALL;
            }
            child_node_data.node_type = child_node_type;

            eval = -negamax(board, next_depth, -(alpha + 1), -alpha, childPV, child_node_data);
            eval_adjust(eval);
        }
        
        subtract_accumulators(board, move, white_accumulator[thread_id], black_accumulator[thread_id], nnue);
        board.unmakeMove(move);
    
        // If we raised alpha in a null window search or reduced depth search, re-search with full window and full depth.
        // We don't need to do this for non-PV nodes because when beta = alpha + 1, the full window is the same as the null window.
        // Furthermore, if we are in a non-PV node and a reduced depth search raised alpha, then we will need to 
        // re-search with full window and full depth in some ancestor node anyway so there is no need to do it here. 
        if ((eval > alpha) && (null_window || reduced_depth) && is_pv) {

            // Now this child becomes a PV node.
            child_node_data.node_type = NodeType::PV;

            add_accumulators(board, move, white_accumulator[thread_id], black_accumulator[thread_id], nnue);
            move_stack[thread_id][ply] = move_index(move);
            board.makeMove(move);

            eval = -negamax(board, depth - 1, -beta, -alpha, childPV, child_node_data);
            eval_adjust(eval);

            subtract_accumulators(board, move, white_accumulator[thread_id], black_accumulator[thread_id], nnue);
            board.unmakeMove(move);
        }

        if (eval > best_eval) {
            best_eval = eval;
            if (best_eval > alpha) {
                alpha = best_eval;
                update_pv(PV, move, childPV);
            }
        }

        if (eval <= alpha) {
            if (is_capture) {
                bad_captures.push_back(move);
            } else {
                bad_quiets.push_back(move);
            }
        }

        // Beta cutoff.
        if (beta <= alpha) {
            int mv_index = move_index(move);
            int currentScore = history[thread_id][stm][mv_index];
            int limit = MAX_HIST;// is_capture ? MAX_CAP_HIST : MAX_HIST;
            int delta = (1.0 - static_cast<float>(std::abs(currentScore)) / static_cast<float>(limit)) * depth * depth;

            // Update history scores for the move that caused the cutoff and the previous moves that failed to cutoffs.
            if (!is_capture) {
                update_killers(move, ply, thread_id);
                history[thread_id][stm][mv_index] += delta;
                history[thread_id][stm][mv_index] = std::clamp(history[thread_id][stm][mv_index], -MAX_HIST, MAX_HIST);

                // penalize bad quiet moves
                for (auto& bad_quiet : bad_quiets) {
                    int bad_mv_idex = move_index(bad_quiet);
                    history[thread_id][stm][bad_mv_idex] -= delta;
                    history[thread_id][stm][bad_mv_idex] = std::clamp(history[thread_id][stm][bad_mv_idex], -MAX_HIST, MAX_HIST);
                }
            } 

            // combine follow-up and counter-move heuristics
            // we store the pair of moves in (ply - 2, ply) and (ply - 1, ply) that caused a beta cut-off
            if (ply >= 2) {
                int move_index_2 = move_index(move_stack[thread_id][ply - 2]);
                int move_index_1 = move_index(move_stack[thread_id][ply - 1]);
                int move_index_0 = move_index(move);
                mg_2ply[thread_id].insert({move_index_2, move_index_0});
                mg_2ply[thread_id].insert({move_index_1, move_index_0});
            } 

            break;
        } 
    }

    if (is_pv && excluded_move == Move::NO_MOVE) {
        // If the best_eval is in (alpha0, beta), then best_eval is EXACT.
        // If the best_eval <= alpha0, then best_eval is UPPERBOUND because this is caused by one of the children's beta-cutoff.
        // If the best_eval >= beta then we quit the loop early, then we know that this is a LOWERBOUND. 
        // This ccould be exact if the cut off is at the last child, but it's not too important to handle this case separately.
        // If the best_eval is in (alpha0, beta), then we know that this is an exact score.
        // This is nice in the sense that if a node is non-PV, the bounds are always UPPERBOUND and LOWERBOUND.
        // EXACT flag only happens at PV nodes.
        // For non-PV nodes, the bounds are artifical so we can't say for sure.
        EntryType type;

        if (best_eval > alpha0 && best_eval < beta) {
            type = EXACT;
        } else if (best_eval <= alpha0) {
            type = UPPERBOUND;
        } else {
            type = LOWERBOUND;
        } 

        if (PV.size() > 0) {
            table_insert(board, depth, best_eval, true, PV[0], type, tt_table);
        } else {
            table_insert(board, depth, best_eval, true, Move::NO_MOVE, type, tt_table);
        }

    } else if (excluded_move == Move::NO_MOVE) {
        // For non-PV nodes:
        // alpha is artifical for CUT nodes and beta is artifical for ALL nodes.
        // In CUT nodes, we have a fake alpha. We can only tell if best_eval is a LOWERBOUND if we have a beta cutoff.
        // IN ALL nodess, we have a fake beta. Similarly, we can only tell if best_eval is a UPPERBOUND if we have an alpha cutoff.
        if (best_eval >= beta) {
            EntryType type = LOWERBOUND;
            if (PV.size() > 0) {
                table_insert(board, depth, best_eval, false, PV[0], type, tt_table);
            } else {
                table_insert(board, depth, best_eval, false, Move::NO_MOVE, type, tt_table);
            }  
        } 
    }
    
    return best_eval;
}

//     Root search function to communicate with UCI interface. 
//     Time control: 
//     Hard deadline: 2x time limit
//     - Case 1: As long as we are within the time limit, we search as deep as we can.
//     - Case 2: Stop if we reach the hard deadline or certain depth.
std::tuple<Move, int, int, std::vector<Move>> root_search(Board& board, int max_depth = 30, int time_limit = 15000, int thread_id = 0) {

    // Time management variables
    auto start_time = std::chrono::high_resolution_clock::now();
    hard_deadline = start_time + 2 * std::chrono::milliseconds(time_limit);
    bool time_limit_exceed = false;

    int best_eval = -INF;
    int color = board.sideToMove() == Color::WHITE ? 1 : -1;

    std::vector<Move> root_moves (ENGINE_DEPTH + 1, Move::NO_MOVE);
    std::vector<int> evals (2 * ENGINE_DEPTH + 1, 0);
    std::vector<std::pair<Move, int>> moves;

    Move best_move = Move(); 
    Move syzygy_move;

    // Syzygy tablebase probe
    int wdl = 0;
    if (syzygy::probe_syzygy(board, syzygy_move, wdl)) {
        int score = 0;
        if (wdl == 1) {
            score = SZYZYGY_INF;
        } else if (wdl == -1) {
            score = -SZYZYGY_INF;
        }

        if (syzygy_move != Move::NO_MOVE && thread_id == 0) {
            std::cout << "info depth 0 score cp " << score  
                        << " nodes 0 time 0  pv " << uci::moveToUci(syzygy_move) << std::endl;
        }
        
        if (syzygy_move != Move::NO_MOVE) {
            try {
                Board board_copy = board;
                board_copy.makeMove(syzygy_move);
                return {syzygy_move, 0, score, {syzygy_move}};
            } catch (const std::exception&) {
                // In case somehow the move is invalid, continue with the search
            }
        }
    }
    
    // Start the search
    int stand_pat = nnue.evaluate(white_accumulator[thread_id], black_accumulator[thread_id]);
    int depth = 1;
    std::vector<Move> PV; 

    while (depth <= std::min(ENGINE_DEPTH, max_depth)) {
        Move curr_best_move = Move(); // Track the best move for the current depth
        int curr_best_eval = -INF;
        bool hash_move_found = false;

        // Aspiration window
        int window = 150;
        int alpha = (depth > 6) ? evals[depth - 1] - window : -INF;
        int beta  = (depth > 6) ? evals[depth - 1] + window : INF;
        
        std::vector<std::pair<Move, int>> new_moves;
        
        
        if (depth == 1) {
            moves = order_move(board, 0, 0, hash_move_found);
        }

        while (true) {
            curr_best_eval = -INF;
            int alpha0 = alpha;
            new_moves.clear();
            std::vector<Move> curr_pv;
            
            for (int i = 0; i < moves.size(); i++) {

                Move move = moves[i].first;
                std::vector<Move> childPV; 
                Board local_board = board;
                static_eval[thread_id][0] = stand_pat;

                int ply = 0;
                int next_depth = late_move_reduction(local_board, move, i, depth, 0, true, NodeType::PV, thread_id);
                int eval = -INF;

                NodeData child_node_data = {1, // ply of child node
                                        true, // NMP ok
                                        depth, // root depth
                                        NodeType::PV, // child of a root node is a PV node
                                        Move::NO_MOVE, // no excluded move
                                        thread_id};
                
                add_accumulators(local_board, move, white_accumulator[thread_id], black_accumulator[thread_id], nnue);
                move_stack[thread_id][ply] = move_index(move);
                local_board.makeMove(move);

                eval = -negamax(local_board, next_depth, -beta, -alpha, childPV, child_node_data);
                eval_adjust(eval);

                subtract_accumulators(local_board, move, white_accumulator[thread_id], black_accumulator[thread_id], nnue);
                local_board.unmakeMove(move);

                // Check for stop search flag
                if (stop_search) {
                    return {best_move, depth - 1, best_eval, PV};
                }

                if (eval > curr_best_eval && next_depth < depth - 1) {
                    // Re-search with full depth if we have a new best move
                    add_accumulators(local_board, move, white_accumulator[thread_id], black_accumulator[thread_id], nnue);
                    move_stack[thread_id][ply] = move_index(move);
                    local_board.makeMove(move);

                    eval = -negamax(local_board, depth - 1, -beta, -alpha, childPV, child_node_data);
                    eval_adjust(eval);

                    subtract_accumulators(local_board, move, white_accumulator[thread_id], black_accumulator[thread_id], nnue);
                    local_board.unmakeMove(move);

                    if (stop_search) {
                        return {best_move, depth - 1, best_eval, PV};
                    }
                }

                new_moves.push_back({move, eval}); // Store the move and its evaluation

                // If found the new best move
                if (eval > curr_best_eval) {
                    curr_best_eval = eval;
                    curr_best_move = move;
                    alpha = std::max(alpha, curr_best_eval);
                    update_pv(curr_pv, move, childPV);
                } 
                
                if (alpha >= beta) {
                    break;
                }
            }

            if (curr_best_eval <= alpha0 || curr_best_eval >= beta) {
                alpha = -INF;
                beta = INF;
                new_moves.clear();
            } else {
                PV = curr_pv;
                break;
            }
        }
        
        // Update the global best move and evaluation after this depth if the time limit is not exceeded
        best_move = curr_best_move;
        best_eval = curr_best_eval;
        

        // Sort the moves by evaluation for the next iteration
        std::sort(new_moves.begin(), new_moves.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });

        table_insert(board, depth, best_eval, true, best_move, EntryType::EXACT, tt_table);

        moves = new_moves;

        U64 total_node_count = 0, total_table_hit = 0;
        for (int i = 0; i < MAX_THREADS; i++) {
            total_node_count += node_count[i];
            total_table_hit += table_hit[i];
        }
    
        if (thread_id == 0){
            // Only print the analysis for the first thread to avoid clutter 
            std::string analysis = format_analysis(depth, best_eval, total_node_count, total_table_hit, start_time, PV, board);
            std::cout << analysis << std::endl;
        }

        
        if (moves.size() == 1) {
            return {moves[0].first, 0, stand_pat, {moves[0].first}}; // If there is only one move, return it immediately.
        }

        auto current_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();

        time_limit_exceed = duration > time_limit;
        bool spend_too_much_time = current_time >= hard_deadline;

        evals[depth] = best_eval;
        root_moves[depth] = best_move; 

        if (depth >= 6 
            && abs(evals[depth - 1]) >= INF/2 - 100 
            && abs(evals[depth]) >= INF/2 - 100) {
            break; // If two consecutive depths found mate, stop searching.
        }
        
        if (!time_limit_exceed) {
            depth++; // If the time limit is not exceeded, we can search deeper.
        } else {
            if (spend_too_much_time || (depth >= 1 && root_moves[depth] == root_moves[depth - 1] && depth >= 14)) {
                break; // If we go beyond the hard limit or stabilize.
            } 
            depth++; 
        }
    }

    return {best_move, depth, best_eval, PV};
}

Move lazysmp_root_search(Board &board, int num_threads, int max_depth, int timeLimit) {
    
    precompute_lmr(100, 500);  // Precompute late move reduction table
    omp_set_num_threads(num_threads); // Set the number of threads for OpenMP
    Move best_move = Move(); 

    stop_search = false;
    auto start_time = std::chrono::high_resolution_clock::now();

    // singular_ext_count = 0;
    // singular_search_count = 0;

    // Update if the size for the transposition table changes.
    if (tt_table.size() != table_size) {
        tt_table = std::vector<LockedTableEntry>(table_size);
    }

    for (int i = 0; i < MAX_THREADS; i++) {
        // Reset history scores 
        for (int j = 0; j < 64 * 64; j++) {
            history[i][0][j] = 0;
            history[i][1][j] = 0;
        }

        mg_2ply[i].clear(); 

        // Reset killer moves
        for (int j = 0; j < ENGINE_DEPTH; j++) {
            killer[i][j] = {Move::NO_MOVE, Move::NO_MOVE};
        }

        node_count[i] = 0;
        table_hit[i] = 0;
        seeds[i] = rand();

        // Make accumulators for each thread
        make_accumulators(board, white_accumulator[i], black_accumulator[i], nnue);
    }

    int depth = -1;
    int eval = -INF;
    std::vector<Move> PV;

    // Crude implementation of lazy SMP using OpenMP
    #pragma omp parallel for schedule (static, 1)
    for (int i = 0; i < num_threads; i++) {
        Board local_board = board;
        auto [thread_move, thread_depth, thread_eval, thread_pv] = root_search(local_board, max_depth, timeLimit, i);
        if (i == 0) { 
            // Get the result from thread 0
            depth = thread_depth;
            best_move = thread_move; 
            eval = thread_eval; 
            PV = thread_pv; 
            stop_search = true; // Stop all threads
        }
    }

    // Print the final analysis
    int total_node_count = 0;
    int total_table_hit = 0;
    for (int i = 0; i < num_threads; i++) {
        total_node_count += node_count[i];
        total_table_hit += table_hit[i];
    }

    std::string analysis = format_analysis(depth, eval, total_node_count, total_table_hit, start_time, PV, board);
    std::cout << analysis << std::endl;
    return best_move; 
}