#pragma once

#include <tuinator/render/style.hpp>
#include <tuinator/widgets/widget.hpp>

#include <functional>
#include <string>
#include <vector>

namespace configwizard_ui {

struct FuzzyItem {
    std::string value;
    std::string label;
};

/// Fuzzy (fzf-like) single-select overlay. Type to filter, Up/Down to move,
/// Enter to accept, Escape to cancel.
class FuzzySelect : public tuinator::Widget {
  public:
    FuzzySelect(std::vector<FuzzyItem> items, std::string initial_value,
                tuinator::Style border_style, tuinator::Style item_style,
                tuinator::Style selected_style, tuinator::Style muted_style);

    void set_on_select(std::function<void(const FuzzyItem &)> callback) { on_select_ = std::move(callback); }
    void set_on_cancel(std::function<void()> callback) { on_cancel_ = std::move(callback); }

    bool wants_full_screen() const override { return true; }
    std::string_view widget_type_name() const override { return "FuzzySelect"; }
    tuinator::Size preferred_size() const override;
    void paint(tuinator::PaintContext &ctx) const override;
    bool handle_event(const tuinator::Event &event) override;
    bool is_focusable() const override { return true; }

  private:
    static bool fuzzy_match(const std::string &haystack, const std::string &needle);
    void rebuild_matches();
    void move(int delta);
    void accept();

    std::vector<FuzzyItem> items_;
    std::vector<int> matches_;
    std::string query_;
    int selected_ = 0;
    tuinator::Style border_style_;
    tuinator::Style item_style_;
    tuinator::Style selected_style_;
    tuinator::Style muted_style_;
    std::function<void(const FuzzyItem &)> on_select_;
    std::function<void()> on_cancel_;
};

} // namespace configwizard_ui
