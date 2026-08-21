// 全部独立写法的正确性自检：
//   cin / scanf / getchar / getchar_unlocked / fread / streambuf /
//   mmap单字节 / mmap+双字节打表 / UltraReader / fwrite / 主库
//   g++ -O2 -std=c++17 -Iinclude -o variants_test src/variants_test.cpp
#include <cassert>
#include <climits>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../include/fastio_all.hpp"

static const char* IN = "/tmp/fastio_var.in";

static void write_text(const char* path, const std::string& s) {
    std::ofstream(path, std::ios::binary) << s;
}
static std::string read_text(const char* path) {
    std::ifstream f(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

static const std::string TEXT =
    "8\n"
    "0 -2147483648 2147483647 -1 42\n"
    "+99 1000000000000 -9223372036854775808\n";

template <class R>
static void check_read(R& r, const char* tag) {
    assert(r.template read<int>() == 8);
    assert(r.template read<int>() == 0);
    assert(r.template read<int>() == INT_MIN);
    assert(r.template read<int>() == INT_MAX);
    assert(r.template read<int>() == -1);
    assert(r.template read<int>() == 42);
    assert(r.template read<int>() == 99);
    assert(r.template read<long long>() == 1000000000000LL);
    assert(r.template read<long long>() == LLONG_MIN);
    std::printf("  read  [%-22s] OK\n", tag);
}

template <class W>
static void check_write(W& w, const char* tag, const char* path) {
    w << 0 << ' ' << 9999 << ' ' << 10000 << ' ' << 123456789 << '\n';
    w << INT_MIN << ' ' << LLONG_MIN << ' ' << (unsigned long long)ULLONG_MAX << '\n';
    int a[3] = {7, -8, 9};
    w.write_n(a, 3);
    w.flush();
    std::string got = read_text(path);
    std::string want =
        "0 9999 10000 123456789\n"
        "-2147483648 -9223372036854775808 18446744073709551615\n"
        "7 -8 9\n";
    if (got != want) {
        std::printf("  write [%s] MISMATCH:\n%s", tag, got.c_str());
        std::exit(1);
    }
    std::printf("  write [%-22s] OK\n", tag);
}

int main() {
    write_text(IN, TEXT);

    {   // 9: mmap + 双字节打表
        fio_mmap::Reader r;
        assert(r.open(IN));
        check_read(r, "9 mmap+双字节打表");
    }
    {   // 10: UltraReader（文件 -> mmap）
        fio_ultra::UltraReader r;
        assert(r.load_file(IN));
        assert(r.mode() == fio_ultra::UltraReader::MAPPED);
        check_read(r, "10 UltraReader/mmap");
    }
    {   // 10: UltraReader（管道 -> 整读回退）
        FILE* pp = popen("cat /tmp/fastio_var.in", "r");
        fio_ultra::UltraReader r(pp);
        assert(r.mode() == fio_ultra::UltraReader::SLURPED);
        check_read(r, "10 UltraReader/管道");
        pclose(pp);
    }
    {   // 5: fread
        fio_fread::Reader r;
        assert(r.open(IN));
        check_read(r, "5 fread");
    }
    {   // 5: fread 走管道
        FILE* pp = popen("cat /tmp/fastio_var.in", "r");
        fio_fread::Reader r(pp);
        check_read(r, "5 fread/管道");
        pclose(pp);
    }
    {   // 6: streambuf
        std::ifstream fin(IN, std::ios::binary);
        fio_sbuf::Reader r(fin.rdbuf());
        check_read(r, "6 streambuf");
    }
    {   // 1: cin 关同步
        std::ifstream fin(IN);
        fio_cin::Reader r(fin);
        check_read(r, "1 cin 关同步");
    }
    {   // 2: scanf
        fio_scanf::Reader r;
        assert(r.open(IN));
        check_read(r, "2 scanf");
    }
    {   // 3: getchar
        fio_getchar::Reader r;
        assert(r.open(IN));
        check_read(r, "3 getchar");
    }
    {   // 4: getchar_unlocked
        fio_gcu::Reader r;
        assert(r.open(IN));
        check_read(r, "4 getchar_unlocked");
    }
    {   // 7: mmap 单字节
        fio_mmap_byte::Reader r;
        assert(r.open(IN));
        check_read(r, "7 mmap 单字节");
    }
    {   // 主库
        fastio::Reader r;
        assert(r.open(IN));
        check_read(r, "★ 主库 fastio.hpp");
    }

    {   // 写：5 fread + 四位打表
        const char* out = "/tmp/fastio_var_c.out";
        fio_fread::Writer w;
        assert(w.open(out));
        check_write(w, "5 fwrite+四位打表", out);
    }
    {   // 写：6 streambuf
        const char* out = "/tmp/fastio_var_d.out";
        std::ofstream fout(out, std::ios::binary);
        fio_sbuf::Writer w(fout.rdbuf());
        check_write(w, "6 streambuf+四位打表", out);
    }
    {   // 写：2 printf
        const char* out = "/tmp/fastio_var_2.out";
        fio_scanf::Writer w;
        assert(w.open(out));
        check_write(w, "2 printf", out);
    }
    {   // 写：3 putchar
        const char* out = "/tmp/fastio_var_3.out";
        fio_getchar::Writer w;
        assert(w.open(out));
        check_write(w, "3 putchar", out);
    }
    {   // 写：4 putchar_unlocked
        const char* out = "/tmp/fastio_var_4.out";
        fio_gcu::Writer w;
        assert(w.open(out));
        check_write(w, "4 putchar_unlocked", out);
    }
    {   // 写：8 fwrite 缓冲（不打表）
        const char* out = "/tmp/fastio_var_8.out";
        fio_fwrite::Writer w;
        assert(w.open(out));
        check_write(w, "8 fwrite 缓冲", out);
    }
    {   // 写：1 cout 关同步
        const char* out = "/tmp/fastio_var_1.out";
        std::ofstream fo(out);
        fio_cin::Writer w(fo);
        check_write(w, "1 cout 关同步", out);
    }
    {   // 写：★ 主库
        const char* out = "/tmp/fastio_var_m.out";
        fastio::Writer w;
        assert(w.open(out));
        check_write(w, "★ 主库 fastio.hpp", out);
    }

    {   // 大批量往返：20 万随机数，四种读法结果必须一致
        std::vector<long long> v;
        unsigned long long s = 88172645463325252ULL;
        for (int i = 0; i < 200000; ++i) {
            s ^= s << 13; s ^= s >> 7; s ^= s << 17;
            v.push_back(i % 3 ? (long long)(int)s : (long long)s);
        }
        const char* path = "/tmp/fastio_var_big.txt";
        {
            fio_fread::Writer w;
            w.open(path);
            for (size_t i = 0; i < v.size(); ++i) w << v[i] << (i % 8 == 7 ? '\n' : ' ');
            w << '\n';
        }
        fio_mmap::Reader ra;   ra.open(path);
        fio_ultra::UltraReader rb; rb.load_file(path);
        fio_fread::Reader rc;  rc.open(path);
        std::ifstream fin(path, std::ios::binary);
        fio_sbuf::Reader rd(fin.rdbuf());
        for (size_t i = 0; i < v.size(); ++i) {
            long long x = v[i];
            assert(ra.read<long long>() == x);
            assert(rb.read<long long>() == x);
            assert(rc.read<long long>() == x);
            assert(rd.read<long long>() == x);
        }
        std::printf("  cross-check 200k × 4 主力读法 OK\n");
    }

    std::printf("variants: ALL OK\n");
    return 0;
}
