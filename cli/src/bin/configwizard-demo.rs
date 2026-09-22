use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

use configwizard_core::{
    load_answers_file, load_questionnaire_file, save_answers_json, AnswerSources, SessionOptions,
};
use configwizard_ui_cli::{PlainCliFrontend, RunError, RunOptions, WizardCliFrontend, WizardRunner};

fn usage() -> &'static str {
    "ConfigWizard demo — interactive questionnaire runner\n\
\n\
Usage:\n\
  configwizard-demo [options]\n\
\n\
Options:\n\
  --plain              Use minimal CLI (no page chrome)\n\
  --wizard             Use wizard-style page headers (default)\n\
  --schema <path>      Questionnaire JSON/YAML (default: examples/full_demo.json)\n\
  --answers <path>     Pre-fill from answers JSON file\n\
  --non-interactive    Apply defaults/sources only, then commit\n\
  --strict             Reject unknown keys in answer sources\n\
  -h, --help           Show this help\n\
\n\
Examples:\n\
  configwizard-demo\n\
  configwizard-demo --plain\n\
  configwizard-demo --schema ../examples/full_demo.json\n\
  configwizard-demo --answers ./my-answers.json --non-interactive\n"
}

fn default_schema_path() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../examples/full_demo.json")
}

fn resolve_schema_path(argument: Option<&str>) -> Result<PathBuf, String> {
    let path = match argument {
        Some(value) => PathBuf::from(value),
        None => default_schema_path(),
    };

    if path.is_file() {
        return Ok(path);
    }

    Err(format!("schema not found: {}", path.display()))
}

fn parse_args() -> Result<DemoArgs, String> {
    let mut style = DemoStyle::Wizard;
    let mut schema = None;
    let mut answers = None;
    let mut non_interactive = false;
    let mut strict = false;

    let args: Vec<String> = env::args().skip(1).collect();
    let mut index = 0;
    while index < args.len() {
        match args[index].as_str() {
            "--plain" => style = DemoStyle::Plain,
            "--wizard" => style = DemoStyle::Wizard,
            "--non-interactive" => non_interactive = true,
            "--strict" => strict = true,
            "-h" | "--help" => return Err(String::new()),
            "--schema" => {
                index += 1;
                schema = Some(
                    args.get(index)
                        .ok_or_else(|| "--schema requires a path".to_string())?
                        .clone(),
                );
            }
            "--answers" => {
                index += 1;
                answers = Some(
                    args.get(index)
                        .ok_or_else(|| "--answers requires a path".to_string())?
                        .clone(),
                );
            }
            other => return Err(format!("unknown argument: {}", other)),
        }
        index += 1;
    }

    Ok(DemoArgs {
        style,
        schema: resolve_schema_path(schema.as_deref())?,
        answers,
        non_interactive,
        strict,
    })
}

enum DemoStyle {
    Plain,
    Wizard,
}

struct DemoArgs {
    style: DemoStyle,
    schema: PathBuf,
    answers: Option<String>,
    non_interactive: bool,
    strict: bool,
}

fn load_schema(path: &Path) -> Result<configwizard_core::Questionnaire, String> {
    match path.extension().and_then(|ext| ext.to_str()) {
        Some("yaml") | Some("yml") => configwizard_core::load_questionnaire_yaml(
            &fs::read_to_string(path).map_err(|error| error.to_string())?,
        )
        .map_err(|error| error.to_string()),
        _ => load_questionnaire_file(path).map_err(|error| error.to_string()),
    }
}

fn main() -> ExitCode {
    let demo_args = match parse_args() {
        Ok(args) => args,
        Err(message) => {
            if message.is_empty() {
                print!("{}", usage());
                return ExitCode::SUCCESS;
            }
            eprintln!("error: {}", message);
            eprintln!();
            eprintln!("{}", usage());
            return ExitCode::from(2);
        }
    };

    let questionnaire = match load_schema(&demo_args.schema) {
        Ok(schema) => schema,
        Err(error) => {
            eprintln!("failed to load schema ({}): {}", demo_args.schema.display(), error);
            return ExitCode::from(1);
        }
    };

    let mut sources = AnswerSources::default();
    if let Some(path) = &demo_args.answers {
        match load_answers_file(path, &questionnaire, demo_args.strict) {
            Ok(file_answers) => sources.file = file_answers,
            Err(error) => {
                eprintln!("failed to load answers ({}): {}", path, error);
                return ExitCode::from(1);
            }
        }
    }

    let options = RunOptions {
        sources,
        session_options: SessionOptions {
            non_interactive: demo_args.non_interactive,
            strict: demo_args.strict,
        },
    };

    let result = match demo_args.style {
        DemoStyle::Plain => {
            let mut frontend = PlainCliFrontend::new();
            WizardRunner::run(questionnaire, options, &mut frontend)
        }
        DemoStyle::Wizard => {
            let mut frontend = WizardCliFrontend::new();
            WizardRunner::run(questionnaire, options, &mut frontend)
        }
    };

    match result {
        Ok(answers) => {
            match save_answers_json(&answers) {
                Ok(json) => println!("{}", json),
                Err(error) => {
                    eprintln!("failed to serialize answers: {}", error);
                    return ExitCode::from(1);
                }
            }
            ExitCode::SUCCESS
        }
        Err(RunError::Aborted) => {
            eprintln!("aborted by user");
            ExitCode::from(1)
        }
        Err(RunError::Core(error)) => {
            eprintln!("error: {}", error);
            ExitCode::from(1)
        }
    }
}
