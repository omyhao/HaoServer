#include "hao_internet_address.h"
#include "hao_log.h"
#include "hao_algorithm.h"

#include <arpa/inet.h>
#include <endian.h>


using namespace hao_log;


InternetAddress::InternetAddress(uint16_t port, AddressType type = AddressType::Any, IpType ip_type = IpType::Ipv4)
{
    if(ip_type == IpType::Ipv6)
    {
        MemZero(&addr6_, sizeof(addr6_));
        addr6_.sin6_family = AF_INET6;
        addr6_.sin6_addr = (type == AddressType::Loopback)? in6addr_loopback:in6addr_any;
        addr6_.sin6_port = htons(port);
    }
    else
    {
        MemZero(&addr4_, sizeof(addr4_));
        addr4_.sin_family = AF_INET;
        addr4_.sin_addr.s_addr = (type == AddressType::Loopback)?INADDR_LOOPBACK:INADDR_ANY;
        addr4_.sin_port = htons(port);
    }
}

InternetAddress::InternetAddress(string& ip, uint16_t port, IpType ip_type = IpType::Ipv4)
{
    
    // ipv6格式地址以:分割
    if(ip_type == IpType::Ipv6 || strchr(ip.c_str(), ':'))
    {
        MemZero(&addr6_, sizeof(addr6_));
        addr6_.sin6_family = AF_INET6;
        addr6_.sin6_port = htons(port);
        
        if(::inet_pton(AF_INET6, ip.c_str(), &addr6_) != 1)
        {
            LOG_ERROR << "ipv6 address error";
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        MemZero(&addr4_, sizeof(addr4_));
        addr4_.sin_family = AF_INET;
        addr4_.sin_port = htons(port);
        if(::inet_pton(AF_INET, ip.c_str(), &addr4_) != 1)
        {
            LOG_ERROR << "ipv4 address error";
            exit(EXIT_FAILURE);
        }
    }
}
InternetAddress::InternetAddress(const sockaddr_in& address)
    :addr4_{address}
{
}

InternetAddress::InternetAddress(const sockaddr_in6& address)
    : addr6_{address}
{
}

sa_family_t InternetAddress::Family() const
{
    return addr_.sa_family;
}

string InternetAddress::ToIP() const
{
    char buffer[64] = "";
    switch (addr_.sa_family)
    {
        case AF_INET:
            ::inet_ntop(AF_INET, &addr4_.sin_addr, buffer, 64);
            break;
        
        case AF_INET6:
            ::inet_ntop(AF_INET, &addr6_.sin6_addr, buffer, 64);
            break;
        default:
            ::strncpy(buffer, "Unknown AF", 64);
            break;
    }
    return buffer;
}
string InternetAddress::ToIPPort() const
{
    char buffer[64] = "";
    size_t end;
    uint16_t port;
    switch (addr_.sa_family)
    {
        case AF_INET:
            ::inet_ntop(AF_INET, &addr4_.sin_addr, buffer, 64);
            end = ::strlen(buffer);
            port = ntohs(addr4_.sin_port);
            snprintf(buffer+end, 64-end, ":%u", port);
            break;
        
        case AF_INET6:
            ::inet_ntop(AF_INET, &addr6_.sin6_addr, buffer, 64);
            end = ::strlen(buffer);
            port = ntohs(addr6_.sin6_port);
            snprintf(buffer+end, 64-end, ":%u", port);
            break;
        default:
            ::strncpy(buffer, "Unknown AF", 64);
            break;
    }
    return buffer;
}
uint16_t InternetAddress::Port()
{
    return ntohs(addr4_.sin_port);
}

const struct sockaddr* InternetAddress::SockAddr() const
{
    return &addr_;
}

const socklen_t InternetAddress::Size() const
{
    if(addr_.sa_family == AF_INET)
    {
        return static_cast<socklen_t>(sizeof(struct sockaddr_in));
    }
    else
    {
        return static_cast<socklen_t>(sizeof(struct sockaddr_in6));
    }
}

void InternetAddress::set_sockaddr(const struct sockaddr_in6& address_6)
{
    addr6_ = address_6;
}

void InternetAddress::set_sockaddr(const struct sockaddr_in& address_4)
{
    addr4_ = address_4;
}