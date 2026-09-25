#pragma once
// ============================================================================
//  fastio_mmap_byte.hpp  —  写法 7：mmap 整文件映射 + 单字节解析（文1 §4.2.1）
//
//  实测：读 107 ms（5.3× cin）
//  （100 MiB / 998 万个 9 位整数、正负随机；基线 cin 565 ms / cout 473 ms）
//
//  mmap 的第一步版本：整份输入映射进地址空间后，仍然一个字符一个字符地
//      while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ ^ 48);
//  已经省掉了系统调用与缓冲拷贝，但每个字符仍有一次比较 + 一次乘法。
//  再往上就是双字节打表（见 fastio_mmap.hpp），能再快 1.6 倍。
//
//  只适合 stdin 被重定向成普通文件的场景。
//
//  用法：
//      #include "fastio_mmap_byte.hpp"
//      int n = fio_mmap_byte::in.read<int>();
//
//  减分支选项（#define 后再 include）：
//      FASTIO_ASSUME_UNSIGNED  保证没有负号：跳过符号处理
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fio_mmap_byte {

class Reader {
public:
    Reader() = default;
    explicit Reader(FILE* fp) { attach(fp); }
    ~Reader() { release(); }
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    bool attach(FILE* fp) {
        release();
        int fd = fp ? fileno(fp) : -1;
        struct stat st {};
        if (fd < 0 || fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) return false;
        size_t n = size_t(st.st_size);
        size_t page = size_t(sysconf(_SC_PAGESIZE));
        size_t total = ((n + page - 1) / page) * page + page;
        void* base = mmap(nullptr, total, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) return false;
        if (n && mmap(base, n, PROT_READ, MAP_PRIVATE | MAP_FIXED
#ifdef MAP_POPULATE
                                              | MAP_POPULATE
#endif
                      ,
                      fd, 0) == MAP_FAILED) {
            munmap(base, total);
            return false;
        }
        base_ = static_cast<char*>(base);
        len_ = total;
        p_ = base_;
        end_ = base_ + n;
        std::memset(base_ + ((n + page - 1) / page) * page, SENT, page);  // 哨兵
        return true;
    }
    bool open(const char* path) {
        FILE* fp = std::fopen(path, "rb");
        if (!fp) return false;
        bool ok = attach(fp);
        std::fclose(fp);
        return ok;
    }

    // ---- 单字节解析 ----------------------------------------------------
    template <class T>
    static inline T parse(const char*& q) {
        using U = typename std::make_unsigned<T>::type;
        while ((unsigned char)*q <= ' ') ++q;
#ifdef FASTIO_ASSUME_UNSIGNED
        constexpr bool neg = false;   // 用户承诺无负号：符号分支编译期消失
#else
        unsigned c0 = (unsigned char)*q;
        bool neg = false;
        if (std::is_signed<T>::value) {
            neg = (c0 == '-');
            q += unsigned(neg) | unsigned(c0 == '+');
        } else {
            q += unsigned(c0 == '+');
        }
#endif
        U v = 0;
        while ((unsigned)(*q - '0') < 10u) v = U(v * 10 + U(*q++ ^ 48));
        const U mask = U(0) - U(neg);
        return T((v ^ mask) - mask);
    }

    template <class T>
    T read() {
        const char* q = p_;
        T v = parse<T>(q);
        p_ = q;
        return v;
    }
    template <class T>
    typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, char>::value,
                            Reader&>::type
    read(T& x) { x = read<T>(); return *this; }
    Reader& read(std::string& s) {
        while ((unsigned char)*p_ <= ' ') ++p_;
        const char* q = p_;
        while (q < end_ && (unsigned char)*q > ' ') ++q;
        s.assign(p_, size_t(q - p_));
        p_ = q;
        return *this;
    }
    template <class A, class B, class... R>
    Reader& read(A& a, B& b, R&... r) { read(a); return read(b, r...); }
    template <class T>
    Reader& read_n(T* a, size_t n) {
        const char* q = p_;
        for (size_t i = 0; i < n; ++i) a[i] = parse<T>(q);
        p_ = q;
        return *this;
    }
    template <class T>
    Reader& operator>>(T& x) { return read(x); }
    bool eof() {
        while ((unsigned char)*p_ <= ' ') ++p_;
        return p_ >= end_;
    }

    void release() {
        if (base_) { munmap(base_, len_); base_ = nullptr; len_ = 0; }
        p_ = end_ = sentinel();
    }

private:
    static constexpr char SENT = char(0xFF);
    static const char* sentinel() {
        static const char z[64] = {SENT, SENT, SENT, SENT, SENT, SENT, SENT, SENT,
                                   SENT, SENT, SENT, SENT, SENT, SENT, SENT, SENT,
                                   SENT, SENT, SENT, SENT, SENT, SENT, SENT, SENT,
                                   SENT, SENT, SENT, SENT, SENT, SENT, SENT, SENT,
                                   SENT, SENT, SENT, SENT, SENT, SENT, SENT, SENT,
                                   SENT, SENT, SENT, SENT, SENT, SENT, SENT, SENT,
                                   SENT, SENT, SENT, SENT, SENT, SENT, SENT, SENT,
                                   SENT, SENT, SENT, SENT, SENT, SENT, SENT, SENT};
        return z;
    }
    const char* p_ = sentinel();
    const char* end_ = sentinel();
    char* base_ = nullptr;
    size_t len_ = 0;
};

struct StdinReader : Reader {
    StdinReader() { ok = attach(stdin); }
    bool ok = false;
};
inline StdinReader in;

}  // namespace fio_mmap_byte
