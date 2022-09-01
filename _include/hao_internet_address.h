#ifndef _HAO_INTERNET_ADDRESS_H_
#define _HAO_INTERNET_ADDRESS_H_

#include <netinet/in.h>

#include <string>
#include <tuple>
using std::string;
using std::tuple;

// 一些类型
enum class IpType {Ipv4, Ipv6};
enum class AddressType {Loopback, Any};
// tuple<port, address_type, ip_type>
using ServerAddress = tuple<uint16_t, AddressType, IpType>;

class InternetAddress
{
    public:
        InternetAddress() = default;
        InternetAddress(uint16_t port, AddressType type, IpType ip_type);
        InternetAddress(string& ip, uint16_t port, IpType ip_type);
        
        explicit InternetAddress(const sockaddr_in& address);

        explicit InternetAddress(const sockaddr_in6& address);
        
        sa_family_t Family() const;
        const struct sockaddr* SockAddr() const;
        const socklen_t Size() const;

        void set_sockaddr(const struct sockaddr_in6& address_6);
        void set_sockaddr(const struct sockaddr_in& address_4);
        
        string ToIP() const;
        string ToIPPort() const;
        uint16_t Port();
    private:
        union {
            struct sockaddr     addr_;
            struct sockaddr_in  addr4_;
            struct sockaddr_in6 addr6_;
        };
};

#endif