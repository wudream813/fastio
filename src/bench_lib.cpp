// 库速度验收：把 fastio 库 与 cin/cout 基线在同一份 100MiB 数据上对比
//   g++ -O2 -std=c++17 -Iinclude -o bench_lib src/bench_lib.cpp && ./bench_lib [MiB]
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "../include/fastio.hpp"

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

static const char* IN = "/tmp/fastio_lib_in.txt";
static const char* OUT = "/tmp/fastio_lib_out.txt";

int main(int argc, char** argv) {
    size_t target = (argc > 1 ? size_t(std::atoi(argv[1])) : 100) * 1024 * 1024;
    int n = int(target / 10.5);  // 9 位稠密约 10.5 B/数

    std::printf("生成 %d 个 9 位整数 (~%.2f MiB) ...\n", n, target / 1048576.0);
    std::vector<int> a(static_cast<size_t>(n));
    {
        std::mt19937 rng(0xC0FFEEu);
        std::uniform_int_distribution<int> d(100000000, 999999999);
        fastio::Writer w;
        w.open(IN);
        w << n << '\n';
        for (int i = 0; i < n; ++i) {
            a[size_t(i)] = d(rng);
            if (i % 7 == 3) a[size_t(i)] = -a[size_t(i)];
            w << a[size_t(i)] << (i % 10 == 9 ? '\n' : ' ');
        }
        w << '\n';
    }
    long long want = 0;
    for (int x : a) want += x;

    // ---------------- 读 ----------------
    long long chk = 0;
    double t_cin = med([&] {
        std::ifstream f(IN);
        int m;
        f >> m;
        long long s = 0;
        for (int i = 0; i < m; ++i) { int x; f >> x; s += x; }
        chk = s;
    });
    if (chk != want) { std::printf("cin checksum bad\n"); return 1; }

    double t_lib = med([&] {
        fastio::Reader r;
        r.open(IN);
        int m = r.read<int>();
        long long s = 0;
        for (int i = 0; i < m; ++i) s += r.read<int>();
        chk = s;
    });
    if (chk != want) { std::printf("lib checksum bad\n"); return 1; }

    std::vector<int> tmp(static_cast<size_t>(n));
    double t_libn = med([&] {
        fastio::Reader r;
        r.open(IN);
        int m = r.read<int>();
        r.read_n(tmp.data(), size_t(m));
        long long s = 0;
        for (int i = 0; i < m; ++i) s += tmp[size_t(i)];
        chk = s;
    });
    if (chk != want) { std::printf("lib read_n checksum bad\n"); return 1; }

    // ---------------- 写 ----------------
    double w_cout = med([&] {
        std::ofstream f(OUT);
        for (int i = 0; i < n; ++i) f << a[size_t(i)] << (i % 10 == 9 ? '\n' : ' ');
        f << '\n';
    });
    double w_lib = med([&] {
        fastio::Writer w;
        w.open(OUT);
        for (int i = 0; i < n; ++i) w << a[size_t(i)] << (i % 10 == 9 ? '\n' : ' ');
        w << '\n';
    });

    std::printf("\n%-34s %10s %8s\n", "档位", "耗时(ms)", "加速比");
    std::printf("%-34s %10.2f %8s\n", "读 · std::ifstream/cin", t_cin, "1.00x");
    std::printf("%-34s %10.2f %7.2fx\n", "读 · fastio (mmap+双字节打表)", t_lib, t_cin / t_lib);
    std::printf("%-34s %10.2f %7.2fx\n", "读 · fastio read_n 批量入数组", t_libn, t_cin / t_libn);
    std::printf("%-34s %10.2f %8s\n", "写 · std::ofstream/cout", w_cout, "1.00x");
    std::printf("%-34s %10.2f %7.2fx\n", "写 · fastio (fwrite+四位打表)", w_lib, w_cout / w_lib);
    std::remove(IN);
    std::remove(OUT);
    return 0;
}
