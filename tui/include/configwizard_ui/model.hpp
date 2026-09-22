#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace configwizard_ui {

struct ChoiceView {
    std::string value;
    std::string label;
};

struct WizardField {
    std::string key;
    std::string prompt;
    std::string help;
    std::string placeholder;
    std::string default_value;
    std::string type;
    std::vector<ChoiceView> choices;
    bool sensitive = false;
    bool optional = false;
};

struct WizardSection {
    std::string id;
    std::string title;
    std::string description;
    std::vector<WizardField> fields;
};

struct WizardGroupPage {
    std::string title;
    std::string description;
    std::size_t index = 1;
    std::size_t total = 1;
    std::vector<WizardSection> sections;

    std::vector<WizardField> all_fields() const {
        std::vector<WizardField> fields;
        for (const WizardSection &section : sections) {
            fields.insert(fields.end(), section.fields.begin(), section.fields.end());
        }
        return fields;
    }
};

enum class GroupPageAction {
    Next,
    Back,
    Quit,
    Jump,
};

struct GroupPageResult {
    GroupPageAction action = GroupPageAction::Quit;
    /// Zero-based group index when action == Jump.
    std::size_t jump_index = 0;
};

} // namespace configwizard_ui
