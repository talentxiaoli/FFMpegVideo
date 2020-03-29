TEMLATE = app
CONFIG += console c++11
SOURCES += FFmpegVideo.cpp
INCLUDEPATH += $$PWD/../../include
LIBS += -L$$PWD/../../lib
message($$QMAKESPEC)
