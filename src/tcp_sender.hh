#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <queue>
// 作者：haha 链接：https
//   : // zhuanlan.zhihu.com/p/642465322
//     来源：知乎 著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。

class Timer
{
private:
  size_t time_passed_ { 0 };
  size_t RTO_;

  bool running_ { false };
  size_t initial_RTO_;

public:
  explicit Timer( size_t initial_RTO_ms ) : RTO_( initial_RTO_ms ), initial_RTO_( initial_RTO_ms ) {}

  void tick( size_t ms_since_last_tick )
  {
    if ( running_ ) {
      time_passed_ += ms_since_last_tick;
    }
  }

  void start()
  {
    //  assert( !running_ );
    running_ = true;
    time_passed_ = 0;
  }

  void stop()
  {
    assert( running_ );
    running_ = false;
  }

  void reset_RTO() { RTO_ = initial_RTO_; }
  void double_RTO() { RTO_ *= 2; }

  bool is_running() const { return running_; }
  bool is_expired() const { return running_ && time_passed_ >= RTO_; }
};

class TCPSender
{

  Wrap32 isn_;
  size_t initial_RTO_ms_;

  std::queue<TCPSenderMessage> outstanding_segments_ {};
  uint64_t outstanding_seqnos_ { 0 };
  uint64_t acknoed_seqno_ { 0 };
  Timer timer_ { initial_RTO_ms_ };
  uint64_t retransmit_cnt_ { 0 };

  bool retx_ { false };
  TCPSenderMessage retx_seg_ {};

  std::deque<TCPSenderMessage> pushed_segments_ {};
  uint64_t pushed_seqnos_ { 0 };
  // uint64_t retx_seqnos_{0};
  uint64_t next_seqno_ { 0 };
  uint16_t window_size_ { 1 };

  bool syn_pushed_ { false };
  bool fin_pushed_ { false };

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};
