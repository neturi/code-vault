# Copyright c1997-2010 Trygve Isaacson. All rights reserved.
# This file is part of the Code Vault version 3.1
# http://www.bombaydigital.com/

#
# This is a qmake include file -- to be included from a .pro file.
#
# Because paths are relative to the main file (not this one), this
# file relies on the main file defining the variable $${VAULT_BASE} so
# that this file can specify paths correctly.
#

win32:LIBS += wsock32.lib
win32:LIBS += psapi.lib

debug {
DEFINES += V_DEBUG
}

macx {
CONFIG += = vault_mac
} else:unix {
CONFIG += = vault_unix
} else:win32 {
CONFIG += = vault_win32
}

vault_mac {
DEPENDPATH += $${VAULT_BASE}/source/vtypes/_mac
DEPENDPATH += $${VAULT_BASE}/source/threads/_unix
DEPENDPATH += $${VAULT_BASE}/source/sockets/_unix
DEPENDPATH += $${VAULT_BASE}/source/files/_unix

INCLUDEPATH += $${VAULT_BASE}/source/vtypes/_mac
INCLUDEPATH += $${VAULT_BASE}/source/threads/_unix
INCLUDEPATH += $${VAULT_BASE}/source/sockets/_unix
INCLUDEPATH += $${VAULT_BASE}/source/files/_unix

HEADERS += $${VAULT_BASE}/source/vtypes/_mac/vtypes_internal_platform.h
HEADERS += $${VAULT_BASE}/source/vtypes/_mac/vtypes_platform.h
SOURCES += $${VAULT_BASE}/source/vtypes/_mac/vtypes_platform.cpp
OBJECTIVE_SOURCES += $${VAULT_BASE}/source/vtypes/_mac/vtypes_platform_objc.mm
SOURCES += $${VAULT_BASE}/source/containers/_unix/vinstant_platform.cpp
SOURCES += $${VAULT_BASE}/source/files/_unix/vfsnode_platform.cpp
HEADERS += $${VAULT_BASE}/source/threads/_unix/vthread_platform.h
SOURCES += $${VAULT_BASE}/source/threads/_unix/vthread_platform.cpp
HEADERS += $${VAULT_BASE}/source/sockets/_unix/vsocket.h
SOURCES += $${VAULT_BASE}/source/sockets/_unix/vsocket.cpp
SOURCES += $${VAULT_BASE}/source/toolbox/_unix/vhighresolutiontimehelper_platform.cpp
}

vault_unix {
DEPENDPATH += $${VAULT_BASE}/source/vtypes/_unix
DEPENDPATH += $${VAULT_BASE}/source/threads/_unix
DEPENDPATH += $${VAULT_BASE}/source/sockets/_unix
DEPENDPATH += $${VAULT_BASE}/source/files/_unix

INCLUDEPATH += $${VAULT_BASE}/source/vtypes/_unix
INCLUDEPATH += $${VAULT_BASE}/source/threads/_unix
INCLUDEPATH += $${VAULT_BASE}/source/sockets/_unix
INCLUDEPATH += $${VAULT_BASE}/source/files/_unix

HEADERS += $${VAULT_BASE}/source/vtypes/_unix/vtypes_internal_platform.h
HEADERS += $${VAULT_BASE}/source/vtypes/_unix/vtypes_platform.h
SOURCES += $${VAULT_BASE}/source/vtypes/_unix/vtypes_platform.cpp
SOURCES += $${VAULT_BASE}/source/containers/_unix/vinstant_platform.cpp
SOURCES += $${VAULT_BASE}/source/files/_unix/vfsnode_platform.cpp
HEADERS += $${VAULT_BASE}/source/threads/_unix/vthread_platform.h
SOURCES += $${VAULT_BASE}/source/threads/_unix/vthread_platform.cpp
HEADERS += $${VAULT_BASE}/source/sockets/_unix/vsocket.h
SOURCES += $${VAULT_BASE}/source/sockets/_unix/vsocket.cpp
SOURCES += $${VAULT_BASE}/source/toolbox/_unix/vhighresolutiontimehelper_platform.cpp
}

vault_win32 {
DEPENDPATH += $${VAULT_BASE}/source/vtypes/_win
DEPENDPATH += $${VAULT_BASE}/source/threads/_win
DEPENDPATH += $${VAULT_BASE}/source/sockets/_win
DEPENDPATH += $${VAULT_BASE}/source/files/_win

INCLUDEPATH += $${VAULT_BASE}/source/vtypes/_win
INCLUDEPATH += $${VAULT_BASE}/source/threads/_win
INCLUDEPATH += $${VAULT_BASE}/source/sockets/_win
INCLUDEPATH += $${VAULT_BASE}/source/files/_win

HEADERS += $${VAULT_BASE}/source/vtypes/_win/vtypes_internal_platform.h
HEADERS += $${VAULT_BASE}/source/vtypes/_win/vtypes_platform.h
SOURCES += $${VAULT_BASE}/source/vtypes/_win/vtypes_platform.cpp
SOURCES += $${VAULT_BASE}/source/containers/_win/vinstant_platform.cpp
SOURCES += $${VAULT_BASE}/source/files/_win/vfsnode_platform.cpp
HEADERS += $${VAULT_BASE}/source/threads/_win/vthread_platform.h
SOURCES += $${VAULT_BASE}/source/threads/_win/vthread_platform.cpp
SOURCES += $${VAULT_BASE}/source/sockets/_win/vcommsessioneventproducerfactory.cpp
HEADERS += $${VAULT_BASE}/source/sockets/_win/vcommsessioninfo.h
SOURCES += $${VAULT_BASE}/source/sockets/_win/vcommsessioninfo.cpp
HEADERS += $${VAULT_BASE}/source/sockets/_win/vpollingthreadinfo.h
SOURCES += $${VAULT_BASE}/source/sockets/_win/vpollingthreadinfo.cpp
HEADERS += $${VAULT_BASE}/source/sockets/_win/vsocket.h
SOURCES += $${VAULT_BASE}/source/sockets/_win/vsocket.cpp
HEADERS += $${VAULT_BASE}/source/sockets/_win/vwsautils.h
SOURCES += $${VAULT_BASE}/source/sockets/_win/vwsautils.cpp
SOURCES += $${VAULT_BASE}/source/toolbox/_win/vhighresolutiontimehelper_platform.cpp
HEADERS += $${VAULT_BASE}/source/sockets/_win/vwsaeventproducer.h
SOURCES += $${VAULT_BASE}/source/sockets/_win/vwsaeventproducer.cpp

LINKER_LIBRARIES += Psapi.lib
}

DEPENDPATH += $${VAULT_BASE}/source
DEPENDPATH += $${VAULT_BASE}/source/containers
DEPENDPATH += $${VAULT_BASE}/source/files
DEPENDPATH += $${VAULT_BASE}/source/server
DEPENDPATH += $${VAULT_BASE}/source/sockets
DEPENDPATH += $${VAULT_BASE}/source/streams
DEPENDPATH += $${VAULT_BASE}/source/threads
DEPENDPATH += $${VAULT_BASE}/source/toolbox
DEPENDPATH += $${VAULT_BASE}/source/vtypes

INCLUDEPATH += $${VAULT_BASE}/source
INCLUDEPATH += $${VAULT_BASE}/source/containers
INCLUDEPATH += $${VAULT_BASE}/source/files
INCLUDEPATH += $${VAULT_BASE}/source/server
INCLUDEPATH += $${VAULT_BASE}/source/sockets
INCLUDEPATH += $${VAULT_BASE}/source/streams
INCLUDEPATH += $${VAULT_BASE}/source/threads
INCLUDEPATH += $${VAULT_BASE}/source/toolbox
INCLUDEPATH += $${VAULT_BASE}/source/vtypes

HEADERS += $${VAULT_BASE}/source/vault.h
#HEADERS += $${VAULT_BASE}/source/vconfigure.h
HEADERS += $${VAULT_BASE}/source/vtypes/vtypes_internal.h
SOURCES += $${VAULT_BASE}/source/vtypes/vtypes_internal.cpp
HEADERS += $${VAULT_BASE}/source/vtypes/vtypes.h
SOURCES += $${VAULT_BASE}/source/vtypes/vtypes.cpp
HEADERS += $${VAULT_BASE}/source/containers/vbento.h
SOURCES += $${VAULT_BASE}/source/containers/vbento.cpp
HEADERS += $${VAULT_BASE}/source/containers/vchar.h
SOURCES += $${VAULT_BASE}/source/containers/vchar.cpp
HEADERS += $${VAULT_BASE}/source/containers/vcolor.h
SOURCES += $${VAULT_BASE}/source/containers/vcolor.cpp
HEADERS += $${VAULT_BASE}/source/containers/vcompactingdeque.h
HEADERS += $${VAULT_BASE}/source/containers/vexception.h
SOURCES += $${VAULT_BASE}/source/containers/vexception.cpp
HEADERS += $${VAULT_BASE}/source/containers/vgeometry.h
SOURCES += $${VAULT_BASE}/source/containers/vgeometry.cpp
HEADERS += $${VAULT_BASE}/source/containers/vinstant.h
SOURCES += $${VAULT_BASE}/source/containers/vinstant.cpp
HEADERS += $${VAULT_BASE}/source/containers/vstring.h
SOURCES += $${VAULT_BASE}/source/containers/vstring.cpp
HEADERS += $${VAULT_BASE}/source/files/vabstractfilestream.h
SOURCES += $${VAULT_BASE}/source/files/vabstractfilestream.cpp
HEADERS += $${VAULT_BASE}/source/files/vbufferedfilestream.h
SOURCES += $${VAULT_BASE}/source/files/vbufferedfilestream.cpp
HEADERS += $${VAULT_BASE}/source/files/vdirectiofilestream.h
SOURCES += $${VAULT_BASE}/source/files/vdirectiofilestream.cpp
HEADERS += $${VAULT_BASE}/source/files/vfsnode.h
SOURCES += $${VAULT_BASE}/source/files/vfsnode.cpp
HEADERS += $${VAULT_BASE}/source/server/vclientsession.h
SOURCES += $${VAULT_BASE}/source/server/vclientsession.cpp
HEADERS += $${VAULT_BASE}/source/server/vlistenersocket.h
SOURCES += $${VAULT_BASE}/source/server/vlistenersocket.cpp
HEADERS += $${VAULT_BASE}/source/server/vlistenerthread.h
SOURCES += $${VAULT_BASE}/source/server/vlistenerthread.cpp
HEADERS += $${VAULT_BASE}/source/server/vmanagementinterface.h
HEADERS += $${VAULT_BASE}/source/server/vmessage.h
SOURCES += $${VAULT_BASE}/source/server/vmessage.cpp
HEADERS += $${VAULT_BASE}/source/server/vmessagehandler.h
SOURCES += $${VAULT_BASE}/source/server/vmessagehandler.cpp
HEADERS += $${VAULT_BASE}/source/server/vmessageinputthread.h
SOURCES += $${VAULT_BASE}/source/server/vmessageinputthread.cpp
HEADERS += $${VAULT_BASE}/source/server/vmessageoutputthread.h
SOURCES += $${VAULT_BASE}/source/server/vmessageoutputthread.cpp
HEADERS += $${VAULT_BASE}/source/server/vmessagepostprocessor.h
SOURCES += $${VAULT_BASE}/source/server/vmessagepostprocessor.cpp
HEADERS += $${VAULT_BASE}/source/server/vmessagepreprocessor.h
SOURCES += $${VAULT_BASE}/source/server/vmessagepreprocessor.cpp
HEADERS += $${VAULT_BASE}/source/server/vmessagequeue.h
SOURCES += $${VAULT_BASE}/source/server/vmessagequeue.cpp
HEADERS += $${VAULT_BASE}/source/server/vserver.h
SOURCES += $${VAULT_BASE}/source/server/vserver.cpp
HEADERS += $${VAULT_BASE}/source/server/vmessagesecurity.h
SOURCES += $${VAULT_BASE}/source/server/vmessagesecurity.cpp
HEADERS += $${VAULT_BASE}/source/sockets/vcommsession.h
SOURCES += $${VAULT_BASE}/source/sockets/vcommsession.cpp
HEADERS += $${VAULT_BASE}/source/sockets/vcommsessionclosedevent.h
SOURCES += $${VAULT_BASE}/source/sockets/vcommsessionclosedevent.cpp
HEADERS += $${VAULT_BASE}/source/sockets/vcommsessionenums.h
SOURCES += $${VAULT_BASE}/source/sockets/vcommsessionenums.cpp
HEADERS += $${VAULT_BASE}/source/sockets/vcommsessionevent.h
SOURCES += $${VAULT_BASE}/source/sockets/vcommsessionevent.cpp
HEADERS += $${VAULT_BASE}/source/sockets/vcommsessioneventhandler.h
HEADERS += $${VAULT_BASE}/source/sockets/vcommsessioneventproducer.h
SOURCES += $${VAULT_BASE}/source/sockets/vcommsessioneventproducer.cpp
HEADERS += $${VAULT_BASE}/source/sockets/vcommsessioneventproducerfactory.h
HEADERS += $${VAULT_BASE}/source/sockets/vcommsessionreadevent.h
SOURCES += $${VAULT_BASE}/source/sockets/vcommsessionreadevent.cpp
HEADERS += $${VAULT_BASE}/source/sockets/vcommtypes.h
HEADERS += $${VAULT_BASE}/source/sockets/vrxmessagedispatchhandler.h
SOURCES += $${VAULT_BASE}/source/sockets/vrxmessagedispatchhandler.cpp
HEADERS += $${VAULT_BASE}/source/sockets/vrxmessagereceptionhandler.h
SOURCES += $${VAULT_BASE}/source/sockets/vrxmessagereceptionhandler.cpp
HEADERS += $${VAULT_BASE}/source/sockets/vsessionlifetimemanagementhandler.h
SOURCES += $${VAULT_BASE}/source/sockets/vsessionlifetimemanagementhandler.cpp
HEADERS += $${VAULT_BASE}/source/sockets/vsocketbase.h
SOURCES += $${VAULT_BASE}/source/sockets/vsocketbase.cpp
HEADERS += $${VAULT_BASE}/source/sockets/vsocketfactory.h
SOURCES += $${VAULT_BASE}/source/sockets/vsocketfactory.cpp
HEADERS += $${VAULT_BASE}/source/sockets/vsocketstream.h
SOURCES += $${VAULT_BASE}/source/sockets/vsocketstream.cpp
HEADERS += $${VAULT_BASE}/source/sockets/vsocketthread.h
SOURCES += $${VAULT_BASE}/source/sockets/vsocketthread.cpp
HEADERS += $${VAULT_BASE}/source/sockets/vsocketthreadfactory.h
HEADERS += $${VAULT_BASE}/source/sockets/networkmonitor.h
SOURCES += $${VAULT_BASE}/source/sockets/networkmonitor.cpp
HEADERS += $${VAULT_BASE}/source/streams/vbinaryiostream.h
SOURCES += $${VAULT_BASE}/source/streams/vbinaryiostream.cpp
HEADERS += $${VAULT_BASE}/source/streams/viostream.h
SOURCES += $${VAULT_BASE}/source/streams/viostream.cpp
HEADERS += $${VAULT_BASE}/source/streams/vmemorystream.h
SOURCES += $${VAULT_BASE}/source/streams/vmemorystream.cpp
HEADERS += $${VAULT_BASE}/source/streams/vstream.h
SOURCES += $${VAULT_BASE}/source/streams/vstream.cpp
HEADERS += $${VAULT_BASE}/source/streams/vstreamcopier.h
SOURCES += $${VAULT_BASE}/source/streams/vstreamcopier.cpp
HEADERS += $${VAULT_BASE}/source/streams/vtextiostream.h
SOURCES += $${VAULT_BASE}/source/streams/vtextiostream.cpp
HEADERS += $${VAULT_BASE}/source/streams/vwritebufferedstream.h
SOURCES += $${VAULT_BASE}/source/streams/vwritebufferedstream.cpp
HEADERS += $${VAULT_BASE}/source/threads/vgenerictaskscheduler.h
HEADERS += $${VAULT_BASE}/source/threads/vtaskhandlerproxy.h
HEADERS += $${VAULT_BASE}/source/threads/vtaskscheduler.h
HEADERS += $${VAULT_BASE}/source/threads/vmutex.h
SOURCES += $${VAULT_BASE}/source/threads/vmutex.cpp
HEADERS += $${VAULT_BASE}/source/threads/vmutexlocker.h
SOURCES += $${VAULT_BASE}/source/threads/vmutexlocker.cpp
HEADERS += $${VAULT_BASE}/source/threads/vsemaphore.h
SOURCES += $${VAULT_BASE}/source/threads/vsemaphore.cpp
HEADERS += $${VAULT_BASE}/source/threads/vthread.h
SOURCES += $${VAULT_BASE}/source/threads/vthread.cpp
HEADERS += $${VAULT_BASE}/source/toolbox/vassert.h
SOURCES += $${VAULT_BASE}/source/toolbox/vassert.cpp
HEADERS += $${VAULT_BASE}/source/toolbox/vblockingqueue.h
HEADERS += $${VAULT_BASE}/source/toolbox/vcancellationtoken.h
SOURCES += $${VAULT_BASE}/source/toolbox/vcancellationtoken.cpp
HEADERS += $${VAULT_BASE}/source/toolbox/vcancellationtokensource.h
SOURCES += $${VAULT_BASE}/source/toolbox/vcancellationtokensource.cpp
HEADERS += $${VAULT_BASE}/source/toolbox/vclassregistry.h
SOURCES += $${VAULT_BASE}/source/toolbox/vclassregistry.cpp
HEADERS += $${VAULT_BASE}/source/toolbox/vhex.h
SOURCES += $${VAULT_BASE}/source/toolbox/vhex.cpp
HEADERS += $${VAULT_BASE}/source/toolbox/vlogger.h
SOURCES += $${VAULT_BASE}/source/toolbox/vlogger.cpp
SOURCES += $${VAULT_BASE}/source/toolbox/vmemorytracker.cpp
HEADERS += $${VAULT_BASE}/source/toolbox/vsettings.h
SOURCES += $${VAULT_BASE}/source/toolbox/vsettings.cpp
HEADERS += $${VAULT_BASE}/source/toolbox/vshutdownregistry.h
SOURCES += $${VAULT_BASE}/source/toolbox/vshutdownregistry.cpp
HEADERS += $${VAULT_BASE}/source/toolbox/vsingleton.h
HEADERS += $${VAULT_BASE}/source/toolbox/vhighresolutiontimehelper.h
SOURCES += $${VAULT_BASE}/source/toolbox/vhighresolutiontimehelper.cpp
HEADERS += $${VAULT_BASE}/source/toolbox/stack_crawler.h
SOURCES += $${VAULT_BASE}/source/toolbox/stack_crawler.cpp
HEADERS += $${VAULT_BASE}/source/toolbox/stackwalker.h
SOURCES += $${VAULT_BASE}/source/toolbox/stackwalker.cpp
HEADERS += $${VAULT_BASE}/source/toolbox/vstackcrawler.h
HEADERS += $${VAULT_BASE}/source/toolbox/vutils.h
SOURCES += $${VAULT_BASE}/source/toolbox/vutils.cpp
HEADERS += $${VAULT_BASE}/source/toolbox/vwaittoken.h
SOURCES += $${VAULT_BASE}/source/toolbox/vwaittoken.cpp
HEADERS += $${VAULT_BASE}/source/toolbox/vwaittokensource.h
SOURCES += $${VAULT_BASE}/source/toolbox/vwaittokensource.cpp