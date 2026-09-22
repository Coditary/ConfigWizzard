#include "demo_loader.hpp"

#include "configwizard_ui/tuinator_wizard.hpp"

#include <filesystem>
#include <iostream>
#include <map>
#include <string>

namespace {

std::filesystem::path default_schema_path() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "examples" /
           "full_demo.json";
}

void print_usage() {
    std::cout << "ConfigWizard TUI demo\n\n"
              << "Usage:\n"
              << "  configwizard-tui-demo [schema.json]\n\n"
              << "Defaults to configwizard-ui/examples/full_demo.json\n"
              << "Tab between fields | Back/Next buttons | q to quit\n";
}

void clear_page_values(const configwizard_ui::WizardGroupPage &page,
                       std::map<std::string, std::string> &values) {
    for (const configwizard_ui::WizardField &field : page.all_fields()) {
        values.erase(field.key);
    }
}

void seed_page_values(const configwizard_ui::WizardGroupPage &page,
                      std::map<std::string, std::string> &values) {
    for (const configwizard_ui::WizardField &field : page.all_fields()) {
        if (!field.default_value.empty()) {
            values[field.key] = field.default_value;
        }
    }
}

} // namespace

int main(int argc, char **argv) {
    if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        print_usage();
        return 0;
    }

    const std::filesystem::path schema_path =
        argc > 1 ? std::filesystem::path(argv[1]) : default_schema_path();

    configwizard_ui::demo::DemoSession session;
    try {
        session = configwizard_ui::demo::open_demo_session(schema_path);
    } catch (const std::exception &error) {
        std::cerr << "failed to load schema (" << schema_path << "): " << error.what() << '\n';
        return 1;
    }

    std::cout << session.title << '\n';
    if (!session.description.empty()) {
        std::cout << session.description << '\n';
    }
    std::cout << "Schema: " << schema_path << "\n\n";

    std::map<std::string, std::string> values;

    configwizard_ui::WizardNavigator navigator{
        .page_count =
            [&](const std::map<std::string, std::string> &current_values) {
                configwizard_ui::demo::refresh_visible_groups(session, current_values);
                return session.groups.size();
            },
        .load_page =
            [&](const std::size_t page_index, const std::map<std::string, std::string> &current_values) {
                return configwizard_ui::demo::render_group_page(session, page_index, current_values);
            },
        .seed_page =
            [](const configwizard_ui::WizardGroupPage &page, std::map<std::string, std::string> &current_values) {
                seed_page_values(page, current_values);
            },
        .on_navigate =
            [&](const configwizard_ui::GroupPageAction action, const std::size_t from_index,
                const std::size_t to_index, std::map<std::string, std::string> &current_values) {
                if (action != configwizard_ui::GroupPageAction::Back) {
                    return;
                }

                const configwizard_ui::WizardGroupPage leaving_page =
                    configwizard_ui::demo::render_group_page(session, from_index, current_values);
                clear_page_values(leaving_page, current_values);

                const configwizard_ui::WizardGroupPage target_page =
                    configwizard_ui::demo::render_group_page(session, to_index, current_values);
                clear_page_values(target_page, current_values);
            },
    };

    const configwizard_ui::WizardRunOutcome outcome = configwizard_ui::run_wizard(navigator, values);

    configwizard_ui::demo::close_demo_session(session);

    if (outcome == configwizard_ui::WizardRunOutcome::Aborted) {
        std::cerr << "aborted by user\n";
        return 1;
    }

    std::cout << "\nCollected answers:\n";
    for (const auto &[key, value] : values) {
        std::cout << "  " << key << ": " << value << '\n';
    }

    return 0;
}
