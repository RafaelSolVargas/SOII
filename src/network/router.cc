#include <network/ip.h>

__BEGIN_SYS

void Router::populate_paths_table(const MAC_Address & mac_addr) 
{
    // Sender
    if (mac_addr == MAC_Address("86:52:18:0:84:9")) 
    {
        // Creates interface with Network One
        _interfaces.insert((new (SYSTEM) RouterInterface(_nic, _arp, Address("150.162.60.2")))->link());

        // Default Route
        _routings.insert((new (SYSTEM) Routing(Address("0.0.0.0"), Address("150.162.60.1")))->link());

        // Auto Route
        _routings.insert((new (SYSTEM) Routing(Address("150.162.60.0"), Address("150.162.60.2")))->link());
    }
    // Receiver
    else if (mac_addr == MAC_Address("86:52:18:0:84:8")) 
    {
        // Creates interface with Network Two
        _interfaces.insert((new (SYSTEM) RouterInterface(_nic, _arp, Address("150.162.1.50")))->link());

        // Default Route
        _routings.insert((new (SYSTEM) Routing(Address("0.0.0.0"), Address("150.162.1.1")))->link());

        // Auto Route
        _routings.insert((new (SYSTEM) Routing(Address("150.162.1.0"), Address("150.162.1.50")))->link());
    } 
    // Gateway
    else if (mac_addr == MAC_Address("86:52:18:0:84:7")) 
    {
        // Interface with Network One
        _interfaces.insert((new (SYSTEM) RouterInterface(_nic, _arp, Address("150.162.60.1")))->link());

        // Interface with Network Two
        _interfaces.insert((new (SYSTEM) RouterInterface(_nic, _arp, Address("150.162.1.60")))->link());

        // Default route
        _routings.insert((new (SYSTEM) Routing(Address("0.0.0.0"), Address("150.162.1.1")))->link());

        // Auto Route Network One
        _routings.insert((new (SYSTEM) Routing(Address("150.162.60.0"), Address("150.162.60.1")))->link());

        // Auto Route Network Two
        _routings.insert((new (SYSTEM) Routing(Address("150.162.1.0"), Address("150.162.1.60")))->link());
    }
}


Router::MAC_Address Router::route(const Address & dst_addr) 
{
    Address next_host_addr = define_actual_destination_address(dst_addr);

    return _arp->resolve(next_host_addr);
}

Router::Address Router::define_actual_destination_address(const Address & dst) 
{
    // Check if the destiny address is in any sub network that we have an interface
    for (InterfacesList::Element *el = _interfaces.head(); el; el = el->next()) 
    {
        RouterInterface* interface = el->object();

        Address address = interface->interface_address();

        bool is_same_sub_network = (address[0] == dst[0] && address[1] == dst[1] && address[2] == dst[2]);

        if (is_same_sub_network) 
        {
            return address;
        }
    }
    
    // When called by the sender, will return 150.162.60.1, one interface of the gateway
    // When called by the receiver, will return 150.162.1.1, one interface of the gateway
    return get_routing_of_address(Address("0.0.0.0"))->gateway();   
}

Routing* Router::get_routing_of_address(const Address & dst_addr) 
{
    // Delete all paths
    for (RoutingList::Element *el = _routings.head(); el; el = el->next()) 
    {
        Routing* routing = el->object();

        if (routing->destiny() == dst_addr) 
        {
            return routing;
        }
    }

    return nullptr;
}

Router::MAC_Address Router::convert_ip_to_mac_address(const Address & address) 
{
    // :7 -> Gateway -> 60.1
    // :8 -> Receiver -> .1.50
    // :9 -> Sender -> .60.2
    
    // Sender
    if (address == Address("150.162.60.2")) 
    {
        return MAC_Address("86:52:18:0:84:9");
    } 
    // Receiver
    else if (address == Address("150.162.1.50"))
    {
        return MAC_Address("86:52:18:0:84:8");
    } 
    // Gateway
    else if (address == Address("150.162.60.1")) 
    {
        return MAC_Address("86:52:18:0:84:7");
    }
    else 
    {
        return MAC_Address("0:0:0:0:0:0");
    }
}

Router::Address Router::convert_mac_to_ip_address(const MAC_Address & address) 
{
    // :7 -> Gateway -> 60.1
    // :8 -> Receiver -> .1.50
    // :9 -> Sender -> .60.2

    // Sender
    if (address == MAC_Address("86:52:18:0:84:9")) 
    {
        return Address("150.162.60.2");
    } 
    // Receiver
    else if (address == MAC_Address("86:52:18:0:84:8"))
    {
        return Address("150.162.1.50");
    }
    // Gateway
    else if (address == MAC_Address("86:52:18:0:84:7")) 
    {
        // Returns the IP Address of the Network with the Sender, this value should not be considered and the Table
        // must be used when defining the destination IP address
        return Address("150.162.60.1");
    } 
    else 
    {
        return Address("0.0.0.0");
    }
}

Router::~Router() 
{
    // Delete all paths
    for (RouterInterface::Element *el = _interfaces.head(); el; el = el->next()) 
    {
        RouterInterface* path = el->object();

        delete path;
    }
}

__END_SYS
