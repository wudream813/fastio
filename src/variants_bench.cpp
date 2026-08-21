// 四种独立写法 + 主库 的横向对比（默认 100 MiB）
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

#include "../include/fastio.hpp"
#include "../include/fastio_fread.hpp"
#include "../include/fastio_mmap.hpp"
#include "../include/fastio_streambuf.hpp"
#include "../include/fastio_ultra.hpp"

using clk = std::chrono::steady_clock;
static double ms_since(clk::time_point t0) {
    return std::chrono::duration<double, std::milli>(clk::now() - t0).count();
}
template <class Fn>
static double med(Fn fn, int rounds = 3) {
    fn();
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

int main(int argc, char** argv) {
    size_t mb = argc > 1 ? size_t(std::atoi(argv[1])) : 100;
    int n = int(mb * 1024 * 1024 / 10.5);
    std::printf("生成 %d 个 9 位整数（约 %zu MiB）…\n\n", n, mb);

    std::vector<int> a(static_cast<size_t>(n));
    {
        std::mt19937 rng(0xC0FFEEu);
        std::uniform_int_distribution<int> d(100000000, 999999999);
        fio_fread::Writer w;
        w.open(IN);
        w << n << '\n';
        for (int i = 0; i < n; ++i) {
            int x = d(rng);
            if (rng() & 1) x = -x;
            a[size_t(i)] = x;
            w << x << (i % 10 == 9 ? '\n' : ' ');
        }
        w << '\n';
    }
    long long want = 0;
    for (size_t i = 0; i < a.size(); ++i) want += a[i];

    struct Row { const char* name; double t; };
    std::vector<Row> reads, writes;
    long long chk = 0;
    auto add_read = [&](const char* name, double t) {
        if (chk != want) { std::printf("!! %s checksum 错误\n", name); std::exit(1); }
        reads.push_back({name, t});
    };

    // ---------- 基线 ----------
    add_read("基线 · std::cin / ifstream", med([&] {
        std::ifstream f(IN);
        int m; f >> m;
        long long s = 0;
        for (int i = 0; i < m; ++i) { int x; f >> x; s += x; }
        chk = s;
    }));

    // ---------- A: fastio_mmap.hpp ----------
    add_read("A · mmap + 双字节打表", med([&] {
        fio_mmap::Reader r;
        r.open(IN);
        int m = r.read<int>();
        long long s = 0;
        for (int i = 0; i < m; ++i) s += r.read<int>();
        chk = s;
    }));

    // ---------- B: fastio_ultra.hpp ----------
    add_read("B · UltraReader（逐个）", med([&] {
        fio_ultra::UltraReader r;
        r.load_file(IN);
        int m = r.read<int>();
        long long s = 0;
        for (int i = 0; i < m; ++i) s += r.read<int>();
        chk = s;
    }));
    std::vector<int> tmp(static_cast<size_t>(n));
    add_read("B · UltraReader（read_n 批量）", med([&] {
        fio_ultra::UltraReader r;
        r.load_file(IN);
        int m = r.read<int>();
        r.read_n(tmp.data(), size_t(m));
        long long s = 0;
        for (int i = 0; i < m; ++i) s += tmp[size_t(i)];
        chk = s;
    }));

    // ---------- C: fastio_fread.hpp ----------
    add_read("C · fread 缓冲快读", med([&] {
        fio_fread::Reader r;
        r.open(IN);
        int m = r.read<int>();
        long long s = 0;
        for (int i = 0; i < m; ++i) s += r.read<int>();
        chk = s;
    }));

    // ---------- D: fastio_streambuf.hpp ----------
    add_read("D · streambuf::sgetn", med([&] {
        std::ifstream fin(IN, std::ios::binary);
        fio_sbuf::Reader r(fin.rdbuf());
        int m = r.read<int>();
        long long s = 0;
        for (int i = 0; i < m; ++i) s += r.read<int>();
        chk = s;
    }));

    // ---------- 主库 ----------
    add_read("主库 · fastio.hpp（自动选档）", med([&] {
        fastio::Reader r;
        r.open(IN);
        int m = r.read<int>();
        long long s = 0;
        for (int i = 0; i < m; ++i) s += r.read<int>();
        chk = s;
    }));

    // ============ 写 ============
    writes.push_back({"基线 · std::cout / ofstream", med([&] {
        std::ofstream f(OUT);
        for (int i = 0; i < n; ++i) f << a[size_t(i)] << (i % 10 == 9 ? '\n' : ' ');
    })});
    writes.push_back({"C · fwrite + 四位打表", med([&] {
        fio_fread::Writer w;
        w.open(OUT);
        for (int i = 0; i < n; ++i) w << a[size_t(i)] << (i % 10 == 9 ? '\n' : ' ');
    })});
    writes.push_back({"D · streambuf::sputn + 四位打表", med([&] {
        std::ofstream fo(OUT, std::ios::binary);
        fio_sbuf::Writer w(fo.rdbuf());
        for (int i = 0; i < n; ++i) w << a[size_t(i)] << (i % 10 == 9 ? '\n' : ' ');
        w.flush();
    })});
    writes.push_back({"主库 · fastio.hpp Writer", med([&] {
        fastio::Writer w;
        w.open(OUT);
        for (int i = 0; i < n; ++i) w << a[size_t(i)] << (i % 10 == 9 ? '\n' : ' ');
    })});

    double base_r = reads[0].t, base_w = writes[0].t;
    std::printf("%-36s %10s %9s\n", "读入", "耗时(ms)", "加速比");
    for (auto& r : reads)
        std::printf("%-36s %10.2f %8.2fx\n", r.name, r.t, base_r / r.t);
    std::printf("\n%-36s %10s %9s\n", "输出", "耗时(ms)", "加速比");
    for (auto& w : writes)
        std::printf("%-36s %10.2f %8.2fx\n", w.name, w.t, base_w / w.t);

    std::remove(IN);
    std::remove(OUT);
    return 0;
}
