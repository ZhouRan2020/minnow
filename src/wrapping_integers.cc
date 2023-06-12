#include "wrapping_integers.hh"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <map>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point+static_cast<uint32_t>(n%width32);
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t midbase{checkpoint&set32zero};
  uint64_t lowbase{midbase>=width32?midbase-width32:midbase};
  uint64_t upbase{midbase<=lastbase?midbase+width32:midbase};
  uint32_t off{raw_value_>=zero_point.raw_value_?raw_value_-zero_point.raw_value_:UINT32_MAX-(zero_point.raw_value_-raw_value_)+1};
  uint64_t dl=checkpoint-(lowbase+off);
  uint64_t dm=midbase+off>=checkpoint?midbase+off-checkpoint:checkpoint-(midbase+off);
  uint64_t du=upbase+off-checkpoint;
  if(dl<=dm){
    if(dl<=du){
      return lowbase+off;
    }
    return upbase+off;
  }
  if(dm<=du){
    return midbase+off;
  }
  return upbase+off;
}
