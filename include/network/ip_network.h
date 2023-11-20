#ifndef __ip_network_h
#define __ip_network_h

#include "system/config.h"

__BEGIN_SYS

class IP;

class ICMP;

class Network;

class IPEventsHandler 
{
friend class IP;
friend class ICMP;
friend class Network;
public:
    static IPEventsHandler * init(IP * ip);

protected:
    typedef unsigned char IPEvent;
    enum {
        INSUFFICIENT_MEMORY_IN_RECEIVE = 0x1,
        REASSEMBLING_TIMEOUT = 0x2,
        HOST_UNREACHABLE = 0x3,
    };

    static void process_event(IPEvent event);

    ~IPEventsHandler() { }

    ICMP * icmp() { return _icmp; }

protected:
    IPEventsHandler(IP * ip);

private:
    static IPEventsHandler * _instance;
    IP * _ip;
    ICMP * _icmp;
};


__END_SYS

#endif