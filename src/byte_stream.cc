#include "byte_stream.hh"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
using namespace std;
ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ), data_queue_ {}, data_view_ {} {}
void Writer::push( string data )
{
  if ( has_error_ ) {
    throw runtime_error( "The stream has error. Push failed.\n" );
  }
  if ( is_closed_ ) {
    return;
  }
  if ( data.empty() || available_capacity() == 0 ) {
    return;
  }
  auto const topush=min(available_capacity(),data.length());
  if(topush<data.size()){
    data=data.substr(0,topush);
  }
  data_queue_.push_back(move(data));
  data_view_.emplace_back(data_queue_.back().data(),topush);
  bytes_pushed_+=topush;
}

void Writer::close()
{
  is_closed_ = true;
}
void Writer::set_error()
{
  has_error_ = true;
}
bool Writer::is_closed() const
{
  return is_closed_;
}
uint64_t Writer::available_capacity() const
{
  return capacity_ - ( bytes_pushed_ - bytes_popped_ );
}
uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_;
}
string_view Reader::peek() const
{
  if(data_view_.empty()) { 
    return {};
  }
  return data_view_.front();
}
bool Reader::is_finished() const
{
  return is_closed_ && bytes_pushed_ == bytes_popped_;
}
bool Reader::has_error() const
{
  return has_error_;
}
void Reader::pop( uint64_t len )
{
  if ( has_error() ) {
    throw runtime_error( "The stream has error. Read failed.\n" );
  }
  if ( is_finished() ) {
    return;
  }
  if(len==0||data_view_.empty()){
    return;
  }
  auto topop=min(len,bytes_buffered());
  while(topop>=data_view_.front().size()){
    topop-=data_view_.front().size();
    data_queue_.pop_front();
    data_view_.pop_front();
  }
  if(topop){
    data_view_.front().remove_prefix(topop); 
  }
  bytes_popped_+=topop;
}
uint64_t Reader::bytes_buffered() const
{
  return bytes_pushed_ - bytes_popped_;
}
uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}
