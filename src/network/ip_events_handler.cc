#include <network/ip_network.h>
#include <network/icmp.h>

__BEGIN_SYS

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

void IPEventsHandler::process_event(IPEvent event) 
{

}

__END_SYS
