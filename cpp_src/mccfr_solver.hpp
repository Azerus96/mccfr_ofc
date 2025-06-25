#pragma once
#include "game_state.hpp"
#include "infoset.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <numeric>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <atomic> // Для счетчика

namespace ofc {

    struct Node {
        std::vector<double> regret_sum;
        std::vector<double> strategy_sum;
        int num_actions = 0;
    };

    class MCCFRSolver {
    public:
        MCCFRSolver() : traverse_counter(0) {}

        inline void train(int iterations) {
            std::cout << "C++: train() called with " << iterations << " iterations." << std::endl;
            for (int i = 0; i < iterations; ++i) {
                if (i % 1000 == 0) { // Печатаем прогресс каждые 1000 итераций
                    std::cout << "C++: train() progress: " << i << "/" << iterations << std::endl;
                }
                GameState initial_state;
                mccfr_traverse(initial_state, 1.0, 1.0);
            }
            std::cout << "C++: train() finished." << std::endl;
        }

        // ... (save_strategy и load_strategy остаются без изменений) ...
        inline void save_strategy(const std::string& path) const {
            std::ofstream out(path, std::ios::binary);
            if (!out) throw std::runtime_error("Cannot open file for writing: " + path);

            size_t map_size = nodes_.size();
            out.write(reinterpret_cast<const char*>(&map_size), sizeof(map_size));

            for (const auto& pair : nodes_) {
                size_t key_len = pair.first.length();
                out.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
                out.write(pair.first.c_str(), key_len);
                const Node& node = pair.second;
                out.write(reinterpret_cast<const char*>(&node.num_actions), sizeof(node.num_actions));
                out.write(reinterpret_cast<const char*>(node.regret_sum.data()), node.num_actions * sizeof(double));
                out.write(reinterpret_cast<const char*>(node.strategy_sum.data()), node.num_actions * sizeof(double));
            }
        }

        inline void load_strategy(const std::string& path) {
            std::ifstream in(path, std::ios::binary);
            if (!in) { std::cerr << "Strategy file not found, starting new." << std::endl; return; }

            nodes_.clear();
            size_t map_size;
            in.read(reinterpret_cast<char*>(&map_size), sizeof(map_size));
            if (in.fail()) return;

            for (size_t i = 0; i < map_size; ++i) {
                size_t key_len;
                in.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
                std::string key(key_len, ' ');
                in.read(&key[0], key_len);
                Node node;
                in.read(reinterpret_cast<char*>(&node.num_actions), sizeof(node.num_actions));
                node.regret_sum.resize(node.num_actions);
                node.strategy_sum.resize(node.num_actions);
                in.read(reinterpret_cast<char*>(node.regret_sum.data()), node.num_actions * sizeof(double));
                in.read(reinterpret_cast<char*>(node.strategy_sum.data()), node.num_actions * sizeof(double));
                nodes_[key] = node;
            }
            std::cout << "Loaded " << nodes_.size() << " infosets from strategy file." << std::endl;
        }


    private:
        // Отладочный счетчик
        std::atomic<long long> traverse_counter;

        inline Node* get_node(const std::string& infoset_key, int num_actions) {
            auto it = nodes_.find(infoset_key);
            if (it == nodes_.end() || it->second.num_actions != num_actions) {
                Node new_node;
                new_node.regret_sum.assign(num_actions, 0.0);
                new_node.strategy_sum.assign(num_actions, 0.0);
                new_node.num_actions = num_actions;
                nodes_[infoset_key] = new_node;
                return &nodes_[infoset_key];
            }
            return &it->second;
        }

        inline std::vector<double> mccfr_traverse(GameState state, double p1_reach, double p2_reach) {
            long long current_count = traverse_counter++;
            if (current_count > 500) { // Предохранитель от бесконечной рекурсии
                 throw std::runtime_error("C++: mccfr_traverse recursion limit exceeded!");
            }

            if (state.is_terminal()) {
                traverse_counter--;
                auto payoffs = state.get_payoffs(evaluator_);
                return {payoffs.first, payoffs.second};
            }

            int player = state.get_current_player();
            auto legal_actions = state.get_legal_actions();
            
            if (legal_actions.empty()) {
                traverse_counter--;
                return mccfr_traverse(state.apply_action({{}, INVALID_CARD}), p1_reach, p2_reach);
            }
            
            std::string infoset_key = get_infoset_key(state);
            int num_actions = legal_actions.size();
            Node* node = get_node(infoset_key, num_actions);

            std::vector<double> strategy(num_actions);
            double total_positive_regret = 0.0;
            for (int i = 0; i < num_actions; ++i) {
                strategy[i] = (node->regret_sum[i] > 0) ? node->regret_sum[i] : 0.0;
                total_positive_regret += strategy[i];
            }

            if (total_positive_regret > 0) {
                for (int i = 0; i < num_actions; ++i) strategy[i] /= total_positive_regret;
            } else {
                std::fill(strategy.begin(), strategy.end(), 1.0 / num_actions);
            }

            double reach_prob = (player == 0) ? p1_reach : p2_reach;
            for (int i = 0; i < num_actions; ++i) {
                node->strategy_sum[i] += reach_prob * strategy[i];
            }

            std::vector<std::vector<double>> action_utils(num_actions, std::vector<double>(2));
            std::vector<double> node_util(2, 0.0);

            for (int i = 0; i < num_actions; ++i) {
                GameState next_state = state.apply_action(legal_actions[i]);
                if (player == 0) action_utils[i] = mccfr_traverse(next_state, p1_reach * strategy[i], p2_reach);
                else action_utils[i] = mccfr_traverse(next_state, p1_reach, p2_reach * strategy[i]);
                for (int p = 0; p < 2; ++p) node_util[p] += strategy[i] * action_utils[i][p];
            }

            for (int i = 0; i < num_actions; ++i) {
                double regret = action_utils[i][player] - node_util[player];
                node->regret_sum[i] += ((player == 0) ? p2_reach : p1_reach) * regret;
            }
            
            traverse_counter--;
            return node_util;
        }

        std::unordered_map<std::string, Node> nodes_;
        HandEvaluator evaluator_;
    };
}
