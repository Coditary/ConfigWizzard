#include "demo_loader.hpp"

#include <configwizard_ffi.h>

#include <nlohmann/json.hpp>

#include <sstream>
#include <stdexcept>

namespace configwizard_ui::demo {

namespace {

std::string ffi_error_message() {
    char *error = cw_last_error();
    if (error == nullptr) {
        return "unknown FFI error";
    }
    const std::string message(error);
    cw_string_free(error);
    return message;
}

std::string values_to_json(const std::map<std::string, std::string> &values) {
    nlohmann::json object = nlohmann::json::object();
    for (const auto &[key, value] : values) {
        object[key] = value;
    }
    return object.dump();
}

WizardField parse_field(const nlohmann::json &field) {
    WizardField parsed;
    parsed.key = field.value("key", "");
    parsed.type = field.value("type", "string");
    parsed.prompt = field.value("prompt", parsed.key);
    parsed.help = field.value("help", "");
    parsed.placeholder = field.value("placeholder", "");
    parsed.default_value = field.value("current", "");
    parsed.optional = field.value("optional", false);
    parsed.sensitive = field.value("sensitive", false);

    if (field.contains("choices") && field["choices"].is_array()) {
        for (const auto &choice : field["choices"]) {
            if (!choice.is_object()) {
                continue;
            }
            parsed.choices.push_back(ChoiceView{
                .value = choice.value("value", ""),
                .label = choice.value("label", choice.value("value", "")),
            });
        }
    }

    return parsed;
}

} // namespace

DemoSession open_demo_session(const std::filesystem::path &schema_path) {
    CwSession *handle = cw_session_open(schema_path.string().c_str());
    if (handle == nullptr) {
        throw std::runtime_error(ffi_error_message());
    }

    char *metadata_json = cw_session_metadata_json(handle);
    if (metadata_json == nullptr) {
        cw_session_close(handle);
        throw std::runtime_error(ffi_error_message());
    }

    DemoSession session;
    session.handle = handle;

    try {
        const nlohmann::json metadata = nlohmann::json::parse(metadata_json);
        session.title = metadata.value("title", "ConfigWizard");
        session.description = metadata.value("description", "");
        if (metadata.contains("groups") && metadata["groups"].is_array()) {
            for (const auto &group : metadata["groups"]) {
                if (group.is_string()) {
                    session.groups.push_back(group.get<std::string>());
                }
            }
        }
    } catch (const std::exception &error) {
        cw_string_free(metadata_json);
        cw_session_close(handle);
        throw std::runtime_error(std::string("failed to parse session metadata: ") + error.what());
    }

    cw_string_free(metadata_json);

    if (session.groups.empty()) {
        cw_session_close(handle);
        session.handle = nullptr;
        throw std::runtime_error("schema produced no wizard groups");
    }

    return session;
}

void close_demo_session(DemoSession &session) {
    if (session.handle != nullptr) {
        cw_session_close(session.handle);
        session.handle = nullptr;
    }
}

void sync_session_values(DemoSession &session, const std::map<std::string, std::string> &values) {
    if (session.handle == nullptr) {
        throw std::runtime_error("session is not open");
    }

    const std::string payload = values_to_json(values);
    if (!cw_session_sync_values(session.handle, payload.c_str())) {
        throw std::runtime_error(ffi_error_message());
    }
}

void refresh_visible_groups(DemoSession &session, const std::map<std::string, std::string> &values) {
    if (session.handle == nullptr) {
        throw std::runtime_error("session is not open");
    }

    sync_session_values(session, values);

    char *metadata_json = cw_session_metadata_json(session.handle);
    if (metadata_json == nullptr) {
        throw std::runtime_error(ffi_error_message());
    }

    try {
        const nlohmann::json metadata = nlohmann::json::parse(metadata_json);
        session.groups.clear();
        if (metadata.contains("groups") && metadata["groups"].is_array()) {
            for (const auto &group : metadata["groups"]) {
                if (group.is_string()) {
                    session.groups.push_back(group.get<std::string>());
                }
            }
        }
    } catch (const std::exception &error) {
        cw_string_free(metadata_json);
        throw std::runtime_error(std::string("failed to parse session metadata: ") + error.what());
    }

    cw_string_free(metadata_json);

    if (session.groups.empty()) {
        throw std::runtime_error("schema produced no visible wizard groups");
    }
}

WizardGroupPage render_group_page(DemoSession &session, const std::size_t group_index,
                                  const std::map<std::string, std::string> &values) {
    if (session.handle == nullptr) {
        throw std::runtime_error("session is not open");
    }
    if (group_index >= session.groups.size()) {
        throw std::runtime_error("group index out of range");
    }

    refresh_visible_groups(session, values);

    const std::string &group_id = session.groups[group_index];
    char *page_json = cw_session_render_group_json(session.handle, group_id.c_str(), group_index + 1,
                                                   session.groups.size());
    if (page_json == nullptr) {
        throw std::runtime_error(ffi_error_message());
    }

    WizardGroupPage page;
    try {
        const nlohmann::json payload = nlohmann::json::parse(page_json);
        page.title = payload.value("title", group_id);
        page.description = payload.value("description", "");
        page.index = payload.value("index", group_index + 1);
        page.total = payload.value("total", session.groups.size());

        if (payload.contains("sections") && payload["sections"].is_array()) {
            for (const auto &section : payload["sections"]) {
                if (!section.is_object()) {
                    continue;
                }
                WizardSection parsed_section;
                parsed_section.id = section.value("id", "");
                parsed_section.title = section.value("title", "");
                parsed_section.description = section.value("description", "");
                if (section.contains("fields") && section["fields"].is_array()) {
                    for (const auto &field : section["fields"]) {
                        if (field.is_object()) {
                            parsed_section.fields.push_back(parse_field(field));
                        }
                    }
                }
                if (!parsed_section.fields.empty()) {
                    page.sections.push_back(std::move(parsed_section));
                }
            }
        }
    } catch (const std::exception &error) {
        cw_string_free(page_json);
        throw std::runtime_error(std::string("failed to parse group page: ") + error.what());
    }

    cw_string_free(page_json);
    return page;
}

} // namespace configwizard_ui::demo
