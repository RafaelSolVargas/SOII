#ifndef __icmp_h
#define __icmp_h

#include "network/ip.h"
#include "network/ethernet.h"
#include <network/ip_network.h>

__BEGIN_SYS

class ICMP
{
    friend class IP;
    friend class IPEventsHandler;

public:
    typedef IP::AllocationMap AllocationMap;
    typedef IP::Header IPHeader;

    // ICMP Packet Types
    typedef unsigned char Type;
    enum 
    {
        ECHO_REPLY = 0,
        ECHO = 8
    };

    // ICMP Packet Codes
    typedef unsigned char Code;
    enum {
        HOST_UNREACHABLE = 1
    };

    class Header
    {
    public:
        Header() {}
        Header(const Type & type, const Code & code): _type(type), _code(code) {}
        Header(const Type & type, const Code & code, unsigned short id, unsigned short seq):
            _type(type), _code(code), _checksum(0), _id(htons(id)), _sequence(htons(seq)) {}

        Type & type() { return _type; }
        Code & code() { return _code; }
        unsigned short checksum() { return _checksum; }

        unsigned short id() { return htons(_id); }
        void id(unsigned short id) { _id = htons(id); }

        unsigned short sequence() { return htons(_sequence); }
        void sequence(unsigned short seq) { _sequence = htons(seq); }

    protected:
        Type _type;
        Code _code;
        unsigned short _checksum;
        unsigned short _id;
        unsigned short _sequence;
    } __attribute__((packed));

    // ICMP Packet
    static const unsigned int MTU = 56; // to make a traditional 64-byte packet
    static const unsigned int HEADERS_SIZE = sizeof(IP::Header) + sizeof(Header);

    typedef unsigned char Data[MTU];

    class Packet: public Header
    {
    public:
        Packet(){}
        Packet(const Type & type, const Code & code): Header(type, code) {}
        Packet(const Type & type, const Code & code, unsigned short id, unsigned short seq): Header(type, code, id, seq) {}

        void sum() { _checksum = 0; _checksum = htons(IP::generic_checksum(reinterpret_cast<unsigned char *>(this), sizeof(Packet))); }
        bool check() { return (IP::generic_checksum(reinterpret_cast<unsigned char *>(this), sizeof(Packet)) != 0xffff); }

    private:
        Data _data;
    } __attribute__((packed));

protected:
    ICMP(IP * ip);
    static void datagram_callback(IPHeader * header, AllocationMap * map);

public:
    ~ICMP() { }

private:
    IP * _ip;
};

__END_SYS

#endif