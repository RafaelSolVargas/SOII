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
    // Procura uma entrada na tabela que mapeie o resultado da máscara para um gateway
    // Caso 150.162.60.4 => 150.162.60.0 = 150.162.60.2 => Própria Subnet
    // Caso 150.162.1.0 => 150.162.1.0 = NULL => Default Gateway  
    Routing * routing = get_routing_of_address(dst_addr);

    if (routing) 
    {
        // Para o caso de estar enviando pacote para 150.162.60.4, existe um auto route para a minha própria interface
        // Caso o gateway destino seja o meu próprio address, executo o ARP do endereço destino
        RouterInterface * own_gateway = get_interface_to_same_subnet(routing->gateway()); 
        if (own_gateway)
        {
            return own_gateway->arp()->resolve(dst_addr);
        }
    }

    // routing.gateway() = 150.162.60.1
    routing = get_routing_of_address(Address("0.0.0.0"));

    // Pega uma interface que leve para a mesma subnet que o default gateway
    RouterInterface * default_interface = get_interface_to_same_subnet(routing->gateway());

    // Retorna MAC do Gateway
    return default_interface->arp()->resolve(routing->gateway());
}

RouterInterface* Router::get_interface_to_same_subnet(const Address & dst_addr) 
{
    // If dst_addr == 150.160.60.4 and interface_address = 150.160.60.1 it'll accept 
    for (InterfacesList::Element *el = _interfaces.head(); el; el = el->next()) 
    {
        RouterInterface* interface = el->object();

        if ((interface->interface_address() & Address("255.255.255.0")) == (dst_addr & Address("255.255.255.0"))) 
        {
            return interface;
        }
    }

    return nullptr;
}

Routing* Router::get_routing_of_address(const Address & dst_addr) 
{
    // Delete all paths
    for (RoutingList::Element *el = _routings.head(); el; el = el->next()) 
    {
        Routing* routing = el->object();

        if ((routing->destiny() & Address("255.255.255.0")) == (dst_addr & Address("255.255.255.0"))) 
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
    // Delete all interfaces
    for (RouterInterface::Element *el = _interfaces.head(); el; el = el->next()) 
    {
        RouterInterface* obj = el->object();

        delete obj;
    }

    // Delete all routes
    for (RoutingList::Element *el = _routings.head(); el; el = el->next()) 
    {
        Routing* obj = el->object();

        delete obj;
    }
}

__END_SYS
