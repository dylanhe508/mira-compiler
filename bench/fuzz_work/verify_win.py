# win/mira.exe 全用例 O0-O3 差分验证(复用 fuzz 判定逻辑)
import subprocess, os, glob, sys, time
MIRA = "/mnt/e/mira/mira/win/mira.exe"
cases = sorted(glob.glob("fuzz_*"))
cases = [c for c in cases if os.path.isdir(c) and os.path.isdir(os.path.join(c, "o0"))]
t0 = time.time()
passed = failed = 0
for case in cases:
    outs = {}
    for opt in (0, 1, 2, 3):
        od = os.path.join(case, "o%d" % opt)
        p = subprocess.run([MIRA, "-O%d" % opt, os.path.join("..", "case.mira")],
                           capture_output=True, text=True, timeout=30, cwd=od)
        if p.returncode != 0:
            print("[%s] O%d 编译失败: %s" % (case, opt, (p.stderr or p.stdout).strip()[:120]))
            failed += 1
            break
        exe = os.path.join(od, "case.exe")
        if not os.path.exists(exe):
            print("[%s] O%d 缺产物 case.exe" % (case, opt))
            failed += 1
            break
        r = subprocess.run(["./case.exe"], capture_output=True, text=True, timeout=15, cwd=od)
        outs[opt] = (r.returncode, r.stdout)
    else:
        sigs = {outs[o][0] for o in outs}
        outs_set = {outs[o][1] for o in outs}
        if len(sigs) != 1:
            failed += 1
            print("[%s] 崩溃差异: %s" % (case, {o: outs[o][0] for o in outs}))
        elif any(s != 0 for s in sigs):
            failed += 1
            print("[%s] 全级别崩溃: rc=%s" % (case, {o: outs[o][0] for o in outs}))
        elif len(outs_set) > 1:
            failed += 1
            print("[%s] 输出不一致!" % case)
            for o in (1, 2, 3):
                if outs[o][1] != outs[0][1]:
                    print("    O%d != O0: rc=%d out=%r" % (o, outs[o][0], outs[o][1][:100]))
        else:
            passed += 1
print("---- win 差分: %d 程序, %d pass / %d fail, %.1fs" % (len(cases), passed, failed, time.time() - t0))
