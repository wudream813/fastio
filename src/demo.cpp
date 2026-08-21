// 全接口演示：跑 ./demo < demo.in 看效果
//   g++ -O2 -std=c++17 -o demo src/demo.cpp
#include <string>
#include <vector>

#include "../include/fastio.hpp"

int main() {
    // 1) 基础：>> 读，<< 写
    int n;
    io >> n;

    // 2) 变参一次读多个
    long long a, b;
    io.read(a, b);

    // 3) 数组批量读
    std::vector<int> v(static_cast<size_t>(n));
    io.read_n(v.data(), size_t(n));

    // 4) 字符串 / 字符 / 浮点
    std::string word;
    char ch;
    double d;
    io >> word >> ch >> d;

    // 5) 整行
    std::string line;
    io.readln(line);               // 吃掉上一行残余
    io.readln(line);

    // 6) 输出：print 拼接、println 空格分隔+换行、write_n 数组
    io.print("n=", n, " a+b=", a + b, '\n');
    io.println(word, ch, d);
    io.write_n(v.data(), size_t(n));          // 空格分隔，末尾换行
    io << "line=[" << line << "]\n";

    // 7) 浮点精度
    io.set_precision(3);
    io << 3.14159265 << '\n';

    // 8) 读到文件尾
    long long tail = 0;
    while (!io.eof()) tail += io.read<long long>();
    io << "tail_sum=" << tail << '\n';

    io.flush();                    // 可省略：析构会自动 flush
    return 0;
}
