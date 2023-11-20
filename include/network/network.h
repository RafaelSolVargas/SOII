#ifndef __network_h
#define __network_h

#include "network/ip.h"
#include <network/ethernet.h>
#include <utility/random.h>
#include <machine/nic.h>
#include <network/ip.h>
#include <system.h>
#include <time.h>
#include <network/ip_network.h>
#include <network/icmp.h>
#include <network/tcp.h>

__BEGIN_SYS

class Network 
{
public:
    typedef IP::Address Address;
 
    static Network * init() 
    {
        if (!_instance) 
        {
            _instance = new (SYSTEM) Network();
        }

        return _instance;
    }

    ~Network() 
    {
        delete _instance;
    }

    void ping(Address address) 
    {
        _icmp->ping(address);
    }

    IP * ip() { return _ip; }
    TCP * tcp() { return _tcp; }
    SiFiveU_NIC * nic() { return _nic; }

protected:
    Network() 
    {
        _nic = SiFiveU_NIC::init();
        _ip = IP::init(_nic);
        IPEventsHandler * events = IPEventsHandler::init(_ip);
        _tcp = TCP::init(_ip);


        _icmp = events->icmp();
    }

private:
    static Network * _instance;
    IP * _ip;
    TCP * _tcp;
    ICMP * _icmp;
    SiFiveU_NIC * _nic;
};

__END_SYS

#endif
