#!/usr/bin/env python3
"""Replay the gicprobe/gicprobe2 register sequences against the qemu-pi4
raspi400 model through qtest and print the same key=value records, so the
model can be diffed against the physical GIC-400 capture."""
import os, socket, subprocess, sys, tempfile, time

GICD, GICC = 0xff841000, 0xff842000
qemu = sys.argv[1]
tmp = tempfile.mkdtemp()
path = os.path.join(tmp, "qtest.sock")
srv = socket.socket(socket.AF_UNIX); srv.bind(path); srv.listen(1)
proc = subprocess.Popen([qemu, "-machine", "raspi400", "-accel", "qtest", "-qtest", "unix:" + path,
                         "-display", "none", "-monitor", "none", "-serial", "none"],
                        stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
srv.settimeout(30); conn, _ = srv.accept(); f = conn.makefile("rw")
def cmd(s):
    f.write(s + "\n"); f.flush()
    while True:
        line = f.readline()
        if not line: raise SystemExit("qemu died: " + proc.stderr.read().decode()[-400:])
        if line.startswith("OK") or line.startswith("FAIL") or line.startswith("ERR"): return line.split()
def rd(b, off): return int(cmd(f"readl 0x{b + off:x}")[1], 16)
def wr(b, off, v): cmd(f"writel 0x{b + off:x} 0x{v & 0xffffffff:x}")
d, c = GICD, GICC
ID = int(sys.argv[2]) if len(sys.argv) > 2 else 255; word = ID // 32; bit = 1 << (ID % 32); sh = 8 * (ID % 4)
prio_off = 0x400 + (ID & ~3); tgt_off = 0x800 + (ID & ~3)
cfg_off = 0xc00 + 4 * (ID // 16); cfg_sh = 2 * (ID % 16); cfg_bit = 2 << cfg_sh
def pend(): return int(bool(rd(d, 0x200 + 4 * word) & bit))
def act(): return int(bool(rd(d, 0x300 + 4 * word) & bit))
def R(k, v): print(f"{k}=0x{v:08x}")

# Reset-state identity before any guest-like setup.
for k, b, o in (("reset_GICD_CTLR", d, 0), ("reset_GICC_CTLR", c, 0), ("reset_GICC_PMR", c, 4), ("reset_GICC_BPR", c, 8)):
    R(k, rd(b, o))
# Bring the interface to the state Linux leaves it in on the board.
wr(d, 0x000, 1)
for n in range(8): wr(d, 0x400 + 4 * n, 0xa0a0a0a0)
wr(c, 0x04, 0xf0); wr(c, 0x00, 0x261)
prio = rd(d, prio_off); tgt = rd(d, tgt_off); cfg = rd(d, cfg_off)
selfm = rd(d, 0x800) & 0xff; pmr = rd(c, 4); bpr = rd(c, 8)

print("# gicprobe2")
R("intid", ID); R("cpu_if_mask", selfm)
for k, b, o in (("GICD_CTLR", d, 0), ("GICD_TYPER", d, 4), ("GICD_IIDR", d, 8), ("GICD_IGROUPR0_nsview", d, 0x80),
                ("GICD_ICFGR0", d, 0xc00), ("GICD_ICFGR1", d, 0xc04), ("GICD_PPISR", d, 0xd00), ("GICD_SPISR0", d, 0xd04),
                ("GICD_IPRIORITYR0", d, 0x400), ("GICD_ITARGETSR0", d, 0x800)):
    R(k, rd(b, o))
for i, n in enumerate(("PIDR4", "PIDR5", "PIDR6", "PIDR7", "PIDR0", "PIDR1", "PIDR2", "PIDR3", "CIDR0", "CIDR1", "CIDR2", "CIDR3")):
    R("GICD_" + n, rd(d, 0xfd0 + 4 * i))
for k, o in (("GICC_CTLR", 0), ("GICC_PMR", 4), ("GICC_BPR", 8), ("GICC_RPR", 0x14), ("GICC_HPPIR", 0x18), ("GICC_ABPR", 0x1c),
             ("GICC_AHPPIR", 0x28), ("GICC_APR0", 0xd0), ("GICC_NSAPR0", 0xe0), ("GICC_IIDR", 0xfc)):
    R(k, rd(c, o))
wr(c, 4, 0xff); R("PMR_write_ff", rd(c, 4)); wr(c, 4, 1); R("PMR_write_01", rd(c, 4)); wr(c, 4, pmr)
wr(c, 8, 0); R("BPR_write_0", rd(c, 8)); wr(c, 8, 7); R("BPR_write_7", rd(c, 8)); wr(c, 8, bpr)
wr(d, prio_off, prio | (0xff << sh)); R("IPRIORITY_write_ff", (rd(d, prio_off) >> sh) & 0xff)
wr(d, prio_off, prio & ~(0xff << sh)); R("IPRIORITY_write_00", (rd(d, prio_off) >> sh) & 0xff)
wr(d, tgt_off, tgt | (0xff << sh)); R("ITARGETS_write_ff", (rd(d, tgt_off) >> sh) & 0xff)
wr(d, tgt_off, tgt & ~(0xff << sh)); R("ITARGETS_write_00", (rd(d, tgt_off) >> sh) & 0xff)
wr(d, cfg_off, cfg | (3 << cfg_sh)); R("ICFGR_write_3", (rd(d, cfg_off) >> cfg_sh) & 3)
wr(d, cfg_off, cfg & ~(3 << cfg_sh)); R("ICFGR_write_0", (rd(d, cfg_off) >> cfg_sh) & 3)
v = rd(d, 0xc04); wr(d, 0xc04, ~v); R("ICFGR1_ppi_write_inverted", rd(d, 0xc04)); wr(d, 0xc04, v)
v = rd(d, 0xc00); wr(d, 0xc00, ~v); R("ICFGR0_sgi_write_inverted", rd(d, 0xc00)); wr(d, 0xc00, v)
wr(d, prio_off, (prio & ~(0xff << sh)) | (0x40 << sh)); wr(d, tgt_off, (tgt & ~(0xff << sh)) | (selfm << sh))
wr(d, cfg_off, cfg & ~cfg_bit)
wr(d, 0x200 + 4 * word, bit); R("disabled_pend", pend()); R("disabled_hppir", rd(c, 0x18))
wr(d, 0x280 + 4 * word, bit); R("disabled_pend_after_icpendr", pend())
wr(d, 0x100 + 4 * word, bit); wr(d, 0x200 + 4 * word, bit)
R("level_pend", pend()); R("level_hppir", rd(c, 0x18))
wr(d, 0x280 + 4 * word, bit); R("level_pend_after_icpendr", pend()); R("level_hppir_after_icpendr", rd(c, 0x18))
wr(c, 4, 0x40); R("masked_PMR", rd(c, 4)); wr(d, 0x200 + 4 * word, bit)
R("masked_pend", pend()); R("masked_hppir", rd(c, 0x18)); iar = rd(c, 0xc); R("masked_iar", iar)
if (iar & 0x3ff) == ID: wr(c, 0x10, iar); wr(c, 0x1000, iar)
R("masked_pend_after_iar", pend()); R("masked_act_after_iar", act())
wr(c, 4, 0x50); R("unmasked_PMR", rd(c, 4)); R("unmasked_hppir", rd(c, 0x18))
wr(d, 0x280 + 4 * word, bit); wr(c, 4, pmr)
wr(d, 0x200 + 4 * word, bit)
iar = rd(c, 0xc); R("icact_iar", iar)
R("icact_apr0_active", rd(c, 0xd0)); R("icact_nsapr0_active", rd(c, 0xe0)); R("icact_rpr_active", rd(c, 0x14))
wr(c, 0x10, iar); R("icact_apr0_after_eoir", rd(c, 0xd0)); R("icact_act_after_eoir", act())
wr(d, 0x380 + 4 * word, bit); R("icact_act_after_icactiver", act())
R("icact_rpr_final", rd(c, 0x14)); R("icact_hppir_final", rd(c, 0x18))
for o in (0x180, 0x280, 0x380): wr(d, o + 4 * word, bit)
wr(d, 0x300 + 4 * word, bit); R("isact_act", act()); R("isact_rpr", rd(c, 0x14)); R("isact_hppir", rd(c, 0x18))
wr(d, 0x200 + 4 * word, bit); wr(d, 0x100 + 4 * word, bit)
R("isact_pend_while_active", pend()); R("isact_hppir_pend_while_active", rd(c, 0x18))
wr(d, 0x180 + 4 * word, bit); wr(d, 0x280 + 4 * word, bit); wr(c, 0x1000, ID); R("isact_act_after_dir", act())
for o in (0x180, 0x280, 0x380): wr(d, o + 4 * word, bit)

for mode, edge, repend in (("level", 0, 0), ("level-repend", 0, 1), ("edge", 1, 0), ("edge-repend", 1, 1)):
    print("# gicprobe", mode)
    P = lambda k, v: print(f"{mode} {k}=0x{v:x}")
    wr(d, cfg_off, (cfg | cfg_bit) if edge else (cfg & ~cfg_bit))
    P("icfgr_bit", int(bool(rd(d, cfg_off) & cfg_bit)))
    wr(d, 0x100 + 4 * word, bit); P("pend_initial", pend()); P("act_initial", act())
    wr(d, 0x200 + 4 * word, bit); P("pend_after_set", pend()); P("act_after_set", act())
    P("hppir_pending", rd(c, 0x18)); iar = rd(c, 0xc); P("iar", iar); P("rpr_active", rd(c, 0x14))
    P("PEND_AFTER_IAR", pend()); P("act_after_iar", act())
    if repend:
        wr(d, 0x200 + 4 * word, bit); P("pend_after_repend", pend()); P("act_after_repend", act())
        P("hppir_after_repend", rd(c, 0x18))
    wr(c, 0x10, iar); P("rpr_after_eoir", rd(c, 0x14)); P("pend_after_eoir", pend()); P("act_after_eoir", act())
    P("hppir_after_eoir", rd(c, 0x18))
    wr(c, 0x1000, iar); P("PEND_AFTER_DIR", pend()); P("act_after_dir", act()); P("hppir_after_dir", rd(c, 0x18))
    if (rd(c, 0x18) & 0x3ff) == ID:
        iar = rd(c, 0xc); P("iar_second", iar); P("pend_after_iar2", pend()); P("act_after_iar2", act())
        wr(c, 0x10, iar); wr(c, 0x1000, iar)
        P("pend_after_dir2", pend()); P("act_after_dir2", act()); P("hppir_after_dir2", rd(c, 0x18))
    for o in (0x180, 0x280, 0x380): wr(d, o + 4 * word, bit)
    P("pend_final", pend()); P("act_final", act())
proc.kill()
