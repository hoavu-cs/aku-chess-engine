#pragma once

#include "chess.hpp"
#include "../lib/fathom/src/tbprobe.h"
#include <algorithm>
#include <iostream>
#include <string>

namespace syzygy {

    using namespace chess;
    using U64 = std::uint64_t; 

    inline void initialize_syzygy(std::string path) {
        std::cout << "Initializing endgame table at path: " << path << std::endl;
        if (!tb_init(path.c_str())) {
            std::cerr << "Failed to initialize endgame table." << std::endl;
        } else {
            std::cout << "Endgame table initialized successfully!" << std::endl;
        }
    }
    
    inline bool probe_syzygy(const Board& board, Move& suggestedMove, int& wdl) {
        // Convert the board to bitboard representation
        U64 white = board.us(Color::WHITE).getBits();
        U64 black = board.us(Color::BLACK).getBits();
        U64 kings = board.pieces(PieceType::KING).getBits();
        U64 queens = board.pieces(PieceType::QUEEN).getBits();
        U64 rooks = board.pieces(PieceType::ROOK).getBits();
        U64 bishops = board.pieces(PieceType::BISHOP).getBits();
        U64 knights = board.pieces(PieceType::KNIGHT).getBits();
        U64 pawns = board.pieces(PieceType::PAWN).getBits();
    
        unsigned rule50 = board.halfMoveClock() / 2;
        unsigned castling = board.castlingRights().hashIndex();
        unsigned ep = (board.enpassantSq() != Square::underlying::NO_SQ) ? board.enpassantSq().index() : 0;
        bool turn = (board.sideToMove() == Color::WHITE);
    
        // Create structure to store root move suggestions
        TbRootMoves results;
    
        int probe_success = tb_probe_root_dtz(
            white, black, kings, queens, rooks, bishops, knights, pawns,
            rule50, castling, ep, turn, 
            true, true, &results
        );
    
        // Handle probe failure
        if (!probe_success) {
            probe_success = tb_probe_root_wdl(
                white, black, kings, queens, rooks, bishops, knights, pawns,
                rule50, castling, ep, turn, true, &results
            );
    
            if (!probe_success) {
                return false;
            }
        }
    
        if (results.size > 0) {
            TbRootMove *best_move = std::max_element(results.moves, results.moves + results.size, 
                [](const TbRootMove &a, const TbRootMove &b) {
                    return a.tbRank < b.tbRank; // Higher rank is better
                });
    
            unsigned from = TB_MOVE_FROM(best_move->move);
            unsigned to = TB_MOVE_TO(best_move->move);
            unsigned promotes = TB_MOVE_PROMOTES(best_move->move);
    
            int from_index = from;
            int to_index = to;
    
            if (promotes) {
                switch (promotes) {
                    case TB_PROMOTES_QUEEN:
                        suggestedMove = Move::make<Move::PROMOTION>(Square(from_index), Square(to_index), PieceType::QUEEN);
                        break;
                    case TB_PROMOTES_ROOK:
                        suggestedMove = Move::make<Move::PROMOTION>(Square(from_index), Square(to_index), PieceType::ROOK);
                        break;
                    case TB_PROMOTES_BISHOP:
                        suggestedMove = Move::make<Move::PROMOTION>(Square(from_index), Square(to_index), PieceType::BISHOP);
                        break;
                    case TB_PROMOTES_KNIGHT:
                        suggestedMove = Move::make<Move::PROMOTION>(Square(from_index), Square(to_index), PieceType::KNIGHT);
                        break;
                }
                
            } else {
                suggestedMove = Move::make<Move::NORMAL>(Square(from_index), Square(to_index));
            }
    
            if (best_move->tbScore > 0) {
                wdl = 1;
            } else if (best_move->tbScore < 0) {
                wdl = -1;
            } else {
                wdl = 0;
            }
            return true;
        } else {
            return false;
        }
    }
}
