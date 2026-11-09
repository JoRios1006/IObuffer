// test_iobuffer.cpp - tests de IObuffer.hpp (POSIX, C++17).
// FULL DISCLOSURE CLAUDE WROTE THIS
//
// Compilar y correr (una línea):
//   g++ -std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined -pthread test_iobuffer.cpp -o test_iobuffer && ./test_iobuffer
//
// CHECK no usa assert(): sigue activo con -DNDEBUG / -O2.

#include "./IObuffer.hpp"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include <fcntl.h>
#include <sys/time.h>

using iobuffer::IOBuffer;

static int g_failed = 0;
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
      ++g_failed;                                                              \
    }                                                                          \
  } while (0)

#define RUN(fn)                                                                \
  do {                                                                         \
    const int before = g_failed;                                               \
    fn();                                                                      \
    std::printf("[%s] %s\n", g_failed == before ? " ok " : "FAIL", #fn);       \
  } while (0)

// ---- helpers ---------------------------------------------------------------
static int tmp_fd() { // archivo temporal anónimo, legible con slurp()
  char path[] = "/tmp/iobuffer_test_XXXXXX";
  const int fd = mkstemp(path);
  unlink(path);
  return fd;
}
static std::string slurp(int fd) {
  std::string out;
  char buf[4096];
  lseek(fd, 0, SEEK_SET);
  for (ssize_t n; (n = ::read(fd, buf, sizeof buf)) > 0;) {
    out.append(buf, static_cast<std::size_t>(n));
  }
  return out;
}

// ---- formato ---------------------------------------------------------------
static void test_format_mixed() {
  const int fd = tmp_fd();
  IOBuffer<256> o(fd);
  o << "a=" << 42 << ' ' << true << ' ' << -7L << ' ' << 3.5 << '\n';
  o.append_hex(255U).append(' ').append_fixed(3.14159, 2);
  CHECK(o.flush() && o.good());
  CHECK(slurp(fd) == "a=42 true -7 3.5\nff 3.14");
  close(fd);
}
static void test_types() {
  const int fd = tmp_fd();
  IOBuffer<256> o(fd);
  o << std::string("str") << std::string_view("sv") << 'c' << false << 255
    << 7U << -1LL << 0.25F;
  o.flush();
  CHECK(slurp(fd) == "strsvcfalse2557-10.25");
  close(fd);
}

// ---- capacidad / auto-flush ------------------------------------------------
static void test_autoflush_many_small() {
  const int fd = tmp_fd();
  IOBuffer<64> o(fd);
  std::string expected;
  for (int i = 0; i < 1000; ++i) {
    o << i << ',';
    expected += std::to_string(i) + ',';
  }
  o.flush();
  CHECK(slurp(fd) == expected && o.good());
  close(fd);
}
static void test_string_fits_after_flush() { // bug histórico: se perdía
  const int fd = tmp_fd();
  {
    IOBuffer<64> o(fd);
    o.append(std::string(40, 'a'));
    o.append(std::string(40, 'b'));
  }
  CHECK(slurp(fd) == std::string(40, 'a') + std::string(40, 'b'));
  close(fd);
}
static void test_string_bigger_than_capacity() { // bug histórico: overflow
  const int fd = tmp_fd();
  const std::string big(1000, 'z');
  {
    IOBuffer<64> o(fd);
    o << "H" << std::string_view(big) << "T";
  }
  CHECK(slurp(fd) == "H" + big + "T");
  close(fd);
}
static void test_exact_capacity() {
  const int fd = tmp_fd();
  {
    IOBuffer<64> o(fd);
    o.append(std::string(64, 'x'));
    o << 'y';
  }
  CHECK(slurp(fd) == std::string(64, 'x') + "y");
  close(fd);
}
static void test_null_and_empty() {
  const int fd = tmp_fd();
  IOBuffer<64> o(fd);
  o << static_cast<const char *>(nullptr) << nullptr << "" << std::string_view()
    << "ok";
  o.flush();
  CHECK(slurp(fd) == "ok");
  close(fd);
}

// ---- ciclo de vida ---------------------------------------------------------
static void test_flush_in_destructor() {
  const int fd = tmp_fd();
  {
    IOBuffer<64> o(fd);
    o << "dtor";
  }
  CHECK(slurp(fd) == "dtor");
  close(fd);
}
static void test_flush_return_value() { // bug histórico: retornaba basura
  const int fd = tmp_fd();
  IOBuffer<64> o(fd);
  o << "hola";
  CHECK(o.flush() == true);
  CHECK(o.flush() == true); // vacío: no escribe, sigue ok
  close(fd);
}
static void test_discard_and_wipe() {
  const int fd = tmp_fd();
  IOBuffer<64> o(fd);
  o << "secret";
  o.discard();
  CHECK(o.written() == 0 && o.flush() && slurp(fd).empty());
  close(fd);

  IOBuffer<64> w(-1);
  w << "secret";
  w.wipe();
  CHECK(w.written() == 0);
  bool all_zero = true;
  for (std::size_t i = 0; i < w.capacity(); ++i) {
    all_zero = all_zero && (w.data()[i] == 0);
  }
  CHECK(all_zero);
}
static void test_accessors() {
  IOBuffer<64> o(-1);
  CHECK(o.capacity() == 64 && o.remaining() == 64 && o.written() == 0);
  o << "abc";
  CHECK(o.written() == 3 && o.remaining() == 61 && o.view() == "abc");
  o.discard();
}

// ---- fd y errores ----------------------------------------------------------
static void test_set_fd_no_flush() {
  const int a = tmp_fd();
  const int b = tmp_fd();
  IOBuffer<64> o(a);
  o << "uno";
  o.flush();
  o << "dos";
  o.set_fd(b); // lo pendiente va al nuevo fd
  o.flush();
  CHECK(slurp(a) == "uno" && slurp(b) == "dos" && o.fd() == b);
  close(a);
  close(b);
}
static void test_flush_to_other_fd() {
  const int a = tmp_fd();
  const int b = tmp_fd();
  IOBuffer<64> o(a);
  o << "log";
  o.flush(b);
  o << "out";
  o.flush();
  CHECK(o.fd() == a && slurp(b) == "log" && slurp(a) == "out");
  close(a);
  close(b);
}
static void test_error_detected_and_sticky() { // bug histórico: nunca detectaba
  IOBuffer<64> o(-1);
  o << "x";
  CHECK(!o.flush() && o.bad() && !o.good());
  const int fd = tmp_fd();
  o.set_fd(fd);
  o << "perdido";
  CHECK(!o.flush());        // sigue en error: descarta
  CHECK(slurp(fd).empty()); // y no escribió nada
  o.reset_error();
  o << "ok";
  CHECK(o.flush() && o.good() && slurp(fd) == "ok");
  close(fd);
}
static void test_error_on_big_string() {
  IOBuffer<64> o(-1);
  o.append(std::string(500, 'x')); // write directo a fd inválido
  CHECK(o.bad());
}

// ---- writes parciales y EINTR ----------------------------------------------
static volatile std::sig_atomic_t g_signals = 0;
static void on_alarm(int) { g_signals = g_signals + 1; }

static void test_partial_writes_and_eintr() {
  int p[2];
  CHECK(pipe(p) == 0);

  struct sigaction sa {};
  sa.sa_handler = on_alarm;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0; // sin SA_RESTART: write puede devolver parcial o EINTR
  sigaction(SIGALRM, &sa, nullptr);
  const struct itimerval tick = {{0, 500}, {0, 500}}; // cada 500 us
  setitimer(ITIMER_REAL, &tick, nullptr);

  const std::size_t N = 2'000'000;
  const std::string big(N, 'q');
  std::string got;
  std::thread reader([&] {
    sigset_t m;
    sigemptyset(&m);
    sigaddset(&m, SIGALRM);
    pthread_sigmask(SIG_BLOCK, &m, nullptr); // las señales caen en el escritor
    char buf[1000];
    for (ssize_t n; (n = ::read(p[0], buf, sizeof buf)) > 0;) {
      got.append(buf, static_cast<std::size_t>(n));
      if (got.size() % 50000 < 1000) {
        usleep(200); // lector lento: el pipe se llena
      }
    }
  });
  {
    IOBuffer<4096> o(p[1]);
    o.append(big);
    o.append("END");
    CHECK(o.flush() && o.good());
  }
  const struct itimerval off = {};
  setitimer(ITIMER_REAL, &off, nullptr);
  close(p[1]);
  reader.join();
  close(p[0]);

  CHECK(got.size() == N + 3);
  CHECK(got.compare(0, N, big) == 0 && got.substr(N) == "END");
  std::printf("       (señales entregadas durante la prueba: %d)\n",
              static_cast<int>(g_signals));
}

int main() {
  RUN(test_format_mixed);
  RUN(test_types);
  RUN(test_autoflush_many_small);
  RUN(test_string_fits_after_flush);
  RUN(test_string_bigger_than_capacity);
  RUN(test_exact_capacity);
  RUN(test_null_and_empty);
  RUN(test_flush_in_destructor);
  RUN(test_flush_return_value);
  RUN(test_discard_and_wipe);
  RUN(test_accessors);
  RUN(test_set_fd_no_flush);
  RUN(test_flush_to_other_fd);
  RUN(test_error_detected_and_sticky);
  RUN(test_error_on_big_string);
  RUN(test_partial_writes_and_eintr);

  std::printf(g_failed == 0 ? "\nTODO OK\n" : "\n%d CHECK(s) fallaron\n", g_failed);
  return g_failed == 0 ? 0 : 1;
}
