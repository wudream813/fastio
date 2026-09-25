#pragma once
// ============================================================================
//  fastio_fwrite.hpp  —  写法 8：fwrite 大缓冲快写（不打表，文1 二§3 / 文2 §四）
//
//  实测：写 251 ms（1.9× cout）
//  （100 MiB / 998 万个 9 位整数、正负随机；基线 cin 565 ms / cout 473 ms）
//
//  思路：所有输出先堆进 4 MiB 内存缓冲，满了 / 结束时一次 fwrite 落盘，
//        把「每个字符一次调用」降成「每 4 MiB 一次系统调用」。
//        整数转字符串仍是逐位取模（对照组：加上四位打表 → fastio_fread.hpp，
//        9 位整数上还能再快 1.5 倍）。
//
//  用法：
//      #include "fastio_fwrite.hpp"
//      fio_fwrite::out << x << '\n';      // 析构自动 flush
//
//  减分支选项（#define 后再 include）：
//      FASTIO_ASSUME_UNSIGNED  保证没有负数：写侧的符号分支整体消失
// ============================================================================

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>

namespace fio_fwrite {

class Writer {
public:
    static constexpr size_t SZ = 1 << 22;  // 4 MiB

    Writer() : fp_(stdout) { buf_ = static_cast<char*>(std::malloc(SZ + 64)); cur_ = buf_; }
    explicit Writer(FILE* fp) : Writer() { fp_ = fp ? fp : stdout; }
    ~Writer() { spill(); std::free(buf_); if (own_) std::fclose(own_); }
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    void attach(FILE* fp) { spill(); fp_ = fp ? fp : stdout; }
    bool open(const char* path) {
        FILE* fp = std::fopen(path, "wb");
        if (!fp) return false;
        attach(fp);
        own_ = fp;
        return true;
    }

    inline void spill() {   // 只把缓冲倒进 stdio
        if (cur_ != buf_) { std::fwrite(buf_, 1, size_t(cur_ - buf_), fp_); cur_ = buf_; }
    }
    void flush() { spill(); std::fflush(fp_); }   // 对外：真正落到流里

    inline void put(char c) {
        if (cur_ == buf_ + SZ) spill();
        *cur_++ = c;
    }
    void put_raw(const char* s, size_t n) {
        if (n >= SZ) { spill(); std::fwrite(s, 1, n, fp_); return; }
        if (size_t(buf_ + SZ - cur_) < n) spill();
        std::memcpy(cur_, s, n);
        cur_ += n;
    }

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
        if (size_t(buf_ + SZ - cur_) < 24) spill();
        char tmp[24];
        int k = 0;
        do { tmp[k++] = char('0' + int(v % 10)); v /= 10; } while (v);   // 逐位取模
        while (k) *cur_++ = tmp[--k];
    }
    void write(char c) { put(c); }
    void write(const char* s) { put_raw(s, std::strlen(s)); }
    void write(const std::string& s) { put_raw(s.data(), s.size()); }
    void write(double x) {
        if (size_t(buf_ + SZ - cur_) < 400) spill();
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

inline Writer out;

}  // namespace fio_fwrite
