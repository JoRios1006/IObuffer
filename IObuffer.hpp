#pragma once
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstring>
#include <string_view>
#include <unistd.h>
template <std::size_t Capacity> class IOBuffer {
  int FD;
  bool ERR = false;
  std::size_t LEN = 0;
  char BUFFER[Capacity];
  static bool write_all(int fd, const char *P, std::size_t N) {
    while (N > 0) {
      ssize_t WRITTEN = ::write(fd, P, N);
      if (WRITTEN < 0) {
        if (errno == EINTR)
          continue;   // señal: reintentar
        return false; // EAGAIN, EPIPE, EBADF... => error
      }
      P += WRITTEN;
      N -= static_cast<std::size_t>(WRITTEN);
    }
  };
  template <typename T>
  static constexpr bool is_int_v =
      std::is_integral_v<T> && !std::is_same_v<std::remove_cv_t<T>, bool> &&
      !std::is_same_v<std::remove_cv_t<T>, char>;

  template <typename... A> IOBuffer &put_num(A... a) noexcept {
    auto r = std::to_chars(BUFFER + LEN, BUFFER + Capacity, a...);
    if (r.ec != std::errc{}) {
      flush();
      r = std::to_chars(BUFFER, BUFFER + Capacity, a...);
      if (r.ec != std::errc{}) {
        ERR = true;
        return *this;
      }
    }
    LEN = static_cast<std::size_t>(r.ptr - BUFFER);
    return *this;
  }
public:
  // constructor
  explicit IOBuffer(int fd = STDOUT_FILENO) noexcept : FD(fd) {};
  // descructor
  ~IOBuffer() { flush(); }
  IOBuffer(const IOBuffer &) = delete;
  IOBuffer &operator=(const IOBuffer &) = delete;
  IOBuffer &append(void) {};
  IOBuffer &append(std::string_view sv) noexcept {
    if (sv.empty())
      return *this; // prevent memcpy a nullptr
    if (sv.size() <= Capacity - LEN) {
      std::memcpy(BUFFER + LEN, sv.data(), sv.size());
      LEN += sv.size();
      return *this;
    }
    flush(); // <- Why? You ran out of memory and don't want data to get lost,
             // pay the cost of a syscall.
    if (sv.size() >= Capacity) { // <- Doesn't even fit
      if (!ERR && !write_all(FD, sv.data(), sv.size())) {
        ERR = !ERR;
      } else {
        std::memcpy(BUFFER, sv.data(), sv.size());
        LEN = sv.size();
      }
      return *this;
    }
  };

  IOBuffer &append(const char *s) noexcept {
    return append(s ? std::string_view(s)
                    : std::string_view()); // wrap a char* to a string view ;P
  }
  IOBuffer &append(bool b) noexcept {
    return append(b ? std::string_view("true") : std::string_view("false"));
  }
  IOBuffer &append(char c) noexcept {
    if (LEN == Capacity)
      flush();
    BUFFER[LEN++] = c;
    return *this;
  }
  // Numeros
  template <typename T>
  std::enable_if_t<is_int_v<T>, IOBuffer &> append(T v) noexcept {
    return put_num(v);
  }
  template <typename T>
  std::enable_if_t<std::is_floating_point_v<T>, IOBuffer &>
  append(T v) noexcept {
    return put_num(v);
  }
  // Decimal fijo: append_fixed(3.14159, 2) -> "3.14"
  template <typename T>
  std::enable_if_t<std::is_floating_point_v<T>, IOBuffer &>
  append_fixed(T v, int precision) noexcept {
    return put_num(v, std::chars_format::fixed, precision);
  }
  // Hex en minúscula, sin prefijo: append_hex(255) -> "ff"
  template <typename T>
  std::enable_if_t<is_int_v<T>, IOBuffer &> append_hex(T v) noexcept {
    return put_num(v, 16);
  }
  // SALIDA
  // flush->write(2)
  bool flush(void) noexcept { return flush(FD); };
  bool flush(int fd) noexcept {
    if (LEN <= 0)
      return !ERR;
    if (!ERR && !write_all(fd, BUFFER, LEN))
      ERR = true;
    LEN = 0;
  }
  // throw away without flushing setting LEN = 0
  void discard(void) noexcept { LEN = 0; };
  // set all the buffer to zeroes
  void wipe(void) noexcept {
    volatile char *P = BUFFER;
    for (size_t I = 0; I < Capacity; ++I) {
      P[I] = 0;
    }
    LEN = 0;
  };
  // INFO
  // get error flag value, agnostic of what pattern you prefer
  bool good(void) const noexcept { return !ERR; };
  bool bad(void) const noexcept { return ERR; };
  // set error flag to default
  void reset_error(void) noexcept { ERR = false; };
  // get the amount of characters written so far
  std::size_t written(void) const noexcept { return LEN; };
  // get the amount of space left in the buffer
  std::size_t remaining(void) const noexcept { return Capacity - LEN; };
  static constexpr std::size_t capacity(void) noexcept { return Capacity; };
  // string view of the current buffer
  std::string_view view(void) const noexcept { return {BUFFER, LEN}; };
  // pointer to the raw internal buffer
  const char *data(void) const noexcept { return BUFFER; };
  // return the current File Descriptor
  int fd(void) const noexcept { return FD; };
  // set the new File Descriptor
  void set_fd(int fd) noexcept { FD = fd; };
};
