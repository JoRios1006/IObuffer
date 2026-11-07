#pragma once
#include <cstddef>
#include <string_view>
#include <unistd.h>
template <std::size_t Capacity> class IOBuffer {
  int FD; 
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
  // throw away without
  void discard(void) noexcept {};
  void wipe(void) noexcept {};
  void reset_error(void) noexcept {};
  bool good(void) const noexcept {};
  std::size_t size(void) const noexcept {};
  std::size_t remaining(void) const noexcept {};
  static constexpr std::size_t capacity(void) noexcept {return Capacity;};
  std::string_view view(void) const noexcept {};
  const char* data(void) const noexcept {};
  int fd(void) const noexcept {};
  void set_fd(void) noexcept {};
};
