// 最小使用示例：读 n 个数求和（洛谷 P10815 那类题的标准写法）
//   g++ -O2 -std=c++17 -o example src/example.cpp
//   ./example < input.txt
#include "../include/fastio.hpp"

int main() {
    int n;
    io >> n;                       // 等价 int n = io.read<int>();
    long long sum = 0;
    for (int i = 0; i < n; ++i) {
        int x;
        io >> x;
        sum += x;
    }
    io << sum << '\n';             // 程序退出时自动 flush
    return 0;
}
