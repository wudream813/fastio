// 减分支/激进选项的编译期自检：本文件用不同的 -D 组合分别编译（见 Makefile）：
//   （默认）               全功能：负号 / INT_MIN / LLONG_MIN / ULLONG_MAX / +前缀 / __int128
//   FASTIO_NO_EOF_CHECK          忽略 EOF
//   FASTIO_ASSUME_UNSIGNED       无负号（此时只生成非负数据）
//   FASTIO_PAIR_STEPS_INT=3 -DFASTIO_PAIR_STEPS_LL=4   位数预算收窄
//   FASTIO_REPLACE_CIN_COUT      cin/cout/endl 顶替 iostream
//   FASTIO_INPUT_MAX / FASTIO_OUTPUT_MAX           激进的一次读 / 一次写承诺
// 每个编译产物都把 8 个手写解析层（mmap/ultra/fread/streambuf/mmap_byte/
// getchar/getchar_unlocked/主库）跑同一份数据交叉比对，写侧 Writer 逐字节比对。
// 注意：INPUT_MAX 会一口气把 stdin 读到 EOF —— Makefile 里已用 < /dev/null 防挂。
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../include/fastio_all.hpp"

static const char* IN = "/tmp/fastio_opt.in";
[[maybe_unused]] static const char* IN2 = "/tmp/fastio_opt2.in";
[[maybe_unused]] static const char* OUT2 = "/tmp/fastio_opt2.out";

static std::string read_text(const char* path) {
    std::ifstream f(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

// ---- 按编译期选项计算「位数预算」 ------------------------------------------
#ifdef FASTIO_PAIR_STEPS_INT
static constexpr int IDI = FASTIO_PAIR_STEPS_INT * 2 + 1 > 10 ? 10
                                                              : FASTIO_PAIR_STEPS_INT * 2 + 1;
#else
static constexpr int IDI = 10;   // 自动档：int/uint 恰好 10 位预算
#endif
#ifdef FASTIO_PAIR_STEPS_LL
static constexpr int LDI = FASTIO_PAIR_STEPS_LL * 2 + 1 > 18 ? 18
                                                             : FASTIO_PAIR_STEPS_LL * 2 + 1;
#else
static constexpr int LDI = 18;   // 自动档：ll 19 位预算；随机数压 18 位（满位边界单列）
#endif
static long long pow10_(int n) {
    long long r = 1;
    for (int i = 0; i < n; ++i) r *= 10;
    return r;
}
[[maybe_unused]] static long long nines(int n) { return pow10_(n) - 1; }

// ---- __int128 慢速参考解析/格式化（跟库实现独立，用来交叉验证） -------------
#if defined(__SIZEOF_INT128__)
static __uint128_t to_u128(const char* s) {
    if (*s == '+') ++s;
    __uint128_t v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + unsigned(*s++ - '0');
    return v;
}
static __int128 to_i128(const char* s) {
    if (*s == '-') return -(__int128)to_u128(s + 1);
    return (__int128)to_u128(s);
}
static int u128_to_str(__uint128_t v, char* out) {
    char tmp[48];
    int k = 0;
    do { tmp[k++] = char('0' + int(v % 10)); v /= 10; } while (v);
    int n = 0;
    while (k) out[n++] = tmp[--k];
    out[n] = 0;
    return n;
}
static int i128_to_str(__int128 x, char* out) {
    if (x < 0) {
        out[0] = '-';
        return 1 + u128_to_str((__uint128_t)0 - (__uint128_t)x, out + 1);
    }
    return u128_to_str((__uint128_t)x, out);
}
static __uint128_t pow10_128(int n) {
    __uint128_t r = 1;
    for (int i = 0; i < n; ++i) r *= 10;
    return r;
}
#ifdef FASTIO_PAIR_STEPS_I128
static constexpr int WDI = FASTIO_PAIR_STEPS_I128 * 2 + 1 > 38 ? 38
                                                               : FASTIO_PAIR_STEPS_I128 * 2 + 1;
#else
static constexpr int WDI = 38;   // 自动档：39 位预算；随机数压 38 位（满位边界单列）
#endif
#endif  // __SIZEOF_INT128__

// ---- 造数据（同步产出文本与期望向量） ---------------------------------------
struct Gen {
    std::string text;
    std::vector<int> iv;
    std::vector<long long> lv;
    std::vector<unsigned long long> uv;
#if defined(__SIZEOF_INT128__)
    std::vector<__int128> xv;
    std::vector<__uint128_t> xuv;
#endif
    char tmp[48];

    void raw(const char* tok) { text += tok; text += ' '; }
    void emit_int(int x) {
        std::snprintf(tmp, sizeof tmp, "%d", x);
        raw(tmp);
        iv.push_back(x);
    }
    void emit_int_raw(const char* tok, int x) { raw(tok); iv.push_back(x); }  // +99 之类
    void emit_ll(long long x) {
        std::snprintf(tmp, sizeof tmp, "%lld", x);
        raw(tmp);
        lv.push_back(x);
    }
    void emit_ull(unsigned long long x) {
        std::snprintf(tmp, sizeof tmp, "%llu", x);
        raw(tmp);
        uv.push_back(x);
    }
#if defined(__SIZEOF_INT128__)
    void emit_i128_str(const char* tok) { raw(tok); xv.push_back(to_i128(tok)); }
    void emit_u128_str(const char* tok) { raw(tok); xuv.push_back(to_u128(tok)); }
    void emit_i128(__int128 x) {
        i128_to_str(x, tmp);
        raw(tmp);
        xv.push_back(x);
    }
#endif
};

static unsigned long long g_s = 88172645463325252ULL;
static unsigned long long nextrand() {
    g_s ^= g_s << 13; g_s ^= g_s >> 7; g_s ^= g_s << 17;
    return g_s;
}

static int rnd_int(unsigned long long raw) {
    long long lim = pow10_(IDI);
    if (lim > 2147483648LL) lim = 2147483648LL;      // 保证能装进 int
    long long x = (long long)(raw % (unsigned long long)lim);  // [0, lim)
#ifndef FASTIO_ASSUME_UNSIGNED
    if ((raw >> 32) & 1) x = -x;                     // |x| ≤ INT_MAX，取负安全
#endif
    return (int)x;
}
static long long rnd_ll(unsigned long long raw) {
    long long lim = pow10_(LDI);                     // ≤ 1e18，装得进 ll
    long long x = (long long)(raw % (unsigned long long)lim);
#ifndef FASTIO_ASSUME_UNSIGNED
    if ((raw >> 40) & 1) x = -x;
#endif
    return x;
}
#if defined(__SIZEOF_INT128__)
static __int128 rnd_i128() {
    __uint128_t r = ((__uint128_t)nextrand() << 64) | nextrand();
    __uint128_t lim = pow10_128(WDI);                // ≤ 1e38，装得进 i128
    __int128 x = (__int128)(r % lim);
#ifndef FASTIO_ASSUME_UNSIGNED
    if (nextrand() & 1) x = -x;
#endif
    return x;
}
#endif

static Gen gen_data() {
    Gen g;
    // —— int 区 ——：边界值按当前选项挑合法的
    g.emit_int(0);
    g.emit_int(1);
#if FASTIO_PAIR_STEPS_INT >= 5
    g.emit_int(INT_MAX);
  #ifndef FASTIO_ASSUME_UNSIGNED
    g.emit_int(INT_MIN + 1);          // 10 位负值；INT_MIN 留给 variants_test
    g.emit_int(-1);
  #ifndef FASTIO_NO_WS_SKIP
    g.emit_int_raw("+99", 99);        // + 前缀也认（NO_WS_SKIP 承诺首字符仅数字或 '-'）
  #endif
  #endif
#else
    g.emit_int((int)nines(IDI));      // 位数预算内满位值
#endif
    g.emit_int(42);
    for (int i = 0; i < 100000; ++i) g.emit_int(rnd_int(nextrand()));
    // —— ll 区 ——
#if !defined(FASTIO_PAIR_STEPS_LL) || FASTIO_PAIR_STEPS_LL >= 9
    g.emit_ll(LLONG_MAX);             // 19 位
  #ifndef FASTIO_ASSUME_UNSIGNED
    g.emit_ll(LLONG_MIN);
  #endif
#endif
    for (int i = 0; i < 20000; ++i) g.emit_ll(rnd_ll(nextrand()));
    // —— ull 区 ——
#if !defined(FASTIO_PAIR_STEPS_LL) || FASTIO_PAIR_STEPS_LL >= 10
    g.emit_ull(18446744073709551615ULL);   // 20 位
#endif
    // —— i128 区 ——（文本顺序必须与 check_reader 一致：先全部 i128，再 u128）
#if defined(__SIZEOF_INT128__)
  #if !defined(FASTIO_PAIR_STEPS_I128)
    g.emit_i128_str("170141183460469231731687303715884105727");    // i128 max，39 位
    #ifndef FASTIO_ASSUME_UNSIGNED
    g.emit_i128_str("-170141183460469231731687303715884105728");   // i128 min
    #endif
  #endif
    for (int i = 0; i < 20000; ++i) g.emit_i128(rnd_i128());
  #if !defined(FASTIO_PAIR_STEPS_I128)
    g.emit_u128_str("340282366920938463463374607431768211455");    // u128 max，39 位
  #endif
#endif
    g.text += '\n';
    return g;
}

// ---- 8 个手写解析层交叉比对 -------------------------------------------------
template <class R>
static void check_reader(R& r, const Gen& g, const char* tag) {
    for (size_t i = 0; i < g.iv.size(); ++i) assert(r.template read<int>() == g.iv[i]);
    for (size_t i = 0; i < g.lv.size(); ++i) assert(r.template read<long long>() == g.lv[i]);
    for (size_t i = 0; i < g.uv.size(); ++i)
        assert(r.template read<unsigned long long>() == g.uv[i]);
#if defined(__SIZEOF_INT128__)
    for (size_t i = 0; i < g.xv.size(); ++i) assert(r.template read<__int128>() == g.xv[i]);
    for (size_t i = 0; i < g.xuv.size(); ++i)
        assert(r.template read<__uint128_t>() == g.xuv[i]);
#endif
    std::printf("  read  [%-24s] OK\n", tag);
}

// ---- 写侧：6 个 Writer 逐字节比对 -------------------------------------------
static void check_writers(const Gen& g) {
    const int K = 20000;
    std::string want;
    char tmp[48];
    for (int i = 0; i < K; ++i) {
        std::snprintf(tmp, sizeof tmp, "%d ", g.iv[size_t(i)]);
        want += tmp;
    }
#if defined(__SIZEOF_INT128__)
    for (int i = 0; i < 200; ++i) {   // 顺带验证 i128 输出（主库 + fread 档）
        i128_to_str(g.xv[size_t(i)], tmp);
        want += tmp;
        want += ' ';
    }
#endif
    want += '\n';

    struct Case { const char* tag; const char* path; };
    const Case cases[] = {
        {"fread/fwrite", "/tmp/fastio_opt_w5.out"},
        {"streambuf",    "/tmp/fastio_opt_w6.out"},
        {"fwrite 不打表", "/tmp/fastio_opt_w8.out"},
        {"putchar",      "/tmp/fastio_opt_w3.out"},
        {"putchar_unlocked", "/tmp/fastio_opt_w4.out"},
        {"★ 主库",       "/tmp/fastio_opt_wm.out"},
    };
    auto emit = [&](auto& w) {
        for (int i = 0; i < K; ++i) w << g.iv[size_t(i)] << ' ';
#if defined(__SIZEOF_INT128__)
        for (int i = 0; i < 200; ++i) w << g.xv[size_t(i)] << ' ';
#endif
        w << '\n';
    };
    { fio_fread::Writer w; assert(w.open(cases[0].path)); emit(w); }
    {
        std::ofstream fo(cases[1].path, std::ios::binary);
        fio_sbuf::Writer w(fo.rdbuf());
        emit(w);                            // 析构时 flush 到仍存活的 ofstream
    }
    { fio_fwrite::Writer w; assert(w.open(cases[2].path)); emit(w); }
    { fio_getchar::Writer w; assert(w.open(cases[3].path)); emit(w); }
    { fio_gcu::Writer w; assert(w.open(cases[4].path)); emit(w); }
    { fastio::Writer w; assert(w.open(cases[5].path)); emit(w); }
    for (const auto& c : cases) {
        std::string got = read_text(c.path);
        if (got != want) {
            std::printf("!! write [%s] MISMATCH\n", c.tag);
            std::exit(1);
        }
        std::printf("  write [%-20s] OK\n", c.tag);
    }
}

int main() {
    std::printf(
        "config: NO_EOF=%d ASSUME_UNSIGNED=%d STEPS_INT=%d STEPS_LL=%d STEPS_I128=%d"
        " REPLACE_CIN_COUT=%d INPUT_MAX=%lld OUTPUT_MAX=%lld\n",
#ifdef FASTIO_NO_EOF_CHECK
        1,
#else
        0,
#endif
#ifdef FASTIO_ASSUME_UNSIGNED
        1,
#else
        0,
#endif
        FASTIO_PAIR_STEPS_INT,
#ifdef FASTIO_PAIR_STEPS_LL
        FASTIO_PAIR_STEPS_LL,
#else
        -1,   // 自动：signed 9 / unsigned 10
#endif
#ifdef FASTIO_PAIR_STEPS_I128
        FASTIO_PAIR_STEPS_I128,
#else
        -1,   // 自动：19
#endif
#ifdef FASTIO_REPLACE_CIN_COUT
        1,
#else
        0,
#endif
#ifdef FASTIO_INPUT_MAX
        (long long)FASTIO_INPUT_MAX,
#else
        -1LL,
#endif
#ifdef FASTIO_OUTPUT_MAX
        (long long)FASTIO_OUTPUT_MAX
#else
        -1LL
#endif
    );

    Gen g = gen_data();
    { std::ofstream(IN, std::ios::binary) << g.text; }
    // FASTIO_INPUT_MAX 按承诺上限一次读，数据必须装得下
#ifdef FASTIO_INPUT_MAX
    assert(g.text.size() <= size_t(FASTIO_INPUT_MAX));
#endif

    { fio_mmap::Reader r;        assert(r.open(IN));    check_reader(r, g, "9 mmap+打表"); }
    { fio_ultra::UltraReader r;  assert(r.load_file(IN)); check_reader(r, g, "10 UltraReader"); }
    { fio_fread::Reader r;       assert(r.open(IN));    check_reader(r, g, "5 fread"); }
    {
        std::ifstream fin(IN, std::ios::binary);
        fio_sbuf::Reader r(fin.rdbuf());
        check_reader(r, g, "6 streambuf");
    }
    { fio_mmap_byte::Reader r;   assert(r.open(IN));    check_reader(r, g, "7 mmap 单字节"); }
    { fio_getchar::Reader r;     assert(r.open(IN));    check_reader(r, g, "3 getchar"); }
    { fio_gcu::Reader r;         assert(r.open(IN));    check_reader(r, g, "4 getchar_unlocked"); }
    { fastio::Reader r;          assert(r.open(IN));    check_reader(r, g, "★ 主库 fastio.hpp"); }

#ifdef FASTIO_NO_EOF_CHECK
    {   // 回归：NO_EOF_CHECK 下管道（流式）也必须读对 —— refill 属于窗口正确性，不删
        char cmd[128];
        std::snprintf(cmd, sizeof cmd, "cat %s", IN);
        FILE* pp = popen(cmd, "r");
        assert(pp);
        fastio::Reader r(pp);
        check_reader(r, g, "★ 主库 NO_EOF/管道");
        pclose(pp);
    }
#endif

#ifdef FASTIO_INPUT_MAX
    {   // 激进模式：管道也被一次性整读（直到 EOF）
        char cmd[128];
        std::snprintf(cmd, sizeof cmd, "cat %s", IN);
        FILE* pp = popen(cmd, "r");
        assert(pp);
        fastio::Reader r(pp);
        check_reader(r, g, "★ 主库 INPUT_MAX/管道");
        pclose(pp);
    }
#endif

    check_writers(g);

#ifdef FASTIO_REPLACE_CIN_COUT
    {   // 选项：cin / cout / endl 直接顶替 iostream（单数字，任意步数配置都合法）
        { std::ofstream(IN2, std::ios::binary) << "4 7 1"; }
        FILE* fp = std::fopen(IN2, "rb");
        assert(fp);
        fastio::io.bind(fp, stdout);                  // 全局 io 换绑到测试文件
        assert(fastio::io.open_out(OUT2));
        int a, b, c;
        cin >> a >> b >> c;                           // 看着是 iostream，走的是 fastio
        assert(a == 4 && b == 7 && c == 1);
        cout << a + b + c << endl;                    // 12\n
        cout << a << ' ' << b << ' ' << c << endl;    // 4 7 1\n
        cout << "hello " << std::string("world") << endl;
        fastio::io.flush();
        assert(read_text(OUT2) == "12\n4 7 1\nhello world\n");
        std::printf("  cin/cout/endl 替换             OK\n");
    }
#endif

    std::printf("options: ALL OK\n");
    return 0;
}
