#include <machine/riscv/sifive_u/sifiveu_nic.h>
#include <network/ethernet.h>
#include <system.h>
#include <memory.h>
#include <machine.h>
#include <time.h>

extern "C" { void _panic(); }

__BEGIN_SYS

SiFiveU_NIC::BufferInfo * SiFiveU_NIC::alloc(const Address & dst, const Protocol & prot, unsigned int payload) 
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
    SiFiveU_NIC::BufferInfo::List buffers;

    // Calculate how many frames are needed to hold the transport PDU and allocate enough buffers
    for (unsigned int size = payload; size > 0; size -= MTU)
    {
        // Busy waiting até conseguir pegar o próximo buffer para fazer o envio 
        unsigned int i = _tx_cur;
        for (bool locked = false; !locked;)
        {
            for (; _tx_ring[i].is_owner(); ++i %= TX_BUFS);

            // Faz Lock no Buffer
            locked = _tx_buffers[i]->lock();
        }

        // Update the tx_current
        _tx_cur = (i + 1) % TX_BUFS;
        
        Tx_Desc* descriptor = &_tx_ring[i];
        Buffer* buffer = _tx_buffers[i];
        
        // Armazena a maior quantidade possível que caiba num Frame, a MTU, caso sobre será alocado no próximo buffer
        // O tamanho do buffer configurado no descriptor será atualizado na função send()
        unsigned long size_sended = (size > MTU) ? MTU : size;

        // Inicializa o Frame, não faz a cópia dos dados pois não tem dado 
        new (buffer->alloc(size_sended)) Frame(_configuration.address, dst, prot);

        db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::alloc:descriptor[" << i << "]=" << descriptor << " => " << *descriptor << endl;
        db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::alloc:buffer=" << buffer << " => " << *buffer << endl;

        // Pass the information of this buffer to the allocatedBuffer and store in the list
        BufferInfo * buffer_info = new (SYSTEM) BufferInfo(buffer, i, size_sended);

        buffers.insert(buffer_info->link());
    }

    return buffers.head()->object();
}

int SiFiveU_NIC::send(SiFiveU_NIC::BufferInfo * buffer_info) 
{
    unsigned int size = 0;

    for (BufferInfo::Element *el = buffer_info->link(); el; el = el->next())
    {
        buffer_info = el->object();
        unsigned long buff_size = buffer_info->size();
        unsigned int index = buffer_info->index();

        // This buffer was locked by the alloc method, will be unlock when the transmit complete
        Buffer* buffer = buffer_info->buffer();
        Tx_Desc* descriptor = &_tx_ring[index];

        db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::send:AllocBuffer=" << buffer_info << " => (" 
                             << "buff=" << buffer << ",index=" << index << ",size=" << buff_size
                             << ")" << endl;

        db<SiFiveU_NIC>(INF) << "::buffer=" << buffer << " => " << *buffer << endl;

        // Atualiza o descritor
        descriptor->update_size(buff_size + sizeof(Header));
        descriptor->ctrl &= ~Tx_Desc::OWNER;
        descriptor->ctrl |= Tx_Desc::LAST;

        // Começa o DMA
        GEM::apply_or_mask(R_NW_CTRL, R_NW_CTRL_B_TX_START);

        size += buff_size;

        _statistics.tx_packets++;
        _statistics.tx_bytes += buff_size;

        db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::send:descriptor=" << *descriptor  << endl;

        // Deleta o BufferInfo
        delete buffer_info;
    }

    return size;
}

int SiFiveU_NIC::send(const Address & dst, const Protocol & prot, const void * data, unsigned int size) 
{
    db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::send(s=" << _configuration.address
                        << ",d=" << dst << ",p=" << hex << prot << dec
                        << ",d=" << data << ",s=" << size << ")" << endl;

    // Busy waiting até conseguir pegar o próximo buffer para fazer o envio 
    unsigned long i = _tx_cur;
    for (bool locked = false; !locked;)
    {
        for (; !(_tx_ring[i].is_owner()); ++i %= TX_BUFS);

        // Lock the buffer to use it
        // The buffer will be unlocked when the transmit finish
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

    // Store the Frame in the CBuffer
    new (buffer->address()) Frame(_configuration.address, dst, prot, data, size);

    // Update descriptor
    descriptor->update_size(total_size);
    descriptor->ctrl &= ~Tx_Desc::OWNER; 
    descriptor->ctrl |= Tx_Desc::LAST;

    db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::TX_DESC[" << i << "] => " << _tx_ring[i] << endl;

    // Start the DMA
    *GEM::reg(R_NW_CTRL) |= R_NW_CTRL_B_TX_START;

    _statistics.tx_packets++;
    _statistics.tx_bytes += size;

    return size;
}

int SiFiveU_NIC::receive(Address * src, Protocol * prot, void * data, unsigned int size) 
{
    db<SiFiveU_NIC>(TRC) << "SiFiveU_NIC::receive(s=" << *src << ",p=" << hex
                        << *prot << dec << ",d=" << data << ",s=" << size
                        << ") => " << endl;

    // Wait for a received frame and seize it
    unsigned int i = _rx_cur;
    for (bool locked = false; !locked;)
    {
        for (; !(_rx_ring[i].is_owner()); ++i %= RX_BUFS);

        locked = _rx_buffers[i]->lock();
    }

    // Update for the next Buffer
    _rx_cur = (i + 1) % RX_BUFS;

    Buffer * buffer = _rx_buffers[i];
    Rx_Desc * descriptor = &_rx_ring[i];

    db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::receive:descriptor[" << i << "]=" << descriptor
                         << " => " << *descriptor << endl;

    // The internal protocol is to set the data always in the address() of CBuffer
    Frame *frame = reinterpret_cast<Frame*>(buffer->address());
    *src = frame->src();
    *prot = frame->prot();

    unsigned long frame_size = descriptor->frame_size();

    // Copy the data
    memcpy(data, frame->data<void>(), size);

    _statistics.rx_packets++;
    _statistics.rx_bytes += frame_size;

    descriptor->clear_after_received();

    buffer->unlock();

    return size;
}

void SiFiveU_NIC::receive() 
{
    for (unsigned int count = RX_BUFS, i = _rx_cur; 
                    count && (_rx_ring[i].is_owner()); 
                    count--, ++i %= RX_BUFS, _rx_cur = i) 
    {
        // Lock the buffer and only unlock in the free(Buffer *) 
        if (_rx_buffers[i]->lock()) 
        {
            Rx_Desc * descriptor = &_rx_ring[i];
            Buffer * buffer = _rx_buffers[i];
            
            // The internal protocol is to set the data always in the address() of CBuffer
            Frame * frame = reinterpret_cast<Frame*>(buffer->address());

            db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::receive::desc[" << i << "] => " << *descriptor << endl;

            // Build the buffer info to pass to higher layers
            unsigned long frame_size = descriptor->frame_size();
            BufferInfo * buffer_info = new (SYSTEM) BufferInfo(buffer, i, frame_size); 

            // Update the data address to remove the Header
            buffer_info->shrink_left(sizeof(Header));

            // Remove the size considered by the bits of FCS 
            if (BRING_FCS_TO_MEM) {
                buffer_info->shrink(sizeof(CRC));
            }
            
            // Caso estejamos recebendo um buffer que veio da mesma máquina, liberar diretamente
            if (frame->header()->src() == _configuration.address) 
            {
                db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::receive::desc[" << i << "] => SRC == DST, releasing frame " << endl;

                free(buffer_info);

                continue;
            }

            _statistics.rx_packets++;
            _statistics.rx_bytes += frame_size;

            // Se ninguém foi notificado descarta o buffer, caso contrário os observadores tem a responsabilidade de chamar o free 
            bool someone_was_notified = notify(frame->prot(), buffer_info);
            if (!someone_was_notified) 
            {
                free(buffer_info);
            } 
        }
    }
}

bool SiFiveU_NIC::free(BufferInfo * buffer_info) 
{
    // Recupera o buffer e o descritor
    unsigned int index = buffer_info->index();

    Buffer* buffer = buffer_info->buffer();
    Rx_Desc* descriptor = &_rx_ring[index];

    descriptor->clear_after_received();

    // Libera o lock do buffer Rx
    buffer->unlock();

    db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::receive::desc[" << index << "] released => " << descriptor << " => " << descriptor->is_owner() << endl;

    return true;  
} 

void SiFiveU_NIC::interruption_handler(unsigned int interrupt) 
{
    db<SiFiveU_NIC, IC>(TRC) << "SiFiveU_NIC::interruption(int=" << interrupt << ")" << endl;

    if (!_device) {
        return;
    }

    _device->handle_interruption();
}

void SiFiveU_NIC::handle_interruption() 
{
    // Read the ISR register, the bits read will be automatically cleared by the hardware
    Reg32 interruptionReg = GEM::reg_value(R_INT_STATUS);

    db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::int_handler(): R_INT_STATUS=" << hex << interruptionReg << endl;

    // Transmit finished 
    if (interruptionReg & R_INT_STATUS_B_TX_COMPLETE) 
    {
        // Get the buffer that was sended using the last buffers that was unlocked
        unsigned int i = (_tx_last_unlocked + 1) % TX_BUFS;

        Tx_Desc* descriptor = &_tx_ring[i];
        Buffer* buffer = _tx_buffers[i];

        descriptor->clear_after_send();

        buffer->unlock();

        db<SiFiveU_NIC>(INF) << "SiFiveU_NIC transmit completed in desc[" << i << "] => " << *descriptor << endl;

        // Write 1 to signal that the transmit was finished
        *GEM::reg(R_TRANSMIT_STATS) |= R_TRANSMIT_STATS_B_TX_COMPLETE;
    }

    // One or more frames was received and stored in our buffers
    if (interruptionReg & R_INT_STATUS_B_RX_COMPLETE) 
    {
        receive();

        // Write 1 to these bits to set that the receive was finished
        GEM::reg_value(R_INT_STATUS) = R_INT_STATUS_B_RX_COMPLETE;
        GEM::reg_value(R_RECEIVE_STATS) = R_RECEIVE_STATS_B_RX_COMPLETE;
    }

}

void SiFiveU_NIC::address(const Address &) 
{

}

bool SiFiveU_NIC::reconfigure(const Configuration * c) 
{
    return true;
} 

__END_SYS
