#include "configwizard_ui/theme.hpp"

namespace configwizard_ui {

tuinator::Style WizardPalette::heading() const {
    tuinator::Style style = tuinator::style_fg(text);
    style.bold = true;
    return style;
}

tuinator::Style WizardPalette::label() const {
    return tuinator::style_fg(text);
}

tuinator::Style WizardPalette::muted_text() const {
    return tuinator::style_fg(muted);
}

tuinator::Style WizardPalette::accent_text() const {
    return tuinator::style_fg(accent);
}

tuinator::Style WizardPalette::panel_border() const {
    return tuinator::style_fg(border);
}

tuinator::Style WizardPalette::panel_title() const {
    tuinator::Style style = tuinator::style_fg(accent);
    style.bold = true;
    return style;
}

tuinator::Style WizardPalette::button_secondary() const {
    tuinator::Style style = tuinator::style_fg(muted);
    style.dim = true;
    return style;
}

tuinator::Style WizardPalette::button_primary() const {
    tuinator::Style style = tuinator::style_fg(success);
    style.bold = true;
    return style;
}

tuinator::Style WizardPalette::input() const {
    return tuinator::style_fg(text);
}

tuinator::Style WizardPalette::input_focused() const {
    tuinator::Style style = tuinator::style_fg(text);
    style.bold = true;
    return style;
}

tuinator::Style WizardPalette::status_bar() const {
    return tuinator::style_fg(muted);
}

tuinator::Theme WizardPalette::to_theme() const {
    tuinator::Theme theme{};
    theme.heading = heading();
    theme.label = label();
    theme.muted = muted_text();
    theme.button = button_secondary();
    theme.button_focused = button_primary();
    theme.text_input = input();
    theme.text_input_focused = input_focused();
    theme.border = panel_border();
    theme.accent = accent_text();
    theme.success = button_primary();
    theme.danger = tuinator::style_fg(danger);
    theme.glyphs = tuinator::border_glyphs_for(tuinator::BorderStyle::Heavy);
    return theme;
}

std::filesystem::path WizardPalette::stylesheet_path() const {
    return std::filesystem::path(__FILE__).parent_path().parent_path() / "assets" / "wizard.theme";
}

const WizardPalette &wizard_palette() {
    static const WizardPalette palette{};
    return palette;
}

} // namespace configwizard_ui
