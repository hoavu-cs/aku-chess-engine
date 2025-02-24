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
#include "../lib/stockfish_nnue_probe/probe.h"

using namespace chess;
using namespace Stockfish;

typedef std::uint64_t U64;

/*-------------------------------------------------------------------------------------------- 
    Initialize the NNUE evaluation function.
--------------------------------------------------------------------------------------------*/
void initializeNNUE() {
    static bool initialized = false;

    if (!initialized) {
        Stockfish::Probe::init("nn-b1a57edbea57.nnue", "nn-b1a57edbea57.nnue");
        initialized = true;
    }
}

/*-------------------------------------------------------------------------------------------- 
    Constants and global variables.
--------------------------------------------------------------------------------------------*/

std::unordered_map<U64, std::pair<int, int>> transpositionTable; // Hash -> (eval, depth)
std::unordered_map<U64, Move> hashMoveTable; // Hash -> move
std::unordered_map<U64, U64> historyTable; // History heuristic table

std::chrono::time_point<std::chrono::high_resolution_clock> hardDeadline; // Search hardDeadline
std::chrono::time_point<std::chrono::high_resolution_clock> softDeadline;

const int maxTableSize = 10000000; // Maximum size of the transposition table
U64 nodeCount; // Node count for each thread
U64 tableHit;
std::vector<Move> previousPV; // Principal variation from the previous iteration
std::vector<std::vector<Move>> killerMoves(1000); // Killer moves

int globalMaxDepth = 0; // Maximum depth of current search
bool mopUp = false; // Mop up flag

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
    Transposition table lookup and clear.
--------------------------------------------------------------------------------------------*/
bool tableLookUp(U64 hash, int depth, int& eval) {    
    auto it = transpositionTable.find(hash);
    bool found = it != transpositionTable.end() && it->second.second >= depth;

    if (found) {
        eval = it->second.first;
        return true;
    } else {
        return false;
    }
}

void clearTables() {
    if (transpositionTable.size() > maxTableSize) {
        transpositionTable = {};
        hashMoveTable = {};
        historyTable = {};
    }
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
void updateKillerMoves(const Move& move, int depth) {
    #pragma omp critical
    {
        if (killerMoves[depth].size() < 2) {
            killerMoves[depth].push_back(move);
        } else {
            killerMoves[depth][1] = killerMoves[depth][0];
            killerMoves[depth][0] = move;
        }
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
int lateMoveReduction(Board& board, Move move, int i, int depth, int ply, bool isPV, int quietCount) {

    if (mopUp) {
        return depth - 1;
    }

    // Late move pruning.
    if (!isPV && i > 10 && !board.inCheck()) {
        return 0;
    }

    // Late move reduction
    int R = 0;
    if (quietCount >= 4 * depth) {
        R = 1;
    }

    if (i <= 5 || depth <= 2) { 
        return depth - 1;
    } else {
        return depth - 2 - R;
    }
}

/*-------------------------------------------------------------------------------------------- 
    Returns a list of candidate moves ordered by priority.
--------------------------------------------------------------------------------------------*/
std::vector<std::pair<Move, int>> orderedMoves(
    Board& board, 
    int depth, std::vector<Move>& previousPV, 
    bool leftMost) {

    Movelist moves;
    movegen::legalmoves(moves, board);

    std::vector<std::pair<Move, int>> candidates;
    std::vector<std::pair<Move, int>> quietCandidates;

    candidates.reserve(moves.size());
    quietCandidates.reserve(moves.size());

    bool whiteTurn = board.sideToMove() == Color::WHITE;
    Color color = board.sideToMove();
    U64 hash = board.hash();

    // Move ordering 1. promotion 2. captures 3. killer moves 4. hash 5. checks 6. quiet moves
    for (const auto& move : moves) {
        int priority = 0;
        bool quiet = false;
        int moveIndex = move.from().index() * 64 + move.to().index();
        int ply = globalMaxDepth - depth;
        bool hashMove = false;

        // Previous PV move > hash moves > captures/killer moves > checks > quiet moves
        if (hashMoveTable.count(hash) && hashMoveTable[hash] == move) {
            priority = 9000;
            candidates.push_back({move, priority});
            hashMove = true;
        }
      
        if (hashMove) continue;
        
        if (previousPV.size() > ply && leftMost) {
            if (previousPV[ply] == move) {
                priority = 10000; // PV move
            }
        } else if (std::find(killerMoves[depth].begin(), killerMoves[depth].end(), move) != killerMoves[depth].end()) {
            priority = 4000; // Killer moves
        } else if (isPromotion(move)) {
            priority = 6000; 
        } else if (board.isCapture(move)) { 
            priority = 4000 + see(board, move);
        } else {
            board.makeMove(move);
            bool isCheck = board.inCheck();
            board.unmakeMove(move);

            if (isCheck) {
                priority = 4000;
            } else {
                quiet = true;
                U64 moveIndex = move.from().index() * 64 + move.to().index();
                if (historyTable.count(moveIndex)) {
                    priority = historyTable[moveIndex];
                } else {
                    priority = 0;// quietPriority(board, move);
                }
            }
        } 

        if (!quiet) {
            candidates.push_back({move, priority});
        } else {
            quietCandidates.push_back({move, priority});
        }
    }

    // Sort capture, promotion, checks by priority
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    std::sort(quietCandidates.begin(), quietCandidates.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    for (const auto& move : quietCandidates) {
        candidates.push_back(move);
    }

    return candidates;
}

/*-------------------------------------------------------------------------------------------- 
    Quiescence search for captures only.
--------------------------------------------------------------------------------------------*/
int quiescence(Board& board, int alpha, int beta) {
    
    #pragma omp critical
    nodeCount++;

    Movelist moves;
    movegen::legalmoves<movegen::MoveGenType::CAPTURE>(moves, board);

    int color = board.sideToMove() == Color::WHITE ? 1 : -1;
    int standPat = 0;

    if (mopUp) {
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

        // Delta pruning. If the material gain is not big enough, prune the move.
        // Commented out since it makes the engine behavior weird
        const int deltaMargin = 400;
        if (standPat + victimValue - attackerValue + deltaMargin < beta) {
            continue;
        }

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

    #pragma omp critical
    clearTables();

    auto currentTime = std::chrono::high_resolution_clock::now();
    if (currentTime >= hardDeadline) {
        return 0;
    }

    #pragma omp critical
    nodeCount++;

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

    // Probe the transposition table
    U64 hash = board.hash();
    bool found = false;
    int storedEval;
    
    #pragma omp critical
    {
        if (tableLookUp(hash, depth, storedEval) && storedEval >= beta) {
            #pragma 
            {
                tableHit++;
            }

            found = true;
        }
    }

    if (found) {
        return storedEval;
    } 

    if (depth <= 0) {
        int quiescenceEval = quiescence(board, alpha, beta);
        
        #pragma omp critical
        {
            transpositionTable[hash] = {quiescenceEval, 0};
        }
            
        return quiescenceEval;
    }

    // Only pruning if the position is not in check, mop up flag is not set, and it's not the endgame phase
    // Disable pruning for when alpha is very high to avoid missing checkmates
    
    bool pruningCondition = !board.inCheck() && !mopUp && !endGameFlag && alpha < INF/4 && alpha > -INF/4;
    int standPat = Probe::eval(board.getFen().c_str());

    // Razoring: Skip deep search if the position is too weak. Only applied to non-PV nodes.
    if (depth <= 3 && pruningCondition && !isPV) {
        int razorMargin = 350 + depth * 60; // Threshold increases slightly with depth

        if (standPat + razorMargin < alpha) {
            // If the position is too weak and unlikely to raise alpha, skip deep search
            return quiescence(board, alpha, beta);
        } 
    }

    // Futility pruning outside move loop
    if (depth <= 2 && pruningCondition && globalMaxDepth >= 10) {
        int margin = depth * 200;
        if (standPat - margin > beta) {
            // If the static evaluation - margin > beta, 
            // then it is considered to be too good and most likely a cutoff
            return standPat - margin;
        } 
    }

    // Null move pruning. Avoid null move pruning in the endgame phase.
    const int nullDepth = 4; // Only apply null move pruning at depths >= 4

    if (depth >= nullDepth && !endGameFlag && !leftMost && !board.inCheck() && !mopUp) {
        std::vector<Move> nullPV;
        int nullEval;
        int reduction = 3 + depth / 4;

        board.makeNullMove();
        nullEval = -negamax(board, depth - reduction, -beta, -(beta - 1), nullPV, false, ply + 1);
        board.unmakeNullMove();

        if (nullEval >= beta) { 
            // Even if we skip our move and the evaluation is >= beta, this is a cutoff since it is
            // a fail high (too good for us)
            return beta;
        } 
    }

    std::vector<std::pair<Move, int>> moves = orderedMoves(board, depth, previousPV, leftMost);
    int bestEval = -INF;
    int quietCount = 0;


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

        //  Futility pruning. If the move is quiet and late.
        if (depth <= 2 && pruningCondition && quiet && quietCount >= 10 && globalMaxDepth >= 10) {
            int margin = depth * 200;
            if (standPat + margin < alpha) {
                // If it is unlikely to raise alpha, skip the move
                return alpha;
            } 
        }

        int eval = 0;
        int nextDepth = lateMoveReduction(board, move, i, depth, ply, isPV, quietCount); 
        
        if (i > 0) {
            leftMost = false;
        }

        board.makeMove(move);
        
        /*--------------------------------------------------------------------------------------------
            PVS search: 
            Full window & full depth for the first node or during mop up.

            After the first node, search other nodes with a null window and potentially reduced depth.
            - If the depth is reduced and alpha is raised, research with full depth but still 
            with a null window.
            - Then, if alpha is raised, re-search with a full window & full depth. 

        --------------------------------------------------------------------------------------------*/

        bool nullWindow = false;
        if (i <= 0 || mopUp) {
            // full window & full depth search for the first few nodes
            // In an ideal world, with good move ordering, we only need to do this for i = 0
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
                updateKillerMoves(move, depth);

                U64 moveIndex = move.from().index() * 64 + move.to().index();
                #pragma omp critical
                {
                    historyTable[moveIndex] += depth * depth;
                }
            }
            break;
        }
    }

    #pragma omp critical
    {
        // Update hash tables
        if (PV.size() > 0) {
            transpositionTable[board.hash()] = {bestEval, depth}; 
            hashMoveTable[board.hash()] = PV[0];
        }
    }

    #pragma omp critical
    clearTables();

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

    initializeNNUE();

    auto startTime = std::chrono::high_resolution_clock::now();
    hardDeadline = startTime + 3 * std::chrono::milliseconds(timeLimit);
    softDeadline = startTime + 2 * std::chrono::milliseconds(timeLimit);
    bool timeLimitExceeded = false;

    Move bestMove = Move(); 
    int bestEval = -INF;
    int color = board.sideToMove() == Color::WHITE ? 1 : -1;

    std::vector<std::pair<Move, int>> moves;
    std::vector<Move> globalPV (maxDepth);

    if (board.us(Color::WHITE).count() == 1 || board.us(Color::BLACK).count() == 1) {
        mopUp = true;
    }

    omp_set_num_threads(numThreads);

    // Clear transposition tables
    #pragma omp critical
    clearTables();
    
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
            moves = orderedMoves(board, depth, previousPV, false);
        }
        auto iterationStartTime = std::chrono::high_resolution_clock::now();

        bool stopNow = false;
        int quietCount = 0;

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
    
            bool quiet = !isCapture && !isCheck && !isPromo && !inCheck && !isPromoThreat;
            if (quiet) {
                quietCount++;
            }

            bool newBestFlag = false;  
            int nextDepth = lateMoveReduction(localBoard, move, i, depth, 0, true, quietCount);
            int eval = -INF;
            int aspiration;

            localBoard.makeMove(move);
            eval = -negamax(localBoard, nextDepth, -INF, INF, childPV, leftMost, 0);
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
                eval = -negamax(localBoard, depth - 1, -INF, INF, childPV, leftMost, 0);
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
            // If the search was interrupted, break out of the loop.
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
            transpositionTable[board.hash()] = {bestEval, depth};
        }

        moves = newMoves;
        previousPV = PV;

        std::string depthStr = "depth " +  std::to_string(PV.size());
        std::string scoreStr = "score cp " + std::to_string(color * bestEval);
        std::string nodeStr = "nodes " + std::to_string(nodeCount);

        auto iterationEndTime = std::chrono::high_resolution_clock::now();
        std::string timeStr = "time " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(iterationEndTime - iterationStartTime).count());


        std::string pvStr = "pv ";
        for (const auto& move : PV) {
            pvStr += uci::moveToUci(move) + " ";
        }

        std::string analysis = "info " + depthStr + " " + scoreStr + " " +  nodeStr + " " + timeStr + " " + " " + pvStr;
        std::cout << analysis << std::endl;

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
            (depth > 3 && std::abs(candidateMove[depth] != candidateMove[depth - 1]))){
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
    
    #pragma omp critical
    clearTables();

    return bestMove; 
}