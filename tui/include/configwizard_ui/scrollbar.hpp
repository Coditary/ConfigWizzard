#pragma once

#include <tuinator/render/style.hpp>
#include <tuinator/widgets/widget.hpp>

#include <functional>

namespace configwizard_ui {

/// Simple vertical scrollbar that renders a track and thumb.
/// Syncs with a ScrollView via set_scroll_position().
class Scrollbar : public tuinator::Widget {
  public:
    Scrollbar(tuinator::Style track_style, tuinator::Style thumb_style);

    void set_scroll_position(int scroll_y, int content_height, int viewport_height);

    std::string_view widget_type_name() const override { return "Scrollbar"; }
    tuinator::Size preferred_size() const override { return {1, 10}; }
    void paint(tuinator::PaintContext &ctx) const override;
    bool is_focusable() const override { return false; }

  private:
    tuinator::Style track_style_;
    tuinator::Style thumb_style_;
    int scroll_y_ = 0;
    int content_height_ = 0;
    int viewport_height_ = 0;
};

} // namespace configwizard_ui
