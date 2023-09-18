#include <machine/riscv/sifive_u/sifiveu_nic.h>
#include <network/ethernet.h>
#include <system.h>
#include <memory.h>
#include <machine.h>
#include <time.h>

extern "C" { void _panic(); }

__BEGIN_SYS

SiFiveU_NIC::AllocBufferInfo * SiFiveU_NIC::alloc(const Address & dst, const Protocol & prot, unsigned int payload) 
{
    db<SiFiveU_NIC>(TRC) << "SiFiveU_NIC::alloc(s=" << _configuration.address
                            << ",d=" << dst << ",p=" << hex << prot << dec
                            << ",pl=" << payload << ")" << endl;

    if (payload / MTU > TX_BUFS)
    {
        db<SiFiveU_NIC>(WRN) << "SiFiveU_NIC::alloc: sizeof(Network::Packet::Data) > ""sizeof(NIC::Frame::Data) * TX_BUFS!" << endl;
        
        return 0;
    }

    // Create the list of AllocatedBuffers to return them
    SiFiveU_NIC::AllocBufferInfo::List buffers;

    // Calculate how many frames are needed to hold the transport PDU and allocate enough buffers
    for (unsigned int size = payload; size > 0; size -= MTU)
    {
        // Busy waiting até conseguir pegar o próximo buffer para fazer o envio 
        unsigned int i = _tx_cur;
        for (bool locked = false; !locked;)
        {
            for (; _tx_ring[i].ctrl & Tx_Desc::OWNER; ++i %= TX_BUFS);

            locked = _tx_buffers[i]->lock();
        }

        // Update the tx_current
        _tx_cur = (i + 1) % TX_BUFS;
        
        Tx_Desc* descriptor = &_tx_ring[i];
        Buffer* buffer = _tx_buffers[i];
        
        // Armazena a maior quantidade possível que caiba num Frame, a MTU, caso sobre será alocado no próximo buffer
        unsigned long size_sended = (size > MTU) ? MTU : size;

        // Inicializa o Frame, não faz a cópia dos dados pois não tem dado 
        new (buffer->alloc(size_sended)) Frame(_configuration.address, dst, prot);

        db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::alloc:descriptor[" << i << "]=" << descriptor << " => " << *descriptor << endl;
        db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::alloc:buf=" << buffer << " => " << *buffer << endl;

        // Pass the information of this buffer to the allocatedBuffer and store in the list
        AllocBufferInfo * buffer_info = new (SYSTEM) AllocBufferInfo(buffer, i, size_sended);

        buffers.insert(buffer_info->link());
    }

    return buffers.head()->object();
}

int SiFiveU_NIC::send(SiFiveU_NIC::AllocBufferInfo * buffer_info) 
{
    unsigned int size = 0;

    for (AllocBufferInfo::Element *el = buffer_info->link(); el; el = el->next())
    {
        buffer_info = el->object();
        unsigned long buff_size = buffer_info->size();
        unsigned int index = buffer_info->index();

        Buffer* buffer = buffer_info->buffer();
        Tx_Desc descriptor = _tx_ring[index];

        db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::send:AllocBuffer=" << buffer_info << " => (" 
                             << "buff=" << buffer << ",index=" << index << ",size=" << buff_size
                             << ")" << endl;

        db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::send:buf=" << buffer << " => " << *buffer << endl;

        // Atualiza o descritor
        descriptor.update_size(buff_size + sizeof(Header));
        descriptor.ctrl &= ~Tx_Desc::OWNER;

        // Começa o DMA
        GEM::apply_or_mask(R_NW_CTRL, R_NW_CTRL_B_TX_START);

        size += buff_size;

        _statistics.tx_packets++;
        _statistics.tx_bytes += buff_size;

        db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::send:descriptor=" << descriptor << endl;

        // Deleta o AllocBufferInfo
        delete buffer_info;

        // Espera o DMA terminar, o unlock do buffer será chamado pelo handle_int
        while (!(descriptor.ctrl & Tx_Desc::OWNER));
    }

    return size;
}

int SiFiveU_NIC::send(const Address & dst, const Protocol & prot, const void * data, unsigned int size) 
{
    db<SiFiveU_NIC>(TRC) << "SiFiveU_NIC::send(s=" << _configuration.address
                        << ",d=" << dst << ",p=" << hex << prot << dec
                        << ",d=" << data << ",s=" << size << ")" << endl;

    // Busy waiting até conseguir pegar o próximo buffer para fazer o envio 
    unsigned long i = _tx_cur;
    for (bool locked = false; !locked;)
    {
        for (; !(_tx_ring[i].ctrl & Tx_Desc::OWNER); ++i %= TX_BUFS);

        locked = _tx_buffers[i]->lock();
    }

    // Atualiza o próximo tx_current    
    _tx_cur = (i + 1) % TX_BUFS; 

    Tx_Desc* descriptor = &_tx_ring[i];
    Buffer* buffer = _tx_buffers[i];

    // Calculate the size of buffer required
    unsigned long total_size = size + sizeof(Header);

    // Verify the total_size is more than what is available in the CBuffer
    if (total_size > BUFF_SIZE) 
    {
        db<System>(ERR) << "SiFiveU_NIC::send(this=" << this << "): Tried to send frame with " 
                            << total_size << " bytes. Maximum size is: " << BUFF_SIZE << " bytes!" << endl;

        _panic();
    }

    db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::send:Buffer[" << i << "]=" << buffer << " => " << *buffer << endl;

    // Store the Frame in the CBuffer
    new (buffer->alloc(total_size)) Frame(_configuration.address, dst, prot, data, size);

    // Update descriptor
    descriptor->ctrl &= ~Tx_Desc::OWNER; 
    descriptor->update_size(total_size);
    descriptor->ctrl |= Tx_Desc::LAST;

    // Start the DMA
    GEM::apply_or_mask(R_NW_CTRL, R_NW_CTRL_B_TX_START);

    _statistics.tx_packets++;
    _statistics.tx_bytes += size;

    _tx_buffers[i]->unlock();

    return size;
}

int SiFiveU_NIC::receive(Address * src, Protocol * prot, void * data, unsigned int size) 
{
    return 1;
}

void SiFiveU_NIC::address(const Address &) 
{

}

bool SiFiveU_NIC::reconfigure(const Configuration * c) 
{
    return true;
} 

__END_SYS
