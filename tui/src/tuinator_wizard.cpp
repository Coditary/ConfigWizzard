#include "configwizard_ui/tuinator_wizard.hpp"
#include "configwizard_ui/choice_select.hpp"
#include "configwizard_ui/theme.hpp"

#include <tuinator/tuinator.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace configwizard_ui {

namespace {

bool parse_bool(const std::string &value) {
    return value == "true" || value == "1" || value == "yes" || value == "on";
}

std::string bool_to_string(const bool value) {
    return value ? "true" : "false";
}

std::string trim_trailing_zeros(std::string value) {
    if (value.find('.') == std::string::npos) {
        return value;
    }
    while (!value.empty() && value.back() == '0') {
        value.pop_back();
    }
    if (!value.empty() && value.back() == '.') {
        value.pop_back();
    }
    return value;
}

std::string format_number_string(const std::string &value) {
    const auto decimal = value.find('.');
    if (decimal == std::string::npos) {
        return value;
    }
    return trim_trailing_zeros(value);
}

std::string field_value(const WizardField &field, const std::map<std::string, std::string> &values) {
    const auto current = values.find(field.key);
    if (current != values.end()) {
        return current->second;
    }
    return field.default_value;
}

std::string display_default(const WizardField &field, const std::map<std::string, std::string> &values) {
    const std::string value = field_value(field, values);
    if (value.empty()) {
        return {};
    }
    if (field.type == "number") {
        return format_number_string(value);
    }
    if (field.type == "multiselect") {
        return {};
    }
    return value;
}

std::vector<std::string> parse_multiselect_default(const std::string &value) {
    std::vector<std::string> selected;
    if (value.empty()) {
        return selected;
    }

    if (value.front() == '[') {
        std::string item;
        bool in_quotes = false;
        for (std::size_t index = 1; index < value.size(); ++index) {
            const char ch = value[index];
            if (ch == '"') {
                in_quotes = !in_quotes;
                continue;
            }
            if (!in_quotes && (ch == ',' || ch == ']')) {
                if (!item.empty()) {
                    selected.push_back(item);
                    item.clear();
                }
                continue;
            }
            if (!std::isspace(static_cast<unsigned char>(ch)) || in_quotes) {
                item.push_back(ch);
            }
        }
        if (!item.empty()) {
            selected.push_back(item);
        }
        return selected;
    }

    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ',')) {
        while (!item.empty() && std::isspace(static_cast<unsigned char>(item.front()))) {
            item.erase(item.begin());
        }
        while (!item.empty() && std::isspace(static_cast<unsigned char>(item.back()))) {
            item.pop_back();
        }
        if (!item.empty()) {
            selected.push_back(item);
        }
    }
    return selected;
}

std::string serialize_multiselect(const std::vector<std::string> &values) {
    std::string json = "[";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            json.push_back(',');
        }
        json.push_back('"');
        json += values[index];
        json.push_back('"');
    }
    json.push_back(']');
    return json;
}

/// Full-terminal backdrop that paints over stale cells while keeping content compact.
class WizardBackdrop : public tuinator::Widget {
  public:
    void set_content(std::unique_ptr<tuinator::Widget> content) {
        children_.clear();
        content_ = nullptr;
        if (content) {
            add_child(std::move(content));
            content_ = children_.back().get();
        }
        mark_layout_dirty();
    }

    bool wants_full_screen() const override { return true; }

    tuinator::Size preferred_size() const override {
        if (content_ == nullptr) {
            return {0, 0};
        }
        return content_->preferred_size();
    }

    void layout(tuinator::Rect bounds) override {
        bounds_ = bounds;
        if (content_ == nullptr) {
            return;
        }

        const tuinator::Size preferred = content_->preferred_size();
        const int width = std::min(preferred.width, bounds.width);
        const int height = std::min(preferred.height, bounds.height);
        content_->layout({bounds.x, bounds.y, width, height});
    }

    void paint(tuinator::PaintContext &ctx) const override {
        paint_bounds_background(ctx);

        if (content_ == nullptr) {
            return;
        }

        const tuinator::Rect local{
            content_->bounds().x - bounds_.x,
            content_->bounds().y - bounds_.y,
            content_->bounds().width,
            content_->bounds().height,
        };
        ctx.with_clip(local, [&](tuinator::PaintContext &child_ctx) { content_->paint(child_ctx); });
    }

    tuinator::Widget *hit_test(tuinator::Point point) override {
        if (content_ == nullptr) {
            return nullptr;
        }
        return content_->hit_test(point);
    }

    tuinator::Widget *hit_test_focusable(tuinator::Point point) override {
        if (content_ == nullptr) {
            return nullptr;
        }
        return content_->hit_test_focusable(point);
    }

    void collect_focusable(std::vector<tuinator::Widget *> &out) override {
        if (content_ != nullptr) {
            content_->collect_focusable(out);
        }
    }

    bool handle_event(const tuinator::Event &event) override {
        if (content_ == nullptr) {
            return false;
        }
        return content_->handle_event(event);
    }

    std::string_view widget_type_name() const override { return "WizardBackdrop"; }

  private:
    tuinator::Widget *content_ = nullptr;
};

class ProgressDot : public tuinator::Widget {
  public:
    ProgressDot(const bool active, const tuinator::Style &active_style, const tuinator::Style &inactive_style,
                std::function<void()> on_click)
        : active_(active), active_style_(active_style), inactive_style_(inactive_style),
          on_click_(std::move(on_click)) {}

    tuinator::Size preferred_size() const override { return {2, 1}; }

    void paint(tuinator::PaintContext &ctx) const override {
        tuinator::Style style = active_ ? active_style_ : inactive_style_;
        if (is_hovered() && on_click_) {
            style.bold = true;
        }
        ctx.canvas.draw_text({0, 0}, active_ ? "● " : "○ ", style);
    }

    bool handle_event(const tuinator::Event &event) override {
        if (!on_click_) {
            return false;
        }

        const auto *mouse = std::get_if<tuinator::MouseEvent>(&event);
        if (mouse == nullptr) {
            return false;
        }

        if (mouse->action != tuinator::MouseAction::Click && mouse->action != tuinator::MouseAction::Release) {
            return false;
        }

        if (!contains_point(mouse->position)) {
            return false;
        }

        on_click_();
        mark_dirty();
        return true;
    }

    bool is_focusable() const override { return false; }
    bool wants_hover() const override { return on_click_ != nullptr; }

    std::string_view widget_type_name() const override { return "ProgressDot"; }

  private:
    bool active_ = false;
    tuinator::Style active_style_;
    tuinator::Style inactive_style_;
    std::function<void()> on_click_;
};

tuinator::CheckboxOptions checkbox_options(const tuinator::Theme &theme) {
    tuinator::CheckboxOptions options = tuinator::checkbox_options_default(theme);
    options.glyphs = tuinator::CheckboxGlyphs{
        .unchecked = "□",
        .checked = "■",
        .gap = " ",
    };
    return options;
}

struct FieldBinding {
    std::string key;
    bool is_bool = false;
    tuinator::TextInput *text_input = nullptr;
    tuinator::Checkbox *checkbox = nullptr;
    ChoiceSelect *choice_select = nullptr;
    std::vector<tuinator::Checkbox *> multiselect_boxes;
    std::vector<std::string> multiselect_values;
};

void collect_bindings(const std::vector<FieldBinding> &bindings, std::map<std::string, std::string> &values) {
    for (const FieldBinding &binding : bindings) {
        if (binding.is_bool && binding.checkbox != nullptr) {
            values[binding.key] = bool_to_string(binding.checkbox->checked());
            continue;
        }
        if (!binding.multiselect_boxes.empty()) {
            std::vector<std::string> selected;
            for (std::size_t index = 0; index < binding.multiselect_boxes.size(); ++index) {
                if (binding.multiselect_boxes[index]->checked()) {
                    selected.push_back(binding.multiselect_values[index]);
                }
            }
            values[binding.key] = serialize_multiselect(selected);
            continue;
        }
        if (binding.choice_select != nullptr) {
            values[binding.key] = binding.choice_select->selected_item().value;
            continue;
        }
        if (binding.text_input != nullptr) {
            values[binding.key] = binding.text_input->value();
        }
    }
}

int compute_form_width(const tuinator::Application &app) {
    const int terminal_width = app.terminal_size().width;
    const int horizontal_margin = 8;
    return std::max(40, std::min(terminal_width - horizontal_margin, 72));
}

std::unique_ptr<tuinator::Widget> align_left(std::unique_ptr<tuinator::Widget> child) {
    auto row = std::make_unique<tuinator::HBox>(tuinator::BoxOptions{.gap = 0, .padding = 0});
    row->add_child(std::move(child));
    return row;
}

void add_left_aligned(tuinator::VBox &parent, std::unique_ptr<tuinator::Widget> child) {
    parent.add_child(align_left(std::move(child)));
}

tuinator::TextInputOptions text_input_options(const WizardField &field, const int field_width) {
    tuinator::TextInputOptions options{
        .min_width = field_width,
        .placeholder = field.placeholder.empty() ? " " : field.placeholder,
        .password = field.sensitive,
        .chrome = tuinator::TextInputChrome::Framed,
        .min_content_lines = 1,
        .max_content_lines = 1,
        .border_label = field.prompt,
    };
    return options;
}

std::unique_ptr<tuinator::Widget> make_field_block(const WizardField &field, const WizardPalette &palette,
                                                   const std::map<std::string, std::string> &values,
                                                   const int form_width, FieldBinding &binding,
                                                   tuinator::Application &app,
                                                   const std::function<void()> &on_visibility_change) {
    const std::string current_value = field_value(field, values);
    auto block = std::make_unique<tuinator::VBox>(tuinator::BoxOptions{.gap = 0, .padding = 0});

    if ((field.type == "multiselect" || field.type == "choice") && !field.prompt.empty()) {
        add_left_aligned(*block, std::make_unique<tuinator::Label>(field.prompt, palette.label(), false));
    }
    if (!field.help.empty()) {
        auto help = std::make_unique<tuinator::Label>(field.help, palette.muted_text(), false);
        help->add_widget_class("muted");
        add_left_aligned(*block, std::move(help));
    }

    const tuinator::CheckboxOptions checkbox_opts = checkbox_options(palette.to_theme());
    const auto notify_visibility_change = [on_visibility_change](bool) {
        if (on_visibility_change) {
            on_visibility_change();
        }
    };

    if (field.type == "bool") {
        binding.is_bool = true;
        const std::string label = field.prompt.empty() ? field.key : field.prompt;
        auto checkbox = std::make_unique<tuinator::Checkbox>(label, parse_bool(current_value), checkbox_opts,
                                                             notify_visibility_change);
        checkbox->set_on_submit([&app]() { app.advance_focus(); });
        binding.checkbox = checkbox.get();
        add_left_aligned(*block, std::move(checkbox));
        return block;
    }

    if (field.type == "multiselect") {
        const std::vector<std::string> defaults = parse_multiselect_default(current_value);
        auto choices = std::make_unique<tuinator::VBox>(tuinator::BoxOptions{.gap = 0, .padding = 0});
        for (const ChoiceView &choice : field.choices) {
            const bool checked =
                std::find(defaults.begin(), defaults.end(), choice.value) != defaults.end();
            const std::string label = choice.label.empty() ? choice.value : choice.label;
            auto checkbox =
                std::make_unique<tuinator::Checkbox>(label, checked, checkbox_opts, notify_visibility_change);
            checkbox->set_on_submit([&app]() { app.advance_focus(); });
            binding.multiselect_boxes.push_back(checkbox.get());
            binding.multiselect_values.push_back(choice.value);
            add_left_aligned(*choices, std::move(checkbox));
        }
        add_left_aligned(*block, std::move(choices));
        return block;
    }

    if (field.type == "choice") {
        std::vector<ChoiceItem> items;
        items.reserve(field.choices.size());
        for (const ChoiceView &choice : field.choices) {
            items.push_back(ChoiceItem{
                .value = choice.value,
                .label = choice.label.empty() ? choice.value : choice.label,
            });
        }

        int selected = 0;
        if (!current_value.empty()) {
            const auto it = std::find_if(field.choices.begin(), field.choices.end(), [&](const ChoiceView &choice) {
                return choice.value == current_value;
            });
            if (it != field.choices.end()) {
                selected = static_cast<int>(std::distance(field.choices.begin(), it));
            }
        }

        auto select = std::make_unique<ChoiceSelect>(std::move(items), selected, palette.label(),
                                                     palette.accent_text(), palette.muted_text(), 5);
        select->set_on_advance([&app]() { app.advance_focus(); });
        binding.choice_select = select.get();
        add_left_aligned(*block, std::move(select));
        return block;
    }

    const int field_width = std::max(20, form_width - 5);
    auto input = std::make_unique<tuinator::TextInput>(text_input_options(field, field_width), palette.input(),
                                                       palette.input_focused());
    const std::string default_value = display_default(field, values);
    if (!default_value.empty()) {
        input->set_value(default_value);
    }
    input->set_on_submit([&app](const std::string &) { app.advance_focus(); });
    binding.text_input = input.get();
    add_left_aligned(*block, std::move(input));
    return block;
}

int form_viewport_height(const tuinator::Size &form_size, const int available_height) {
    return std::max(3, std::min(form_size.height, available_height));
}

tuinator::ScrollViewOptions form_scroll_options(const int form_width, const int viewport_height) {
    auto scroll_options = tuinator::ScrollViewOptions{
        .width = form_width,
        .height = viewport_height,
    };
    scroll_options.scrollbars.config.enabled = true;
    scroll_options.scrollbars.config.vertical = true;
    scroll_options.scrollbars.config.horizontal = false;
    return scroll_options;
}

void mount_form_in_scroll_view(tuinator::ScrollView &scroll_view, std::unique_ptr<tuinator::Widget> form_fields,
                               const int form_width, const int available_height) {
    const int viewport_height = form_viewport_height(form_fields->preferred_size(), available_height);
    scroll_view.set_options(form_scroll_options(form_width, viewport_height));
    scroll_view.set_content(std::move(form_fields));
}

bool section_uses_sub_panel(const WizardSection &section) {
    return !section.title.empty() || !section.id.empty();
}

bool page_uses_section_panels(const WizardGroupPage &page) {
    for (const WizardSection &section : page.sections) {
        if (section_uses_sub_panel(section)) {
            return true;
        }
    }
    return false;
}

std::string section_panel_title(const WizardSection &section) {
    if (!section.title.empty()) {
        return section.title;
    }
    if (!section.id.empty()) {
        return section.id;
    }
    return "Allgemein";
}

std::unique_ptr<tuinator::Widget> build_section_widget(const WizardSection &section, const WizardPalette &palette,
                                                      std::map<std::string, std::string> &values,
                                                      const int form_width, std::vector<FieldBinding> &bindings,
                                                      tuinator::Application &app,
                                                      const std::function<void()> &on_visibility_change) {
    const bool wrapped = section_uses_sub_panel(section);
    const int section_form_width = wrapped ? std::max(20, form_width - 6) : form_width;

    auto section_body = std::make_unique<tuinator::VBox>(tuinator::BoxOptions{.gap = 2, .padding = wrapped ? 1 : 0});
    if (!section.description.empty()) {
        auto description = std::make_unique<tuinator::Label>(section.description, palette.muted_text(), false);
        description->add_widget_class("muted");
        add_left_aligned(*section_body, std::move(description));
    }

    for (const WizardField &field : section.fields) {
        FieldBinding binding{.key = field.key};
        auto block =
            make_field_block(field, palette, values, section_form_width, binding, app, on_visibility_change);
        bindings.push_back(std::move(binding));
        section_body->add_child(std::move(block));
    }

    if (!wrapped) {
        return section_body;
    }

    auto section_panel = std::make_unique<tuinator::Panel>(section_panel_title(section), palette.muted_text(),
                                                           palette.label(), tuinator::unicode_rounded_border_glyphs());
    section_panel->set_content(std::move(section_body));
    return section_panel;
}

std::unique_ptr<tuinator::VBox> build_form_fields(const WizardGroupPage &page, const WizardPalette &palette,
                                                  std::map<std::string, std::string> &values, const int form_width,
                                                  std::vector<FieldBinding> &bindings, tuinator::Application &app,
                                                  const std::function<void()> &on_visibility_change) {
    auto form_fields = std::make_unique<tuinator::VBox>(tuinator::BoxOptions{.gap = 2, .padding = 0});
    bindings.clear();

    std::size_t total_fields = 0;
    for (const WizardSection &section : page.sections) {
        total_fields += section.fields.size();
    }
    bindings.reserve(total_fields);

    for (const WizardSection &section : page.sections) {
        if (section.fields.empty()) {
            continue;
        }

        form_fields->add_child(build_section_widget(section, palette, values, form_width, bindings, app,
                                                    on_visibility_change));
    }

    return form_fields;
}

void configure_wizard_app(tuinator::Application &app) {
    const WizardPalette &palette = wizard_palette();
    app.set_theme(palette.to_theme());
    try {
        app.load_stylesheet(palette.stylesheet_path());
    } catch (const std::exception &) {
        // Stylesheet is optional; built-in palette still applies.
    }
}

struct WizardPageHost {
    tuinator::Application &app;
    const WizardPalette &palette;
    std::map<std::string, std::string> &values;
    WizardNavigator *navigator = nullptr;
    GroupPageRefreshFn single_refresh;

    GroupPageResult single_result;
    WizardRunOutcome outcome = WizardRunOutcome::Aborted;

    std::size_t page_index = 0;
    std::vector<FieldBinding> bindings;
    tuinator::ScrollView *scroll_view = nullptr;
    WizardGroupPage current_page;
    std::function<void()> on_visibility_change;
    tuinator::TimerId pending_refresh_timer = -1;

    int form_width = 0;
    int available_height = 0;

    void configure_layout() {
        form_width = compute_form_width(app);
        const int chrome_height = 4 + 1 + 3 + 2 + 2 + 1 + 1 + 1;
        available_height = std::max(3, app.terminal_size().height - chrome_height);
    }

    std::size_t visible_page_count() const {
        if (navigator == nullptr || !navigator->page_count) {
            return 0;
        }
        return navigator->page_count(values);
    }

    GroupPageRefreshFn active_refresh() {
        if (navigator == nullptr) {
            return single_refresh;
        }
        return [this](const std::map<std::string, std::string> &current_values) {
            return navigator->load_page(page_index, current_values);
        };
    }

    void mount_page(const WizardGroupPage &page) {
        current_page = page;
        configure_layout();
        app.set_root(build_root(page));
        app.refresh_focus();
        app.present();
    }

    std::size_t find_next_index(const std::size_t from, const int direction) const {
        std::size_t index = from;
        while (true) {
            if (direction > 0) {
                if (index + 1 >= visible_page_count()) {
                    return visible_page_count();
                }
                ++index;
            } else {
                if (index == 0) {
                    return 0;
                }
                --index;
            }

            const WizardGroupPage page = navigator->load_page(index, values);
            if (navigator->should_show_page(page)) {
                return index;
            }

            if (direction < 0 && index == 0) {
                return 0;
            }
        }
    }

    void show_page_at(const std::size_t index) {
        std::size_t candidate = index;
        while (candidate < visible_page_count()) {
            WizardGroupPage page = navigator->load_page(candidate, values);
            if (navigator->should_show_page(page)) {
                page_index = candidate;
                if (navigator->seed_page) {
                    navigator->seed_page(page, values);
                }
                mount_page(page);
                return;
            }
            ++candidate;
        }

        outcome = WizardRunOutcome::Completed;
        app.quit();
    }

    void navigate(const GroupPageAction action, const std::size_t target_index) {
        collect_bindings(bindings, values);

        if (navigator == nullptr) {
            single_result.action = action;
            if (action == GroupPageAction::Jump) {
                single_result.jump_index = target_index;
            }
            app.quit();
            return;
        }

        const std::size_t from_index = page_index;
        if (navigator->on_navigate) {
            navigator->on_navigate(action, from_index, target_index, values);
        }

        if (action == GroupPageAction::Next && target_index >= visible_page_count()) {
            outcome = WizardRunOutcome::Completed;
            app.quit();
            return;
        }

        page_index = target_index;
        show_page_at(page_index);
    }

    void on_next(const WizardGroupPage &page) {
        if (navigator == nullptr) {
            navigate(GroupPageAction::Next, 0);
            return;
        }

        if (page.index >= page.total) {
            outcome = WizardRunOutcome::Completed;
            app.quit();
            return;
        }

        navigate(GroupPageAction::Next, find_next_index(page_index, 1));
    }

    void on_back() {
        if (navigator == nullptr) {
            navigate(GroupPageAction::Back, 0);
            return;
        }

        if (page_index == 0) {
            return;
        }

        navigate(GroupPageAction::Back, find_next_index(page_index, -1));
    }

    void on_jump(const std::size_t target_index) {
        if (target_index == page_index) {
            return;
        }
        navigate(GroupPageAction::Jump, target_index);
    }

    std::unique_ptr<tuinator::Widget> build_root(const WizardGroupPage &page) {
        scroll_view = nullptr;
        pending_refresh_timer = -1;
        on_visibility_change = nullptr;

        auto content = std::make_unique<tuinator::VBox>(tuinator::BoxOptions{.gap = 1, .padding = 2});
        content->set_widget_id("wizard-root");
        tuinator::VBox *root = content.get();

        auto header = std::make_unique<tuinator::HBox>(tuinator::BoxOptions{.gap = 2, .padding = 0});
        auto dots = std::make_unique<tuinator::HBox>(tuinator::BoxOptions{.gap = 0, .padding = 0});
        for (std::size_t step = 1; step <= page.total; ++step) {
            const bool active = step == page.index;
            auto dot = std::make_unique<ProgressDot>(
                active, palette.accent_text(), palette.muted_text(),
                [this, step]() { on_jump(step - 1); });
            if (active) {
                dot->add_widget_class("accent");
            } else {
                dot->add_widget_class("muted");
            }
            dots->add_child(std::move(dot));
        }
        header->add_child(std::move(dots));
        auto step = std::make_unique<tuinator::Label>(
            "Step " + std::to_string(page.index) + " of " + std::to_string(page.total), palette.muted_text(), false);
        step->add_widget_class("muted");
        header->add_child(std::move(step));
        root->add_child(align_left(std::move(header)));

        const GroupPageRefreshFn refresh = active_refresh();
        if (refresh) {
            on_visibility_change = [this, refresh]() {
                collect_bindings(bindings, values);

                if (pending_refresh_timer >= 0) {
                    app.cancel_timer(pending_refresh_timer);
                }

                pending_refresh_timer = app.set_timeout(1, [this, refresh]() {
                    pending_refresh_timer = -1;
                    const std::size_t previous_total = current_page.total;
                    WizardGroupPage refreshed = refresh(values);
                    if (refreshed.total != previous_total) {
                        current_page = refreshed;
                        mount_page(current_page);
                        return;
                    }

                    current_page = refreshed;
                    auto form_fields = build_form_fields(current_page, palette, values, form_width, bindings, app,
                                                         on_visibility_change);
                    if (scroll_view != nullptr) {
                        mount_form_in_scroll_view(*scroll_view, std::move(form_fields), form_width, available_height);
                    }
                    app.refresh_focus();
                    app.repaint_all();
                });
            };
        }

        auto form_fields = build_form_fields(page, palette, values, form_width, bindings, app, on_visibility_change);
        const tuinator::Size form_size = form_fields->preferred_size();

        std::unique_ptr<tuinator::Widget> form_widget;
        if (refresh != nullptr) {
            const int viewport_height = form_viewport_height(form_size, available_height);
            auto scroll = std::make_unique<tuinator::ScrollView>(std::move(form_fields),
                                                                 form_scroll_options(form_width, viewport_height));
            scroll_view = scroll.get();
            form_widget = std::move(scroll);
        } else if (form_size.height > available_height) {
            form_widget = std::make_unique<tuinator::ScrollView>(
                std::move(form_fields), form_scroll_options(form_width, available_height));
        } else {
            form_widget = std::move(form_fields);
        }

        auto panel_body = std::make_unique<tuinator::VBox>(tuinator::BoxOptions{.gap = 1, .padding = 1});
        if (!page.description.empty() && !page_uses_section_panels(page)) {
            auto intro = std::make_unique<tuinator::Label>(page.description, palette.muted_text(), false);
            intro->add_widget_class("muted");
            add_left_aligned(*panel_body, std::move(intro));
        }
        panel_body->add_child(std::move(form_widget));

        auto panel = std::make_unique<tuinator::Panel>(page.title, palette.panel_border(), palette.panel_title(),
                                                       tuinator::unicode_heavy_border_glyphs());
        panel->set_content(std::move(panel_body));
        root->add_child(std::move(panel));

        auto actions = std::make_unique<tuinator::HBox>(tuinator::BoxOptions{.gap = 2, .padding = 0});

        if (page.index > 1) {
            actions->add_child(std::make_unique<tuinator::Button>(
                "Back", [this]() { on_back(); }, palette.button_secondary()));
        }

        auto spacer = std::make_unique<tuinator::Label>("", palette.muted_text(), false);
        spacer->set_flex(1);
        actions->add_child(std::move(spacer));

        actions->add_child(std::make_unique<tuinator::Button>(
            page.index == page.total ? "Finish" : "Next", [this, page]() { on_next(page); }, palette.button_primary()));

        root->add_child(std::move(actions));

        auto status =
            std::make_unique<tuinator::StatusLine>(tuinator::StatusLineStyle{.background = palette.status_bar()});
        status->set_left({tuinator::StatusSegment{.kind = tuinator::StatusSegmentKind::Pill, .text = "Tab"}});
        status->set_center({tuinator::StatusSegment{.kind = tuinator::StatusSegmentKind::Text,
                                                     .text = "Navigate · Enter select"}});
        status->set_right({tuinator::StatusSegment{.kind = tuinator::StatusSegmentKind::Text, .text = "q quit"}});
        root->add_child(std::move(status));

        auto backdrop = std::make_unique<WizardBackdrop>();
        backdrop->set_content(std::move(content));
        return backdrop;
    }
};

} // namespace

WizardRunOutcome run_wizard(WizardNavigator navigator, std::map<std::string, std::string> &values,
                            const std::size_t start_index) {
    tuinator::Application app;
    configure_wizard_app(app);
    app.ensure_terminal();

    const WizardPalette &palette = wizard_palette();
    WizardPageHost host{app, palette, values, &navigator, nullptr};
    host.show_page_at(start_index);
    app.run();
    return host.outcome;
}

GroupPageResult run_group_page(const WizardGroupPage &page, std::map<std::string, std::string> &values,
                               GroupPageRefreshFn refresh) {
    tuinator::Application app;
    configure_wizard_app(app);
    app.ensure_terminal();

    const WizardPalette &palette = wizard_palette();
    WizardPageHost host{app, palette, values, nullptr, refresh};
    host.mount_page(page);
    app.run();
    return host.single_result;
}

} // namespace configwizard_ui
