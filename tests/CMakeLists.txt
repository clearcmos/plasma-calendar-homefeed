add_executable(feedparsertest feedparsertest.cpp)
target_link_libraries(feedparsertest PRIVATE homefeedparser Qt6::Test)
target_compile_definitions(feedparsertest PRIVATE FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_test(NAME feedparsertest COMMAND feedparsertest)
# Day assignment depends on the local time zone; pin it so the fixture's
# offsets mean the same thing on every machine and in CI.
set_tests_properties(feedparsertest PROPERTIES ENVIRONMENT "TZ=UTC")
