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
            for (; _tx_ring[i].ctrl & Tx_Desc::OWNER_IS_NIC; ++i %= TX_BUFS);

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

        Buffer* buffer = buffer_info->buffer();
        Tx_Desc descriptor = _tx_ring[index];

        db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::send:AllocBuffer=" << buffer_info << " => (" 
                             << "buff=" << buffer << ",index=" << index << ",size=" << buff_size
                             << ")" << endl;

        db<SiFiveU_NIC>(INF) << "::buffer=" << buffer << " => " << *buffer << endl;

        // Atualiza o descritor
        descriptor.update_size(buff_size + sizeof(Header));
        descriptor.ctrl &= ~Tx_Desc::OWNER_IS_NIC;

        // Começa o DMA
        GEM::apply_or_mask(R_NW_CTRL, R_NW_CTRL_B_TX_START);

        size += buff_size;

        _statistics.tx_packets++;
        _statistics.tx_bytes += buff_size;

        db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::send:descriptor=" << descriptor << endl;

        // Deleta o BufferInfo
        delete buffer_info;

        // Espera o bit OWNER_IS_NIC ser verdadeiro, ou seja, o DMA terminar
        while (!(descriptor.ctrl & Tx_Desc::OWNER_IS_NIC));

        // Unlock the buffer that was locked in the alloc method
        buffer->unlock();
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
        for (; !(_tx_ring[i].ctrl & Tx_Desc::OWNER_IS_NIC); ++i %= TX_BUFS);

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
    descriptor->ctrl &= ~Tx_Desc::OWNER_IS_NIC; 
    descriptor->update_size(total_size);
    descriptor->ctrl |= Tx_Desc::LAST;

    // Start the DMA
    GEM::apply_or_mask(R_NW_CTRL, R_NW_CTRL_B_TX_START);

    _statistics.tx_packets++;
    _statistics.tx_bytes += size;

    _tx_buffers[i]->unlock();

    return size;
}

void SiFiveU_NIC::receive() 
{
    // Passa, começando pelo Buffer em _rx_current por no máximo RX_BUFS buffers, utiliza um contador decrescente para 
    // limitar o máximo de loops
    for(unsigned int count = RX_BUFS, i = _rx_cur; 
                    count && !(_rx_ring[i].ctrl & Rx_Desc::OWNER_IS_NIC); 
                    count--, ++i %= RX_BUFS, _rx_cur = i) 
    {
        if(_rx_buffers[i]->lock()) // Trava o Buffer para tratar o receive 
        {
            Rx_Desc * descriptor = &_rx_ring[i];
            Buffer * buffer = _rx_buffers[i];
            // TODO -> Adicionar um parâmetro data_address dentro do Buffer e sempre que ocorrer um alloc dentro do Buffer
            // guardar o endereço alocado, para que aqui eu consiga o dado alocado, por meio desse address e usando um data() com template
            Frame * frame = 0;

            // Tomando como base que o payload do Frame é o último dado, podemos atualizar o data_address do buffer, aumentando em sizeof
            // (Header) para que os observadores dessa NIC ao chamar data() consiga fazer o cast direto para o dado que ele enviou.

            // Permitir também possibilidade de diminuir o tamanho, para cortar, caso necessário, dados após o payload, como o CRC.

            db<SiFiveU_NIC>(TRC) << "SiFiveU_NIC::receive: frame = " << *frame << endl;
            db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::handle_int: descriptor[" << i << "] = " << descriptor << " => " << *descriptor << endl;

            // TODO -> Ler o size do descriptor e passar para cá
            BufferInfo * buffer_info = new (SYSTEM) BufferInfo(buffer, i, 0); 

            // Caso estejamos recebendo um buffer que veio de nós, liberar diretamente
            if (frame->header()->src() == _configuration.address) 
            {
                free(buffer_info);

                continue;
            }

            // Se ninguém foi notificado descarta o buffer, caso contrário os observadores tem a responsabilidade de chamar o free 
            bool someone_was_notified = notify(frame->prot(), buffer_info);
            if(!someone_was_notified) 
            {
                free(buffer_info);
            } 
        }
    }
}

int SiFiveU_NIC::receive(Address * src, Protocol * prot, void * data, unsigned int size) 
{
    // Ainda é necessário verificar onde esse método será chamado

    // TODO -> Verificar como remover o Header que foi recebido dentro desse Buffer
    // Isso é o cabeçalho dessa camada e não deve ser passada para as camadas superiores
    // E essa remoção também precisa ser feita logo antes de chamar o notify, onde quer que ele vá ficar

    return 1;
}

bool SiFiveU_NIC::free(BufferInfo * buffer_info) 
{
    // Esse método precisa limpar um buffer que possuía algum dado que foi recebido
    // e passado para os observadores e após eles utilizarem estão liberando
    // Esse buffer nesse meio tempo precisa de algum tipo de proteção para dizer que já foi recebido
    // pelo tratador de interrupções mas não foi liberado ainda

    unsigned long buff_size = buffer_info->size();
    unsigned int index = buffer_info->index();

    Buffer* buffer = buffer_info->buffer();
    Rx_Desc descriptor = _rx_ring[index];

    _statistics.rx_packets++;
    _statistics.rx_bytes += buff_size;

    // Libera o lock do buffer Rx, pode estar preso desde que foi entendido como que recebeu alguma coisa
    buffer->unlock();

    // Volta o tamanho para o original e marca o buffer como propriedade do controller agora
    descriptor.update_size(sizeof(Frame));
    descriptor.ctrl &= ~Rx_Desc::OWNER_IS_NIC; // TODO -> Verificar isso

    return true;  
} 

void SiFiveU_NIC::interruption_handler() 
{
    // Esse método precisa acessar o registrador ISR e analisar o status

    // Verificar se é um pacote chegando de rede, observar o bit 1 do registrador

    // Caso vá seguir o padrão de interrupção de TX, caso o DMA tenha terminado, verificar também se é esse tipo de interrupção
    // Ao mesmo tempo alinhar junto com a função send() para liberar alguma coisa que ela precise
}

void SiFiveU_NIC::address(const Address &) 
{

}

bool SiFiveU_NIC::reconfigure(const Configuration * c) 
{
    return true;
} 

__END_SYS
