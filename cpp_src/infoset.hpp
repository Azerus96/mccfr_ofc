// mccfr_ofc-main/cpp_src/infoset.hpp

#pragma once
#include "game_state.hpp"
#include <string>
#include <sstream>
#include <algorithm>

namespace ofc {

    inline std::string get_row_summary(const CardSet& cards) {
        if (cards.empty()) return "E";

        int flush_suit = -1;
        if (cards.size() > 1) {
            bool is_flush_draw = true;
            int first_suit = get_suit(cards[0]);
            for (size_t i = 1; i < cards.size(); ++i) {
                if (get_suit(cards[i]) != first_suit) {
                    is_flush_draw = false;
                    break;
                }
            }
            if (is_flush_draw) flush_suit = first_suit;
        }

        std::vector<int> ranks;
        for(Card c : cards) ranks.push_back(get_rank(c));
        std::sort(ranks.begin(), ranks.end());

        int pairs = 0, trips = 0;
        for (size_t i = 0; i < ranks.size(); ) {
            size_t j = i;
            while (j < ranks.size() && ranks[j] == ranks[i]) j++;
            if (j - i == 2) pairs++;
            if (j - i == 3) trips++;
            i = j;
        }

        std::stringstream ss;
        ss << "C" << cards.size();
        if (trips > 0) ss << "T" << trips;
        if (pairs > 0) ss << "P" << pairs;
        if (flush_suit != -1) ss << "F" << flush_suit;
        
        return ss.str();
    }

    inline std::string get_infoset_key(const GameState& state) {
        std::stringstream ss;
        int player = state.get_current_player();
        const Board& my_board = state.get_player_board(player);
        const Board& opp_board = state.get_opponent_board(player);

        // УЛУЧШЕНО: Используем новый API для get_row_cards, чтобы избежать лишних аллокаций
        CardSet my_bot_cards, my_mid_cards, my_top_cards;
        my_board.get_row_cards("bottom", my_bot_cards);
        my_board.get_row_cards("middle", my_mid_cards);
        my_board.get_row_cards("top", my_top_cards);

        CardSet opp_bot_cards, opp_mid_cards, opp_top_cards;
        opp_board.get_row_cards("bottom", opp_bot_cards);
        opp_board.get_row_cards("middle", opp_mid_cards);
        opp_board.get_row_cards("top", opp_top_cards);

        ss << "S" << state.get_street() << "|";
        ss << "B:" << get_row_summary(my_bot_cards) << ";";
        ss << "M:" << get_row_summary(my_mid_cards) << ";";
        ss << "T:" << get_row_summary(my_top_cards) << "|";
        ss << "OB:" << get_row_summary(opp_bot_cards) << ";";
        ss << "OM:" << get_row_summary(opp_mid_cards) << ";";
        ss << "OT:" << get_row_summary(opp_top_cards) << "|";

        CardSet hand = state.get_dealt_cards();
        std::sort(hand.begin(), hand.end());
        ss << "H:";
        for(Card c : hand) ss << card_to_string(c);
        
        return ss.str();
    }
}
