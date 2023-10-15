#include <machine/riscv/sifive_u/sifiveu_nic.h>
#include <network/ethernet.h>
#include <system.h>

__BEGIN_SYS

void SiFiveU_NIC::attach_callback(void (*callback)(BufferInfo *), const Protocol & prot) 
{
    CallbacksWrapper* callbackWrapper = new (SYSTEM) CallbacksWrapper(callback, prot);

    _callbacks.insert(callbackWrapper->link());
}

void SiFiveU_NIC::configure_callbacks() 
{
    // Flag to delete the Callbacks Thread
    _deleted = false;

    // Create the semaphore for the Thread
    _semaphore = new (SYSTEM) Semaphore(0);

    // Create the thread as Ready, it will be blocked in first execution
    _callbacks_caller_thread = new (SYSTEM) Thread(Thread::Configuration(Thread::READY, Thread::HIGH), &class_callbacks_handler);
}

int SiFiveU_NIC::class_callbacks_handler() {
    return get()->callbacks_handler();
}

int SiFiveU_NIC::callbacks_handler() 
{
    db<SiFiveU_NIC>(INF) << "SiFiveU_NIC initializing the CallbackThread" << endl;

    while (!_deleted) 
    {
        db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::CallbackThread => Waiting for next package" << endl;

        _semaphore->p();

        // Index of the next buffer that was received, will not use the _rx_cur because many packages can arrive 
        // before the code reaches here
        unsigned int i = _rx_callback_cur %= RX_BUFS;
        
        _rx_callback_cur++;

        Rx_Desc * descriptor = &_rx_ring[i];
        Buffer * buffer = _rx_buffers[i];

        Frame* frame = reinterpret_cast<Frame*>(buffer->address());
        
        db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::CallbackThread::Preparing package in RX_DESC[" << i << "]"<< endl;

        // Build the buffer info to pass to higher layers
        unsigned long frame_size = descriptor->frame_size();
        BufferInfo * buffer_info = new (SYSTEM) BufferInfo(buffer, i, frame_size); 

        // Update the data address to remove the Header
        buffer_info->shrink_left(sizeof(Header));

        // Remove the size considered by the bits of FCS 
        if (BRING_FCS_TO_MEM) 
        {
            buffer_info->shrink(sizeof(CRC));
        }

        // Call all callbacks that was registered in the NIC
        for (CallbacksWrapper::Element *el = _callbacks.head(); el; el = el->next()) 
        {
            CallbacksWrapper* wrapper = el->object();

            if (wrapper->protocol() == frame->prot()) 
            {
                db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::CallbackThread::Sending RX_DESC[" << i << "]" << endl;

                wrapper->callCallback(buffer_info);
            }
        }

        // Releases the RX buffer 
        free(buffer_info);
    }

    return 1;
}


__END_SYS
