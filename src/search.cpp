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

using namespace chess;

// Constants and global variables
std::unordered_map<std::uint64_t, std::pair<int, int>> lowerBoundTable; // Hash -> (eval, depth)
std::unordered_map<std::uint64_t, Move> whiteHashMove;

std::unordered_map<std::uint64_t, std::pair<int, int>> upperBoundTable; // Hash -> (eval, depth)
std::unordered_map<std::uint64_t, Move> blackHashMove;

// Time management
std::vector<Move> previousPV; // Principal variation from the previous iteration

std::vector<std::vector<Move>> killerMoves(100); // Killer moves
uint64_t positionCount = 0; // Number of positions evaluated for benchmarking

const size_t tableMaxSize = 1000000000; 
int tableHit = 0;
int globalMaxDepth = 0; // Maximum depth of current search
int globalQuiescenceDepth = 0; // Quiescence depth
int k = 2; // top k moves in LMR
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

// Transposition table lookup
bool transTableLookUp(std::unordered_map<std::uint64_t, std::pair<int, int>>& table, 
                            std::uint64_t hash, 
                            int depth, 
                            int& eval) {
    auto it = table.find(hash);
    bool found = it != table.end() && it->second.second >= depth;

    if (found) {
        eval = it->second.first;
        return true;
    } else {
        return false;
    }
}

// Check if a move is a promotion
bool isPromotion(const Move& move) {
    return (move.typeOf() & Move::PROMOTION) != 0;
}

// Return the priority of a quiet move based on some heuristics
int quietPriority(const Board& board, const Move& move) {
    auto type = board.at<Piece>(move.from()).type();
    Color color = board.sideToMove();

    Board boardAfter = board;
    boardAfter.makeMove(move);

    Bitboard theirQueen = board.pieces(PieceType::QUEEN, !color);
    Bitboard theirRook = board.pieces(PieceType::ROOK, !color);
    Bitboard theirBishop = board.pieces(PieceType::BISHOP, !color);
    Bitboard theirKnight = board.pieces(PieceType::KNIGHT, !color);
    Bitboard theirPawn = board.pieces(PieceType::PAWN, !color);

    int threat = 0;

    while (theirQueen) {
        int sqIndex = theirQueen.lsb();
        Bitboard attackerBefore = attacks::attackers(board, color, Square(sqIndex));
        Bitboard attackerAfter = attacks::attackers(boardAfter, color, Square(sqIndex));

        threat += attackerAfter.count() > attackerBefore.count() ? 9 : 0;

        theirQueen.clear(sqIndex);
    }

    while (theirRook) {
        int sqIndex = theirRook.lsb();
        Bitboard attackerBefore = attacks::attackers(board, color, Square(sqIndex));
        Bitboard attackerAfter = attacks::attackers(boardAfter, color, Square(sqIndex));

        threat += attackerAfter.count() > attackerBefore.count() ? 5 : 0;

        theirRook.clear(sqIndex);
    }

    while (theirBishop) {
        int sqIndex = theirBishop.lsb();
        Bitboard attackerBefore = attacks::attackers(board, color, Square(sqIndex));
        Bitboard attackerAfter = attacks::attackers(boardAfter, color, Square(sqIndex));

        threat += attackerAfter.count() > attackerBefore.count() ? 3 : 0;

        theirBishop.clear(sqIndex);
    }

    while (theirKnight) {
        int sqIndex = theirKnight.lsb();
        Bitboard attackerBefore = attacks::attackers(board, color, Square(sqIndex));
        Bitboard attackerAfter = attacks::attackers(boardAfter, color, Square(sqIndex));

        threat += attackerAfter.count() > attackerBefore.count() ? 3 : 0;

        theirKnight.clear(sqIndex);
    }

    int gamePhase = board.pieces(PieceType::KNIGHT, Color::WHITE).count() + board.pieces(PieceType::KNIGHT, Color::BLACK).count() +
                     board.pieces(PieceType::BISHOP, Color::WHITE).count() + board.pieces(PieceType::BISHOP, Color::BLACK).count() +
                     board.pieces(PieceType::ROOK, Color::WHITE).count() * 2 + board.pieces(PieceType::ROOK, Color::BLACK).count() * 2 +
                     board.pieces(PieceType::QUEEN, Color::WHITE).count() * 4 + board.pieces(PieceType::QUEEN, Color::BLACK).count() * 4;

    // Pawn push and king move is prioritized in endgame
    if (type == PieceType::PAWN  && gamePhase <= 12) {
        threat += 5;
    }

    return threat;
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

    if (i <= 4 || depth <= 3 || board.isCapture(move) || isPromotion(move) || isCheck || isPawnMove || mopUp) {
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
    std::uint64_t hash = board.hash();

    // Move ordering 1. promotion 2. captures 3. killer moves 4. hash 5. checks 6. quiet moves
    for (const auto& move : moves) {
        int priority = 0;
        bool quiet = false;
        int moveIndex = move.from().index() * 64 + move.to().index();
        int ply = globalMaxDepth - depth;
        bool hashMove = false;

        // Previous hash moves, PV, killer moves, history heuristic, captures, promotions, checks, quiet moves
        #pragma omp critial 
        {
            if (whiteTurn) {
                if (whiteHashMove.find(hash) != whiteHashMove.end() && whiteHashMove[hash] == move) {
                    priority = 9000;
                    candidates.push_back({move, priority});
                    hashMove = true;
                }

            } else {
  
                if (blackHashMove.find(hash) != blackHashMove.end() && blackHashMove[hash] == move) {
                    priority = 9000;
                    candidates.push_back({move, priority});
                    hashMove = true;
                }
            }
        }

        if (hashMove) {
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
            } else {
                priority = quietPriority(board, move);
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

    // Probe the transposition table
    std::uint64_t hash = board.hash();
    bool found = false;
    int storedEval;
    
    #pragma omp critical
    { 
        if ((whiteTurn && transTableLookUp(lowerBoundTable, hash, depth, storedEval) && storedEval >= beta) ||
            (!whiteTurn && transTableLookUp(upperBoundTable, hash, depth, storedEval) && storedEval <= alpha)) {
            found = true;

            tableHit++;
        }
    }

    if (found) {
        return storedEval;
    } 

    if (depth <= 0) {
        int quiescenceEval = quiescence(board, quiescenceDepth, alpha, beta);
        
        if (whiteTurn) {
            #pragma omp critical
            {
                lowerBoundTable[hash] = {quiescenceEval, 0};
            }
            
        } else {
            #pragma omp critical 
            {
                upperBoundTable[hash] = {quiescenceEval, 0};
            }
        }
        
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

    #pragma omp critical
    {
        // Update hash tables
        if (whiteTurn && PV.size() > 0) {
            lowerBoundTable[board.hash()] = {bestEval, depth}; 
            whiteHashMove[board.hash()] = PV[0];
        } else if (PV.size() > 0) {
            upperBoundTable[board.hash()] = {bestEval, depth}; 
            blackHashMove[board.hash()] = PV[0];
        }
    }

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

    Move bestMove = Move(); 
    int bestEval = (board.sideToMove() == Color::WHITE) ? -INF : INF;
    bool whiteTurn = board.sideToMove() == Color::WHITE;

    std::vector<std::pair<Move, int>> moves;
    std::vector<Move> globalPV (maxDepth);


    if (board.us(Color::WHITE).count() == 1 || board.us(Color::BLACK).count() == 1) {
        mopUp = true;
        k = INF;
    }


    globalQuiescenceDepth = quiescenceDepth;
    omp_set_num_threads(numThreads);

    // Clear transposition tables
    #pragma omp critical
    {
        if (lowerBoundTable.size() > tableMaxSize) {
            lowerBoundTable.clear();
        }
        if (upperBoundTable.size() > tableMaxSize) {
            upperBoundTable.clear();
        }  
    }
    
    const int baseDepth = 1;
    int apsiration = evaluate(board);
    int depth = baseDepth;
    int evals[1000];
    Move candidateMove[1000];

    while (depth <= maxDepth) {
        globalMaxDepth = depth;
        positionCount = 0;
        
        // Track the best move for the current depth
        Move currentBestMove = Move();
        int currentBestEval = whiteTurn ? -INF : INF;
        std::vector<std::pair<Move, int>> newMoves;
        std::vector<Move> PV; // Principal variation

        if (depth == baseDepth) {
            moves = prioritizedMoves(board, depth, previousPV, false);
        }

        bool leftMost;
        auto iterationStartTime = std::chrono::high_resolution_clock::now();

        #pragma omp for schedule(static)
        for (int i = 0; i < moves.size(); i++) {

            if (i == 0) {
                leftMost = true; // First move from the root starts the leftmost path
            } else {
                leftMost = false;
            }

            Move move = moves[i].first;
            std::vector<Move> childPV; 
            
            Board localBoard = board;
            bool newBestFlag = false;  
            int nextDepth = depthReduction(localBoard, move, i, depth);

            // Aspiration window
            int aspirationOption = 150;
            int eval = whiteTurn ? -INF : INF;

            // Aspiration window search except for the first move
            while (true) {

                localBoard.makeMove(move);
                eval = alphaBeta(localBoard, 
                                nextDepth, 
                                apsiration - aspirationOption, 
                                apsiration + aspirationOption, 
                                quiescenceDepth, 
                                childPV, 
                                leftMost);
                localBoard.unmakeMove(move);

                if (eval >= apsiration + aspirationOption || eval <= apsiration - aspirationOption) {
                    aspirationOption = aspirationOption * 2;
                } else {
                    break;
                }
            }

            #pragma omp critical
            {
                if ((whiteTurn && eval > currentBestEval) || (!whiteTurn && eval < currentBestEval)) {
                    newBestFlag = true;
                }
            }

            if (newBestFlag) {
                localBoard.makeMove(move);
                eval = alphaBeta(localBoard, depth - 1, -INF, INF, quiescenceDepth, childPV, leftMost);
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
        
        // Update the global best move and evaluation after this depth if the time limit is not exceeded
        bestMove = currentBestMove;
        bestEval = currentBestEval;

        if (whiteTurn) {
            std::sort(newMoves.begin(), newMoves.end(), [](const auto& a, const auto& b) {
                return a.second > b.second;
            });

            #pragma omp critical
            {
                lowerBoundTable[board.hash()] = {bestEval, depth};
            }
        } else {
            std::sort(newMoves.begin(), newMoves.end(), [](const auto& a, const auto& b) {
                return a.second < b.second;
            });

            #pragma omp critical
            {
                upperBoundTable[board.hash()] = {bestEval, depth};
            }
        }

        moves = newMoves;
        previousPV = PV;
        evals[depth] = bestEval;
        candidateMove[depth] = bestMove;

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
        std::cout << analysis << std::endl;

        auto currentTime = std::chrono::high_resolution_clock::now();
        bool spendTooMuchTime = false;
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();

        if (duration > timeLimit) {
            timeLimitExceeded = true;
        }

        bool stableEval = true;
        // if (depth >= 2 && depth <= ENGINE_DEPTH &&
        //     (std::abs(evals[depth] - evals[depth - 2]) > 75 || candidateMove[depth] != candidateMove[depth - 1])) {

        //     stableEval = false; 
        // }

        // A position is unstable if the average evaluation changes by more than 50cp from 4 plies ago
        if (depth >= 4 && depth <= ENGINE_DEPTH) {  // Ensure we have at least 4 plies to compare
            const int N = 4;  // Consider last 4 plies
            int sumEvalDiff = 0;

            for (int i = depth - 1; i >= depth - N; i--) {  // Look at last 4 plies inclusively
                sumEvalDiff += std::abs(evals[i] - evals[i - 1]);
            }

            double avgEvalDiff = sumEvalDiff / (double)N;

            if (avgEvalDiff > 50) {
                stableEval = false; 
            }
        }

        if (timeLimitExceeded && PV.size() == depth && stableEval) {
            break; // Break out of the loop if the time limit is exceeded and the evaluation is stable.
        }


        if (PV.size() == depth) {
            // Increase depth if the PV is full. If not, we have a cutoff at some LMR. Redo the search.  
            depth++; 

            if (depth > ENGINE_DEPTH) {
                // Break out of the loop if the maximum allowed depth is reached
                break;
            }
        }

        // Check for PV.size < depth because of checkmate, stalemate, or repetition
        Board localBoard = board;
        for (int j = 0; j < PV.size(); j++) {
            localBoard.makeMove(PV[j]);
        }
        if (localBoard.isGameOver().first != GameResultReason::NONE) {
            break;
        }

        // Final safeguard: quit if we spend too much time. 
        if (duration > 3 * timeLimit) {
            break;
        }
    }

    return bestMove; 
}