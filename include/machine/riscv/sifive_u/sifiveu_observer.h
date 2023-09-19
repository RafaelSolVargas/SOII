#ifndef __si_five_u_observer_h
#define __si_five_u_observer_h

#include <machine/riscv/sifive_u/sifiveu_nic.h>
#include <network/ethernet.h>
#include <utility/list.h>

__BEGIN_SYS

class NIC_Observer: public Ethernet::Observer
{
private:
    typedef Ethernet::BufferInfo BufferInfo;
    typedef Ethernet::Protocol Protocol;
    typedef Ethernet::Observed Observed;

public:
    NIC_Observer() {}

    void update(Observed * observed, const Protocol & c, BufferInfo * d) override
    {
        observed->free(d);
    };
};


__END_SYS

#endif