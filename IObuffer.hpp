// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#if !defined(__unix__) && !defined(__APPLE__)
#error "IObuffer requires a real computer: unistd.h, write(2)"
#endif
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstring>
#include <string_view>
#include <sys/types.h>
#include <system_error>
#include <type_traits>
#include <unistd.h>

template <std::size_t Capacity> class IOBuffer {
  static constexpr std::size_t MIN_CAPACITY = 64;
  static_assert(Capacity >= MIN_CAPACITY);
  int FD;
  bool ERR_ = false;
  std::size_t LEN = 0;
  char BUFFER[Capacity]{}; // NOLINT

  static auto write_all(int FDE, const char *PTR, std::size_t NUM) noexcept
      -> bool {
    while (NUM > 0) { // NOLINT
      const ssize_t WRITTEN = ::write(FDE, PTR, NUM);
      if (WRITTEN < 0) {
        if (errno == EINTR) {
          continue; // señal: reintentar
        }
        return false; // EAGAIN, EPIPE, EBADF... => error
      }
      PTR += WRITTEN;
      NUM -= static_cast<std::size_t>(WRITTEN);
    }
    return true; // fuera del while
  }

  template <typename T>
  static constexpr bool is_int_v =
      std::is_integral_v<T> && !std::is_same_v<std::remove_cv_t<T>, bool> &&
      !std::is_same_v<std::remove_cv_t<T>, char>;

  template <typename... A> auto put_num(A... ARG) noexcept -> IOBuffer & {
    auto RES = std::to_chars(BUFFER + LEN, BUFFER + Capacity, ARG...);
    if (RES.ec != std::errc{}) {
      flush();
      RES = std::to_chars(BUFFER, BUFFER + Capacity, ARG...);
      if (RES.ec != std::errc{}) {
        ERR_ = true;
        return *this;
      }
    }
    LEN = static_cast<std::size_t>(RES.ptr - BUFFER);
    return *this;
  }

public:
  // constructor
  explicit IOBuffer(int fde = STDOUT_FILENO) noexcept : FD(fde) {}
  // descructor
  ~IOBuffer() { flush(); }
  IOBuffer(const IOBuffer &) = delete;
  auto operator=(const IOBuffer &) -> IOBuffer & = delete;
  IOBuffer(IOBuffer &&) = delete;
  auto operator=(IOBuffer &&) -> IOBuffer & = delete;
  auto append(std::string_view STRV) noexcept -> IOBuffer & {
    if (STRV.empty()) {
      return *this; // prevent memcpy a nullptr
    }
    if (STRV.size() <= Capacity - LEN) {
      std::memcpy(BUFFER + LEN, STRV.data(), STRV.size());
      LEN += STRV.size();
      return *this;
    }
    flush(); // <- Why? You ran out of memory and don't want data to get lost,
             // pay the cost of a syscall.
    if (STRV.size() >= Capacity) {
      if (!ERR_ && !write_all(FD, STRV.data(), STRV.size())) {
        ERR_ = true;
      }
    } else {
      std::memcpy(BUFFER, STRV.data(), STRV.size());
      LEN = STRV.size();
    }
    return *this;
  }

  auto append(const char *STR) noexcept -> IOBuffer & {
    return append((STR != nullptr)
                      ? std::string_view(STR)
                      : std::string_view()); // wrap a char* to a string view ;P
  }
  auto append(bool BOOL) noexcept -> IOBuffer & {
    return append(BOOL ? std::string_view("true") : std::string_view("false"));
  }
  auto append(char CHAR) noexcept -> IOBuffer & {
    if (LEN == Capacity) {
      flush();
    }
    BUFFER[LEN++] = CHAR;
    return *this;
  }
  // Numeros
  template <typename T>
  auto append(T value) noexcept -> std::enable_if_t<is_int_v<T>, IOBuffer &> {
    return put_num(value);
  }
  template <typename T>
  auto append(T value) noexcept
      -> std::enable_if_t<std::is_floating_point_v<T>, IOBuffer &> {
    return put_num(value);
  }
  // Decimal fijo: append_fixed(3.14159, 2) -> "3.14"
  template <typename T>
  auto append_fixed(T value, int precision) noexcept
      -> std::enable_if_t<std::is_floating_point_v<T>, IOBuffer &> {
    return put_num(value, std::chars_format::fixed, precision);
  }
  // Hex en minúscula, sin prefijo: append_hex(255) -> "ff"
  template <typename T>
  auto append_hex(T value) noexcept
      -> std::enable_if_t<is_int_v<T>, IOBuffer &> {
    constexpr int HEX_MAGIC = 16;
    return put_num(value, HEX_MAGIC);
  }
  // SALIDA
  template <typename T> auto operator<<(const T &value) noexcept -> IOBuffer & {
    return append(value);
  }
  // flush->write(2)
  auto flush() noexcept -> bool { return flush(FD); }
  auto flush(int FDE) noexcept -> bool {
    if (LEN == 0) {
      return !ERR_;
    }
    if (!ERR_ && !write_all(FDE, BUFFER, LEN)) {
      ERR_ = true;
    }
    LEN = 0;
    return !ERR_;
  }
  // throw away without flushing setting LEN = 0
  void discard() noexcept { LEN = 0; }
  // set all the buffer to zeroes
  void wipe() noexcept {
    volatile char *PTR = BUFFER;
    for (std::size_t IDX = 0; IDX < Capacity; ++IDX) { // NOLINT
      PTR[IDX] = 0;
    }
    LEN = 0;
  }
  // INFO
  // get error flag value, agnostic of what pattern you prefer
  [[nodiscard]] auto good() const noexcept -> bool { return !ERR_; }
  [[nodiscard]] auto bad() const noexcept -> bool { return ERR_; }
  // set error flag to default
  void reset_error() noexcept { ERR_ = false; }
  // get the amount of characters written so far
  [[nodiscard]] auto written() const noexcept -> std::size_t { return LEN; }
  // get the amount of space left in the buffer
  [[nodiscard]] auto remaining() const noexcept -> std::size_t {
    return Capacity - LEN;
  }
  static constexpr auto capacity() noexcept -> std::size_t { return Capacity; }
  // string view of the current buffer
  [[nodiscard]] auto view() const noexcept -> std::string_view {
    return {BUFFER, LEN};
  }
  // pointer to the raw internal buffer
  [[nodiscard]] auto data() const noexcept -> const char * { return BUFFER; }
  // return the current File Descriptor
  [[nodiscard]] auto fd() const noexcept -> int { return FD; }
  // set the new File Descriptor
  void set_fd(int FDE) noexcept { FD = FDE; }
};
