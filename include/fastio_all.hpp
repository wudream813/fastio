#pragma once
// ============================================================================
//  fastio_all.hpp  —  一次性引入全部写法，方便横向对照 / 一键切换
//
//  每种写法一个独立头文件、一个独立命名空间，接口刻意保持一致：
//      读：read<T>() / operator>> / read(a,b,c) / read_n(arr,n) / eof()
//      写：write(x) / operator<< / write_n(arr,n) / put(c) / flush()
//
//  ┌ 写法 ─────────────────── 头文件 ───────────────── 命名空间 ── 100MiB 读 ─ 100MiB 写 ┐
//   1 cin/cout 关同步         fastio_cin.hpp              fio_cin        578 ms    451 ms
//   2 scanf/printf            fastio_scanf.hpp            fio_scanf      746 ms    560 ms
//   3 getchar/putchar 手写    fastio_getchar.hpp          fio_getchar    432 ms    442 ms
//   4 *_unlocked              fastio_getchar_unlocked.hpp fio_gcu        302 ms    302 ms
//   5 fread / fwrite+四位打表 fastio_fread.hpp            fio_fread      143 ms    148 ms
//   6 streambuf sgetn/sputn   fastio_streambuf.hpp        fio_sbuf       148 ms    154 ms
//   7 mmap 单字节             fastio_mmap_byte.hpp        fio_mmap_byte  107 ms      —
//   8 fwrite 缓冲（不打表）   fastio_fwrite.hpp           fio_fwrite       —      251 ms
//   9 mmap + 双字节打表       fastio_mmap.hpp             fio_mmap        67 ms      —
//  10 UltraReader             fastio_ultra.hpp            fio_ultra       67 ms      —
//   ★ 主库（自动选档）        fastio.hpp                  fastio          71 ms    155 ms
//  └──────────────────────────────────────────────────────────────────────────────────┘
//  基线：std::cin 565 ms / std::cout 473 ms（同机同数据，998 万个 9 位整数，正负随机）
//
//  平时直接用主库 `io` 就行；这个聚合头主要给对照实验用（make benchvar）。
// ============================================================================

#include "fastio.hpp"

#include "fastio_cin.hpp"
#include "fastio_fread.hpp"
#include "fastio_fwrite.hpp"
#include "fastio_getchar.hpp"
#include "fastio_getchar_unlocked.hpp"
#include "fastio_scanf.hpp"
#include "fastio_streambuf.hpp"

#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
  #include "fastio_mmap.hpp"
  #include "fastio_mmap_byte.hpp"
  #include "fastio_ultra.hpp"
#endif
