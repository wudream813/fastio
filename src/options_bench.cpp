// 减分支选项的收益对照（默认 100 MiB，9 位正整数）：
//   make benchopt
//     -> ./options_bench       默认配置
//     -> ./options_bench_fast  -DFASTIO_NO_EOF_CHECK -DFASTIO_ASSUME_UNSIGNED
//                              -DFASTIO_PAIR_STEPS_INT=4 -DFASTIO_PAIR_STEPS_LL=9
// 两份二进制跑的是同一批数据（全正、9 位 = 4 个双字节 + 1 个单字节），对结果 checksum 一致。
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

static const char* IN = "/tmp/fastio_opt_bench_in.txt";
static const char* OUT = "/tmp/fastio_opt_bench_out.txt";

int main(int argc, char** argv) {
    size_t mb = argc > 1 ? size_t(std::atoi(argv[1])) : 100;
    int n = int(mb * 1024 * 1024 / 10);  // 9 位 + 空格 ≈ 10 B/个

#ifdef FASTIO_NO_EOF_CHECK
    std::printf("config = FAST  (NO_EOF_CHECK + ASSUME_UNSIGNED + STEPS_INT=%d STEPS_LL=%d)\n",
                FASTIO_PAIR_STEPS_INT, FASTIO_PAIR_STEPS_LL);
#else
    std::printf("config = 默认  (steps 自动：int 5 / ll 9 / ull 10 / i128 19)\n");
#endif
    std::printf("生成 %d 个 9 位正整数（约 %zu MiB）…\n\n", n, mb);

    std::vector<int> a((size_t)n);
    {
        std::mt19937 rng(0xC0FFEEu);
        std::uniform_int_distribution<int> d(100000000, 999999999);
        fio_fread::Writer w;
        w.open(IN);
        w << n << '\n';
        for (int i = 0; i < n; ++i) {
            a[size_t(i)] = d(rng);
            w << a[size_t(i)] << (i % 10 == 9 ? '\n' : ' ');
        }
        w << '\n';
    }
    long long want = 0;
    for (size_t i = 0; i < a.size(); ++i) want += a[i];

    long long chk = 0;
    std::vector<int> buf((size_t)n);
    auto R = [&](const char* name, double t) {
        if (chk != want) { std::printf("!! %s checksum 错误\n", name); std::exit(1); }
        std::printf("  读  %-30s %8.2f ms\n", name, t);
    };

    R("mmap + 双字节打表 read_n", med([&] {
        fio_mmap::Reader r;
        r.open(IN);
        int m = r.read<int>();
        r.read_n(buf.data(), size_t(m));
        long long s = 0;
        for (int i = 0; i < m; ++i) s += buf[size_t(i)];
        chk = s;
    }));
    R("UltraReader read_n", med([&] {
        fio_ultra::UltraReader r;
        r.load_file(IN);
        int m = r.read<int>();
        r.read_n(buf.data(), size_t(m));
        long long s = 0;
        for (int i = 0; i < m; ++i) s += buf[size_t(i)];
        chk = s;
    }));
    R("fread 缓冲 逐个 read", med([&] {
        fio_fread::Reader r;
        r.open(IN);
        int m = r.read<int>();
        long long s = 0;
        for (int i = 0; i < m; ++i) s += r.read<int>();
        chk = s;
    }));
    R("streambuf 逐个 read", med([&] {
        std::ifstream fin(IN, std::ios::binary);
        fio_sbuf::Reader r(fin.rdbuf());
        int m = r.read<int>();
        long long s = 0;
        for (int i = 0; i < m; ++i) s += r.read<int>();
        chk = s;
    }));
    R("★ 主库 read_n", med([&] {
        fastio::Reader r;
        r.open(IN);
        int m = r.read<int>();
        r.read_n(buf.data(), size_t(m));
        long long s = 0;
        for (int i = 0; i < m; ++i) s += buf[size_t(i)];
        chk = s;
    }));

    {
        double t = med([&] {
            fastio::Writer w;
            w.open(OUT);
            for (int i = 0; i < n; ++i) w << a[size_t(i)] << (i % 10 == 9 ? '\n' : ' ');
            w << '\n';
        });
        std::printf("  写  ★ 主库 fwrite + 四位打表     %8.2f ms\n", t);
    }
    return 0;
}
