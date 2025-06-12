            int two_ply_bonus = ply >= 2 ? mg_2ply[thread_id].get_count({move_index(move_stack[thread_id][ply - 2]), move_idx}) : 0;
            int one_ply_bonus = ply >= 1 ? mg_2ply[thread_id].get_count({move_index(move_stack[thread_id][ply - 1]), move_idx}) : 0;
            // two_ply_bonus = std::clamp(two_ply_bonus, 0, 50);
            // one_ply_bonus = std::clamp(one_ply_bonus, 0, 50);