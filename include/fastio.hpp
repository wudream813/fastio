#pragma once
// ============================================================================
//  fastio.hpp  —  超级快读快写（单头文件，开箱即用）
//
//  读：mmap 整文件映射 + 双字节打表        （100MiB 实测 ~70 ms，约 8× cin）
//  写：fwrite 大缓冲   + 四位打表          （100MiB 实测 ~155 ms，约 3× cout）
//  非常规文件（管道 / 终端 / 交互题）自动降级为 fread 流式，接口完全一样。
//
//  最简用法：
//      #include "fastio.hpp"
//      int main() {
//          int n; io >> n;
//          long long s = 0;
//          for (int i = 0; i < n; i++) { int x; io >> x; s += x; }
//          io << s << '\n';        // 程序结束自动 flush
//      }
//
//  常用接口：
//      io >> a >> b;                   io << a << ' ' << b << '\n';
//      int x = io.read<int>();         io.write(x);  io.writeln(x);
//      io.read(a, b, c);               io.print(a, ' ', b, '\n');
//      io.read_n(arr, n);              io.println(a, b, c);   // 空格分隔+换行
//      std::string s; io >> s;         io.write_n(arr, n, ' ');
//      io.readln(line);                io.flush();   // 交互题每轮必须
//
//  编译：g++ -O2 -std=c++17 main.cpp
//  可选宏（模式类）：
//      FASTIO_NO_MMAP      禁用 mmap（强制 fread）
//      FASTIO_STREAM       强制流式读（交互题；不把整份输入读进内存）
//      FASTIO_OBUF_BITS    输出缓冲大小位数，默认 22（4 MiB）
//      FASTIO_IBUF_BITS    流式输入缓冲位数，默认 20（1 MiB）
//
//  减分支选项（#define 之后再 #include；「你保证」成立才开，否则结果错误）：
//      FASTIO_NO_EOF_CHECK     忽略 EOF：getch/peek/read<char> 不再判边界，
//                              fread/streambuf/getchar 档的 `c != EOF` 比较消失。
//                              流式缓冲的 refill 永远保留（那是窗口正确性不是
//                              EOF 检查），所以管道喂数照样安全——只要你保证
//                              数据规范、读不到流末尾。
//      FASTIO_ASSUME_UNSIGNED  保证输入没有负号：读、写两侧的符号分支整体消失
//      FASTIO_PAIR_STEPS_INT   覆盖 ≤32 位整型的双字节步数（默认 int/uint 精确 5）
//      FASTIO_PAIR_STEPS_LL    覆盖 64 位（默认 signed 9 = 19 位 / unsigned 10 = 20 位）
//      FASTIO_PAIR_STEPS_I128  覆盖 __int128（默认 19 = 39 位）
//      FASTIO_REPLACE_CIN_COUT 定义全局 cin/cout/endl，直接替换 iostream 写法：
//                                  int a; cin >> a; cout << a << endl;
//
//  激进选项（承诺更硬，换取最后一次系统调用都省掉）：
//      FASTIO_INPUT_MAX=n      保证 stdin 总量 ≤ n 字节：attach 时一次性读完
//                              （连 mmap 都不用），之后零系统调用；超出部分丢弃！
//                              ⚠ 会一口气读到 EOF —— 交互题绝对禁用
//      FASTIO_OUTPUT_MAX=n     保证总输出 ≤ n 字节：缓冲一次配足，整个程序只
//                              fwrite 一次；边界检查全删。⚠ 超出 = 堆损坏！
// ============================================================================

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
  #include <fcntl.h>
  #include <sys/mman.h>
  #include <sys/stat.h>
  #include <unistd.h>
  #define FASTIO_UNIX 1
#else
  #define FASTIO_UNIX 0
#endif

#if FASTIO_UNIX && !defined(FASTIO_NO_MMAP)
  #define FASTIO_HAS_MMAP 1
#else
  #define FASTIO_HAS_MMAP 0
#endif

#ifndef FASTIO_OBUF_BITS
  #define FASTIO_OBUF_BITS 22
#endif
#ifndef FASTIO_IBUF_BITS
  #define FASTIO_IBUF_BITS 20
#endif
// 双字节打表步数：默认按类型最长十进制位数自动精确展开
//   int/unsigned 10 位 → 5 对、ll 19 位 → 9 对、ull 20 位 → 10 对、__int128 39 位 → 19 对
// 想强行调小（知道数据位数少）可用 FASTIO_PAIR_STEPS_INT / _LL / _I128 覆盖：
#ifndef FASTIO_PAIR_STEPS_INT
  #define FASTIO_PAIR_STEPS_INT 5
#endif
#if defined(FASTIO_PAIR_STEPS_INT) && (FASTIO_PAIR_STEPS_INT < 0 || FASTIO_PAIR_STEPS_INT > 10)
  #error "FASTIO_PAIR_STEPS_INT 取值范围 0..10"
#endif
#if defined(FASTIO_PAIR_STEPS_LL) && (FASTIO_PAIR_STEPS_LL < 0 || FASTIO_PAIR_STEPS_LL > 10)
  #error "FASTIO_PAIR_STEPS_LL 取值范围 0..10"
#endif
#if defined(FASTIO_PAIR_STEPS_I128) && (FASTIO_PAIR_STEPS_I128 < 0 || FASTIO_PAIR_STEPS_I128 > 19)
  #error "FASTIO_PAIR_STEPS_I128 取值范围 0..19"
#endif
#if defined(FASTIO_INPUT_MAX) && FASTIO_INPUT_MAX <= 0
  #error "FASTIO_INPUT_MAX 必须是正数"
#endif
#if defined(FASTIO_OUTPUT_MAX) && FASTIO_OUTPUT_MAX <= 0
  #error "FASTIO_OUTPUT_MAX 必须是正数"
#endif

#if defined(__GNUC__)
  #define FASTIO_HOT       __attribute__((hot))
  #define FASTIO_ALWAYS    inline __attribute__((always_inline))
  #define FASTIO_LIKELY(x) __builtin_expect(!!(x), 1)
  #define FASTIO_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
  #define FASTIO_HOT
  #define FASTIO_ALWAYS    inline
  #define FASTIO_LIKELY(x) (x)
  #define FASTIO_UNLIKELY(x) (x)
#endif

namespace fastio {
namespace detail {

// ---------------------------------------------------------------- 打表 ----
// 双字节表：小端下 "37" 这两个字节拼成的 uint16 索引到 37；非数字对为 -1
struct PairTable {
    int32_t v[65536];
    PairTable() {
        for (int i = 0; i < 65536; ++i) v[i] = -1;
        for (int a = '0'; a <= '9'; ++a)
            for (int b = '0'; b <= '9'; ++b)
                v[a | (b << 8)] = (a ^ 48) * 10 + (b ^ 48);
    }
};

// 四位表：0000..9999 直接存成 4 个 ASCII 字节（小端打包成一个 uint32）
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

inline const PairTable pair_tbl{};
inline const QuadTable quad_tbl{};

FASTIO_ALWAYS uint16_t load16(const char* p) {
    uint16_t w;
    std::memcpy(&w, p, 2);
    return w;
}
FASTIO_ALWAYS void store32(char* p, uint32_t w) { std::memcpy(p, &w, 4); }

template <class T>
struct is_int : std::integral_constant<bool, std::is_integral<T>::value &&
                                                 !std::is_same<typename std::remove_cv<T>::type, char>::value &&
                                                 !std::is_same<typename std::remove_cv<T>::type, bool>::value> {};

template <class T>
struct is_flt : std::is_floating_point<T> {};

// ---- __int128 兼容：严格 -std=c++17 下标准萃取不认识这个 GNU 扩展类型 ------
template <class T> struct uns_of { using type = typename std::make_unsigned<T>::type; };
template <class T> struct is_signed_of : std::is_signed<T> {};
#if defined(__SIZEOF_INT128__)
template <> struct uns_of<__int128_t> { using type = __uint128_t; };
template <> struct uns_of<__uint128_t> { using type = __uint128_t; };
template <> struct is_signed_of<__int128_t> : std::true_type {};
template <> struct is_int<__int128_t> : std::true_type {};
template <> struct is_int<__uint128_t> : std::true_type {};
#endif
template <class T> using uns_t = typename uns_of<T>::type;

}  // namespace detail

// ==========================================================================
//                                 Reader
//   模式 1（默认，最快）：mmap 整文件 + 双字节打表；末尾补一整页匿名零页，
//                        所以热循环里不需要边界判断。
//   模式 2（管道/未知大小）：一次性 fread 进内存（同样带零填充尾）。
//   模式 3（终端 / FASTIO_STREAM / 交互）：fread 分块流式，接口一致。
// ==========================================================================
class Reader {
public:
    enum Mode { MAPPED, SLURPED, STREAM, EMPTY };

    Reader() = default;
    explicit Reader(FILE* fp) { attach(fp); }
    ~Reader() { release(); }
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    // ---- 绑定输入源 --------------------------------------------------
    void attach(FILE* fp) {
        release();
        fin_ = fp ? fp : stdin;
#ifdef FASTIO_STREAM
        init_stream();
        return;
#elif defined(FASTIO_INPUT_MAX)
        // 用户承诺总输入 ≤ FASTIO_INPUT_MAX 字节：一口气整读到 EOF（连 mmap 都不用），
        // 之后热路径永远零系统调用。⚠ 会阻塞等到 EOF —— 交互题绝对禁用本选项！
        init_gulp(size_t(FASTIO_INPUT_MAX));
        return;
#else
        int fd = fileno(fin_);
        if (fd >= 0) {
#if FASTIO_UNIX
            struct stat st {};
            if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode)) {
                size_t n = size_t(st.st_size) - size_t(ftell_safe());
                if (n == 0) { mode_ = EMPTY; stream_ = false; p_ = end_ = zero_pad(); return; }
#if FASTIO_HAS_MMAP
                if (try_mmap(fd, size_t(st.st_size))) return;
#endif
                if (slurp(n)) return;
            }
#endif
        }
        init_stream();
#endif
    }

    bool open(const char* path) {
        FILE* fp = std::fopen(path, "rb");
        if (!fp) return false;
        attach(fp);
        owns_file_ = fp;
        return true;
    }
    // 兼容旧接口
    bool load(FILE* fp = stdin) { attach(fp); return true; }
    bool load_file(const char* path) { return open(path); }

    // ---- 基础状态 ----------------------------------------------------
    bool eof() {
        skip_ws();
        return p_ >= end_;
    }
    Mode mode() const { return mode_; }
    const char* pos() const { return p_; }
    const char* end() const { return end_; }
    size_t remain() const { return p_ < end_ ? size_t(end_ - p_) : 0; }

    // ---- 字符级 ------------------------------------------------------
    FASTIO_ALWAYS int getch() {  // 原样取一个字节，无则 EOF
#ifndef FASTIO_NO_EOF_CHECK
        if (FASTIO_UNLIKELY(p_ >= end_) && !fill()) return EOF;
#endif
        return (unsigned char)*p_++;
    }
    FASTIO_ALWAYS int peek() {
#ifndef FASTIO_NO_EOF_CHECK
        if (FASTIO_UNLIKELY(p_ >= end_) && !fill()) return EOF;
#endif
        return (unsigned char)*p_;
    }
    FASTIO_ALWAYS void skip_ws() {
        for (;;) {
            while ((unsigned char)*p_ <= ' ') ++p_;   // 靠哨兵收尾，无需判边界
            if (FASTIO_LIKELY(!stream_) || p_ < end_) return;
            if (!fill()) return;
        }
    }

    // ---- 整数（双字节打表核心） --------------------------------------
    // 核心解析：只吃局部指针，不碰成员 —— 供 read<T>() 和 read_n() 复用
    template <class T>
    FASTIO_ALWAYS static T parse_int(const char*& q) {
        using U = detail::uns_t<T>;
        while ((unsigned char)*q <= ' ') ++q;
#ifdef FASTIO_ASSUME_UNSIGNED
        constexpr bool neg = false;   // 用户承诺无负号：符号分支编译期消失
#else
        // 符号位无分支处理：负号在随机数据上分支预测失败率极高
        unsigned c0 = (unsigned char)*q;
        bool neg = false;
        if constexpr (detail::is_signed_of<T>::value) {
            neg = (c0 == '-');
            q += unsigned(neg) | unsigned(c0 == '+');
        } else {
            q += unsigned(c0 == '+');
        }
#endif
        const int32_t* tb = detail::pair_tbl.v;
        U v = 0;
        int32_t w;
// 每个 STEP 吃两位数字；顺序展开、不用 break（失败后的 STEP 必然也失败）
#define FASTIO_STEP                                        \
    if (FASTIO_LIKELY((w = tb[detail::load16(q)]) >= 0)) { \
        v = U(v * 100 + U(w));                             \
        q += 2;                                            \
    }
// 预处理器层重复：0..19 步（19 对 = 38 位 + 末尾单字节 = 39 位，够 __int128 用）
#define FASTIO_STEPS_0
#define FASTIO_STEPS_1  FASTIO_STEP
#define FASTIO_STEPS_2  FASTIO_STEPS_1 FASTIO_STEP
#define FASTIO_STEPS_3  FASTIO_STEPS_2 FASTIO_STEP
#define FASTIO_STEPS_4  FASTIO_STEPS_3 FASTIO_STEP
#define FASTIO_STEPS_5  FASTIO_STEPS_4 FASTIO_STEP
#define FASTIO_STEPS_6  FASTIO_STEPS_5 FASTIO_STEP
#define FASTIO_STEPS_7  FASTIO_STEPS_6 FASTIO_STEP
#define FASTIO_STEPS_8  FASTIO_STEPS_7 FASTIO_STEP
#define FASTIO_STEPS_9  FASTIO_STEPS_8 FASTIO_STEP
#define FASTIO_STEPS_10 FASTIO_STEPS_9 FASTIO_STEP
#define FASTIO_STEPS_11 FASTIO_STEPS_10 FASTIO_STEP
#define FASTIO_STEPS_12 FASTIO_STEPS_11 FASTIO_STEP
#define FASTIO_STEPS_13 FASTIO_STEPS_12 FASTIO_STEP
#define FASTIO_STEPS_14 FASTIO_STEPS_13 FASTIO_STEP
#define FASTIO_STEPS_15 FASTIO_STEPS_14 FASTIO_STEP
#define FASTIO_STEPS_16 FASTIO_STEPS_15 FASTIO_STEP
#define FASTIO_STEPS_17 FASTIO_STEPS_16 FASTIO_STEP
#define FASTIO_STEPS_18 FASTIO_STEPS_17 FASTIO_STEP
#define FASTIO_STEPS_19 FASTIO_STEPS_18 FASTIO_STEP
#define FASTIO_PASTE_(a, b) a##b
#define FASTIO_PASTE(a, b) FASTIO_PASTE_(a, b)
        // 默认按类型最长十进制位数精确展开，可用 FASTIO_PAIR_STEPS_* 覆盖调小
        if constexpr (sizeof(U) > 8) {                    // （unsigned）__int128
#ifdef FASTIO_PAIR_STEPS_I128
            FASTIO_PASTE(FASTIO_STEPS_, FASTIO_PAIR_STEPS_I128)
#else
            FASTIO_STEPS_19                               // 最长 39 位 = 19 对 + 1 单
#endif
        } else if constexpr (sizeof(U) > 4) {             // 64 位
#ifdef FASTIO_PAIR_STEPS_LL
            FASTIO_PASTE(FASTIO_STEPS_, FASTIO_PAIR_STEPS_LL)
#else
            if constexpr (detail::is_signed_of<T>::value) {
                FASTIO_STEPS_9                            // ll 最长 19 位 = 9 对 + 1 单
            } else {
                FASTIO_STEPS_10                           // ull 最长 20 位 = 10 对
            }
#endif
        } else {                                          // ≤32 位
#ifdef FASTIO_PAIR_STEPS_INT
            FASTIO_PASTE(FASTIO_STEPS_, FASTIO_PAIR_STEPS_INT)
#else
            if constexpr (std::numeric_limits<T>::digits10 + 1 <= 3) {
                FASTIO_STEPS_1                            // int8/uint8：≤3 位
            } else if constexpr (std::numeric_limits<T>::digits10 + 1 <= 5) {
                FASTIO_STEPS_2                            // int16/uint16：≤5 位
            } else {
                FASTIO_STEPS_5                            // int32/uint32：10 位
            }
#endif
        }
#undef FASTIO_PASTE
#undef FASTIO_PASTE_
#undef FASTIO_STEPS_0
#undef FASTIO_STEPS_1
#undef FASTIO_STEPS_2
#undef FASTIO_STEPS_3
#undef FASTIO_STEPS_4
#undef FASTIO_STEPS_5
#undef FASTIO_STEPS_6
#undef FASTIO_STEPS_7
#undef FASTIO_STEPS_8
#undef FASTIO_STEPS_9
#undef FASTIO_STEPS_10
#undef FASTIO_STEPS_11
#undef FASTIO_STEPS_12
#undef FASTIO_STEPS_13
#undef FASTIO_STEPS_14
#undef FASTIO_STEPS_15
#undef FASTIO_STEPS_16
#undef FASTIO_STEPS_17
#undef FASTIO_STEPS_18
#undef FASTIO_STEPS_19
#undef FASTIO_STEP
        if ((unsigned)(*q - '0') < 10u) v = U(v * 10 + U(*q++ ^ 48));
        const U mask = U(0) - U(neg);           // 无分支取负（ASSUME_UNSIGNED 下恒 0）
        v = U((v ^ mask) - mask);               // INT_MIN / LLONG_MIN 安全
        return T(v);
    }

    template <class T>
    FASTIO_HOT typename std::enable_if<detail::is_int<T>::value, T>::type read() {
        // 流式（管道/终端）必须 refill 保证窗口有数据 —— 这是缓冲正确性，
        // 不是 EOF 检查，FASTIO_NO_EOF_CHECK 也不跳。mmap/整读下 stream_ 恒 false，
        // 这条分支被完美预测，开销可忽略。
        if (FASTIO_UNLIKELY(stream_)) { skip_ws(); ensure(48); }
        const char* q = p_;
        T v = parse_int<T>(q);
        p_ = q;
        return v;
    }

    // ---- 浮点 --------------------------------------------------------
    template <class T>
    typename std::enable_if<detail::is_flt<T>::value, T>::type read() {
        skip_ws();
        if (FASTIO_UNLIKELY(stream_)) ensure(512);
        char* fin = nullptr;
        double d = std::strtod(p_, &fin);
        if (fin) p_ = fin;
        return T(d);
    }

    // ---- 字符 / 字符串 ------------------------------------------------
    template <class T>
    typename std::enable_if<std::is_same<T, char>::value, T>::type read() {
        skip_ws();
#ifdef FASTIO_NO_EOF_CHECK
        return *p_++;
#else
        return p_ >= end_ ? '\0' : *p_++;
#endif
    }
    template <class T>
    typename std::enable_if<std::is_same<T, std::string>::value, T>::type read() {
        std::string s;
        read(s);
        return s;
    }

    Reader& read(char& c) { c = read<char>(); return *this; }
    Reader& read(bool& b) { b = read<int>() != 0; return *this; }

    Reader& read(std::string& s) {
        s.clear();
        skip_ws();
        for (;;) {
            const char* q = p_;
            while (q < end_ && (unsigned char)*q > ' ') ++q;
            s.append(p_, size_t(q - p_));
            p_ = q;
            if (p_ < end_ || !stream_) break;
            if (!fill()) break;
        }
        return *this;
    }
    Reader& read(char* s) {  // 需保证缓冲够大
        skip_ws();
        for (;;) {
            const char* q = p_;
            while (q < end_ && (unsigned char)*q > ' ') ++q;
            size_t k = size_t(q - p_);
            std::memcpy(s, p_, k);
            s += k;
            p_ = q;
            if (p_ < end_ || !stream_) break;
            if (!fill()) break;
        }
        *s = '\0';
        return *this;
    }

    template <class T>
    typename std::enable_if<detail::is_int<T>::value || detail::is_flt<T>::value, Reader&>::type
    read(T& x) {
        x = read<T>();
        return *this;
    }

    template <class A, class B, class... Rest>
    Reader& read(A& a, B& b, Rest&... rest) {
        read(a);
        return read(b, rest...);
    }

    // 批量读数组：整型在缓冲模式下走「游标常驻寄存器」的最快路径
    template <class T>
    Reader& read_n(T* a, size_t n) {
        if constexpr (detail::is_int<T>::value) {
            if (FASTIO_LIKELY(!stream_)) {
                const char* q = p_;
                for (size_t i = 0; i < n; ++i) a[i] = parse_int<T>(q);
                p_ = q;
                return *this;
            }
        }
        for (size_t i = 0; i < n; ++i) read(a[i]);
        return *this;
    }

    // 整行（含空格，不含换行）
    bool readln(std::string& s) {
        s.clear();
        bool any = false;
        for (;;) {
            const char* q = p_;
            while (q < end_ && *q != '\n') ++q;
            if (q != p_) { s.append(p_, size_t(q - p_)); any = true; }
            p_ = q;
            if (p_ < end_) { ++p_; any = true; break; }
            if (!stream_ || !fill()) break;
        }
        if (!s.empty() && s.back() == '\r') s.pop_back();
        return any;
    }

    template <class T>
    Reader& operator>>(T& x) { return read(x); }

    void release() {
#if FASTIO_HAS_MMAP
        if (map_base_) { munmap(map_base_, map_len_); map_base_ = nullptr; map_len_ = 0; }
#endif
        if (heap_) { std::free(heap_); heap_ = nullptr; }
        if (owns_file_) { std::fclose(owns_file_); owns_file_ = nullptr; }
        p_ = end_ = zero_pad();
        mode_ = EMPTY;
        stream_ = false;
        fin_ = nullptr;
    }

private:
    static const char* zero_pad() {  // 空输入时的哨兵区
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
    long ftell_safe() {
        long t = std::ftell(fin_);
        return t > 0 ? t : 0;
    }

#if FASTIO_HAS_MMAP
    bool try_mmap(int fd, size_t fsize) {
        long off = ftell_safe();
        size_t page = size_t(sysconf(_SC_PAGESIZE));
        size_t total = ((fsize + page - 1) / page) * page + page;  // 末尾留一整页零
        void* base = mmap(nullptr, total, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) return false;
        void* got = mmap(base, fsize, PROT_READ, MAP_PRIVATE | MAP_FIXED
#ifdef MAP_POPULATE
                             | MAP_POPULATE
#endif
                         ,
                         fd, 0);
        if (got == MAP_FAILED) { munmap(base, total); return false; }
        map_base_ = static_cast<char*>(base);
        map_len_ = total;
        p_ = map_base_ + off;
        end_ = map_base_ + fsize;
        mode_ = MAPPED;
        // 尾页是可写匿名页：放哨兵，热循环就不必判边界
        std::memset(map_base_ + ((fsize + page - 1) / page) * page, SENT, page);
        return true;
    }
#endif

    bool slurp(size_t n) {  // 已知大小，一次读完，尾部补零
        char* buf = static_cast<char*>(std::malloc(n + PAD));
        if (!buf) return false;
        size_t got = std::fread(buf, 1, n, fin_);
        std::memset(buf + got, SENT, PAD);
        heap_ = buf;
        p_ = buf;
        end_ = buf + got;
        mode_ = SLURPED;
        return true;
    }

#ifdef FASTIO_INPUT_MAX
    // FASTIO_INPUT_MAX：按用户承诺的上限一次配足缓冲，循环直读到 EOF。
    // 超出承诺的部分直接丢弃（缓冲只有这么大）；malloc 失败兜底为原来的流式。
    void init_gulp(size_t n) {
        char* buf = static_cast<char*>(std::malloc(n + PAD));
        if (!buf) { init_stream(); return; }
        size_t len = 0;
        while (len < n) {
            size_t got = raw_read(buf + len, n - len);
            if (got == 0) break;   // EOF
            len += got;
        }
        std::memset(buf + len, SENT, PAD);
        heap_ = buf;
        p_ = buf;
        end_ = buf + len;
        mode_ = SLURPED;
        stream_ = false;
    }
#endif

    void init_stream() {
        cap_ = size_t(1) << FASTIO_IBUF_BITS;
        heap_ = static_cast<char*>(std::malloc(cap_ + PAD));
        p_ = end_ = heap_;
        std::memset(heap_, SENT, PAD);
        mode_ = STREAM;
        stream_ = true;
        // 不预读：交互题里第一次 read 才去要数据
    }

    // 流式：把剩余字节挪到头部，再补满；返回是否有可读字节
    bool fill() {
        if (mode_ != STREAM) return false;
        size_t rem = size_t(end_ - p_);
        if (rem && p_ != heap_) std::memmove(heap_, p_, rem);
        p_ = heap_;
        size_t got = raw_read(heap_ + rem, cap_ - rem);
        end_ = heap_ + rem + got;
        std::memset(const_cast<char*>(end_), SENT, PAD);
        return end_ != p_;
    }
    // 底层取数：Unix 下直接 read()，避免 fread 在交互/管道时死等填满缓冲
    size_t raw_read(char* dst, size_t n) {
#if FASTIO_UNIX
        int fd = fileno(fin_);
        if (fd >= 0) {
            ssize_t k = ::read(fd, dst, n);
            return k > 0 ? size_t(k) : 0;
        }
#endif
        return std::fread(dst, 1, n, fin_);
    }
    // 流式：保证前方至少 k 字节可读（不足则说明到 EOF，尾部有零填充兜底）
    FASTIO_ALWAYS void ensure(size_t k) {
        if (p_ + k > end_) fill();
    }

    static constexpr size_t PAD = 64;
    static constexpr char SENT = char(0xFF);  // 哨兵：> ' '，非数字、非负号

    FILE* fin_ = nullptr;
    FILE* owns_file_ = nullptr;
    const char* p_ = zero_pad();
    const char* end_ = zero_pad();
    char* heap_ = nullptr;
    char* map_base_ = nullptr;
    size_t map_len_ = 0;
    size_t cap_ = 0;
    Mode mode_ = EMPTY;
    bool stream_ = false;
};

// ==========================================================================
//                                 Writer
//   fwrite 大缓冲 + 四位打表（一次落 4 个字符）
// ==========================================================================
class Writer {
public:
#ifdef FASTIO_OUTPUT_MAX
    // 用户承诺总输出 ≤ FASTIO_OUTPUT_MAX 字节：一次配足，整程只 fwrite 一次。
    // ⚠ 超出承诺 = 堆损坏（stdlib 不拦）。+64 是写整数时定长 24/48 字节拷贝的尾巴。
    static constexpr size_t OBUF = size_t(FASTIO_OUTPUT_MAX);
#else
    static constexpr size_t OBUF = size_t(1) << FASTIO_OBUF_BITS;
#endif

    Writer() : fout_(stdout) {
        buf_ = static_cast<char*>(std::malloc(OBUF + 64));
        cur_ = buf_;
    }
    explicit Writer(FILE* fp) : Writer() { fout_ = fp ? fp : stdout; }
    ~Writer() {
        spill();
        std::free(buf_);
        buf_ = nullptr;
        if (owns_file_) std::fclose(owns_file_);
    }
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    void attach(FILE* fp) {
        spill();
        if (owns_file_) { std::fclose(owns_file_); owns_file_ = nullptr; }
        fout_ = fp ? fp : stdout;
    }
    bool open(const char* path) {
        FILE* fp = std::fopen(path, "wb");
        if (!fp) return false;
        attach(fp);
        owns_file_ = fp;
        return true;
    }

    // 对外 flush：连 stdio 缓冲一起落，交互题用这个
    void flush() {
        spill();
        std::fflush(fout_);
    }
    // 内部：缓冲满时倒进 stdio，不强制 fflush
    FASTIO_ALWAYS void spill() {
        if (cur_ != buf_) {
            std::fwrite(buf_, 1, size_t(cur_ - buf_), fout_);
            cur_ = buf_;
        }
    }

    FASTIO_ALWAYS void put(char c) {
#ifndef FASTIO_OUTPUT_MAX
        if (FASTIO_UNLIKELY(cur_ == buf_ + OBUF)) spill();
#endif
        *cur_++ = c;
    }
    void put_raw(const char* s, size_t n) {
#ifdef FASTIO_OUTPUT_MAX
        // 承诺总量 ≤ MAX：单次必 ≤ MAX，边界判断全删
        std::memcpy(cur_, s, n);
        cur_ += n;
#else
        if (FASTIO_UNLIKELY(n >= OBUF)) {
            spill();
            std::fwrite(s, 1, n, fout_);
            return;
        }
        if (FASTIO_UNLIKELY(size_t(buf_ + OBUF - cur_) < n)) spill();
        std::memcpy(cur_, s, n);
        cur_ += n;
#endif
    }

    // ---- 无符号整数：从低位起每次 4 位查表 ----------------------------
    template <class U>
    FASTIO_HOT void write_uns(U x) {
        constexpr size_t WD = sizeof(U) > 8 ? 48 : 24;  // __int128 最长 39 位 + 符号
        reserve(WD);
        const uint32_t* tb = detail::quad_tbl.v;
        char tmp[WD * 2];  // 尾部定长 WD 字节拷贝最多越 q 一格，留足一倍余量
        char* e = tmp + WD;
        char* q = e;
        while (x >= 10000) {
            q -= 4;
            detail::store32(q, tb[unsigned(x % 10000)]);
            x /= 10000;
        }
        unsigned head = unsigned(x);
        if (head >= 1000) {
            q -= 4;
            detail::store32(q, tb[head]);
        } else {
            uint32_t w = tb[head];
            int skip = head >= 100 ? 1 : head >= 10 ? 2 : 3;
            char four[4];
            detail::store32(four, w);
            q -= (4 - skip);
            std::memcpy(q, four + skip, size_t(4 - skip));
        }
        size_t len = size_t(e - q);
        std::memcpy(cur_, q, WD);  // 定长拷贝，比变长快；已 reserve
        cur_ += len;
    }

    template <class T>
    typename std::enable_if<detail::is_int<T>::value, void>::type write(T x) {
        using U = detail::uns_t<T>;
#ifdef FASTIO_ASSUME_UNSIGNED
        write_uns(U(x));  // 用户保证没有负数：符号分支编译期消失
#else
        if constexpr (detail::is_signed_of<T>::value) {
            if (x < 0) {
                put('-');
                write_uns(U(U(0) - U(x)));  // INT_MIN / LLONG_MIN 安全
                return;
            }
        }
        write_uns(U(x));
#endif
    }

    void write(char c) { put(c); }
    void write(bool b) { put(b ? '1' : '0'); }
    void write(const char* s) { put_raw(s, std::strlen(s)); }
    void write(const std::string& s) { put_raw(s.data(), s.size()); }

    template <class T>
    typename std::enable_if<detail::is_flt<T>::value, void>::type write(T x) {
        reserve(400);
        int k = std::snprintf(cur_, 400, "%.*f", precision_, double(x));
        if (k > 0) cur_ += k;
    }
    void set_precision(int p) { precision_ = p; }

    template <class T>
    Writer& operator<<(const T& x) { write(x); return *this; }

    template <class T>
    void writeln(const T& x) { write(x); put('\n'); }

    template <class... Args>
    void print(const Args&... args) {
        int dummy[] = {0, (write(args), 0)...};
        (void)dummy;
    }
    template <class T>
    void println(const T& x) { write(x); put('\n'); }
    template <class T, class... Rest>
    void println(const T& x, const Rest&... rest) {
        write(x);
        put(' ');
        println(rest...);
    }
    template <class T>
    void write_n(const T* a, size_t n, char sep = ' ', char last = '\n') {
        for (size_t i = 0; i < n; ++i) {
            write(a[i]);
            put(i + 1 == n ? last : sep);
        }
    }

private:
    FASTIO_ALWAYS void reserve(size_t n) {
#ifdef FASTIO_OUTPUT_MAX
        (void)n;   // 承诺总量 ≤ MAX：永不 spill
#else
        if (FASTIO_UNLIKELY(size_t(buf_ + OBUF - cur_) < n)) spill();
#endif
    }
    FILE* fout_ = nullptr;
    FILE* owns_file_ = nullptr;
    char* buf_ = nullptr;
    char* cur_ = nullptr;
    int precision_ = 6;
};

// ==========================================================================
//   FastIO：读写合体，就是全局的 io。既能 io >> x，也能 io << x。
// ==========================================================================
class FastIO : public Reader, public Writer {
public:
    FastIO() = default;
    FastIO(FILE* in, FILE* out) { bind(in, out); }

    void bind(FILE* in, FILE* out) {
        if (in) Reader::attach(in);
        Writer::attach(out ? out : stdout);
    }
    bool open_in(const char* path) { return Reader::open(path); }
    bool open_out(const char* path) { return Writer::open(path); }

    using Reader::eof;
    using Reader::getch;
    using Reader::peek;
    using Reader::read;
    using Reader::read_n;
    using Reader::readln;
    using Reader::operator>>;

    using Writer::flush;
    using Writer::print;
    using Writer::println;
    using Writer::put;
    using Writer::set_precision;
    using Writer::write;
    using Writer::write_n;
    using Writer::writeln;
    using Writer::operator<<;
};

namespace detail {
// 全局对象：构造时就把 stdin 准备好（常规文件 -> mmap；否则流式，不预读）
struct StdIO : FastIO {
    StdIO() { Reader::attach(stdin); }
};
}  // namespace detail

// 进程级默认对象：读 stdin，写 stdout，析构自动 flush
inline detail::StdIO io;

// 旧名字兼容
using UltraReader = Reader;

}  // namespace fastio

using fastio::io;

// ============================================================================
//  选项：FASTIO_REPLACE_CIN_COUT —— 全局 cin / cout / endl 顶替 iostream
//
//      #define FASTIO_REPLACE_CIN_COUT
//      #include "fastio.hpp"
//      int main() { int a, b; cin >> a >> b; cout << a + b << endl; }
//
//  · cin/cout 本质都是 fastio::io：>> 走 mmap 快读、<< 走四位打表快写，
//    endl 就是 '\n'；程序结束自动 flush；
//  · 全局声明的 cin/cout 会盖住 using namespace std 引入的 std::cin，
//    所以就算模板里带着 <bits/stdc++.h>，写 cin/cout 也自动变成快读；
//  · 没有 cin.tie() / setw / fixed 这类 iostream 花活，要格式化请 printf；
//  · std::endl 是函数指针传不进来，用我们自己的 endl（或 '\n'）。
// ============================================================================
#ifdef FASTIO_REPLACE_CIN_COUT
inline fastio::FastIO& cin = fastio::io;
inline fastio::FastIO& cout = fastio::io;
inline constexpr char endl = '\n';
#endif
