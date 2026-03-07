add_test(
    NAME help_flag_exits_successfully
    COMMAND parallel-translation --help
)
set_tests_properties(help_flag_exits_successfully PROPERTIES
    PASS_REGULAR_EXPRESSION "Usage:"
)
