#include <network/ethernet.h>
#include <network/ip.h>
#include <system.h>

__BEGIN_SYS

void IP::update(Observed * observed, const Protocol & c, BufferInfo * d)
{
    Fragment * fragment = reinterpret_cast<Fragment *>(d->data());

    db<IP>(TRC) << "IP::receive(frag=" << *fragment << ")" << endl;

    char dataOne[Fragment::DATA_SIZE];

    memcpy(dataOne, fragment->data(), Fragment::DATA_SIZE);

    db<IP>(TRC) << "IP::Data Received= " << endl;

    for (unsigned int byteIndex = 0; byteIndex < Fragment::DATA_SIZE; byteIndex++) {
        db<IP>(TRC) << dataOne[byteIndex];
    }

    observed->free(d);
};

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
 
    // Calculate the required size, including the space for creating and fragment and a datagram
    unsigned int required_size = size + sizeof(FragmentHeader) + sizeof(Header);

    NIC<Ethernet>::BufferInfo * buffer = _nic->alloc(dst, IP_PROT, required_size);

    // Identifier of the datagram
    unsigned int package_id = get_next_id();

    db<IP>(TRC) << "IP::Creating Fragment in = " << buffer->data() << endl;
    db<IP>(TRC) << "IP::Buffer size = " << buffer->size() << endl;

    // Create an fragment without copying the data directly
    Fragment * fragment = new (buffer->data()) Fragment(size, package_id, FragmentFlags::DATAGRAM, 0);

    db<IP>(TRC) << "IP::Creating Datagram in = " << fragment->data() << endl;
    db<IP>(TRC) << "IP::Fragment size = " << fragment->FRAGMENT_SIZE << endl;
    db<IP>(TRC) << "IP::Fragment Data size = " << fragment->DATA_SIZE << endl;

    // Creates an empty datagram to use the placeholder for the data
    Datagram * datagram = new (fragment->data()) Datagram();

    db<IP>(TRC) << "IP::Copying the data to = " << datagram->data() << endl;
    
    // Copy the data into the datagram
    memcpy(datagram->data(), data, size);

    _nic->send(buffer);

    _nic->send(dst, IP_PROT, fragment, size);
}


/* void send_without_fragmentation(Address dst, const void * data, unsigned int size) 
{
    db<IP>(TRC) << "IP::send_wo_frag(dst= " << dst << ",size=" << size << ",data=" << data << ")" << endl;
 
    // Identifier of the datagram
    unsigned int package_id = get_next_id();

    Fragment * fragment = new (SYSTEM) Fragment(size, package_id, FragmentFlags::DATAGRAM, 0, data, size);

    _nic->send(dst, IP_PROT, fragment, size);
} */

unsigned int IP::get_next_id() 
{
    // TODO -> Protect with mutex

    return _datagram_count++;
}

__END_SYS
