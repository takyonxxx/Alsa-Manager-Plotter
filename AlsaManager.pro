QT += core gui
QT += quick multimedia quickcontrols2

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17 console
CONFIG -= app_bundle
TARGET = alsamanager

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        alsadevices.cpp \
        alsamanager.cpp \
        kalmanfilter.cpp \
        main.cpp \
        plotter.cpp \
        voicetranslator.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    alsadevices.h \
    alsamanager.h \
    constants.h \
    kalmanfilter.h \
    plotter.h \
    voicetranslator.h

win32 {
    contains(TARGET_ARCH, x86_64){
        message("Win 64 bit enabled")

    }else{
        message("Win 32 bit enabled")
    }
}

macx{
    message("Macx enabled")
}

unix:!macx{
    message("Unix enabled")
    #QMAKE_INCDIR += /usr/local/include

    QMAKE_LIBDIR +=usr/lib
    QMAKE_LIBDIR += /usr/local/lib
    INCLUDEPATH += /usr/local/include
    INCLUDEPATH += /usr/local/include/pocketsphinx
    INCLUDEPATH += /usr/local/include/sphinxbase
    LIBS += -lasound -lsphinxbase -lpocketsphinx -lsphinxad -lfftw3
}

DISTFILES += \
    glscope/README

#sudo apt-get install bison
#sudo apt-get install swig
#sudo apt-get install libasound2-dev
#sudo apt-get install alsa alsa-tools
#sudo apt install libsphinxbase-dev
#sudo apt-get install libfftw3-dev
#pocketsphinx_continuous -infile test.wav -keyphrase "oh mighty computer" -kws_threshold 1e-20
#sudo ldconfig

RESOURCES += \
    resources.qrc

FORMS +=


