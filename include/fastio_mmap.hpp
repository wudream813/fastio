#pragma once
// ============================================================================
//  fastio_mmap.hpp  —  写法 9：mmap 整文件映射 + 双字节打表（文1 §4.2.2）
//
//  实测：读 66.5 ms（8.5× cin）
//  （100 MiB / 998 万个 9 位整数、正负随机；基线 cin 565 ms / cout 473 ms）
//
//  思路：
//    1. mmap 把整份输入一次映射进地址空间，零系统调用往返、零缓冲拷贝；
//    2. int32_t tbl[65536]：相邻两字节拼成的 uint16 直接索引出这两位数字的值，
//       非法数字对为 -1，一次吃 2 个字符；
//    3. 顺序展开 5 次（32 位）/ 10 次（64 位），失配后的步骤必然也失配，不用 break；
//    4. 映射尾部挂一整页哨兵（0xFF），热循环彻底不判边界。
//
//  仅适用于「stdin 是重定向的普通文件」或显式打开文件；
//  管道 / 终端 / 交互题请用 fastio_fread.hpp 或主库 fastio.hpp。
//
//  用法：
//      #include "fastio_mmap.hpp"
//      int n = fio_mmap::in.read<int>();
//      fio_mmap::in >> a >> b;
//
//  减分支选项（#define 后再 include）：
//      FASTIO_ASSUME_UNSIGNED  保证没有负号：跳过符号处理
//      FASTIO_PAIR_STEPS_INT   int 级的双字节步数，默认 5（=10 位数字）
//      FASTIO_PAIR_STEPS_LL    long long 级，默认 10
//  本写法靠尾部哨兵页，本来就零 EOF 判断，不需要 FASTIO_NO_EOF_CHECK。
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef FASTIO_PAIR_STEPS_INT
  #define FASTIO_PAIR_STEPS_INT 5   // int 最长 10 位 = 5 个双字符
#endif
#ifndef FASTIO_PAIR_STEPS_LL
  #define FASTIO_PAIR_STEPS_LL 10   // long long 最长 20 位 = 10 个
#endif

namespace fio_mmap {

// ---- 双字节查找表 ----------------------------------------------------------
struct PairTable {
    int32_t v[65536];
    PairTable() {
        for (int i = 0; i < 65536; ++i) v[i] = -1;
        for (int a = '0'; a <= '9'; ++a)
            for (int b = '0'; b <= '9'; ++b)
                v[a | (b << 8)] = (a ^ 48) * 10 + (b ^ 48);
    }
};
inline const PairTable pair_tbl{};

inline uint16_t load16(const char* p) {
    uint16_t w;
    std::memcpy(&w, p, 2);
    return w;
}

class Reader {
public:
    Reader() = default;
    explicit Reader(FILE* fp) { attach(fp); }
    ~Reader() { release(); }
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    // 映射一个已打开的普通文件（默认 stdin，必须是重定向来的文件）
    bool attach(FILE* fp) {
        release();
        int fd = fileno(fp);
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
        std::memset(base_ + ((n + page - 1) / page) * page, SENT, page);
        return true;
    }
    bool open(const char* path) {
        FILE* fp = std::fopen(path, "rb");
        if (!fp) return false;
        bool ok = attach(fp);
        std::fclose(fp);  // mmap 之后 fd 可以关掉
        return ok;
    }

    // ---- 核心：双字节打表解析（只吃局部指针） --------------------------
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
        }
#endif
        const int32_t* tb = pair_tbl.v;
        U v = 0;
        int32_t w;
#define FIO_MMAP_STEP                        \
    if ((w = tb[load16(q)]) >= 0) {          \
        v = U(v * 100 + U(w));               \
        q += 2;                              \
    }
#define FIO_MMAP_STEPS_0
#define FIO_MMAP_STEPS_1  FIO_MMAP_STEP
#define FIO_MMAP_STEPS_2  FIO_MMAP_STEPS_1 FIO_MMAP_STEP
#define FIO_MMAP_STEPS_3  FIO_MMAP_STEPS_2 FIO_MMAP_STEP
#define FIO_MMAP_STEPS_4  FIO_MMAP_STEPS_3 FIO_MMAP_STEP
#define FIO_MMAP_STEPS_5  FIO_MMAP_STEPS_4 FIO_MMAP_STEP
#define FIO_MMAP_STEPS_6  FIO_MMAP_STEPS_5 FIO_MMAP_STEP
#define FIO_MMAP_STEPS_7  FIO_MMAP_STEPS_6 FIO_MMAP_STEP
#define FIO_MMAP_STEPS_8  FIO_MMAP_STEPS_7 FIO_MMAP_STEP
#define FIO_MMAP_STEPS_9  FIO_MMAP_STEPS_8 FIO_MMAP_STEP
#define FIO_MMAP_STEPS_10 FIO_MMAP_STEPS_9 FIO_MMAP_STEP
#define FIO_MMAP_PASTE_(a, b) a##b
#define FIO_MMAP_PASTE(a, b) FIO_MMAP_PASTE_(a, b)
        if (sizeof(U) > 4) {
            FIO_MMAP_PASTE(FIO_MMAP_STEPS_, FASTIO_PAIR_STEPS_LL)   // 64 位默认 10 步
        } else {
            FIO_MMAP_PASTE(FIO_MMAP_STEPS_, FASTIO_PAIR_STEPS_INT)  // 32 位默认 5 步
        }
#undef FIO_MMAP_PASTE
#undef FIO_MMAP_PASTE_
#undef FIO_MMAP_STEPS_0
#undef FIO_MMAP_STEPS_1
#undef FIO_MMAP_STEPS_2
#undef FIO_MMAP_STEPS_3
#undef FIO_MMAP_STEPS_4
#undef FIO_MMAP_STEPS_5
#undef FIO_MMAP_STEPS_6
#undef FIO_MMAP_STEPS_7
#undef FIO_MMAP_STEPS_8
#undef FIO_MMAP_STEPS_9
#undef FIO_MMAP_STEPS_10
#undef FIO_MMAP_STEP
        if ((unsigned)(*q - '0') < 10u) v = U(v * 10 + U(*q++ ^ 48));
        const U mask = U(0) - U(neg);   // 无分支取负，INT_MIN 安全
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
    Reader& operator>>(T& x) {
        x = read<T>();
        return *this;
    }
    template <class T>
    Reader& read_n(T* a, size_t n) {  // 批量：游标常驻寄存器，最快
        const char* q = p_;
        for (size_t i = 0; i < n; ++i) a[i] = parse<T>(q);
        p_ = q;
        return *this;
    }

    bool eof() {
        while ((unsigned char)*p_ <= ' ') ++p_;
        return p_ >= end_;
    }
    const char* pos() const { return p_; }
    const char* end() const { return end_; }

    void release() {
        if (base_) { munmap(base_, len_); base_ = nullptr; len_ = 0; }
        p_ = end_ = nullptr;
    }

private:
    static constexpr char SENT = char(0xFF);
    const char* p_ = nullptr;
    const char* end_ = nullptr;
    char* base_ = nullptr;
    size_t len_ = 0;
};

// 默认对象：映射 stdin（必须是 ./a.out < input.txt 这种重定向）
struct StdinReader : Reader {
    StdinReader() { ok = attach(stdin); }
    bool ok = false;
};
inline StdinReader in;

}  // namespace fio_mmap
