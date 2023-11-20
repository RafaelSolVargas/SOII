#include <network/ip_network.h>
#include <network/icmp.h>
#include <network/network.h>

__BEGIN_SYS


Network * Network::_instance;

IPEventsHandler * IPEventsHandler::_instance;

IPEventsHandler::IPEventsHandler(IP * ip) : _ip(ip) 
{
    _icmp = new (SYSTEM) ICMP(ip);
}

IPEventsHandler * IPEventsHandler::init(IP * ip) 
{
    if (!_instance) 
    {
        _instance = new (SYSTEM) IPEventsHandler(ip);
    }

    return _instance;
}

__END_SYS
