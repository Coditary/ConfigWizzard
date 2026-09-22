#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace configwizard_ui {

enum class FrontendAction {
    Submit,
    Back,
    Quit,
};

struct PromptResult {
    FrontendAction action = FrontendAction::Submit;
    std::string value;
};

class IWizardFrontend {
  public:
    virtual ~IWizardFrontend() = default;

    virtual std::optional<PromptResult> prompt(const std::string &text, bool sensitive = false) = 0;
    virtual void write_line(const std::string &text) = 0;
    virtual void begin_group(const std::string &name, std::size_t index, std::size_t total) = 0;
    virtual void end_group() = 0;
};

} // namespace configwizard_ui
