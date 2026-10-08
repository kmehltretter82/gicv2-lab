// SPDX-License-Identifier: GPL-2.0
/*
 * One-shot probe of the physical GIC-400 on a Raspberry Pi 400: what does
 * GICD_ISPENDR report for a software-pended SPI after it is acknowledged?
 * Runs on one CPU with local interrupts masked, on an SPI that is disabled,
 * idle and unused, and restores every register it changes.
 */
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/irqflags.h>
#include <linux/smp.h>

#define GICD_PHYS 0xff841000UL
#define GICC_PHYS 0xff842000UL

static void __iomem *d, *c;
static u32 id, word, bit;

static u32 rd(void __iomem *b, u32 off) { return readl_relaxed(b + off); }
static void wr(void __iomem *b, u32 off, u32 v) { writel_relaxed(v, b + off); mb(); udelay(2); }
static u32 pend(void) { return !!(rd(d, 0x200 + 4 * word) & bit); }
static u32 act(void) { return !!(rd(d, 0x300 + 4 * word) & bit); }

#define MAXREC 64
static struct { const char *k; u32 v; } rec[MAXREC];
static int nrec;
static void R(const char *k, u32 v) { if (nrec < MAXREC) { rec[nrec].k = k; rec[nrec].v = v; nrec++; } }

static void state(const char *k_p, const char *k_a)
{
	R(k_p, pend()); R(k_a, act());
}

/* Returns 0 if the sequence stayed on the probe interrupt. */
static int run(bool edge, bool repend)
{
	u32 cfg_off = 0xc00 + 4 * (id / 16), cfg_bit = 2U << (2 * (id % 16));
	u32 cfg, iar;
	int ret = -1;

	cfg = rd(d, cfg_off);
	wr(d, cfg_off, edge ? (cfg | cfg_bit) : (cfg & ~cfg_bit));
	R("icfgr_bit", !!(rd(d, cfg_off) & cfg_bit));
	wr(d, 0x100 + 4 * word, bit);			/* ISENABLER */
	state("pend_initial", "act_initial");
	wr(d, 0x200 + 4 * word, bit);			/* ISPENDR */
	state("pend_after_set", "act_after_set");
	R("hppir_pending", rd(c, 0x18));
	if ((rd(c, 0x18) & 0x3ff) != id)
		goto out;
	iar = rd(c, 0x0c);
	R("iar", iar);
	if ((iar & 0x3ff) != id) {
		if ((iar & 0x3ff) < 1020) { wr(c, 0x10, iar); wr(c, 0x1000, iar); }
		goto out;
	}
	R("rpr_active", rd(c, 0x14));
	state("PEND_AFTER_IAR", "act_after_iar");
	if (repend) {
		wr(d, 0x200 + 4 * word, bit);
		state("pend_after_repend", "act_after_repend");
		R("hppir_after_repend", rd(c, 0x18));
	}
	wr(c, 0x10, iar);				/* EOIR: priority drop */
	R("rpr_after_eoir", rd(c, 0x14));
	state("pend_after_eoir", "act_after_eoir");
	R("hppir_after_eoir", rd(c, 0x18));
	wr(c, 0x1000, iar);				/* DIR: deactivate */
	state("PEND_AFTER_DIR", "act_after_dir");
	R("hppir_after_dir", rd(c, 0x18));
	if ((rd(c, 0x18) & 0x3ff) == id) {
		iar = rd(c, 0x0c);
		R("iar_second", iar);
		state("pend_after_iar2", "act_after_iar2");
		if ((iar & 0x3ff) == id) { wr(c, 0x10, iar); wr(c, 0x1000, iar); }
		state("pend_after_dir2", "act_after_dir2");
		R("hppir_after_dir2", rd(c, 0x18));
	}
	ret = 0;
out:
	wr(d, 0x180 + 4 * word, bit);			/* ICENABLER */
	wr(d, 0x280 + 4 * word, bit);			/* ICPENDR */
	wr(d, 0x380 + 4 * word, bit);			/* ICACTIVER */
	state("pend_final", "act_final");
	wr(d, cfg_off, cfg);
	return ret;
}

static int __init gicprobe_init(void)
{
	unsigned long flags;
	u32 typer, lines, prio_off, tgt_off, prio, tgt, self, ctlr, pmr;
	int i, r, n, mode;
	static const char * const names[] = { "level", "level-repend", "edge", "edge-repend" };

	d = ioremap(GICD_PHYS, 0x1000);
	c = ioremap(GICC_PHYS, 0x2000);
	if (!d || !c)
		return -ENOMEM;
	pr_info("gicprobe: GICD_IIDR=%08x GICC_IIDR=%08x GICD_TYPER=%08x GICD_CTLR=%08x\n",
		rd(d, 0x08), rd(c, 0xfc), rd(d, 0x04), rd(d, 0x00));
	if (rd(c, 0xfc) != 0x0202143b) {
		pr_err("gicprobe: not a GIC-400 CPU interface, aborting\n");
		goto unmap;
	}
	typer = rd(d, 0x04);
	lines = 32 * ((typer & 0x1f) + 1);

	for (mode = 0; mode < 4; mode++) {
		local_irq_save(flags);
		id = 0;
		for (i = lines - 1; i >= 32; i--) {
			word = i / 32; bit = BIT(i % 32);
			if (!(rd(d, 0x100 + 4 * word) & bit) && !pend() && !act()) {
				id = i;
				break;
			}
		}
		if (!id) {
			local_irq_restore(flags);
			pr_err("gicprobe: no idle SPI found\n");
			break;
		}
		word = id / 32; bit = BIT(id % 32);
		prio_off = 0x400 + (id & ~3U); tgt_off = 0x800 + (id & ~3U);
		prio = rd(d, prio_off); tgt = rd(d, tgt_off);
		self = rd(d, 0x800) & 0xff;
		ctlr = rd(c, 0x00); pmr = rd(c, 0x04);
		nrec = 0;
		wr(d, prio_off, (prio & ~(0xffU << (8 * (id % 4)))) | (0x40U << (8 * (id % 4))));
		wr(d, tgt_off, (tgt & ~(0xffU << (8 * (id % 4)))) | (self << (8 * (id % 4))));
		R("prio_readback", (rd(d, prio_off) >> (8 * (id % 4))) & 0xff);
		r = run(mode >= 2, mode & 1);
		wr(d, prio_off, prio); wr(d, tgt_off, tgt);
		local_irq_restore(flags);

		pr_info("gicprobe: mode=%s intid=%u cpu_if_mask=%02x GICC_CTLR=%08x GICC_PMR=%02x result=%d\n",
			names[mode], id, self, ctlr, pmr, r);
		for (n = 0; n < nrec; n++)
			pr_info("gicprobe: %s %s=0x%x\n", names[mode], rec[n].k, rec[n].v);
	}
	pr_info("gicprobe: done\n");
unmap:
	iounmap(d); iounmap(c);
	return 0;
}

static void __exit gicprobe_exit(void) { }
module_init(gicprobe_init);
module_exit(gicprobe_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("One-shot GIC-400 software-pending latch probe");
