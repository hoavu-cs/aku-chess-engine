#include "chess.hpp"
#include "evaluation_utils.hpp"
#include <tuple>
#include <unordered_map>
#include <cstdint>
#include <map>
#include <omp.h> 

using namespace chess; 

/*------------------------------------------------------------------------
    Helper Functions
------------------------------------------------------------------------*/

// Return true if the game is in the endgame phase 
bool isEndGame(const Board& board) {
    const int materialThreshold = 32;
    const int knightValue = 3, bishopValue = 3, rookValue = 5, queenValue = 9;

    int totalValue = board.pieces(PieceType::KNIGHT, Color::WHITE).count() * knightValue
                  + board.pieces(PieceType::BISHOP, Color::WHITE).count() * bishopValue
                  + board.pieces(PieceType::ROOK, Color::WHITE).count() * rookValue
                  + board.pieces(PieceType::QUEEN, Color::WHITE).count() * queenValue
                  + board.pieces(PieceType::KNIGHT, Color::BLACK).count() * knightValue
                  + board.pieces(PieceType::BISHOP, Color::BLACK).count() * bishopValue
                  + board.pieces(PieceType::ROOK, Color::BLACK).count() * rookValue
                  + board.pieces(PieceType::QUEEN, Color::BLACK).count() * queenValue;

    if (totalValue <= materialThreshold) {
        return true;
    } else {
        return false;
    }
}

// Generate a bitboard mask for the specified file
Bitboard generateFileMask(const File& file) {
    constexpr Bitboard fileMasks[] = {
        0x0101010101010101ULL, // File A
        0x0202020202020202ULL, // File B
        0x0404040404040404ULL, // File C
        0x0808080808080808ULL, // File D
        0x1010101010101010ULL, // File E
        0x2020202020202020ULL, // File F
        0x4040404040404040ULL, // File G
        0x8080808080808080ULL  // File H
    };

    // Convert File to index (0 = A, ..., 7 = H)
    auto index = static_cast<int>(file.internal());

    // Return the corresponding file mask
    if (index >= 0 && index < 8) {
        return Bitboard(fileMasks[index]);
    }

    // Return an empty bitboard for invalid files
    return Bitboard(0ULL);
}

// Check if the given square is a passed pawn
bool isPassedPawn(int squareIndex, Color color, const Bitboard& theirPawns) {
    // Determine the rank and file of the pawn
    int file = squareIndex % 8;  // File index (0 = A, ..., 7 = H)
    int rank = squareIndex / 8;  // Rank index (0 = 1, ..., 7 = 8)

    // Create file masks for the current file and adjacent files
    Bitboard currentFileMask = generateFileMask(File(file));
    Bitboard leftFileMask = file > 0 ? generateFileMask(File(file - 1)) : Bitboard(0ULL);
    Bitboard rightFileMask = file < 7 ? generateFileMask(File(file + 1)) : Bitboard(0ULL);

    // Combine the masks into one for the current and adjacent files
    Bitboard relevantFilesMask = currentFileMask | leftFileMask | rightFileMask;

    // Create a mask for squares in front of the pawn
    Bitboard frontMask;
    if (color == Color::WHITE) {
        frontMask = relevantFilesMask & Bitboard(0xFFFFFFFFFFFFFFFFULL << ((rank + 1) * 8));
    } else { 
        frontMask = relevantFilesMask & Bitboard(0xFFFFFFFFFFFFFFFFULL >> ((7 - rank) * 8));
    }

    // Check if any their pawns are in the front mask
    return (frontMask & theirPawns).empty();
} 

// Function to compute the Manhattan distance between two squares
int manhattanDistance(const Square& sq1, const Square& sq2) {
    return std::abs(sq1.file() - sq2.file()) + std::abs(sq1.rank() - sq2.rank());
}

// Check if a square is an outpost
bool isOutPost(const Board& board, int sqIndex, Color color) {
    // Determine the file and rank of the square
    int file = sqIndex % 8, rank = sqIndex / 8;

    if (color == Color::WHITE) {
        if (rank < 4) {
            return false;
        }
    } else {
        if (rank > 3) {
            return false;
        }
    }

    bool isSupported = false;

    Bitboard ourPawns = board.pieces(PieceType::PAWN, color);

    while (ourPawns) {
        int pawnIndex = ourPawns.lsb();
        int pawnFile = pawnIndex % 8, pawnRank = pawnIndex / 8;

        if (std::abs(pawnFile - file) == 1) {
            if (color == Color::WHITE) {
                if (pawnRank == rank - 1) {
                    isSupported = true;
                    break;
                }
            } else {
                if (pawnRank == rank + 1) {
                    isSupported = true;
                    break;
                }
            }
        }
        ourPawns.clear(pawnIndex);
    }

    if (!isSupported) {
        return false;
    }
    
    Bitboard theirPawns = board.pieces(PieceType::PAWN, !color);
    while (theirPawns) {
        int pawnIndex = theirPawns.lsb();

        int pawnFile = pawnIndex % 8, pawnRank = pawnIndex / 8;
        if (std::abs(pawnFile - file) == 1) {
            if (color == Color::WHITE) {
                if (pawnRank > rank) {
                    return false;
                }
            } else {
                if (pawnRank < rank) {
                    return false;
                }
            }
        }
        theirPawns.clear(pawnIndex);
    }

    return true;
}

/*------------------------------------------------------------------------
 Main Functions 
------------------------------------------------------------------------*/

// Compute the value of the pawns on the board
int pawnValue(const Board& board, int baseValue, Color color, bool endGameFlag) {

    Bitboard ourPawns = board.pieces(PieceType::PAWN, color);
    Bitboard theirPawns = board.pieces(PieceType::PAWN, !color);

    std::uint64_t ourPawnsBits = ourPawns.getBits();
    std::uint64_t theirPawnsBits = theirPawns.getBits();

    std::tuple<std::uint64_t, std::uint64_t, bool> key = 
            std::make_tuple(ourPawnsBits, theirPawnsBits, endGameFlag);

    // constants
    const int passedPawnBonus = 50;
    const int centralPawnBonus = 20;
    const int isolatedPawnPenalty = 20;
    const int doubledPawnPenalty = 20;
    const int* pawnTable;

    if (color == Color::WHITE) {
        if (endGameFlag) {
            pawnTable = whitePawnTableEnd;
        } else {
            pawnTable = whitePawnTableMid;
        }
    } else {
        if (endGameFlag) {
            pawnTable = blackPawnTableEnd;
        } else {
            pawnTable = blackPawnTableMid;
        }
    }

    int files[8] = {0};
    int value = 0;
    int advancedPawnBonus = endGameFlag ? 5 : 2;

    Bitboard theirPieces = board.pieces(PieceType::BISHOP, !color) 
                            | board.pieces(PieceType::KNIGHT, !color) 
                            | board.pieces(PieceType::ROOK, !color) 
                            | board.pieces(PieceType::QUEEN, !color); 
                            
    Bitboard ourPawnsCopy = ourPawns;
    while (!ourPawns.empty()) {
        int sqIndex = ourPawns.lsb(); 
        int file = sqIndex % 8; // Get the file of the pawn
        files[file]++; // Increment the count of pawns on the file
        ourPawns.clear(sqIndex);
    }
    ourPawns = ourPawnsCopy;

    while (!ourPawns.empty()) {
        int sqIndex = ourPawns.lsb(); 
        
        value += baseValue; 
        value += pawnTable[sqIndex]; 

        int file = sqIndex % 8;
        int rank = sqIndex / 8; 

        if (file > 0 && file < 7) {
            if (files[file - 1] == 0 && files[file + 1] == 0) {
                value -= 10; 
            }
        }

        if (file > 0 && file < 7) {
            if (files[file - 1] == 0 && files[file + 1] == 0) {
                value -= isolatedPawnPenalty; 
            }
        }

        if (file == 0 && files[1] == 0) {
            value -= isolatedPawnPenalty;
        }

        if (file == 7 && files[6] == 0) {
                value -= isolatedPawnPenalty; 
        }    

        if (file == 3 || file == 4) {
            value += centralPawnBonus; 
        }

        if (isPassedPawn(sqIndex, color, theirPawns)) {
            value += passedPawnBonus;
        }

        if (color == Color::WHITE) {
            value += rank * advancedPawnBonus;
        } else {
            value += (7 - rank) * advancedPawnBonus;
        }

        ourPawns.clear(sqIndex);
    }
    
    // Add penalty for doubled pawns
    for (int i = 0; i < 8; i++) {
        if (files[i] > 1) {
            value -= (files[i] - 1) * doubledPawnPenalty;
        }
    }

    return value;
}

// Compute the value of the knights on the board
int knightValue(const Board& board, int baseValue, Color color, bool endGameFlag) {

    // Constants
    const int* knightTable;
    const int attackPenalty = 10;
    const int outpostBonus = 20;
 
    if (color == Color::WHITE) {
        if (endGameFlag) {
            knightTable = whiteKnightTableEnd;
        } else {
            knightTable = whiteKnightTableMid;
        }
    } else {
        if (endGameFlag) {
            knightTable = blackKnightTableEnd;
        } else {
            knightTable = blackKnightTableMid;
        }
    } 

    Bitboard knights = board.pieces(PieceType::KNIGHT, color);
    int value = 0;

    while (!knights.empty()) {
        value += baseValue;
        int sqIndex = knights.lsb();
        value += knightTable[sqIndex];

        Bitboard attackers = attacks::attackers(board, !color, Square(sqIndex));
        Bitboard theirPawns = board.pieces(PieceType::PAWN, !color);

        if (attackers & theirPawns) {
            value -= attackPenalty;
        }

        if (isOutPost(board, sqIndex, color)) {
            value += outpostBonus;
        }

        knights.clear(sqIndex);
    }

    return value;
}

// Compute the value of the bishops on the board
int bishopValue(const Board& board, int baseValue, Color color, bool endGameFlag) {

    // Constants
    const int bishopPairBonus = 30;
    const int mobilityBonus = 2;
    const int attackedPenalty = 10;
    const int outpostBonus = 20;
    const int *bishopTable;

    if (color == Color::WHITE) {
        if (endGameFlag) {
            bishopTable = whiteBishopTableEnd;
        } else {
            bishopTable = whiteBishopTableMid;
        }
    } else {
        if (endGameFlag) {
            bishopTable = blackBishopTableEnd;
        } else {
            bishopTable = blackBishopTableMid;
        }
    }

    Bitboard bishops = board.pieces(PieceType::BISHOP, color);
    int value = 0;
    
    if (bishops.count() >= 2) {
        value += bishopPairBonus;
    }
 
    while (!bishops.empty()) {
        value += baseValue;
        int sqIndex = bishops.lsb();
        value += bishopTable[sqIndex];
        Bitboard bishopMoves = attacks::bishop(Square(sqIndex), board.occ());
        value += bishopMoves.count() * mobilityBonus;

        Bitboard attackers = attacks::attackers(board, !color, Square(sqIndex));
        Bitboard theirPawns = board.pieces(PieceType::PAWN, !color);
        
        if (attackers & theirPawns) {
            value -= attackedPenalty;
        }

        if (isOutPost(board, sqIndex, color)) {
            value += outpostBonus;
        }

        bishops.clear(sqIndex);
    }

    return value;
}


// Compute the total value of the rooks on the board
int rookValue(const Board& board, int baseValue, Color color, bool endGameFlag) {

    // Constants
    const double mobilityBonus = 2;
    const int* rookTable;
    const int attackedPenalty = 10;

    if (color == Color::WHITE) {
        if (endGameFlag) {
            rookTable = whiteRookTableEnd;
        } else {
            rookTable = whiteRookTableMid;
        }
    } else {
        if (endGameFlag) {
            rookTable = blackRookTableEnd;
        } else {
            rookTable = blackRookTableMid;
        }
    }

    Bitboard rooks = board.pieces(PieceType::ROOK, color);
    int value = 0;

    while (!rooks.empty()) {
        int sqIndex = rooks.lsb(); 

        value += baseValue; // Add the base value of the rook
        value += rookTable[sqIndex]; // Add the value from the piece-square table

        Bitboard rookMoves = attacks::rook(Square(sqIndex), board.occ());
        value += mobilityBonus * rookMoves.count();

        Bitboard attackers = attacks::attackers(board, !color, Square(sqIndex));
        Bitboard theirQueen = board.pieces(PieceType::QUEEN, !color);

        if (attackers & ~theirQueen) {
            value -= attackedPenalty;
        }

        rooks.clear(sqIndex); // Remove the processed rook
    }
    return value;
}

// Compute the total value of the queens on the board
int queenValue(const Board& board, int baseValue, Color color, bool endGameFlag) {

    // Constants
    const int* queenTable;
    const int mobilityBonus = 1;
    const int attackedPenalty = 15;

    if (color == Color::WHITE) {
        if (endGameFlag) {
            queenTable = whiteQueenTableEnd;
        } else {
            queenTable = whiteQueenTableMid;
        }
    } else {
        if (endGameFlag) {
            queenTable = blackQueenTableEnd;
        } else {
            queenTable = blackQueenTableMid;
        }
    }
    
    Bitboard queens = board.pieces(PieceType::QUEEN, color);
    Bitboard theirKing = board.pieces(PieceType::KING, !color);

    Square theirKingSQ = Square(theirKing.lsb()); // Get the square of the their king
    int value = 0;

    while (!queens.empty()) {
        int sqIndex = queens.lsb(); 
        int queenRank = sqIndex / 8, queenFile = sqIndex % 8;
        value += baseValue; 
        value += queenTable[sqIndex]; 

        Bitboard queenMoves = attacks::queen(Square(sqIndex), board.occ());
        value += mobilityBonus * queenMoves.count();

        Bitboard attackers = attacks::attackers(board, !color, Square(sqIndex));
        
        if (attackers) {
            value -= attackedPenalty;
        }

        queens.clear(sqIndex); 
    }
    return value;
}

// Compute the value of the kings on the board
int kingValue(const Board& board, int baseValue, Color color, bool endGameFlag) {

    // Constants
    const int pawnShieldBonus = 20;
    const int pieceProtectionBonus = 15;
    const int* kingTable;

    if (color == Color::WHITE) {
        if (endGameFlag) {
            kingTable = whiteKingTableEnd;
        } else {
            kingTable = whiteKingTableMid;
        }
    } else {
        if (endGameFlag) {
            kingTable = blackKingTableEnd;
        } else {
            kingTable = blackKingTableMid;
        }
    }

    Bitboard king = board.pieces(PieceType::KING, color);
    const PieceType allPieceTypes[] = {PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN};
    
    int value = baseValue;
    int sqIndex = king.lsb();
    int kingRank = sqIndex / 8, kingFile = sqIndex % 8;
    Bitboard pawns = board.pieces(PieceType::PAWN, color);

    value += kingTable[sqIndex];
    

    if (!endGameFlag) {
        // King protection by pawns
        while (!pawns.empty()) {
            int pawnIndex = pawns.lsb();
            int pawnRank = pawnIndex / 8, pawnFile = pawnIndex % 8;
            // if the pawn is in front of the king and on an adjacent file, add shield bonus
            if (color == Color::WHITE 
                        && pawnRank == kingRank + 1 && std::abs(pawnFile - kingFile) <= 1) {
                value += pawnShieldBonus;
            } else if (color == Color::BLACK 
                        && pawnRank == kingRank - 1 && std::abs(pawnFile - kingFile) <= 1) {
                value += pawnShieldBonus; 
            }
            pawns.clear(pawnIndex);
        }
        
        // King protection by pieces
        for (const auto& type : allPieceTypes) {
            Bitboard pieces = board.pieces(type, !color);

            while (!pieces.empty()) {
                int pieceIndex = pieces.lsb();
                if (color == Color::WHITE) {
                    if (pieceIndex / 8 > sqIndex / 8 && manhattanDistance(Square(pieceIndex), Square(sqIndex)) <= 4) 
                        value += pieceProtectionBonus; // if the piece is in front of the king with distance <= 2
                } else {
                    if (pieceIndex / 8 < sqIndex / 8 && manhattanDistance(Square(pieceIndex), Square(sqIndex)) <= 4) 
                        value += pieceProtectionBonus; // if the piece is in front of the king with distance <= 2
                }
                pieces.clear(pieceIndex);
            }
        }

        // Penalty for being attacked
        Bitboard attackers;
        
        // Get the bitboard of attackers
        for (const auto& sq : adjSquares.at(sqIndex)) {
            attackers = attackers | attacks::attackers(board, !color, Square(sq));
        }

        double attackWeight = 0;
        double threatScore = 0;

        // The more attackers, the higher the penalty
        switch (attackers.count()) {
            case 0: attackWeight = 0; break;
            case 1: attackWeight = 0; break;
            case 2: attackWeight = 0.50; break;
            case 3: attackWeight = 0.75; break;
            case 4: attackWeight = 1.00; break;
            case 5: attackWeight = 1.25; break;
            case 6: attackWeight = 1.50; break;
            case 7: attackWeight = 1.70; break;
            case 8: attackWeight = 2.00; break;
            default: break;
        }

        while (attackers) {
            int attackerIndex = attackers.lsb();
            Piece attacker = board.at(Square(attackerIndex));

            if (attacker.type() == PieceType::PAWN) {
                threatScore += attackWeight * 10;
            } else if (attacker.type() == PieceType::KNIGHT) {
                threatScore += attackWeight * 20;
            } else if (attacker.type() == PieceType::BISHOP) {
                threatScore += attackWeight * 20;
            } else if (attacker.type() == PieceType::ROOK) {
                threatScore += attackWeight * 40;
            } else if (attacker.type() == PieceType::QUEEN) {
                threatScore += attackWeight * 90;
            }

            attackers.clear(attackerIndex);
        }
        
        value -= static_cast<int>(threatScore);
    }

    return value;
}

// Function to evaluate the board position
int evaluate(const Board& board) {
    // Initialize totals
    int whiteScore = 0;
    int blackScore = 0;

    // Mop-up phase: if only their king is left
    Color theirColor = (board.sideToMove() == Color::WHITE) ? Color::BLACK : Color::WHITE;
    bool endGameFlag = isEndGame(board);

    Bitboard theirPieces = board.pieces(PieceType::PAWN, theirColor) | board.pieces(PieceType::KNIGHT, theirColor) | 
                           board.pieces(PieceType::BISHOP, theirColor) | board.pieces(PieceType::ROOK, theirColor) | 
                           board.pieces(PieceType::QUEEN, theirColor);

    Bitboard ourPieces =  board.pieces(PieceType::ROOK, board.sideToMove()) | board.pieces(PieceType::QUEEN, board.sideToMove());

    if (theirPieces.count() == 0 && ourPieces.count() > 0) {
        Square ourKing = Square(board.pieces(PieceType::KING, board.sideToMove()).lsb());
        Square theirKing = Square(board.pieces(PieceType::KING, theirColor).lsb());
        Square E4 = Square(28);

        int kingDist = manhattanDistance(ourKing, theirKing);
        int distToCenter = manhattanDistance(theirKing, E4);
        int ourMaterial = 900 * board.pieces(PieceType::QUEEN, board.sideToMove()).count() + 
                          500 * board.pieces(PieceType::ROOK, board.sideToMove()).count() + 
                          300 * board.pieces(PieceType::BISHOP, board.sideToMove()).count() + 
                          300 * board.pieces(PieceType::KNIGHT, board.sideToMove()).count() + 
                          100 * board.pieces(PieceType::PAWN, board.sideToMove()).count(); // avoid throwing away pieces
        int score = 5000 +  distToCenter + (14 - kingDist);

        return board.sideToMove() == Color::WHITE ? score : -score;
    }

    // Traverse each piece type
    const PieceType allPieceTypes[] = {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, 
                                           PieceType::ROOK, PieceType::QUEEN, PieceType::KING};

    for (const auto& type : allPieceTypes) {
        // Determine base value of the current piece type
        int baseValue = 0;

        switch (type.internal()) {
            case PieceType::PAWN: baseValue = PAWN_VALUE; break;
            case PieceType::KNIGHT: baseValue = KNIGHT_VALUE; break;
            case PieceType::BISHOP: baseValue = BISHOP_VALUE; break;
            case PieceType::ROOK: baseValue = ROOK_VALUE; break;
            case PieceType::QUEEN: baseValue = QUEEN_VALUE; break;
            case PieceType::KING: baseValue = KING_VALUE; break;
            default: break;
        }

        // Process white pieces
        if (type == PieceType::KNIGHT) {
            whiteScore += knightValue(board, baseValue, Color::WHITE, endGameFlag);
            blackScore += knightValue(board, baseValue, Color::BLACK, endGameFlag);
        } else if (type == PieceType::BISHOP) {
            whiteScore += bishopValue(board, baseValue, Color::WHITE, endGameFlag);
            blackScore += bishopValue(board, baseValue, Color::BLACK, endGameFlag);
        } else if (type == PieceType::KING) {
            whiteScore += kingValue(board, baseValue, Color::WHITE, endGameFlag);
            blackScore += kingValue(board, baseValue, Color::BLACK, endGameFlag);
        }  else if (type == PieceType::PAWN) {
            whiteScore += pawnValue(board, baseValue, Color::WHITE, endGameFlag);
            blackScore += pawnValue(board, baseValue, Color::BLACK, endGameFlag);
        } else if (type == PieceType::ROOK) {
            whiteScore += rookValue(board, baseValue, Color::WHITE, endGameFlag);
            blackScore += rookValue(board, baseValue, Color::BLACK, endGameFlag);
        } else if (type == PieceType::QUEEN) {
            whiteScore += queenValue(board, baseValue, Color::WHITE, endGameFlag);
            blackScore += queenValue(board, baseValue, Color::BLACK, endGameFlag);
        } 
    }

    // Avoid trading pieces for pawns in middle game
    const int knightValue = 3, bishopValue = 3, rookValue = 5, queenValue = 9;
    int whitePieceValue = queenValue * board.pieces(PieceType::QUEEN, Color::WHITE).count() + 
                        rookValue * board.pieces(PieceType::ROOK, Color::WHITE).count() + 
                        bishopValue * board.pieces(PieceType::BISHOP, Color::WHITE).count() + 
                        knightValue * board.pieces(PieceType::KNIGHT, Color::WHITE).count();

    int blackPieceValue = queenValue * board.pieces(PieceType::QUEEN, Color::BLACK).count() + 
                        rookValue * board.pieces(PieceType::ROOK, Color::BLACK).count() + 
                        bishopValue * board.pieces(PieceType::BISHOP, Color::BLACK).count() + 
                        knightValue * board.pieces(PieceType::KNIGHT, Color::BLACK).count();

    if (whitePieceValue > blackPieceValue) {
        whiteScore += 50;
    } else if (blackPieceValue > whitePieceValue) {
        blackScore += 50;
    }

    return whiteScore - blackScore;
}
