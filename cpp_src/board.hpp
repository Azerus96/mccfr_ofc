#pragma once
#include "card.hpp"
#include "hand_evaluator.hpp"
#include <array>
#include <string>
#include <vector>
#include <numeric>

namespace ofc {

    class Board {
    public:
        std::array<Card, 3> top;
        std::array<Card, 5> middle;
        std::array<Card, 5> bottom;

        Board() {
            top.fill(INVALID_CARD);
            middle.fill(INVALID_CARD);
            bottom.fill(INVALID_CARD);
        }

        inline CardSet get_row_cards(const std::string& row_name) const {
            CardSet cards;
            if (row_name == "top") for(Card c : top) if (c != INVALID_CARD) cards.push_back(c);
            else if (row_name == "middle") for(Card c : middle) if (c != INVALID_CARD) cards.push_back(c);
            else if (row_name == "bottom") for(Card c : bottom) if (c != INVALID_CARD) cards.push_back(c);
            return cards;
        }

        inline CardSet get_all_cards() const {
            CardSet all;
            for(Card c : top) if (c != INVALID_CARD) all.push_back(c);
            for(Card c : middle) if (c != INVALID_CARD) all.push_back(c);
            for(Card c : bottom) if (c != INVALID_CARD) all.push_back(c);
            return all;
        }

        inline int get_card_count() const { return get_all_cards().size(); }

        inline bool is_foul(const HandEvaluator& evaluator) const {
            if (get_card_count() != 13) return false;
            HandRank top_rank = evaluator.evaluate(get_row_cards("top"));
            HandRank mid_rank = evaluator.evaluate(get_row_cards("middle"));
            HandRank bot_rank = evaluator.evaluate(get_row_cards("bottom"));
            return (mid_rank < bot_rank) || (top_rank < mid_rank);
        }

        inline int get_total_royalty(const HandEvaluator& evaluator) const {
            if (is_foul(evaluator)) return 0;
            return evaluator.get_royalty(get_row_cards("top"), "top") +
                   evaluator.get_royalty(get_row_cards("middle"), "middle") +
                   evaluator.get_royalty(get_row_cards("bottom"), "bottom");
        }

        inline bool qualifies_for_fantasyland(const HandEvaluator& evaluator) const {
            if (is_foul(evaluator)) return false;
            CardSet top_cards = get_row_cards("top");
            if (top_cards.size() != 3) return false;
            HandRank hr = evaluator.evaluate(top_cards);
            if (hr.type_str == "Pair") {
                int r0 = get_rank(top_cards[0]), r1 = get_rank(top_cards[1]), r2 = get_rank(top_cards[2]);
                int pair_rank = (r0 == r1 || r0 == r2) ? r0 : r1;
                return pair_rank >= 10;
            }
            return hr.type_str == "Trips";
        }

        inline int get_fantasyland_card_count(const HandEvaluator& evaluator) const {
            if (!qualifies_for_fantasyland(evaluator)) return 0;
            CardSet top_cards = get_row_cards("top");
            HandRank hr = evaluator.evaluate(top_cards);
            if (hr.type_str == "Trips") return 17;
            if (hr.type_str == "Pair") {
                int r0 = get_rank(top_cards[0]), r1 = get_rank(top_cards[1]), r2 = get_rank(top_cards[2]);
                int pair_rank = (r0 == r1 || r0 == r2) ? r0 : r1;
                if (pair_rank == 10) return 14;
                if (pair_rank == 11) return 15;
                if (pair_rank == 12) return 16;
            }
            return 0;
        }
    };
}
