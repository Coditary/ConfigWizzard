#include "configwizard_ui/scrollbar.hpp"

#include <tuinator/render/canvas.hpp>

#include <algorithm>

namespace configwizard_ui {

Scrollbar::Scrollbar(tuinator::Style track_style, tuinator::Style thumb_style)
    : track_style_(track_style), thumb_style_(thumb_style) {}

void Scrollbar::set_scroll_position(const int scroll_y, const int content_height, const int viewport_height) {
    scroll_y_ = scroll_y;
    content_height_ = content_height;
    viewport_height_ = viewport_height;
    mark_dirty();
}

void Scrollbar::paint(tuinator::PaintContext &ctx) const {
    tuinator::Canvas &canvas = ctx.canvas;
    if (bounds_.width <= 0 || bounds_.height <= 0) {
        return;
    }

    const int track_height = bounds_.height;
    if (track_height <= 0 || content_height_ <= 0 || viewport_height_ <= 0) {
        return;
    }

    // Calculate thumb size and position
    const int thumb_size = std::max(1, track_height * viewport_height_ / content_height_);
    const int max_scroll = std::max(0, content_height_ - viewport_height_);
    const int thumb_pos = max_scroll > 0 ? (track_height - thumb_size) * scroll_y_ / max_scroll : 0;

    for (int row = 0; row < track_height; ++row) {
        const bool is_thumb = row >= thumb_pos && row < thumb_pos + thumb_size;
        canvas.draw_text({0, row}, is_thumb ? "█" : "│", is_thumb ? thumb_style_ : track_style_);
    }
}

} // namespace configwizard_ui
