add_executable(feedparsertest feedparsertest.cpp)
target_link_libraries(feedparsertest PRIVATE homefeedparser Qt6::Test)
target_compile_definitions(feedparsertest PRIVATE FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_test(NAME feedparsertest COMMAND feedparsertest)
# Day assignment depends on the local time zone; pin it so the fixture's
# offsets mean the same thing on every machine and in CI.
set_tests_properties(feedparsertest PROPERTIES ENVIRONMENT "TZ=UTC")

# Loads the real plugin binary through KPluginMetaData and QPluginLoader, the
# same path plasma-workspace takes. An empty XDG_CONFIG_HOME keeps it offline.
add_executable(pluginloadtest pluginloadtest.cpp)
target_link_libraries(pluginloadtest PRIVATE Qt6::Test KF6::CoreAddons KF6::CalendarEvents)
target_compile_definitions(pluginloadtest PRIVATE PLUGIN_PATH="$<TARGET_FILE:homefeedevents>")
add_dependencies(pluginloadtest homefeedevents)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xdg-config)
add_test(NAME pluginloadtest COMMAND pluginloadtest)
set_tests_properties(pluginloadtest PROPERTIES ENVIRONMENT "XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/xdg-config;TZ=UTC")
