#include <machine/riscv/sifive_u/sifiveu_nic.h>
#include <network/ethernet.h>
#include <network/ip.h>
#include <machine/nic.h>
#include <system.h>
#include <memory.h>
#include <machine.h>

__BEGIN_SYS

IP* IP::_ip;

IP* IP::init(NIC<Ethernet> * nic) 
{
    if (!_ip) 
    {
        // Initialize the IP
        _ip = new (SYSTEM) IP(nic);

        _ip->configure_sending_queue();

        // Configure the callback to be called when the NIC receive frames
        _ip->_nic->attach_callback(&class_nic_callback, PROTOCOL);
    }

    return _ip;
}

IP::IP(NIC<Ethernet> * nic) : _nic(reinterpret_cast<SiFiveU_NIC*>(nic))
{ 
    db<IP>(TRC) << "Initializing IP" << endl;

    // Get the IP from the MAC Address
    _address = convert_mac_to_ip_address(_nic->address());
}

void IP::configure_sending_queue() 
{
    // Flag to delete the Callbacks Thread
    _deleted = false;

    // Create the semaphore for the Thread
    _semaphore = new (SYSTEM) Semaphore(0);

    // Create the thread as Ready, it will be blocked in first execution
    _sending_queue_thread = new (SYSTEM) Thread(Thread::Configuration(Thread::READY, Thread::HIGH), &class_sending_queue_function);
}

IP::~IP() {
    // Mark the flag for the SendingThread finish
    _deleted = true;

    // Releases the SendingThread
    _semaphore->v();
}

__END_SYS
