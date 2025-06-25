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

        // =================== НОВАЯ ОТЛАДОЧНАЯ ВЕРСИЯ TRAIN ===================
        inline void train(int iterations) {
            std::cout << "C++: train() DEBUG version started." << std::flush;

            #pragma omp parallel for
            for (int i = 0; i < iterations; ++i) {
                // Создаем состояние игры. Это проверит RNG.
                GameState initial_state;
                
                // Вызываем генерацию действий. Это проверит комбинаторику.
                auto actions = initial_state.get_legal_actions();

                // Просто выводим сообщение, чтобы показать, что итерация завершилась
                if (i % 1000 == 0) {
                    #pragma omp critical
                    {
                        std::cout << "C++: Iteration " << i << " on thread " << omp_get_thread_num() << " completed with " << actions.size() << " actions." << std::endl << std::flush;
                    }
                }
            }
            std::cout << "C++: train() DEBUG version finished." << std::endl << std::flush;
        }
        // ======================================================================

        inline void save_strategy(const std::string& path) const {
            // Пока ничего не делаем
            std::cout << "C++: save_strategy() called (NO-OP for debug)." << std::endl << std::flush;
        }

        inline void load_strategy(const std::string& path) {
            // Пока ничего не делаем
            std::cout << "C++: load_strategy() called (NO-OP for debug)." << std::endl << std::flush;
        }

    private:
        // Вся логика CFR временно отключена
        std::unordered_map<std::string, Node> nodes_;
        mutable std::mutex map_mutex_;
        HandEvaluator evaluator_;
    };
}
