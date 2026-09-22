#pragma once

#include "configwizard_ui/frontend.hpp"

#include <iostream>
#include <optional>

namespace configwizard_ui {

/// Minimal stdin/stdout frontend (Tempify `PlainCliFrontend` equivalent).
class PlainCliFrontend final : public IWizardFrontend {
  public:
    std::optional<PromptResult> prompt(const std::string &text, bool sensitive = false) override;
    void write_line(const std::string &text) override;
    void begin_group(const std::string &name, std::size_t index, std::size_t total) override;
    void end_group() override;

  private:
    std::ostream &output_ = std::cout;
    std::istream &input_ = std::cin;
};

} // namespace configwizard_ui
