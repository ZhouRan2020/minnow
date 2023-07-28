#include "router.hh"
#include "address.hh"
#include "ipv4_datagram.hh"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  Item it { route_prefix, prefix_length, next_hop, interface_num };
  routing_table.push_back( it );
}

Router::Table::const_iterator Router::longest_prefix_match( uint32_t dst_ip ) const
{
  if ( routing_table.empty() ) {
    return routing_table.end();
  }
  auto res = routing_table.end();
  uint8_t longest_prefix_length = 0;
  for ( auto it = routing_table.begin(); it != routing_table.end(); ++it ) {
    if ( match_length_( it->route_prefix, dst_ip, it->prefix_length ) ) {
      res = it->prefix_length >= longest_prefix_length ? it : res;
    }
  }
  return res;
}

void Router::route()
{
  for ( auto&& interf : interfaces_ ) {
    auto op_dg = interf.maybe_receive();
    while ( op_dg.has_value() ) {
      InternetDatagram dgram = op_dg.value();
      auto dst_ip = dgram.header.dst;
      auto it = longest_prefix_match( dst_ip );
      if ( it != routing_table.end() ) {
        if ( dgram.header.ttl > 1 ) {
          dgram.header.ttl--;
          dgram.header.compute_checksum();
          auto op_next_hop = it->next_hop;
          Address nh = op_next_hop.value_or( Address::from_ipv4_numeric( dst_ip ) );
          interface( it->interface_num ).send_datagram( dgram, nh );
        }
      }
      op_dg = interf.maybe_receive();
    }
  }
}

bool Router::match_length_( uint32_t src_ip, uint32_t tgt_ip, uint8_t tgt_len )
{
  if ( tgt_len == 0 ) {
    return true;
  }

  if ( tgt_len > 32 ) {
    return false;
  }

  // tgt_len < 32
  uint8_t const len = 32U - tgt_len;
  src_ip = src_ip >> len;
  tgt_ip = tgt_ip >> len;
  return src_ip == tgt_ip;
}

// 作者：haha
// 链接：https://zhuanlan.zhihu.com/p/642466343
// 来源：知乎
// 著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。