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

std::unordered_set<std::string> mateThreatFens;

// Constants and global variables
std::unordered_map<std::uint64_t, std::pair<int, int>> lowerBoundTable; // Hash -> (eval, depth)
std::unordered_map<std::uint64_t, Move> whiteHashMove; // Hash -> move

std::unordered_map<std::uint64_t, std::pair<int, int>> upperBoundTable; // Hash -> (eval, depth)
std::unordered_map<std::uint64_t, Move> blackHashMove; // Hash -> move

const int maxTableSize = 10000000; // Maximum size of the transposition table
std::vector<U64> nodeCount(1000, 0); // Node count for each thread
std::vector<Move> previousPV; // Principal variation from the previous iteration
std::vector<std::vector<Move>> killerMoves(1000); // Killer moves

int globalMaxDepth = 0; // Maximum depth of current search
int globalQuiescenceDepth = 0; // Quiescence depth of current search
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

// Transposition table lookup.
bool transTableLookUp(std::unordered_map<std::uint64_t, 
                        std::pair<int, int>>& table, 
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
bool isQueenPromotion(const Move& move) {

    if (move.typeOf() & Move::PROMOTION) {
        if (move.promotionType() == PieceType::QUEEN) {
            return true;
        } 
    } 

    return false;
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
    Late move reduction. No reduction for the first few moves, checks, or when in check.
    Reduce less on captures, checks, killer moves, etc.
    isPV is true if the node is a principal variation node. However, right now it's not used 
    since our move ordering is not that good.
--------------------------------------------------------------------------------------------*/
int lateMoveReduction(const Board& board, Move move, int i, int depth, bool isPV) {

    Bitboard theirKing = board.pieces(PieceType::KING, !board.sideToMove());
    bool mateThreat = manhattanDistance(move.to(), Square(theirKing.lsb())) <= 3;

    Board localBoard = board;
    Color color = board.sideToMove();
    localBoard.makeMove(move);

    bool isCheck = localBoard.inCheck(); 
    bool isCapture = board.isCapture(move);
    bool inCheck = board.inCheck();
    bool isKillerMove = std::find(killerMoves[depth].begin(), killerMoves[depth].end(), move) != killerMoves[depth].end();
    int deepReduction = depth > 13 ? depth - 13 : 0; // We can increase 13 if we can search deeper faster

    if (i <= 2 || depth <= 3  || mopUp || isKillerMove || isCheck || inCheck) {
        return depth - 1;
    } else if (i <= 5 || isCapture) {
        return depth - 2 - deepReduction;
    } else {
        return depth - 3 - deepReduction;
    }

    // Log formula: very fast but misses some shallow tactics (it's better to make this work consistently)
    // reduction = static_cast<int>(std::max(1.0, std::floor(1.0 + log (depth) * log(i) / 1.5 )));
}

// Generate a prioritized list of moves based on their tactical value
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
    std::uint64_t hash = board.hash();

    // Move ordering 1. promotion 2. captures 3. killer moves 4. hash 5. checks 6. quiet moves
    for (const auto& move : moves) {
        int priority = 0;
        bool quiet = false;
        int moveIndex = move.from().index() * 64 + move.to().index();
        int ply = globalMaxDepth - depth;
        bool hashMove = false;

        // Previous PV move > hash moves > captures/killer moves > checks > quiet moves
        if ((whiteTurn && whiteHashMove.count(hash) && whiteHashMove[hash] == move) ||
        (!whiteTurn && blackHashMove.count(hash) && blackHashMove[hash] == move)) {
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
            priority = 2000; // Killer moves
        } else if (isQueenPromotion(move)) {
            priority = 6000; 
        } else if (board.isCapture(move)) { 
            auto victim = board.at<Piece>(move.to());
            auto attacker = board.at<Piece>(move.from());\
            int victimValue = pieceValues[static_cast<int>(victim.type())];
            int attackerValue = pieceValues[static_cast<int>(attacker.type())];
            priority = 4000 + victimValue - attackerValue;
        } else {
            board.makeMove(move);
            bool isCheck = board.inCheck();
            board.unmakeMove(move);

            if (isCheck) {
                priority = 3000;
            } else {
                quiet = true;
                priority = 0;// quietPriority(board, move);
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

    for (const auto& move : quietCandidates) {
        candidates.push_back(move);
    }

    return candidates;
}

int quiescence(Board& board, int depth, int alpha, int beta, int threadID) {
    
    nodeCount[threadID]++;
    
    if (depth <= 0) {
        return evaluate(board);
    }

    bool whiteTurn = board.sideToMove() == Color::WHITE;

    int standPat = evaluate(board);
    int bestScore = standPat;
        
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
    candidateMoves.reserve(moves.size());

    for (const auto& move : moves) {
        if (!board.isCapture(move) && !isQueenPromotion(move)) {
            continue;
        }

        if (isQueenPromotion(move)) {

            candidateMoves.push_back({move, 5000});

        } else if (board.isCapture(move)) {

            auto victim = board.at<Piece>(move.to());
            auto attacker = board.at<Piece>(move.from());
            int victimValue = pieceValues[static_cast<int>(victim.type())];
            int attackerValue = pieceValues[static_cast<int>(attacker.type())];

            int priority = victimValue - attackerValue;
            candidateMoves.push_back({move, priority});
        } 
    }

    std::sort(candidateMoves.begin(), candidateMoves.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    for (const auto& [move, priority] : candidateMoves) {

        board.makeMove(move);
        int score = 0;
        score = quiescence(board, depth - 1, alpha, beta, threadID);

        board.unmakeMove(move);

        if (whiteTurn) {
            if (score >= beta) { 
                return beta;
            }
            bestScore = std::max(bestScore, score);
        } else {
            if (score <= alpha) {
                return alpha;
            }
            bestScore = std::min(bestScore, score);
        }
    }
    return bestScore;

}

int alphaBeta(Board& board, 
            int depth, 
            int alpha, 
            int beta, 
            int quiescenceDepth,
            std::vector<Move>& PV,
            bool leftMost,
            int extension,
            int threadID) {


    nodeCount[threadID]++;

    bool whiteTurn = board.sideToMove() == Color::WHITE;
    Color color = board.sideToMove();
    bool endGameFlag = gamePhase(board) <= 12;

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
    

    if ((whiteTurn && transTableLookUp(lowerBoundTable, hash, depth, storedEval) && storedEval >= beta) ||
        (!whiteTurn && transTableLookUp(upperBoundTable, hash, depth, storedEval) && storedEval <= alpha)) {
        found = true;
    }
    

    if (found) {
        return storedEval;
    } 

    if (depth <= 0) {
        int quiescenceEval = quiescence(board, quiescenceDepth, alpha, beta, threadID);
        
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
                int reduction = 3 + depth / 4;

                if (whiteTurn) {
                    nullEval = alphaBeta(board, depth - reduction, beta - 1, beta, quiescenceDepth, nullPV, false, extension, threadID);
                } else {
                    nullEval = alphaBeta(board, depth - reduction, alpha, alpha + 1, quiescenceDepth, nullPV, false, extension,threadID);
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

    std::vector<std::pair<Move, int>> moves = orderedMoves(board, depth, previousPV, leftMost);
    int bestEval = whiteTurn ? -INF : INF;
    bool isPV = (alpha < beta - 1); // Principal variation node flag

    for (int i = 0; i < moves.size(); i++) {

        Move move = moves[i].first;
        std::vector<Move> childPV;

        // Futility pruning: to avoid risky behaviors at low depths, only prune when globalMaxDepth >= 8
        const int futilityMargins[4] = {0, 200, 300, 500};
        if (depth <= 3 
            && !board.inCheck() 
            && !endGameFlag 
            && !mopUp 
            && !leftMost 
            && !board.isCapture(move)
            && alpha <= 5000 && beta >= -5000
        ) {
            int futilityMargin = futilityMargins[depth];
            int standPat = evaluate(board);
            if (whiteTurn) {
                if (standPat + futilityMargin < alpha) {
                    return standPat + futilityMargin;
                }
            } else {
                if (standPat - futilityMargin > beta) {
                    return standPat - futilityMargin;
                }
            }
        }

        int eval = 0;
        int nextDepth = lateMoveReduction(board, move, i, depth, isPV); 
        
        if (i > 0) {
            leftMost = false;
        }

        board.makeMove(move);
        
        // Check for extensions
        bool isCheck = board.inCheck();
        bool extensionFlag = isCheck && extension > 0; // if the move is a check, extend the search
        if (extensionFlag) {
            nextDepth++;
            extension--; // Decrement the extension counter
        }

        // TODO: PVS when having a good move ordering (searching with null window)
        if (isPV) {
            eval = alphaBeta(board, nextDepth, alpha, beta, quiescenceDepth, childPV, leftMost, extension, threadID);
        } else {
            if (whiteTurn) {
                eval = alphaBeta(board, nextDepth, alpha, alpha + 1, quiescenceDepth, childPV, leftMost, extension, threadID);
            } else {
                eval = alphaBeta(board, nextDepth, beta - 1, beta, quiescenceDepth, childPV, leftMost, extension, threadID);
            }
        }
        
        //eval = alphaBeta(board, nextDepth, alpha, beta, quiescenceDepth, childPV, leftMost, extension, threadID);

        board.unmakeMove(move);

        // re-search if white raises alpha or black lowers beta in reduced depth
        bool interesting = whiteTurn ? eval > alpha : eval < beta;

        if (leftMost || (interesting && nextDepth < depth - 1)) {
            board.makeMove(move);
            eval = alphaBeta(board, depth - 1, alpha, beta, quiescenceDepth, childPV, leftMost, extension, threadID);
            board.unmakeMove(move);
        }

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
    nodeCount = std::vector<U64>(1000, 0);
    bool timeLimitExceeded = false;

    Move bestMove = Move(); 
    int bestEval = (board.sideToMove() == Color::WHITE) ? -INF : INF;
    bool whiteTurn = board.sideToMove() == Color::WHITE; 

    std::vector<std::pair<Move, int>> moves;
    std::vector<Move> globalPV (maxDepth);

    if (board.us(Color::WHITE).count() == 1 || board.us(Color::BLACK).count() == 1) {
        mopUp = true;
    }


    globalQuiescenceDepth = quiescenceDepth;
    omp_set_num_threads(numThreads);

    // Clear transposition tables
    #pragma omp critical
    {
        if (lowerBoundTable.size() + upperBoundTable.size() > maxTableSize) {
            lowerBoundTable = {};
            upperBoundTable = {};
            whiteHashMove = {};
            blackHashMove = {};
            clearPawnHashTable();
        }
    }
    
    const int baseDepth = 1;
    int apsiration = evaluate(board);
    int depth = baseDepth;
    int evals[ENGINE_DEPTH + 1];
    Move candidateMove[ENGINE_DEPTH + 1];

    while (depth <= maxDepth) {
        globalMaxDepth = depth;
        
        // Track the best move for the current depth
        Move currentBestMove = Move();
        int currentBestEval = whiteTurn ? -INF : INF;
        std::vector<std::pair<Move, int>> newMoves;
        std::vector<Move> PV; // Principal variation

        if (depth == baseDepth) {
            moves = orderedMoves(board, depth, previousPV, false);
        }

        auto iterationStartTime = std::chrono::high_resolution_clock::now();
        bool unfinished = false;

        #pragma omp for schedule(static)
        for (int i = 0; i < moves.size(); i++) {

            bool leftMost = (i == 0);

            Move move = moves[i].first;
            std::vector<Move> childPV; 
            int extension = 2;
        
            Board localBoard = board;
            bool newBestFlag = false;  
            int nextDepth = lateMoveReduction(localBoard, move, i, depth, true);
            int eval = whiteTurn ? -INF : INF;
            int aspiration;

            if (depth == 1) {
                aspiration = evaluate(localBoard); // if at depth = 1, aspiration = static evaluation
            } else {
                aspiration = evals[depth - 1]; // otherwise, aspiration = previous depth evaluation
            }

            // aspiration window search
            int windowLeft = 50;
            int windowRight = 50;

            while (true) {
                localBoard.makeMove(move);
                eval = alphaBeta(localBoard, 
                                nextDepth, 
                                aspiration - windowLeft, 
                                aspiration + windowRight,
                                quiescenceDepth, 
                                childPV, 
                                leftMost, 
                                extension,
                                i);
                localBoard.unmakeMove(move);

                if (eval <= aspiration - windowLeft) {
                    windowLeft *= 2;
                } else if (eval >= aspiration + windowRight) {
                    windowRight *= 2;
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

            if (newBestFlag && nextDepth < depth - 1) {
                localBoard.makeMove(move);
                eval = alphaBeta(localBoard, depth - 1, -INF, INF, quiescenceDepth, childPV, leftMost, extension, i);
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

        // Sort the moves by evaluation for the next iteration
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

        U64 totalNodes = 0;
        for (int i = 0; i < numThreads; i++) {
            totalNodes += nodeCount[i];
        }

        std::string depthStr = "depth " +  std::to_string(depth);
        std::string scoreStr = "score cp " + std::to_string(bestEval);
        std::string nodeStr = "nodes " + std::to_string(totalNodes);

        auto iterationEndTime = std::chrono::high_resolution_clock::now();
        std::string timeStr = "time " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(iterationEndTime - iterationStartTime).count());

        std::string pvStr = "pv ";
        for (const auto& move : PV) {
            pvStr += uci::moveToUci(move) + " ";
        }

        std::string analysis = "info " + depthStr + " " + scoreStr + " " +  nodeStr + " " + timeStr + " " + " " + pvStr;
        std::cout << analysis << std::endl;

        auto currentTime = std::chrono::high_resolution_clock::now();
        bool spendTooMuchTime = false;
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();

        timeLimitExceeded = duration > timeLimit;
        spendTooMuchTime = duration > 2 * timeLimit;

        evals[depth] = bestEval;
        candidateMove[depth] = bestMove; 

        // Check for stable evaluation
        bool stableEval = true;
        if (depth >= 4 && depth <= ENGINE_DEPTH) {  
            for (int i = 0; i < 4; i++) {
                if (std::abs(evals[depth - i] - evals[depth - i - 1]) > 25) {
                    stableEval = false;
                    break;
                }
            }
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
    {
        if (lowerBoundTable.size() + upperBoundTable.size() > maxTableSize) {
            lowerBoundTable = {};
            upperBoundTable = {};
            whiteHashMove = {};
            blackHashMove = {};
            clearPawnHashTable();
        }
    }

    return bestMove; 
}