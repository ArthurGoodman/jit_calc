CONFIG -= qt app_bundle
CONFIG += console c++11

INCLUDEPATH += ../compiler/compiler
LIBS += -L../compiler/compiler/release -lcompiler

#QMAKE_CXXFLAGS_RELEASE -= -O1
#QMAKE_CXXFLAGS_RELEASE -= -O2
#QMAKE_CXXFLAGS_RELEASE -= -O3
#QMAKE_CXXFLAGS_RELEASE += -O0

SOURCES += \
    jit_calc.cpp
