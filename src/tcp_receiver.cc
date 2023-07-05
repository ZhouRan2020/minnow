#include "tcp_receiver.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <iostream>
#include <optional>
#include <type_traits>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  if ( message.SYN ) {
    isn = message.seqno;
    has_syn = true;
  }
  if ( !has_syn ) {
    return;
  }
  const uint64_t fi = message.seqno.unwrap( isn, inbound_stream.bytes_pushed() ) - !message.SYN;
  reassembler.insert( fi, message.payload.release(), message.FIN, inbound_stream );
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  auto wsz { inbound_stream.available_capacity() > UINT16_MAX
               ? static_cast<uint16_t>( UINT16_MAX )
               : static_cast<uint16_t>( inbound_stream.available_capacity() ) };
  if ( has_syn ) {
    auto ackno = Wrap32::wrap( inbound_stream.bytes_pushed() + has_syn + inbound_stream.is_closed(), isn );
    return { ackno, wsz };
  }
  return { {}, wsz };
}
