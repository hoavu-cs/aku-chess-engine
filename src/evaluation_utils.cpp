#include "chess.hpp"
#include "evaluation_utils.hpp"
#include <tuple>
#include <unordered_map>
#include <cstdint>
#include <map>
using namespace chess; 

long long hit = 0;

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
bool isPassedPawn(int squareIndex, Color color, const Bitboard& enemyPawns) {
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

    // Check if any enemy pawns are in the front mask
    return (frontMask & enemyPawns).empty();
}

// Check if the specified file is open (no pawns on it)
bool isOpenFile(const Board& board, const File& file) {
    // Get bitboards for white and black pawns
    Bitboard whitePawns = board.pieces(PieceType::PAWN, Color::WHITE);
    Bitboard blackPawns = board.pieces(PieceType::PAWN, Color::BLACK);

    // Generate the mask for the given file
    Bitboard mask = generateFileMask(file);

    // A file is open if it has no pawns of either color
    return !(whitePawns & mask) && !(blackPawns & mask);
}

// Check if the specified file is semi-open (no pawns of the given color)
bool isSemiOpenFile(const Board& board, const File& file, Color color) {
    // Get the bitboard for the pawns of the given color
    Bitboard ownPawns = board.pieces(PieceType::PAWN, color);

    // Generate the mask for the given file
    Bitboard mask = generateFileMask(file);

    // A file is semi-open if it has no pawns of the given color
    return !(ownPawns & mask);
}


/*------------------------------------------------------------------------
 Main Functions 
------------------------------------------------------------------------*/


// Compute the value of the knights on the board
int knightValue(const Board& board, int baseValue, Color color) {

    Bitboard knights = board.pieces(PieceType::KNIGHT, color);
    int value = 0;
    const int* knightTable;
    
    if (color == Color::WHITE) {
        knightTable = whiteKnightTable;
    } else {
        knightTable = blackKnightTable;
    } 

    while (!knights.empty()) {
        value += baseValue;
        int sqIndex = knights.lsb();
        value += knightTable[sqIndex];
        knights.clear(sqIndex);
    }

    return value;
}

// Compute the value of the bishops on the board
int bishopValue(const Board& board, int baseValue, Color color) {

    // Constants
    const int bishopPairBonus = 30;
    const int mobilityBonus = 1;

    Bitboard bishops = board.pieces(PieceType::BISHOP, color);
    int value = 0;
    
    if (bishops.count() >= 2) {
        value += bishopPairBonus;
    }

    while (!bishops.empty()) {
        value += baseValue;
        int sqIndex = bishops.lsb();
        if (color == Color::WHITE) {
            value += whiteBishopTable[sqIndex];
        } else {
            value += blackBishopTable[sqIndex];
        }

        Bitboard bishopMoves = attacks::bishop(Square(sqIndex), board.occ());
        value += bishopMoves.count() * mobilityBonus;
        bishops.clear(sqIndex);
    }

    return value;
}


// Compute the total value of the rooks on the board
int rookValue(const Board& board, int baseValue, Color color) {

    // Constants
    const int openFileBonus = 20;
    const int semiOpenFileBonus = 15;
    const int mobilityBonus = 1;

    Bitboard rooks = board.pieces(PieceType::ROOK, color);
    int value = 0;

    // Traverse each rook
    while (!rooks.empty()) {
        int sqIndex = rooks.lsb(); // Get the index of the least significant bit and remove it
        Square sq = Square(sqIndex); // Create a Square object
        File file = sq.file();       // Get the file of the rook

        value += baseValue; // Add the base value of the rook

        // Add piece-square table bonus
        if (color == Color::WHITE) {
            value += whiteRookTable[sqIndex];
        } else {
            value += blackRookTable[sqIndex];
        }

        // Check for open and semi-open files
        if (isOpenFile(board, file)) {
            value += openFileBonus; // Add open file bonus
        } else if (isSemiOpenFile(board, file, color)) {
            value += semiOpenFileBonus; // Add semi-open file bonus
        }

        Bitboard rookMoves = attacks::rook(Square(sq), board.occ());
        value += mobilityBonus * rookMoves.count();

        rooks.clear(sqIndex); // Remove the processed rook
    }
    return value;
}

// Compute the value of the pawns on the board
int pawnValue(const Board& board, int baseValue, Color color) {
    bool endGameFlag = isEndGame(board);
    // constants
    const int passedPawnBonus = 15;
    const int doublePawnPenalty = 40;
    const int centralPawnBonus = 20;
    const int isolatedPawnPenalty = 20;

    Bitboard ourPawns = board.pieces(PieceType::PAWN, color);
    Bitboard enemyPawns = board.pieces(PieceType::PAWN, !color);
    int files[8] = {0};
    int value = 0;
    int advancedPawnBonus = endGameFlag ? 3 : 0;

    Bitboard enemyPieces = board.pieces(PieceType::BISHOP, !color) | board.pieces(PieceType::KNIGHT, !color) | board.pieces(PieceType::ROOK, !color) | board.pieces(PieceType::QUEEN, !color); 
    int enemyPieceCount = enemyPieces.count();

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

        if (file == 3 || file == 4) {
            value += centralPawnBonus; // Add central pawn bonus
        }

        // Check if the pawn is a passed pawn
        if (isPassedPawn(sqIndex, color, enemyPawns)) {
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
            value += doublePawnPenalty * (files[i] - 1);
        }
    }

    return value;
}

// Compute the total value of the queens on the board
int queenValue(const Board& board, int baseValue, Color color) {
    Bitboard queens = board.pieces(PieceType::QUEEN, color);
    Bitboard enemyKing = board.pieces(PieceType::KING, !color);

    Square enemyKingSQ = Square(enemyKing.lsb()); // Get the square of the enemy king
    int value = 0;

    while (!queens.empty()) {
        int sqIndex = queens.lsb(); // Get the index of the least significant bit and remove it
        value += baseValue; // Add the base value
        if (color == Color::WHITE) {
            value += whiteQueenTable[sqIndex];
        } else {
            value += blackQueenTable[sqIndex];
        }
        
        queens.clear(sqIndex); // Clear the processed queen
    }
    return value;
}

// Compute the value of the kings on the board
int kingValue(const Board& board, int baseValue, Color color) {

    // Constants
    const int pawnShieldBonus = 20;
    const int pieceProtectionBonus = 20;
    const int openFilePenalty = 15;

    Bitboard king = board.pieces(PieceType::KING, color);
    const PieceType allPieceTypes[] = {PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN};
    
    int pieceCount = countPieces(board);
    bool endGameFlag = isEndGame(board);
    int value = baseValue;
    int sqIndex = king.lsb();

    const auto& kingTableMid = (color == Color::WHITE) ? whiteKingTableMid : blackKingTableMid;
    const auto& kingTableEnd = (color == Color::WHITE) ? whiteKingTableEnd : blackKingTableEnd;
    value += endGameFlag ? kingTableEnd[sqIndex] : kingTableMid[sqIndex];

    Bitboard pawns = board.pieces(PieceType::PAWN, color);

    if (!endGameFlag) {
        // King protection by pawns
        while (!pawns.empty()) {
            int pawnIndex = pawns.lsb();
            if (manhattanDistance(Square(pawnIndex), Square(sqIndex)) <= 2) {
                value += pawnShieldBonus;
            }
            pawns.clear(pawnIndex);
        }
        // King protection by pieces
        // for (const auto& type : allPieceTypes) {
        //     Bitboard pieces = board.pieces(type, !color);

        //     while (!pieces.empty()) {
        //         int pieceIndex = pieces.lsb();
        //         if (color == Color::WHITE) {
        //             if (pieceIndex / 8 > sqIndex / 8 && manhattanDistance(Square(pieceIndex), Square(sqIndex)) <= 4) 
        //                 value += pieceProtectionBonus; // if the piece is in front of the king with distance <= 2
        //         } else {
        //             if (pieceIndex / 8 < sqIndex / 8 && manhattanDistance(Square(pieceIndex), Square(sqIndex)) <= 4) 
        //                 value += pieceProtectionBonus; // if the piece is in front of the king with distance <= 2
        //         }
        //         pieces.clear(pieceIndex);
        //     }
        // }

        // Penalize king for being in an open file
        if (isOpenFile(board, Square(sqIndex).file()) || isSemiOpenFile(board, Square(sqIndex).file(), color)) {
            value -= openFilePenalty;
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
            case 2: attackWeight = 0.75; break;
            case 3: attackWeight = 0.80; break;
            case 4: attackWeight = 0.89; break;
            case 5: attackWeight = 0.94; break;
            case 6: attackWeight = 0.32; break;
            case 7: attackWeight = 0.97; break;
            case 8: attackWeight = 0.99; break;
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


// Function to count the total number of pieces on the board
int countPieces(const Board& board) {

    int pieceCount = 0;
    const PieceType allPieceTypes[] = {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, 
                                           PieceType::ROOK, PieceType::QUEEN, PieceType::KING};
    for (const auto& type : allPieceTypes) {
        Bitboard whitePieces = board.pieces(type, Color::WHITE);
        pieceCount += whitePieces.count();
        Bitboard blackPieces = board.pieces(type, Color::BLACK);
        pieceCount += blackPieces.count();
    }
    return pieceCount;
}

// Function to compute the Manhattan distance between two squares
int manhattanDistance(const Square& sq1, const Square& sq2) {
    return std::abs(sq1.file() - sq2.file()) + std::abs(sq1.rank() - sq2.rank());
}

int minDistance(const Square& sq1, const Square& sq2) {
    int dx = std::abs(sq1.file() - sq2.file());
    int dy = std::abs(sq1.rank() - sq2.rank());
    return std::min(dx, dy);
}

// Function to evaluate the board position
int evaluate(const Board& board) {
    // Initialize totals
    int whiteScore = 0;
    int blackScore = 0;

    // Mop-up phase: if only enemy king is left
    Color theirColor = (board.sideToMove() == Color::WHITE) ? Color::BLACK : Color::WHITE;

    Bitboard enemyPieces = board.pieces(PieceType::PAWN, theirColor) | board.pieces(PieceType::KNIGHT, theirColor) | 
                           board.pieces(PieceType::BISHOP, theirColor) | board.pieces(PieceType::ROOK, theirColor) | 
                           board.pieces(PieceType::QUEEN, theirColor);

    Bitboard ourPieces =  board.pieces(PieceType::ROOK, board.sideToMove()) | board.pieces(PieceType::QUEEN, board.sideToMove());

    if (enemyPieces.count() == 0 && ourPieces.count() > 0) {
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
            whiteScore += knightValue(board, baseValue, Color::WHITE);
            blackScore += knightValue(board, baseValue, Color::BLACK);
        } else if (type == PieceType::BISHOP) {
            whiteScore += bishopValue(board, baseValue, Color::WHITE);
            blackScore += bishopValue(board, baseValue, Color::BLACK);
        } else if (type == PieceType::KING) {
            whiteScore += kingValue(board, baseValue, Color::WHITE);
            blackScore += kingValue(board, baseValue, Color::BLACK);
        }  else if (type == PieceType::PAWN) {
            whiteScore += pawnValue(board, baseValue, Color::WHITE);
            blackScore += pawnValue(board, baseValue, Color::BLACK);
        } else if (type == PieceType::ROOK) {
            whiteScore += rookValue(board, baseValue, Color::WHITE);
            blackScore += rookValue(board, baseValue, Color::BLACK);
        } else if (type == PieceType::QUEEN) {
            whiteScore += queenValue(board, baseValue, Color::WHITE);
            blackScore += queenValue(board, baseValue, Color::BLACK);
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

    if (!isEndGame(board)) { // avoid trading pieces for pawns in middle game
        if (whitePieceValue > blackPieceValue) {
            whiteScore += 20;
        } else if (blackPieceValue > whitePieceValue) {
            blackScore += 20;
        }
    }

    return whiteScore - blackScore;
}

