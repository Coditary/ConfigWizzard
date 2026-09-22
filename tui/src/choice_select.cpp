#include "configwizard_ui/choice_select.hpp"

#include <tuinator/core/event.hpp>
#include <tuinator/render/canvas.hpp>
#include <tuinator/render/text.hpp>

#include <algorithm>

namespace configwizard_ui {

ChoiceSelect::ChoiceSelect(std::vector<ChoiceItem> items, int selected_index, tuinator::Style item_style,
                           tuinator::Style selected_style, tuinator::Style muted_style, int max_visible)
    : items_(std::move(items)), item_style_(item_style), selected_style_(selected_style),
      muted_style_(muted_style), max_visible_(std::max(1, max_visible)) {
    if (!items_.empty()) {
        selected_index_ = std::clamp(selected_index, 0, static_cast<int>(items_.size()) - 1);
    }
}

int ChoiceSelect::overflow_indicator_rows() const {
    return static_cast<int>(items_.size()) > max_visible_ ? 1 : 0;
}

int ChoiceSelect::item_row_capacity(const int bounds_height) const {
    return std::max(0, bounds_height - overflow_indicator_rows());
}

tuinator::Size ChoiceSelect::preferred_size() const {
    int width = 4;
    for (const ChoiceItem &item : items_) {
        width = std::max(width, tuinator::text_display_width(item.label) + 4);
    }
    const int item_rows = std::min(static_cast<int>(items_.size()), max_visible_);
    const int height = item_rows + overflow_indicator_rows();
    return {width, std::max(1, height)};
}

void ChoiceSelect::paint(tuinator::PaintContext &ctx) const {
    tuinator::Canvas &canvas = ctx.canvas;
    if (bounds_.width <= 0 || bounds_.height <= 0 || items_.empty()) {
        return;
    }

    const bool focused = is_focused();
    const int item_rows = item_row_capacity(bounds_.height);
    const int visible = std::min(static_cast<int>(items_.size()), item_rows);
    const bool show_overflow = static_cast<int>(items_.size()) > visible;

    for (int row = 0; row < visible; ++row) {
        const int index = scroll_y_ + row;
        if (index >= static_cast<int>(items_.size())) {
            break;
        }
        const ChoiceItem &item = items_[static_cast<std::size_t>(index)];
        const bool selected = index == selected_index_;

        tuinator::Style style = selected ? selected_style_ : item_style_;
        if (selected && focused) {
            style.reverse = true;
        }

        const std::string prefix = selected ? "> " : "  ";
        canvas.draw_text({0, row}, prefix, style);
        const int max_columns = std::max(0, bounds_.width - 2);
        const std::size_t bytes = tuinator::text_byte_length_for_width(item.label, max_columns);
        canvas.draw_text({2, row}, item.label.substr(0, bytes), style);
    }

    if (show_overflow) {
        const std::string more = std::to_string(items_.size() - visible) + " more";
        canvas.draw_text({0, visible}, more, muted_style_);
    }
}

bool ChoiceSelect::handle_event(const tuinator::Event &event) {
    const auto *key = std::get_if<tuinator::KeyPress>(&event);
    if (key == nullptr || !is_focused() || items_.empty()) {
        return false;
    }

    if (key->key == tuinator::Key::Up) {
        move(-1);
        return true;
    }
    if (key->key == tuinator::Key::Down) {
        move(1);
        return true;
    }
    if (key->key == tuinator::Key::Home) {
        selected_index_ = 0;
        ensure_visible();
        mark_dirty();
        return true;
    }
    if (key->key == tuinator::Key::End) {
        selected_index_ = static_cast<int>(items_.size()) - 1;
        ensure_visible();
        mark_dirty();
        return true;
    }
    if (key->key == tuinator::Key::Enter || key->character == '\r') {
        if (on_advance_) {
            on_advance_();
        }
        return true;
    }

    return false;
}

void ChoiceSelect::move(const int delta) {
    if (items_.empty()) {
        return;
    }
    const int size = static_cast<int>(items_.size());
    selected_index_ = (selected_index_ + delta + size) % size;
    ensure_visible();
    mark_dirty();
    if (on_change_) {
        on_change_(selected_index_);
    }
}

void ChoiceSelect::ensure_visible() {
    if (selected_index_ < scroll_y_) {
        scroll_y_ = selected_index_;
    }
    const int visible = std::max(1, item_row_capacity(bounds_.height > 0 ? bounds_.height : max_visible_));
    if (selected_index_ >= scroll_y_ + visible) {
        scroll_y_ = selected_index_ - visible + 1;
    }
}

} // namespace configwizard_ui
