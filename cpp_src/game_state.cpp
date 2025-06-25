#include "game_state.hpp"

namespace ofc {
    // Это определение выделяет память для статической переменной rng_
    // и инициализирует ее.
    std::mt19937 GameState::rng_(std::random_device{}());
}
