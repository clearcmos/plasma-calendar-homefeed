# The parser is a separate static library so the tests link it without the
# Qt plugin entry point or the network layer.
add_library(homefeedparser STATIC feedparser.cpp feedparser.h)
target_include_directories(homefeedparser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(homefeedparser PUBLIC Qt6::Core KF6::CalendarEvents)
set_target_properties(homefeedparser PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Config, timer, and fetch shared by the clock plugin and the panel applet.
add_library(homefeedsource STATIC feedsource.cpp feedsource.h)
target_link_libraries(homefeedsource PUBLIC homefeedparser Qt6::Network KF6::ConfigCore)
set_target_properties(homefeedsource PROPERTIES POSITION_INDEPENDENT_CODE ON)

ecm_qt_declare_logging_category(homefeedsource
    HEADER homefeed_debug.h
    IDENTIFIER HOMEFEED
    CATEGORY_NAME org.clearcmos.plasma.homefeed
    DESCRIPTION "Plasma calendar home feed plugin"
    DEFAULT_SEVERITY Warning
)

# Installs to <qt plugin dir>/plasmacalendarplugins/homefeedevents.so, which is
# where plasma-workspace's EventPluginsManager scans for calendar plugins.
kcoreaddons_add_plugin(homefeedevents
    SOURCES homefeedevents.cpp homefeedevents.h
    INSTALL_NAMESPACE "plasmacalendarplugins"
)

target_link_libraries(homefeedevents PRIVATE
    homefeedsource
    Qt6::Core
    Qt6::Gui
    KF6::CalendarEvents
)
