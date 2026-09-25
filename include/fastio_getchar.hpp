#pragma once
// ============================================================================
//  fastio_getchar.hpp  —  写法 3：getchar / putchar 手写整型读写（文1 §2 / 文2 §一）
//
//  实测：读 432 ms（1.3× cin）、写 442 ms（1.07× cout）
//  （100 MiB / 998 万个 9 位整数、正负随机；基线 cin 565 ms / cout 473 ms）
//
//  最经典的手写快读：
//      int x = 0, f = 1; char c = getchar();
//      while (c < '0' || c > '9') { if (c == '-') f = -1; c = getchar(); }
//      while (c >= '0' && c <= '9') { x = x * 10 + (c ^ 48); c = getchar(); }
//  快在跳过了 scanf 的格式解析，但 getchar 每次都要加锁（线程安全），
//  所以还有 getchar_unlocked 一档（见 fastio_getchar_unlocked.hpp）。
//
//  用法：
//      #include "fastio_getchar.hpp"
//      int n = fio_getchar::in.read<int>();
//      fio_getchar::out << n << '\n';
//
//  减分支选项（#define 后再 include）：
//      FASTIO_NO_EOF_CHECK     忽略 EOF：跳过空白/回退处不再判 EOF（数据必须规范）
//      FASTIO_ASSUME_UNSIGNED  保证没有负号：读、写两侧的符号分支整体消失
//  支持 __int128（GNU 扩展类型）。
// ============================================================================

#include <cstddef>
#include <cstdio>
#include <string>
#include <type_traits>

namespace fio_getchar {
// ---- __int128 兼容：严格 -std=c++17 下标准萃取不认识这个 GNU 扩展类型 ------
template <class T> struct uns_of { using type = typename std::make_unsigned<T>::type; };
template <class T> struct is_signed_of : std::is_signed<T> {};
template <class T> struct is_int_of
    : std::integral_constant<bool, std::is_integral<T>::value> {};
#if defined(__SIZEOF_INT128__)
template <> struct uns_of<__int128_t> { using type = __uint128_t; };
template <> struct uns_of<__uint128_t> { using type = __uint128_t; };
template <> struct is_signed_of<__int128_t> : std::true_type {};
template <> struct is_int_of<__int128_t> : std::true_type {};
template <> struct is_int_of<__uint128_t> : std::true_type {};
#endif
template <class T> using uns_t = typename uns_of<T>::type;


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

    inline int gc() { return std::fgetc(fp_); }   // fp_==stdin 时即 getchar()

    template <class T>
    T read() {
        using U = uns_t<T>;
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
        const U mask = U(0) - U(neg && is_signed_of<T>::value);
        return T((v ^ mask) - mask);   // 无分支取负，INT_MIN 安全
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
    typename std::enable_if<is_int_of<T>::value || std::is_floating_point<T>::value,
                            Reader&>::type read(T& x) {
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

    inline void put(char c) { std::fputc(c, fp_); }   // fp_==stdout 时即 putchar()

    template <class T>
    typename std::enable_if<is_int_of<T>::value && !std::is_same<T, char>::value,
                            void>::type
    write(T x) {
        using U = uns_t<T>;
        U v;
#ifdef FASTIO_ASSUME_UNSIGNED
        v = U(x);  // 用户保证没有负数：符号分支编译期消失
#else
        if (is_signed_of<T>::value && x < 0) { put('-'); v = U(U(0) - U(x)); }
        else v = U(x);
#endif
        char buf[sizeof(U) > 8 ? 48 : 24];   // __int128 最长 39 位
        int k = 0;
        do { buf[k++] = char('0' + int(v % 10)); v /= 10; } while (v);
        while (k) put(buf[--k]);
    }
    void write(char c) { put(c); }
    void write(const char* s) { std::fputs(s, fp_); }
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

}  // namespace fio_getchar
