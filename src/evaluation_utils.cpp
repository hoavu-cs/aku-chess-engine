#include "chess.hpp"
#include "evaluation_utils.hpp"

using namespace chess;

/*
Conventions:
We use value += penalty and not value -= penalty.
*/

// Constants for the evaluation function
const int PAWN_VALUE = 100;
const int KNIGHT_VALUE = 320;
const int BISHOP_VALUE = 330;
const int ROOK_VALUE = 500;
const int QUEEN_VALUE = 900;
const int KING_VALUE = 5000;
const int CASTLE_VALUE = 100;
const int END_PIECE_COUNT = 14;
const int DOUBLE_PAWN_PENALTY = -20;

// Constants for attacking the enemy king
const int ATTACK_KING_BONUS_QUEEN = 30;
const int ATTACK_KING_BONUS_KNIGHT = 10;

const int ATTACK_KING_BONUS_QUEEN_DIST = 4;
const int ATTACK_KING_BONUS_KNIGHT_DIST = 4;

const int KING_PAWN_SHIELD_BONUS = 10;

// Function to check if the given color has lost castling rights
bool hasLostCastlingRights(const chess::Board& board, chess::Color color, chess::Board::CastlingRights::Side side) {
    return !board.castlingRights().has(color, side);
}

// Knight piece-square table
const int KNIGHT_PENALTY_TABLE_WHITE[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};

const int KNIGHT_PENALTY_TABLE_BLACK[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};

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

// Bishop piece-square table
const int BISHOP_PENALTY_TABLE_WHITE[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,   5,   0,   0,   0,   0,   5, -10,
    -10,  10,  10,  10,  10,  10,  10, -10,
    -10,   0,  10,  10,  10,  10,   0, -10,
    -10,   5,   5,  10,  10,   5,   5, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -20, -10, -10, -10, -10, -10, -10, -20,
};

const int BISHOP_PENALTY_TABLE_BLACK[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
};


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

/// Piece-square tables for pawns 
const int PAWN_PENALTY_TABLE_WHITE_MID[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10,-20,-20, 10, 10,  5,
     5, -5,-10,  0,  0,-10, -5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5,  5, 10, 25, 25, 10,  5,  5,
    10, 10, 20, 30, 30, 20, 10, 10,
    50, 50, 50, 50, 50, 50, 50, 50,
     0,  0,  0,  0,  0,  0,  0,  0
};

const int PAWN_PENALTY_TABLE_BLACK_MID[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};

const int PAWN_PENALTY_TABLE_WHITE_END[64] = {
     0,    0,    0,    0,    0,    0,    0,    0,  // 1st rank
    -20,  -30,  -10,  -40,  -40,  -10,  -30,  -20, // 2nd rank (stronger penalty for unpushed pawns)
      0,    0,   20,   50,   50,   20,    0,    0, // 3rd rank
      0,    5,   50,  100,  100,   50,    5,    0, // 4th rank (strong bonuses for central pawns)
     10,   10,   70,  150,  150,   70,   10,   10, // 5th rank
     20,   50,  100,  200,  200,  100,   50,   20, // 6th rank
     80,   80,  150,  300,  300,  150,   80,   80, // 7th rank
      0,    0,    0,    0,    0,    0,    0,    0  // 8th rank
};

const int PAWN_PENALTY_TABLE_BLACK_END[64] = {
     0,    0,    0,    0,    0,    0,    0,    0,  // 8th rank
     80,   80,  150,  300,  300,  150,   80,   80, // 7th rank
     20,   50,  100,  200,  200,  100,   50,   20, // 6th rank
     10,   10,   70,  150,  150,   70,   10,   10, // 5th rank
      0,    5,   50,  100,  100,   50,    5,    0, // 4th rank (strong bonuses for central pawns)
      0,    0,   20,   50,   50,   20,    0,    0, // 3rd rank
    -20,  -30,  -10,  -40,  -40,  -10,  -30,  -20, // 2nd rank (stronger penalty for unpushed pawns)
      0,    0,    0,    0,    0,    0,    0,    0  // 1st rank
};



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

// Rook piece-square table
const int ROOK_PENALTY_TABLE_WHITE[64] = {
       0,    0,    0,    5,    5,    0,    0,    0,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
       5,   10,   10,   10,   10,   10,   10,    5,
       0,    0,    0,    0,    0,    0,    0,    0,
};

const int ROOK_PENALTY_TABLE_BLACK[64] = {
       0,    0,    0,    0,    0,    0,    0,    0,
       5,   10,   10,   10,   10,   10,   10,    5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
       0,    0,    0,    5,    5,    0,    0,    0,
};


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

const int ROOK_OPEN_FILE_BONUS = 30;
const int ROOK_SEMI_OPEN_FILE_BONUS = 15;

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

// Queen piece-square table
const int QUEEN_PENALTY_WHITE[64] = {
     -20,  -10,  -10,   -5,   -5,  -10,  -10,  -20,
     -10,    0,    0,    0,    0,    5,    0,  -10,
     -10,    0,    5,    5,    5,    5,    5,  -10,
      -5,    0,    5,    5,    5,    5,    0,    0,
      -5,    0,    5,    5,    5,    5,    0,   -5,
     -10,    0,    5,    5,    5,    5,    0,  -10,
     -10,    0,    0,    0,    0,    0,    0,  -10,
     -20,  -10,  -10,   -5,   -5,  -10,  -10,  -20,
};

const int QUEEN_PENALTY_BLACK[64] = {
     -20,  -10,  -10,   -5,   -5,  -10,  -10,  -20,
     -10,    0,    0,    0,    0,    0,    0,  -10,
     -10,    0,    5,    5,    5,    5,    0,  -10,
      -5,    0,    5,    5,    5,    5,    0,   -5,
      -5,    0,    5,    5,    5,    5,    0,    0,
     -10,    0,    5,    5,    5,    5,    5,  -10,
     -10,    0,    0,    0,    0,    5,    0,  -10,
     -20,  -10,  -10,   -5,   -5,  -10,  -10,  -20,
};

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

// King piece-square table
const int KING_PENALTY_TABLE_WHITE_MID[64] = {
      20,   75,  75,    0,    0,   10,  75,   20,
      20,   20,    0,    0,    0,    0,   20,   20,
     -10,  -20,  -20,  -20,  -20,  -20,  -20,  -10,
     -20,  -30,  -30,  -40,  -40,  -30,  -30,  -20,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
};

const int KING_PENALTY_TABLE_BLACK_MID[64] = {
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -20,  -30,  -30,  -40,  -40,  -30,  -30,  -20,
     -10,  -20,  -20,  -20,  -20,  -20,  -20,  -10,
      20,   20,    0,    0,    0,    0,   20,   20,
      20,   75,  75,    0,    0,   10,  75,   20,
};

const int KING_PENALTY_TABLE_WHITE_END[64] = {
     -50,  -30,  -30,  -30,  -30,  -30,  -30,  -50,
     -30,  -30,    0,    0,    0,    0,  -30,  -30,
     -30,  -10,   20,   30,   30,   20,  -10,  -30,
     -30,  -10,   30,   40,   40,   30,  -10,  -30,
     -30,  -10,   30,   40,   40,   30,  -10,  -30,
     -30,  -10,   20,   30,   30,   20,  -10,  -30,
     -30,  -20,  -10,    0,    0,  -10,  -20,  -30,
     -50,  -40,  -30,  -20,  -20,  -30,  -40,  -50,
};

const int KING_PENALTY_TABLE_BLACK_END[64] = {
     -50,  -40,  -30,  -20,  -20,  -30,  -40,  -50,
     -30,  -20,  -10,    0,    0,  -10,  -20,  -30,
     -30,  -10,   20,   30,   30,   20,  -10,  -30,
     -30,  -10,   30,   40,   40,   30,  -10,  -30,
     -30,  -10,   30,   40,   40,   30,  -10,  -30,
     -30,  -10,   20,   30,   30,   20,  -10,  -30,
     -30,  -30,    0,    0,    0,    0,  -30,  -30,
     -50,  -30,  -30,  -30,  -30,  -30,  -30,  -50,
};

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

    // if (hasLostCastlingRights(board, Color::WHITE, Board::CastlingRights::Side::KING_SIDE) &&
    //         hasLostCastlingRights(board, Color::WHITE, Board::CastlingRights::Side::QUEEN_SIDE)) {
    //     whiteScore -= 100;
    // } 

    // if (hasLostCastlingRights(board, Color::BLACK, Board::CastlingRights::Side::KING_SIDE) &&
    //         hasLostCastlingRights(board, Color::BLACK, Board::CastlingRights::Side::QUEEN_SIDE)) {
    //     blackScore -= 100;
    // }

    // Special case: Mop-up Evaluation
    // chess::Color ourColor = board.sideToMove();
    // chess::Color enemyColor = !board.sideToMove();
    // int mateScore = 0;

    // if (board.pieces(PieceType::PAWN, enemyColor).count() == 0 && board.pieces(PieceType::KNIGHT, enemyColor).count() == 0 &&
    //     board.pieces(PieceType::BISHOP, enemyColor).count() == 0 && board.pieces(PieceType::ROOK, enemyColor).count() == 0 &&
    //     board.pieces(PieceType::QUEEN, enemyColor).count() == 0) { 
    //     // If the enemy has no piece left, we can checkmate
    //     whiteScore = 0;
    //     blackScore = 0;
    //     Bitboard enemyKing = board.pieces(PieceType::KING, enemyColor);
    //     Bitboard ourKing = board.pieces(PieceType::KING, ourColor);

    //     chess::Square enemyKingSQ = chess::Square(enemyKing.lsb());
    //     chess::Square ourKingSQ = chess::Square(ourKing.lsb());

    //     chess::File fileA = chess::File::FILE_A;
    //     chess::Rank rank1 = chess::Rank::RANK_1;
    //     chess::File fileE = chess::File::FILE_E;
    //     chess::Rank rank4 = chess::Rank::RANK_4;

    //     chess::Square centerSQ = chess::Square(fileE, rank4);

    //     int kingCMD = manhattanDistance(ourKingSQ, enemyKingSQ);
    //     int queenCMD = 15;
    //     int rookCMD = 15;
    //     int enemyKingCornerDist = manhattanDistance(enemyKingSQ, chess::Square(fileA, rank1));

    //     if (board.pieces(PieceType::QUEEN, ourColor).count() > 0) {
    //         Bitboard ourQueen = board.pieces(PieceType::QUEEN, ourColor);
    //         chess::Square ourQueenSQ = chess::Square(ourQueen.lsb());
    //         queenCMD = manhattanDistance(ourQueenSQ, enemyKingSQ);
    //     }

    //     if (board.pieces(PieceType::ROOK, ourColor).count() > 0) {
    //         Bitboard ourRook = board.pieces(PieceType::ROOK, ourColor);
    //         chess::Square ourRookSQ = chess::Square(ourRook.lsb());
    //         rookCMD = manhattanDistance(ourRookSQ, enemyKingSQ);
    //     }

    //     if (ourColor == Color::WHITE) {
    //         whiteScore = mateScore;
    //         blackScore = manhattanDistance(centerSQ, enemyKingSQ);
    //     } else {
    //         blackScore = mateScore;
    //         whiteScore = manhattanDistance(centerSQ, enemyKingSQ);
    //     }

    //     return whiteScore - blackScore;
    // }

    return whiteScore - blackScore;
}

