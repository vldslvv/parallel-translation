add_test(
    NAME help_flag_exits_successfully
    COMMAND parallel-translation --help
)
set_tests_properties(help_flag_exits_successfully PROPERTIES
    PASS_REGULAR_EXPRESSION "Usage:"
)

include(Catch)

add_executable(test_translator test_translator.cpp ../src/translators/stub.cpp)
target_include_directories(test_translator PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_translator PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(test_translator)

add_executable(test_tokenizer test_tokenizer.cpp)
target_include_directories(test_tokenizer PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_tokenizer PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(test_tokenizer)
