QT += core
QT -= gui
QT += network

TARGET = MarketWorker
CONFIG += console
CONFIG -= app_bundle
CONFIG += c++14

PKGCONFIG += openssl
PKGCONFIG += rtmp

TEMPLATE = app

SOURCES += \
    market_worker.cpp \
    lib/jsoncpp-1.8.3/src/jsoncpp.cpp \
    lib/sqlite-amalgamation/sqlite3.c

HEADERS += \
    lib/jsoncpp-1.8.3/include/json/json.h \
    lib/jsoncpp-1.8.3/include/json/json-forwards.h \
    lib/libcurl-7.56.0/include/curl/curl.h \
    lib/libcurl-7.56.0/include/curl/curlver.h \
    lib/libcurl-7.56.0/include/curl/easy.h \
    lib/libcurl-7.56.0/include/curl/mprintf.h \
    lib/libcurl-7.56.0/include/curl/multi.h \
    lib/libcurl-7.56.0/include/curl/stdcheaders.h \
    lib/libcurl-7.56.0/include/curl/system.h \
    lib/libcurl-7.56.0/include/curl/typecheck-gcc.h \
    lib/libwebsockets-2.4.0/include/libwebsockets.h \
    lib/libwebsockets-2.4.0/include/lws-plugin-ssh.h \
    lib/libwebsockets-2.4.0/include/lws_config.h \
    lib/libbinacpp/include/binacpp.h \
    lib/libbinacpp/include/binacpp_logger.h \
    lib/libbinacpp/include/binacpp_utils.h \
    lib/libbinacpp/include/binacpp_websocket.h \
    lib/sqlite-amalgamation/sqlite3.h \
    lib/sqlite_orm/sqlite_orm.h

INCLUDEPATH += $$PWD/lib/jsoncpp-1.8.3/include
DEPENDPATH += $$PWD/lib/jsoncpp-1.8.3/include

unix:!macx: LIBS += -L$$PWD/lib/libbinacpp/lib/ -lbinacpp

INCLUDEPATH += $$PWD/lib/libbinacpp/include
DEPENDPATH += $$PWD/lib/libbinacpp/include

unix:!macx: LIBS += -L$$PWD/lib/libcurl-7.56.0/lib/ -lcurl

INCLUDEPATH += $$PWD/lib/libcurl-7.56.0/include
DEPENDPATH += $$PWD/lib/libcurl-7.56.0/include

unix:!macx: PRE_TARGETDEPS += $$PWD/lib/libcurl-7.56.0/lib/libcurl.a

unix:!macx: LIBS += -L$$PWD/lib/libwebsockets-2.4.0/lib/ -lwebsockets

INCLUDEPATH += $$PWD/lib/libwebsockets-2.4.0/include
DEPENDPATH += $$PWD/lib/libwebsockets-2.4.0/include

unix:!macx: PRE_TARGETDEPS += $$PWD/lib/libwebsockets-2.4.0/lib/libwebsockets.a

INCLUDEPATH += $$PWD/lib/sqlite-amalgamation
DEPENDPATH += $$PWD/lib/sqlite-amalgamation

unix:!macx: LIBS += -L$$PWD/amalgamation/ -ldl
