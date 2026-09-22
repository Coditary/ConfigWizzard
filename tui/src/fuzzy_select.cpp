#include "configwizard_ui/fuzzy_select.hpp"

#include <tuinator/core/event.hpp>
#include <tuinator/render/canvas.hpp>
#include <tuinator/render/glyphs.hpp>
#include <tuinator/render/text.hpp>

#include <algorithm>
#include <cctype>

namespace configwizard_ui {

namespace {

bool fuzzy_match(const std::string &haystack, const std::string &needle) {
    if (needle.empty()) {
        return true;
    }
    std::size_t start = 0;
    for (const char ch : needle) {
        const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        bool found = false;
        for (std::size_t i = start; i < haystack.size(); ++i) {
            const char candidate = static_cast<char>(std::tolower(static_cast<unsigned char>(haystack[i])));
            if (candidate == lower) {
                start = i + 1;
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

} // namespace

FuzzySelect::FuzzySelect(std::vector<FuzzyItem> items, std::string initial_value, tuinator::Style border_style,
                         tuinator::Style item_style, tuinator::Style selected_style, tuinator::Style muted_style)
    : items_(std::move(items)), border_style_(border_style), item_style_(item_style),
      selected_style_(selected_style), muted_style_(muted_style) {
    rebuild_matches();
    if (!initial_value.empty()) {
        for (std::size_t index = 0; index < matches_.size(); ++index) {
            if (items_[static_cast<std::size_t>(matches_[index])].value == initial_value) {
                selected_ = static_cast<int>(index);
                break;
            }
        }
    }
}

tuinator::Size FuzzySelect::preferred_size() const { return {bounds_.width, bounds_.height}; }

void FuzzySelect::paint(tuinator::PaintContext &ctx) const {
    tuinator::Canvas &canvas = ctx.canvas;
    if (bounds_.width <= 0 || bounds_.height <= 0) {
        return;
    }

    tuinator::Style dim = item_style_;
    dim.dim = true;
    canvas.fill_rect({0, 0, bounds_.width, bounds_.height}, ' ', dim);

    const int width = std::min(56, std::max(30, bounds_.width - 8));
    const int list_rows = std::min(8, static_cast<int>(matches_.size()));
    const int height = list_rows + 4;
    const int x = std::max(0, (bounds_.width - width) / 2);
    const int y = 2;

    canvas.draw_box({x, y, width, height}, border_style_, tuinator::unicode_heavy_border_glyphs());

    const std::string query_line = "> " + query_ + "_";
    canvas.draw_text({x + 2, y + 1}, query_line, item_style_);

    const int list_y = y + 2;
    for (int row = 0; row < list_rows; ++row) {
        const FuzzyItem &item = items_[static_cast<std::size_t>(matches_[static_cast<std::size_t>(row)])];
        const bool selected = row == selected_;
        const tuinator::Style style = selected ? selected_style_ : item_style_;
        const std::string prefix = selected ? "> " : "  ";
        canvas.draw_text({x + 2, list_y + row}, prefix + item.label, style);
    }

    const std::string hint = "Enter select · Esc cancel";
    canvas.draw_text({x + 2, y + height - 1}, hint, muted_style_);
}

bool FuzzySelect::handle_event(const tuinator::Event &event) {
    const auto *key = std::get_if<tuinator::KeyPress>(&event);
    if (key == nullptr) {
        return true;
    }

    if (key->key == tuinator::Key::Escape) {
        if (on_cancel_) {
            on_cancel_();
        }
        return true;
    }
    if (key->key == tuinator::Key::Up) {
        move(-1);
        return true;
    }
    if (key->key == tuinator::Key::Down) {
        move(1);
        return true;
    }
    if (key->key == tuinator::Key::Enter) {
        accept();
        return true;
    }
    if (key->key == tuinator::Key::Backspace) {
        if (!query_.empty()) {
            query_.pop_back();
            rebuild_matches();
            mark_dirty();
        }
        return true;
    }
    if (key->character >= 32 && key->character <= 126) {
        query_.push_back(key->character);
        rebuild_matches();
        mark_dirty();
        return true;
    }

    return true;
}

void FuzzySelect::rebuild_matches() {
    matches_.clear();
    for (int index = 0; index < static_cast<int>(items_.size()); ++index) {
        const FuzzyItem &item = items_[static_cast<std::size_t>(index)];
        if (fuzzy_match(item.label + " " + item.value, query_)) {
            matches_.push_back(index);
        }
    }
    selected_ = std::clamp(selected_, 0, std::max(0, static_cast<int>(matches_.size()) - 1));
}

void FuzzySelect::move(const int delta) {
    if (matches_.empty()) {
        return;
    }
    const int size = static_cast<int>(matches_.size());
    selected_ = (selected_ + delta + size) % size;
    mark_dirty();
}

void FuzzySelect::accept() {
    if (matches_.empty()) {
        return;
    }
    if (on_select_) {
        on_select_(items_[static_cast<std::size_t>(matches_[static_cast<std::size_t>(selected_)])]);
    }
}

} // namespace configwizard_ui
