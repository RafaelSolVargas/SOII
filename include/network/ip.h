#ifndef __ip_layer_h
#define __ip_layer_h

#include <machine/nic.h>
#include <network/ethernet.h>
#include <utility/list.h>
#include <system.h>

__BEGIN_SYS

class IP: public NIC<Ethernet>::Observer
{
private:
    /// @brief Buffer received by the NIC
    typedef Ethernet::BufferInfo BufferInfo;
    typedef Ethernet::Protocol Protocol;
    typedef Ethernet::Observed Observed;
    typedef Ethernet::Address Address;

    // Flags
    enum {
        MF = 1, // More Fragments
        DF = 2 // Don't Fragment
    };

    // The Header of the fragment, the static data to identify it
    class FragmentHeader
    {
    public:
        FragmentHeader() {}
        FragmentHeader(unsigned short total_length, unsigned short id, unsigned char flags, const unsigned short offset) 
            : _total_length(total_length), _id(id), _flags(flags), _offset(offset) { }

        bool more_fragments() { return _flags == MF; }
        bool not_fragment() { return _flags == DF; }

        unsigned int id() { return _id; }
        unsigned int offset() { return _offset; }

        friend Debug & operator<<(Debug & db, const FragmentHeader & f) {
            db << "{s=" << f._total_length << ",id=" << f._id << ",f=" << f._flags << ",i=" << f._offset << "}";
            
            return db;
        }

    protected:
        unsigned short _total_length; // Total length of the datagram, including the header 
        unsigned short _id; // Identifier of the datagram
        unsigned char _flags :3; // Flags (DF, MF and Reserved)
        unsigned int _offset :13; // Offset of this frame in the datagram
    } __attribute__((packed, may_alias));

    // Fragment Data
    static const unsigned int MTU_FRAGMENT = NIC<Ethernet>::MTU - sizeof(FragmentHeader);
    typedef unsigned char FragmentData[MTU_FRAGMENT];

    /// @brief The Fragment class that will be created in the Fragmentation process
    class Fragment : public FragmentHeader 
    {
    public:
        Fragment() {}
        Fragment(unsigned short total_length, unsigned short id, unsigned char flags, const unsigned short offset, 
        const void * data, unsigned int size) : FragmentHeader(total_length, id, flags, offset) 
        {
            memcpy(_data, data, size);
        }

        template<typename T>
        T * data() { return reinterpret_cast<T *>(&_data); }
        
        unsigned int size() { return sizeof(FragmentHeader) + sizeof(FragmentData); }

        FragmentHeader * header() { return this; }

        friend Debug & operator<<(Debug & db, const Fragment & f) {
            db << "{h=" << reinterpret_cast<const FragmentHeader &>(f) << ",d=" << f._data << "}";

            return db;
        }

    protected:
        FragmentData _data;
    } __attribute__((packed, may_alias));

    class Header 
    {
        public: 
            Header() {
                a = 1;
                b = 2;
                c = 3;
                e = 4;
                e = 5;
                f = 6;
            }

            friend Debug & operator<<(Debug & db, const Header & f) {
                db << "{a=" << f.a << ",b=" << f.b << ",c=" << f.c << ",d=" << f.d << ",e=" << f.e << ",f=" << f.f << "}";

                return db;
            }
     
            unsigned int a;
            unsigned int b;
            unsigned int c;
            unsigned int d;
            unsigned int e;
            unsigned int f;
    };


public:
    class Datagram : public Header 
    {
        public:
            Datagram(const void * data, unsigned int data_size) : Header(), _data_size(data_size)
            { 
                // MUDAR ISSO PARA O MEU BUFFER
                _data = new (SYSTEM) unsigned char[data_size];

                memcpy(_data, data, data_size);
            }

            ~Datagram() 
            {
                delete _data;
            }

            Header * header() { return this; }

            template<typename T>
            T * data() { return reinterpret_cast<T *>(&_data); }
            unsigned int size() const { return sizeof(Header) + _data_size; }

            friend Debug & operator<<(Debug & db, const Datagram & f) {
                db << "{a=" << reinterpret_cast<const Header &>(f) << ",s=" << f._data_size << ",d=" << *f._data << "}";

                return db;
            }


        protected:
            unsigned char * _data;
            unsigned int _data_size;
    };

    /// @brief Maximum data stored in a Datagram
    static const unsigned int MTU = 65535 - sizeof(Header);

    /// @brief Maximum data that can be send without fragmentation in one send
    static const unsigned int MTU_WO_FRAG = MTU_FRAGMENT - sizeof(Header);
protected:
    IP(NIC<Ethernet> * nic);

public:
    static IP* init(NIC<Ethernet> * nic);

    static const unsigned int IP_PROT = Ethernet::PROTO_IP;

    void send(Address dst, const void * data, unsigned int size);

    void update(Observed * observed, const Protocol & c, BufferInfo * d) override;

    NIC<Ethernet> * nic() { return _nic; }

private:
    static unsigned int _datagram_count;
    static IP * _ip;
    NIC<Ethernet> * _nic;

    void send_with_fragmentation(Address dst, const void * data, unsigned int size);
    void send_without_fragmentation(Address dst, const void * data, unsigned int size);
    static unsigned int get_next_id();
};

__END_SYS

#endif