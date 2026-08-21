// 对照两篇文章中各档 IO 方法的性能测试
// 编译：g++ -O2 -std=c++17 -o benchmark src/benchmark.cpp
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../include/fastio.hpp"

using clk = std::chrono::steady_clock;
using ns = std::chrono::nanoseconds;

static inline double ms_since(clk::time_point t0) {
    return std::chrono::duration<double, std::milli>(clk::now() - t0).count();
}

// ============================================================================
//  各档读入实现（各自独立，避免互相污染缓冲）
// ============================================================================

static long long read_cin_raw(const char* path) {
    std::ifstream in(path);
    auto* old = std::cin.rdbuf(in.rdbuf());
    int n;
    std::cin >> n;
    long long s = 0;
    for (int i = 0; i < n; ++i) {
        int x;
        std::cin >> x;
        s += x;
    }
    std::cin.rdbuf(old);
    return s;
}

static long long read_cin_fast(const char* path) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::ifstream in(path);
    auto* old = std::cin.rdbuf(in.rdbuf());
    int n;
    std::cin >> n;
    long long s = 0;
    for (int i = 0; i < n; ++i) {
        int x;
        std::cin >> x;
        s += x;
    }
    std::cin.rdbuf(old);
    std::ios::sync_with_stdio(true);
    return s;
}

static long long read_scanf(const char* path) {
    FILE* fp = std::fopen(path, "r");
    int n;
    std::fscanf(fp, "%d", &n);
    long long s = 0;
    for (int i = 0; i < n; ++i) {
        int x;
        std::fscanf(fp, "%d", &x);
        s += x;
    }
    std::fclose(fp);
    return s;
}

template <class Getc>
static long long read_getchar_style(const char* path, Getc getc) {
    FILE* fp = std::fopen(path, "r");
    auto rd = [&]() {
        int k = 0, f = 1, c = getc(fp);
        for (; c != EOF && (c < '0' || c > '9'); c = getc(fp))
            if (c == '-') f = -1;
        for (; c >= '0' && c <= '9'; c = getc(fp)) k = k * 10 + (c ^ 48);
        return k * f;
    };
    int n = rd();
    long long s = 0;
    for (int i = 0; i < n; ++i) s += rd();
    std::fclose(fp);
    return s;
}

static long long read_getchar(const char* path) {
    return read_getchar_style(path, [](FILE* fp) { return std::getc(fp); });
}

static long long read_getchar_unlocked(const char* path) {
    return read_getchar_style(path, [](FILE* fp) { return getc_unlocked(fp); });
}

static long long read_fread(const char* path) {
    FILE* fp = std::fopen(path, "rb");
    static char buf[1 << 20];
    char *p1 = buf, *p2 = buf;
    auto gc = [&]() -> int {
        if (p1 == p2) {
            p1 = buf;
            p2 = buf + std::fread(buf, 1, 1 << 20, fp);
            if (p1 == p2) return EOF;
        }
        return int((unsigned char)*p1++);
    };
    auto rd = [&]() {
        int k = 0, f = 1, c = gc();
        for (; c != EOF && (c < '0' || c > '9'); c = gc())
            if (c == '-') f = -1;
        for (; c >= '0' && c <= '9'; c = gc()) k = k * 10 + (c ^ 48);
        return k * f;
    };
    int n = rd();
    long long s = 0;
    for (int i = 0; i < n; ++i) s += rd();
    std::fclose(fp);
    return s;
}

static long long read_fastio_lib(const char* path) {
    FILE* fp = std::fopen(path, "rb");
    fastio::FastIO fio;
    fio.bind(fp, stdout);
    int n = fio.read<int>();
    long long s = 0;
    for (int i = 0; i < n; ++i) s += fio.read<int>();
    std::fclose(fp);
    return s;
}

static long long read_mmap_byte(const char* path) {
    int fd = open(path, O_RDONLY);
    struct stat st{};
    fstat(fd, &st);
    char* pc = static_cast<char*>(
        mmap(nullptr, size_t(st.st_size), PROT_READ, MAP_PRIVATE, fd, 0));
    close(fd);
    char* p = pc;
    char* e = pc + st.st_size;
    auto rd = [&]() {
        while (p < e && (*p < '0' || *p > '9') && *p != '-') ++p;
        int f = 1;
        if (p < e && *p == '-') {
            f = -1;
            ++p;
        }
        int k = 0;
        while (p < e && *p >= '0' && *p <= '9') {
            k = k * 10 + (*p ^ 48);
            ++p;
        }
        return k * f;
    };
    int n = rd();
    long long s = 0;
    for (int i = 0; i < n; ++i) s += rd();
    munmap(pc, size_t(st.st_size));
    return s;
}

static int32_t g_pair[65536];
static bool g_pair_ok = false;
static void init_pair() {
    if (g_pair_ok) return;
    std::memset(g_pair, 0xFF, sizeof g_pair);
    for (int a = '0'; a <= '9'; ++a)
        for (int b = '0'; b <= '9'; ++b)
            g_pair[a | (b << 8)] = (a ^ 48) * 10 + (b ^ 48);
    g_pair_ok = true;
}

static long long read_mmap_pair(const char* path) {
    init_pair();
    int fd = open(path, O_RDONLY);
    struct stat st{};
    fstat(fd, &st);
    size_t nbyte = size_t(st.st_size);
    char* pc = static_cast<char*>(
        mmap(nullptr, nbyte, PROT_READ, MAP_PRIVATE
#ifdef MAP_POPULATE
                                            | MAP_POPULATE
#endif
             ,
             fd, 0));
    close(fd);
    unsigned char* c = reinterpret_cast<unsigned char*>(pc);

    auto rd = [&]() -> int {
        while (*c <= ' ') ++c;
        int f = 0;
        if (*c == '-') {
            f = 1;
            ++c;
        }
        unsigned v = 0;
        int w;
        if (~(w = g_pair[*reinterpret_cast<uint16_t*>(c)])) v = unsigned(w), c += 2;
        if (~(w = g_pair[*reinterpret_cast<uint16_t*>(c)])) v = v * 100 + unsigned(w), c += 2;
        if (~(w = g_pair[*reinterpret_cast<uint16_t*>(c)])) v = v * 100 + unsigned(w), c += 2;
        if (~(w = g_pair[*reinterpret_cast<uint16_t*>(c)])) v = v * 100 + unsigned(w), c += 2;
        if (~(w = g_pair[*reinterpret_cast<uint16_t*>(c)])) v = v * 100 + unsigned(w), c += 2;
        if (*c >= '0') v = v * 10 + (*c++ ^ 48);
        return f ? -int(v) : int(v);
    };

    int n = rd();
    long long s = 0;
    for (int i = 0; i < n; ++i) s += rd();
    munmap(pc, nbyte);
    return s;
}

static long long read_ultra_lib(const char* path) {
    fastio::UltraReader ur;
    ur.load_file(path);
    int n = ur.read<int>();
    long long s = 0;
    for (int i = 0; i < n; ++i) s += ur.read<int>();
    return s;
}

// streambuf 版（第二篇文章）
static long long read_streambuf(const char* path) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::ifstream fin(path);
    std::streambuf* inbuf = fin.rdbuf();
    constexpr int MAX_INPUT = 1 << 20;
    char buf[MAX_INPUT];
    char *p1 = buf, *p2 = buf;
    auto getc = [&]() -> int {
        if (p1 == p2) {
            auto n = inbuf->sgetn(buf, MAX_INPUT);
            p1 = buf;
            p2 = buf + n;
            if (p1 == p2) return EOF;
        }
        return int((unsigned char)*p1++);
    };
    auto rd = [&]() {
        int k = 0, f = 1, c = getc();
        for (; c != EOF && (c < '0' || c > '9'); c = getc())
            if (c == '-') f = -1;
        for (; c >= '0' && c <= '9'; c = getc()) k = k * 10 + (c ^ 48);
        return k * f;
    };
    int n = rd();
    long long s = 0;
    for (int i = 0; i < n; ++i) s += rd();
    std::ios::sync_with_stdio(true);
    return s;
}

// ============================================================================
//  各档输出实现
// ============================================================================

static void write_cout_raw(const char* path, const std::vector<int>& a) {
    std::ofstream out(path);
    std::ostream* os = &out;
    *os << int(a.size()) << '\n';
    for (int x : a) *os << x << '\n';
}

static void write_cout_fast(const char* path, const std::vector<int>& a) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::ofstream out(path);
    out.tie(nullptr);
    out << int(a.size()) << '\n';
    for (int x : a) out << x << '\n';
    out.flush();
    std::ios::sync_with_stdio(true);
}

static void write_printf(const char* path, const std::vector<int>& a) {
    FILE* fp = std::fopen(path, "w");
    std::fprintf(fp, "%d\n", int(a.size()));
    for (int x : a) std::fprintf(fp, "%d\n", x);
    std::fclose(fp);
}

static void write_putchar(const char* path, const std::vector<int>& a) {
    FILE* fp = std::fopen(path, "w");
    auto wr = [&](int x) {
        char buf[16];
        int len = 0;
        if (x < 0) {
            std::putc('-', fp);
            unsigned u = unsigned(0) - unsigned(x);
            do {
                buf[len++] = char('0' + u % 10);
                u /= 10;
            } while (u);
        } else {
            unsigned u = unsigned(x);
            do {
                buf[len++] = char('0' + u % 10);
                u /= 10;
            } while (u);
        }
        while (len--) std::putc(buf[len], fp);
        std::putc('\n', fp);
    };
    wr(int(a.size()));
    for (int x : a) wr(x);
    std::fclose(fp);
}

static void write_putchar_unlocked(const char* path, const std::vector<int>& a) {
    FILE* fp = std::fopen(path, "w");
    auto wr = [&](int x) {
        char buf[16];
        int len = 0;
        if (x < 0) {
            putc_unlocked('-', fp);
            unsigned u = unsigned(0) - unsigned(x);
            do {
                buf[len++] = char('0' + u % 10);
                u /= 10;
            } while (u);
        } else {
            unsigned u = unsigned(x);
            do {
                buf[len++] = char('0' + u % 10);
                u /= 10;
            } while (u);
        }
        while (len--) putc_unlocked(buf[len], fp);
        putc_unlocked('\n', fp);
    };
    wr(int(a.size()));
    for (int x : a) wr(x);
    std::fclose(fp);
}

static void write_fwrite(const char* path, const std::vector<int>& a) {
    FILE* fp = std::fopen(path, "wb");
    constexpr int BUF = 1 << 22;
    std::vector<char> obuf(BUF + 32);
    char* now = obuf.data();
    char* base = obuf.data();
    auto flush = [&]() {
        std::fwrite(base, 1, size_t(now - base), fp);
        now = base;
    };
    auto pc = [&](char c) {
        if (now - base > BUF) flush();
        *now++ = c;
    };
    auto wr = [&](int x) {
        if (x == 0) {
            pc('0');
            pc('\n');
            return;
        }
        char tmp[16];
        int len = 0;
        unsigned u;
        if (x < 0) {
            pc('-');
            u = unsigned(0) - unsigned(x);
        } else {
            u = unsigned(x);
        }
        do {
            tmp[len++] = char('0' + u % 10);
            u /= 10;
        } while (u);
        if (now - base + len + 2 > BUF) flush();
        while (len--) *now++ = tmp[len];
        *now++ = '\n';
    };
    wr(int(a.size()));
    for (int x : a) wr(x);
    flush();
    std::fclose(fp);
}

static uint32_t g_d4[10000];
static bool g_d4_ok = false;
static void init_d4() {
    if (g_d4_ok) return;
    for (int n = 0; n < 10000; ++n) {
        g_d4[n] = uint32_t(n / 1000 % 10 + '0') |
                  (uint32_t(n / 100 % 10 + '0') << 8) |
                  (uint32_t(n / 10 % 10 + '0') << 16) |
                  (uint32_t(n % 10 + '0') << 24);
    }
    g_d4_ok = true;
}

static void write_fwrite_table(const char* path, const std::vector<int>& a) {
    init_d4();
    FILE* fp = std::fopen(path, "wb");
    constexpr int BUF = 1 << 22;
    std::vector<char> obuf(BUF + 64);
    char* now = obuf.data();
    char* base = obuf.data();
    auto flush = [&]() {
        std::fwrite(base, 1, size_t(now - base), fp);
        now = base;
    };
    auto wr = [&](int x) {
        if (now - base > BUF) flush();
        if (x == 0) {
            *now++ = '0';
            *now++ = '\n';
            return;
        }
        if (x < 0) {
            *now++ = '-';
            unsigned u = unsigned(0) - unsigned(x);
            char tmp[16];
            char* end = tmp + 16;
            char* p = end;
            while (u >= 10000) {
                p -= 4;
                *reinterpret_cast<uint32_t*>(p) = g_d4[u % 10000];
                u /= 10000;
            }
            char grp[4];
            *reinterpret_cast<uint32_t*>(grp) = g_d4[u];
            int skip = 0;
            while (skip < 3 && grp[skip] == '0') ++skip;
            int keep = 4 - skip;
            std::memcpy(now, grp + skip, size_t(keep));
            now += keep;
            std::memcpy(now, p, size_t(end - p));
            now += end - p;
            *now++ = '\n';
            return;
        }
        unsigned u = unsigned(x);
        char tmp[16];
        char* end = tmp + 16;
        char* p = end;
        while (u >= 10000) {
            p -= 4;
            *reinterpret_cast<uint32_t*>(p) = g_d4[u % 10000];
            u /= 10000;
        }
        char grp[4];
        *reinterpret_cast<uint32_t*>(grp) = g_d4[u];
        int skip = 0;
        while (skip < 3 && grp[skip] == '0') ++skip;
        int keep = 4 - skip;
        std::memcpy(now, grp + skip, size_t(keep));
        now += keep;
        std::memcpy(now, p, size_t(end - p));
        now += end - p;
        *now++ = '\n';
    };
    wr(int(a.size()));
    for (int x : a) wr(x);
    flush();
    std::fclose(fp);
}

static void write_fastio_lib(const char* path, const std::vector<int>& a) {
    FILE* fp = std::fopen(path, "wb");
    fastio::FastIO fio;
    fio.bind(stdin, fp);
    fio << int(a.size()) << '\n';
    for (int x : a) fio << x << '\n';
    fio.flush();
    std::fclose(fp);
}

// ============================================================================
//  数据生成 / 计时框架
// ============================================================================

struct Row {
    std::string suite;
    std::string name;
    std::string group;  // read / write
    std::string article;
    double ms = 0;
    long long checksum = 0;
    bool ok = true;
    std::string note;
};

static uint32_t rng_state = 0x9E3779B9u;
static inline uint32_t rng() {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static std::vector<int> gen_mixed(int n, uint32_t seed) {
    rng_state = seed;
    std::vector<int> a(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        uint32_t r = rng();
        int mag;
        int bin = int(r % 100);
        if (bin < 10)
            mag = int(r % 10);  // 1 位
        else if (bin < 25)
            mag = int(r % 100);
        else if (bin < 45)
            mag = int(r % 10000);
        else if (bin < 70)
            mag = int(r % 1000000);
        else
            mag = int(r % 1000000000);
        a[size_t(i)] = (r & 1) ? -mag : mag;
    }
    a[0] = 0;
    if (n > 1) a[1] = -2147483647 - 1;  // INT_MIN
    if (n > 2) a[2] = 2147483647;
    return a;
}

static std::vector<int> gen_dense(int n, uint32_t seed) {
    rng_state = seed;
    std::vector<int> a(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        uint32_t r = rng();
        int mag = 100000000 + int(r % 900000000);
        a[size_t(i)] = (r & 4) ? -mag : mag;
    }
    return a;
}

static void write_input_file(const char* path, const std::vector<int>& a) {
    FILE* fp = std::fopen(path, "wb");
    constexpr int BUF = 1 << 22;
    std::vector<char> buf(BUF + 64);
    char* now = buf.data();
    char* base = buf.data();
    auto flush = [&]() {
        std::fwrite(base, 1, size_t(now - base), fp);
        now = base;
    };
    auto emit = [&](int x) {
        if (now - base > BUF) flush();
        if (x < 0) {
            *now++ = '-';
            unsigned u = unsigned(0) - unsigned(x);
            char tmp[16];
            int n = 0;
            do {
                tmp[n++] = char('0' + u % 10);
                u /= 10;
            } while (u);
            while (n--) *now++ = tmp[n];
        } else {
            unsigned u = unsigned(x);
            char tmp[16];
            int n = 0;
            do {
                tmp[n++] = char('0' + u % 10);
                u /= 10;
            } while (u);
            while (n--) *now++ = tmp[n];
        }
    };
    emit(int(a.size()));
    *now++ = '\n';
    for (size_t i = 0; i < a.size(); ++i) {
        emit(a[i]);
        *now++ = ((i + 1) % 16 == 0) ? '\n' : ' ';
    }
    *now++ = '\n';
    for (int i = 0; i < 32; ++i) *now++ = ' ';  // mmap 双字节打表越界垫片
    flush();
    std::fclose(fp);
}

static long long checksum_of(const std::vector<int>& a) {
    long long s = 0;
    for (int x : a) s += x;
    return s;
}

template <class Fn>
static double time_median(Fn fn, int rounds) {
    std::vector<double> ts;
    ts.reserve(size_t(rounds));
    for (int i = 0; i < rounds; ++i) {
        auto t0 = clk::now();
        fn();
        ts.push_back(ms_since(t0));
    }
    std::sort(ts.begin(), ts.end());
    return ts[ts.size() / 2];
}

static std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '&')
            o += "&amp;";
        else if (c == '<')
            o += "&lt;";
        else if (c == '>')
            o += "&gt;";
        else
            o += c;
    }
    return o;
}

static std::string bar_color(const std::string& name) {
    if (name.find("Ultra") != std::string::npos ||
        name.find("打表") != std::string::npos)
        return "#e85d04";
    if (name.find("FastIO") != std::string::npos ||
        name.find("mmap") != std::string::npos)
        return "#dc2f02";
    if (name.find("fread") != std::string::npos ||
        name.find("fwrite") != std::string::npos)
        return "#f48c06";
    if (name.find("unlocked") != std::string::npos)
        return "#faa307";
    if (name.find("getchar") != std::string::npos ||
        name.find("putchar") != std::string::npos)
        return "#ffba08";
    if (name.find("scanf") != std::string::npos ||
        name.find("printf") != std::string::npos)
        return "#2a9d8f";
    if (name.find("关同步") != std::string::npos)
        return "#457b9d";
    return "#6c757d";
}

int main(int argc, char** argv) {
    int target_mb = 100;  // 目标输入文件大小（MiB）
    int rounds = 3;
    if (argc >= 2) target_mb = std::atoi(argv[1]);
    if (argc >= 3) rounds = std::atoi(argv[2]);
    if (target_mb < 1) target_mb = 1;
    if (rounds < 1) rounds = 1;
    const size_t target_bytes = size_t(target_mb) * 1024ull * 1024ull;
    // 上次实测：混合约 6.81 B/数，9 位稠密约 10.50 B/数
    const int n_mixed = std::max(1000, int(target_bytes / 6.81));
    const int n_dense = std::max(1000, int(target_bytes / 10.50));

    struct ReadCase {
        const char* name;
        const char* article;
        const char* note;
        long long (*fn)(const char*);
        bool skip_large;
    };
    ReadCase reads[] = {
        {"cin 默认（同步开启）", "文1 §1 / 文2 前言",
         "C++ 为兼容 C 把 cin 和 stdio 绑在一起，最慢的一档", read_cin_raw,
         false},
        {"cin 关同步 + untie", "文1 §1 / 文2 §五",
         "ios::sync_with_stdio(0), cin.tie(0)", read_cin_fast, false},
        {"scanf", "文1 对照 / 文2 前言", "C 风格格式化输入", read_scanf,
         false},
        {"getchar 手写整型", "文1 §2 / 文2 §一",
         "逐字节 + isdigit 累乘，最经典的快读", read_getchar, false},
        {"getchar_unlocked", "文1 §2", "去掉线程锁的 getchar",
         read_getchar_unlocked, false},
        {"fread 缓冲快读", "文1 §3 / 文2 §三", "一次吞 1MiB，再从内存取字符",
         read_fread, false},
        {"streambuf::sgetn", "文2 §五", "iostream 底层缓冲区，本质接近 fread",
         read_streambuf, false},
        {"FastIO 库（fread）", "本文库", "通用流式快读，INT_MIN 安全，可混读字符串",
         read_fastio_lib, false},
        {"mmap 单字节", "文1 §4.2.1", "把整个文件映射进地址空间",
         read_mmap_byte, false},
        {"mmap + 双字节打表", "文1 §4.2.2",
         "一次吃两个数字字符，65536 项查找表，循环展开", read_mmap_pair, false},
        {"UltraReader 库", "本文库", "mmap/整读 + 双字节打表 + INT_MIN 安全",
         read_ultra_lib, false},
    };

    struct WriteCase {
        const char* name;
        const char* article;
        const char* note;
        void (*fn)(const char*, const std::vector<int>&);
        bool skip_large;
    };
    WriteCase writes[] = {
        {"cout 默认（同步开启）", "文1 二§1", "未关同步的 iostream",
         write_cout_raw, false},
        {"cout 关同步 + untie", "文1 二§1 / 文2 §四",
         "sync_with_stdio(0) + tie(0)", write_cout_fast, false},
        {"printf", "文1 二§2", "C 风格格式化输出，并不快", write_printf, false},
        {"putchar 手写整型", "文1 二§2 / 文2 §四", "逐位除 10 再逆序吐出",
         write_putchar, false},
        {"putchar_unlocked", "文1 二§2", "去掉线程锁的 putchar",
         write_putchar_unlocked, false},
        {"fwrite 缓冲快写", "文1 二§3 / 文2 §四", "先堆在 4MiB 缓冲再一次性写",
         write_fwrite, false},
        {"fwrite + 四位打表", "文1 二§4", "一次写出 4 个 ASCII 数字",
         write_fwrite_table, false},
        {"FastIO 库（fwrite+打表）", "本文库",
         "四位打表 + 无符号绕开 INT_MIN + 析构 flush", write_fastio_lib, false},
    };

    struct Suite {
        const char* id;
        const char* title;
        int n;
        bool large;
        std::vector<int> (*gen)(int, uint32_t);
        uint32_t seed;
    };
    Suite suites[] = {
        {"mixed", "混合位数 ≈100MiB（日常题面）", n_mixed, false, gen_mixed,
         20260812u},
        {"dense", "9 位稠密 ≈100MiB（P10815 风格）", n_dense, false, gen_dense,
         0xC0FFEEu},
    };

    const char* in_path = "/tmp/fastio_in.txt";
    const char* out_path = "/tmp/fastio_out.txt";
    std::vector<Row> rows;
    double small_mib = 0, mixed_mib = 0, dense_mib = 0;
    (void)small_mib;
    long long expect_mixed = 0, expect_dense = 0;

    auto run_suite = [&](const Suite& su) {
        std::fprintf(stderr, "\n========== %s  n=%d ==========\n", su.title,
                     su.n);
        auto data = su.gen(su.n, su.seed);
        long long expect = checksum_of(data);
        write_input_file(in_path, data);
        struct stat st{};
        stat(in_path, &st);
        double mib = double(st.st_size) / (1024.0 * 1024.0);
        if (std::string(su.id) == "small") small_mib = mib;
        if (std::string(su.id) == "mixed") {
            mixed_mib = mib;
            expect_mixed = expect;
        }
        if (std::string(su.id) == "dense") {
            dense_mib = mib;
            expect_dense = expect;
        }
        std::fprintf(stderr, "[*] %.2f MiB  checksum=%lld\n", mib, expect);

        std::fprintf(stderr, "---- 读入 ----\n");
        for (auto& c : reads) {
            if (c.skip_large && su.large) {
                std::fprintf(stderr, "  skip %s\n", c.name);
                Row r;
                r.suite = su.id;
                r.name = c.name;
                r.group = "read";
                r.article = c.article;
                r.note = std::string(c.note) + "（本规模跳过）";
                r.ok = false;
                rows.push_back(r);
                continue;
            }
            std::fprintf(stderr, "  %-22s ", c.name);
            std::fflush(stderr);
            long long got = 0;
            bool ok = true;
            double med = 0;
            got = c.fn(in_path);
            ok = (got == expect);
            med = time_median([&]() { got = c.fn(in_path); }, rounds);
            ok = ok && (got == expect);
            Row r;
            r.suite = su.id;
            r.name = c.name;
            r.group = "read";
            r.article = c.article;
            r.note = c.note;
            r.ms = med;
            r.checksum = got;
            r.ok = ok;
            rows.push_back(r);
            std::fprintf(stderr, "%7.2f ms  %s\n", med, ok ? "OK" : "MISMATCH");
        }

        std::fprintf(stderr, "---- 输出 ----\n");
        for (auto& c : writes) {
            if (c.skip_large && su.large) {
                std::fprintf(stderr, "  skip %s\n", c.name);
                Row r;
                r.suite = su.id;
                r.name = c.name;
                r.group = "write";
                r.article = c.article;
                r.note = std::string(c.note) + "（本规模跳过）";
                r.ok = false;
                rows.push_back(r);
                continue;
            }
            std::fprintf(stderr, "  %-22s ", c.name);
            std::fflush(stderr);
            c.fn(out_path, data);
            double med = time_median([&]() { c.fn(out_path, data); }, rounds);
            Row r;
            r.suite = su.id;
            r.name = c.name;
            r.group = "write";
            r.article = c.article;
            r.note = c.note;
            r.ms = med;
            r.ok = true;
            rows.push_back(r);
            std::fprintf(stderr, "%7.2f ms\n", med);
        }
    };

    for (auto& su : suites) run_suite(su);

    auto best_of = [&](const std::string& suite, const std::string& group) {
        double b = 1e100;
        for (auto& r : rows)
            if (r.suite == suite && r.group == group && r.ok && r.ms > 0)
                b = std::min(b, r.ms);
        return b;
    };

    auto dump_md_table = [&](std::ostringstream& md, const std::string& suite,
                             const std::string& group) {
        double best = best_of(suite, group);
        if (group == "read")
            md << "| 方法 | 来源 | 中位耗时 | 相对最快 | 校验 |\n|---|---|---:|---:|:---:|\n";
        else
            md << "| 方法 | 来源 | 中位耗时 | 相对最快 |\n|---|---|---:|---:|\n";
        for (auto& r : rows) {
            if (r.suite != suite || r.group != group) continue;
            md << "| " << r.name << " | " << r.article << " | ";
            if (!r.ok && r.ms == 0) {
                md << (group == "read" ? "— | — | skip |\n" : "— | — |\n");
            } else if (group == "read") {
                md << std::fixed << std::setprecision(2) << r.ms << " ms | "
                   << std::setprecision(2) << (r.ms / best) << "× | "
                   << (r.ok ? "✓" : "✗") << " |\n";
            } else {
                md << std::fixed << std::setprecision(2) << r.ms << " ms | "
                   << std::setprecision(2) << (r.ms / best) << "× |\n";
            }
        }
    };

    std::ostringstream md;
    md << "# Super FastIO 性能测试报告\n\n";
    md << "- 日期：2026-08-12\n";
    md << "- 目标输入约 **" << target_mb << " MiB**，每档 **" << rounds
       << "** 次中位数（另有 1 次预热）\n";
    md << "- 编译：`g++ -O2 -std=c++17`，2 核 Intel Xeon @ 2.60GHz，约 1.9 GiB RAM\n";
    md << "- 混合位数 n = " << n_mixed << "（" << std::fixed << std::setprecision(2)
       << mixed_mib << " MiB，checksum `" << expect_mixed << "`）\n";
    md << "- 9 位稠密 n = " << n_dense << "（" << dense_mib << " MiB，checksum `"
       << expect_dense << "`）\n\n";

    md << "## 一、混合位数（日常题面：1～10 位、正负、INT_MIN）\n\n### 读入\n\n";
    dump_md_table(md, "mixed", "read");
    md << "\n### 输出\n\n";
    dump_md_table(md, "mixed", "write");

    md << "\n## 二、9 位稠密（洛谷 P10815 / Fast Write 那种卡常数据）\n\n### 读入\n\n";
    dump_md_table(md, "dense", "read");
    md << "\n### 输出\n\n";
    dump_md_table(md, "dense", "write");

    md << "\n## 方法说明\n\n";
    {
        std::vector<std::string> seen;
        for (auto& r : rows) {
            if (r.suite != "mixed") continue;
            md << "- **" << r.name << "**（" << r.article << "）：" << r.note
               << "\n";
        }
    }

    md << "\n## 结论（结合两篇文章）\n\n";
    md << "1. **关同步的 cin/cout** 已经能打过 scanf/printf，日常题够用。\n";
    md << "2. **getchar / getchar_unlocked** 是第一档真正的「快读」，实现短、可移植。\n";
    md << "3. **fread / streambuf** 把系统调用摊掉，是正式赛里最稳的「超级快读」。\n";
    md << "4. **mmap + 双字节打表** 在位数整齐（9 位）时才明显领先；短数字上分支更多，不一定更快。\n";
    md << "5. **fwrite 缓冲** 比 putchar 再快一截；四位打表同样更吃长整数。\n";
    md << "6. **不要在交互题用 mmap / 不解绑后忘记 flush**。\n";

    std::ofstream("report.md") << md.str();

    auto emit_chart = [&](std::ostringstream& html, const std::string& suite,
                          const std::string& group, const std::string& title) {
        std::vector<Row*> vis;
        double mx = 0;
        for (auto& r : rows) {
            if (r.suite == suite && r.group == group && r.ok && r.ms > 0) {
                vis.push_back(&r);
                mx = std::max(mx, r.ms);
            }
        }
        html << "<h3>" << title << "</h3>\n<div class='chart'>\n";
        for (auto* r : vis) {
            double pct = mx > 0 ? (r->ms / mx * 100.0) : 0;
            html << "<div class='row'><div class='lab'>" << esc(r->name)
                 << "</div><div class='track'><div class='bar' style='width:"
                 << std::fixed << std::setprecision(2) << pct
                 << "%;background:" << bar_color(r->name) << "'></div></div>"
                 << "<div class='val'>" << std::setprecision(1) << r->ms
                 << " ms</div></div>\n";
        }
        html << "</div>\n";
    };

    auto emit_table = [&](std::ostringstream& html, const std::string& suite,
                          const std::string& group) {
        double best = best_of(suite, group);
        html << "<table><tr><th>方法</th><th>来源</th><th>耗时</th><th>相对</th>";
        if (group == "read") html << "<th>校验</th>";
        html << "</tr>";
        for (auto& r : rows) {
            if (r.suite != suite || r.group != group) continue;
            html << "<tr><td>" << esc(r.name) << "</td><td class='note'>"
                 << esc(r.article) << "</td>";
            if (!r.ok && r.ms == 0) {
                html << "<td class='num'>—</td><td class='num'>—</td>";
                if (group == "read") html << "<td>skip</td>";
                html << "</tr>";
            } else {
                html << "<td class='num'>" << std::fixed << std::setprecision(2)
                     << r.ms << " ms</td><td class='num'>"
                     << std::setprecision(2) << (r.ms / best) << "×</td>";
                if (group == "read")
                    html << "<td class='" << (r.ok ? "ok" : "bad") << "'>"
                         << (r.ok ? "OK" : "FAIL") << "</td>";
                html << "</tr>";
            }
        }
        html << "</table>";
    };

    std::ostringstream html;
    html << R"HTML(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8"/>
<title>Super FastIO 性能测试</title>
<style>
  :root { --bg:#0f1419; --card:#1a2332; --ink:#e7ecf3; --muted:#8b9bb4; --line:#2a3548; --acc:#ff6b35; }
  * { box-sizing:border-box; }
  body { margin:0; font-family:"Iowan Old Style","Palatino Linotype",Palatino,"Songti SC","Noto Serif SC",serif;
         background:var(--bg); color:var(--ink); line-height:1.6; }
  .wrap { max-width:980px; margin:0 auto; padding:48px 28px 80px; }
  h1 { font-size:34px; font-weight:700; letter-spacing:-.03em; margin:0 0 8px; }
  .sub { color:var(--muted); margin-bottom:28px; font-size:15px; }
  .meta { display:flex; flex-wrap:wrap; gap:10px; margin:0 0 36px; }
  .pill { background:var(--card); border:1px solid var(--line); border-radius:999px;
          padding:6px 14px; font-size:13px; color:var(--muted); font-family:ui-monospace,Menlo,Consolas,monospace; }
  h2 { font-size:22px; margin:44px 0 8px; font-weight:650; }
  h3 { font-size:16px; margin:22px 0 10px; color:#c5d0e0; font-weight:600; }
  .lead { color:var(--muted); margin:0 0 16px; }
  .chart { background:var(--card); border:1px solid var(--line); border-radius:16px; padding:18px 18px 10px; }
  .row { display:grid; grid-template-columns: 220px 1fr 88px; gap:12px; align-items:center; margin-bottom:10px; }
  .lab { font-size:13px; color:#c5d0e0; text-align:right; }
  .track { background:#0d1218; border-radius:6px; height:18px; overflow:hidden; }
  .bar { height:100%; border-radius:6px; min-width:2px; }
  .val { font-family:ui-monospace,Menlo,Consolas,monospace; font-size:12px; color:#ffd166; }
  table { width:100%; border-collapse:collapse; background:var(--card);
          border:1px solid var(--line); border-radius:16px; overflow:hidden; margin:12px 0 8px; }
  th,td { padding:10px 12px; text-align:left; border-bottom:1px solid var(--line); font-size:14px; }
  th { color:var(--muted); font-weight:600; font-size:12px; letter-spacing:.04em; text-transform:uppercase; }
  td.num { font-family:ui-monospace,Menlo,Consolas,monospace; text-align:right; }
  tr:last-child td { border-bottom:0; }
  .ok { color:#6ee7b7; } .bad { color:#fb7185; }
  .note { color:var(--muted); font-size:14px; }
  code { font-family:ui-monospace,Menlo,Consolas,monospace; background:#0d1218;
         padding:1px 6px; border-radius:4px; font-size:13px; }
  .foot { margin-top:48px; color:var(--muted); font-size:13px; }
  ol.concl { color:#c5d0e0; }
  @media (max-width:720px){ .row{grid-template-columns:1fr; } .lab{text-align:left;} }
</style>
</head>
<body>
<div class="wrap">
<h1>Super FastIO 性能测试</h1>
<p class="sub">综合两篇洛谷 IO 优化文：从关同步、getchar、fread，到 mmap 双字节打表与 fwrite 四位打表。三组数据分开测，避免「短数字上打表反而更慢」被平均掉。</p>
<div class="meta">
)HTML";
    html << "<span class='pill'>target " << target_mb << " MiB</span>";
    html << "<span class='pill'>mixed n=" << n_mixed << "</span>";
    html << "<span class='pill'>dense n=" << n_dense << "</span>";
    html << "<span class='pill'>median of " << rounds << "</span>";
    html << "<span class='pill'>g++ -O2 -std=c++17</span>";
    html << "</div>\n";

    html << "<h2>一、混合位数 ≈100MiB</h2><p class='lead'>n = " << n_mixed << "，"
         << std::fixed << std::setprecision(2) << mixed_mib
         << " MiB。含 0 / INT_MIN / INT_MAX 和 1～10 位正负数。</p>";
    emit_chart(html, "mixed", "read", "读入");
    emit_table(html, "mixed", "read");
    emit_chart(html, "mixed", "write", "输出");
    emit_table(html, "mixed", "write");

    html << "<h2>二、9 位稠密 ≈100MiB</h2><p class='lead'>n = " << n_dense << "，"
         << dense_mib << " MiB。对齐洛谷 P10815 / Fast Write。</p>";
    emit_chart(html, "dense", "read", "读入");
    emit_table(html, "dense", "read");
    emit_chart(html, "dense", "write", "输出");
    emit_table(html, "dense", "write");

    html << "<h2>怎么选</h2><ol class='concl'>";
    html << "<li>正式赛、时间不太紧：<code>ios::sync_with_stdio(0), cin.tie(0)</code>。</li>";
    html << "<li>需要稳的常数：<code>FastIO</code>（fread + fwrite 打表），头文件直接丢进去。</li>";
    html << "<li>输入全是长整数、并且允许 Linux：<code>UltraReader</code> / mmap 双字节打表。</li>";
    html << "<li>交互题禁止解绑后忘记 flush；不要对终端 stdin 用 mmap。</li>";
    html << "</ol>";

    html << "<p class='foot'>INT_MIN / 0 已在 FastIO 中用无符号绕开。"
            "本报告由 <code>src/benchmark.cpp</code> 自动生成。</p>";
    html << "</div></body></html>";

    std::ofstream("report.html") << html.str();

    std::ofstream js("results.json");
    js << "{\n  \"target_mb\": " << target_mb << ",\n  \"n_mixed\": " << n_mixed
       << ",\n  \"n_dense\": " << n_dense << ",\n  \"rounds\": " << rounds
       << ",\n  \"rows\": [\n";
    for (size_t i = 0; i < rows.size(); ++i) {
        auto& r = rows[i];
        js << "    {\"suite\":\"" << r.suite << "\",\"name\":\"" << r.name
           << "\",\"group\":\"" << r.group << "\",\"ms\":" << std::fixed
           << std::setprecision(3) << r.ms << ",\"ok\":"
           << (r.ok ? "true" : "false") << "}";
        js << (i + 1 == rows.size() ? "\n" : ",\n");
    }
    js << "  ]\n}\n";

    std::printf("\n报告已写入 report.md / report.html / results.json\n");
    return 0;
}
