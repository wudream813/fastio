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
| 免跳字节 | `io.skip(k);`（配合 `NO_WS_SKIP` 手吞定宽分隔符） |
| 无检查写字符 | `io.put_nochk(c);`（承诺缓冲未满；紧跟 `write(x)` 后恒安全——写整数已 `reserve(48)`） |
| 融合写一行 | `io.write_nl(x);` = `write(x)` + `put_nochk('\n')`，冲榜配方专用（v1.2.0+） |

类型支持：所有整型——`int / long long / unsigned / unsigned long long`，
以及 GNU 扩展的 `__int128 / unsigned __int128`（主库和各手写解析层都支持）；
`bool`、`char`、`char*`、`std::string`、`float/double`。
**边界安全**：`0`、`INT_MIN`、`LLONG_MIN`、`ULLONG_MAX`、`__int128` 的 39 位满位值、`+` 前缀、`\r\n` 全部特判过。

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

减分支 / 顶替 iostream / 激进定制 的选项（`FASTIO_NO_EOF_CHECK`、`FASTIO_ASSUME_UNSIGNED`、
`FASTIO_PAIR_STEPS_INT/_LL/_I128`、`FASTIO_REPLACE_CIN_COUT`、`FASTIO_INPUT_MAX`、
`FASTIO_OUTPUT_MAX`）见下面 **§4 编译期选项**。

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

**⚠️ 每个选项都是你对数据的一份承诺；承诺不成立 = 结果错误（激进选项甚至会崩）。**
同一个 `#define` 对后面包含的所有手写解析层（主库 + 各独立头）同时生效。

### 减分支选项

| 宏 | 你的承诺 | 被删掉的分支 | 作用于 |
|---|---|---|---|
| `FASTIO_NO_EOF_CHECK` | 数据完整规范、热路径读不到流末尾 | `getch/peek/read<char>` 的边界判断、`c != EOF` 比较（流式 refill **永远保留**，管道也安全） | 主库、fread、streambuf、getchar(±unlocked) |
| `FASTIO_ASSUME_UNSIGNED` | 输入没有负号（也没有 `+`） | 读侧符号判断、写侧 `x < 0` 判断 | **全部**手写解析层 |
| `FASTIO_PAIR_STEPS_INT=n` | ≤32 位整型最多 `2n+1` 位十进制 | 多余的双字节查表展开 | 主库、mmap、ultra |
| `FASTIO_PAIR_STEPS_LL=n` | 64 位整型最多 `2n+1` 位十进制 | 同上 | 同上 |
| `FASTIO_PAIR_STEPS_I128=n` | `__int128` 最多 `2n+1` 位十进制 | 同上 | 同上 |
| `FASTIO_NO_WS_SKIP` | 格式精确：数前无空白（首字符是数字或 `-`，无符号类型必为数字、无 `+`），每个数后恰好 1 字节分隔符（空格或 `\n`，`\r\n` 不行），最后一个数后也有 | 每个数的空白扫描循环、符号多判一次、分隔符由解析顺带吃掉（`read_n` / `>>` 用法不变）；配合 `io.skip(k)` 手吞定宽分隔符 | 主库 |
| `FASTIO_REPLACE_CIN_COUT` | — | 全局 `cin/cout/endl` 顶替 iostream | 主库 |
| `FASTIO_SWAR8` | 64 位数据普遍很长（如满量程 u64，~96% 有 17~20 位十进制） | 解析每 16 位的 8 步串行 `v=v*100+w`，换 8B 载入 + SWAR 折叠（Mula）——仅 64 位解析生效；8 位左右的 int 数据别开（~10% 失配惩罚反而慢） | 主库 |
| `FASTIO_NO_WRITE_SSE` | 极老/非 x86 平台 | 关掉写侧四位组的 SSE 单条 16B `movups`，回退 2×8B 拼字写（v1.2.0 起默认开） | 主库 |

双字节步数**默认就按类型精确展开**，一般不需要动：

| 类型 | 最长十进制位数 | 双字节展开 |
|---|---:|---:|
| `int8 / uint8` | 3 | 1 对 + 末尾单字节 |
| `short / unsigned short` | 5 | 2 对 + 1 单 |
| **`int / unsigned int`** | **10** | **5 对** |
| **`long long`** | **19** | **9 对 + 1 单** |
| **`unsigned long long`** | **20** | **10 对** |
| **`__int128 / unsigned __int128`** | **39** | **19 对 + 1 单** |

只有当你**知道数据位数更小**（比如坐标 ≤ 6 位）时才覆盖调小，省几次失配查表。

> mmap / ultra / mmap_byte 三个头靠尾部哨兵本来就零 EOF 判断，
> `FASTIO_NO_EOF_CHECK` 对它们没有可删的分支，为统一开关清单仍接受该宏。

### 激进选项（最后一次系统调用都省掉）

| 宏 | 你的承诺 | 效果 | 违约后果 |
|---|---|---|---|
| `FASTIO_INPUT_MAX=n` | stdin 总量 ≤ n **字节** | attach 时一口气整读到 EOF（连 mmap 都不用），之后零系统调用、零 refill | **超出部分直接丢弃**；口径含管道——所以**交互题绝对禁用**（会一直阻塞等 EOF） |
| `FASTIO_OUTPUT_MAX=n` | 总输出 ≤ n **字节** | 缓冲一次配足，整个程序只在结束时 `fwrite` 一次；`put/reserve` 的边界判断全删 | **堆损坏/段错误**（stdlib 不会拦你） |

```cpp
#define FASTIO_INPUT_MAX  (4 << 20)    // 输入 ≤ 4 MiB：一次读入
#define FASTIO_OUTPUT_MAX (1 << 20)    // 输出 ≤ 1 MiB：一次写出
#include "fastio.hpp"
```

适合数据规模写死在自己手里的大文件批处理场景（离线评测、数据生成器）。

> ⚠ **大输出别用 `FASTIO_OUTPUT_MAX`**：实测 24.5 MB 输出（120 万行），一次配足
> 32 MiB 末尾一次写出 57 ms，而默认流式冲刷（4 MiB）49 ms、`OBUF_BITS=20`(1 MiB)
> 48 ms——几十 MB 的冷缓冲分页 + 一次性写反而更慢。`OUTPUT_MAX` 只适合输出很小
> （比如一个数），真正图的是删掉每次 put 的边界判断。v1.0.x 曾把它推荐给
> U539374 一类大输出题，实测反伤 ~15%，特此更正。

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
| mmap + 双字节打表 | 73.5 ms | **41.4 ms（-44%）** |
| UltraReader | 72.0 ms | **42.2 ms（-41%）** |
| ★ 主库 | 69.8 ms | **41.8 ms（-40%）** |

`STEPS_INT=4` 恰好覆盖 9 位数字（4 双字节 + 1 单字节），每次少一次失配查表，
加上符号判断消失，双字节打表系稳定快 40% 上下（v1.2.0 把整条读写链强制内联
后收益进一步放大）；fread / streambuf 逐字符档与写侧基本无感（瓶颈不在这里）。
共享机器 ±15% 噪声，方向稳定，以你本机实测为准。

### 极限格式精确（v1.1.0+，939 MiB / 1 亿个带符号 int 实测）

`FASTIO_NO_WS_SKIP` 把每个数的"找空白、判符号、留分隔符"三件小事全省，
同机同数据（含 `-O2`，块读 `read_n`）：

| 配置 | min | 相对 |
|---|---:|---|
| v1.0.1 最优组合（STEPS4 等） | 701 ms | 1.00× |
| 默认 + STEPS4（v1.1.0） | 670 ms | -4% |
| + `FASTIO_NO_WS_SKIP` | **601 ms** | **-14%** |
| + 融合读（直接 `s += io.read<int>()`）+ `-O3`（v1.1.1） | 587 ms | -16% |
| 同配方 v1.2.0（内联链修复后） | **575 ms** | **-18%** |
| 43ms 记录的 mmap 模板（同轮参考） | 591 ms | -16% |

u64 异或对（48.9 MiB 输入 / 24.5 MiB 输出)：v1.0.1 推荐配置 61 ms →
`ASSUME_UNSIGNED + NO_WS_SKIP + OBUF_BITS=20`（v1.1.1）48.4 ms（-21%）→
同配方 v1.2.0 45.9 ms（-25%）→ 同配方 v1.3.0 追加 `FASTIO_SWAR8`
**45.2 ms（-26%）**，模板同轮 45.5 ms——**min/med 均已反超**。

### 冲榜配方（v1.2.0+；评测机只给 -O2 时 `#pragma GCC optimize("O3")` 补上）

> v1.2.0 针对那套 mmap 模板做了三件"取其精华"的改良（详见本节末）：**整条读写
> 链强制内联**、**输出缓冲改模块级定长数组 + 下标游标**（寄存器常驻）、
> **写四位组改单条 16B `movups`**。下方实测均为同机同轮交替计时。

读多写少（P10815 型，带符号 int 求和，939 MiB/1e8 数实测）：

```cpp
#pragma GCC optimize("O3")
#define FASTIO_NO_EOF_CHECK
#define FASTIO_NO_WS_SKIP        // 题面数据：数前无空白、单字节分隔符
#define FASTIO_PAIR_STEPS_INT 4  // |a_i| ≤ 1e8 → 最长 9 位，4 对+1 单恰好
#include "fastio.hpp"
static int b[1 << 16];
int main() {
    int n = io.read<int>();
    long long s = 0;
    for (int i = 0; i < n; ++i) s += io.read<int>();   // 融合读：-O3 下实测超 mmap 模板
    io << s << '\n';
}
```

实测 min/med（v1.2.0，8 轮交替）：**575/591 ms vs 模板 591/610 ms（胜 ~3%）**；
纯 -O2 无 pragma 时改用 `read_n` 分块，以自己机器实测为准。

读写各半（U539374 型，u64 满量程、逐行输出）：

```cpp
#pragma GCC optimize("O3")
#define FASTIO_ASSUME_UNSIGNED     // 0 <= A,B < 2^64
#define FASTIO_NO_EOF_CHECK
#define FASTIO_NO_WS_SKIP          // "T\nA B\n..." 全是单字节分隔
#define FASTIO_SWAR8               // 满量程 u64（~96% 有 17~20 位）：16 位 SWAR 折叠读
#define FASTIO_OBUF_BITS 20        // 大输出 1 MiB 热缓冲流式冲刷——别用大 OUTPUT_MAX！
#include "fastio.hpp"
static unsigned long long b[1 << 13];
int main() {
    unsigned long long t; io >> t;
    for (unsigned long long rem = t * 2; rem > 0;) {
        size_t k = rem < (1 << 13) ? (size_t)rem : (1 << 13);
        io.read_n(b, k);
        for (size_t i = 0; i < k; i += 2) io.write_nl(b[i] ^ b[i + 1]);  // 融合写一行
        rem -= k;
    }
}
```

实测 min/med（v1.3.0，12 轮交替）：**46.0/46.3 ms vs 模板 46.2/47.0 ms——反超**
（SWAR8 单项消融：同轮 45.2/45.8 vs 不开的 47.1/48.1，约 -5%；共享机噪声
±1.5 ms，min 值已稳定低于模板。多交几次碰好机器，纪录可期）。

**"指令集还能不能再挤"——评估结论（诚实版）**：
- 写侧 SSE2 单条 16B `movups` 已到位（v1.2.0）；AVX2 对 u64→十进制的核心瓶颈
  （5 条 64 位乘）没有对应宽指令，AVX-512DQ 才有 `vpmullq`——评测机没有，弃。
- 读侧 BMI2 `pext` 变体估算只省 ~2 uop，却引入"评测机必须支持 BMI2"的运行时
  风险（老评测机直接 SIGILL），收益低于噪声——弃。
- 最终胜出的是 **SWAR8**：纯 u64 标量魔法数，哪个评测机都合法，才集成了。

v1.2.0 溯源笔记（反汇编 + 逐件消融实测）：

- 模板真正快的不是任何"写法"，而是**存储形态**：全局定长数组 + 全局下标游标。
  GCC 因此能把游标钉在寄存器里；换成"类成员指针游标"后，`char` 写在 C++ 语义下
  可能别名游标本体，每写一个数都要重载/写回游标 —— 这单独就值 **20%+**。
- 同一原因，v1.1.x 的 `read()/write_uns()` 体大被 GCC 拒绝内联，热点循环里每次
  读写都是一次真实 `call` + 状态 spill。v1.2.0 给整条热链加 `always_inline`，
  纯写探针 16.8 → 13.6 ms（**-19%**），FAST 档 `read_n` 55 → 42 ms（**-24%**）。
- 写四位组不再等 GCC 的 store-merging 施舍（同样的源码在普通函数里能合并成
  `movups`，从类成员内联出来就不触发），x86 下用 SSE intrinsic 显式单条 16B 写；
  不想用 SSE 定义 `FASTIO_NO_WRITE_SSE` 即退回 2×8B 拼字写。
- 模板的"逐级取余除法链"和"16B 现场拼字节"两种写法在本机实测都不赢，已弃。

---

## 5. 性能（本机实测）

环境：2 核 Intel Xeon @2.60 GHz / 1.9 GiB RAM / g++ 14.2 `-O2`，100 MiB 输入，3 轮取中位数。

### 库 vs 标准流（9 位整数 × 998 万 ≈ 100 MiB）

| 档位 | 耗时 | 加速比 |
|---|---:|---:|
| 读 · `std::cin` / `ifstream` | 489.6 ms | 1.00× |
| **读 · fastio `io >> x`（mmap + 双字节打表）** | **66.5 ms** | **7.4×** |
| **读 · fastio `io.read_n(arr, n)`** | **70.0 ms** | **7.0×** |
| 写 · `std::cout` / `ofstream` | 469.5 ms | 1.00× |
| **写 · fastio `io << x`（fwrite + 四位打表）** | **94.3 ms** | **5.0×** |

复现：`make benchlib`

### 全档位对照（同一份 100 MiB 数据，`report.html`）

| 读入档位 | 9 位稠密 | 混合位数 |
|---|---:|---:|
| `cin` 默认 | 541.7 ms | 1064.1 ms |
| `cin` 关同步 + untie | 541.6 ms | 880.2 ms |
| `scanf` | 688.4 ms | 1260.3 ms |
| `getchar` 手写 | 435.0 ms | 550.0 ms |
| `getchar_unlocked` | 151.8 ms | 297.2 ms |
| `fread` 缓冲快读 | 145.7 ms | 275.5 ms |
| `streambuf::sgetn` | 149.0 ms | 280.7 ms |
| mmap 单字节 | 148.6 ms | 294.4 ms |
| mmap + 双字节打表（手写） | 92.1 ms | 262.3 ms |
| **本库 Reader** | **65.3 ms** | **258.5 ms** |

| 输出档位 | 9 位稠密 | 混合位数 |
|---|---:|---:|
| `cout` 默认 | 463.2 ms | 823.6 ms |
| `printf` | 647.2 ms | 1006.8 ms |
| `putchar` 手写 | 502.6 ms | 628.0 ms |
| `putchar_unlocked` | 334.7 ms | 475.2 ms |
| `fwrite` 缓冲 | 219.6 ms | 360.9 ms |
| fwrite + 四位打表（手写） | 154.4 ms | 407.6 ms |
| **本库 Writer** | **123.6 ms** | **326.2 ms** |

复现：`make bench`（生成 `report.md` / `report.html` / `results.json`）

> 库比文章里的手写版还快一点，靠的是两处额外优化：
> **① 无分支符号处理**（`neg` 用掩码 `v = (v ^ m) - m` 取负），随机正负数据上比
> `if (*p=='-')` 快 30%+，因为负号分支几乎必然预测失败；
> **② 映射尾部挂哨兵页**，热循环彻底不判边界。

## 6. 原理（为什么快）

**读**
1. `mmap` 把整份输入一次性映射进地址空间：没有 `read()` 系统调用往返，没有用户态缓冲拷贝，缺页由内核批量补（带 `MAP_POPULATE` 预取）。
2. **双字节打表**：`uint8_t tbl[65536]`（v1.1.0 起值 +1 编码，0 = 非数字对），用相邻两个字节拼成的 `uint16` 直接索引出这两位数字的值。一次吃 2 个字符，`v = v*100 + tbl[w] - 1`，把「逐字符判断 + 乘 10」的分支砍掉一半。
3. 循环按 5（32 位）/ 10（64 位）次**顺序展开**、不用 `break`：某一步失配后，后续步骤必然也失配，让 CPU 分支预测器一路跑到底。
4. 映射尾部额外挂一整页**可写匿名页并填哨兵 `0xFF`**（`> ' '`、非数字、非负号），
   所以跳空白和吃数字的热循环里**完全不需要边界检查**，也绝不会越界读。
5. **符号无分支**：`neg = (*p == '-')`，指针 `+= neg`，最后用三元 `neg ? U(0)-v : v`
   取负（编译成 `neg` + `cmov`）。正负随机的数据里，`if (*p == '-')` 这一跳几乎必错，
   去掉它能再省 30%。
6. `read_n(arr, n)` 把游标留在寄存器里连续解析整个数组，比逐个 `>>` 少一次
   「存回成员 → 再取出」的存储转发延迟。
7. **整条热链 `always_inline`**（v1.2.0）：`read/read_n/write/write_uns` 体大，
   GCC 默认拒绝内联，热点循环里每次读写都是一次真实 `call` + 状态 spill，
   修复后纯读/纯写各快 20% 上下。
8. **`FASTIO_SWAR8`**（v1.3.0，仅 64 位解析生效）：8B 一次载入，SWAR 位运算
   验证"8 字节全是数字"（Mula 编码），命中则三条乘加魔法数把 8 个 ASCII
   直接折叠成值，替代 4 步串行 `v = v*100 + w` 依赖链；两段连用一次吃 16 位。
   剩余位数和失配情况自动落回双字节打表路径，行为完全等价。

**写**
1. 输出缓冲（默认 4 MiB）做成**模块级定长数组 + 模块级下标游标**（v1.2.0，mmap
   模板同款形态）：基址编译期已知、与游标分属不同存储对象，GCC 因此找得到
   「写进缓冲的字节不可能改动游标本体」，整个热点循环游标常驻寄存器；
   指针成员形态每写一个数都要重载/写回，实测慢 8%~25%。满了一次 `fwrite` 落盘。
2. **四位打表**：`uint32_t tbl[10000]`，把 `0000..9999` 预存成 4 个 ASCII 字节打包的
   `uint32`；五个四位组用**各自独立的常量除法并行**求出（无 `x /= 10000` 串行链），
   按最高非零组分支定长直写，除法次数减到 1/4。
3. x86 下四个四位组用 **SSE 拼成单条 16B `movups`** 一次落地（v1.2.0；
   不再指望 GCC 的 store-merging——同形态源码从类成员内联出来后它经常不干活）。
4. 最高一组去前导零；负数在**无符号域**取负（`U(0) - U(x)`），`INT_MIN` 不会溢出。

---

## 7. 注意事项（踩过的坑）

- **交互题**：要么每次输出后 `io.flush()`，要么直接 `-DFASTIO_STREAM`；千万别对交互题用整文件 mmap 的思路。**`FASTIO_INPUT_MAX` 尤其禁止**——它会一直读到 EOF 才往下走。
- **别混用**：用了 `io` 就不要再用 `scanf` / `cin` 读同一个流，缓冲互相看不见对方消费的字节。
- **激进选项的边界**：`FASTIO_INPUT_MAX` 超了是**静默丢数据**；`FASTIO_OUTPUT_MAX` 超了是**缓冲越界**（一般会立刻段错误，正好暴露假承诺）。只在数据规模可控时开。
- **mmap 只对常规文件生效**，管道/终端会自动降级，不用你操心。
- 输出如果被 `exit()` / 崩溃打断，缓冲区可能没落盘；析构会 flush，但 `_Exit()` 不会。
- 负号只在有符号类型下识别；读 `unsigned` 时 `-1` 不做特殊处理。
- **`__int128`**：主库和 mmap/ultra/fread/streambuf/getchar(±unlocked)/mmap_byte 档都支持读写；
  它是 GNU 扩展（MSVC 没有），且 `scanf/printf` 档与 iostream 档天生不认它。
  `-DNDEBUG` 一类 flag 不影响；严格 `-std=c++17` 下标准萃取不认这个类型，库里已自带兼容萃取。

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
├── src/options_test.cpp   ← 12 组编译期选项组合逐一自检（含 __int128 / 激进选项）
├── src/options_bench.cpp  ← 减分支选项收益对照（默认 vs 全选项）
├── src/bench_lib.cpp      ← 主库 vs cin/cout（100 MiB）
├── src/benchmark.cpp      ← 11 档读 + 8 档写全对照，出 HTML 报告
├── Makefile               ← make test / make benchlib / make bench
├── .github/workflows/ci.yml              ← 每次 push：编译 + 自检 + 8MiB 快测
├── .github/workflows/bench-release.yml   ← 打 v* tag：100MiB 全量基准 + 发 Release
└── report.html            ← 全档位性能报告
```

```bash
make test        # 正确性（主库 + 全部写法 + 12 组选项组合自检）
make benchlib    # 100MiB 主库 vs 标准流
make benchvar    # 100MiB 全部写法横向对比
make benchopt    # 100MiB 减分支选项收益对照
make bench       # 全档位对照 + HTML 报告

git tag v1.0.0 && git push origin v1.0.0   # 触发 CI 跑 100MiB 基准并发 Release
```
