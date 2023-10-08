
#ifndef __ip_traits_h
#define __ip_traits_h

#include <system/config.h>

__BEGIN_SYS

template<> struct Traits<IP>: public Traits<Debug>
{
    static const bool debugged = true;
    static const bool error   = true;
    static const bool warning = true;
    static const bool info    = true;
    static const bool trace   = true;
};

__END_SYS

#endif
