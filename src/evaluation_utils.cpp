#include "chess.hpp"
#include "evaluation_utils.hpp"

using namespace chess;

/*
Conventions:
We use value += penalty and not value -= penalty.
*/

// Compute the value of the knights on the board
int knightValue(const chess::Board& board, int baseValue, chess::Color color) {
    Bitboard knights = board.pieces(PieceType::KNIGHT, color);
    Bitboard enemyKing = board.pieces(PieceType::KING, !color);

    chess::Square enemyKingSQ = chess::Square(enemyKing.lsb()); // Get the square of the enemy king
    int value = 0;

    const int* KNIGHT_PENALTY_TABLE;
    if (color == Color::WHITE) {
        KNIGHT_PENALTY_TABLE = KNIGHT_PENALTY_TABLE_WHITE;
    } else {
        KNIGHT_PENALTY_TABLE = KNIGHT_PENALTY_TABLE_BLACK;
    } 

    while (!knights.empty()) {
        value += baseValue;

        // Get the least significant bit (square index) and create a square object
        int sqIndex = knights.lsb();
        value += KNIGHT_PENALTY_TABLE[sqIndex];

        // Add bonus for being close to the enemy king
        if (!enemyKing.empty()) {
            chess::Square knightSQ = chess::Square(sqIndex); // Create a square object for the knight
            if (chess::Square::distance(knightSQ, enemyKingSQ) <= ATTACK_KING_BONUS_KNIGHT_DIST) {
                value += ATTACK_KING_BONUS_KNIGHT;
            }
        }
        knights.clear(sqIndex);
    }

    return value;
}

// Compute the value of the bishops on the board
int bishopValue(const chess::Board& board, int baseValue, chess::Color color) {
    Bitboard bishops = board.pieces(PieceType::BISHOP, color);
    int value = 0;

    while (!bishops.empty()) {
        value += baseValue;
        // Get the least significant bit (square index) and create a square object
        int sqIndex = bishops.lsb();
        if (color == chess::Color::WHITE) {
            value += BISHOP_PENALTY_TABLE_WHITE[sqIndex];
        } else {
            value += BISHOP_PENALTY_TABLE_BLACK[sqIndex];
        }
        bishops.clear(sqIndex);
    }

    return value;
}

// Compute the value of the pawns on the board
int pawnValue(const chess::Board& board, int baseValue, chess::Color color) {
    
    Bitboard pawns = board.pieces(PieceType::PAWN, color);
    int value = 0;
    // Traverse each pawn
    const int* penaltyTable;

    if (countPieces(board) >= END_PIECE_COUNT) {
        if (color == Color::WHITE) {
            penaltyTable = PAWN_PENALTY_TABLE_WHITE_MID;
        } else {
            penaltyTable = PAWN_PENALTY_TABLE_BLACK_MID;
        }
    } else {
        if (color == Color::WHITE) {
            penaltyTable = PAWN_PENALTY_TABLE_WHITE_END;
        } else {
            penaltyTable = PAWN_PENALTY_TABLE_BLACK_END;
        }
    }

    int files[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    while (!pawns.empty()) {
        int sqIndex = pawns.lsb(); // Get the index of the least significant bit and remove it
        int file = sqIndex % 8; // Get the file of the pawn
        files[file]++; // Increment the count of pawns on the file

        value += baseValue; // Add the base value
        if (color == Color::WHITE) {
            value += penaltyTable[sqIndex];
        } else {
            value += penaltyTable[sqIndex];
        }

        pawns.clear(sqIndex); // Clear the processed pawn
    }

    for (int i = 0; i < 8; i++) {
        if (files[i] > 1) {
            value += DOUBLE_PAWN_PENALTY * (files[i] - 1);
        }
    }

    return value;
}

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


bool isOpenFile(const chess::Board& board, const File& file) {
    // Get bitboards for white and black pawns
    Bitboard whitePawns = board.pieces(PieceType::PAWN, Color::WHITE);
    Bitboard blackPawns = board.pieces(PieceType::PAWN, Color::BLACK);

    // Generate the mask for the given file
    Bitboard mask = generateFileMask(file);

    // A file is open if it has no pawns of either color
    return !(whitePawns & mask) && !(blackPawns & mask);
}

bool isSemiOpenFile(const chess::Board& board, const File& file, Color color) {
    // Get the bitboard for the pawns of the given color
    Bitboard ownPawns = board.pieces(PieceType::PAWN, color);

    // Generate the mask for the given file
    Bitboard mask = generateFileMask(file);

    // A file is semi-open if it has no pawns of the given color
    return !(ownPawns & mask);
}

// Compute the total value of the rooks on the board
int rookValue(const chess::Board& board, int baseValue, chess::Color color) {
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
            value += ROOK_PENALTY_TABLE_WHITE[sqIndex];
        } else {
            value += ROOK_PENALTY_TABLE_BLACK[sqIndex];
        }

        // Check for open and semi-open files
        if (isOpenFile(board, file)) {
            value += ROOK_OPEN_FILE_BONUS; // Add open file bonus
        } else if (isSemiOpenFile(board, file, color)) {
            value += ROOK_SEMI_OPEN_FILE_BONUS; // Add semi-open file bonus
        }

        rooks.clear(sqIndex); // Remove the processed rook
    }
    return value;
}


// Compute the total value of the queens on the board
int queenValue(const chess::Board& board, int baseValue, chess::Color color) {
    Bitboard queens = board.pieces(PieceType::QUEEN, color);
    Bitboard enemyKing = board.pieces(PieceType::KING, !color);

    chess::Square enemyKingSQ = chess::Square(enemyKing.lsb()); // Get the square of the enemy king
    int value = 0;

    while (!queens.empty()) {
        int sqIndex = queens.lsb(); // Get the index of the least significant bit and remove it
        value += baseValue; // Add the base value
        if (color == Color::WHITE) {
            value += QUEEN_PENALTY_WHITE[sqIndex];
        } else {
            value += QUEEN_PENALTY_BLACK[sqIndex];
        }

        // Add bonus for being close to the enemy king
        if (!enemyKing.empty()) {
            chess::Square queenSQ = chess::Square(sqIndex); // Create a square object for the queen
            if (chess::Square::distance(queenSQ, enemyKingSQ) <= ATTACK_KING_BONUS_QUEEN_DIST) {
                value += ATTACK_KING_BONUS_QUEEN;
            }
        }

        queens.clear(sqIndex); // Clear the processed queen
    }
    return value;
}



// Compute the value of the kings on the board
int kingValue(const chess::Board& board, int baseValue, chess::Color color) {
    Bitboard king = board.pieces(PieceType::KING, color);
    Bitboard CASTLE_SQUARES;
    
    int pieceCount = countPieces(board);
    bool isEndGame = (pieceCount <= END_PIECE_COUNT);

    int value = baseValue;
    int sqIndex = king.lsb();

    if (color == chess::Color::WHITE) {
        if (isEndGame) {
            value += KING_PENALTY_TABLE_WHITE_END[sqIndex];
        } else {
            value += KING_PENALTY_TABLE_WHITE_MID[sqIndex];
            // Add bonus if there are pawns close to the king
            Bitboard whitePawns = board.pieces(PieceType::PAWN, Color::WHITE);
            while (!whitePawns.empty()) {
                int pawnIndex = whitePawns.lsb();
                int manhattanDist = manhattanDistance(Square(pawnIndex), Square(sqIndex));
                if (manhattanDist <= 2) {
                    value += KING_PAWN_SHIELD_BONUS;
                } 
                whitePawns.clear(pawnIndex);
            }
        }
    } else {
        if (isEndGame) {
            value += KING_PENALTY_TABLE_BLACK_END[sqIndex];
        } else {
            value += KING_PENALTY_TABLE_BLACK_MID[sqIndex];
            // Add bonus if there are pawns close to the king
            Bitboard blackPawns = board.pieces(PieceType::PAWN, Color::BLACK);
            while (!blackPawns.empty()) {
                int pawnIndex = blackPawns.lsb();
                int manhattanDist = manhattanDistance(Square(pawnIndex), Square(sqIndex));
                if (manhattanDist <= 2) {
                    value += KING_PAWN_SHIELD_BONUS;
                } 
                blackPawns.clear(pawnIndex);
            }
        }
    } 
    return value;
}


// Function to count the total number of pieces on the board
int countPieces(const chess::Board& board) {

    int pieceCount = 0;

    // Traverse all piece types and colors
    const PieceType allPieceTypes[] = {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, 
                                           PieceType::ROOK, PieceType::QUEEN, PieceType::KING};

    for (const auto& type : allPieceTypes) {
        // Count white pieces of the given type
        Bitboard whitePieces = board.pieces(type, Color::WHITE);
        pieceCount += whitePieces.count(); // Add the count of set bits

        // Count black pieces of the given type
        Bitboard blackPieces = board.pieces(type, Color::BLACK);
        pieceCount += blackPieces.count(); // Add the count of set bits
    }

    return pieceCount;
}

// Function to count the total number of legal moves for the current side
int countLegalMoves(const Board& board) {
    Movelist moves;
    movegen::legalmoves(moves, board); // Generate all legal moves for the current side
    return moves.size();
}

// Function to compute the Manhattan distance between two squares
int manhattanDistance(const Square& sq1, const Square& sq2) {
    return std::abs(sq1.file() - sq2.file()) + std::abs(sq1.rank() - sq2.rank());
}

// Function to evaluate the board position
int evaluate(const chess::Board& board) {
    // Initialize totals
    int whiteScore = 0;
    int blackScore = 0;

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

    return whiteScore - blackScore;
}

