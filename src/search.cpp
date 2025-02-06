#include "search.hpp"
#include "chess.hpp"
#include "evaluation.hpp"
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

using namespace chess;

typedef std::uint64_t U64;

// Time management
std::vector<Move> previousPV; // Principal variation from the previous iteration (consider make this a parameters)

std::vector<std::vector<Move>> killerMoves(100); // Killer moves
uint64_t positionCount = 0; // Number of positions evaluated for benchmarking

const size_t TABLE_MAX_SIZE = 2000000; 



int tableHit = 0;
int globalMaxDepth = 0; // Maximum depth of current search. Use this to compute the current ply.
int k = 4; // top k moves in LMR will not be reduced
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
Transposition table. We use a hash table to store the evaluation values of the board positions,
the depth of the search, and the best move. This is used to avoid re-evaluating the same position
--------------------------------------------------------------------------------------------*/
struct hashEntry {
    int eval = 0;
    int depth = 0;
    Move bestMove;
    U64 hash = 0;
};

std::vector<hashEntry> whiteHashTable (TABLE_MAX_SIZE);
std::vector<hashEntry> blackHashTable (TABLE_MAX_SIZE);

// Look up the hash table for a given board position
bool tableLookUp(const Board& board, int& eval, const int depth, Move& bestMove, Color color) {
    U64 hash = board.hash();
    bool whiteTurn = board.sideToMove() == Color::WHITE;
    bool found = false;

    // If at the location in the hash table we find the same hash and the depth >= current depth
    // Return the evaluation and best move
    if (whiteTurn) {
        U64 index = hash % TABLE_MAX_SIZE; 

        if (whiteHashTable[index].hash == hash && whiteHashTable[index].depth >= depth) {
            eval = whiteHashTable[index].eval;
            bestMove = whiteHashTable[index].bestMove;
            found = true;
        }
    } else {
        U64 index = hash % TABLE_MAX_SIZE;

        if (blackHashTable[index].hash == hash && blackHashTable[index].depth >= depth) {
            eval = blackHashTable[index].eval;
            bestMove = blackHashTable[index].bestMove;
            found = true;
        }
    }
    return found;
}

// Update the hash table with the evaluation value, depth, best move, and hash
void tableUpdate(Board board, int eval, int depth, Move bestMove, Color color) {
    auto hash = board.hash();
    bool whiteTurn = board.sideToMove() == Color::WHITE;
    auto index = hash % TABLE_MAX_SIZE;

    if (whiteTurn) {
        #pragma omp critical
        {
            whiteHashTable[index] = {eval, depth, bestMove, hash};
        }
    } else {
        #pragma omp critical
        {
            blackHashTable[index] = {eval, depth, bestMove, hash};
        }
    }
}

// Check if a move is a promotion
bool isPromotion(const Move& move) {
    return (move.typeOf() & Move::PROMOTION) != 0;
}

// Update the killer moves
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
Late move reduction. I'm using a simple formula for now.
We don't reduce depth if the move is a capture, promotion, or check or near the frontier nodes.
It seems to work well for the most part.
--------------------------------------------------------------------------------------------*/
int depthReduction(Board& board, Move move, int i, int depth) {

    Board localBoard = board;
    localBoard.makeMove(move);
    bool isCheck = localBoard.inCheck();
    bool isPawnMove = localBoard.at<Piece>(move.from()).type() == PieceType::PAWN;

    if (i <= k || depth <= 3 || board.isCapture(move) || isPromotion(move) || isCheck || mopUp || isPawnMove) {
        return depth - 1;
    } else {
        return depth / 2;
    }

}

// Generate a prioritized list of moves based on their tactical value
std::vector<std::pair<Move, int>> prioritizedMoves(
    Board& board, 
    int depth, std::vector<Move>& previousPV, 
    bool leftMost) {

    Movelist moves;
    movegen::legalmoves(moves, board);
    std::vector<std::pair<Move, int>> candidates;
    std::vector<std::pair<Move, int>> quietCandidates;

    bool whiteTurn = board.sideToMove() == Color::WHITE;
    Color color = board.sideToMove();
    U64 hash = board.hash();

    // Move ordering 1. promotion 2. captures 3. killer moves 4. hash 5. checks 6. quiet moves
    for (const auto& move : moves) {
        int priority = 0;
        bool quiet = false;
        int ply = globalMaxDepth - depth;
        bool hashMove = false;

        int tableEval;
        Move tableBestMove;

        // Previous hash moves, PV, killer moves, history heuristic, captures, promotions, checks, quiet moves
        // Currently has a bug with the Move object that I don't know how to fix yet
        // #pragma omp critical 
        // {
        //     if (tableLookUp(board, tableEval, 0, tableBestMove, color)) {
        //         if (tableBestMove == move) {
        //             // std::cout << "Hash move found" << std::endl;
        //             priority = 9000;
        //             hashMove = true;
        //         }
        //     }
        // }

        if (hashMove) {
            candidates.push_back({move, priority});
            continue;
        }


        if (previousPV.size() > ply && leftMost) {
            // Previous PV
            if (previousPV[ply] == move) {
                priority = 100000;
            }

        } else if (std::find(killerMoves[depth].begin(), killerMoves[depth].end(), move) 
                != killerMoves[depth].end()) {
            // Killer
            priority = 8000;

        } else if (isPromotion(move)) {

            priority = 5000; 

        } else if (board.isCapture(move)) { 

            // MVV-LVA priority for captures
            auto victim = board.at<Piece>(move.to());
            auto attacker = board.at<Piece>(move.from());

            priority = 4000 + pieceValues[static_cast<int>(victim.type())] 
                            - pieceValues[static_cast<int>(attacker.type())];

        } else {
            board.makeMove(move);
            bool isCheck = board.inCheck();
            board.unmakeMove(move);

            if (isCheck) {
                priority = 3000;
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

    // Sort the quiet moves by priority
    std::sort(quietCandidates.begin(), quietCandidates.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    for (const auto& move : quietCandidates) {
        candidates.push_back(move);
    }

    return candidates;
}

int quiescence(Board& board, int depth, int alpha, int beta) {
    #pragma  omp critical 
    {
        positionCount++;
    }
    
    if (depth <= 0) {
        return evaluate(board);
    }

    bool whiteTurn = board.sideToMove() == Color::WHITE;
    int standPat = evaluate(board);
        
    if (whiteTurn) {
        if (standPat >= beta) {
            return beta;
        }
        if (standPat > alpha) {
            alpha = standPat;
        }
    } else {
        if (standPat <= alpha) {
            return alpha;
        }
        if (standPat < beta) {
            beta = standPat;
        }
    }

    Movelist moves;
    movegen::legalmoves(moves, board);
    std::vector<std::pair<Move, int>> candidateMoves;
    int greatestMaterialGain = 0;

    for (const auto& move : moves) {
        if (!board.isCapture(move) && !isPromotion(move)) {
            continue;
        }

        //  Prioritize promotions & captures
        if (isPromotion(move)) {

            candidateMoves.push_back({move, 5000});
            greatestMaterialGain = pieceValues[5]; // Assume queen promotion

        } else if (board.isCapture(move)) {

            auto victim = board.at<Piece>(move.to());
            auto attacker = board.at<Piece>(move.from());

            int priority = pieceValues[static_cast<int>(victim.type())] - pieceValues[static_cast<int>(attacker.type())];
            candidateMoves.push_back({move, priority});
            greatestMaterialGain = std::max(greatestMaterialGain, pieceValues[static_cast<int>(attacker.type())]);

        }         
    }

    std::sort(candidateMoves.begin(), candidateMoves.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });
    
    for (const auto& [move, priority] : candidateMoves) {

        board.makeMove(move);
        int score = 0;
        score = quiescence(board, depth - 1, alpha, beta);

        board.unmakeMove(move);

        if (whiteTurn) {
            if (score >= beta) { 
                return beta;
            }
            if (score > alpha) {
                alpha = score;
            }
        } else {
            if (score <= alpha) {
                return alpha;
            }
            if (score < beta) {
                beta = score;
            }
        }
    }

    return whiteTurn ? alpha : beta;
}

int alphaBeta(Board& board, 
            int depth, 
            int alpha, 
            int beta, 
            int quiescenceDepth,
            std::vector<Move>& PV,
            bool leftMost) {

    #pragma  omp critical
    {
        positionCount++;
    }


    bool whiteTurn = board.sideToMove() == Color::WHITE;
    Color color = board.sideToMove();
    bool endGameFlag;

    if (whiteTurn) {
        endGameFlag = isEndGame(board, Color::WHITE);
    } else {
        endGameFlag = isEndGame(board, Color::BLACK);
    }

    // Check if the game is over
    auto gameOverResult = board.isGameOver();
    if (gameOverResult.first != GameResultReason::NONE) {
        if (gameOverResult.first == GameResultReason::CHECKMATE) {
            if (whiteTurn) {
                return -INF/2 + (1000 - depth); // Get the fastest checkmate possible
            } else {
                return INF/2 - (1000 - depth); 
            }
        }
        return 0;
    }

    int tableEval;
    Move tableBestMove;
    bool found = tableLookUp(board, tableEval, depth, tableBestMove, color);
    if (found) {
        #pragma omp critical
        {
            tableHit++;
        }
        return tableEval;
    }

    if (depth <= 0) {
        int quiescenceEval = quiescence(board, quiescenceDepth, alpha, beta);
        tableUpdate(board, quiescenceEval, 0, -1, color); // Store the quiescence evaluation in the transposition table
        return quiescenceEval;
    }

    // Null move pruning. Avoid null move pruning in the endgame phase.
    if (!endGameFlag) {
        const int nullDepth = 4; // Only apply null move pruning at depths >= 4

        if (depth >= nullDepth && !leftMost) {
            if (!board.inCheck()) {

                board.makeNullMove();
                std::vector<Move> nullPV;
                int nullEval;
                int reduction = 3;

                if (whiteTurn) {
                    nullEval = alphaBeta(board, depth - reduction, beta - 1, beta, quiescenceDepth, nullPV, false);
                } else {
                    nullEval = alphaBeta(board, depth - reduction, alpha, alpha + 1, quiescenceDepth, nullPV, false);
                }

                board.unmakeNullMove();

                if (whiteTurn && nullEval >= beta) { // opponent failed to lower beta as black
                    return beta;
                } else if (!whiteTurn && nullEval <= alpha) { // opponent failed to raise alpha as white
                    return alpha;
                }
            }
        }
    }

    //

    // Razoring
    // if (depth == 3 && !board.inCheck() && !endGameFlag && !leftMost) {
    //     int razorMargin = 600;
    //     int standPat = quiescence(board, quiescenceDepth, alpha, beta);
    //     if (whiteTurn) {
    //         if (standPat + razorMargin < alpha) {
    //             return standPat + razorMargin;
    //         }
    //     } else {
    //         if (standPat - razorMargin > beta) {
    //             return standPat - razorMargin;
    //         }
    //     }
    // }

    std::vector<std::pair<Move, int>> moves = prioritizedMoves(board, depth, previousPV, leftMost);
    int bestEval = whiteTurn ? alpha - 1 : beta + 1;
    

    for (int i = 0; i < moves.size(); i++) {
        Move move = moves[i].first;
        std::vector<Move> childPV;


        int eval = 0;
        int nextDepth = depthReduction(board, move, i, depth); // Apply Late Move Reduction (LMR)
        
        if (i > 0) {
            leftMost = false;
        }

        bool fullSearchNeeded = true;

        board.makeMove(move);
        if (whiteTurn) {
            eval = alphaBeta(board, nextDepth, alpha, beta, quiescenceDepth, childPV, leftMost);
        } else {
            eval = alphaBeta(board, nextDepth, alpha, beta, quiescenceDepth, childPV, leftMost);
        }
        board.unmakeMove(move);

        if (whiteTurn) {
            if (eval > alpha) {
                PV.clear();
                PV.push_back(move);
                for (auto& move : childPV) {
                    PV.push_back(move);
                }
            }

            bestEval = std::max(bestEval, eval);
            alpha = std::max(alpha, eval);
        } else {
            if (eval < beta) {
                PV.clear();
                PV.push_back(move);
                for (auto& move : childPV) {
                    PV.push_back(move);
                }
            }

            bestEval = std::min(bestEval, eval);
            beta = std::min(beta, eval);
        }

        if (beta <= alpha) {
            updateKillerMoves(move, depth);
            break;
        }
    }

    Move bestMove;
    if (PV.size() > 0) {
        bestMove = PV[0];
    } else

    tableUpdate(board, bestEval, depth, bestMove, color);

    return bestEval;
}


Move findBestMove(Board& board, 
                int numThreads = 4, 
                int maxDepth = 8, 
                int quiescenceDepth = 10, 
                int timeLimit = 15000,
                bool quiet = false) {

    auto startTime = std::chrono::high_resolution_clock::now();
    bool timeLimitExceeded = false;

    int bestEval = (board.sideToMove() == Color::WHITE) ? -INF : INF;
    Move bestMove = Move(); 

    bool whiteTurn = board.sideToMove() == Color::WHITE;

    std::vector<std::pair<Move, int>> moves; // A vector of moves and their evaluation


    if (board.us(Color::WHITE).count() == 1 || board.us(Color::BLACK).count() == 1) {
        mopUp = true;
        k = INF;
    }

    //globalQuiescenceDepth = quiescenceDepth;
    omp_set_num_threads(numThreads);

    // Clear transposition tables
    const int baseDepth = 1;
    int apsiration = evaluate(board);
    int depth = baseDepth;
    int evals[1000];
    Move candidateMove[1000];

    while (depth <= maxDepth) {
        globalMaxDepth = depth;
        positionCount = 0;
        std::unordered_set<std::string> consoleMessages;
        
        // Track the best move for the current depth
        Move currentBestMove = Move();
        int currentBestEval = whiteTurn ? -INF : INF;
        std::vector<std::pair<Move, int>> newMoves;
        std::vector<Move> PV; // Principal variation

        if (depth == baseDepth) {
            moves = prioritizedMoves(board, depth, previousPV, false);
        }

        auto iterationStartTime = std::chrono::high_resolution_clock::now();

        #pragma omp for schedule(static)
        for (int i = 0; i < moves.size(); i++) {

            bool leftMost = (i == 0);

            Move move = moves[i].first;
            std::vector<Move> childPV; 
            
            Board localBoard = board;
            bool newBestFlag = false;  
            int nextDepth = depthReduction(localBoard, move, i, depth);
            int eval = whiteTurn ? -INF : INF;

            localBoard.makeMove(move);
            eval = alphaBeta(localBoard, 
                            nextDepth, 
                           -INF, 
                            INF, 
                            quiescenceDepth, 
                            childPV, 
                            leftMost);
            localBoard.unmakeMove(move);

            #pragma omp critical
            {
                if ((whiteTurn && eval > currentBestEval) || (!whiteTurn && eval < currentBestEval)) {
                    newBestFlag = true;
                }
            }
            // If we find a new best move, search it with full depth if needed
            if (newBestFlag && nextDepth < depth - 1) {
                localBoard.makeMove(move);
                eval = alphaBeta(localBoard, depth - 1, -INF, INF, quiescenceDepth, childPV, true);
                localBoard.unmakeMove(move);
            }

            #pragma omp critical
            newMoves.push_back({move, eval});

            #pragma omp critical
            {
                if ((whiteTurn && eval > currentBestEval) || 
                    (!whiteTurn && eval < currentBestEval)) {
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

        bestEval = currentBestEval;
        bestMove = currentBestMove;
        
        // Update the global best move and evaluation after this depth if the time limit is not exceeded
        if (whiteTurn) {
            std::sort(newMoves.begin(), newMoves.end(), [](const auto& a, const auto& b) {
                return a.second > b.second;
            });
            tableUpdate(board, currentBestEval, depth, currentBestMove, Color::WHITE);

        } else {
            std::sort(newMoves.begin(), newMoves.end(), [](const auto& a, const auto& b) {
                return a.second < b.second;
            });
            tableUpdate(board, currentBestEval, depth, currentBestMove, Color::BLACK);
        }

        moves = newMoves;
        previousPV = PV;

        std::string depthStr = "depth " + std::to_string(depth);
        std::string scoreStr = "score cp " + std::to_string(bestEval);
        std::string nodeStr = "nodes " + std::to_string(positionCount);

        auto iterationEndTime = std::chrono::high_resolution_clock::now();
        std::string timeStr = "time " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(iterationEndTime - iterationStartTime).count());

        std::string pvStr = "pv ";
        for (const auto& move : PV) {
            pvStr += uci::moveToUci(move) + " ";
        }
        
        std::string analysis = "info " + depthStr + " " + scoreStr + " " +  nodeStr + " " + timeStr + " " + pvStr;
        if (consoleMessages.find(analysis) == consoleMessages.end()) {
            std::cout << analysis << std::endl;
            consoleMessages.insert(analysis);
        }
        

        auto currentTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();

        timeLimitExceeded = duration > timeLimit;
        bool spendTooMuchTime = duration > 2 * timeLimit;

        // A position is unstable if the average evaluation changes by more than 50cp from 4 plies ago
        bool stableEval = true;
        if (depth >= 4 && depth <= ENGINE_DEPTH) {  
            if (std::abs(evals[depth] - evals[depth - 4]) > 25) {
                stableEval = false; 
            }
        }

        // Break out of the loop if the time limit is exceeded and the evaluation is stable.
        // We allow exceeding up to 2 * the time limit if the evaluation is unstable 
        if ((timeLimitExceeded && stableEval) || depth > ENGINE_DEPTH || spendTooMuchTime) {
            break;
        }

        depth++;
    }

    return bestMove; 
}