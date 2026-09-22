use std::collections::VecDeque;

use configwizard_core::{load_questionnaire_json, AnswerSources, SessionOptions};
use configwizard_ui_cli::{Frontend, FrontendAction, PromptResult, RunOptions, WizardRunner};
use indexmap::IndexMap;

const BASIC_CPP: &str = include_str!("../../../configwizard-core/tests/basic_cpp.json");

struct ScriptedFrontend {
    prompts: VecDeque<String>,
    outputs: Vec<String>,
}

impl ScriptedFrontend {
    fn new(inputs: &[&str]) -> Self {
        Self {
            prompts: inputs.iter().map(|value| value.to_string()).collect(),
            outputs: Vec::new(),
        }
    }
}

impl Frontend for ScriptedFrontend {
    fn prompt(&mut self, _text: &str, _sensitive: bool) -> Option<PromptResult> {
        let value = self.prompts.pop_front().unwrap_or_default();
        Some(PromptResult {
            action: FrontendAction::Submit,
            value,
        })
    }

    fn write_line(&mut self, text: &str) {
        self.outputs.push(text.to_string());
    }

    fn begin_group(&mut self, _name: &str, _index: usize, _total: usize) {}
    fn end_group(&mut self) {}
}

#[test]
fn runs_basic_questionnaire_with_defaults() {
    let questionnaire = load_questionnaire_json(BASIC_CPP).expect("schema");
    let mut frontend = ScriptedFrontend::new(&["", "", "", "", ""]);

    let answers = WizardRunner::run(
        questionnaire,
        RunOptions::default(),
        &mut frontend,
    )
    .expect("wizard should finish");

    assert_eq!(answers.get("project_name").map(String::as_str), Some("My App"));
    assert_eq!(answers.get("namespace").map(String::as_str), Some("sample"));
}

#[test]
fn skips_locked_cli_keys() {
    let questionnaire = load_questionnaire_json(BASIC_CPP).expect("schema");
    let mut frontend = ScriptedFrontend::new(&["", "", ""]);

    let answers = WizardRunner::run(
        questionnaire,
        RunOptions {
            sources: AnswerSources {
                cli: IndexMap::from([
                    ("project_name".into(), "From CLI".into()),
                    ("namespace".into(), "cli_ns".into()),
                ]),
                ..Default::default()
            },
            session_options: SessionOptions::default(),
        },
        &mut frontend,
    )
    .expect("wizard should finish");

    assert_eq!(answers.get("project_name").map(String::as_str), Some("From CLI"));
    assert_eq!(answers.get("namespace").map(String::as_str), Some("cli_ns"));
}

#[test]
fn non_interactive_uses_bootstrap() {
    let questionnaire = load_questionnaire_json(BASIC_CPP).expect("schema");

    let answers = WizardRunner::run(
        questionnaire,
        RunOptions {
            sources: AnswerSources::default(),
            session_options: SessionOptions {
                non_interactive: true,
                strict: false,
            },
        },
        &mut ScriptedFrontend::new(&[]),
    )
    .expect("non-interactive run");

    assert!(answers.contains_key("project_name"));
}
