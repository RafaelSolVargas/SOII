#include <network/ethernet.h>
#include <network/ip.h>
#include <system.h>

__BEGIN_SYS

void IP::class_nic_callback(BufferInfo * bufferInfo) 
{
    _ip->nic_callback(bufferInfo);
}

void IP::nic_callback(BufferInfo * bufferInfo) 
{
    Header * fragment = reinterpret_cast<Header *>(bufferInfo->data());

    db<IP>(TRC) << "IP::receive(frag=" << *fragment << ")" << endl;

    if (fragment->flags() == FragmentFlags::DATAGRAM) 
    {
        handle_datagram(fragment);
    } 
    else 
    {
        handle_fragmentation(fragment);
    }
}

void IP::send(const Address & dst, const Protocol & prot, const void * data, unsigned int size) 
{
    db<IP>(TRC) << "IP::send(dst= " << dst << ",size=" << size << ",data=" << data << ")" << endl;

    if (size > Header::FRAGMENT_MTU) 
    {
        send_with_fragmentation(dst, prot, data, size);
    } else 
    {
        send_without_fragmentation(dst, prot, data, size);
    }
}

void IP::send(const Address & dst, const Protocol & prot, NonCBuffer * ncBuffer) 
{   
}

void IP::send_with_fragmentation(const Address & dst, const Protocol & prot, const void * data, unsigned int size) 
{
    db<IP>(TRC) << "IP::send_wi_frag(dst= " << dst << ",size=" << size << ",data=" << data << ")" << endl;
}

void IP::send_without_fragmentation(const Address & dst, const Protocol & prot, const void * data, unsigned int size) 
{
    db<IP>(TRC) << "IP::send_wo_frag(dst= " << dst << ",size=" << size << ",data=" << data << ")" << endl;
 
    // Calculate the required size, adding the space for creating and fragment and a datagram
    unsigned int required_size = size + sizeof(Header);

    // Get the MAC Address of the Address
    MAC_Address dst_mac = convert_ip_to_mac_address(dst);

    // Preallocate an buffer to use it for sending the package 
    NIC<Ethernet>::BufferInfo * buffer = _nic->alloc(dst_mac, PROTOCOL, required_size);

    // Identifier of the datagram
    unsigned int id = get_next_id();

    // Create an datagram 
    new (buffer->data()) Header(_address, dst, prot, required_size, size, id, FragmentFlags::DATAGRAM, 0, data);

    _nic->send(buffer);
}

void IP::handle_datagram(Header * datagram) 
{
    unsigned int payload = datagram->data_size();

    char receivedData[payload];

    memcpy(receivedData, datagram->data_address(), payload);

    db<IP>(TRC) << "IP::Datagram Data Received= " << *datagram << endl;

    for (unsigned int index = 0; index < payload; index++) 
    {
        db<IP>(TRC) << receivedData[index];
    }

    _datagrams_received++;
}

void IP::handle_fragmentation(Header * fragment) 
{
    if (fragment->flags() == FragmentFlags::LAST_FRAGMENT) 
    {

    } 
    else // Mid Fragment
    {

    }
}

unsigned int IP::get_next_id() 
{
    // TODO -> Protect with mutex

    return _datagram_count++;
}

IP::MAC_Address IP::convert_ip_to_mac_address(const Address & address) 
{
    MAC_Address mac;
    mac[0] = 0x56;
    mac[1] = 0x34;
    mac[2] = 0x12;
    mac[3] = 0x0;
    mac[4] = 0x54;

    Address ip;
    ip[0] = 0x44;
    ip[1] = 0x44;
    ip[2] = 0x0;
    ip[3] = 0x1;

    if (address == ip) 
    {
        mac[5] = 0x9;
    } 
    else
    {
        mac[5] = 0x8;
    }

    db<IP>(TRC) << "IP::Converting IP Address=" << address << " to MAC=" << mac << endl;

    return mac;
}

IP::Address IP::convert_mac_to_ip_address(const MAC_Address & address) 
{
    Address ip;
    ip[0] = 0x44;
    ip[1] = 0x44;
    ip[2] = 0x0;

    MAC_Address mac;
    mac[0] = 0x56;
    mac[1] = 0x34;
    mac[2] = 0x12;
    mac[3] = 0x0;
    mac[4] = 0x54;
    mac[5] = 0x9;

    if (address == mac)  
    {
        ip[3] = 0x1;
    } 
    else
    {
        ip[3] = 0x2;
    }

    db<IP>(TRC) << "IP::Converting MAC Address=" << address << " to IP=" << ip << endl;

    return ip;
}

__END_SYS
