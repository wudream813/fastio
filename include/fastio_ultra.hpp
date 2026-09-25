#pragma once
// ============================================================================
//  fastio_ultra.hpp  —  写法 10：UltraReader（mmap 打表读的"工程化"封装）
//
//  实测：读 66.5 ms（8.5× cin，与写法 A 并列最快）
//  （100 MiB / 998 万个 9 位整数、正负随机；基线 cin 565 ms / cout 473 ms）
//
//  与写法 9（fastio_mmap.hpp）的区别：写法 9 是文章里那段极简手写循环，只吃普通文件；
//  B 在同样的 mmap + 双字节打表核心上补齐了工程细节：
//    · mmap 失败 / 输入是管道 → 自动 fread 整读进堆缓冲，接口不变；
//    · 尾部哨兵页，热循环零边界判断；
//    · 无分支符号处理（掩码取负），INT_MIN / LLONG_MIN 安全；
//    · 支持 int/long long/unsigned、char、std::string、整行、eof()、批量 read_n；
//    · 析构自动 munmap / free。
//
//  不适合交互题（会把整份输入读进内存）；交互题用 fastio_fread.hpp 或主库。
//
//  用法：
//      #include "fastio_ultra.hpp"
//      int n = fio_ultra::in.read<int>();
//      fio_ultra::in.read_n(a, n);
//
//  减分支选项（#define 后再 include）：
//      FASTIO_ASSUME_UNSIGNED  保证没有负号：跳过符号处理
//      FASTIO_PAIR_STEPS_INT   int 级的双字节步数，默认 5（=10 位数字）
//      FASTIO_PAIR_STEPS_LL    long long 级，默认 10
//  本写法靠尾部哨兵，本来就零 EOF 判断，不需要 FASTIO_NO_EOF_CHECK。
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
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

namespace fio_ultra {

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

class UltraReader {
public:
    enum Mode { NONE, MAPPED, SLURPED };

    UltraReader() = default;
    explicit UltraReader(FILE* fp) { load(fp); }
    ~UltraReader() { release(); }
    UltraReader(const UltraReader&) = delete;
    UltraReader& operator=(const UltraReader&) = delete;

    // 普通文件 -> mmap；其它（管道等）-> 一次性 fread 整读
    bool load(FILE* fp = stdin) {
        release();
        if (!fp) return false;
        int fd = fileno(fp);
        struct stat st {};
        if (fd >= 0 && fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0) {
            if (map_fd(fd, size_t(st.st_size))) return true;
        }
        return slurp(fp);
    }
    bool load_file(const char* path) {
        FILE* fp = std::fopen(path, "rb");
        if (!fp) return false;
        bool ok = load(fp);
        std::fclose(fp);
        return ok;
    }

    // ---- 核心解析（只吃局部指针，供逐个读与批量读共用） ----------------
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
        const int32_t* tb = pair_tbl.v;
        U v = 0;
        int32_t w;
#define FIO_ULTRA_STEP              \
    if ((w = tb[load16(q)]) >= 0) { \
        v = U(v * 100 + U(w));      \
        q += 2;                     \
    }
#define FIO_ULTRA_STEPS_0
#define FIO_ULTRA_STEPS_1  FIO_ULTRA_STEP
#define FIO_ULTRA_STEPS_2  FIO_ULTRA_STEPS_1 FIO_ULTRA_STEP
#define FIO_ULTRA_STEPS_3  FIO_ULTRA_STEPS_2 FIO_ULTRA_STEP
#define FIO_ULTRA_STEPS_4  FIO_ULTRA_STEPS_3 FIO_ULTRA_STEP
#define FIO_ULTRA_STEPS_5  FIO_ULTRA_STEPS_4 FIO_ULTRA_STEP
#define FIO_ULTRA_STEPS_6  FIO_ULTRA_STEPS_5 FIO_ULTRA_STEP
#define FIO_ULTRA_STEPS_7  FIO_ULTRA_STEPS_6 FIO_ULTRA_STEP
#define FIO_ULTRA_STEPS_8  FIO_ULTRA_STEPS_7 FIO_ULTRA_STEP
#define FIO_ULTRA_STEPS_9  FIO_ULTRA_STEPS_8 FIO_ULTRA_STEP
#define FIO_ULTRA_STEPS_10 FIO_ULTRA_STEPS_9 FIO_ULTRA_STEP
#define FIO_ULTRA_PASTE_(a, b) a##b
#define FIO_ULTRA_PASTE(a, b) FIO_ULTRA_PASTE_(a, b)
        if (sizeof(U) > 4) {
            FIO_ULTRA_PASTE(FIO_ULTRA_STEPS_, FASTIO_PAIR_STEPS_LL)   // 64 位默认 10 步
        } else {
            FIO_ULTRA_PASTE(FIO_ULTRA_STEPS_, FASTIO_PAIR_STEPS_INT)  // 32 位默认 5 步
        }
#undef FIO_ULTRA_PASTE
#undef FIO_ULTRA_PASTE_
#undef FIO_ULTRA_STEPS_0
#undef FIO_ULTRA_STEPS_1
#undef FIO_ULTRA_STEPS_2
#undef FIO_ULTRA_STEPS_3
#undef FIO_ULTRA_STEPS_4
#undef FIO_ULTRA_STEPS_5
#undef FIO_ULTRA_STEPS_6
#undef FIO_ULTRA_STEPS_7
#undef FIO_ULTRA_STEPS_8
#undef FIO_ULTRA_STEPS_9
#undef FIO_ULTRA_STEPS_10
#undef FIO_ULTRA_STEP
        if ((unsigned)(*q - '0') < 10u) v = U(v * 10 + U(*q++ ^ 48));
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
    UltraReader& read_n(T* a, size_t n) {  // 批量：游标常驻寄存器
        const char* q = p_;
        for (size_t i = 0; i < n; ++i) a[i] = parse<T>(q);
        p_ = q;
        return *this;
    }
    template <class T>
    typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, char>::value,
                            UltraReader&>::type
    read(T& x) { x = read<T>(); return *this; }
    UltraReader& read(char& c) {
        while ((unsigned char)*p_ <= ' ') ++p_;
        c = p_ < end_ ? *p_++ : '\0';
        return *this;
    }
    UltraReader& read(std::string& s) {
        while ((unsigned char)*p_ <= ' ') ++p_;
        const char* q = p_;
        while (q < end_ && (unsigned char)*q > ' ') ++q;
        s.assign(p_, size_t(q - p_));
        p_ = q;
        return *this;
    }
    UltraReader& read(double& d) {
        while ((unsigned char)*p_ <= ' ') ++p_;
        char* fin = nullptr;
        d = std::strtod(p_, &fin);
        if (fin) p_ = fin;
        return *this;
    }
    template <class A, class B, class... R>
    UltraReader& read(A& a, B& b, R&... r) { read(a); return read(b, r...); }
    template <class T>
    UltraReader& operator>>(T& x) { return read(x); }

    bool readln(std::string& s) {  // 整行（不含换行）
        if (p_ >= end_) return false;
        const char* q = p_;
        while (q < end_ && *q != '\n') ++q;
        s.assign(p_, size_t(q - p_));
        if (!s.empty() && s.back() == '\r') s.pop_back();
        p_ = (q < end_) ? q + 1 : q;
        return true;
    }
    bool eof() {
        while ((unsigned char)*p_ <= ' ') ++p_;
        return p_ >= end_;
    }
    Mode mode() const { return mode_; }
    const char* pos() const { return p_; }
    const char* end() const { return end_; }
    size_t size() const { return size_t(end_ - base_data()); }

    void release() {
        if (map_base_) { munmap(map_base_, map_len_); map_base_ = nullptr; map_len_ = 0; }
        if (heap_) { std::free(heap_); heap_ = nullptr; }
        p_ = end_ = sentinel();
        mode_ = NONE;
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
    const char* base_data() const { return map_base_ ? map_base_ : (heap_ ? heap_ : p_); }

    bool map_fd(int fd, size_t n) {
        size_t page = size_t(sysconf(_SC_PAGESIZE));
        size_t total = ((n + page - 1) / page) * page + page;
        void* base = mmap(nullptr, total, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) return false;
        if (mmap(base, n, PROT_READ, MAP_PRIVATE | MAP_FIXED
#ifdef MAP_POPULATE
                                         | MAP_POPULATE
#endif
                 ,
                 fd, 0) == MAP_FAILED) {
            munmap(base, total);
            return false;
        }
        map_base_ = static_cast<char*>(base);
        map_len_ = total;
        p_ = map_base_;
        end_ = map_base_ + n;
        std::memset(map_base_ + ((n + page - 1) / page) * page, SENT, page);
        mode_ = MAPPED;
        return true;
    }

    bool slurp(FILE* fp) {  // 管道 / 未知大小：整读进堆缓冲，尾部补哨兵
        constexpr size_t CHUNK = 1 << 20;
        size_t cap = CHUNK, len = 0;
        char* buf = static_cast<char*>(std::malloc(cap + 64));
        if (!buf) return false;
        for (;;) {
            if (len + CHUNK + 64 > cap) {
                cap <<= 1;
                char* nb = static_cast<char*>(std::realloc(buf, cap + 64));
                if (!nb) { std::free(buf); return false; }
                buf = nb;
            }
            size_t got = std::fread(buf + len, 1, CHUNK, fp);
            len += got;
            if (got < CHUNK) break;
        }
        std::memset(buf + len, SENT, 64);
        heap_ = buf;
        p_ = buf;
        end_ = buf + len;
        mode_ = SLURPED;
        return true;
    }

    const char* p_ = sentinel();
    const char* end_ = sentinel();
    char* map_base_ = nullptr;
    char* heap_ = nullptr;
    size_t map_len_ = 0;
    Mode mode_ = NONE;
};

// 默认对象：绑定 stdin（文件走 mmap，管道走整读）
struct StdinUltra : UltraReader {
    StdinUltra() { load(stdin); }
};
inline StdinUltra in;

}  // namespace fio_ultra
