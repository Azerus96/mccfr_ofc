#include "game_state.hpp"

namespace ofc {
    // === ИЗМЕНЕНИЕ ЗДЕСЬ ===
    // Определяем thread_local статическую переменную.
    // Она будет инициализирована один раз для каждого потока.
    thread_local std::mt19937 GameState::rng_(std::random_device{}());
}
