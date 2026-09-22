//! C FFI for driving the Tuinator TUI from `configwizard-core::ConfigSession`.

use std::collections::HashMap;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::path::Path;
use std::ptr;
use std::sync::Mutex;

use configwizard_core::{
    load_questionnaire_file, ConfigSession, Question, QuestionType, SessionEvent,
};
use serde::Serialize;

static LAST_ERROR: Mutex<Option<String>> = Mutex::new(None);

fn set_error(message: impl Into<String>) {
    *LAST_ERROR.lock().expect("error mutex poisoned") = Some(message.into());
}

fn take_error_string() -> Option<CString> {
    LAST_ERROR
        .lock()
        .expect("error mutex poisoned")
        .take()
        .and_then(|message| CString::new(message).ok())
}

fn to_c_string(value: String) -> *mut c_char {
    CString::new(value)
        .map(|string| string.into_raw())
        .unwrap_or_else(|_| {
            set_error("string contains interior NUL byte");
            ptr::null_mut()
        })
}

#[derive(Serialize)]
struct TuiChoice {
    value: String,
    label: String,
}

#[derive(Serialize)]
struct TuiField {
    key: String,
    #[serde(rename = "type")]
    field_type: String,
    prompt: String,
    help: String,
    placeholder: String,
    current: String,
    optional: bool,
    sensitive: bool,
    choices: Vec<TuiChoice>,
}

#[derive(Serialize)]
struct TuiSection {
    id: String,
    title: String,
    description: String,
    fields: Vec<TuiField>,
}

#[derive(Serialize)]
struct TuiGroupPage {
    title: String,
    description: String,
    index: usize,
    total: usize,
    sections: Vec<TuiSection>,
}

#[derive(Serialize)]
struct TuiMetadata {
    title: String,
    description: String,
    groups: Vec<String>,
}

pub struct CwSession {
    session: ConfigSession,
}

fn question_type_name(question_type: QuestionType) -> &'static str {
    match question_type {
        QuestionType::String => "string",
        QuestionType::Bool => "bool",
        QuestionType::Choice => "choice",
        QuestionType::Multiselect => "multiselect",
        QuestionType::Integer => "integer",
        QuestionType::Number => "number",
        QuestionType::Path => "path",
        QuestionType::File => "file",
    }
}

fn choice_label(question: &Question, value: &str) -> String {
    question
        .choices
        .iter()
        .find(|choice| choice.value == value)
        .map(|choice| choice.label.clone())
        .unwrap_or_else(|| value.to_string())
}

fn build_field(
    question: &Question,
    current: &str,
    available: &[String],
) -> TuiField {
    let choices = available
        .iter()
        .map(|value| TuiChoice {
            value: value.clone(),
            label: choice_label(question, value),
        })
        .collect();

    TuiField {
        key: question.key.clone(),
        field_type: question_type_name(question.question_type).to_string(),
        prompt: question.prompt.clone(),
        help: question.help.clone(),
        placeholder: question.placeholder.clone().unwrap_or_default(),
        current: current.to_string(),
        optional: question.optional,
        sensitive: question.sensitive,
        choices,
    }
}

fn section_metadata(
    group: Option<&configwizard_core::GroupDefinition>,
    section_id: &str,
) -> (String, String) {
    if section_id.is_empty() {
        return (String::new(), String::new());
    }

    if let Some(group) = group {
        if let Some(section) = group.sections.iter().find(|section| section.id == section_id) {
            let title = section
                .title
                .clone()
                .filter(|title| !title.is_empty())
                .unwrap_or_else(|| section_id.to_string());
            return (title, section.description.clone().unwrap_or_default());
        }
    }

    (section_id.to_string(), String::new())
}

fn build_group_page(session: &ConfigSession, group_id: &str, index: usize, total: usize) -> Result<TuiGroupPage, String> {
    let questionnaire = session.questionnaire();
    let patch = session.snapshot().map_err(|error| error.to_string())?;

    let group = questionnaire.groups.iter().find(|group| group.id == group_id);
    let title = group
        .and_then(|group| group.title.clone())
        .filter(|title| !title.is_empty())
        .unwrap_or_else(|| group_id.to_string());
    let description = group
        .and_then(|group| group.description.clone())
        .unwrap_or_default();

    let mut sections = Vec::new();
    for key in questionnaire.ordered_question_keys() {
        let question = questionnaire
            .question(&key)
            .ok_or_else(|| format!("unknown question key '{}'", key))?;

        if question.group != group_id || question.hidden {
            continue;
        }
        if patch.visibility.get(&key) != Some(&true) {
            continue;
        }
        if patch.interactive.get(&key) != Some(&true) {
            continue;
        }

        let available = patch
            .available_choices
            .get(&key)
            .cloned()
            .unwrap_or_default();
        let current = patch
            .display_values
            .get(&key)
            .map(String::as_str)
            .unwrap_or("");

        let section_id = question.section.clone().unwrap_or_default();
        let needs_new_section = sections
            .last()
            .map(|section: &TuiSection| section.id != section_id)
            .unwrap_or(true);
        if needs_new_section {
            let (section_title, section_description) = section_metadata(group, &section_id);
            sections.push(TuiSection {
                id: section_id,
                title: section_title,
                description: section_description,
                fields: Vec::new(),
            });
        }

        sections
            .last_mut()
            .expect("section bucket")
            .fields
            .push(build_field(question, current, &available));
    }

    Ok(TuiGroupPage {
        title,
        description,
        index,
        total,
        sections,
    })
}

fn apply_values_json(session: &mut ConfigSession, values_json: &str) -> Result<(), String> {
    let values: HashMap<String, String> =
        serde_json::from_str(values_json).map_err(|error| error.to_string())?;

    for (key, value) in values {
        session
            .apply(SessionEvent::SetValue { key, value })
            .map_err(|error| error.to_string())?;
    }

    Ok(())
}

fn metadata_for(session: &ConfigSession) -> Result<TuiMetadata, String> {
    let questionnaire = session.questionnaire();
    let patch = session.snapshot().map_err(|error| error.to_string())?;

    let groups = patch.group_order.clone();

    Ok(TuiMetadata {
        title: questionnaire
            .title
            .clone()
            .unwrap_or_else(|| questionnaire.id.clone()),
        description: questionnaire.description.clone().unwrap_or_default(),
        groups,
    })
}

#[no_mangle]
pub extern "C" fn cw_last_error() -> *mut c_char {
    match take_error_string() {
        Some(message) => message.into_raw(),
        None => ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn cw_string_free(value: *mut c_char) {
    if !value.is_null() {
        unsafe {
            drop(CString::from_raw(value));
        }
    }
}

#[no_mangle]
pub extern "C" fn cw_session_open(schema_path: *const c_char) -> *mut CwSession {
    if schema_path.is_null() {
        set_error("schema path is null");
        return ptr::null_mut();
    }

    let path = unsafe { CStr::from_ptr(schema_path) };
    let path = match path.to_str() {
        Ok(value) => value,
        Err(error) => {
            set_error(format!("invalid schema path: {}", error));
            return ptr::null_mut();
        }
    };

    let questionnaire = match load_questionnaire_file(Path::new(path)) {
        Ok(questionnaire) => questionnaire,
        Err(error) => {
            set_error(error.to_string());
            return ptr::null_mut();
        }
    };

    match ConfigSession::new(questionnaire) {
        Ok(session) => Box::into_raw(Box::new(CwSession { session })),
        Err(error) => {
            set_error(error.to_string());
            ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "C" fn cw_session_close(session: *mut CwSession) {
    if session.is_null() {
        return;
    }
    unsafe {
        drop(Box::from_raw(session));
    }
}

#[no_mangle]
pub extern "C" fn cw_session_sync_values(session: *mut CwSession, values_json: *const c_char) -> i32 {
    if session.is_null() {
        set_error("session is null");
        return 0;
    }
    if values_json.is_null() {
        return 1;
    }

    let values_json = unsafe { CStr::from_ptr(values_json) };
    let values_json = match values_json.to_str() {
        Ok(value) => value,
        Err(error) => {
            set_error(format!("invalid values json: {}", error));
            return 0;
        }
    };

    let session = unsafe { &mut *session };
    match apply_values_json(&mut session.session, values_json) {
        Ok(()) => 1,
        Err(error) => {
            set_error(error);
            0
        }
    }
}

#[no_mangle]
pub extern "C" fn cw_session_metadata_json(session: *mut CwSession) -> *mut c_char {
    if session.is_null() {
        set_error("session is null");
        return ptr::null_mut();
    }

    let session = unsafe { &*session };
    match metadata_for(&session.session) {
        Ok(metadata) => match serde_json::to_string(&metadata) {
            Ok(json) => to_c_string(json),
            Err(error) => {
                set_error(error.to_string());
                ptr::null_mut()
            }
        },
        Err(error) => {
            set_error(error);
            ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "C" fn cw_session_render_group_json(
    session: *mut CwSession,
    group_id: *const c_char,
    index: usize,
    total: usize,
) -> *mut c_char {
    if session.is_null() {
        set_error("session is null");
        return ptr::null_mut();
    }
    if group_id.is_null() {
        set_error("group id is null");
        return ptr::null_mut();
    }

    let group_id = unsafe { CStr::from_ptr(group_id) };
    let group_id = match group_id.to_str() {
        Ok(value) => value,
        Err(error) => {
            set_error(format!("invalid group id: {}", error));
            return ptr::null_mut();
        }
    };

    let session = unsafe { &*session };
    match build_group_page(&session.session, group_id, index, total) {
        Ok(page) => match serde_json::to_string(&page) {
            Ok(json) => to_c_string(json),
            Err(error) => {
                set_error(error.to_string());
                ptr::null_mut()
            }
        },
        Err(error) => {
            set_error(error);
            ptr::null_mut()
        }
    }
}

#[cfg(test)]
mod tests {
    use std::path::PathBuf;

    use super::*;

    fn scaffold_schema_path() -> PathBuf {
        PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join("../examples/project_scaffold_demo.json")
    }

    #[test]
    fn build_tool_field_hides_when_disabled() {
        let questionnaire =
            load_questionnaire_file(scaffold_schema_path()).expect("load scaffold schema");
        let mut session = ConfigSession::new(questionnaire).expect("session");
        session
            .apply(SessionEvent::SetValue {
                key: "use_build_tool".into(),
                value: "false".into(),
            })
            .expect("disable build tool");

        let page = build_group_page(&session, "Build", 2, 4).expect("build page");
        assert_eq!(page.sections.len(), 1);
        assert_eq!(page.sections[0].fields.len(), 1);
        assert_eq!(page.sections[0].fields[0].key, "use_build_tool");
    }

    #[test]
    fn quality_group_hides_when_disabled() {
        let questionnaire =
            load_questionnaire_file(scaffold_schema_path()).expect("load scaffold schema");
        let mut session = ConfigSession::new(questionnaire).expect("session");
        session
            .apply(SessionEvent::SetValue {
                key: "use_qa".into(),
                value: "false".into(),
            })
            .expect("disable qa");

        let metadata = metadata_for(&session).expect("metadata");
        assert_eq!(
            metadata.groups,
            vec![
                "Project".to_string(),
                "Build".to_string(),
                "VCS".to_string(),
            ]
        );
    }

    #[test]
    fn rust_language_filters_build_tools() {
        let questionnaire =
            load_questionnaire_file(scaffold_schema_path()).expect("load scaffold schema");
        let mut session = ConfigSession::new(questionnaire).expect("session");
        session
            .apply(SessionEvent::SetValue {
                key: "language".into(),
                value: "rust".into(),
            })
            .expect("set language");

        let page = build_group_page(&session, "Build", 2, 4).expect("build page");
        let build_tool = page
            .sections
            .iter()
            .flat_map(|section| section.fields.iter())
            .find(|field| field.key == "build_tool")
            .expect("build_tool field");

        let values = build_tool
            .choices
            .iter()
            .map(|choice| choice.value.clone())
            .collect::<Vec<_>>();
        assert_eq!(values, vec!["cargo".to_string()]);
    }
}
