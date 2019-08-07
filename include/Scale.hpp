#pragma once

#include <SFML/System/Vector2.hpp>

namespace QuickMedia {
    sf::Vector2f wrap_to_size(const sf::Vector2f &size, const sf::Vector2f &clamp_size);
    sf::Vector2f clamp_to_size(const sf::Vector2f &size, const sf::Vector2f &clamp_size);
    sf::Vector2f get_ratio(const sf::Vector2f &original_size, const sf::Vector2f &new_size);
}