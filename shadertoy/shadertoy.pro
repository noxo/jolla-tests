# NOTICE:
#
# Application name defined in TARGET has a corresponding QML filename.
# If name defined in TARGET is changed, the following needs to be done
# to match new name:
#   - corresponding QML filename must be changed
#   - desktop icon filename must be changed
#   - desktop filename must be changed
#   - icon definition filename in desktop file must be changed
#   - translation filenames have to be changed

# The name of your application
TARGET = shadertoy

CONFIG += sailfishapp

SOURCES += src/shadertoy.cpp \
    shadertoyglview.cpp

OTHER_FILES += qml/shadertoy.qml \
    qml/cover/CoverPage.qml \
    qml/pages/FirstPage.qml \
    rpm/shadertoy.changes.in \
    rpm/shadertoy.spec \
    rpm/shadertoy.yaml \
    translations/*.ts \
    shadertoy.desktop

# to disable building translations every time, comment out the
# following CONFIG line
CONFIG += sailfishapp_i18n
TRANSLATIONS += translations/shadertoy-de.ts

HEADERS += \
    shadertoyglview.h

RESOURCES += \
    resources.qrc

