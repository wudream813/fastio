// 正确性自检：mmap 档 / 流式档 / 边界值 / 字符串 / 浮点 / EOF 循环
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "../include/fastio.hpp"

static void write_text(const char* path, const std::string& s) {
    std::ofstream(path, std::ios::binary) << s;
}

static std::string read_text(const char* path) {
    std::ifstream f(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

int main() {
    const char* in = "/tmp/fastio_corr.in";
    const char* out = "/tmp/fastio_corr.out";

    std::string text =
        "7\n"
        "0 -2147483648 2147483647\n"
        "   -1 42\r\n"
        "+99\n"
        "1000000000000 -9223372036854775808 18446744073709551615\n"
        "hello  world\n"
        "3.5 -0.125\n"
        "tail line with spaces\n";
    write_text(in, text);

    auto check_reader = [&](fastio::Reader& r, const char* tag) {
        assert(r.read<int>() == 7);
        int a, b, c, d, e, f;
        r.read(a, b, c, d, e, f);
        assert(a == 0 && b == INT_MIN && c == INT_MAX);
        assert(d == -1 && e == 42 && f == 99);
        assert(r.read<long long>() == 1000000000000LL);
        assert(r.read<long long>() == LLONG_MIN);
        assert(r.read<unsigned long long>() == ULLONG_MAX);
        std::string s1, s2;
        r.read(s1, s2);
        assert(s1 == "hello" && s2 == "world");
        double x = r.read<double>(), y = r.read<double>();
        assert(x == 3.5 && y == -0.125);
        std::string line;
        r.readln(line);                    // 吃掉浮点行剩余的换行
        assert(r.readln(line));
        assert(line == "tail line with spaces");
        assert(r.eof());
        std::printf("  [%s] mode=%d OK\n", tag, int(r.mode()));
    };

    {   // ---- 档 1：mmap 整文件
        fastio::Reader r;
        assert(r.open(in));
        assert(r.mode() == fastio::Reader::MAPPED || r.mode() == fastio::Reader::SLURPED);
        check_reader(r, "mmap/file");
    }
    {   // ---- 档 2：管道 -> 流式
        FILE* pp = popen("cat /tmp/fastio_corr.in", "r");
        assert(pp);
        fastio::Reader r(pp);
        assert(r.mode() == fastio::Reader::STREAM);
        check_reader(r, "stream/pipe");
        pclose(pp);
    }
    {   // ---- 档 3：小缓冲流式（强制多次 refill）：逐个数字校验
        FILE* pp = popen("seq 1 100000", "r");
        assert(pp);
        fastio::Reader r(pp);
        long long sum = 0;
        while (!r.eof()) sum += r.read<int>();
        assert(sum == 100000LL * 100001 / 2);
        pclose(pp);
        std::printf("  [stream/seq] OK\n");
    }

    {   // ---- 写：四位打表正确性 + 边界
        fastio::Writer w;
        assert(w.open(out));
        w << 0 << ' ' << 7 << ' ' << 42 << ' ' << 999 << ' ' << 1000 << ' '
          << 9999 << ' ' << 10000 << '\n';
        w << INT_MIN << ' ' << INT_MAX << ' ' << LLONG_MIN << ' ' << LLONG_MAX
          << ' ' << ULLONG_MAX << '\n';
        w.println(1, 2, 3);
        int arr[5] = {5, -4, 3, -2, 1};
        w.write_n(arr, 5);
        w.print("pi=", 3.14159, '\n');
        w << std::string("done") << '\n';
        w.flush();
    }
    {
        std::string got = read_text(out);
        std::string want =
            "0 7 42 999 1000 9999 10000\n"
            "-2147483648 2147483647 -9223372036854775808 9223372036854775807 "
            "18446744073709551615\n"
            "1 2 3\n"
            "5 -4 3 -2 1\n"
            "pi=3.141590\n"
            "done\n";
        if (got != want) {
            std::printf("MISMATCH\n--- got ---\n%s--- want ---\n%s", got.c_str(),
                        want.c_str());
            return 1;
        }
        std::printf("  [writer] OK\n");
    }

    {   // ---- 随机往返：写出去再读回来必须一致
        std::vector<long long> v;
        unsigned long long s = 88172645463325252ULL;
        for (int i = 0; i < 200000; ++i) {
            s ^= s << 13; s ^= s >> 7; s ^= s << 17;
            long long x = (long long)s;
            if (i % 3 == 0) x = (long long)(int)s;
            v.push_back(x);
        }
        {
            fastio::Writer w;
            assert(w.open("/tmp/fastio_rt.txt"));
            for (size_t i = 0; i < v.size(); ++i) w << v[i] << (i % 10 == 9 ? '\n' : ' ');
            w << '\n';
        }
        fastio::Reader r;
        assert(r.open("/tmp/fastio_rt.txt"));
        for (size_t i = 0; i < v.size(); ++i) {
            long long got = r.read<long long>();
            assert(got == v[i]);
        }
        std::printf("  [roundtrip 200k] OK\n");
    }

    {   // ---- 兼容旧接口：FastIO::bind / UltraReader
        FILE* fp = std::fopen(in, "rb");
        FILE* fo = std::fopen("/tmp/fastio_corr2.out", "wb");
        fastio::FastIO fio(fp, fo);
        assert(fio.read<int>() == 7);
        fio << 123 << '\n';
        fio.flush();
        std::fclose(fp);
        std::fclose(fo);
        fastio::UltraReader ur;
        assert(ur.load_file(in));
        assert(ur.read<int>() == 7);
        std::printf("  [compat] OK\n");
    }

    std::printf("correctness: OK\n");
    return 0;
}
