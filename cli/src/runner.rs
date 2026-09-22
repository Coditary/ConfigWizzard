use std::collections::{HashMap, HashSet};

use configwizard_core::{
    AnswerSources, Answers, ConfigSession, ConfigWizardError, Question, Questionnaire,
    SessionEvent, SessionOptions, SessionPhase,
};

use crate::frontend::{Frontend, FrontendAction};
use crate::prompt::{format_review_lines, normalize_bool_input, prompt_text};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RunOptions {
    pub sources: AnswerSources,
    pub session_options: SessionOptions,
}

impl Default for RunOptions {
    fn default() -> Self {
        Self {
            sources: AnswerSources::default(),
            session_options: SessionOptions::default(),
        }
    }
}

#[derive(Debug, PartialEq, Eq)]
pub enum RunError {
    Aborted,
    Core(ConfigWizardError),
}

impl From<ConfigWizardError> for RunError {
    fn from(error: ConfigWizardError) -> Self {
        Self::Core(error)
    }
}

pub struct WizardRunner;

impl WizardRunner {
    pub fn run(
        questionnaire: Questionnaire,
        options: RunOptions,
        frontend: &mut impl Frontend,
    ) -> std::result::Result<Answers, RunError> {
        let locked_keys = locked_source_keys(&options.sources);

        if options.session_options.non_interactive {
            let mut session = ConfigSession::bootstrap(
                questionnaire,
                options.sources,
                options.session_options,
            )?;
            let patch = session.apply(SessionEvent::Commit)?;
            return Ok(patch.values);
        }

        let mut session = ConfigSession::with_options(questionnaire, options.session_options)?;
        if options.sources != AnswerSources::default() {
            session.apply_sources(options.sources, Default::default())?;
        }

        Self::run_interactive(&mut session, &locked_keys, frontend)
    }

    fn run_interactive(
        session: &mut ConfigSession,
        locked_keys: &HashSet<String>,
        frontend: &mut impl Frontend,
    ) -> std::result::Result<Answers, RunError> {
        let group_titles = group_titles(session.questionnaire());
        let groups = session.snapshot()?.group_order;
        let total_groups = groups.len().max(1);

        let mut group_index = 0usize;
        let mut last_interactive_group: Option<usize> = None;

        while group_index < groups.len() {
            let group_id = groups[group_index].clone();
            if !group_has_interactive_questions(session, &group_id, locked_keys)? {
                group_index += 1;
                continue;
            }

            let title = group_titles
                .get(&group_id)
                .cloned()
                .unwrap_or_else(|| group_id.clone());
            frontend.begin_group(&title, group_index + 1, total_groups);

            let outcome = Self::prompt_group(session, &group_id, locked_keys, frontend)?;
            frontend.end_group();

            match outcome {
                GroupOutcome::Continue => {
                    last_interactive_group = Some(group_index);
                    group_index += 1;
                }
                GroupOutcome::Back => {
                    if group_index == 0 {
                        frontend.write_line("Already at first page.");
                        continue;
                    }
                    clear_group_values(session, &groups[group_index], locked_keys)?;
                    group_index -= 1;
                    clear_group_values(session, &groups[group_index], locked_keys)?;
                }
                GroupOutcome::Quit => return Err(RunError::Aborted),
            }
        }

        if review_enabled(session) {
            if let Some(review_group) = last_interactive_group {
                if !Self::run_review(session, frontend)? {
                    clear_group_values(session, &groups[review_group], locked_keys)?;
                    return Self::run_interactive_from_group(
                        session,
                        locked_keys,
                        frontend,
                        review_group,
                        last_interactive_group,
                    );
                }
            }
        }

        let patch = session.apply(SessionEvent::Commit)?;
        if patch.phase == SessionPhase::Review {
            if !Self::run_review(session, frontend)? {
                session.apply(SessionEvent::CancelReview)?;
                let restart = last_interactive_group.unwrap_or(0);
                clear_group_values(session, &groups[restart], locked_keys)?;
                return Self::run_interactive_from_group(
                    session,
                    locked_keys,
                    frontend,
                    restart,
                    last_interactive_group,
                );
            }
            let patch = session.apply(SessionEvent::ConfirmReview)?;
            return Ok(patch.values);
        }

        Ok(patch.values)
    }

    fn run_interactive_from_group(
        session: &mut ConfigSession,
        locked_keys: &HashSet<String>,
        frontend: &mut impl Frontend,
        start_group: usize,
        last_interactive_group: Option<usize>,
    ) -> std::result::Result<Answers, RunError> {
        let group_titles = group_titles(session.questionnaire());
        let groups = session.snapshot()?.group_order;
        let total_groups = groups.len().max(1);

        let mut group_index = start_group;
        let mut last_interactive = last_interactive_group;

        while group_index < groups.len() {
            let group_id = groups[group_index].clone();
            if !group_has_interactive_questions(session, &group_id, locked_keys)? {
                group_index += 1;
                continue;
            }

            let title = group_titles
                .get(&group_id)
                .cloned()
                .unwrap_or_else(|| group_id.clone());
            frontend.begin_group(&title, group_index + 1, total_groups);

            let outcome = Self::prompt_group(session, &group_id, locked_keys, frontend)?;
            frontend.end_group();

            match outcome {
                GroupOutcome::Continue => {
                    last_interactive = Some(group_index);
                    group_index += 1;
                }
                GroupOutcome::Back => {
                    if group_index == 0 {
                        frontend.write_line("Already at first page.");
                        continue;
                    }
                    clear_group_values(session, &groups[group_index], locked_keys)?;
                    group_index -= 1;
                    clear_group_values(session, &groups[group_index], locked_keys)?;
                }
                GroupOutcome::Quit => return Err(RunError::Aborted),
            }
        }

        if review_enabled(session) {
            if let Some(review_group) = last_interactive {
                if !Self::run_review(session, frontend)? {
                    clear_group_values(session, &groups[review_group], locked_keys)?;
                    return Self::run_interactive_from_group(
                        session,
                        locked_keys,
                        frontend,
                        review_group,
                        last_interactive,
                    );
                }
            }
        }

        let patch = session.apply(SessionEvent::Commit)?;
        if patch.phase == SessionPhase::Review {
            if !Self::run_review(session, frontend)? {
                session.apply(SessionEvent::CancelReview)?;
                let restart = last_interactive.unwrap_or(start_group);
                clear_group_values(session, &groups[restart], locked_keys)?;
                return Self::run_interactive_from_group(
                    session,
                    locked_keys,
                    frontend,
                    restart,
                    last_interactive,
                );
            }
            let patch = session.apply(SessionEvent::ConfirmReview)?;
            return Ok(patch.values);
        }

        Ok(patch.values)
    }

    fn prompt_group(
        session: &mut ConfigSession,
        group_id: &str,
        locked_keys: &HashSet<String>,
        frontend: &mut impl Frontend,
    ) -> std::result::Result<GroupOutcome, RunError> {
        let keys = questions_for_group(session, group_id, locked_keys)?;
        for key in keys {
            if locked_keys.contains(&key) {
                continue;
            }

            let question = session
                .questionnaire()
                .question(&key)
                .expect("group question must exist")
                .clone();

            match ask_question(session, &question, frontend)? {
                AskOutcome::Value(value) => {
                    session.apply(SessionEvent::SetValue {
                        key: key.clone(),
                        value,
                    })?;
                }
                AskOutcome::Skip => {
                    session.apply(SessionEvent::SetValue {
                        key: key.clone(),
                        value: String::new(),
                    })?;
                }
                AskOutcome::Back => return Ok(GroupOutcome::Back),
                AskOutcome::Quit => return Ok(GroupOutcome::Quit),
            }
        }

        Ok(GroupOutcome::Continue)
    }

    fn run_review(
        session: &mut ConfigSession,
        frontend: &mut impl Frontend,
    ) -> std::result::Result<bool, RunError> {
        let patch = session.snapshot()?;
        let prompt = patch
            .review_prompt
            .clone()
            .unwrap_or_else(|| "Generate with these values? [Y/n]: ".to_string());

        frontend.write_line("");
        frontend.write_line("Review values before finish:");
        for line in format_review_lines(&patch) {
            frontend.write_line(&line);
        }

        loop {
            let input = frontend
                .prompt(&prompt, false)
                .ok_or(RunError::Aborted)?;
            match input.action {
                FrontendAction::Back => return Ok(false),
                FrontendAction::Quit => return Err(RunError::Aborted),
                FrontendAction::Submit => {
                    let normalized = normalize_confirmation(&input.value);
                    if normalized.is_empty() || normalized == "y" || normalized == "yes" {
                        return Ok(true);
                    }
                    if normalized == "n" || normalized == "no" {
                        return Ok(false);
                    }
                    frontend.write_line("Enter yes or no.");
                }
            }
        }
    }
}

enum GroupOutcome {
    Continue,
    Back,
    Quit,
}

enum AskOutcome {
    Value(String),
    Skip,
    Back,
    Quit,
}

fn locked_source_keys(sources: &AnswerSources) -> HashSet<String> {
    let mut locked = HashSet::new();
    for (key, _) in sources.cli.iter().chain(sources.file.iter()) {
        locked.insert(key.clone());
    }
    locked
}

fn group_has_interactive_questions(
    session: &ConfigSession,
    group_id: &str,
    locked_keys: &HashSet<String>,
) -> configwizard_core::Result<bool> {
    let patch = session.snapshot()?;
    for key in session.questionnaire().ordered_question_keys() {
        let question = session.questionnaire().question(&key).expect("ordered key");
        if question.group != group_id {
            continue;
        }
        if locked_keys.contains(&key) {
            continue;
        }
        if patch.visibility.get(&key) != Some(&true) {
            continue;
        }
        if patch.interactive.get(&key) != Some(&true) {
            continue;
        }
        return Ok(true);
    }
    Ok(false)
}

fn questions_for_group(
    session: &ConfigSession,
    group_id: &str,
    locked_keys: &HashSet<String>,
) -> configwizard_core::Result<Vec<String>> {
    let patch = session.snapshot()?;
    let mut keys = Vec::new();
    for key in session.questionnaire().ordered_question_keys() {
        let question = session.questionnaire().question(&key).expect("ordered key");
        if question.group != group_id {
            continue;
        }
        if locked_keys.contains(&key) {
            continue;
        }
        if patch.visibility.get(&key) != Some(&true) {
            continue;
        }
        if patch.interactive.get(&key) != Some(&true) {
            continue;
        }
        keys.push(key);
    }
    Ok(keys)
}

fn ask_question(
    session: &mut ConfigSession,
    question: &Question,
    frontend: &mut impl Frontend,
) -> std::result::Result<AskOutcome, RunError> {
    loop {
        let patch = session.snapshot()?;
        let text = prompt_text(question, &patch);
        let input = frontend
            .prompt(&text, question.sensitive)
            .ok_or(RunError::Aborted)?;

        match input.action {
            FrontendAction::Back => return Ok(AskOutcome::Back),
            FrontendAction::Quit => return Ok(AskOutcome::Quit),
            FrontendAction::Submit => {
                let trimmed = input.value.trim();
                if trimmed == "?" && !question.help.is_empty() {
                    frontend.write_line(&question.help);
                    continue;
                }
                if trimmed == "-" && question.optional {
                    return Ok(AskOutcome::Skip);
                }

                let candidate = match question.question_type {
                    configwizard_core::QuestionType::Bool => {
                        let default = patch.display_values.get(&question.key).map(String::as_str);
                        match normalize_bool_input(trimmed, default) {
                            Some(value) => value,
                            None => {
                                frontend.write_line("Enter yes or no.");
                                continue;
                            }
                        }
                    }
                    _ if trimmed.is_empty() => patch
                        .display_values
                        .get(&question.key)
                        .cloned()
                        .unwrap_or_default(),
                    _ => trimmed.to_string(),
                };

                if candidate.is_empty() && !question.optional {
                    frontend.write_line("Value required.");
                    continue;
                }
                if candidate.is_empty() && question.optional {
                    return Ok(AskOutcome::Skip);
                }

                let validation_patch = session.apply(SessionEvent::SetValue {
                    key: question.key.clone(),
                    value: candidate.clone(),
                })?;

                if let Some(error) = validation_patch.errors.get(&question.key) {
                    frontend.write_line(error);
                    continue;
                }

                return Ok(AskOutcome::Value(
                    validation_patch
                        .values
                        .get(&question.key)
                        .cloned()
                        .unwrap_or(candidate),
                ));
            }
        }
    }
}

fn clear_group_values(
    session: &mut ConfigSession,
    group_id: &str,
    locked_keys: &HashSet<String>,
) -> std::result::Result<(), RunError> {
    for key in session.questionnaire().ordered_question_keys() {
        let question = session.questionnaire().question(&key).expect("ordered key");
        if question.group != group_id || locked_keys.contains(&key) {
            continue;
        }
        session.apply(SessionEvent::SetValue {
            key,
            value: String::new(),
        })?;
    }
    Ok(())
}

fn review_enabled(session: &ConfigSession) -> bool {
    session
        .questionnaire()
        .review
        .as_ref()
        .map(|review| review.enabled)
        .unwrap_or(false)
}

fn group_titles(questionnaire: &Questionnaire) -> HashMap<String, String> {
    questionnaire
        .groups
        .iter()
        .map(|group| {
            let title = group
                .title
                .clone()
                .filter(|value| !value.is_empty())
                .unwrap_or_else(|| group.id.clone());
            (group.id.clone(), title)
        })
        .collect()
}

fn normalize_confirmation(value: &str) -> String {
    value
        .chars()
        .filter(|ch| !ch.is_whitespace())
        .collect::<String>()
        .to_ascii_lowercase()
}
