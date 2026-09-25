#pragma once
// ============================================================================
//  fastio_getchar_unlocked.hpp  —  写法 4：getchar_unlocked / putchar_unlocked（文1 §2.1）
//
//  实测：读 302 ms（1.9× cin）、写 302 ms（1.6× cout）
//  （100 MiB / 998 万个 9 位整数、正负随机；基线 cin 565 ms / cout 473 ms）
//
//  与写法 3 唯一的区别：用「不加锁」版本。标准 getchar/putchar 每次调用都要
//  给 FILE* 上锁保证线程安全，单线程的 OI 程序完全不需要，去掉锁就白赚一截。
//    · Linux/macOS：getchar_unlocked / putchar_unlocked（POSIX）
//    · Windows/MSVC：_getchar_nolock / _putchar_nolock
//    · 兜底：getc_unlocked(stdin) / 普通 getchar
//  注意：**多线程环境下不要用**。
//
//  用法：
//      #include "fastio_getchar_unlocked.hpp"
//      int n = fio_gcu::in.read<int>();
//      fio_gcu::out << n << '\n';
//
//  减分支选项（#define 后再 include）：
//      FASTIO_NO_EOF_CHECK     忽略 EOF：跳过空白/回退处不再判 EOF（数据必须规范）
//      FASTIO_ASSUME_UNSIGNED  保证没有负号：读、写两侧的符号分支整体消失
// ============================================================================

#include <cstddef>
#include <cstdio>
#include <string>
#include <type_traits>

namespace fio_gcu {

// ---- 平台适配 --------------------------------------------------------------
#if defined(_WIN32) || defined(_MSC_VER)
  #define FIO_GETC(fp)     ((void)(fp), _getchar_nolock())
  #define FIO_PUTC(c, fp)  ((void)(fp), _putchar_nolock(c))
#elif defined(getc_unlocked) || defined(__unix__) || defined(__linux__) || defined(__APPLE__)
  #define FIO_GETC(fp)     getc_unlocked(fp)
  #define FIO_PUTC(c, fp)  putc_unlocked(c, fp)
#else
  #define FIO_GETC(fp)     std::fgetc(fp)
  #define FIO_PUTC(c, fp)  std::fputc(c, fp)
#endif

class Reader {
public:
    Reader() : fp_(stdin) {}
    explicit Reader(FILE* fp) : fp_(fp ? fp : stdin) {}
    ~Reader() { if (own_) std::fclose(own_); }
    void attach(FILE* fp) { fp_ = fp ? fp : stdin; }
    bool open(const char* path) {
        FILE* fp = std::fopen(path, "rb");
        if (!fp) return false;
        fp_ = own_ = fp;
        return true;
    }

    inline int gc() { return FIO_GETC(fp_); }

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
        std::ungetc(c, fp_);              // 承诺读不到 EOF：直接回退非数字字符
#else
        if (c != EOF) std::ungetc(c, fp_);
#endif
        const U mask = U(0) - U(neg && std::is_signed<T>::value);
        return T((v ^ mask) - mask);
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
        if (c != EOF) std::ungetc(c, fp_);
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
    bool eof() {
        int c = gc();
        while (c != EOF && c <= ' ') c = gc();
        if (c == EOF) return true;
        std::ungetc(c, fp_);
        return false;
    }

private:
    FILE* fp_;
    FILE* own_ = nullptr;
};

class Writer {
public:
    Writer() : fp_(stdout) {}
    explicit Writer(FILE* fp) : fp_(fp ? fp : stdout) {}
    ~Writer() { flush(); if (own_) std::fclose(own_); }
    void attach(FILE* fp) { fp_ = fp ? fp : stdout; }
    bool open(const char* path) {
        FILE* fp = std::fopen(path, "wb");
        if (!fp) return false;
        fp_ = own_ = fp;
        return true;
    }

    inline void put(char c) { FIO_PUTC(c, fp_); }

    template <class T>
    typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, char>::value,
                            void>::type
    write(T x) {
        using U = typename std::make_unsigned<T>::type;
        U v;
#ifdef FASTIO_ASSUME_UNSIGNED
        v = U(x);  // 用户保证没有负数：符号分支编译期消失
#else
        if (std::is_signed<T>::value && x < 0) { put('-'); v = U(U(0) - U(x)); }
        else v = U(x);
#endif
        char buf[24];
        int k = 0;
        do { buf[k++] = char('0' + int(v % 10)); v /= 10; } while (v);
        while (k) put(buf[--k]);
    }
    void write(char c) { put(c); }
    void write(const char* s) { while (*s) put(*s++); }
    void write(const std::string& s) { for (char c : s) put(c); }
    template <class T>
    Writer& operator<<(const T& x) { write(x); return *this; }
    template <class T>
    void write_n(const T* a, size_t n, char sep = ' ', char last = '\n') {
        for (size_t i = 0; i < n; ++i) { write(a[i]); put(i + 1 == n ? last : sep); }
    }
    void flush() { std::fflush(fp_); }

private:
    FILE* fp_;
    FILE* own_ = nullptr;
};

inline Reader in;
inline Writer out;

}  // namespace fio_gcu
