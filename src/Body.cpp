#include "../include/Body.hpp"
#include "../include/QuickMedia.hpp"
#include "../plugins/Plugin.hpp"
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <assert.h>
#include <cmath>

const sf::Color front_color(43, 45, 47);
const sf::Color back_color(33, 35, 37);

namespace QuickMedia {
    Body::Body(Program *program, sf::Font &font) :
        program(program),
        title_text("", font, 14),
        progress_text("", font, 14),
        selected_item(0),
        draw_thumbnails(false),
        loading_thumbnail(false)
    {
        title_text.setFillColor(sf::Color::White);
        progress_text.setFillColor(sf::Color::White);
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

    void Body::select_first_item() {
        selected_item = 0;
        clamp_selection();
    }

    void Body::reset_selected() {
        selected_item = 0;
    }

    void Body::clear_items() {
        items.clear();
        selected_item = 0;
        //item_thumbnail_textures.clear();
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

    std::shared_ptr<sf::Texture> Body::load_thumbnail_from_url(const std::string &url) {
        auto result = std::make_shared<sf::Texture>();
        result->setSmooth(true);
        assert(!loading_thumbnail);
        loading_thumbnail = true;
        thumbnail_load_thread = std::thread([this, result, url]() {
            std::string texture_data;
            if(program->get_current_plugin()->download_to_string(url, texture_data) == DownloadResult::OK) {
                if(result->loadFromMemory(texture_data.data(), texture_data.size())) {
                    //result->generateMipmap();
                    result->setSmooth(true);
                }
            }
            loading_thumbnail = false;
        });
        thumbnail_load_thread.detach();
        return result;
    }

    void Body::draw(sf::RenderWindow &window, sf::Vector2f pos, sf::Vector2f size) {
        Json::Value empty_object(Json::objectValue);
        draw(window, pos, size, empty_object);
    }

    // TODO: Use a render target for the whole body so all images can be put into one.
    // TODO: Unload thumbnails once they are no longer visible on the screen.
    // TODO: Load thumbnails with more than one thread.
    // TODO: Show chapters (rows) that have been read differently to make it easier to see what
    // hasn't been read yet.
    void Body::draw(sf::RenderWindow &window, sf::Vector2f pos, sf::Vector2f size, const Json::Value &content_progress) {
        const float font_height = title_text.getCharacterSize() + 8.0f;
        const float image_height = 100.0f;

        sf::RectangleShape image_fallback(sf::Vector2f(50, image_height));
        image_fallback.setFillColor(sf::Color::White);

        sf::Sprite image;

        sf::RectangleShape item_background;
        item_background.setFillColor(front_color);
        //item_background.setOutlineThickness(1.0f);
        //item_background.setOutlineColor(sf::Color(13, 15, 17));
        sf::RectangleShape item_background_shadow;
        item_background_shadow.setFillColor(sf::Color(23, 25, 27));

        sf::RectangleShape selected_border;
        selected_border.setFillColor(sf::Color(0, 85, 119));
        const float selected_border_width = 5.0f;

        int num_items = items.size();
        if(num_items == 0)
            return;

        if((int)item_thumbnail_textures.size() != num_items)
            item_thumbnail_textures.resize(num_items);

        float row_height = font_height;
        if(draw_thumbnails)
            row_height = image_height;
        const float total_row_height = row_height + 15.0f;
        const int max_visible_rows = size.y / total_row_height - 1;

        // Find the starting row that can be drawn to make selected row visible as well
        int visible_rows = 0;
        int first_visible_item = selected_item;
        assert(first_visible_item >= 0 && first_visible_item < (int)items.size());
        for(; first_visible_item >= 0 && visible_rows < max_visible_rows; --first_visible_item) {
            auto &item = items[first_visible_item];
            if(item->visible)
                ++visible_rows;
        }

        auto window_size = window.getSize();

        for(int i = first_visible_item + 1; i < num_items; ++i) {
            const auto &item = items[i];
            auto &item_thumbnail = item_thumbnail_textures[i];

            if(pos.y >= window_size.y)
                return;

            if(!item->visible)
                continue;

            float row_height = font_height;
            if(draw_thumbnails) {
                row_height = image_height;
                // TODO: Should this be optimized by instead of checking if url changes based on index,
                // put thumbnails in hash map based on url?
                if(item->thumbnail_url.empty() && item_thumbnail.texture) {
                    item_thumbnail.texture = nullptr;
                    item_thumbnail.url = "";
                } else if(!item->thumbnail_url.empty() && !loading_thumbnail && (!item_thumbnail.texture || item_thumbnail.url != item->url)) {
                    item_thumbnail.url = item->url;
                    item_thumbnail.texture = load_thumbnail_from_url(item->thumbnail_url);
                }
            }

            sf::Vector2f item_pos = pos;
            if(i == selected_item) {
                selected_border.setPosition(pos);
                selected_border.setSize(sf::Vector2f(selected_border_width, row_height));
                window.draw(selected_border);
                item_pos.x += selected_border_width;
                item_background.setFillColor(sf::Color(0, 85, 119));
            } else {
                item_background.setFillColor(front_color);
            }

            item_pos.x = std::floor(item_pos.x);
            item_pos.y = std::floor(item_pos.y);
            item_background_shadow.setPosition(item_pos + sf::Vector2f(5.0f, 5.0f));
            item_background_shadow.setSize(sf::Vector2f(size.x, row_height));
            window.draw(item_background_shadow);
            item_background.setPosition(item_pos);
            item_background.setSize(sf::Vector2f(size.x, row_height));
            window.draw(item_background);

            float text_offset_x = 0.0f;
            if(draw_thumbnails) {
                // TODO: Verify if this is safe. The thumbnail is being modified in another thread
                // and it might not be fully finished before the native handle is set?
                if(item_thumbnail.texture && item_thumbnail.texture->getNativeHandle() != 0) {
                    image.setTexture(*item_thumbnail.texture, true);
                    auto image_size = image.getTexture()->getSize();
                    auto height_ratio = image_height / image_size.y;
                    auto scale = image.getScale();
                    auto image_scale_ratio = scale.x / scale.y;
                    const float width_ratio = height_ratio * image_scale_ratio;
                    image.setScale(width_ratio, height_ratio);
                    image.setPosition(item_pos);
                    window.draw(image);
                    text_offset_x = width_ratio * image_size.x;
                } else if(!item->thumbnail_url.empty()) {
                    image_fallback.setPosition(item_pos);
                    window.draw(image_fallback);
                    text_offset_x = image_fallback.getSize().x;
                }
            }

            title_text.setString(item->title);
            title_text.setPosition(std::floor(item_pos.x + text_offset_x + 10.0f), std::floor(item_pos.y));
            window.draw(title_text);

            // TODO: Do the same for non-manga content
            const Json::Value &item_progress = content_progress[item->title];
            if(item_progress.isObject()) {
                const Json::Value &current_json = item_progress["current"];
                const Json::Value &total_json = item_progress["total"];
                if(current_json.isNumeric() && total_json.isNumeric()) {
                    progress_text.setString(std::string("Page: ") + std::to_string(current_json.asInt()) + "/" + std::to_string(total_json.asInt()));
                    auto bounds = progress_text.getLocalBounds();
                    progress_text.setPosition(std::floor(item_pos.x + size.x - bounds.width - 10.0f), std::floor(item_pos.y));
                    window.draw(progress_text);
                }
            }

            pos.y += total_row_height;
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