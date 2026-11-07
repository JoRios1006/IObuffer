#pragma once
#include <cstddef>
#include <string_view>
#include <unistd.h>
template <std::size_t Capacity> class IOBuffer {
  int FD;
  bool ERR = false;
  std::size_t LEN = 0;
  char BUFFER[Capacity];
  static bool write_all(void) {};
public:
  // constructor
  explicit IOBuffer(int fd = STDOUT_FILENO) noexcept : FD(fd) {};
  // descructor
  ~IOBuffer() { flush(); }
  IOBuffer(const IOBuffer&) = delete;
  IOBuffer& operator=(const IOBuffer&) = delete;
  IOBuffer &append(void) {};
  // SALIDA
  // flush->write(2) 
  bool flush(void){};
  // throw away without flushing setting LEN = 0
  void discard(void) noexcept { LEN = 0;};
  // set all the buffer to zeroes
  void wipe(void) noexcept {
    volatile char *P = BUFFER;
    for (size_t I = 0; I < Capacity; ++I) {
    P[I] = 0;
    }
    LEN = 0;    
  };
  const char *data(void) const noexcept {};
  // INFO
  // get error flag value, agnostic of what pattern you prefer
  bool good(void) const noexcept {return !ERR;};
  bool bad(void) const noexcept {return ERR;};
  // set error flag to default
  void reset_error(void) noexcept { ERR = false;};
  // get the amount of characters written so far
  std::size_t written(void) const noexcept {return LEN;};
  //get the amount of space left in the buffer
  std::size_t remaining(void) const noexcept { return Capacity - LEN;};
  static constexpr std::size_t capacity(void) noexcept {return Capacity;};
  std::string_view view(void) const noexcept {};
  // return the current File Descriptor
  int fd(void) const noexcept {return FD;};
  // set the new File Descriptor  
  void set_fd(int fd) noexcept {FD = fd;};
};
