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

    // Datagram * datagram = new (SYSTEM) Datagram(data, size);
    // TODO -> Set variables of the Datagram

    //db<IP>(TRC) << "IP::id= " << id << endl;

    // db<IP>(TRC) << "IP::datagram= " << *datagram << endl;

    // Total length of the datagram, the data stored in the Fragment, including the Header
    //unsigned int total_length = datagram->size();

    //db<IP>(TRC) << "IP::total_length= " << total_length << endl;

    Fragment * fragment = new (SYSTEM) Fragment(size, id, FragmentHeader::DATAGRAM, 0, data, size);

    db<IP>(TRC) << "IP::frag_size= " << Fragment::DATA_SIZE << endl;
    
    db<IP>(TRC) << "IP::fragment= " << *fragment << endl;

    _nic->send(dst, IP_PROT, fragment, Fragment::DATA_SIZE);
}

unsigned int IP::get_next_id() 
{
    // TODO -> Protect with mutex

    return _datagram_count++;
}

__END_SYS
