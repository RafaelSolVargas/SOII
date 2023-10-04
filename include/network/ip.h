#ifndef __ip_layer_h
#define __ip_layer_h

#include <machine/nic.h>
#include <network/ethernet.h>
#include <utility/list.h>

__BEGIN_SYS

class IP: public NIC<Ethernet>::Observer
{
private:
    /// @brief Buffer received by the NIC
    typedef Ethernet::BufferInfo BufferInfo;
    typedef Ethernet::Protocol Protocol;
    typedef Ethernet::Observed Observed;
    typedef Ethernet::Address Address;
\
    // IP Fragment created by the Fragmentation process
    class Fragment
    {
    public:
        Fragment() {}
        Fragment(const unsigned int id, const unsigned int flags, const unsigned int offset) 
            : _id(id), _flags(flags), _offset(offset) { }

        bool more_fragments() { return _flags == Flags::MF; }
        bool not_fragment() { return _flags == Flags::DF; }

        unsigned int id() { return _id; }
        unsigned int offset() { return _offset; }

        friend Debug & operator<<(Debug & db, const Fragment & f) {
            db << "{id=" << f._id << ",i=" << f._offset << "}";
            
            return db;
        }

    protected:
        unsigned int _id;
        unsigned int _flags;
        unsigned int _offset;
    } __attribute__((packed, may_alias));

    // Flags
    enum Flags {
        MF = 1, // More Fragments
        DF = 2 // Don't Fragment
    };

public:
   // RFC 1700 Protocols
    typedef unsigned char Protocol;
    enum {
        TCP     = 0x06,
    };




protected:
    IP(NIC<Ethernet> * nic);

public:
    static IP* init(NIC<Ethernet> * nic);

    static const unsigned int PROTOCOL = Ethernet::PROTO_IP;

    void send(Address dst, const void * data, unsigned int size);

    void update(Observed * observed, const Protocol & c, BufferInfo * d) override;

private:
    static IP * _ip;
    NIC<Ethernet> * _nic;
};

__END_SYS

#endif