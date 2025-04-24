#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <array>
#include <fstream>
#include <cstdint>
#include <string>
#include <iostream>
#include "chess.hpp"

using namespace chess;

constexpr int INPUT_SIZE = 768;
constexpr int HIDDEN_SIZE = 64;
constexpr int SCALE = 400;
constexpr int QA = 255;
constexpr int QB = 64;

/*--------------------------------------------------------------------------------------------
    Calculate index of a piece, square, and side to move
    Square: 0 - 63
    PieceType: Pawn = 0, Knight = 1, Bishop = 2, Rook = 3, Queen = 4, King = 5
    Side: us = 0, them = 1
--------------------------------------------------------------------------------------------*/
inline int calculateIndex(int side, int pieceType, int square) {
    return side * 64 * 6 + pieceType * 64 + square;
}

/*--------------------------------------------------------------------------------------------
    PieceType to piece index
--------------------------------------------------------------------------------------------*/
inline int pieceTypeToIndex(PieceType type) {
    if (type == PieceType::PAWN) return 0;
    if (type == PieceType::KNIGHT) return 1;
    if (type == PieceType::BISHOP) return 2;
    if (type == PieceType::ROOK) return 3;
    if (type == PieceType::QUEEN) return 4;
    if (type == PieceType::KING) return 5;
    return -1; // Invalid piece type
}

/*--------------------------------------------------------------------------------------------
    Clip ReLU
--------------------------------------------------------------------------------------------*/
inline int crelu(int16_t x) {
    int val = static_cast<int>(x);
    return std::clamp(val, 0, static_cast<int>(QA));
}

/*--------------------------------------------------------------------------------------------
    Square Clip ReLU (SCReLU)
--------------------------------------------------------------------------------------------*/
inline int screlu(int16_t x) {
    int val = std::clamp(static_cast<int>(x), 0, static_cast<int>(QA));
    return val * val;
}

/*--------------------------------------------------------------------------------------------
    Forward declaration
--------------------------------------------------------------------------------------------*/
struct Network;

/*--------------------------------------------------------------------------------------------
    Accumulator
--------------------------------------------------------------------------------------------*/
struct Accumulator {
    std::array<int16_t, HIDDEN_SIZE> vals;

    static Accumulator fromBias(const Network& net);
    void addFeature(size_t feature_idx, const Network& net);
    void removeFeature(size_t feature_idx, const Network& net);
};


/*--------------------------------------------------------------------------------------------
    768 -> HIDDEN_SIZE x 2 -> 1
    Network architecture:
    x1 : 768 for side-to-move
    x2 : 768 for not-side-to-move

    h1 = Wx1 + b  
    h2 = Wx2 + b

    o = O1 * relu(h1) + O2 * relu(h2) + c
--------------------------------------------------------------------------------------------*/
struct Network {
    std::array<Accumulator, 768> featureWeights;
    Accumulator feature_bias;
    std::array<int16_t, 2 * HIDDEN_SIZE> outputWeights;
    int16_t output_bias;

    int evaluate(const Accumulator& us, const Accumulator& them) const {
        int output = 0;
    
        #pragma omp simd reduction(+:output)
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            output += screlu(us.vals[i]) * static_cast<int>(outputWeights[i]) +
                      screlu(them.vals[i]) * static_cast<int>(outputWeights[HIDDEN_SIZE + i]);
        }
    
        output /= QA;
        output += static_cast<int>(output_bias);

        output *= SCALE;
        output /= QA * QB;
    
        return output;
    }
    
};

/*--------------------------------------------------------------------------------------------
    Accumulator functions.
--------------------------------------------------------------------------------------------*/
inline Accumulator Accumulator::fromBias(const Network& net) {
    return net.feature_bias;
}

inline void Accumulator::addFeature(size_t feature_idx, const Network& net) {
    #pragma omp simd
    for (size_t i = 0; i < HIDDEN_SIZE; ++i) {
        vals[i] += net.featureWeights[feature_idx].vals[i];
    }
}

inline void Accumulator::removeFeature(size_t feature_idx, const Network& net) {
    #pragma omp simd
    for (size_t i = 0; i < HIDDEN_SIZE; ++i) {
        vals[i] -= net.featureWeights[feature_idx].vals[i];
    }
}


/*--------------------------------------------------------------------------------------------
    Load network from file.
--------------------------------------------------------------------------------------------*/
bool loadNetwork(const std::string& filepath, Network& net) {
    std::ifstream stream(filepath, std::ios::binary);
    if (!stream.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return false;
    }

    // Load feature weights: 768 x 64 = 393216 int16_t
    for (int i = 0; i < INPUT_SIZE; ++i) {
        stream.read(reinterpret_cast<char*>(net.featureWeights[i].vals.data()),
                    HIDDEN_SIZE * sizeof(int16_t));
    }

    // Load hidden layer bias: 512 int16_t
    stream.read(reinterpret_cast<char*>(net.feature_bias.vals.data()),
                HIDDEN_SIZE * sizeof(int16_t));

    // Load output weights: 2 x 512 int16_t
    stream.read(reinterpret_cast<char*>(net.outputWeights.data()),
                2 * HIDDEN_SIZE * sizeof(int16_t));

    // Load output bias: 1 int16_t
    stream.read(reinterpret_cast<char*>(&net.output_bias), sizeof(int16_t));

    if (!stream) {
        std::cerr << "Failed to read full network from file.\n";
        return false;
    }

    return true;
}

/*--------------------------------------------------------------------------------------------
    Create accumulators for white and black pieces.
--------------------------------------------------------------------------------------------*/
inline int mirrorSquare(int sq) {
    return (sq ^ 56); // flips rank (A1 to A8, H2 to H7, etc.)
}

void makeAccumulators(Board& board, Accumulator& whiteAccumulator, Accumulator& blackAccumulator, Network& evalNetwork) {
    // Initialize the accumulators
    whiteAccumulator = Accumulator::fromBias(evalNetwork);
    blackAccumulator = Accumulator::fromBias(evalNetwork);

    Bitboard whitePawns = board.pieces(PieceType::PAWN, Color::WHITE);
    Bitboard whiteKnights = board.pieces(PieceType::KNIGHT, Color::WHITE);
    Bitboard whiteBishops = board.pieces(PieceType::BISHOP, Color::WHITE);
    Bitboard whiteRooks = board.pieces(PieceType::ROOK, Color::WHITE);
    Bitboard whiteQueens = board.pieces(PieceType::QUEEN, Color::WHITE);
    Bitboard whiteKings = board.pieces(PieceType::KING, Color::WHITE);

    Bitboard blackPawns = board.pieces(PieceType::PAWN, Color::BLACK);
    Bitboard blackKnights = board.pieces(PieceType::KNIGHT, Color::BLACK);
    Bitboard blackBishops = board.pieces(PieceType::BISHOP, Color::BLACK);
    Bitboard blackRooks = board.pieces(PieceType::ROOK, Color::BLACK);
    Bitboard blackQueens = board.pieces(PieceType::QUEEN, Color::BLACK);
    Bitboard blackKings = board.pieces(PieceType::KING, Color::BLACK);

    for (int i = 0; i < 12; i++) {
        Bitboard bb;
        if (i == 0) bb = whitePawns;
        else if (i == 1) bb = whiteKnights;
        else if (i == 2) bb = whiteBishops;
        else if (i == 3) bb = whiteRooks;
        else if (i == 4) bb = whiteQueens;
        else if (i == 5) bb = whiteKings;
        else if (i == 6) bb = blackPawns;
        else if (i == 7) bb = blackKnights;
        else if (i == 8) bb = blackBishops;
        else if (i == 9) bb = blackRooks;
        else if (i == 10) bb = blackQueens;
        else if (i == 11) bb = blackKings;

        while (bb) {
            int sq = bb.lsb();
            int msq = mirrorSquare(sq);
            bb.clear(sq);
        
            int type = i % 6;
            bool white = (i < 6);
        
            if (white) {
                // from White’s view
                whiteAccumulator.addFeature(calculateIndex(0, type, sq),  evalNetwork); // us
                blackAccumulator.addFeature(calculateIndex(1, type, msq), evalNetwork); // them
            } else {
                // from Black’s view
                blackAccumulator.addFeature(calculateIndex(0, type, msq),  evalNetwork); // us
                whiteAccumulator.addFeature(calculateIndex(1, type, sq), evalNetwork); // them
            }
        }
        
    }
}

/*--------------------------------------------------------------------------------------------
    Update accumulator given a move.
    Work in progress: need to consider castling &  enpassant.
    To be called before board.makeMove(move).
--------------------------------------------------------------------------------------------*/
void addAccumulators(Board& board, 
                        Move& move, 
                        Accumulator& whiteAccumulator, 
                        Accumulator& blackAccumulator,
                        Network& evalNetwork) {

    Color color = board.sideToMove();
    PieceType pieceType = board.at<Piece>(move.from()).type();

    int pieceIdx = pieceTypeToIndex(pieceType);
    bool isPromotion = move.typeOf() & Move::PROMOTION;
    bool isEnpassant = move.typeOf() & Move::ENPASSANT;
    bool isCastle = move.typeOf() & Move::CASTLING; 
    bool isNullMove = move.typeOf() & Move::NULL_MOVE;
    bool isCapture = board.isCapture(move);

    if (isPromotion || isEnpassant || isCastle || isNullMove) {
        // For now calculate from scratch for promotion and enpassant
        board.makeMove(move);
        makeAccumulators(board, whiteAccumulator, blackAccumulator, evalNetwork);
        board.unmakeMove(move);
        return;
    }


    if (color == Color::WHITE) {
        int fromIdxUs = calculateIndex(0, pieceIdx, move.from().index());
        int toIdxUs = calculateIndex(0, pieceIdx, move.to().index());
        int fromIdxThem = calculateIndex(1, pieceIdx, mirrorSquare(move.from().index()));
        int toIdxThem = calculateIndex(1, pieceIdx, mirrorSquare(move.to().index()));

        // Calculate index of from and to from "us" perspective
        whiteAccumulator.removeFeature(fromIdxUs, evalNetwork);
        whiteAccumulator.addFeature(toIdxUs, evalNetwork);

        blackAccumulator.removeFeature(fromIdxThem, evalNetwork);
        blackAccumulator.addFeature(toIdxThem, evalNetwork);


        //position fen r2q1r1k/1b3p2/p2Ppn2/1p4Q1/8/3B4/PPP2PPP/R4RK1 w - - 1 22
        if (board.isCapture(move)) {
            // Remove the captured piece
            PieceType captured = board.at<Piece>(move.to()).type();
            int capturedIdx = pieceTypeToIndex(captured);

            // remove the black piece from white's perspective
            int captureIndexUs = calculateIndex(1, capturedIdx, move.to().index());
            whiteAccumulator.removeFeature(captureIndexUs, evalNetwork);

            // remove the black piece from black's perspective
            int captureIndexThem = calculateIndex(0, capturedIdx, mirrorSquare(move.to().index()));
            blackAccumulator.removeFeature(captureIndexThem, evalNetwork);
        }

    } else {


        int fromIdxUs = calculateIndex(0, pieceIdx, mirrorSquare(move.from().index()));
        int toIdxUs = calculateIndex(0, pieceIdx, mirrorSquare(move.to().index()));

        int fromIdxThem = calculateIndex(1, pieceIdx, move.from().index());
        int toIdxThem = calculateIndex(1, pieceIdx, move.to().index());

        // mirror the square because we are in black's perspective
        blackAccumulator.removeFeature(fromIdxUs, evalNetwork);
        blackAccumulator.addFeature(toIdxUs, evalNetwork);

        whiteAccumulator.removeFeature(fromIdxThem, evalNetwork);
        whiteAccumulator.addFeature(toIdxThem, evalNetwork);




        if (board.isCapture(move)) {

            // board.makeMove(move);
            // makeAccumulators(board, whiteAccumulator, blackAccumulator, evalNetwork);
            // board.unmakeMove(move);
            // return;
            PieceType captured = board.at<Piece>(move.to()).type();
            int capturedIdx = pieceTypeToIndex(captured);

            // remove the white piece from black's perspective
            int captureIndexUs = calculateIndex(1, capturedIdx, mirrorSquare(move.to().index()));
            blackAccumulator.removeFeature(captureIndexUs, evalNetwork);

            // remove the white piece from white's perspective
            int captureIndexThem = calculateIndex(0, capturedIdx, move.to().index());
            whiteAccumulator.removeFeature(captureIndexThem, evalNetwork);
        }
    }
}

/*--------------------------------------------------------------------------------------------
    Update accumulator given an unmakeMove.
    To be called before board.unmakeMove(move).
--------------------------------------------------------------------------------------------*/
void subtractAccumulators(Board& board, 
                                Move& move, 
                                Accumulator& whiteAccumulator, 
                                Accumulator& blackAccumulator,
                                Network& evalNetwork) {

    bool isPromotion = move.typeOf() & Move::PROMOTION;
    bool isEnpassant = move.typeOf() & Move::ENPASSANT;
    bool isCastle = move.typeOf() & Move::CASTLING;      
    bool isNullMove = move.typeOf() & Move::NULL_MOVE;

    board.unmakeMove(move);
    makeAccumulators(board, whiteAccumulator, blackAccumulator, evalNetwork);
    board.makeMove(move);
    return;

    // if (isPromotion || isEnpassant || isCastle || isNullMove) {
    //     // For now calculate from scratch for promotion and enpassant
    //     board.unmakeMove(move);
    //     makeAccumulators(board, whiteAccumulator, blackAccumulator, evalNetwork);
    //     board.makeMove(move);
    //     return;
    // }

    // board.unmakeMove(move); 

    // Color color = board.sideToMove(); 
    // PieceType pieceType = board.at<Piece>(move.from()).type();
    // int pieceIdx = pieceTypeToIndex(pieceType);

    // if (color == Color::WHITE) {

    //     int fromIdx = calculateIndex(0, pieceIdx, move.from().index());
    //     int toIdx = calculateIndex(0, pieceIdx, move.to().index());

    //     // Remove the piece from the destination square & put the piece back in its original position
    //     whiteAccumulator.removeFeature(toIdx, evalNetwork);
    //     whiteAccumulator.addFeature(fromIdx, evalNetwork);


    //     if (board.isCapture(move)) {
    //         makeAccumulators(board, whiteAccumulator, blackAccumulator, evalNetwork);

    //         // PieceType captured = board.at<Piece>(move.to()).type();
    //         // int capturedIdx = pieceTypeToIndex(captured);

    //         // // Put the black piece back in its original position from white's perspective
    //         // int captureIndexUs = calculateIndex(1, capturedIdx, move.to().index());
    //         // whiteAccumulator.addFeature(captureIndexUs, evalNetwork);

    //         // // Put the black piece back in its original position from black's perspective
    //         // int captureIndexThem = calculateIndex(0, capturedIdx, mirrorSquare(move.to().index()));
    //         // blackAccumulator.addFeature(captureIndexThem, evalNetwork); 
    //     }


    // } else {
    //     int fromIdx = calculateIndex(0, pieceIdx, mirrorSquare(move.from().index()));
    //     int toIdx = calculateIndex(0, pieceIdx, mirrorSquare(move.to().index()));

    //     // Remove the piece from the destination square & put the piece back in its original position
    //     blackAccumulator.removeFeature(toIdx, evalNetwork);
    //     blackAccumulator.addFeature(fromIdx, evalNetwork);

    //     if (board.isCapture(move)) {
    //         makeAccumulators(board, whiteAccumulator, blackAccumulator, evalNetwork);
            
    //         // PieceType captured = board.at<Piece>(move.to()).type();
    //         // int capturedIdx = pieceTypeToIndex(captured);

    //         // // Put the white piece back from black's perspective
    //         // int captureIndexUs = calculateIndex(1, capturedIdx, mirrorSquare(move.to().index()));
    //         // blackAccumulator.addFeature(captureIndexUs, evalNetwork);

    //         // // Put the white piece from white's perspective
    //         // int captureIndexThem = calculateIndex(0, capturedIdx, move.to().index());
    //         // whiteAccumulator.addFeature(captureIndexThem, evalNetwork);
    //     }

    // }

    // board.makeMove(move);
}

