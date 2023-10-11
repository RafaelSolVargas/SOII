#include <network/ethernet.h>
#include <network/ip.h>
#include <system.h>

__BEGIN_SYS


void IP::send(Address dst, const void * data, unsigned int size) 
{
    db<IP>(TRC) << "IP::send(dst= " << dst << ",size=" << size << ",data=" << data << ")" << endl;

    if (size > MTU_WO_FRAG) 
    {
        send_with_fragmentation(dst, data, size);
    } else 
    {
        send_without_fragmentation(dst, data, size);
    }
}

void IP::send_with_fragmentation(Address dst, const void * data, unsigned int size) 
{
    db<IP>(TRC) << "IP::send_wi_frag(dst= " << dst << ",size=" << size << ",data=" << data << ")" << endl;
}

void IP::send_without_fragmentation(Address dst, const void * data, unsigned int size) 
{
    db<IP>(TRC) << "IP::send_wo_frag(dst= " << dst << ",size=" << size << ",data=" << data << ")" << endl;
 
    // Calculate the required size, adding the space for creating and fragment and a datagram
    unsigned int required_size = size + sizeof(FragmentHeader) + sizeof(Header);

    // Preallocate an buffer to use it for sending the package 
    NIC<Ethernet>::BufferInfo * buffer = _nic->alloc(dst, IP_PROT, required_size);

    // Identifier of the datagram
    unsigned int package_id = get_next_id();

    // Create an fragment without copying the data directly
    Fragment * fragment = new (buffer->data()) Fragment(size, package_id, FragmentFlags::DATAGRAM, 0);

    // Creates the datagram inside the fragment passing all the parameters
    new (fragment->data()) Datagram(_nic->address(), dst, IP_PROT, size, package_id, FragmentFlags::DATAGRAM, data, size);

    _nic->send(buffer);
}

void IP::update(Observed * observed, const Protocol & c, BufferInfo * d)
{
    Fragment * fragment = reinterpret_cast<Fragment *>(d->data());

    db<IP>(TRC) << "IP::receive(frag=" << *fragment << ")" << endl;

    if (fragment->is_datagram()) 
    {
        Datagram* datagram = reinterpret_cast<Datagram *>(fragment->data());

        handle_datagram(datagram);
    } 
    else 
    {
        handle_fragmentation(fragment);
    }

    observed->free(d);
};

void IP::handle_datagram(Datagram * datagram) 
{
    // Instead of this, read the payload from the Datagram
    unsigned int payload = 1500;

    char receivedData[payload];

    memcpy(receivedData, datagram->data(), payload);

    db<IP>(TRC) << "IP::Datagram Data Received= " << endl;

    for (unsigned int index = 0; index < payload; index++) 
    {
        db<IP>(TRC) << receivedData[index];
    }
}

void IP::handle_fragmentation(Fragment * fragment) 
{
    if (fragment->is_last_datagram()) 
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

__END_SYS
