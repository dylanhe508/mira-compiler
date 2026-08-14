#!/usr/bin/env python3
# measure.py - mira vs gcc 同一批 40 程序 × O0-O3 编译+运行耗时对比
# 对拍: gcc O0 输出必须与 mira O0 输出逐字节一致(转换器正确性验证)
import os, sys, time, glob, shutil, subprocess

BASE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, BASE)
from mira2c import mira2c

MIRA = os.environ.get("MIRA", "/mnt/e/mira/mira/linux/mira")

def run(cmd, cwd, timeout=60):
    t0 = time.perf_counter()
    try:
        r = subprocess.run(cmd, cwd=cwd, capture_output=True, timeout=timeout)
        return time.perf_counter() - t0, r.returncode, r.stdout
    except subprocess.TimeoutExpired:
        return time.perf_counter() - t0, -1, b""

mc = mr = gc = gr = 0.0
mismatch = []
gcc_fail = []
cases = sorted(glob.glob(os.path.join(BASE, "fuzz_*")))
for case in cases:
    name = os.path.basename(case)
    src = open(os.path.join(case, "case.mira")).read()
    # ---- mira: 4 级编译+运行 ----
    mira_o0_out = None
    for opt in range(4):
        d = os.path.join(case, "m_%d" % opt)
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        t, rc, _ = run([MIRA, "-O%d" % opt, os.path.join(case, "case.mira")], d)
        mc += t
        if rc != 0:
            print("%s mira O%d 编译失败 rc=%d" % (name, opt, rc)); break
        t, rc, so = run(["./case"], d)
        mr += t
        if opt == 0:
            mira_o0_out = so
    # ---- gcc: 转 C 后 4 级编译+运行 ----
    csrc = mira2c(src)
    gcc_o0_out = None
    for opt in range(4):
        d = os.path.join(case, "g_%d" % opt)
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        with open(os.path.join(d, "case.c"), "w") as f:
            f.write(csrc)
        t, rc, _ = run(["gcc", "-O%d" % opt, "-w", "-o", "case", "case.c"], d)
        gc += t
        if rc != 0:
            gcc_fail.append(name)
            print("%s gcc O%d 编译失败 rc=%d" % (name, opt, rc)); break
        t, rc, so = run(["./case"], d)
        gr += t
        if opt == 0:
            gcc_o0_out = so
    if mira_o0_out is not None and gcc_o0_out is not None and mira_o0_out != gcc_o0_out:
        mismatch.append(name)

n = len(cases)
print("\n== 40 用例 × O0-O3 ==")
print("mira: 编译 %.2fs  运行 %.2fs  合计 %.2fs" % (mc, mr, mc + mr))
print("gcc : 编译 %.2fs  运行 %.2fs  合计 %.2fs" % (gc, gr, gc + gr))
print("mira 单用例×4级平均 %.1fms, gcc %.1fms" %
      ((mc + mr) / n * 1000, (gc + gr) / n * 1000))
print("O0 输出对拍: %d 不一致 %s" % (len(mismatch), mismatch or "无"))
print("gcc 编译失败: %s" % (gcc_fail or "无"))
