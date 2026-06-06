// tests/chlib/test_simulator_trace.cpp
//
// Tests the public API of `ch::chlib::UartCapture` declared in
// `include/chlib/simulator_trace.h`.
//
// Note: The header file is named `simulator_trace.h`, but the actual class
// implemented inside is `ch::chlib::UartCapture` — a pure utility class
// (NOT a `Component` subclass) that buffers UART TX output and splits it
// into lines. There is no real UART or hardware simulation involved.

#include "catch_amalgamated.hpp"
#include "chlib/simulator_trace.h"

#include <string>

using ch::chlib::UartCapture;

// ---------------------------------------------------------------------------
// TEST 1: UartCapture accumulates characters, finalizes a line on '\n', and
//         exposes both `output()` and `line_count()` correctly.
// ---------------------------------------------------------------------------
TEST_CASE("UartCapture buffers chars and finalizes on newline",
          "[chlib][simulator_trace][uart]") {
    UartCapture cap;

    // Feed "hello\nworld" one character at a time with tx_valid=1.
    const std::string first  = "hello";
    const std::string second = "world";
    for (char c : first) {
        cap.capture_tick(static_cast<uint64_t>(c), 1);
    }
    cap.capture_tick(static_cast<uint64_t>('\n'), 1);

    // After "\n", line "hello" is committed but "world" is still in buffer.
    REQUIRE(cap.line_count() == 1);
    REQUIRE(cap.line(0) == "hello");

    for (char c : second) {
        cap.capture_tick(static_cast<uint64_t>(c), 1);
    }
    // No terminating '\n' yet — "world" must remain in the un-flushed buffer.
    REQUIRE(cap.line_count() == 1);
    REQUIRE(cap.line(0) == "hello");

    // output() should return: "hello\nworld" (committed line + tail buffer).
    REQUIRE(cap.output() == "hello\nworld");

    // Commit the second line with a '\n'.
    cap.capture_tick(static_cast<uint64_t>('\n'), 1);
    REQUIRE(cap.line_count() == 2);
    REQUIRE(cap.line(0) == "hello");
    REQUIRE(cap.line(1) == "world");
    REQUIRE(cap.output() == "hello\nworld\n");
}

// ---------------------------------------------------------------------------
// TEST 2: `line(i)` returns the correct indexed line, and '\r' is silently
//         dropped from the buffer (Windows-style line endings are tolerated).
// ---------------------------------------------------------------------------
TEST_CASE("UartCapture::line() indexed access and CR is ignored",
          "[chlib][simulator_trace][uart]") {
    UartCapture cap;

    // First line: "abc\r\n" — the '\r' must NOT appear in the committed line.
    for (char c : std::string("abc\r")) {
        cap.capture_tick(static_cast<uint64_t>(c), 1);
    }
    cap.capture_tick(static_cast<uint64_t>('\n'), 1);

    // Second line: "de\n"
    for (char c : std::string("de")) {
        cap.capture_tick(static_cast<uint64_t>(c), 1);
    }
    cap.capture_tick(static_cast<uint64_t>('\n'), 1);

    // Third line: "fg" — uncommitted (no trailing '\n' yet).
    for (char c : std::string("fg")) {
        cap.capture_tick(static_cast<uint64_t>(c), 1);
    }

    REQUIRE(cap.line_count() == 2);
    REQUIRE(cap.line(0) == "abc");  // '\r' was dropped
    REQUIRE(cap.line(1) == "de");

    // Out-of-range index must return empty string (defensive contract).
    REQUIRE(cap.line(2).empty());
    REQUIRE(cap.line(999).empty());

    // Full output: "abc\nde\nfg" (CR filtered, tail buffer appended).
    REQUIRE(cap.output() == "abc\nde\nfg");
}

// ---------------------------------------------------------------------------
// TEST 3: `clear()` resets both the in-progress buffer and the committed
//         lines.
// ---------------------------------------------------------------------------
TEST_CASE("UartCapture::clear() resets buffer and committed lines",
          "[chlib][simulator_trace][uart]") {
    UartCapture cap;

    // Build two complete lines plus a partial third.
    for (char c : std::string("line1")) {
        cap.capture_tick(static_cast<uint64_t>(c), 1);
    }
    cap.capture_tick(static_cast<uint64_t>('\n'), 1);
    for (char c : std::string("line2")) {
        cap.capture_tick(static_cast<uint64_t>(c), 1);
    }
    cap.capture_tick(static_cast<uint64_t>('\n'), 1);
    for (char c : std::string("partial")) {
        cap.capture_tick(static_cast<uint64_t>(c), 1);
    }

    REQUIRE(cap.line_count() == 2);
    REQUIRE_FALSE(cap.output().empty());

    cap.clear();

    REQUIRE(cap.line_count() == 0);
    REQUIRE(cap.line(0).empty());
    REQUIRE(cap.output().empty());

    // After clear(), the instance must still be usable.
    for (char c : std::string("ok")) {
        cap.capture_tick(static_cast<uint64_t>(c), 1);
    }
    cap.capture_tick(static_cast<uint64_t>('\n'), 1);
    REQUIRE(cap.line_count() == 1);
    REQUIRE(cap.line(0) == "ok");
    REQUIRE(cap.output() == "ok\n");
}

// ---------------------------------------------------------------------------
// TEST 4: `capture_tick(c, 0)` must NOT buffer anything when tx_valid=0.
// ---------------------------------------------------------------------------
TEST_CASE("UartCapture ignores ticks when tx_valid is 0",
          "[chlib][simulator_trace][uart]") {
    UartCapture cap;

    // tx_valid=0 — these must be dropped on the floor.
    for (char c : std::string("dropped")) {
        cap.capture_tick(static_cast<uint64_t>(c), 0);
    }

    REQUIRE(cap.line_count() == 0);
    REQUIRE(cap.output().empty());

    // Sanity: subsequent tx_valid=1 ticks still work normally.
    for (char c : std::string("kept")) {
        cap.capture_tick(static_cast<uint64_t>(c), 1);
    }
    REQUIRE(cap.output() == "kept");
}

// ---------------------------------------------------------------------------
// TEST 5: `capture_tick(0, 1)` must NOT buffer when char_code=0 (i.e. the
//         UART is idle but tx_valid is held high — no character available).
// ---------------------------------------------------------------------------
TEST_CASE("UartCapture ignores ticks when char_code is 0",
          "[chlib][simulator_trace][uart]") {
    UartCapture cap;

    // tx_valid=1 but char_code=0 — must not buffer the NUL byte.
    for (int i = 0; i < 5; ++i) {
        cap.capture_tick(0, 1);
    }

    REQUIRE(cap.line_count() == 0);
    REQUIRE(cap.output().empty());

    // Mix: a real character after several idle ticks.
    cap.capture_tick(static_cast<uint64_t>('X'), 1);
    cap.capture_tick(0, 1);
    cap.capture_tick(static_cast<uint64_t>('Y'), 1);
    cap.capture_tick(static_cast<uint64_t>('\n'), 1);

    REQUIRE(cap.line_count() == 1);
    REQUIRE(cap.line(0) == "XY");
    REQUIRE(cap.output() == "XY\n");
}
