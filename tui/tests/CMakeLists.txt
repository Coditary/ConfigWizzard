add_executable(configwizard-ui-tui-tests
    plain_frontend_test.cpp
)

target_link_libraries(configwizard-ui-tui-tests PRIVATE configwizard_ui_tui)

add_test(NAME plain_frontend_prompt COMMAND configwizard-ui-tui-tests)
