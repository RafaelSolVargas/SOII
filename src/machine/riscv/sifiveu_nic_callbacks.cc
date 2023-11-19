#include <machine/riscv/sifive_u/sifiveu_nic.h>
#include <network/ethernet.h>
#include <system.h>

__BEGIN_SYS

void SiFiveU_NIC::attach_callback(void (*callback)(BufferInfo *), const Protocol & prot) 
{
    NICCallbacksWrapper* callbackWrapper = new (SYSTEM) NICCallbacksWrapper(callback, prot);

    _callbacks.insert(callbackWrapper->link());
}

void SiFiveU_NIC::configure_callbacks() 
{
    db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::ConfiguringCallbacks()" << endl;

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

        if (_deleted) break;

        
        
        // Index of the next buffer that was received, will not use the _rx_cur because many packages can arrive before the code reaches here
        // But, if some package was dropped due to 1) callbacks in empty or 2) the sender is the receiver, the _rx_callback_cur will not be updated
        // must check again here in the callback handler each packages until reach to the one that is ours
        unsigned int i = 0;
        unsigned int jumps = 0;
        while (true) 
        {
            i = _rx_callback_cur %= RX_BUFS;
    
            _rx_callback_cur++;

            // Has found a package to process
            if (_rx_ring[i].is_owner()) 
            {
                break;
            }

            jumps++;

            if (jumps >= RX_BUFS) 
            {
                db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::CallbackThread => Could not found a frame to process" << endl;
            
                continue;
            }

            db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::CallbackThread => Jumping one buffer that was dropped" << endl;
        }

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
        for (NICCallbacksWrapper::Element *el = _callbacks.head(); el; el = el->next()) 
        {
            NICCallbacksWrapper* wrapper = el->object();

            if (wrapper->protocol() == frame->prot()) 
            {
                db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::CallbackThread::Sending RX_DESC[" << i << "]" << endl;

                wrapper->_callback(buffer_info);

                db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::CallbackThread::Callback finished RX_DESC[" << i << "]" << endl;
            }
        }

        // Releases the RX buffer 
        free(buffer_info);
    }

    db<SiFiveU_NIC>(INF) << "SiFiveU_NIC::~CallbackThread()" << endl;

    return 1;
}


__END_SYS
