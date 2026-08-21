#pragma once
// ============================================================================
//  fastio_fread.hpp  —  写法 5：fread 大缓冲快读 + fwrite 大缓冲快写
//                       （文1 §3 / 文1 二§3、二§4 / 文2 §二、§四）
//
//  实测：读 143 ms（3.9× cin）、写 148 ms（3.2× cout，四位打表；纯 fwrite 251 ms）
//  （100 MiB / 998 万个 9 位整数、正负随机；基线 cin 565 ms / cout 473 ms）
//
//  特点：不依赖 mmap，管道 / 终端 / Windows 全都能用，是最通用的一档。
//  经典宏写法：
//      #define gc() (p1==p2 && (p2=(p1=buf)+fread(buf,1,SZ,stdin), p1==p2) ? EOF : *p1++)
//  这里封装成类，并保留 INT_MIN 安全与四位打表输出。
//
//  用法：
//      #include "fastio_fread.hpp"
//      int n = fio_fread::in.read<int>();
//      fio_fread::out << n << '\n';        // 析构自动 flush
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>

namespace fio_fread {

// ---- 四位数字表：0000..9999 -> 打包好的 4 个 ASCII 字节 --------------------
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
    static constexpr size_t SZ = 1 << 20;  // 1 MiB 输入缓冲

    Reader() : fp_(stdin) { buf_ = static_cast<char*>(std::malloc(SZ)); p1_ = p2_ = buf_; }
    explicit Reader(FILE* fp) : Reader() { fp_ = fp ? fp : stdin; }
    ~Reader() { std::free(buf_); if (own_) std::fclose(own_); }
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    void attach(FILE* fp) { fp_ = fp ? fp : stdin; p1_ = p2_ = buf_; }
    bool open(const char* path) {
        FILE* fp = std::fopen(path, "rb");
        if (!fp) return false;
        attach(fp);
        own_ = fp;
        return true;
    }

    // 经典 gc()：缓冲空了就 fread 一大块
    inline int gc() {
        if (p1_ == p2_) {
            p2_ = (p1_ = buf_) + std::fread(buf_, 1, SZ, fp_);
            if (p1_ == p2_) return EOF;
        }
        return (unsigned char)*p1_++;
    }
    inline int peek() {
        int c = gc();
        if (c != EOF) --p1_;
        return c;
    }
    bool eof() { return peek_nonspace() == EOF; }

    template <class T>
    T read() {
        using U = typename std::make_unsigned<T>::type;
        int c = gc();
        while (c != EOF && c <= ' ') c = gc();
        bool neg = false;
        if (c == '-') { neg = true; c = gc(); }
        else if (c == '+') { c = gc(); }
        U v = 0;
        while ((unsigned)(c - '0') < 10u) {
            v = U(v * 10 + U(c ^ 48));
            c = gc();
        }
        if (c != EOF) --p1_;
        const U mask = U(0) - U(neg && std::is_signed<T>::value);
        return T((v ^ mask) - mask);  // INT_MIN / LLONG_MIN 安全
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
    int peek_nonspace() {
        int c = gc();
        while (c != EOF && c <= ' ') c = gc();
        if (c != EOF) --p1_;
        return c;
    }
    FILE* fp_;
    FILE* own_ = nullptr;
    char* buf_;
    char* p1_;
    char* p2_;
};

// ============================== 写 =========================================
class Writer {
public:
    static constexpr size_t SZ = 1 << 22;  // 4 MiB 输出缓冲

    Writer() : fp_(stdout) { buf_ = static_cast<char*>(std::malloc(SZ + 64)); cur_ = buf_; }
    explicit Writer(FILE* fp) : Writer() { fp_ = fp ? fp : stdout; }
    ~Writer() { flush(); std::free(buf_); if (own_) std::fclose(own_); }
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    void attach(FILE* fp) { flush(); fp_ = fp ? fp : stdout; }
    bool open(const char* path) {
        FILE* fp = std::fopen(path, "wb");
        if (!fp) return false;
        attach(fp);
        own_ = fp;
        return true;
    }
    void flush() {
        if (cur_ != buf_) {
            std::fwrite(buf_, 1, size_t(cur_ - buf_), fp_);
            cur_ = buf_;
        }
        std::fflush(fp_);   // 保证真的落到流里（交互题必须）
    }
    inline void put(char c) {
        if (cur_ == buf_ + SZ) flush();
        *cur_++ = c;
    }
    void put_raw(const char* s, size_t n) {
        if (n >= SZ) { flush(); std::fwrite(s, 1, n, fp_); return; }
        if (size_t(buf_ + SZ - cur_) < n) flush();
        std::memcpy(cur_, s, n);
        cur_ += n;
    }

    // 四位打表：每次取 x % 10000，一条 32 位 store 落 4 个 ASCII
    template <class U>
    void write_uns(U x) {
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
        if (std::is_signed<T>::value && x < 0) { put('-'); write_uns(U(U(0) - U(x))); }
        else write_uns(U(x));
    }
    void write(char c) { put(c); }
    void write(const char* s) { put_raw(s, std::strlen(s)); }
    void write(const std::string& s) { put_raw(s.data(), s.size()); }
    void write(double x) {
        if (size_t(buf_ + SZ - cur_) < 400) flush();
        int k = std::snprintf(cur_, 400, "%.6f", x);
        if (k > 0) cur_ += k;
    }
    template <class T>
    Writer& operator<<(const T& x) { write(x); return *this; }
    template <class T>
    void write_n(const T* a, size_t n, char sep = ' ', char last = '\n') {
        for (size_t i = 0; i < n; ++i) { write(a[i]); put(i + 1 == n ? last : sep); }
    }

private:
    FILE* fp_;
    FILE* own_ = nullptr;
    char* buf_;
    char* cur_;
};

inline Reader in;
inline Writer out;

}  // namespace fio_fread
