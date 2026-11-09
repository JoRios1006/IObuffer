# IObuffer

Small library to batch and print stuff in one go.

IObuffer is a compact, header-only C++17 utility designed to reduce expensive write calls by buffering output in a fixed-size internal array before flushing to a file descriptor. It is useful for logs, diagnostics, and other output-heavy code paths where batching writes improves efficiency without adding much complexity.

## Features

- Header-only C++ library
- Fixed-capacity buffering with compile-time sizing
- Append strings, characters, booleans, integers, and floating-point values
- Fixed-precision floating-point output
- Hex formatting for integer values
- Stream-style usage via `operator<<`
- Explicit flush, discard, and reset controls
- Safe write handling for `write(2)` with `EINTR` retry logic

## Requirements

- C++17 or newer
- Unix-like platform (`unistd.h` and `write(2)` support)
- xmake is used for building and running tests

## Installation

Clone the repository and include the header directly:

```bash
git clone https://github.com/JoRios1006/IObuffer.git
cd IObuffer
```

Then add the include directory to your compiler include path:

```cpp
#include <iobuffer/IObuffer.hpp>
```

If you are using xmake in another project, add the library as a dependency:

```lua
target("my_app")
    set_kind("binary")
    add_files("src/main.cpp")
    add_deps("iobuffer")
    add_includedirs("include")
target_end()
```

## Quick start

```cpp
#include <iobuffer/IObuffer.hpp>

int main() {
    iobuffer::IOBuffer<1024> out;

    out << "Hello, world!\n";
    out.append("Value: ").append(42).append("\n");
    out.append_fixed(3.14159, 2).append("\n");
    out.append_hex(255).append("\n");

    out.flush();
    return 0;
}
```

This produces output similar to:

```text
Hello, world!
Value: 42
3.14
ff
```

## API overview

The library centers around the `iobuffer::IOBuffer<Capacity>` template.

### Construction

```cpp
iobuffer::IOBuffer<1024> buffer;              // stdout by default
 iobuffer::IOBuffer<4096> errlog(STDERR_FILENO);
```

### Appending data

```cpp
buffer.append("hello");
buffer.append('!');
buffer.append(true);
buffer.append(123);
buffer.append_fixed(12.5, 1);
buffer.append_hex(255);
```

### Stream-style output

```cpp
buffer << "message" << 123 << '\n';
```

### Flushing and state

```cpp
buffer.flush();
buffer.flush(STDERR_FILENO);

bool ok = buffer.good();
buffer.reset_error();
```

Additional helpers include:

- `discard()` to drop pending data without writing
- `wipe()` to clear the internal buffer contents
- `written()` to check how many bytes are currently buffered
- `remaining()` to see available capacity
- `view()` and `data()` to inspect the current buffer contents
- `fd()` and `set_fd()` to manage the target descriptor

## Building and testing

This repository uses xmake.

```bash
xmake
xmake test
```

To build only the library target:

```bash
xmake build iobuffer
```

To run the bundled test binary:

```bash
xmake test
```

The project includes a sanitizer-enabled test target in `xmake.lua`, and the tests are located under the `tests` directory.

## License

This project is licensed under the GNU General Public License v2.0 or later. See the `LICENSE` file for details.

## Notes

The implementation intentionally keeps the buffer compact and dependency-light. It is best suited for small to medium output workloads where a few flushes are preferred over one syscall per item.
