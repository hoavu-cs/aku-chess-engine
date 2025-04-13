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
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include <omp.h> // Include OpenMP header
#include <chrono>
#include <stdlib.h>
#include <cmath>
#include <filesystem>
#include <mutex>


#include "../lib/stockfish_nnue_probe/probe.h"
#include "../lib/fathom/src/tbprobe.h"

using namespace chess;
using namespace Stockfish;
typedef std::uint64_t U64;


/*-------------------------------------------------------------------------------------------- 
    Initialize the NNUE evaluation function.
    Utility function to convert board to pieces array for fast evaluation.
--------------------------------------------------------------------------------------------*/
void initializeNNUE() {
    std::cout << "Initializing NNUE." << std::endl;
    Stockfish::Probe::init("nn-1c0000000000.nnue", "nn-1c0000000000.nnue");
}

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
std::vector<std::vector<int>> lmrTable1;

// More aggressive LMR table
void precomputeLRM1(int maxDepth, int maxI) {
    static bool isPrecomputed = false;
    if (isPrecomputed) return;

    lmrTable1.resize(100 + 1, std::vector<int>(maxI + 1));

    for (int depth = maxDepth; depth >= 1; --depth) {
        for (int i = maxI; i >= 1; --i) {
            lmrTable1[depth][i] =  static_cast<int>(0.75 + 0.75 * log(depth) * log(i));
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
const int maxThreadsID = 50;

std::vector<std::vector<int>> maxHistScore(maxThreadsID, std::vector<int>(2, 0)); // Maximum history score for each thread, for each side
std::vector<std::vector<int>> minHistScore(maxThreadsID, std::vector<int>(2, 0)); // Minimum history score for each thread, for each side

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
std::chrono::time_point<std::chrono::high_resolution_clock> softDeadline;

std::vector<U64> nodeCount (maxThreadsID); // Node count for each thread
std::vector<U64> tableHit (maxThreadsID); // Table hit count for each thread
std::vector<Move> previousPV; // Principal variation from the previous iteration

// Killer moves for each thread and ply
std::vector<std::vector<std::vector<Move>>> killer(maxThreadsID, std::vector<std::vector<Move>> 
    (ENGINE_DEPTH + 1, std::vector<Move>(1, Move::NO_MOVE))); 

// History table for move ordering (threadID, side to move, move index)
std::vector<std::vector<std::vector<int>>> histTable(maxThreadsID, std::vector<std::vector<int>>(2, std::vector<int>(64 * 64, 0)));

//std::vector<std::vector<Move>> moveSequence(maxThreadsID);


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
U64 moveIndex(const Move& move) {
    return move.from().index() * 64 + move.to().index();
}
 
/*-------------------------------------------------------------------------------------------- 
    Check if the move is a promotion.
--------------------------------------------------------------------------------------------*/
bool isPromotion(const Move& move) {
    if (move.typeOf() & Move::PROMOTION) {
        return true;
    } 
    return false;
}

/*-------------------------------------------------------------------------------------------- 
    Update the killer moves. Currently using only 1 slot per ply.
--------------------------------------------------------------------------------------------*/
void updateKillerMoves(const Move& move, int ply, int threadID) {
    killer[threadID][ply][0] = move;
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
                    int seeScore,
                    bool leftMost, 
                    int threadID) {

    if (isMopUpPhase(board)) return depth - 1;
    bool stm = board.sideToMove() == Color::WHITE;

    if (i <= 2 || depth <= 2) { 
        return depth - 1;
    } else {
        int histScore = histTable[threadID][stm][moveIndex(move)];
        int R = lmrTable1[depth][i];

        if (histScore > maxHistScore[threadID][stm] * 0.5) {
            R--;
        } 

        if (board.inCheck()) {
            R--;
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

    std::vector<std::pair<Move, int>> nonQuiet;
    std::vector<std::pair<Move, int>> quiet;

    nonQuiet.reserve(moves.size());
    quiet.reserve(moves.size());

    bool stm = board.sideToMove() == Color::WHITE;
    Color color = board.sideToMove();
    U64 hash = board.hash();

    // Move ordering 1. promotion 2. captures 3. killer moves 4. hash 5. checks 6. quiet moves
    for (const auto& move : moves) {
        int priority = 0;
        bool secondary = false;
        int ply = globalMaxDepth - depth;
        bool hashMove = false;

        // Previous PV move > hash moves > captures/killer moves > checks > quiet moves
        Move tableMove;
        int tableEval;
        int tableDepth;
        EntryType tableType;
        TableEntry entry;

        if (tableLookUp(board, tableDepth, tableEval, tableMove, tableType, ttTable)) {
            // Hash move from the PV transposition table should be searched first 
            // These are best moves or moves that cause beta cutoffs
            if (tableMove == move && tableType == EntryType::EXACT) {
                priority = 9000 + tableEval;
                nonQuiet.push_back({tableMove, priority});
                hashMove = true;
                hashMoveFound = true;
            } else if (tableMove == move && tableType == EntryType::LOWERBOUND) {
                priority = 8000 + tableEval;
                nonQuiet.push_back({tableMove, priority});
                hashMove = true;
                hashMoveFound = true;
            }
        } 
      
        if (hashMove) continue;
        
        if (previousPV.size() > ply && leftMost) {
            if (previousPV[ply] == move) {
                priority = 10000; // PV move
                hashMoveFound = true;
            }
        } else if (isPromotion(move)) {
            priority = 6000; 
        } else if (board.isCapture(move)) { 
            int seeScore = see(board, move, threadID);
            priority = 4000 + seeScore;
        } else if (std::find(killer[threadID][ply].begin(), killer[threadID][ply].end(), move) != killer[threadID][ply].end()) {
            priority = 4000; // Killer move
        } else if (ply >= 2 && std::find(killer[threadID][ply - 2].begin(), killer[threadID][ply - 2].end(), move) != killer[threadID][ply - 2].end()) {
            priority = 3700;  
        } else {
            board.makeMove(move);
            bool isCheck = board.inCheck();
            board.unmakeMove(move);

            if (isCheck) {
                priority = 2000;
            } else {
                secondary = true;
                U64 mvIndex = moveIndex(move);
                priority = histTable[threadID][stm][mvIndex];
            }
        } 

        if (!secondary) {
            nonQuiet.push_back({move, priority});
        } else {
            quiet.push_back({move, priority});
        }
    }

    // Sort capture, promotion, checks by priority
    std::sort(nonQuiet.begin(), nonQuiet.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    std::sort(quiet.begin(), quiet.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    for (const auto& move : quiet) {
        nonQuiet.push_back(move);
    }

    return nonQuiet;
}

/*-------------------------------------------------------------------------------------------- 
    Quiescence search for captures only.
--------------------------------------------------------------------------------------------*/
int quiescence(Board& board, int alpha, int beta, int ply, int threadID) {
    
    nodeCount[threadID]++;

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
        int priority = see(board, move, threadID);
        candidateMoves.push_back({move, priority});
    }

    std::sort(candidateMoves.begin(), candidateMoves.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    for (const auto& [move, priority] : candidateMoves) {
        board.makeMove(move);
        int score = 0;
        score = -quiescence(board, -beta, -alpha, ply + 1, threadID);
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
    int extensions = nodeInfo.extensions;
    Move lastMove = nodeInfo.lastMove;

    nodeCount[threadID]++;
    bool mopUp = isMopUpPhase(board);
    bool endGameFlag = gamePhase(board) <= 12;
    int color = (board.sideToMove() == Color::WHITE) ? 1 : -1;
    bool isPV = (alpha < beta - 1);
    int alpha0 = alpha;
    bool stm = (board.sideToMove() == Color::WHITE);
    
    // Check if the game is over
    auto gameOverResult = board.isGameOver();
    if (gameOverResult.first != GameResultReason::NONE) {
        if (gameOverResult.first == GameResultReason::CHECKMATE) {
            return -(INF/2 - ply); 
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
    
    if (found && tableEval >= beta && (tableType == EXACT || tableType == EntryType::LOWERBOUND)) {
        return tableEval;
    }  

    if (found && tableEval <= alpha && !isPV && tableType == EntryType::UPPERBOUND) {
        return tableEval;
    }
    
    if (depth <= 0 && (!board.inCheck() || ply == globalMaxDepth)) {
        return quiescence(board, alpha, beta, ply + 1, threadID);
    } else if (depth <= 0) {
        depth++;
        return negamax(board, depth, alpha, beta, PV, nodeInfo);
    }
    
    int standPat = Probe::eval(board.getFen().c_str());

    /*--------------------------------------------------------------------------------------------
        Reverse futility pruning: We skip the search if the position is too good for us.
        Avoid pruning in the endgame phase, when alpha is close to the mate score (to avoid missing 
        checkmates). We also not do this in PV nodes.
    ------------------------------------------------------------- -------------------------------*/
    bool pruningCondition = !board.inCheck() 
                            && !endGameFlag 
                            && std::abs(alpha) < 8000
                            && std::abs(beta) < 8000
                            && !leftMost
                            && !mopUp;

    if (depth <= 2 && pruningCondition) {
        int margin = 330 * depth;
        if (standPat - margin > beta) {
            return standPat - margin;
        } 
    } 

    /*-------------------------------------------------------------------------------------------- 
        Null move pruning. Avoid null move pruning in the endgame phase.
    --------------------------------------------------------------------------------------------*/
    const int nullDepth = 4; 

    if (depth >= nullDepth && !endGameFlag && !leftMost && !board.inCheck() && !mopUp) {
        std::vector<Move> nullPV; // dummy PV
        int nullEval;
        int reduction = (depth >= 6) ? 4 : 3;

        NodeInfo nullNodeInfo = {ply + 1, false, extensions, Move::NULL_MOVE, threadID}; 

        //moveSequence[threadID].push_back(Move::NULL_MOVE);

        board.makeNullMove();
        nullEval = -negamax(board, depth - reduction, -beta, -(beta - 1), nullPV, nullNodeInfo);
        board.unmakeNullMove();

        //moveSequence[threadID].pop_back();

        if (nullEval >= beta) return beta;
    }

    bool hashMoveFound = false;
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

    if (!hashMoveFound && !isPV) {
        // Reduce the depth to facilitate the search if no hash move found 
        depth = std::max(depth - 1, 1);
    }

    /*--------------------------------------------------------------------------------------------
        Evaluate moves.
    --------------------------------------------------------------------------------------------*/
    for (int i = 0; i < moves.size(); i++) {

        Move move = moves[i].first;
        std::vector<Move> childPV;

        bool isCapture = board.isCapture(move);
        bool isPromo = isPromotion(move);
        bool isPromoThreat = promotionThreatMove(board, move);
        bool inCheck = board.inCheck();
        board.makeMove(move); 
        bool isCheck = board.inCheck(); 
        board.unmakeMove(move); 

        bool quiet = !isCapture && !isCheck && !isPromo && !inCheck && !isPromoThreat;

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

        if (i > 0) leftMost = false;
        
        int eval = 0;
        int seeScore = 0;
        
        if (isCapture) {
            // see score will be negative if the move is a bad capture
            // if the move is a hash move or leftmost path move, then see > 6000 - 4000 = 2000 > 0.
            seeScore = moves[i].second - 4000;
        }

        int nextDepth = lateMoveReduction(board, move, i, depth, ply, isPV, seeScore, leftMost, threadID); 
        
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

        NodeInfo childNodeInfo = {ply + 1, leftMost, extensions, move, threadID}; 

        // ~ 35 Elo
        if (extensions && board.inCheck()) { 
            // check extension
            nextDepth++;
            childNodeInfo.extensions--; 
        } else if (extensions && moves.size() == 1) { 
            // forced move extension
            nextDepth++;
            childNodeInfo.extensions--;
        }

        //moveSequence[threadID].push_back(move);

        if (i == 0) {
            // full window & full depth search for the first node
            eval = -negamax(board, nextDepth, -beta, -alpha, childPV, childNodeInfo);
        } else {
            // null window and potential reduced depth for the rest
            nullWindow = true;
            eval = -negamax(board, nextDepth, -(alpha + 1), -alpha, childPV, childNodeInfo);
        }
        
        board.unmakeMove(move);
        bool alphaRaised = eval > alpha;
        bool reducedDepth = nextDepth < depth - 1;

        if (alphaRaised && (nullWindow || reducedDepth)  && isPV) {
            // If alpha is raised, research with full window & full depth (we don't do this for i = 0)
            board.makeMove(move);
            eval = -negamax(board, depth - 1, -beta, -alpha, childPV, childNodeInfo);
            board.unmakeMove(move);
        }

        if (eval > alpha) {
            PV.clear();
            PV.push_back(move);
            for (auto& move : childPV) {
                PV.push_back(move);
            }
        } 

        //moveSequence.pop_back();

        bestEval = std::max(bestEval, eval);
        alpha = std::max(alpha, eval);

        if (beta <= alpha) {

            if (i == moves.size() - 1) {
                // This means we have a beta cutoff at the last child node
                // which signals the result is an EXACT score.
                searchAllFlag = true;
            }

            const int maxHistory = 100000; // capped history score ~ 10 Elo

            if (!board.isCapture(move)) {
                updateKillerMoves(move, ply, threadID);
                int mvIndex = moveIndex(move);

                // tapered change new score = old score - (1 - old score / maxHistory) * depth * depth
                // the closer the old score is to maxHistory, the less change is applied.
                int currentHistScore = histTable[threadID][stm][mvIndex];
                float delta = depth * depth;
                
                int change = (1.0 - static_cast<float>(std::abs(currentHistScore)) / static_cast<float>(maxHistory)) * delta;

                histTable[threadID][stm][mvIndex] += change;
                maxHistScore[threadID][stm] = std::max(maxHistScore[threadID][stm], histTable[threadID][stm][mvIndex]);
            } 

            for (int j = 0; j < i; j++) {
                int mvIndex = moveIndex(moves[j].first);

                int currentHistScore = histTable[threadID][stm][mvIndex];
                float delta = depth * depth;
                int change = (1.0 - static_cast<float>(std::abs(currentHistScore)) / static_cast<float>(maxHistory)) * delta;
                
                histTable[threadID][stm][mvIndex] -= change;
                minHistScore[threadID][stm] = std::min(minHistScore[threadID][stm], histTable[threadID][stm][mvIndex]);
            }

            break;
        } 
    }

    if (isPV) {
        EntryType type;

        if (bestEval >= alpha0 && bestEval <= beta && searchAllFlag) {
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
                int maxDepth = 30, 
                int timeLimit = 15000,
                bool quiet = false) {

    // Update if the size for the transposition table changes.
    if (ttTable.size() != tableSize) {
        ttTable = std::vector<LockedTableEntry>(tableSize);
    }

    auto startTime = std::chrono::high_resolution_clock::now();
    bool timeLimitExceeded = false;

    hardDeadline = startTime + 3 * std::chrono::milliseconds(timeLimit);
    softDeadline = startTime + 2 * std::chrono::milliseconds(timeLimit);
    
    // Precompute late move reduction table
    precomputeLRM1(100, 500);

    // Reset history scores 
    for (int i = 0; i < maxThreadsID; i++) {
        // moveSequence[i] = {};
        for (int j = 0; j < 64 * 64; j++) {
            histTable[i][0][j] = 0;
            histTable[i][1][j] = 0;

            maxHistScore[i][0] = 0;
            minHistScore[i][1] = 0;
        }
    }

    // Reset killer moves for each thread and ply
    for (int i = 0; i < maxThreadsID; i++) {
        for (int j = 0; j < ENGINE_DEPTH; j++) {
            killer[i][j] = {};
        }
    }

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

    /*--------------------------------------------------------------------------------------------
        Check if the move position is in the endgame tablebase.
    --------------------------------------------------------------------------------------------*/
    Move syzygyMove;
    int wdl = 0;

    if (probeSyzygy(board, syzygyMove, wdl)) {
        if (!quiet) {
            int score = 0;
            if (wdl == 1) {
                score = SZYZYGY_INF;
            } else if (wdl == -1) {
                score = -SZYZYGY_INF;
            }
            std::cout << "info depth 0 score cp " << score << " nodes 0 time 0  pv " << uci::moveToUci(syzygyMove) << std::endl;
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

    int standPat = Probe::eval(board.getFen().c_str()); 

    while (depth <= maxDepth) {
        globalMaxDepth = depth;

        // reset node count for each thread
        for (int i = 0; i < maxThreadsID; i++) {
            nodeCount[i] = 0;
        }

        // reset table hit count for each thread
        for (int i = 0; i < maxThreadsID; i++) {
            tableHit[i] = 0;
        }

        // Track the best move for the current depth
        Move currentBestMove = Move();
        int currentBestEval = -INF;
        std::vector<std::pair<Move, int>> newMoves;
        std::vector<Move> PV; 
        bool hashMoveFound = false;

        if (depth == baseDepth) {
            moves = orderedMoves(board, depth, 0, previousPV, false, Move::NO_MOVE, 0, hashMoveFound);
        }



        auto iterationStartTime = std::chrono::high_resolution_clock::now();

        bool stopNow = false;
        int aspiration, alpha, beta;

        alpha = -INF;
        beta = INF;

        if (depth > 6) {
            aspiration = evals[depth - 1];
            alpha = aspiration - 150;
            beta = aspiration + 150;
        }

        while (true) {
            currentBestEval = -INF;

            #pragma omp parallel for schedule(dynamic, 1)
            for (int i = 0; i < moves.size(); i++) {

                if (stopNow) continue;
                
                bool leftMost = (i == 0);
                Move move = moves[i].first;
                //moveSequence[omp_get_thread_num()].push_back(move);

                if (depth > 8 && i > 9) {
                    // Ignore late moves after a certain depth
                    // Repeatedly search the top moves instead (lazySMP style)
                    move = moves[i % 9].first;
                }

                std::vector<Move> childPV; 
                Board localBoard = board;

                int ply = 0;
                bool newBestFlag = false;  
                int nextDepth = lateMoveReduction(localBoard, move, i, depth, 0, true, 0, leftMost, omp_get_thread_num());
                int eval = -INF;
                int extensions = 1;

                NodeInfo childNodeInfo = {1, leftMost, extensions, move, omp_get_thread_num()};

                

                localBoard.makeMove(move);
                eval = -negamax(localBoard, nextDepth, -beta, -alpha, childPV, childNodeInfo);
                localBoard.unmakeMove(move);

                // Check if the time limit has been exceeded, if so the search 
                // has not finished. Return the best move so far.
                if (std::chrono::high_resolution_clock::now() >= hardDeadline) {
                    stopNow = true;
                }

                if (stopNow) continue;

                #pragma omp critical
                {
                    if (eval >= currentBestEval) {
                        newBestFlag = true;
                    }
                }

                if (newBestFlag && nextDepth < depth - 1) {
                    localBoard.makeMove(move);
                    eval = -negamax(localBoard, depth - 1, -beta, -alpha, childPV, childNodeInfo);
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

        std::string depthStr = "depth " +  std::to_string(std::max(size_t(depth), PV.size()));
        std::string scoreStr = "score cp " + std::to_string(bestEval);

        U64 totalNodeCount = 0;
        for (int i = 0; i < maxThreadsID; i++) {
            totalNodeCount += nodeCount[i];
        }

        U64 totalTableHit = 0;
        for (int i = 0; i < maxThreadsID; i++) {
            totalTableHit += tableHit[i];
        }

        std::string nodeStr = "nodes " + std::to_string(totalNodeCount);

        std::string tableHitStr = "tableHit " + std::to_string(static_cast<double>(totalTableHit) / totalNodeCount);

        auto iterationEndTime = std::chrono::high_resolution_clock::now();
        std::string timeStr = "time " 
                            + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(iterationEndTime - iterationStartTime).count());

        std::string pvStr = "pv ";
        for (const auto& move : PV) {
            pvStr += uci::moveToUci(move) + " ";
        }

        std::string analysis = "info " + depthStr + " " 
                                    + scoreStr + " " 
                                    + nodeStr + " " 
                                    + timeStr + " " 
                                    + pvStr;

        if (!quiet) {
            std::cout << analysis << std::endl;
        }

        if (moves.size() == 1) {
            // If there is only one move, return it immediately.
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
            if (depth > ENGINE_DEPTH || spendTooMuchTime) break;
            depth++;
        }
    }


    return bestMove; 
}