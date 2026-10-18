# Installs to <qt plugin dir>/plasma/applets/io.github.clearcmos.nextevent.so,
# with main.qml compiled into the plugin's QML module.
plasma_add_applet(io.github.clearcmos.nextevent
    QML_SOURCES main.qml
    CPP_SOURCES nextevent.cpp nextevent.h
    GENERATE_APPLET_CLASS
)

target_link_libraries(io.github.clearcmos.nextevent PRIVATE
    homefeedsource
    Qt6::Qml
)
