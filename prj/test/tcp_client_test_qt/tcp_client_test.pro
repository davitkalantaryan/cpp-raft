# File daqadcreceiver.pro
# File created : 01 Jan 2017
# Created by : Davit Kalantaryan (davit.kalantaryan@desy.de)
# This file can be used to produce Makefile for daqadcreceiver application
# for PITZ
# CONFIG += 2test
# $qmake "CONFIG+=STATIC_CMP"  in the case static linkage will be used
#TEMPLATE = lib

#QMAKE_CXXFLAGS += -include stdint.h
#QMAKE_CXXFLAGS += -include stdio.h
#QMAKE_CXXFLAGS += -include floyd_posix_first_include.h

options = $$find(CONFIG, "STATIC_CMP")

#QMAKE_CXXFLAGS_WARN_ON += -Wno-unused-parameter
#QMAKE_CXXFLAGS_WARN_ON += -Wfpermissive

count(options, 1) {

    message("!!! data_server: static")
    #include(../libdata_server_qt/libdata_server.pri)
    #LIBS += $$LIBDIR/librocksdb.a

}else{

    message("!!! data_server: dynamic")
    #include(../libdata_server_qt/libdata_server_includes.pri)
    #LIBS += -llibdata_server
    #LIBS += -lrocksdb_debug
}

include(../../common/common_qt/sys_common.pri)

LIBS += -L../../../../sys/$$CODENAME/lib

INCLUDEPATH += ../../../sys/$$CODENAME/src
INCLUDEPATH += ../../../include

CONFIG += debug

SOURCES += \
    ../../../src/test/main_tcp_client_test.cpp \
    ../../../common/common_iodevice.cpp \
    ../../../common/common_nonblockingsockettcp.cpp \
    ../../../common/common_socketbase.cpp \
    ../../../common/common_sockettcp.cpp


HEADERS += \
    ../../../include/common/common_sockettcp.hpp \
    ../../../include/common/impl.common_socketbase.hpp \
    ../../../include/common/common_socketbase.hpp \
    ../../../include/common/common_iodevice.hpp \
    ../../../include/cpp11+/common_defination.h \
    ../../../src/test/tcp_server_client_common.h
