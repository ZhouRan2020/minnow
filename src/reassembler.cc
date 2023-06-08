#include "reassembler.hh"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <utility>
using namespace std;
void Reassembler::merge( block_node& bn, set<block_node>::iterator bo )
{
  if ( bo == blocks_.end() ) {
    blocks_.insert( bn );
    return;
  }
  if ( bn.begin < bo->begin && bn.end() <= bo->end() ) {//1
    bn.data = bn.data.substr( 0, bo->begin-bn.begin );
    blocks_.insert( bn );
    return;
  }
  if ( bn.begin >= bo->begin && bn.end() <= bo->end() ) {//2
    return;
  }
  if ( bn.end() <= bo->begin ) {//5
    blocks_.insert( bn );
    return;
  }
  if ( bn.begin >= bo->end() ) {//6
    merge( bn, next( bo ) );
    return;
  }
  if ( bn.begin >= bo->begin && bn.end() > bo->end() ) {//3
    bn.data = bn.data.substr( bo->end() - bn.begin, bn.end()-bo->end() );
    bn.begin = bo->end();
    merge( bn, next( bo ) );
    return;
  }
  if ( bn.begin < bo->begin && bn.end() > bo->end() ) {//4
    block_node bnn { bn.begin, bn.data.substr( 0, bo->begin-bn.begin ) };
    blocks_.insert( bnn );
    bn.data = bn.data.substr( bo->end() - bn.begin, bn.end()-bo->end() );
    bn.begin = bo->end();
    merge( bn, next( bo ) );
    return;
  }
}
void Reassembler::push_substring( block_node& bn, uint64_t first_unacceptable_index )
{
  if ( bn.end() <= first_unassembled_index ) {
    return;
  }
  if ( bn.begin >= first_unacceptable_index ) {
    return;
  }
  if ( bn.begin < first_unassembled_index ) {
    bn.data = bn.data.substr( first_unassembled_index - bn.begin, bn.end()-first_unassembled_index );
    bn.begin = first_unassembled_index;
  }
  if ( bn.end() > first_unacceptable_index ) {
    bn.data = bn.data.substr( 0, first_unacceptable_index-bn.begin );
  }
  set<block_node>::iterator blk;
  if ( blocks_.empty() ) {
    blk = blocks_.end();
  } else {
    blk = blocks_.lower_bound( bn );
    if ( blk != blocks_.begin() ) {
      blk = prev( blk );
    }
  }
  merge( bn, blk );
}
void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  if ( output.is_closed() ) {
    return;
  }
  if ( is_last_substring ) {
    end_check = true;
    end_idx = first_index + data.length();
  }
  if ( data.empty() ) {
    if ( end_check && first_unassembled_index == end_idx ) {
      output.close();
    }
    return;
  }
  uint64_t first_unacceptable_index = first_unassembled_index + output.available_capacity();
  block_node bn { first_index, move( data ) };
  push_substring( bn, first_unacceptable_index );
  while ( !blocks_.empty() && blocks_.begin()->begin == first_unassembled_index ) {
    output.push( blocks_.begin()->data );
    first_unassembled_index = blocks_.begin()->end();
    if ( end_check && first_unassembled_index == end_idx ) {
      output.close();
    }
    blocks_.erase( blocks_.begin() );
  }
}
uint64_t Reassembler::bytes_pending() const
{
  uint64_t res = 0;
  for ( const auto& subs : blocks_ ) {
    res += subs.length();
  }
  return res;
}