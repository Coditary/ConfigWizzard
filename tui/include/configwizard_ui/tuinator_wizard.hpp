#pragma once

#include "configwizard_ui/model.hpp"

#include <functional>
#include <map>
#include <string>

namespace configwizard_ui {

/// Re-render the current group from session state after inline answer changes.
using GroupPageRefreshFn =
    std::function<WizardGroupPage(const std::map<std::string, std::string> &values)>;

using WizardPageLoadFn =
    std::function<WizardGroupPage(std::size_t page_index, const std::map<std::string, std::string> &values)>;

using WizardPageCountFn =
    std::function<std::size_t(const std::map<std::string, std::string> &values)>;

struct WizardNavigator {
    WizardPageCountFn page_count;
    WizardPageLoadFn load_page;
    std::function<bool(const WizardGroupPage &page)> should_show_page =
        [](const WizardGroupPage &page) { return !page.sections.empty(); };
    std::function<void(const WizardGroupPage &page, std::map<std::string, std::string> &values)> seed_page;
    std::function<void(GroupPageAction action, std::size_t from_index, std::size_t to_index,
                       std::map<std::string, std::string> &values)>
        on_navigate;
};

enum class WizardRunOutcome {
    Completed,
    Aborted,
};

/// Runs the full wizard in one terminal session (no flicker between pages).
WizardRunOutcome run_wizard(WizardNavigator navigator, std::map<std::string, std::string> &values,
                            std::size_t start_index = 0);

/// Renders one wizard group page with Tuinator and returns collected field values.
/// When `refresh` is set, bool/multiselect toggles re-query the session and
/// expand or collapse dependent fields on the same page.
GroupPageResult run_group_page(const WizardGroupPage &page, std::map<std::string, std::string> &values,
                               GroupPageRefreshFn refresh = nullptr);

} // namespace configwizard_ui
