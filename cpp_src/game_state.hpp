#pragma once
#include "board.hpp"
#include <vector>
#include <random>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <functional>
#include <map>

namespace ofc {

    class GameState {
    public:
        GameState(int num_players = 2, int dealer_pos = -1)
            : num_players_(num_players), street_(1), boards_(num_players), discards_(num_players) {
            
            deck_.resize(52);
            std::iota(deck_.begin(), deck_.end(), 0);
            std::shuffle(deck_.begin(), deck_.end(), rng_); // Используем thread_local rng_

            if (dealer_pos == -1) {
                std::uniform_int_distribution<int> dist(0, num_players - 1);
                dealer_pos_ = dist(rng_);
            } else {
                dealer_pos_ = dealer_pos;
            }
            current_player_ = (dealer_pos_ + 1) % num_players_;
            deal_cards();
        }

        // ... (остальная часть класса остается без изменений) ...
        // ... (get_payoffs, get_legal_actions, apply_action, etc.) ...
        GameState(const GameState& other) = default;

        inline bool is_terminal() const {
            return street_ > 5 || boards_[0].get_card_count() == 13;
        }

        inline std::pair<float, float> get_payoffs(const HandEvaluator& evaluator) const {
            const int FANTASY_BONUS_QQ = 15, FANTASY_BONUS_KK = 20, FANTASY_BONUS_AA = 25, FANTASY_BONUS_TRIPS = 30;
            const int SCOOP_BONUS = 3;

            const Board& p1_board = boards_[0];
            const Board& p2_board = boards_[1];

            bool p1_foul = p1_board.is_foul(evaluator);
            bool p2_foul = p2_board.is_foul(evaluator);

            int p1_royalty = p1_foul ? 0 : p1_board.get_total_royalty(evaluator);
            int p2_royalty = p2_foul ? 0 : p2_board.get_total_royalty(evaluator);

            if (p1_foul && p2_foul) return {0.0f, 0.0f};
            if (p1_foul) return {-(float)(SCOOP_BONUS + p2_royalty), (float)(SCOOP_BONUS + p2_royalty)};
            if (p2_foul) return {(float)(SCOOP_BONUS + p1_royalty), -(float)(SCOOP_BONUS + p1_royalty)};

            int line_score = 0;
            if (evaluator.evaluate(p1_board.get_row_cards("top")) < evaluator.evaluate(p2_board.get_row_cards("top"))) line_score++; else line_score--;
            if (evaluator.evaluate(p1_board.get_row_cards("middle")) < evaluator.evaluate(p2_board.get_row_cards("middle"))) line_score++; else line_score--;
            if (evaluator.evaluate(p1_board.get_row_cards("bottom")) < evaluator.evaluate(p2_board.get_row_cards("bottom"))) line_score++; else line_score--;

            if (abs(line_score) == 3) line_score = (line_score > 0) ? SCOOP_BONUS : -SCOOP_BONUS;
            
            float p1_total = (float)(line_score + p1_royalty - p2_royalty);

            if (p1_board.qualifies_for_fantasyland(evaluator)) {
                int fc = p1_board.get_fantasyland_card_count(evaluator);
                if (fc == 14) p1_total += FANTASY_BONUS_QQ; else if (fc == 15) p1_total += FANTASY_BONUS_KK;
                else if (fc == 16) p1_total += FANTASY_BONUS_AA; else if (fc == 17) p1_total += FANTASY_BONUS_TRIPS;
            }
            if (p2_board.qualifies_for_fantasyland(evaluator)) {
                int fc = p2_board.get_fantasyland_card_count(evaluator);
                if (fc == 14) p1_total -= FANTASY_BONUS_QQ; else if (fc == 15) p1_total -= FANTASY_BONUS_KK;
                else if (fc == 16) p1_total -= FANTASY_BONUS_AA; else if (fc == 17) p1_total -= FANTASY_BONUS_TRIPS;
            }
            return {p1_total, -p1_total};
        }

        inline std::vector<Action> get_legal_actions() const {
            std::vector<Action> actions;
            if (is_terminal()) return actions;

            if (street_ == 1) {
                generate_abstract_actions_5_cards(dealt_cards_, actions);
            } else {
                for (int i = 0; i < 3; ++i) {
                    CardSet to_place;
                    Card discarded = dealt_cards_[i];
                    for (int j = 0; j < 3; ++j) if (i != j) to_place.push_back(dealt_cards_[j]);
                    generate_abstract_actions_2_cards(to_place, discarded, actions);
                }
            }
            std::sort(actions.begin(), actions.end());
            actions.erase(std::unique(actions.begin(), actions.end()), actions.end());
            
            if (actions.empty()) {
                add_fallback_action(actions);
            }

            return actions;
        }

        inline GameState apply_action(const Action& action) const {
            GameState next_state(*this);
            const auto& placements = action.first;
            const Card& discarded_card = action.second;

            for (const auto& p : placements) {
                const Card& card = p.first;
                const std::string& row = p.second.first;
                int idx = p.second.second;
                if (row == "top") next_state.boards_[current_player_].top[idx] = card;
                else if (row == "middle") next_state.boards_[current_player_].middle[idx] = card;
                else if (row == "bottom") next_state.boards_[current_player_].bottom[idx] = card;
            }
            if (discarded_card != INVALID_CARD) {
                next_state.discards_[current_player_].push_back(discarded_card);
            }

            if (next_state.current_player_ == next_state.dealer_pos_) next_state.street_++;
            next_state.current_player_ = (next_state.current_player_ + 1) % num_players_;
            
            if (!next_state.is_terminal()) next_state.deal_cards();
            return next_state;
        }
        
        int get_street() const { return street_; }
        int get_current_player() const { return current_player_; }
        const CardSet& get_dealt_cards() const { return dealt_cards_; }
        const Board& get_player_board(int player_idx) const { return boards_[player_idx]; }
        const Board& get_opponent_board(int player_idx) const { return boards_[(player_idx + 1) % num_players_]; }

    private:
        inline void deal_cards() {
            int num_to_deal = (street_ == 1) ? 5 : 3;
            if (deck_.size() < num_to_deal) {
                street_ = 6; return;
            }
            dealt_cards_.assign(deck_.end() - num_to_deal, deck_.end());
            deck_.resize(deck_.size() - num_to_deal);
        }

        inline void generate_abstract_actions_5_cards(const CardSet& hand, std::vector<Action>& actions) const {
            std::vector<int> p(5);
            std::iota(p.begin(), p.end(), 0);
            std::vector<bool> v(3);
            std::fill(v.begin(), v.end(), true);
            v.resize(5, false);

            do {
                CardSet back_cards, mid_cards;
                for (int i = 0; i < 5; ++i) {
                    if (v[i]) back_cards.push_back(hand[i]);
                    else mid_cards.push_back(hand[i]);
                }
                
                std::vector<Placement> placement;
                for(size_t i = 0; i < back_cards.size(); ++i) placement.push_back({back_cards[i], {"bottom", (int)i}});
                for(size_t i = 0; i < mid_cards.size(); ++i) placement.push_back({mid_cards[i], {"middle", (int)i}});
                actions.push_back({placement, INVALID_CARD});

            } while (std::prev_permutation(v.begin(), v.end()));
        }

        inline void generate_abstract_actions_2_cards(const CardSet& cards, Card discarded, std::vector<Action>& actions) const {
            const Board& board = boards_[current_player_];
            std::vector<std::pair<std::string, int>> available_slots;
            for(int i=0; i<3; ++i) if(board.top[i] == INVALID_CARD) available_slots.push_back({"top", i});
            for(int i=0; i<5; ++i) if(board.middle[i] == INVALID_CARD) available_slots.push_back({"middle", i});
            for(int i=0; i<5; ++i) if(board.bottom[i] == INVALID_CARD) available_slots.push_back({"bottom", i});

            if (available_slots.size() < 2) return;

            std::vector<int> p(available_slots.size());
            std::iota(p.begin(), p.end(), 0);
            std::vector<bool> v(2);
            std::fill(v.begin(), v.end(), true);
            v.resize(p.size(), false);

            do {
                std::vector<std::pair<std::string, int>> chosen_slots;
                for(size_t i = 0; i < p.size(); ++i) {
                    if (v[i]) chosen_slots.push_back(available_slots[i]);
                }
                actions.push_back({{{cards[0], chosen_slots[0]}, {cards[1], chosen_slots[1]}}, discarded});
                actions.push_back({{{cards[1], chosen_slots[0]}, {cards[0], chosen_slots[1]}}, discarded});
            } while (std::prev_permutation(v.begin(), v.end()));
        }
        
        inline void add_fallback_action(std::vector<Action>& actions) const {
            const Board& board = boards_[current_player_];
            std::vector<std::pair<std::string, int>> available_slots;
            for(int i=0; i<3; ++i) if(board.top[i] == INVALID_CARD) available_slots.push_back({"top", i});
            for(int i=0; i<5; ++i) if(board.middle[i] == INVALID_CARD) available_slots.push_back({"middle", i});
            for(int i=0; i<5; ++i) if(board.bottom[i] == INVALID_CARD) available_slots.push_back({"bottom", i});

            CardSet cards_to_place;
            Card discarded = INVALID_CARD;
            if (street_ == 1) {
                cards_to_place = dealt_cards_;
            } else {
                cards_to_place = {dealt_cards_[0], dealt_cards_[1]};
                discarded = dealt_cards_[2];
            }

            if (available_slots.size() >= cards_to_place.size()) {
                std::vector<Placement> placement;
                for(size_t i = 0; i < cards_to_place.size(); ++i) {
                    placement.push_back({cards_to_place[i], available_slots[i]});
                }
                actions.push_back({placement, discarded});
            }
        }

        int num_players_;
        int street_;
        int dealer_pos_;
        int current_player_;
        std::vector<Board> boards_;
        std::vector<CardSet> discards_;
        CardSet deck_;
        CardSet dealt_cards_;
        
        // === ИЗМЕНЕНИЕ ЗДЕСЬ ===
        // Делаем генератор thread_local, чтобы у каждого потока был свой.
        static thread_local std::mt19937 rng_;
    };
}
