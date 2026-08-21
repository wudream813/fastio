// 全部写法横向对比（默认 100 MiB）
//   g++ -O2 -std=c++17 -Iinclude -o variants_bench src/variants_bench.cpp
//   ./variants_bench [MiB]
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "../include/fastio_all.hpp"

using clk = std::chrono::steady_clock;
static double ms_since(clk::time_point t0) {
    return std::chrono::duration<double, std::milli>(clk::now() - t0).count();
}
template <class Fn>
static double med(Fn fn, int rounds = 3) {
    fn();  // 预热
    std::vector<double> t;
    for (int i = 0; i < rounds; ++i) {
        auto t0 = clk::now();
        fn();
        t.push_back(ms_since(t0));
    }
    std::sort(t.begin(), t.end());
    return t[t.size() / 2];
}

static const char* IN = "/tmp/fastio_var_in.txt";
static const char* OUT = "/tmp/fastio_var_out.txt";

static int g_n;
static long long g_want;
static long long g_chk;

struct Row { std::string name; std::string file; double t; };

// 通用读测试：给一个「打开 + 逐个读」的 lambda
template <class Fn>
static double time_read(Fn fn) {
    return med([&] { g_chk = fn(); });
}

int main(int argc, char** argv) {
    size_t mb = argc > 1 ? size_t(std::atoi(argv[1])) : 100;
    g_n = int(mb * 1024 * 1024 / 10.5);
    std::printf("生成 %d 个 9 位整数（约 %zu MiB，正负随机）…\n\n", g_n, mb);

    std::vector<int> a(static_cast<size_t>(g_n));
    {
        std::mt19937 rng(0xC0FFEEu);
        std::uniform_int_distribution<int> d(100000000, 999999999);
        fio_fread::Writer w;
        w.open(IN);
        w << g_n << '\n';
        for (int i = 0; i < g_n; ++i) {
            int x = d(rng);
            if (rng() & 1) x = -x;
            a[size_t(i)] = x;
            w << x << (i % 10 == 9 ? '\n' : ' ');
        }
        w << '\n';
    }
    g_want = 0;
    for (size_t i = 0; i < a.size(); ++i) g_want += a[i];

    std::vector<Row> reads, writes;
    auto R = [&](const char* name, const char* file, double t) {
        if (g_chk != g_want) { std::printf("!! %s checksum 错误\n", name); std::exit(1); }
        reads.push_back({name, file, t});
    };

    // ---------------------------- 读 ----------------------------
    R("基线 · std::cin（同步开启）", "-", time_read([] {
        std::ifstream f(IN);
        int m; f >> m;
        long long s = 0;
        for (int i = 0; i < m; ++i) { int x; f >> x; s += x; }
        return s;
    }));
    R("1 · cin 关同步 + untie", "fastio_cin.hpp", time_read([] {
        std::ifstream f(IN);
        fio_cin::Reader r(f);
        int m = r.read<int>();
        long long s = 0;
        for (int i = 0; i < m; ++i) s += r.read<int>();
        return s;
    }));
    R("2 · scanf", "fastio_scanf.hpp", time_read([] {
        fio_scanf::Reader r;
        r.open(IN);
        int m = r.read<int>();
        long long s = 0;
        for (int i = 0; i < m; ++i) s += r.read<int>();
        return s;
    }));
    R("3 · getchar 手写整型", "fastio_getchar.hpp", time_read([] {
        fio_getchar::Reader r;
        r.open(IN);
        int m = r.read<int>();
        long long s = 0;
        for (int i = 0; i < m; ++i) s += r.read<int>();
        return s;
    }));
    R("4 · getchar_unlocked", "fastio_getchar_unlocked.hpp", time_read([] {
        fio_gcu::Reader r;
        r.open(IN);
        int m = r.read<int>();
        long long s = 0;
        for (int i = 0; i < m; ++i) s += r.read<int>();
        return s;
    }));
    R("5 · fread 缓冲快读", "fastio_fread.hpp", time_read([] {
        fio_fread::Reader r;
        r.open(IN);
        int m = r.read<int>();
        long long s = 0;
        for (int i = 0; i < m; ++i) s += r.read<int>();
        return s;
    }));
    R("6 · streambuf::sgetn", "fastio_streambuf.hpp", time_read([] {
        std::ifstream fin(IN, std::ios::binary);
        fio_sbuf::Reader r(fin.rdbuf());
        int m = r.read<int>();
        long long s = 0;
        for (int i = 0; i < m; ++i) s += r.read<int>();
        return s;
    }));
    R("7 · mmap 单字节", "fastio_mmap_byte.hpp", time_read([] {
        fio_mmap_byte::Reader r;
        r.open(IN);
        int m = r.read<int>();
        long long s = 0;
        for (int i = 0; i < m; ++i) s += r.read<int>();
        return s;
    }));
    R("9 · mmap + 双字节打表", "fastio_mmap.hpp", time_read([] {
        fio_mmap::Reader r;
        r.open(IN);
        int m = r.read<int>();
        long long s = 0;
        for (int i = 0; i < m; ++i) s += r.read<int>();
        return s;
    }));
    R("10 · UltraReader（逐个）", "fastio_ultra.hpp", time_read([] {
        fio_ultra::UltraReader r;
        r.load_file(IN);
        int m = r.read<int>();
        long long s = 0;
        for (int i = 0; i < m; ++i) s += r.read<int>();
        return s;
    }));
    std::vector<int> tmp(static_cast<size_t>(g_n));
    R("10 · UltraReader（read_n 批量）", "fastio_ultra.hpp", time_read([&] {
        fio_ultra::UltraReader r;
        r.load_file(IN);
        int m = r.read<int>();
        r.read_n(tmp.data(), size_t(m));
        long long s = 0;
        for (int i = 0; i < m; ++i) s += tmp[size_t(i)];
        return s;
    }));
    R("★ 主库 fastio.hpp（自动选档）", "fastio.hpp", time_read([] {
        fastio::Reader r;
        r.open(IN);
        int m = r.read<int>();
        long long s = 0;
        for (int i = 0; i < m; ++i) s += r.read<int>();
        return s;
    }));

    // ---------------------------- 写 ----------------------------
    auto W = [&](const char* name, const char* file, double t) {
        writes.push_back({name, file, t});
    };
    W("基线 · std::cout（同步开启）", "-", med([&] {
        std::ofstream f(OUT);
        for (int i = 0; i < g_n; ++i) f << a[size_t(i)] << (i % 10 == 9 ? '\n' : ' ');
    }));
    W("1 · cout 关同步", "fastio_cin.hpp", med([&] {
        std::ofstream f(OUT);
        fio_cin::Writer w(f);
        for (int i = 0; i < g_n; ++i) w << a[size_t(i)] << (i % 10 == 9 ? '\n' : ' ');
        w.flush();
    }));
    W("2 · printf", "fastio_scanf.hpp", med([&] {
        fio_scanf::Writer w;
        w.open(OUT);
        for (int i = 0; i < g_n; ++i) w << a[size_t(i)] << (i % 10 == 9 ? '\n' : ' ');
    }));
    W("3 · putchar 手写整型", "fastio_getchar.hpp", med([&] {
        fio_getchar::Writer w;
        w.open(OUT);
        for (int i = 0; i < g_n; ++i) w << a[size_t(i)] << (i % 10 == 9 ? '\n' : ' ');
    }));
    W("4 · putchar_unlocked", "fastio_getchar_unlocked.hpp", med([&] {
        fio_gcu::Writer w;
        w.open(OUT);
        for (int i = 0; i < g_n; ++i) w << a[size_t(i)] << (i % 10 == 9 ? '\n' : ' ');
    }));
    W("8 · fwrite 缓冲（不打表）", "fastio_fwrite.hpp", med([&] {
        fio_fwrite::Writer w;
        w.open(OUT);
        for (int i = 0; i < g_n; ++i) w << a[size_t(i)] << (i % 10 == 9 ? '\n' : ' ');
    }));
    W("5 · fwrite + 四位打表", "fastio_fread.hpp", med([&] {
        fio_fread::Writer w;
        w.open(OUT);
        for (int i = 0; i < g_n; ++i) w << a[size_t(i)] << (i % 10 == 9 ? '\n' : ' ');
    }));
    W("6 · streambuf::sputn + 打表", "fastio_streambuf.hpp", med([&] {
        std::ofstream fo(OUT, std::ios::binary);
        fio_sbuf::Writer w(fo.rdbuf());
        for (int i = 0; i < g_n; ++i) w << a[size_t(i)] << (i % 10 == 9 ? '\n' : ' ');
        w.flush();
    }));
    W("★ 主库 fastio.hpp Writer", "fastio.hpp", med([&] {
        fastio::Writer w;
        w.open(OUT);
        for (int i = 0; i < g_n; ++i) w << a[size_t(i)] << (i % 10 == 9 ? '\n' : ' ');
    }));

    double br = reads[0].t, bw = writes[0].t;
    std::printf("%-34s %-30s %10s %9s\n", "读入", "头文件", "耗时(ms)", "加速比");
    for (auto& r : reads)
        std::printf("%-34s %-30s %10.2f %8.2fx\n", r.name.c_str(), r.file.c_str(), r.t, br / r.t);
    std::printf("\n%-34s %-30s %10s %9s\n", "输出", "头文件", "耗时(ms)", "加速比");
    for (auto& w : writes)
        std::printf("%-34s %-30s %10.2f %8.2fx\n", w.name.c_str(), w.file.c_str(), w.t, bw / w.t);

    std::remove(IN);
    std::remove(OUT);
    return 0;
}
