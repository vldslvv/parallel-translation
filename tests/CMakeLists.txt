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
    test_pdf_writer.cpp
    test_text.cpp
    ../src/app.cpp
    ../src/common/process.cpp
    ../src/text/text.cpp
    ../src/config.cpp
    ../src/translators/stub.cpp
    ../src/translators/pass.cpp
    ../src/translators/morpheus.cpp
    ../src/translators/chat_api.cpp
    ../src/readers/sentence_splitter.cpp
    ../src/readers/txt.cpp
    ../src/readers/pdf.cpp
    ../src/writers/txt.cpp
    ../src/writers/pdf.cpp
    ../src/writers/formatted_writer.cpp
    ../src/formatters/plain.cpp
)
# FREETYPE_INCLUDE_DIR is resolved in the root CMakeLists via find_path.
# See that file for the full explanation of why the include path must be set
# explicitly rather than relying on the Freetype::Freetype target.
target_include_directories(test_integration PRIVATE ${CMAKE_SOURCE_DIR}/src "${FREETYPE_INCLUDE_DIR}")
target_link_libraries(test_integration PRIVATE
    Catch2::Catch2WithMain
    CLI11::CLI11
    httplib::httplib
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
    spdlog::spdlog
    poppler::poppler
    PDFHummus::PDFWriter
    Freetype::Freetype
)
target_compile_options(test_integration PRIVATE
    # /home/vv is a symlink to /mnt/shared_linux/home_shared, causing the compiler
    # to embed symlink paths in debug info that the debugger can't resolve.
    # This rewrites them to the real path at compile time.
    $<$<CONFIG:Debug>:-fdebug-prefix-map=/home/vv=/mnt/shared_linux/home_shared>
)
target_compile_definitions(test_integration PRIVATE ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
target_compile_definitions(test_integration PRIVATE
    PT_MORPHEUS_PACKAGE_DIR="${PT_MORPHEUS_PACKAGE_DIR}"
    PT_INSTALL_BINDIR="${CMAKE_INSTALL_BINDIR}"
    PT_MORPHEUS_INSTALL_LIBEXEC_DIR="${PT_MORPHEUS_INSTALL_LIBEXEC_DIR}"
    PT_MORPHEUS_INSTALL_SHARE_DIR="${PT_MORPHEUS_INSTALL_SHARE_DIR}"
)
catch_discover_tests(test_integration)
