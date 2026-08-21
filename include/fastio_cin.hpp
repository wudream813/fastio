#pragma once
// ============================================================================
//  fastio_cin.hpp  —  写法 1：cin / cout 关同步 + untie（文1 §1 / 文2 §五）
//
//  实测：读 578 ms（0.98×）、写 451 ms（1.05×）—— 几乎等于默认 cin/cout
//  （100 MiB / 998 万个 9 位整数、正负随机；基线 cin 565 ms / cout 473 ms）
//
//  一行党的起点：
//      std::ios::sync_with_stdio(false);
//      std::cin.tie(nullptr);
//  代价：从此不能再混用 scanf/printf；忘了 tie 会让交互题看不到提示。
//  这里封装成与其它写法一致的接口，方便横向对照。
//
//  用法：
//      #include "fastio_cin.hpp"
//      int n = fio_cin::in.read<int>();
//      fio_cin::out << n << '\n';
// ============================================================================

#include <cstddef>
#include <iostream>
#include <string>
#include <type_traits>

namespace fio_cin {

inline void unsync() {
    static bool done = false;
    if (!done) {
        std::ios::sync_with_stdio(false);
        std::cin.tie(nullptr);
        std::cout.tie(nullptr);
        done = true;
    }
}

class Reader {
public:
    Reader() : is_(&std::cin) { unsync(); }
    explicit Reader(std::istream& is) : is_(&is) { unsync(); }
    void attach(std::istream& is) { is_ = &is; }

    template <class T>
    T read() {
        T x{};
        *is_ >> x;
        return x;
    }
    template <class T>
    Reader& read(T& x) { *is_ >> x; return *this; }
    template <class A, class B, class... R>
    Reader& read(A& a, B& b, R&... r) { read(a); return read(b, r...); }
    template <class T>
    Reader& read_n(T* a, size_t n) {
        for (size_t i = 0; i < n; ++i) *is_ >> a[i];
        return *this;
    }
    template <class T>
    Reader& operator>>(T& x) { return read(x); }
    bool readln(std::string& s) { return bool(std::getline(*is_, s)); }
    bool eof() { return !(*is_ >> std::ws) || is_->eof(); }

private:
    std::istream* is_;
};

class Writer {
public:
    Writer() : os_(&std::cout) { unsync(); }
    explicit Writer(std::ostream& os) : os_(&os) { unsync(); }
    ~Writer() { os_->flush(); }
    void attach(std::ostream& os) { os_ = &os; }

    template <class T>
    void write(const T& x) { *os_ << x; }
    void put(char c) { os_->put(c); }
    template <class T>
    Writer& operator<<(const T& x) { *os_ << x; return *this; }
    template <class T>
    void write_n(const T* a, size_t n, char sep = ' ', char last = '\n') {
        for (size_t i = 0; i < n; ++i) *os_ << a[i] << (i + 1 == n ? last : sep);
    }
    void flush() { os_->flush(); }

private:
    std::ostream* os_;
};

inline Reader in;
inline Writer out;

}  // namespace fio_cin
