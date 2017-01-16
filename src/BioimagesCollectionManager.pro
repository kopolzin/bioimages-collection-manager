QT       += core gui widgets network concurrent sql xml

TARGET = "Bioimages Collection Manager"
TEMPLATE = app
INCLUDEPATH += .

CONFIG += c++11

RC_ICONS += icon.ico

macx-clang {
    ICON = mac.icns
}

SOURCES += main.cpp\
        startwindow.cpp \
    help.cpp \
    image.cpp \
    determination.cpp \
    sensu.cpp \
    editexisting.cpp \
    newagentdialog.cpp \
    agent.cpp \
    newdeterminationdialog.cpp \
    geonamesother.cpp \
    advancedthumbnailfilter.cpp \
    newsensudialog.cpp \
    tableeditor.cpp \
    exportcsv.cpp \
    taxa.cpp \
    importcsv.cpp \
    mergetables.cpp \
    managecsvs.cpp \
    dataentry.cpp \
    organism.cpp \
    processnewimages.cpp \
    advancedoptions.cpp

HEADERS  += startwindow.h \
    help.h \
    image.h \
    determination.h \
    sensu.h \
    editexisting.h \
    newagentdialog.h \
    agent.h \
    newdeterminationdialog.h \
    geonamesother.h \
    advancedthumbnailfilter.h \
    newsensudialog.h \
    tableeditor.h \
    exportcsv.h \
    taxa.h \
    importcsv.h \
    mergetables.h \
    managecsvs.h \
    dataentry.h \
    organism.h \
    processnewimages.h \
    advancedoptions.h

FORMS    += startwindow.ui \
    help.ui \
    editexisting.ui \
    newagentdialog.ui \
    newdeterminationdialog.ui \
    geonamesother.ui \
    advancedthumbnailfilter.ui \
    newsensudialog.ui \
    managecsvs.ui \
    dataentry.ui \
    processnewimages.ui \
    advancedoptions.ui

RESOURCES += \
    BioCM.qrc
