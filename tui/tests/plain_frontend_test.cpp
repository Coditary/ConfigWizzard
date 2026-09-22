#include "configwizard_ui/plain_frontend.hpp"

int main() {
    configwizard_ui::PlainCliFrontend frontend;
    frontend.write_line("ok");
    frontend.begin_group("Project", 1, 2);
    frontend.end_group();
    return 0;
}
