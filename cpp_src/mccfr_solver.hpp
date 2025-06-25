#pragma once
#include "game_state.hpp"
#include "infoset.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <numeric>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <omp.h>

namespace ofc {

    struct Node {
        std::vector<double> regret_sum;
        std::vector<double> strategy_sum;
        int num_actions = 0;
    };

    struct Update {
        std::string infoset_key;
        int num_actions;
        std::vector<double> regret_update;
        std::vector<double> strategy_update;
    };

    class MCCFRSolver {
    public:
        MCCFRSolver() {}

        inline void train(int iterations) {
            #pragma omp parallel for
            for (int i = 0; i < iterations; ++i) {
                thread_local std::vector<Update> local_updates;
                local_updates.clear();

                GameState initial_state;
                mccfr_traverse(initial_state, 1.0, 1.0, local_updates);

                apply_updates(local_updates);
            }
        }

        inline void save_strategy(const std::string& path) const {
            std::lock_guard<std::mutex> lock(map_mutex_);
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
            std::lock_guard<std::mutex> lock(map_mutex_);
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
        inline Node get_node_copy(const std::string& infoset_key, int num_actions) {
            std::lock_guard<std::mutex> lock(map_mutex_);
            auto it = nodes_.find(infoset_key);
            if (it == nodes_.end() || it->second.num_actions != num_actions) {
                Node new_node;
                new_node.regret_sum.assign(num_actions, 0.0);
                new_node.strategy_sum.assign(num_actions, 0.0);
                new_node.num_actions = num_actions;
                nodes_[infoset_key] = new_node;
                return new_node;
            }
            return it->second;
        }

        inline void apply_updates(const std::vector<Update>& updates) {
            std::lock_guard<std::mutex> lock(map_mutex_);
            for (const auto& update : updates) {
                Node& node = nodes_[update.infoset_key];
                if (node.num_actions != update.num_actions) {
                    node.regret_sum.assign(update.num_actions, 0.0);
                    node.strategy_sum.assign(update.num_actions, 0.0);
                    node.num_actions = update.num_actions;
                }
                for(int i=0; i<update.num_actions; ++i) {
                    node.regret_sum[i] += update.regret_update[i];
                    node.strategy_sum[i] += update.strategy_update[i];
                }
            }
        }

        inline std::vector<double> mccfr_traverse(GameState state, double p1_reach, double p2_reach, std::vector<Update>& local_updates) {
            if (state.is_terminal()) {
                auto payoffs = state.get_payoffs(evaluator_);
                return {payoffs.first, payoffs.second};
            }

            int player = state.get_current_player();
            auto legal_actions = state.get_legal_actions();
            if (legal_actions.empty()) {
                // Этого не должно происходить с новой логикой, но оставим как защиту
                return mccfr_traverse(state.apply_action({{}, INVALID_CARD}), p1_reach, p2_reach, local_updates);
            }
            
            std::string infoset_key = get_infoset_key(state);
            int num_actions = legal_actions.size();
            
            Node node_copy = get_node_copy(infoset_key, num_actions);

            std::vector<double> strategy(num_actions);
            double total_positive_regret = 0.0;
            for (int i = 0; i < num_actions; ++i) {
                strategy[i] = (node_copy.regret_sum[i] > 0) ? node_copy.regret_sum[i] : 0.0;
                total_positive_regret += strategy[i];
            }

            if (total_positive_regret > 0) {
                for (int i = 0; i < num_actions; ++i) strategy[i] /= total_positive_regret;
            } else {
                std::fill(strategy.begin(), strategy.end(), 1.0 / num_actions);
            }

            std::vector<std::vector<double>> action_utils(num_actions, std::vector<double>(2));
            std::vector<double> node_util(2, 0.0);

            for (int i = 0; i < num_actions; ++i) {
                GameState next_state = state.apply_action(legal_actions[i]);
                if (player == 0) action_utils[i] = mccfr_traverse(next_state, p1_reach * strategy[i], p2_reach, local_updates);
                else action_utils[i] = mccfr_traverse(next_state, p1_reach, p2_reach * strategy[i], local_updates);
                for (int p = 0; p < 2; ++p) node_util[p] += strategy[i] * action_utils[i][p];
            }

            Update update;
            update.infoset_key = infoset_key;
            update.num_actions = num_actions;
            update.regret_update.resize(num_actions);
            update.strategy_update.resize(num_actions);

            double reach_prob = (player == 0) ? p1_reach : p2_reach;
            for (int i = 0; i < num_actions; ++i) {
                double regret = action_utils[i][player] - node_util[player];
                update.regret_update[i] = ((player == 0) ? p2_reach : p1_reach) * regret;
                update.strategy_update[i] = reach_prob * strategy[i];
            }
            local_updates.push_back(update);

            return node_util;
        }

        std::unordered_map<std::string, Node> nodes_;
        mutable std::mutex map_mutex_;
        HandEvaluator evaluator_;
    };
}
