#include "network_interface.hh"

#include "address.hh"
#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "ipv4_datagram.hh"
#include "parser.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  auto next_hop_ip = next_hop.ipv4_numeric();
  if ( ip2ether_.contains( next_hop_ip ) ) {
    auto res = ip2ether_[next_hop_ip];
    auto ethernet_address = res.first;
    EthernetFrame ethernet_frame;
    {
      ethernet_frame.header.dst = ethernet_address;
      ethernet_frame.header.src = ethernet_address_;
      ethernet_frame.header.type = EthernetHeader::TYPE_IPv4;
      ethernet_frame.payload = serialize( dgram );
    }
    out_frames_.push( ethernet_frame );
  } else {
    if ( arp_timer_.contains( next_hop_ip ) ) {
      waited_dgrams_[next_hop_ip].push( dgram );
    } else {
      ARPMessage arp_message;
      {
        arp_message.opcode = ARPMessage::OPCODE_REQUEST;
        arp_message.sender_ethernet_address = ethernet_address_;
        arp_message.sender_ip_address = ip_address_.ipv4_numeric();
        arp_message.target_ethernet_address = { 0, 0, 0, 0, 0, 0 };
        arp_message.target_ip_address = next_hop_ip;
      }
      EthernetFrame arp_frame;
      {
        arp_frame.header.src = ethernet_address_;
        arp_frame.header.dst = ETHERNET_BROADCAST;
        arp_frame.header.type = EthernetHeader::TYPE_ARP;
        arp_frame.payload = serialize( arp_message );
      }
      out_frames_.push( arp_frame );
      arp_timer_[next_hop_ip] = 0;
      waited_dgrams_[next_hop_ip].push( dgram );
    }
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if ( !( frame.header.dst == ETHERNET_BROADCAST || frame.header.dst == ethernet_address_ ) ) {
    return {};
  }
  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram dgram;
    if ( parse( dgram, frame.payload ) ) {
      return dgram;
    }
  } else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage arp_message;
    if ( parse( arp_message, frame.payload ) ) {
      ip2ether_[arp_message.sender_ip_address] = { arp_message.sender_ethernet_address, 0 };
      if ( arp_message.opcode == ARPMessage::OPCODE_REQUEST ) {
        if ( arp_message.target_ip_address == ip_address_.ipv4_numeric() ) {
          ARPMessage arp_reply;
          {
            arp_reply.opcode = ARPMessage::OPCODE_REPLY;
            arp_reply.sender_ethernet_address = ethernet_address_;
            arp_reply.sender_ip_address = ip_address_.ipv4_numeric();
            arp_reply.target_ethernet_address = arp_message.sender_ethernet_address;
            arp_reply.target_ip_address = arp_message.sender_ip_address;
          }
          EthernetFrame arp_frame;
          {
            arp_frame.header.dst = arp_message.sender_ethernet_address;
            arp_frame.header.src = ethernet_address_;
            arp_frame.header.type = EthernetHeader::TYPE_ARP;
            arp_frame.payload = serialize( arp_reply );
          }
          out_frames_.push( arp_frame );
        }
      } else {
        auto ip_learned = arp_message.sender_ip_address;
        if ( waited_dgrams_.contains( ip_learned ) && !waited_dgrams_[ip_learned].empty() ) {
          auto& dgrams = waited_dgrams_[arp_message.sender_ip_address];
          while ( !dgrams.empty() ) {
            auto dgram = dgrams.front();
            dgrams.pop();
            send_datagram( dgram, Address::from_ipv4_numeric( ip_learned ) );
          }
        }
      }
    }
  }
  return {};
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // 作者：haha
  // 链接：https://zhuanlan.zhihu.com/p/642466343
  // 来源：知乎
  // 著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。

  static constexpr size_t IP_MAP_TTL = 30000;     // 30s
  static constexpr size_t ARP_REQUEST_TTL = 5000; // 5s

  for ( auto it = ip2ether_.begin(); it != ip2ether_.end(); ) {
    it->second.second += ms_since_last_tick;
    if ( it->second.second >= IP_MAP_TTL ) {
      it = ip2ether_.erase( it );
    } else {
      ++it;
    }
  }

  for ( auto it = arp_timer_.begin(); it != arp_timer_.end(); ) {
    it->second += ms_since_last_tick;
    if ( it->second >= ARP_REQUEST_TTL ) {
      it = arp_timer_.erase( it );
    } else {
      ++it;
    }
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if ( out_frames_.empty() ) {
    return {};
  }
  auto frame = out_frames_.front();
  out_frames_.pop();
  return frame;
}
