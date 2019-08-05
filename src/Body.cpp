#include "../include/Body.hpp"
#include "../plugins/Plugin.hpp"
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <assert.h>

const sf::Color front_color(43, 45, 47);
const sf::Color back_color(33, 35, 37);

namespace QuickMedia {
    Body::Body(sf::Font &font) : title_text("", font, 14), selected_item(0) {
        title_text.setFillColor(sf::Color::White);
    }

    void Body::select_previous_item() {
        if(items.empty())
            return;

        int num_items = (int)items.size();
        for(int i = 0; i < num_items; ++i) {
            --selected_item;
            if(selected_item < 0)
                selected_item = num_items - 1;
            if(items[selected_item]->visible)
                return;
        }
    }

    void Body::select_next_item() {
        if(items.empty())
            return;

        int num_items = (int)items.size();
        for(int i = 0; i < num_items; ++i) {
            ++selected_item;
            if(selected_item == num_items)
                selected_item = 0;
            if(items[selected_item]->visible)
                return;
        }
    }

    void Body::reset_selected() {
        selected_item = 0;
    }

    void Body::clear_items() {
        items.clear();
        item_thumbnail_textures.clear();
    }

    BodyItem* Body::get_selected() const {
        if(items.empty() || !items[selected_item]->visible)
            return nullptr;
        return items[selected_item].get();
    }

    void Body::clamp_selection() {
        int num_items = (int)items.size();
        if(items.empty())
            return;

        if(selected_item < 0)
            selected_item = 0;
        else if(selected_item >= num_items)
            selected_item = num_items - 1;

        for(int i = selected_item; i >= 0; --i) {
            if(items[i]->visible) {
                selected_item = i;
                return;
            }
        }

        for(int i = selected_item; i < num_items; ++i) {
            if(items[i]->visible) {
                selected_item = i;
                return;
            }
        }
    }

    static std::unique_ptr<sf::Texture> load_texture_from_url(const std::string &url) {
        auto result = std::make_unique<sf::Texture>();
        result->setSmooth(true);
        std::string texture_data;
        if(download_to_string(url, texture_data) == DownloadResult::OK)
            result->loadFromMemory(texture_data.data(), texture_data.size());
        return result;
    }

    // TODO: Skip drawing the rows that are outside the window.
    // TODO: Use a render target for the whole body so all images can be put into one.
    // TODO: Only load images once they are visible on the screen.
    // TODO: Load images asynchronously to prevent scroll lag and to improve load time of suggestions.
    void Body::draw(sf::RenderWindow &window, sf::Vector2f pos, sf::Vector2f size) {
        const float font_height = title_text.getCharacterSize() + 8.0f;
        const float image_height = 100.0f;

        sf::RectangleShape image_fallback(sf::Vector2f(50, image_height));
        image_fallback.setFillColor(sf::Color::White);

        sf::Sprite image;

        sf::RectangleShape item_background;
        item_background.setFillColor(front_color);
        item_background.setOutlineThickness(1.0f);
        item_background.setOutlineColor(sf::Color(63, 65, 67));

        sf::RectangleShape selected_border;
        selected_border.setFillColor(sf::Color::Red);
        const float selected_border_width = 5.0f;

        int num_items = items.size();
        if((int)item_thumbnail_textures.size() != num_items)
            item_thumbnail_textures.resize(num_items);

        for(int i = 0; i < num_items; ++i) {
            const auto &item = items[i];
            assert(items.size() == item_thumbnail_textures.size());
            auto &item_thumbnail = item_thumbnail_textures[i];
            if(!item->visible)
                continue;

            bool draw_thumbnail = !item->thumbnail_url.empty();
            float row_height = font_height;
            if(draw_thumbnail) {
                row_height = image_height;
                if(!item_thumbnail)
                    item_thumbnail = load_texture_from_url(item->thumbnail_url);
            }

            sf::Vector2f item_pos = pos;
            if(i == selected_item) {
                selected_border.setPosition(pos);
                selected_border.setSize(sf::Vector2f(selected_border_width, row_height));
                window.draw(selected_border);
                item_pos.x += selected_border_width;
                item_background.setFillColor(front_color);
            } else {
                item_background.setFillColor(sf::Color(38, 40, 42));
            }

            item_background.setPosition(item_pos);
            item_background.setSize(sf::Vector2f(size.x, row_height));
            window.draw(item_background);

            float text_offset_x = 0.0f;
            if(draw_thumbnail) {
                if(item_thumbnail && item_thumbnail->getNativeHandle() != 0) {
                    image.setTexture(*item_thumbnail, true);
                    auto image_size = image.getTexture()->getSize();
                    auto height_ratio = image_height / image_size.y;
                    auto scale = image.getScale();
                    auto image_scale_ratio = scale.x / scale.y;
                    const float width_ratio = height_ratio * image_scale_ratio;
                    image.setScale(width_ratio, height_ratio);
                    image.setPosition(item_pos);
                    window.draw(image);
                    text_offset_x = width_ratio * image_size.x;
                } else {
                    image_fallback.setPosition(item_pos);
                    window.draw(image_fallback);
                }
            }

            title_text.setString(item->title);
            title_text.setPosition(item_pos.x + text_offset_x + 10.0f, item_pos.y);
            window.draw(title_text);

            pos.y += row_height + 10.0f;
        }
    }

    //static
    bool Body::string_find_case_insensitive(const std::string &str, const std::string &substr) {
        auto it = std::search(str.begin(), str.end(), substr.begin(), substr.end(),
            [](char c1, char c2) {
                return std::toupper(c1) == std::toupper(c2);
            });
        return it != str.end();
    }

    void Body::filter_search_fuzzy(const std::string &text) {
        if(text.empty()) {
            for(auto &item : items) {
                item->visible = true;
            }
            return;
        }

        for(auto &item : items) {
            item->visible = string_find_case_insensitive(item->title, text);
        }
    }
}