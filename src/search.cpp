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
#include <atomic>

using namespace chess;

typedef std::uint64_t U64;

std::unordered_set<std::string> mateThreatFens;

// Constants and global variables
std::unordered_map<std::uint64_t, std::pair<int, int>> lowerBoundTable; // Hash -> (eval, depth)
std::unordered_map<std::uint64_t, Move> whiteHashMove; // Hash -> move

std::unordered_map<std::uint64_t, std::pair<int, int>> upperBoundTable; // Hash -> (eval, depth)
std::unordered_map<std::uint64_t, Move> blackHashMove; // Hash -> move

const int maxTableSize = 10000000; // Maximum size of the transposition table
U64 nodeCount; 
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

    if (killerMoves[depth].size() < 2) {
        killerMoves[depth].push_back(move);
    } else {
        killerMoves[depth][1] = killerMoves[depth][0];
        killerMoves[depth][0] = move;
    }
    
}

/*-------------------------------------------------------------------------------------------- 
    Check for tactical threats beside the obvious checks, captures, and promotions.
    To be expanded. 
--------------------------------------------------------------------------------------------*/

bool tacticalMove(Board& board, Move move) {
    Color color = board.sideToMove();
    PieceType type = board.at<Piece>(move.from()).type();

    Bitboard theirKing = board.pieces(PieceType::KING, !color);

    int destinationIndex = move.to().index();   
    int destinationFile = destinationIndex % 8;
    int destinationRank = destinationIndex / 8;

    int theirKingFile = theirKing.lsb() % 8;
    int theirKingRank = theirKing.lsb() / 8;

    if (manhattanDistance(move.to(), Square(theirKing.lsb())) <= 3) {
        return true;
    }

    if (type == PieceType::ROOK || type == PieceType::QUEEN) {
        if (abs(destinationFile - theirKingFile) <= 1 && 
            abs(destinationRank - theirKingRank) <= 1) {
            return true;
        }
    }

    return false;
}

/*--------------------------------------------------------------------------------------------
    Late move reduction. No reduction for the first few moves, checks, or when in check.
    Reduce less on captures, checks, killer moves, etc.
    isPV is true if the node is a principal variation node. However, right now it's not used 
    since our move ordering is not that good.
--------------------------------------------------------------------------------------------*/
int lateMoveReduction(Board& board, Move move, int i, int depth, bool isPV) {

    Color color = board.sideToMove();
    board.makeMove(move);
    bool isCheck = board.inCheck(); 
    board.unmakeMove(move);

    bool isCapture = board.isCapture(move);
    bool inCheck = board.inCheck();
    bool isKillerMove = std::find(killerMoves[depth].begin(), killerMoves[depth].end(), move) != killerMoves[depth].end();
    bool isPromoting = isQueenPromotion(move);
    bool isTactical = tacticalMove(board, move);

    bool reduceLess = isCapture || isKillerMove || isCheck || inCheck || isPromoting ;
    int reduction = 0;
    int nonPVReduction = isPV ? 0 : 1;
    int k1 = 1;
    int k2 = 5;

    if (i <= k1 || mopUp || depth <= 3 ) { 
        return depth - 1;
    } else if (i <= k2 || reduceLess) {
        return depth - 2 - nonPVReduction;
    } else {
        return depth - 3 - nonPVReduction;
    }
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

int quiescence(Board& board, int depth, int alpha, int beta) {
    
    #pragma critical
    {
        nodeCount++;
    }
    
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
    Color color = board.sideToMove();
    movegen::legalmoves<movegen::MoveGenType::CAPTURE>(moves, board);
    std::vector<std::pair<Move, int>> candidateMoves;
    candidateMoves.reserve(moves.size());
    int biggestMaterialGain = 0;
    

    for (const auto& move : moves) {
        auto victim = board.at<Piece>(move.to());
        auto attacker = board.at<Piece>(move.from());
        int victimValue = pieceValues[static_cast<int>(victim.type())];
        int attackerValue = pieceValues[static_cast<int>(attacker.type())];

        int priority = victimValue - attackerValue;
        biggestMaterialGain = std::max(biggestMaterialGain, priority);
        candidateMoves.push_back({move, priority});
        
    }

    // Delta pruning
    // const int deltaMargin = 950;
    // if (whiteTurn && standPat + biggestMaterialGain + deltaMargin <= alpha) {
    //     // If for white, the biggest material gain + the static evaluation + the delta margin 
    //     // is less than alpha, this is unlikely to raise alpha for white so we can prune
    //     return alpha;
    // } else if (!whiteTurn && standPat - biggestMaterialGain - deltaMargin >= beta) {
    //     // Same logic for black
    //     return beta;
    // }

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
            int extension) {

    #pragma omp critical
    {
        nodeCount++;
    }

    bool whiteTurn = board.sideToMove() == Color::WHITE;
    bool endGameFlag = gamePhase(board) <= 12;
    Color color = board.sideToMove();

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
    
    # pragma omp critical
    {
        if ((whiteTurn && transTableLookUp(lowerBoundTable, hash, depth, storedEval) && storedEval >= beta) ||
            (!whiteTurn && transTableLookUp(upperBoundTable, hash, depth, storedEval) && storedEval <= alpha)) {
            found = true;
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

    bool isPV = (alpha < beta - 1); // Principal variation node flag
    int standPat = evaluate(board);

    bool pruningCondition = !board.inCheck() 
                            && !mopUp && !endGameFlag 
                            && std::max(alpha, beta) < INF/4 
                            && std::min(alpha, beta) > -INF/4;

    //  Futility pruning
    if (depth < 3 && pruningCondition) {

        int margin = depth * 130;

        if (whiteTurn && standPat - margin > beta) {
            // If it's white turn and the static evaluation - margin is greater than beta, 
            // then it is considered to be too good for white so we can prune
            return standPat - margin;
        } else if (!whiteTurn && standPat + margin < alpha) {
            // If it's black turn and the static evaluation + margin is less than alpha,
            // then it is considered to be too good for black so we can prune
            return standPat + margin;
        }
    }

    // Razoring: Skip deep search if the position is too weak. Only applied to non-PV nodes.
    if (depth <= 3 && pruningCondition && !isPV) {
        int standPat = evaluate(board); // Static evaluation
        int razorMargin = 300 + (depth - 1) * 60; // Threshold increases slightly with depth

        if ((whiteTurn && standPat + razorMargin < alpha)
            || (!whiteTurn && standPat - razorMargin > beta)) {
            // If the position is too weak (unlikely to raise alpha for white or lower beta for black), skip deep search
            return quiescence(board, quiescenceDepth, alpha, beta);
        } 
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
                    nullEval = alphaBeta(board, depth - reduction, beta - 1, beta, quiescenceDepth, nullPV, false, extension);
                } else {
                    nullEval = alphaBeta(board, depth - reduction, alpha, alpha + 1, quiescenceDepth, nullPV, false, extension);
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

    for (int i = 0; i < moves.size(); i++) {

        Move move = moves[i].first;
        std::vector<Move> childPV;

        int eval = 0;
        int nextDepth = lateMoveReduction(board, move, i, depth, isPV); 
        
        if (i > 0) {
            leftMost = false;
        }

        board.makeMove(move);
        
        // Check for extensions
        bool isCheck = board.inCheck();
        bool extensionFlag = (isCheck || tacticalMove(board, move)) && extension > 0; // if the move is a check, extend the search
        if (extensionFlag) {
            nextDepth += 3; // Extend the search by 1 ply
            extension--; // Decrement the extension counter
        }

        if (isPV || leftMost) {
            // full window search for PV nodes and leftmost line (I think nodes in leftmost line should be PV nodes, but this is just in case)
            eval = alphaBeta(board, nextDepth, alpha, beta, quiescenceDepth, childPV, leftMost, extension);
        } else {
            if (whiteTurn) {
                eval = alphaBeta(board, nextDepth, alpha, alpha + 1, quiescenceDepth, childPV, leftMost, extension);
            } else {
                eval = alphaBeta(board, nextDepth, beta - 1, beta, quiescenceDepth, childPV, leftMost, extension);
            }
        }
        
        board.unmakeMove(move);

        // re-search if white raises alpha or black lowers beta in reduced depth
        bool interesting = whiteTurn ? eval > alpha : eval < beta;

        if (leftMost || (interesting && nextDepth < depth - 1)) {
            board.makeMove(move);
            eval = alphaBeta(board, depth - 1, alpha, beta, quiescenceDepth, childPV, leftMost, extension);
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
            #pragma omp critical
            {
                updateKillerMoves(move, depth);
            }
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
    omp_set_num_threads(numThreads);

    bool whiteTurn = board.sideToMove() == Color::WHITE; 
    std::vector<std::pair<Move, int>> moves;
    std::vector<Move> globalPV (maxDepth);

    if (board.us(Color::WHITE).count() == 1 || board.us(Color::BLACK).count() == 1) {
        mopUp = true;
    }


    globalQuiescenceDepth = quiescenceDepth;
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
    int extension = 2;

    while (depth <= maxDepth) {
        globalMaxDepth = depth;
        nodeCount = 0;
        
        // Track the best move for the current depth
        Move bestMove = Move();
        int bestEval = whiteTurn ? -INF : INF;
        std::vector<std::pair<Move, int>> newMoves;
        std::vector<Move> PV; // Principal variation

        if (depth == baseDepth) {
            moves = orderedMoves(board, depth, previousPV, false);
        }

        auto iterationStartTime = std::chrono::high_resolution_clock::now();

        #pragma omp for schedule(dynamic, 1)
        for (int i = 0; i < moves.size(); i++) {
            bool leftMost = (i == 0);

            Move move = moves[i].first;
            std::vector<Move> childPV; 
            
        
            Board localBoard = board;
            bool newBestFlag = false;  
            int nextDepth = lateMoveReduction(localBoard, move, i, depth, true);

            // Check for extensions
            bool isCheck = board.inCheck();
            bool extensionFlag = (isCheck || tacticalMove(board, move)) && extension > 0; // if the move is a check, extend the search
            if (extensionFlag) {
                nextDepth += 3;
                extension--; 
            }

            int eval = whiteTurn ? -INF : INF;
            int aspiration;

            if (depth == 1) {
                aspiration = evaluate(localBoard); // at depth = 1, aspiration = static evaluation
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
                                extension);
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
                if ((whiteTurn && eval > bestEval) || (!whiteTurn && eval < bestEval)) {
                    newBestFlag = true;
                }
            }

            if (newBestFlag && nextDepth < depth - 1) {
                localBoard.makeMove(move);
                eval = alphaBeta(localBoard, depth - 1, -INF, INF, quiescenceDepth, childPV, leftMost, extension);
                localBoard.unmakeMove(move);
            }

            #pragma omp critical
            newMoves.push_back({move, eval});

            #pragma omp critical
            {
                if ((whiteTurn && eval > bestEval) || 
                    (!whiteTurn && eval < bestEval)) {
                    bestEval = eval;
                    bestMove = move;

                    PV.clear();
                    PV.push_back(move);
                    for (auto& move : childPV) {
                        PV.push_back(move);
                    }
                }
            }
        }

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

        std::string depthStr = "depth " +  std::to_string(depth);
        std::string scoreStr = "score cp " + std::to_string(bestEval);
        std::string nodeStr = "nodes " + std::to_string(nodeCount);

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
        spendTooMuchTime = duration > 3 * timeLimit;

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

    return candidateMove[depth];
}
