#include "include/chess.hpp"
#include "evaluation_utils.hpp"

using namespace chess;

// Function to check if the given color has lost castling rights
bool hasLostCastlingRights(const chess::Board& board, chess::Color color, chess::Board::CastlingRights::Side side) {
    return !board.castlingRights().has(color, side);
}

// Knight piece-square table
constexpr int KNIGHT_PENALTY_TABLE[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};

// Compute the value of the knights on the board
int knightValue(const chess::Board& board, int baseValue, chess::Color color) {
    Bitboard knights = board.pieces(PieceType::KNIGHT, color);
    Bitboard START;
    int value = 0;

    while (!knights.empty()) {
        value += baseValue;

        // Get the least significant bit (square index) and create a square object
        int sqIndex = knights.lsb();
        value += KNIGHT_PENALTY_TABLE[sqIndex];
        knights.clear(sqIndex);
    }

    return value;
}

// Bishop piece-square table
constexpr int BISHOP_PENALTY_TABLE_WHITE[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,   5,   0,   0,   0,   0,   5, -10,
    -10,  10,  10,  10,  10,  10,  10, -10,
    -10,   0,  10,  10,  10,  10,   0, -10,
    -10,   5,   5,  10,  10,   5,   5, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -20, -10, -10, -10, -10, -10, -10, -20,
};

constexpr int BISHOP_PENALTY_TABLE_BLACK[64] = {
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
    Bitboard START;
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
constexpr int PAWN_PENALTY_TABLE_WHITE[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10,-20,-20, 10, 10,  5,
     5, -5,-10,  0,  0,-10, -5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5,  5, 10, 25, 25, 10,  5,  5,
    10, 10, 20, 30, 30, 20, 10, 10,
    50, 50, 50, 50, 50, 50, 50, 50,
     0,  0,  0,  0,  0,  0,  0,  0
};

constexpr int PAWN_PENALTY_TABLE_BLACK[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};

// Compute the value of the pawns on the board
int pawnValue(const chess::Board& board, int baseValue, chess::Color color) {
    
    Bitboard pawns = board.pieces(PieceType::PAWN, color);
    int value = 0;
    // Traverse each pawn
    while (!pawns.empty()) {
        int sqIndex = pawns.lsb(); // Get the index of the least significant bit and remove it
        value += baseValue; // Add the base value
        if (color == Color::WHITE) {
            value += PAWN_PENALTY_TABLE_WHITE[sqIndex];
        } else {
            value += PAWN_PENALTY_TABLE_BLACK[sqIndex];
        }
        pawns.clear(sqIndex); // Clear the processed pawn
    }
    return value;
}

// Rook piece-square table
constexpr int ROOK_PENALTY_TABLE_WHITE[64] = {
       0,    0,    0,    5,    5,    0,    0,    0,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
       5,   10,   10,   10,   10,   10,   10,    5,
       0,    0,    0,    0,    0,    0,    0,    0,
};

constexpr int ROOK_PENALTY_TABLE_BLACK[64] = {
       0,    0,    0,    0,    0,    0,    0,    0,
       5,   10,   10,   10,   10,   10,   10,    5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
       0,    0,    0,    5,    5,    0,    0,    0,
};

// Compute the total value of the rooks on the board
int rookValue(const chess::Board& board, int baseValue, chess::Color color) {
    Bitboard rooks = board.pieces(PieceType::ROOK, color);
    int value = 0;
    // Traverse each rook
    while (!rooks.empty()) {
        int sqIndex = rooks.lsb(); // Get the index of the least significant bit and remove it
        value += baseValue; // Add the base value
        if (color == Color::WHITE) {
            value += ROOK_PENALTY_TABLE_WHITE[sqIndex];
        } else {
            value += ROOK_PENALTY_TABLE_BLACK[sqIndex];
        }
        rooks.clear(sqIndex); // Clear the processed rook
    }
    return value;
}

// Queen piece-square table
constexpr int QUEEN_PENALTY_WHITE[64] = {
     -20,  -10,  -10,   -5,   -5,  -10,  -10,  -20,
     -10,    0,    0,    0,    0,    5,    0,  -10,
     -10,    0,    5,    5,    5,    5,    5,  -10,
      -5,    0,    5,    5,    5,    5,    0,    0,
      -5,    0,    5,    5,    5,    5,    0,   -5,
     -10,    0,    5,    5,    5,    5,    0,  -10,
     -10,    0,    0,    0,    0,    0,    0,  -10,
     -20,  -10,  -10,   -5,   -5,  -10,  -10,  -20,
};

constexpr int QUEEN_PENALTY_BLACK[64] = {
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
    int value = 0;
    // Traverse each queen
    while (!queens.empty()) {
        int sqIndex = queens.lsb(); // Get the index of the least significant bit and remove it
        value += baseValue; // Add the base value
        if (color == Color::WHITE) {
            value += QUEEN_PENALTY_WHITE[sqIndex];
        } else {
            value += QUEEN_PENALTY_BLACK[sqIndex];
        }
        queens.clear(sqIndex); // Clear the processed queen
    }
    return value;
}

// King piece-square table
constexpr int KING_PENALTY_TABLE_WHITE_MID[64] = {
      20,   30,  100,    0,    0,   10,  100,   20,
      20,   20,    0,    0,    0,    0,   20,   20,
     -10,  -20,  -20,  -20,  -20,  -20,  -20,  -10,
     -20,  -30,  -30,  -40,  -40,  -30,  -30,  -20,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
};

constexpr int KING_PENALTY_TABLE_BLACK_MID[64] = {
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -20,  -30,  -30,  -40,  -40,  -30,  -30,  -20,
     -10,  -20,  -20,  -20,  -20,  -20,  -20,  -10,
      20,   20,    0,    0,    0,    0,   20,   20,
      20,   30,  100,    0,    0,   10,  100,   20,
};

constexpr int KING_PENALTY_TABLE_WHITE_END[64] = {
     -50,  -30,  -30,  -30,  -30,  -30,  -30,  -50,
     -30,  -30,    0,    0,    0,    0,  -30,  -30,
     -30,  -10,   20,   30,   30,   20,  -10,  -30,
     -30,  -10,   30,   40,   40,   30,  -10,  -30,
     -30,  -10,   30,   40,   40,   30,  -10,  -30,
     -30,  -10,   20,   30,   30,   20,  -10,  -30,
     -30,  -20,  -10,    0,    0,  -10,  -20,  -30,
     -50,  -40,  -30,  -20,  -20,  -30,  -40,  -50,
};

constexpr int KING_PENALTY_TABLE_BLACK_END[64] = {
     -50,  -40,  -30,  -20,  -20,  -30,  -40,  -50,
     -30,  -20,  -10,    0,    0,  -10,  -20,  -30,
     -30,  -10,   20,   30,   30,   20,  -10,  -30,
     -30,  -10,   30,   40,   40,   30,  -10,  -30,
     -30,  -10,   30,   40,   40,   30,  -10,  -30,
     -30,  -10,   20,   30,   30,   20,  -10,  -30,
     -30,  -30,    0,    0,    0,    0,  -30,  -30,
     -50,  -30,  -30,  -30,  -30,  -30,  -30,  -50,
};

const int endGamePieceCount = 14;

// Compute the value of the kings on the board
int kingValue(const chess::Board& board, int baseValue, chess::Color color) {
    Bitboard pieces = board.pieces(PieceType::KING, color);
    Bitboard CASTLE_SQUARES;

    
    int pieceCount = countPieces(board);
    bool isEndGame = (pieceCount <= endGamePieceCount);

    int value = baseValue;
    int sqIndex = pieces.lsb();

    if (color == chess::Color::WHITE) {
        if (isEndGame) {
            value += KING_PENALTY_TABLE_WHITE_END[sqIndex];
        } else {
            value += KING_PENALTY_TABLE_WHITE_MID[sqIndex];
        }
    } else {
        if (isEndGame) {
            value += KING_PENALTY_TABLE_BLACK_END[sqIndex];
        } else {
            value += KING_PENALTY_TABLE_BLACK_MID[sqIndex];
        }
    } 
    return value;
}


// Function to count the total number of pieces on the board
int countPieces(const chess::Board& board) {

    int pieceCount = 0;

    // Traverse all piece types and colors
    constexpr PieceType allPieceTypes[] = {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, 
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

// Constants for the evaluation function
constexpr int PAWN_VALUE = 100;
constexpr int KNIGHT_VALUE = 320;
constexpr int BISHOP_VALUE = 330;
constexpr int ROOK_VALUE = 500;
constexpr int QUEEN_VALUE = 900;
constexpr int KING_VALUE = 100;

// Function to evaluate the board position
int evaluate(const chess::Board& board) {
    // Initialize totals
    int whiteScore = 0;
    int blackScore = 0;

    // Traverse each piece type
    constexpr PieceType allPieceTypes[] = {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, 
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

    if (hasLostCastlingRights(board, Color::WHITE, Board::CastlingRights::Side::KING_SIDE) &&
            hasLostCastlingRights(board, Color::WHITE, Board::CastlingRights::Side::QUEEN_SIDE)) {
        whiteScore -= 100;
    } 

    if (hasLostCastlingRights(board, Color::BLACK, Board::CastlingRights::Side::KING_SIDE) &&
            hasLostCastlingRights(board, Color::BLACK, Board::CastlingRights::Side::QUEEN_SIDE)) {
        blackScore -= 100;
    }

    // int numWhiteLegalMoves, numBlackLegalMoves;
    // // check which turn it is
    // if (board.sideToMove() == Color::WHITE) {
    //     numWhiteLegalMoves = countLegalMoves(board);
    //     // make a null move to get the number of legal moves for the opponent
    //     chess::Board tempBoard = board;
    //     tempBoard.makeNullMove();
    //     numBlackLegalMoves = countLegalMoves(tempBoard);
    // } else {
    //     numBlackLegalMoves = countLegalMoves(board);
    //     // make a null move to get the number of legal moves for the opponent
    //     chess::Board tempBoard = board;
    //     tempBoard.makeNullMove();
    //     numWhiteLegalMoves = countLegalMoves(tempBoard);
    // }

    // whiteScore += numWhiteLegalMoves / 20;
    // blackScore += numBlackLegalMoves / 20;

    return whiteScore - blackScore;
}

