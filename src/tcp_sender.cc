#include "tcp_sender.hh"
#include "buffer.hh"
#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <random>

using namespace std;

// 作者：haha
// 链接：https://zhuanlan.zhihu.com/p/642465322
// 来源：知乎
// 著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。

TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return outstanding_seqnos_ + pushed_seqnos_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return retransmit_cnt_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  if ( retx_ ) {
    retx_ = false;
    return retx_seg_;
  }
  if ( pushed_segments_.empty() ) {
    return {};
  }
  auto msg = pushed_segments_.front();

  pushed_segments_.pop_front();
  pushed_seqnos_ -= msg.sequence_length();

  outstanding_segments_.push( msg );
  outstanding_seqnos_ += msg.sequence_length();

  if ( !timer_.is_running() ) {
    timer_.start();
  }

  return msg;
}

void TCPSender::push( Reader& outbound_stream )
{
  uint16_t win_sz = window_size_ ? window_size_ : 1;

  while (  win_sz > sequence_numbers_in_flight()  ) {

    TCPSenderMessage msg;

    msg.seqno = Wrap32::wrap( next_seqno_, isn_ );

    if ( !syn_pushed_ ) {
      syn_pushed_ = msg.SYN = true;
      pushed_seqnos_++;
    }

    auto const payload_size = min( TCPConfig::MAX_PAYLOAD_SIZE, win_sz - sequence_numbers_in_flight() );
    if ( payload_size > 0 ) {
      read( outbound_stream, payload_size, msg.payload );
    }
    pushed_seqnos_ += msg.payload.size();

    if ( !fin_pushed_ && outbound_stream.is_finished() &&  win_sz > sequence_numbers_in_flight() ) {
      fin_pushed_ = msg.FIN = true;
      pushed_seqnos_++;
    }

    if ( msg.sequence_length() == 0 ) {
      break;
    }

    pushed_segments_.push_back( msg );
    next_seqno_ += msg.sequence_length();
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  auto seq = Wrap32::wrap( next_seqno_, isn_ );
  return { seq, false, Buffer {}, false };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  window_size_ = msg.window_size;
  if ( msg.ackno.has_value() ) {
    auto ackno = msg.ackno.value().unwrap( isn_, next_seqno_ );
    if ( ackno > next_seqno_ ) {
      return;
    }
    acknoed_seqno_ = ackno;

    if ( !outstanding_segments_.empty() ) {
      auto fseg = outstanding_segments_.front();
      auto fsegend = fseg.seqno.unwrap( isn_, next_seqno_ ) + fseg.sequence_length();

      int seg_popped = 0;
      while ( fsegend <= acknoed_seqno_ && !outstanding_segments_.empty() ) {
        outstanding_segments_.pop();
        outstanding_seqnos_ -= fseg.sequence_length();
        seg_popped++;

        if ( !outstanding_segments_.empty() ) {
          fseg = outstanding_segments_.front();
          fsegend = fseg.seqno.unwrap( isn_, next_seqno_ ) + fseg.sequence_length();
        }
      }
      if ( seg_popped > 0 ) {
        timer_.reset_RTO();
        if ( !outstanding_segments_.empty() ) {
          timer_.start();
        } else {
          timer_.stop();
        }
        retransmit_cnt_ = 0;
      }
    }
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  timer_.tick( ms_since_last_tick );
  if ( outstanding_segments_.empty() ) {
    return;
  }
  if ( timer_.is_expired() ) {
    retx_ = true;
    retx_seg_ = outstanding_segments_.front();
    // outstanding_segments_.pop();
    // outstanding_seqnos_ -= retx_seg.sequence_length();

    // pushed_segments_.push_front( retx_seg );
    // pushed_seqnos_ += retx_seg.sequence_length();
    // retx_seqnos_+=retx_seg.sequence_length();

    if ( window_size_ ) {
      retransmit_cnt_++;
      timer_.double_RTO();
    }
    timer_.start();
  }
}