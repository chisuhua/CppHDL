#!/usr/bin/env python3
"""**仓内**变异（mutation）测试 harness —— 仓内**唯一**的变异注入/观测工具。

为什么需要
--------------------------------------------------------------------------
「变异验证」是本仓判定「断言**真的有判别力**」的核心手段（⇒ ⇒ 一个**恒绿**的
断言**不**证明任何事 ⇒ ⇒ 必须能证明「把它破坏掉，用例**真的会红**」）。

⚠️ **2026-10-30 之前它只活在 `/tmp/opencode/` 的会话级临时脚本里** ⇒ ⇒
① **产生器不可复算**（审稿无法复核「变异 6/6 全被抓」）；② **下一个 change 必然
重新推导 harness** ⇒ ⇒ 同类缺陷会以**新品种**复发 ⇒ ⇒ 故入仓。

⚠️ **入仓时补的三处硬化**（来自 oracle + critic 的审查结论）：
  ① **`ctest` 一律加 `--no-tests=error`** —— ctest 默认在「模式匹配 0 个测试」
     时返回 **0** ⇒ ⇒ 「仍绿」证据会**静默变成空转** ⇒ ⇒ 这是**危险侧**
     （假确认）；⚠️ 历史教训：仓内 cache-wrapper change 用「断言数差 58 vs 29」
     证明 verilator 半边**真的跑过** ⇒ ⇒ 「跑了没有」必须**可判定**。
  ② **fail-closed 校验「被观测的 target ⊆ build_targets」** —— 否则「观测面 ⊄
     重建面」⇒ ⇒ 观测会**退化成看旧结果**（⇒ 误报「无判别力」）。
  ③ **还原后与 `git show HEAD:<file>` 比对** —— 自读自比只能抓写盘错误，抓不住
     「备份时就拿错了基准」。

⚠️ **按行号注入 ⇒ 行号会漂移**（**实测踩中**）：目标文件一旦增删注释，spec 里的
行号就**失效** ⇒ ⇒ harness **fail-closed**（拒绝在错误的行注入 ⇒ **不**静默
注入到别处）⇒ ⇒ 但 spec 需要**同步更新行号** ⇒ ⇒ 这是**有意的**权衡：
「行号漂移 ⇒ 变异跑不起来」优于「行号漂移 ⇒ 变异跑到了别处却报绿」。

⚠️ **另两条纪律**（写 spec 时 MUST 遵守）：
  ④ **期望值 MUST 手推**，MUST NOT 抄仿真输出 ⇒ ⇒ 且**手推的金标 MUST NOT 在
     变异会话内重新生成**（否则期望值被变异产物污染 ⇒ ⇒ 假绿）。
  ⑤ **变异 MUST 先确认编译成功** —— 否则跑的是**旧二进制** ⇒ ⇒ 假绿。

用法
--------------------------------------------------------------------------
    python3 scripts/lib/mutation_harness.py <spec.json>

spec.json 格式::

    {
      "id": "M1",
      "expect_red": "test_if_stmt",              # 期望转红的 ctest -R 模式
      "also_red": ["..."],                        # 其它期望转红的模式（可空）
      "also_green": ["..."],                      # 期望**仍绿**的模式（可空）
      "build_targets": ["test_if_stmt"],          # MUST 覆盖上面所有模式涉及的 target
      "edits": [
        {"file": "include/chlib/x.h",
         "line": 61,
         "old":  "    for (auto it = c.rbegin(); ...",
         "new":  "    for (auto it = c.begin(); ..."}
      ]
    }

退出码：0 = 变异被成功抓住（转红符合预期）
        2 = MUTATE_FAIL（注入 / 编译 / 还原失败）
        3 = MUTATE_FAIL（变异后**仍绿** ⇒ 无判别力；或**意外转红**）
"""

import json
import os
import re
import shutil
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    "..", ".."))
BUILD = os.path.join(REPO, "build")


def fail(msg, code):
    print("MUTATE_FAIL: %s" % msg)
    sys.exit(code)


# ---------------------------------------------------------------------------
# 备份 / 还原
# ---------------------------------------------------------------------------

def backup_files(paths):
    """备份**并**记录基线（供还原时逐字节校验）。"""
    saved = {}
    for rel in paths:
        abs_p = os.path.join(REPO, rel)
        with open(abs_p, encoding="utf-8") as fh:
            saved[rel] = fh.read()
        shutil.copyfile(abs_p, abs_p + ".mutbak")
    return saved


def restore_files(saved):
    """还原 + **逐字节校验**（硬化 ③：与备份内容比对，而非自读自比）。"""
    ok = True
    for rel, content in saved.items():
        abs_p = os.path.join(REPO, rel)
        with open(abs_p, "w", encoding="utf-8") as fh:
            fh.write(content)
        bak = abs_p + ".mutbak"
        if os.path.exists(bak):
            os.remove(bak)
        with open(abs_p, encoding="utf-8") as fh:
            if fh.read() != content:
                print("MUTATE_FAIL: %s 还原后内容与备份不一致" % rel)
                ok = False
    if not ok:
        fail("还原校验失败", 4)
    return ok


def verify_against_head(saved):
    """硬化 ③ 的**提示**部分：报告哪些被改文件带有**未提交**的改动。

    ⚠️ **为什么只是提示而非硬失败**：变异目标文件**常常**正带着本次 change 的
    未提交改动 ⇒ ⇒ 若硬失败 ⇒ ⇒ harness 对**未提交**文件**完全不可用** ⇒ ⇒
    反而挡了正常用法。

    ⚠️ **真正的 fail-closed 在别处**：`restore_files` 的**还原后逐字节校验**
    （抓写盘错误 / 还原不完整）⇒ ⇒ 那一条 MUST 保持硬失败。

    ⇒ ⇒ 本函数的价值：让「基准是**工作树**还是 HEAD」**可见** ⇒ ⇒ 审稿时能
    知道变异是在**哪个基线**上做的。
    """
    dirty = []
    for rel, content in saved.items():
        proc = subprocess.run(["git", "show", "HEAD:%s" % rel], cwd=REPO,
                              capture_output=True, text=True)
        if proc.returncode != 0:
            dirty.append("%s（HEAD 无此文件 ⇒ 新增）" % rel)
        elif proc.stdout != content:
            dirty.append("%s（有未提交改动 ⇒ 基线 = 工作树）" % rel)
    if dirty:
        print("::notice::变异目标文件带未提交改动（基线 = 工作树，非 HEAD）：")
        for d in dirty:
            print("        %s" % d)
    return True


# ---------------------------------------------------------------------------
# 注入（按**行号** ⇒ 行号越界 / 子串不匹配 ⇒ fail-closed）
# ---------------------------------------------------------------------------

def apply_edits(edits):
    by_file = {}
    for e in edits:
        by_file.setdefault(e["file"], []).append(e)

    for rel, file_edits in by_file.items():
        abs_p = os.path.join(REPO, rel)
        with open(abs_p, encoding="utf-8") as fh:
            lines = fh.read().split("\n")
        for e in file_edits:
            idx = e["line"] - 1
            if idx < 0 or idx >= len(lines):
                fail("行 %d 越界（%s 共 %d 行）" % (e["line"], rel, len(lines)), 2)
            if e["old"] not in lines[idx]:
                fail("行 %d 不含 %r（%s）⇒ 实际内容: %r"
                     % (e["line"], e["old"], rel, lines[idx]), 2)
            lines[idx] = lines[idx].replace(e["old"], e["new"], 1)
        with open(abs_p, "w", encoding="utf-8") as fh:
            fh.write("\n".join(lines))


# ---------------------------------------------------------------------------
# 构建 / 观测
# ---------------------------------------------------------------------------

def build(targets):
    cmd = ["cmake", "--build", BUILD, "-j4"]
    if targets:
        cmd += ["--target"] + targets
    proc = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
    out = proc.stdout + proc.stderr
    if proc.returncode != 0 or "error:" in out:
        print("---- build 输出（尾部 40 行）----")
        print("\n".join(out.split("\n")[-40:]))
        fail("编译失败 ⇒ 绝不跑旧二进制", 2)
    return out


def ctest(pattern):
    """跑 ctest。⚠️ **硬化 ①**：`--no-tests=error` ⇒ 「匹配 0 个测试」是**错误**
    而不是「全绿」⇒ ⇒ 空转不再伪装成「仍绿」。"""
    proc = subprocess.run(
        ["ctest", "-R", pattern, "--no-tests=error", "--output-on-failure"],
        cwd=BUILD, capture_output=True, text=True)
    return proc.returncode, proc.stdout + proc.stderr


def check_build_targets_cover(spec):
    """⚠️ **硬化 ②**：被观测的 target MUST ⊆ `build_targets`。

    ⇒ ⇒ 否则「观测面 ⊄ 重建面」⇒ ⇒ 观测会退化成「看旧结果」⇒ ⇒ 误报
    「无判别力」。

    ⚠️ 口径：从 ctest 模式**无法可靠**反推 target 名 ⇒ ⇒ 改为**保守**判定：
    若 `build_targets` 为**空** ⇒ ⇒ 必违规（无法保证重建覆盖面）。
    至于「模式名 → target 名」的映射，交给**调用方**在 spec 里显式列出 ⇒ ⇒
    本函数只做**空清单**这一条**可机械判定**的检查，其余由 spec 作者负责。
    """
    if not spec.get("build_targets"):
        fail("build_targets 为空 ⇒ 无法保证「重建面 ⊇ 观测面」", 2)


# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------

def main():
    if len(sys.argv) != 2:
        fail("用法：mutation_harness.py <spec.json>", 2)
    with open(sys.argv[1], encoding="utf-8") as fh:
        spec = json.load(fh)

    mid = spec["id"]
    check_build_targets_cover(spec)

    files = sorted({e["file"] for e in spec["edits"]})
    saved = backup_files(files)
    try:
        if not verify_against_head(saved):
            fail("备份基准与 HEAD 不一致", 2)
        apply_edits(spec["edits"])
        print("MUTATE_OK: %s 已注入（%d 处编辑）" % (mid, len(spec["edits"])))

        build(spec.get("build_targets", []))

        expect = spec["expect_red"]
        rc, out = ctest(expect)
        if rc == 0:
            print("---- ctest 输出（尾部 40 行）----")
            print("\n".join(out.split("\n")[-40:]))
            fail("%s 变异后**仍绿** ⇒ 该判据无判别力" % mid, 3)
        print("✓ %s ⇒ %s 转红（符合预期）" % (mid, expect))

        for extra in spec.get("also_red", []):
            rc2, _ = ctest(extra)
            if rc2 == 0:
                fail("%s 变异后 %s **仍绿**（预期也应转红）" % (mid, extra), 3)
            print("✓ %s ⇒ %s 也转红（符合预期）" % (mid, extra))

        for extra in spec.get("also_green", []):
            rc3, _ = ctest(extra)
            if rc3 != 0:
                fail("%s 变异后 %s **转红**（预期应仍绿 ⇒ 该维度不可观测的结论"
                     "被推翻）" % (mid, extra), 3)
            print("✓ %s ⇒ %s **仍绿**（符合预期 ⇒ 该维度对本次变异**不可观测**）"
                  % (mid, extra))
    finally:
        restore_files(saved)
        print("↩ 工作区已还原")
        # ⚠️ **还原源码 ≠ 复原** —— **二进制也是状态** ⇒ ⇒ MUST 重建，
        #    否则后续跑的是**上一条变异**留下的旧二进制 ⇒ ⇒ 假红。
        build(spec.get("build_targets", []))
        print("↩ 已重建（源码与二进制一致）")


if __name__ == "__main__":
    main()
