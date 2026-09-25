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

减分支 / 顶替 iostream 的选项（`FASTIO_NO_EOF_CHECK`、`FASTIO_ASSUME_UNSIGNED`、
`FASTIO_PAIR_STEPS_INT/_LL`、`FASTIO_REPLACE_CIN_COUT`）见下面 **§4 编译期选项**。

---

## 3. 每种写法一个独立头文件

文章里出现过的**每一档写法都单独成库**，互不依赖、命名空间独立，可以同时 include 做对照。
接口刻意保持一致：读 `read<T>()` / `>>` / `read(a,b,c)` / `read_n(arr,n)` / `eof()`，
写 `write(x)` / `<<` / `write_n(arr,n)` / `put(c)` / `flush()`，**换一行 include 就换实现**。

| # | 写法 | 头文件 | 命名空间 | 100 MiB 读 | 100 MiB 写 |
|---|---|---|---|---:|---:|
| — | 基线 `std::cin` / `std::cout` | — | — | 565 ms | 473 ms |
| 1 | cin/cout 关同步 + untie | `fastio_cin.hpp` | `fio_cin` | 578 ms（0.98×） | 451 ms（1.05×） |
| 2 | scanf / printf | `fastio_scanf.hpp` | `fio_scanf` | 746 ms（0.76×） | 560 ms（0.85×） |
| 3 | getchar / putchar 手写整型 | `fastio_getchar.hpp` | `fio_getchar` | 432 ms（1.3×） | 442 ms（1.07×） |
| 4 | getchar_unlocked / putchar_unlocked | `fastio_getchar_unlocked.hpp` | `fio_gcu` | 302 ms（1.9×） | 302 ms（1.6×） |
| 5 | fread 缓冲 / fwrite + 四位打表 | `fastio_fread.hpp` | `fio_fread` | 143 ms（3.9×） | **148 ms（3.2×）** |
| 6 | streambuf `sgetn` / `sputn` | `fastio_streambuf.hpp` | `fio_sbuf` | 148 ms（3.8×） | 154 ms（3.1×） |
| 7 | mmap 单字节 | `fastio_mmap_byte.hpp` | `fio_mmap_byte` | 107 ms（5.3×） | — |
| 8 | fwrite 缓冲（不打表） | `fastio_fwrite.hpp` | `fio_fwrite` | — | 251 ms（1.9×） |
| 9 | **mmap + 双字节打表** | `fastio_mmap.hpp` | `fio_mmap` | **66.5 ms（8.5×）** | — |
| 10 | **UltraReader**（9 + 管道回退/字符串/整行/批量） | `fastio_ultra.hpp` | `fio_ultra` | **66.5 ms（8.5×）** | — |
| ★ | 主库（自动选档，读写齐全） | `fastio.hpp` | `fastio` | 70.7 ms（8.0×） | 155 ms（3.05×） |
| — | 一次性引入全部写法 | `fastio_all.hpp` | — | — | — |

同机同数据：998 万个 9 位整数、正负随机，3 轮取中位数。复现：`make benchvar`。

各自的最小用法（都自带绑定 stdin/stdout 的全局对象 `in` / `out`）：

```cpp
#include "fastio_cin.hpp"              // 1
int n = fio_cin::in.read<int>();       fio_cin::out << n << '\n';

#include "fastio_scanf.hpp"            // 2
int n = fio_scanf::in.read<int>();     fio_scanf::out << n << '\n';

#include "fastio_getchar.hpp"          // 3
int n = fio_getchar::in.read<int>();   fio_getchar::out << n << '\n';

#include "fastio_getchar_unlocked.hpp" // 4（单线程专用）
int n = fio_gcu::in.read<int>();       fio_gcu::out << n << '\n';

#include "fastio_fread.hpp"            // 5（最通用：管道/终端/Windows 都行）
int n = fio_fread::in.read<int>();     fio_fread::out << n << '\n';

#include "fastio_streambuf.hpp"        // 6（纯 STL）
int n = fio_sbuf::in.read<int>();      fio_sbuf::out << n << '\n';

#include "fastio_mmap_byte.hpp"        // 7
int n = fio_mmap_byte::in.read<int>();

#include "fastio_fwrite.hpp"           // 8
fio_fwrite::out << x << '\n';

#include "fastio_mmap.hpp"             // 9（读最快，须重定向文件）
int n = fio_mmap::in.read<int>();

#include "fastio_ultra.hpp"            // 10（读最快 + 接口最全）
int n = fio_ultra::in.read<int>();     fio_ultra::in.read_n(a, n);

#include "fastio_all.hpp"              // 全都要（对照实验用）
```

自检：`make test` 里的 `variants_test` 会逐个验证 11 种读法 / 8 种写法，
覆盖 `0` / `INT_MIN` / `LLONG_MIN` / `ULLONG_MAX` / `+` 前缀 / 管道输入，
并让四种主力读法在 20 万随机数上交叉比对；
`options_test` 再用 6 组编译期选项组合，把 8 个手写解析层 + 6 个 Writer 全量互验。

---

## 4. 编译期选项（#define 后再 #include）

**⚠️ 每个选项都是你对数据的一份承诺；承诺不成立 = 结果错误。**
同一个 `#define` 对后面包含的所有手写解析层（主库 + 各独立头）同时生效。

| 宏 | 你的承诺 | 被删掉的分支 | 作用于 |
|---|---|---|---|
| `FASTIO_NO_EOF_CHECK` | 数据完整规范、热路径读不到 EOF | `getch/peek/read` 的边界判断、流式 refill | 主库、fread、streambuf、getchar(±unlocked) |
| `FASTIO_ASSUME_UNSIGNED` | 输入没有负号（也没有 `+`） | 读侧符号判断、写侧 `x < 0` 判断 | **全部**手写解析层 |
| `FASTIO_PAIR_STEPS_INT=n` | `int` 最多 `2n+1` 位十进制（默认 5） | 多余的双字节查表展开 | 主库、mmap、ultra |
| `FASTIO_PAIR_STEPS_LL=n` | `long long` 同上（默认 10） | 同上 | 同上 |
| `FASTIO_REPLACE_CIN_COUT` | — | 全局 `cin/cout/endl` 顶替 iostream | 主库 |

> mmap / ultra / mmap_byte 三个头靠尾部哨兵本来就零 EOF 判断，
> `FASTIO_NO_EOF_CHECK` 对它们没有可删的分支，为统一开关清单仍接受该宏。

### 例：竞赛数据（保证非负、读的是完整文件）

```cpp
#define FASTIO_ASSUME_UNSIGNED     // 没有负号（也没有 + 前缀）
#define FASTIO_NO_EOF_CHECK        // 输入是完整文件，不触 EOF
#define FASTIO_PAIR_STEPS_INT 3    // 知道数字 ≤ 7 位（3 双 + 1 单），再省两次查表
#include "fastio.hpp"
```

### 例：`cin` / `cout` 直接变成快读快写

```cpp
#define FASTIO_REPLACE_CIN_COUT
#include "fastio.hpp"

int main() {
    int n; cin >> n;                       // 全局 cin 就是 fastio::io
    long long s = 0, x;
    for (int i = 0; i < n; i++) { cin >> x; s += x; }
    cout << s << endl;                     // endl == '\n'，程序结束自动 flush
}
```

不用再 `#include <iostream>`——全局 `cin/cout/endl` 会盖住 `using namespace std`
里的那三个（`std::cin` 显式写依然可用，但千万别混用）。
iostream 的 `setw / fixed / tie` 之类花活没有，要格式化请 `printf`。

### 选项收益（100 MiB、全部 9 位正整数，`make benchopt`）

同一批数据，连开 `NO_EOF_CHECK + ASSUME_UNSIGNED + STEPS_INT=4 + STEPS_LL=9`：

| 读入档位（read_n） | 默认配置 | 全选项 |
|---|---:|---:|
| mmap + 双字节打表 | 70.3 ms | **60.6 ms（-14%）** |
| UltraReader | 63.2 ms | **53.4 ms（-16%）** |
| ★ 主库 | 60.3 ms | **51.0 ms（-15%）** |

`STEPS_INT=4` 恰好覆盖 9 位数字（4 双字节 + 1 单字节），每次少一次失配查表，
加上符号判断消失，双字节打表系稳定快 15% 上下；fread / streambuf 逐字符档与
写侧基本无感（瓶颈不在这里）。共享机器 ±15% 噪声，方向稳定，以你本机实测为准。

---

## 5. 性能（本机实测）

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

## 6. 原理（为什么快）

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

## 7. 注意事项（踩过的坑）

- **交互题**：要么每次输出后 `io.flush()`，要么直接 `-DFASTIO_STREAM`；千万别对交互题用整文件 mmap 的思路。
- **别混用**：用了 `io` 就不要再用 `scanf` / `cin` 读同一个流，缓冲互相看不见对方消费的字节。
- **mmap 只对常规文件生效**，管道/终端会自动降级，不用你操心。
- 输出如果被 `exit()` / 崩溃打断，缓冲区可能没落盘；析构会 flush，但 `_Exit()` 不会。
- 负号只在有符号类型下识别；读 `unsigned` 时 `-1` 不做特殊处理。

---

## 8. 目录结构

```
fastio/
├── include/
│   ├── fastio.hpp                  ← ★ 主库（自动选最快档，平时只要这个）
│   ├── fastio_all.hpp              ← 一次性引入全部写法
│   ├── fastio_cin.hpp              ← 1 · cin/cout 关同步
│   ├── fastio_scanf.hpp            ← 2 · scanf/printf
│   ├── fastio_getchar.hpp          ← 3 · getchar/putchar
│   ├── fastio_getchar_unlocked.hpp ← 4 · *_unlocked
│   ├── fastio_fread.hpp            ← 5 · fread / fwrite+四位打表
│   ├── fastio_streambuf.hpp        ← 6 · streambuf sgetn/sputn
│   ├── fastio_mmap_byte.hpp        ← 7 · mmap 单字节
│   ├── fastio_fwrite.hpp           ← 8 · fwrite 缓冲（不打表）
│   ├── fastio_mmap.hpp             ← 9 · mmap + 双字节打表
│   └── fastio_ultra.hpp            ← 10 · UltraReader
├── src/example.cpp        ← 最小示例（读 n 个数求和）
├── src/demo.cpp           ← 全接口演示
├── src/correctness.cpp    ← 主库正确性自检（mmap/流式/边界/往返 20 万随机数）
├── src/variants_test.cpp  ← 全部独立写法自检 + 交叉比对
├── src/variants_bench.cpp ← 全部独立写法横向对比
├── src/options_test.cpp   ← 6 组编译期选项组合逐一自检
├── src/options_bench.cpp  ← 减分支选项收益对照（默认 vs 全选项）
├── src/bench_lib.cpp      ← 主库 vs cin/cout（100 MiB）
├── src/benchmark.cpp      ← 11 档读 + 8 档写全对照，出 HTML 报告
├── Makefile               ← make test / make benchlib / make bench
└── report.html            ← 全档位性能报告
```

```bash
make test        # 正确性（主库 + 全部写法 + 6 组选项组合自检）
make benchlib    # 100MiB 主库 vs 标准流
make benchvar    # 100MiB 全部写法横向对比
make benchopt    # 100MiB 减分支选项收益对照
make bench       # 全档位对照 + HTML 报告
```
