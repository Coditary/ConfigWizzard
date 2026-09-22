#include "configwizard_ui/plain_frontend.hpp"

#include <string>

namespace configwizard_ui {

std::optional<PromptResult> PlainCliFrontend::prompt(const std::string &text, const bool sensitive) {
    output_ << text;
    output_.flush();

    if (sensitive) {
        // Platform-specific echo hiding can be added later.
    }

    std::string line;
    if (!std::getline(input_, line)) {
        return std::nullopt;
    }

    if (line == ":back") {
        return PromptResult{.action = FrontendAction::Back, .value = {}};
    }
    if (line == ":quit") {
        return PromptResult{.action = FrontendAction::Quit, .value = {}};
    }

    return PromptResult{.action = FrontendAction::Submit, .value = line};
}

void PlainCliFrontend::write_line(const std::string &text) {
    output_ << text << '\n';
    output_.flush();
}

void PlainCliFrontend::begin_group(const std::string &name, const std::size_t, const std::size_t) {
    if (!name.empty()) {
        write_line("[" + name + "]");
    }
}

void PlainCliFrontend::end_group() {}

} // namespace configwizard_ui
