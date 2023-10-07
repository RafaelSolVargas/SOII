#include <network/ethernet.h>
#include <network/ip.h>
#include <system.h>

__BEGIN_SYS

void IP::update(Observed * observed, const Protocol & c, BufferInfo * d)
{
    Fragment * fragment = d->data<Fragment>();

    db<IP>(TRC) << "IP::size_incoming= " << d->size() << endl;

    unsigned int frag_size = fragment->size();

    db<IP>(TRC) << "IP::frag_size= " << frag_size << endl;

    db<IP>(TRC) << "IP::receive(frag=" << *fragment << ")" << endl;

    observed->free(d);
};

void IP::send(Address dst, const void * data, unsigned int size) 
{
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

    char dataOne[size];

    memcpy(dataOne, data, size);

    db<IP>(TRC) << "IP::Sending data= " << dataOne << endl;

    // Identifier of the datagram
    unsigned int id = get_next_id();

    Datagram * datagram = new (SYSTEM) Datagram(data, size);
    // TODO -> Set variables of the Datagram

    db<IP>(TRC) << "IP::id= " << id << endl;

    db<IP>(TRC) << "IP::datagram= " << *datagram << endl;

    // Total length of the datagram, the data stored in the Fragment, including the Header
    unsigned int total_length = datagram->size();

    db<IP>(TRC) << "IP::total_length= " << total_length << endl;

    Fragment * fragment = new (SYSTEM) Fragment(total_length, id, 1, 0, datagram, total_length);

    unsigned int frag_size = fragment->size();

    db<IP>(TRC) << "IP::frag_size= " << frag_size << endl;
    
    db<IP>(TRC) << "IP::fragment= " << *fragment << endl;

    Datagram * datagramTwo = fragment->data<Datagram>();

    db<IP>(TRC) << "IP::datagram= " << *datagramTwo << endl;

    char * dataTwo = datagramTwo->data<char>();

    db<IP>(TRC) << "IP::datagram data= " << *dataTwo << endl;

    _nic->send(dst, IP_PROT, fragment, frag_size);
}

unsigned int IP::get_next_id() 
{
    // TODO -> Protect with mutex

    return _datagram_count++;
}

__END_SYS
