#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FrontendAction {
    Submit,
    Back,
    Quit,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PromptResult {
    pub action: FrontendAction,
    pub value: String,
}

pub trait Frontend {
    fn prompt(&mut self, text: &str, sensitive: bool) -> Option<PromptResult>;
    fn write_line(&mut self, text: &str);
    fn begin_group(&mut self, name: &str, index: usize, total: usize);
    fn end_group(&mut self);
}
