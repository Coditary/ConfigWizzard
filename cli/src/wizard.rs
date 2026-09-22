use std::io::{self, Write};

use crate::frontend::{Frontend, FrontendAction, PromptResult};

/// Sequential CLI with wizard-style page headers (Tempify `WizardFrontend` style).
pub struct WizardCliFrontend {
    input: Box<dyn io::BufRead>,
    output: Box<dyn Write>,
}

impl WizardCliFrontend {
    pub fn new() -> Self {
        Self {
            input: Box::new(io::stdin().lock()),
            output: Box::new(io::stdout().lock()),
        }
    }

    pub fn with_io(input: impl io::BufRead + 'static, output: impl Write + 'static) -> Self {
        Self {
            input: Box::new(input),
            output: Box::new(output),
        }
    }
}

impl Default for WizardCliFrontend {
    fn default() -> Self {
        Self::new()
    }
}

impl Frontend for WizardCliFrontend {
    fn prompt(&mut self, text: &str, sensitive: bool) -> Option<PromptResult> {
        let _ = self.output.write_all(text.as_bytes());
        let _ = self.output.flush();

        if sensitive {
            // Sensitive echo hiding is platform-specific; keep the input path simple for now.
        }

        let mut line = String::new();
        if self.input.read_line(&mut line).ok()? == 0 {
            return None;
        }

        let line = line.trim_end_matches(['\r', '\n']).to_string();
        if line == ":back" {
            return Some(PromptResult {
                action: FrontendAction::Back,
                value: String::new(),
            });
        }
        if line == ":quit" {
            return Some(PromptResult {
                action: FrontendAction::Quit,
                value: String::new(),
            });
        }

        Some(PromptResult {
            action: FrontendAction::Submit,
            value: line,
        })
    }

    fn write_line(&mut self, text: &str) {
        let _ = writeln!(self.output, "{}", text);
        let _ = self.output.flush();
    }

    fn begin_group(&mut self, name: &str, index: usize, total: usize) {
        self.write_line("");
        self.write_line("========================================");
        let header = if name.is_empty() {
            format!("Page {}/{}", index, total)
        } else {
            format!("Page {}/{} | {}", index, total, name)
        };
        self.write_line(&header);
        self.write_line("Commands: :back  :quit  ? for help");
        self.write_line("Press Enter to accept shown default value.");
        self.write_line("========================================");
    }

    fn end_group(&mut self) {
        self.write_line("----------------------------------------");
    }
}
