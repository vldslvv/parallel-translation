add_test(
    NAME help_flag_exits_successfully
    COMMAND parallel-translation --help
)
set_tests_properties(help_flag_exits_successfully PROPERTIES
    PASS_REGULAR_EXPRESSION "Usage:"
)

include(Catch)


add_executable(test_integration
    test_translator.cpp
    test_tokenizer.cpp
    test_reader.cpp
    ../src/app.cpp
    ../src/config.cpp
    ../src/translators/stub.cpp
    ../src/translators/pass.cpp
    ../src/translators/ollama.cpp
    ../src/readers/txt.cpp
    ../src/writers/txt.cpp
)
target_include_directories(test_integration PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_integration PRIVATE
    Catch2::Catch2WithMain
    CLI11::CLI11
    httplib::httplib
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
    spdlog::spdlog
)
target_compile_definitions(test_integration PRIVATE ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
catch_discover_tests(test_integration)
