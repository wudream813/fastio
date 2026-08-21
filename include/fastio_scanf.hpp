#pragma once
// ============================================================================
//  fastio_scanf.hpp  —  写法 2：scanf / printf（C 风格格式化 IO）
//
//  实测：读 746 ms（0.76×）、写 560 ms（0.85×）—— 比 cin 还慢，
//  （100 MiB / 998 万个 9 位整数、正负随机；基线 cin 565 ms / cout 473 ms）
//
//  因为每次都要解析格式串。列在这里是为了让对照表完整。
//
//  用法：
//      #include "fastio_scanf.hpp"
//      int n = fio_scanf::in.read<int>();
//      fio_scanf::out << n << '\n';
// ============================================================================

#include <cstddef>
#include <cstdio>
#include <string>
#include <type_traits>

namespace fio_scanf {

// 类型 -> 格式串
template <class T> struct Fmt;
template <> struct Fmt<int>                { static const char* in() { return "%d";   } static const char* out() { return "%d";   } };
template <> struct Fmt<unsigned>           { static const char* in() { return "%u";   } static const char* out() { return "%u";   } };
template <> struct Fmt<long>               { static const char* in() { return "%ld";  } static const char* out() { return "%ld";  } };
template <> struct Fmt<unsigned long>      { static const char* in() { return "%lu";  } static const char* out() { return "%lu";  } };
template <> struct Fmt<long long>          { static const char* in() { return "%lld"; } static const char* out() { return "%lld"; } };
template <> struct Fmt<unsigned long long> { static const char* in() { return "%llu"; } static const char* out() { return "%llu"; } };
template <> struct Fmt<float>              { static const char* in() { return "%f";   } static const char* out() { return "%f";   } };
template <> struct Fmt<double>             { static const char* in() { return "%lf";  } static const char* out() { return "%f";   } };
template <> struct Fmt<char>               { static const char* in() { return " %c";  } static const char* out() { return "%c";   } };

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

    template <class T>
    T read() {
        T x{};
        if (std::fscanf(fp_, Fmt<T>::in(), &x) != 1) x = T{};
        return x;
    }
    template <class T>
    Reader& read(T& x) { x = read<T>(); return *this; }
    Reader& read(std::string& s) {
        char buf[1 << 16];
        if (std::fscanf(fp_, "%65535s", buf) == 1) s = buf; else s.clear();
        return *this;
    }
    template <class A, class B, class... R>
    Reader& read(A& a, B& b, R&... r) { read(a); return read(b, r...); }
    template <class T>
    Reader& read_n(T* a, size_t n) {
        for (size_t i = 0; i < n; ++i) a[i] = read<T>();
        return *this;
    }
    template <class T>
    Reader& operator>>(T& x) { return read(x); }
    bool eof() {
        int c;
        while ((c = std::fgetc(fp_)) != EOF && c <= ' ') {}
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

    template <class T>
    typename std::enable_if<std::is_arithmetic<T>::value && !std::is_same<T, char>::value,
                            void>::type
    write(T x) { std::fprintf(fp_, Fmt<T>::out(), x); }
    void write(char c) { std::fputc(c, fp_); }
    void write(const char* s) { std::fputs(s, fp_); }
    void write(const std::string& s) { std::fwrite(s.data(), 1, s.size(), fp_); }
    void put(char c) { std::fputc(c, fp_); }
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

}  // namespace fio_scanf
