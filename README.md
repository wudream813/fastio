# fastio —— 超级快读快写（单头文件，开箱即用）

> 读：**mmap 整文件映射 + 双字节打表** ｜ 写：**fwrite 大缓冲 + 四位打表**
> 100 MiB 实测：读 **69 ms（7.4× cin）**、写 **132 ms（3.25× cout）**

只需要一个文件：[`include/fastio.hpp`](include/fastio.hpp)。复制到你的项目/提交目录里即可，无依赖，C++17。

---

## 1. 30 秒上手

```cpp
#include "fastio.hpp"

int main() {
    int n; io >> n;
    long long s = 0;
    for (int i = 0; i < n; i++) { int x; io >> x; s += x; }
    io << s << '\n';        // 程序结束自动 flush
}
```

```bash
g++ -O2 -std=c++17 main.cpp -o main
./main < big_input.txt
```

- 输入是**重定向的文件** → 自动走 mmap + 双字节打表（最快档）。
- 输入是**管道 / 终端 / 交互题** → 自动降级成 `read()` 流式分块，**接口完全一样**，不会卡死。
- 单文件提交（洛谷等 OJ）：把 `fastio.hpp` 的内容直接粘到代码顶部即可。

---

## 2. API 速查

| 用途 | 写法 |
|---|---|
| 读单个 | `io >> x;` / `int x = io.read<int>();` |
| 读多个 | `io.read(a, b, c);` |
| 读数组（**最快**） | `io.read_n(arr, n);` |
| 读字符串/字符 | `std::string s; io >> s;` / `char c; io >> c;`（跳空白） |
| 读整行 | `io.readln(line);`（不含换行，自动去 `\r`） |
| 读浮点 | `double d; io >> d;` |
| 判结束 | `while (!io.eof()) { ... }` |
| 原始字节 | `io.getch()` / `io.peek()` |
| 写单个 | `io << x;` / `io.write(x);` |
| 写一行 | `io.writeln(x);` |
| 拼接输出 | `io.print("ans=", x, '\n');` |
| 空格分隔+换行 | `io.println(a, b, c);` |
| 写数组 | `io.write_n(arr, n, ' ');` |
| 浮点精度 | `io.set_precision(9);` |
| 手动刷新 | `io.flush();`（**交互题每轮必须**） |

类型支持：所有整型（含 `unsigned long long`、`__int128` 除外）、`bool`、`char`、`char*`、`std::string`、`float/double`。
**边界安全**：`0`、`INT_MIN`、`LLONG_MIN`、`ULLONG_MAX`、`+` 前缀、`\r\n` 全部特判过。

### 需要读写文件而不是标准流

```cpp
fastio::Reader fin;  fin.open("in.txt");    // mmap
fastio::Writer fout; fout.open("out.txt");  // fwrite 缓冲
int n = fin.read<int>();
fout << n << '\n';                          // 析构自动 flush

fastio::FastIO fio(stdin, stdout);          // 读写合体对象，也可 fio.bind(fp, fo)
```

### 可选编译宏

| 宏 | 作用 |
|---|---|
| `FASTIO_STREAM` | 强制流式读（交互题最保险，不把输入读进内存） |
| `FASTIO_NO_MMAP` | 禁用 mmap（Windows 下自动禁用） |
| `FASTIO_OBUF_BITS` | 输出缓冲位数，默认 22（4 MiB） |
| `FASTIO_IBUF_BITS` | 流式输入缓冲位数，默认 20（1 MiB） |

---

## 3. 四种写法，四个独立头文件

除了「自动选最快档」的主库 `fastio.hpp`，每种写法也各自封装成一个**可单独使用**的头文件，
互不依赖、命名空间独立，可以同时 include 做对照：

| 头文件 | 写法 | 核心 | 100 MiB 读 | 100 MiB 写 | 适用 |
|---|---|---|---:|---:|---|
| [`fastio_mmap.hpp`](include/fastio_mmap.hpp) | A · mmap + 双字节打表 | 文1 §4.2.2 的极简手写循环 | **69.7 ms（8.2×）** | — | 输入是重定向文件、卡常题 |
| [`fastio_ultra.hpp`](include/fastio_ultra.hpp) | B · UltraReader | 同样 mmap+打表，补齐管道回退/字符串/整行/批量 | **70.7 ms（8.1×）** | — | 想要 A 的速度 + 好用的接口 |
| [`fastio_fread.hpp`](include/fastio_fread.hpp) | C · fread / fwrite | 1 MiB 读缓冲；4 MiB 写缓冲 + 四位打表 | 147.2 ms（3.9×） | **172.4 ms（2.7×）** | 最通用：管道、终端、Windows |
| [`fastio_streambuf.hpp`](include/fastio_streambuf.hpp) | D · streambuf | `sgetn` / `sputn` 直接操作 `rdbuf()` | 142.7 ms（4.0×） | **164.4 ms（2.8×）** | 纯 STL，不用 mmap/stdio |
| [`fastio.hpp`](include/fastio.hpp) | 主库 | 文件→mmap+打表，管道/终端→流式；写 fwrite+四位打表 | 74.5 ms（7.7×） | 168.0 ms（2.8×） | 默认就用这个 |

（同机同数据，`std::cin` 573.8 ms / `std::cout` 463.8 ms 作基线，正负号随机；复现 `make benchvar`）

各自的最小用法：

```cpp
#include "fastio_mmap.hpp"        // A
int n = fio_mmap::in.read<int>();          // in 已绑定 stdin（须重定向文件）
fio_mmap::Reader r; r.open("in.txt");      // 或自己开文件

#include "fastio_ultra.hpp"       // B
int n = fio_ultra::in.read<int>();
fio_ultra::in.read_n(a, n);                // 批量最快
std::string s; fio_ultra::in >> s;

#include "fastio_fread.hpp"       // C
int n = fio_fread::in.read<int>();
fio_fread::out << n << '\n';               // 析构自动 flush

#include "fastio_streambuf.hpp"   // D
int n = fio_sbuf::in.read<int>();
fio_sbuf::out << n << '\n';
```

四者接口刻意保持一致（`read<T>()` / `>>` / `read_n` / `eof()`，写侧 `<<` / `write_n` / `flush`），
换一行 include 就能换实现，方便你在自己机器上做对照实验。
自检：`make test` 会跑 `variants_test`，四种读法在 20 万随机数上逐个交叉比对，
并覆盖 `0` / `INT_MIN` / `LLONG_MIN` / `ULLONG_MAX` / `+` 前缀。

---

## 4. 性能（本机实测）

环境：2 核 Intel Xeon @2.60 GHz / 1.9 GiB RAM / g++ 14.2 `-O2`，100 MiB 输入，3 轮取中位数。

### 库 vs 标准流（9 位整数 × 998 万 ≈ 100 MiB）

| 档位 | 耗时 | 加速比 |
|---|---:|---:|
| 读 · `std::cin` / `ifstream` | 511.1 ms | 1.00× |
| **读 · fastio `io >> x`（mmap + 双字节打表）** | **68.9 ms** | **7.4×** |
| **读 · fastio `io.read_n(arr, n)`** | **70.7 ms** | **7.2×** |
| 写 · `std::cout` / `ofstream` | 427.8 ms | 1.00× |
| **写 · fastio `io << x`（fwrite + 四位打表）** | **131.7 ms** | **3.25×** |

复现：`make benchlib`

### 全档位对照（同一份 100 MiB 数据，`report.html`）

| 读入档位 | 9 位稠密 | 混合位数 |
|---|---:|---:|
| `cin` 默认 | 574.6 ms | 1094.5 ms |
| `cin` 关同步 + untie | 569.9 ms | 880.6 ms |
| `scanf` | 717.1 ms | 1112.3 ms |
| `getchar` 手写 | 331.8 ms | 474.8 ms |
| `getchar_unlocked` | 267.6 ms | 378.1 ms |
| `fread` 缓冲快读 | 137.5 ms | 258.0 ms |
| `streambuf::sgetn` | 135.7 ms | 257.4 ms |
| mmap 单字节 | 137.1 ms | 276.6 ms |
| mmap + 双字节打表（手写） | 83.0 ms | 233.7 ms |
| **本库 Reader** | **73.6 ms** | 247.2 ms |

| 输出档位 | 9 位稠密 | 混合位数 |
|---|---:|---:|
| `cout` 默认 | 439.7 ms | 731.1 ms |
| `printf` | 582.3 ms | 926.6 ms |
| `putchar` 手写 | 452.7 ms | 572.5 ms |
| `putchar_unlocked` | 304.6 ms | 370.2 ms |
| `fwrite` 缓冲 | 232.5 ms | 338.2 ms |
| fwrite + 四位打表（手写） | 154.6 ms | 362.0 ms |
| **本库 Writer** | **134.0 ms** | **283.5 ms** |

复现：`make bench`（生成 `report.md` / `report.html` / `results.json`）

> 库比文章里的手写版还快一点，靠的是两处额外优化：
> **① 无分支符号处理**（`neg` 用掩码 `v = (v ^ m) - m` 取负），随机正负数据上比
> `if (*p=='-')` 快 30%+，因为负号分支几乎必然预测失败；
> **② 映射尾部挂哨兵页**，热循环彻底不判边界。

## 5. 原理（为什么快）

**读**
1. `mmap` 把整份输入一次性映射进地址空间：没有 `read()` 系统调用往返，没有用户态缓冲拷贝，缺页由内核批量补（带 `MAP_POPULATE` 预取）。
2. **双字节打表**：`int32_t tbl[65536]`，用相邻两个字节拼成的 `uint16` 直接索引出这两位数字的值（非数字对为 `-1`）。一次吃 2 个字符，`v = v*100 + tbl[w]`，把「逐字符判断 + 乘 10」的分支砍掉一半。
3. 循环按 5（32 位）/ 10（64 位）次**顺序展开**、不用 `break`：某一步失配后，后续步骤必然也失配，让 CPU 分支预测器一路跑到底。
4. 映射尾部额外挂一整页**可写匿名页并填哨兵 `0xFF`**（`> ' '`、非数字、非负号），
   所以跳空白和吃数字的热循环里**完全不需要边界检查**，也绝不会越界读。
5. **符号无分支**：`neg = (*p == '-')`，指针 `+= neg`，最后用掩码 `v = (v ^ m) - m` 取负。
   正负随机的数据里，`if (*p == '-')` 这一跳几乎必错，去掉它能再省 30%。
6. `read_n(arr, n)` 把游标留在寄存器里连续解析整个数组，比逐个 `>>` 少一次
   「存回成员 → 再取出」的存储转发延迟。

**写**
1. 4 MiB 输出缓冲，末尾一次 `fwrite` 落盘。
2. **四位打表**：`uint32_t tbl[10000]`，把 `0000..9999` 预存成 4 个 ASCII 字节打包的 `uint32`。转十进制时从低位起每次取 `x % 10000` 写 4 字节（一条 32 位 store），除法次数减到 1/4。
3. 最高一组去前导零；负数在**无符号域**取负（`U(0) - U(x)`），`INT_MIN` 不会溢出。

---

## 6. 注意事项（踩过的坑）

- **交互题**：要么每次输出后 `io.flush()`，要么直接 `-DFASTIO_STREAM`；千万别对交互题用整文件 mmap 的思路。
- **别混用**：用了 `io` 就不要再用 `scanf` / `cin` 读同一个流，缓冲互相看不见对方消费的字节。
- **mmap 只对常规文件生效**，管道/终端会自动降级，不用你操心。
- 输出如果被 `exit()` / 崩溃打断，缓冲区可能没落盘；析构会 flush，但 `_Exit()` 不会。
- 负号只在有符号类型下识别；读 `unsigned` 时 `-1` 不做特殊处理。

---

## 7. 目录结构

```
fastio/
├── include/
│   ├── fastio.hpp             ← 主库（自动选最快档，唯一需要的文件）
│   ├── fastio_mmap.hpp        ← A · mmap + 双字节打表
│   ├── fastio_ultra.hpp       ← B · UltraReader
│   ├── fastio_fread.hpp       ← C · fread / fwrite + 四位打表
│   └── fastio_streambuf.hpp   ← D · streambuf sgetn / sputn
├── src/example.cpp        ← 最小示例（读 n 个数求和）
├── src/demo.cpp           ← 全接口演示
├── src/correctness.cpp    ← 主库正确性自检（mmap/流式/边界/往返 20 万随机数）
├── src/variants_test.cpp  ← 四种独立写法交叉自检
├── src/variants_bench.cpp ← 四种独立写法横向对比
├── src/bench_lib.cpp      ← 主库 vs cin/cout（100 MiB）
├── src/benchmark.cpp      ← 11 档读 + 8 档写全对照，出 HTML 报告
├── Makefile               ← make test / make benchlib / make bench
└── report.html            ← 全档位性能报告
```

```bash
make test        # 正确性（主库 + 四种写法交叉比对）
make benchlib    # 100MiB 主库 vs 标准流
make benchvar    # 100MiB 四种写法横向对比
make bench       # 全档位对照 + HTML 报告
```
