#pragma once

#include <tuinator/render/theme.hpp>

#include <filesystem>

namespace configwizard_ui {

struct WizardPalette {
    tuinator::Rgb border{0x41, 0x48, 0x68};
    tuinator::Rgb text{0xc0, 0xca, 0xf5};
    tuinator::Rgb muted{0x73, 0x7a, 0xa2};
    tuinator::Rgb accent{0x7a, 0xa2, 0xf7};
    tuinator::Rgb success{0x9e, 0xce, 0x6a};
    tuinator::Rgb danger{0xf7, 0x76, 0x8e};
    tuinator::Rgb warning{0xe0, 0xaf, 0x68};

    tuinator::Style heading() const;
    tuinator::Style label() const;
    tuinator::Style muted_text() const;
    tuinator::Style accent_text() const;
    tuinator::Style panel_border() const;
    tuinator::Style panel_title() const;
    tuinator::Style button_secondary() const;
    tuinator::Style button_primary() const;
    tuinator::Style input() const;
    tuinator::Style input_focused() const;
    tuinator::Style status_bar() const;

    tuinator::Theme to_theme() const;
    std::filesystem::path stylesheet_path() const;
};

const WizardPalette &wizard_palette();

} // namespace configwizard_ui
