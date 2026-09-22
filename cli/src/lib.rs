//! Sequential inline CLI frontends for ConfigWizard.

mod frontend;
mod plain;
mod prompt;
mod runner;
mod wizard;

pub use frontend::{Frontend, FrontendAction, PromptResult};
pub use plain::PlainCliFrontend;
pub use runner::{RunError, RunOptions, WizardRunner};
pub use wizard::WizardCliFrontend;
