#pragma once
// ============================================================================
//  fastio_streambuf.hpp  —  写法 6：基于 std::streambuf 的快读快写（文2 §五）
//
//  实测：读 148 ms（3.8× cin）、写 154 ms（3.1× cout）
//  （100 MiB / 998 万个 9 位整数、正负随机；基线 cin 565 ms / cout 473 ms）
//
//  思路：绕开 istream/ostream 的格式化层，直接拿 rdbuf() 用
//        inbuf->sgetn(buf, N) 批量取字节、outbuf->sputn(buf, N) 批量写字节。
//        纯 C++ 标准库，不用 mmap、不用 fread，Windows 也能跑；
//        速度与 fread 档基本持平，胜在"纯 STL"。
//
//  用法：
//      #include "fastio_streambuf.hpp"
//      int n = fio_sbuf::in.read<int>();
//      fio_sbuf::out << n << '\n';         // 析构自动 flush
//      // 换绑文件： fio_sbuf::in.attach(fin.rdbuf());
//
//  减分支选项（#define 后再 include）：
//      FASTIO_NO_EOF_CHECK     忽略 EOF：跳过空白/回退处不再判 EOF（数据必须规范）
//      FASTIO_ASSUME_UNSIGNED  保证没有负号：读、写两侧的符号分支整体消失
// ============================================================================

#include <cstdint>
#include <cstring>
#include <iostream>
#include <streambuf>
#include <string>
#include <type_traits>

namespace fio_sbuf {

struct QuadTable {
    uint32_t v[10000];
    QuadTable() {
        for (int n = 0; n < 10000; ++n)
            v[n] = uint32_t(n / 1000 % 10 + '0') |
                   (uint32_t(n / 100 % 10 + '0') << 8) |
                   (uint32_t(n / 10 % 10 + '0') << 16) |
                   (uint32_t(n % 10 + '0') << 24);
    }
};
inline const QuadTable quad_tbl{};

// ============================== 读 =========================================
class Reader {
public:
    static constexpr int SZ = 1 << 20;

    Reader() : buf_(new char[SZ]), p1_(buf_), p2_(buf_) {
        std::ios::sync_with_stdio(false);
        std::cin.tie(nullptr);
        sb_ = std::cin.rdbuf();
    }
    explicit Reader(std::streambuf* sb) : Reader() { sb_ = sb; }
    ~Reader() { delete[] buf_; }
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    void attach(std::streambuf* sb) { sb_ = sb; p1_ = p2_ = buf_; }

    inline int gc() {
        if (p1_ == p2_) {
            std::streamsize n = sb_->sgetn(buf_, SZ);
            p1_ = buf_;
            p2_ = buf_ + (n > 0 ? n : 0);
            if (p1_ == p2_) return EOF;
        }
        return (unsigned char)*p1_++;
    }
    bool eof() {
        int c = gc();
        while (c != EOF && c <= ' ') c = gc();
        if (c != EOF) { --p1_; return false; }
        return true;
    }

    template <class T>
    T read() {
        using U = typename std::make_unsigned<T>::type;
        int c = gc();
#ifdef FASTIO_NO_EOF_CHECK
        while (c <= ' ') c = gc();        // 忽略 EOF：空白循环不判 EOF
#else
        while (c != EOF && c <= ' ') c = gc();
#endif
#ifdef FASTIO_ASSUME_UNSIGNED
        constexpr bool neg = false;       // 用户承诺无负号：符号分支编译期消失
#else
        bool neg = false;
        if (c == '-') { neg = true; c = gc(); }
        else if (c == '+') { c = gc(); }
#endif
        U v = 0;
        while ((unsigned)(c - '0') < 10u) { v = U(v * 10 + U(c ^ 48)); c = gc(); }
#ifdef FASTIO_NO_EOF_CHECK
        --p1_;                            // 承诺读不到 EOF：直接回退非数字字符
#else
        if (c != EOF) --p1_;
#endif
        const U mask = U(0) - U(neg && std::is_signed<T>::value);
        return T((v ^ mask) - mask);  // INT_MIN 安全
    }

    Reader& read(char& ch) {
        int c = gc();
        while (c != EOF && c <= ' ') c = gc();
        ch = c == EOF ? '\0' : char(c);
        return *this;
    }
    Reader& read(std::string& s) {
        s.clear();
        int c = gc();
        while (c != EOF && c <= ' ') c = gc();
        while (c != EOF && c > ' ') { s.push_back(char(c)); c = gc(); }
        if (c != EOF) --p1_;
        return *this;
    }
    template <class T>
    typename std::enable_if<std::is_arithmetic<T>::value, Reader&>::type read(T& x) {
        x = read<T>();
        return *this;
    }
    template <class A, class B, class... R>
    Reader& read(A& a, B& b, R&... r) { read(a); return read(b, r...); }
    template <class T>
    Reader& read_n(T* a, size_t n) {
        for (size_t i = 0; i < n; ++i) read(a[i]);
        return *this;
    }
    template <class T>
    Reader& operator>>(T& x) { return read(x); }

private:
    std::streambuf* sb_;
    char* buf_;
    char* p1_;
    char* p2_;
};

// ============================== 写 =========================================
class Writer {
public:
    static constexpr size_t SZ = 1 << 22;

    Writer() : buf_(new char[SZ + 64]), cur_(buf_) {
        std::ios::sync_with_stdio(false);
        sb_ = std::cout.rdbuf();
    }
    explicit Writer(std::streambuf* sb) : Writer() { sb_ = sb; }
    ~Writer() { flush(); delete[] buf_; }
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    void attach(std::streambuf* sb) { flush(); sb_ = sb; }
    void flush() {
        if (cur_ != buf_) { sb_->sputn(buf_, cur_ - buf_); cur_ = buf_; }
        sb_->pubsync();
    }
    inline void put(char c) {
        if (cur_ == buf_ + SZ) flush();
        *cur_++ = c;
    }
    void put_raw(const char* s, size_t n) {
        if (n >= SZ) { flush(); sb_->sputn(s, std::streamsize(n)); return; }
        if (size_t(buf_ + SZ - cur_) < n) flush();
        std::memcpy(cur_, s, n);
        cur_ += n;
    }

    template <class U>
    void write_uns(U x) {   // 同样用四位打表
        if (size_t(buf_ + SZ - cur_) < 24) flush();
        const uint32_t* tb = quad_tbl.v;
        char tmp[48];
        char* e = tmp + 24;
        char* q = e;
        while (x >= 10000) {
            q -= 4;
            uint32_t w = tb[unsigned(x % 10000)];
            std::memcpy(q, &w, 4);
            x /= 10000;
        }
        unsigned head = unsigned(x);
        uint32_t w = tb[head];
        char four[4];
        std::memcpy(four, &w, 4);
        int skip = head >= 1000 ? 0 : head >= 100 ? 1 : head >= 10 ? 2 : 3;
        q -= (4 - skip);
        std::memcpy(q, four + skip, size_t(4 - skip));
        size_t len = size_t(e - q);
        std::memcpy(cur_, q, 24);
        cur_ += len;
    }

    template <class T>
    typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, char>::value,
                            void>::type
    write(T x) {
        using U = typename std::make_unsigned<T>::type;
#ifdef FASTIO_ASSUME_UNSIGNED
        write_uns(U(x));  // 用户保证没有负数：符号分支编译期消失
#else
        if (std::is_signed<T>::value && x < 0) { put('-'); write_uns(U(U(0) - U(x))); }
        else write_uns(U(x));
#endif
    }
    void write(char c) { put(c); }
    void write(const char* s) { put_raw(s, std::strlen(s)); }
    void write(const std::string& s) { put_raw(s.data(), s.size()); }
    template <class T>
    Writer& operator<<(const T& x) { write(x); return *this; }
    template <class T>
    void write_n(const T* a, size_t n, char sep = ' ', char last = '\n') {
        for (size_t i = 0; i < n; ++i) { write(a[i]); put(i + 1 == n ? last : sep); }
    }

private:
    std::streambuf* sb_;
    char* buf_;
    char* cur_;
};

inline Reader in;
inline Writer out;

}  // namespace fio_sbuf
