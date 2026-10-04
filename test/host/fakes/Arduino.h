#ifndef FAKE_ARDUINO_H
#define FAKE_ARDUINO_H

// Host stand-in for the parts of the Arduino-ESP32 core the firmware uses. Behaviour that tests
// steer (clock, restarts, Wi-Fi state, files, network replies) lives in fake::, see Fake.h.

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

typedef bool boolean;
typedef uint8_t byte;

#define PROGMEM
#define pgm_read_byte(addr) (*(const unsigned char*)(addr))
#define pgm_read_word(addr) (*(const unsigned short*)(addr))
#define pgm_read_dword(addr) (*(const unsigned long*)(addr))
#define pgm_read_ptr(addr) (*(void* const*)(addr))
#define pgm_read_pointer(addr) ((void*)pgm_read_ptr(addr))
#define strlen_P strlen
#define memcpy_P memcpy

#define DEG_TO_RAD 0.017453292519943295769236907684886
#define radians(deg) ((deg) * DEG_TO_RAD)

class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define FPSTR(s) (reinterpret_cast<const __FlashStringHelper*>(s))

class String {
public:
    String(const char* s = "") : _s(s ? s : "") {}
    String(const __FlashStringHelper* s) : _s(reinterpret_cast<const char*>(s)) {}
    String(const String&) = default;
    String& operator=(const String&) = default;
    explicit String(int v) : _s(std::to_string(v)) {}
    explicit String(unsigned v) : _s(std::to_string(v)) {}

    const char* c_str() const { return _s.c_str(); }
    unsigned length() const { return static_cast<unsigned>(_s.size()); }
    bool isEmpty() const { return _s.empty(); }
    bool reserve(unsigned n) {
        _s.reserve(n);
        return true;
    }
    bool concat(const char* s) {
        _s += s;
        return true;
    }
    bool concat(char c) {
        _s += c;
        return true;
    }
    bool concat(const char* s, unsigned n) {
        _s.append(s, n);
        return true;
    }
    String& operator+=(const char* s) { return concat(s), *this; }
    String& operator+=(char c) { return concat(c), *this; }
    String& operator+=(const String& s) { return concat(s.c_str()), *this; }
    String& operator+=(const __FlashStringHelper* s) { return concat(reinterpret_cast<const char*>(s)), *this; }
    bool operator==(const char* s) const { return _s == s; }
    bool operator==(const String& s) const { return _s == s._s; }
    bool operator!=(const char* s) const { return _s != s; }
    char operator[](unsigned i) const { return _s[i]; }
    int indexOf(const char* s) const {
        const size_t p = _s.find(s);
        return p == std::string::npos ? -1 : static_cast<int>(p);
    }

private:
    std::string _s;
};

class Print;

class Printable {
public:
    virtual ~Printable() = default;
    virtual size_t printTo(Print& p) const = 0;
};

class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t c) = 0;
    virtual size_t write(const uint8_t* buffer, size_t size) {
        size_t n = 0;
        while (size--) n += write(*buffer++);
        return n;
    }
    size_t write(const char* s) { return write(reinterpret_cast<const uint8_t*>(s), std::strlen(s)); }
    size_t print(const char* s) { return write(s); }
    size_t print(char c) { return write(static_cast<uint8_t>(c)); }
    size_t print(const String& s) { return write(s.c_str()); }
    size_t println(const char* s = "") { return print(s) + print('\n'); }
    virtual void flush() {}
};

class Stream : public Print {
public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
    size_t readBytes(char* buffer, size_t length) {
        size_t n = 0;
        int c;
        while (n < length && (c = read()) >= 0) buffer[n++] = static_cast<char>(c);
        return n;
    }
    size_t readBytes(uint8_t* buffer, size_t length) { return readBytes(reinterpret_cast<char*>(buffer), length); }
    void setTimeout(unsigned long) {}
};

class IPAddress {
public:
    IPAddress() = default;
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : _v((a << 24) | (b << 16) | (c << 8) | d) {}
    bool fromString(const char* s);
    String toString() const;
    bool operator==(const IPAddress& o) const { return _v == o._v; }
    bool operator!=(const IPAddress& o) const { return _v != o._v; }

private:
    uint32_t _v = 0;
};
#define INADDR_NONE IPAddress(0, 0, 0, 0)

class HardwareSerial : public Print {
public:
    void begin(unsigned long) {}
    size_t write(uint8_t) override { return 1; }
};
extern HardwareSerial Serial;

class EspClass {
public:
    // The real one never returns; here it is counted (fake::restarts) and execution goes on.
    void restart();
    uint32_t getFreeHeap() { return 150 * 1024; }
};
extern EspClass ESP;

unsigned long millis();
void delay(unsigned long ms);
float temperatureRead();
uint32_t getCpuFrequencyMhz();
void configTzTime(const char* tz, const char* server1, const char* server2 = nullptr,
                  const char* server3 = nullptr);

#include "Fake.h"

#endif
