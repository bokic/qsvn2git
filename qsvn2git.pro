QT       += core gui xml

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET    = qsvn2git
TEMPLATE  = app

SOURCES  += main.cpp qmaindialog.cpp qconvertorworker.cpp
HEADERS  +=          qmaindialog.h   qconvertorworker.h
FORMS    +=          qmaindialog.ui
