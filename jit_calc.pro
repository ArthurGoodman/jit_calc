CONFIG -= qt app_bundle
CONFIG += console c++11

INCLUDEPATH += ../compiler/compiler
LIBS += -L../compiler/compiler/release -lcompiler

SOURCES += \
    jit_calc.cpp
