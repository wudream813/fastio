// 四种独立写法的正确性自检：mmap打表 / UltraReader / fread / streambuf
//   g++ -O2 -std=c++17 -Iinclude -o variants_test src/variants_test.cpp
#include <cassert>
#include <climits>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../include/fastio_fread.hpp"
#include "../include/fastio_mmap.hpp"
#include "../include/fastio_streambuf.hpp"
#include "../include/fastio_ultra.hpp"

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

    {   // A: mmap + 双字节打表
        fio_mmap::Reader r;
        assert(r.open(IN));
        check_read(r, "A mmap+双字节打表");
    }
    {   // B: UltraReader（文件 -> mmap）
        fio_ultra::UltraReader r;
        assert(r.load_file(IN));
        assert(r.mode() == fio_ultra::UltraReader::MAPPED);
        check_read(r, "B UltraReader/mmap");
    }
    {   // B: UltraReader（管道 -> 整读回退）
        FILE* pp = popen("cat /tmp/fastio_var.in", "r");
        fio_ultra::UltraReader r(pp);
        assert(r.mode() == fio_ultra::UltraReader::SLURPED);
        check_read(r, "B UltraReader/管道");
        pclose(pp);
    }
    {   // C: fread
        fio_fread::Reader r;
        assert(r.open(IN));
        check_read(r, "C fread");
    }
    {   // C: fread 走管道
        FILE* pp = popen("cat /tmp/fastio_var.in", "r");
        fio_fread::Reader r(pp);
        check_read(r, "C fread/管道");
        pclose(pp);
    }
    {   // D: streambuf
        std::ifstream fin(IN, std::ios::binary);
        fio_sbuf::Reader r(fin.rdbuf());
        check_read(r, "D streambuf");
    }

    {   // 写：C fread + 四位打表
        const char* out = "/tmp/fastio_var_c.out";
        fio_fread::Writer w;
        assert(w.open(out));
        check_write(w, "C fwrite+四位打表", out);
    }
    {   // 写：D streambuf
        const char* out = "/tmp/fastio_var_d.out";
        std::ofstream fout(out, std::ios::binary);
        fio_sbuf::Writer w(fout.rdbuf());
        check_write(w, "D streambuf+四位打表", out);
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
        std::printf("  cross-check 200k × 4 写法 OK\n");
    }

    std::printf("variants: OK\n");
    return 0;
}
