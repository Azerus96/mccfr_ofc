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

    class MCCFRSolver {
    public:
        MCCFRSolver() {}

        // =================== ФИНАЛЬНАЯ ОТЛАДОЧНАЯ ВЕРСИЯ TRAIN ===================
        inline void train(int iterations) {
            std::cout << "C++: train() FINAL DEBUG version started." << std::flush;

            #pragma omp parallel for
            for (int i = 0; i < iterations; ++i) {
                // ШАГ 1: Создаем состояние игры.
                GameState initial_state;
                
                // ШАГ 2: ВРЕМЕННО ОТКЛЮЧАЕМ ГЕНЕРАЦИЮ ДЕЙСТВИЙ
                // auto actions = initial_state.get_legal_actions();

                // Просто выводим сообщение, чтобы показать, что итерация создания GameState завершилась
                if (i % 1000 == 0) {
                    #pragma omp critical
                    {
                        // Мы не можем вывести actions.size(), так как actions закомментирован
                        std::cout << "C++: Iteration " << i << " on thread " << omp_get_thread_num() << " completed." << std::endl << std::flush;
                    }
                }
            }
            std::cout << "C++: train() FINAL DEBUG version finished." << std::endl << std::flush;
        }
        // ======================================================================

        inline void save_strategy(const std::string& path) const {
            std::cout << "C++: save_strategy() called (NO-OP for debug)." << std::endl << std::flush;
        }

        inline void load_strategy(const std::string& path) {
            std::cout << "C++: load_strategy() called (NO-OP for debug)." << std::endl << std::flush;
        }

    private:
        std::unordered_map<std::string, Node> nodes_;
        mutable std::mutex map_mutex_;
        HandEvaluator evaluator_;
    };
}
