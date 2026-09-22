#pragma once

#include "configwizard_ui/model.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct CwSession;

namespace configwizard_ui::demo {

struct DemoSession {
    CwSession *handle = nullptr;
    std::string title;
    std::string description;
    std::vector<std::string> groups;
};

DemoSession open_demo_session(const std::filesystem::path &schema_path);
void close_demo_session(DemoSession &session);

/// Apply collected answers to the core session before rendering a page.
void sync_session_values(DemoSession &session, const std::map<std::string, std::string> &values);

/// Re-query visible wizard groups from the session after answer changes.
void refresh_visible_groups(DemoSession &session, const std::map<std::string, std::string> &values);

/// Build one group page using core visibility and filtered choices.
WizardGroupPage render_group_page(DemoSession &session, std::size_t group_index,
                                  const std::map<std::string, std::string> &values);

} // namespace configwizard_ui::demo
