#include "../include/Scale.hpp"

namespace QuickMedia {
    static sf::Vector2f wrap_to_size_x(const sf::Vector2f &size, const sf::Vector2f &clamp_size) {
        sf::Vector2f new_size;
        auto size_ratio = size.y / size.x;
        new_size.x = clamp_size.x;
        new_size.y = new_size.x * size_ratio;
        return new_size;
    }

    static sf::Vector2f wrap_to_size_y(const sf::Vector2f &size, const sf::Vector2f &clamp_size) {
        sf::Vector2f new_size;
        auto size_ratio = size.x / size.y;
        new_size.y = clamp_size.y;
        new_size.x = new_size.y * size_ratio;
        return new_size;
    }

    sf::Vector2f wrap_to_size(const sf::Vector2f &size, const sf::Vector2f &clamp_size) {
        sf::Vector2f new_size;
        new_size = wrap_to_size_x(size, clamp_size);
        if(new_size.y > clamp_size.y)
            new_size = wrap_to_size_y(size, clamp_size);
        return new_size;
    }

    sf::Vector2f clamp_to_size(const sf::Vector2f &size, const sf::Vector2f &clamp_size) {
        sf::Vector2f new_size = size;
        if(size.x > clamp_size.x || size.y > clamp_size.y)
            new_size = wrap_to_size(new_size, clamp_size);
        return new_size;
    }

    sf::Vector2f get_ratio(const sf::Vector2f &original_size, const sf::Vector2f &new_size) {
        return sf::Vector2f(new_size.x / original_size.x, new_size.y / original_size.y);
    }
}