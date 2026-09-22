use configwizard_core::{Question, QuestionType, StatePatch};

pub fn prompt_text(question: &Question, patch: &StatePatch) -> String {
    let mut lines = Vec::new();

    let label = if question.prompt.is_empty() {
        question.key.clone()
    } else {
        question.prompt.clone()
    };
    lines.push(label);

    if !question.help.is_empty() {
        lines.push(format!("  (? for help: {})", question.help));
    }

    match question.question_type {
        QuestionType::Bool => {
            let default = patch
                .display_values
                .get(&question.key)
                .map(String::as_str)
                .unwrap_or("false");
            let hint = bool_hint(default);
            lines.push(format!("  [{}] ", hint));
        }
        QuestionType::Choice | QuestionType::Multiselect => {
            let available = patch
                .available_choices
                .get(&question.key)
                .cloned()
                .unwrap_or_default();
            for choice in &available {
                let label = choice_label(question, choice);
                lines.push(format!("  - {} ({})", label, choice));
            }
            if let Some(default) = patch.display_values.get(&question.key) {
                if !default.is_empty() {
                    lines.push(format!("  [default: {}] ", default));
                }
            }
            lines.push("> ".to_string());
        }
        _ => {
            if let Some(default) = patch.display_values.get(&question.key) {
                if !default.is_empty() {
                    lines.push(format!("  [default: {}] ", default));
                } else {
                    lines.push("> ".to_string());
                }
            } else if let Some(placeholder) = &question.placeholder {
                lines.push(format!("  [placeholder: {}] ", placeholder));
            } else {
                lines.push("> ".to_string());
            }
        }
    }

    lines.join("\n")
}

fn bool_hint(default: &str) -> String {
    match default.trim().to_ascii_lowercase().as_str() {
        "true" | "1" | "yes" | "on" => "Y/n".to_string(),
        _ => "y/N".to_string(),
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

pub fn normalize_bool_input(input: &str, default: Option<&str>) -> Option<String> {
    let trimmed = input.trim();
    if trimmed.is_empty() {
        return default.map(|value| value.to_string());
    }

    match trimmed.to_ascii_lowercase().as_str() {
        "true" | "1" | "yes" | "y" | "on" => Some("true".to_string()),
        "false" | "0" | "no" | "n" | "off" => Some("false".to_string()),
        _ => None,
    }
}

pub fn format_review_lines(patch: &StatePatch) -> Vec<String> {
    let mut lines = Vec::new();
    let mut last_group = String::new();

    for item in &patch.review_items {
        if item.group != last_group {
            if !lines.is_empty() {
                lines.push(String::new());
            }
            lines.push(format!("[{}]", item.group));
            last_group = item.group.clone();
        }
        lines.push(format!("{}: {}", item.prompt, item.display_value));
    }

    lines
}
