#pragma once

#include <tuinator/render/style.hpp>
#include <tuinator/widgets/widget.hpp>

#include <functional>
#include <string>
#include <vector>

namespace configwizard_ui {

struct ChoiceItem {
    std::string value;
    std::string label;
};

/// Inline choice selector: renders a compact list with a highlighted row.
/// Keyboard: Up/Down to move, Enter to confirm (focus moves on).
class ChoiceSelect : public tuinator::Widget {
  public:
    ChoiceSelect(std::vector<ChoiceItem> items, int selected_index,
                 tuinator::Style item_style, tuinator::Style selected_style,
                 tuinator::Style muted_style, int max_visible = 5);

    int selected_index() const { return selected_index_; }
    const ChoiceItem &selected_item() const { return items_[static_cast<std::size_t>(selected_index_)]; }
    void set_on_change(std::function<void(int)> callback) { on_change_ = std::move(callback); }
    void set_on_advance(std::function<void()> callback) { on_advance_ = std::move(callback); }

    std::string_view widget_type_name() const override { return "ChoiceSelect"; }
    tuinator::Size preferred_size() const override;
    void paint(tuinator::PaintContext &ctx) const override;
    bool handle_event(const tuinator::Event &event) override;
    bool is_focusable() const override { return true; }

  private:
    void move(int delta);
    void ensure_visible();
    int overflow_indicator_rows() const;
    int item_row_capacity(int bounds_height) const;

    std::vector<ChoiceItem> items_;
    int selected_index_ = 0;
    int scroll_y_ = 0;
    int max_visible_ = 5;
    tuinator::Style item_style_;
    tuinator::Style selected_style_;
    tuinator::Style muted_style_;
    std::function<void(int)> on_change_;
    std::function<void()> on_advance_;
};

} // namespace configwizard_ui
