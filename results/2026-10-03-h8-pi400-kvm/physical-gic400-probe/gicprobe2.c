// SPDX-License-Identifier: GPL-2.0
/*
 * One-shot capability and corner-case probe of the physical GIC-400 on a
 * Raspberry Pi 400, from the non-secure EL1/EL2 view Linux runs in. It uses
 * one disabled, idle SPI, runs on one CPU with local interrupts masked and
 * restores every register it changes.
 */
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/irqflags.h>

#define GICD_PHYS 0xff841000UL
#define GICC_PHYS 0xff842000UL

static void __iomem *d, *c;
static u32 id, word, bit;

static u32 rd(void __iomem *b, u32 off) { return readl_relaxed(b + off); }
static void wr(void __iomem *b, u32 off, u32 v) { writel_relaxed(v, b + off); mb(); udelay(2); }
static u32 pend(void) { return !!(rd(d, 0x200 + 4 * word) & bit); }
static u32 act(void) { return !!(rd(d, 0x300 + 4 * word) & bit); }

#define MAXREC 160
static struct { const char *k; u32 v; } rec[MAXREC];
static int nrec;
static void R(const char *k, u32 v) { if (nrec < MAXREC) { rec[nrec].k = k; rec[nrec].v = v; nrec++; } }

static void drain(void)
{
	/* Complete the probe interrupt if it is somehow acknowledged. */
	if ((rd(c, 0x18) & 0x3ff) == id) {
		u32 iar = rd(c, 0x0c);
		if ((iar & 0x3ff) == id) { wr(c, 0x10, iar); wr(c, 0x1000, iar); }
	}
}

static void quiesce(void)
{
	wr(d, 0x180 + 4 * word, bit);
	wr(d, 0x280 + 4 * word, bit);
	wr(d, 0x380 + 4 * word, bit);
}

static int __init gicprobe2_init(void)
{
	unsigned long flags;
	u32 lines, sh, prio_off, tgt_off, cfg_off, cfg_bit, prio, tgt, cfg, self, pmr, bpr, iar, v;
	int i, n;

	d = ioremap(GICD_PHYS, 0x1000);
	c = ioremap(GICC_PHYS, 0x2000);
	if (!d || !c)
		return -ENOMEM;
	if (rd(c, 0xfc) != 0x0202143b) {
		pr_err("gicprobe2: not a GIC-400 CPU interface, aborting\n");
		goto unmap;
	}
	lines = 32 * ((rd(d, 0x04) & 0x1f) + 1);

	local_irq_save(flags);
	id = 0;
	for (i = lines - 1; i >= 32; i--) {
		word = i / 32; bit = BIT(i % 32);
		if (!(rd(d, 0x100 + 4 * word) & bit) && !pend() && !act()) { id = i; break; }
	}
	if (!id) { local_irq_restore(flags); pr_err("gicprobe2: no idle SPI\n"); goto unmap; }
	word = id / 32; bit = BIT(id % 32); sh = 8 * (id % 4);
	prio_off = 0x400 + (id & ~3U); tgt_off = 0x800 + (id & ~3U);
	cfg_off = 0xc00 + 4 * (id / 16); cfg_bit = 2U << (2 * (id % 16));
	prio = rd(d, prio_off); tgt = rd(d, tgt_off); cfg = rd(d, cfg_off);
	self = rd(d, 0x800) & 0xff; pmr = rd(c, 0x04); bpr = rd(c, 0x08);

	/* 1. Identity and reset-ish state (read only). */
	R("intid", id); R("cpu_if_mask", self);
	R("GICD_CTLR", rd(d, 0x00)); R("GICD_TYPER", rd(d, 0x04)); R("GICD_IIDR", rd(d, 0x08));
	R("GICD_IGROUPR0_nsview", rd(d, 0x80)); R("GICD_ISENABLER0", rd(d, 0x100));
	R("GICD_ICFGR0", rd(d, 0xc00)); R("GICD_ICFGR1", rd(d, 0xc04));
	R("GICD_PPISR", rd(d, 0xd00)); R("GICD_SPISR0", rd(d, 0xd04));
	R("GICD_IPRIORITYR0", rd(d, 0x400)); R("GICD_IPRIORITYR7", rd(d, 0x41c));
	R("GICD_ITARGETSR0", rd(d, 0x800)); R("GICD_ITARGETSR7", rd(d, 0x81c));
	for (i = 0; i < 12; i++) {
		static const char * const idn[] = { "GICD_PIDR4", "GICD_PIDR5", "GICD_PIDR6", "GICD_PIDR7",
			"GICD_PIDR0", "GICD_PIDR1", "GICD_PIDR2", "GICD_PIDR3",
			"GICD_CIDR0", "GICD_CIDR1", "GICD_CIDR2", "GICD_CIDR3" };
		R(idn[i], rd(d, 0xfd0 + 4 * i));
	}
	R("GICC_CTLR", rd(c, 0x00)); R("GICC_PMR", pmr); R("GICC_BPR", bpr);
	R("GICC_RPR", rd(c, 0x14)); R("GICC_HPPIR", rd(c, 0x18)); R("GICC_ABPR", rd(c, 0x1c));
	R("GICC_AHPPIR", rd(c, 0x28)); R("GICC_APR0", rd(c, 0xd0)); R("GICC_NSAPR0", rd(c, 0xe0));
	R("GICC_IIDR", rd(c, 0xfc));

	/* 2. Writable-field widths, each restored at once. */
	wr(c, 0x04, 0xff); R("PMR_write_ff", rd(c, 0x04));
	wr(c, 0x04, 0x01); R("PMR_write_01", rd(c, 0x04));
	wr(c, 0x04, pmr);
	wr(c, 0x08, 0); R("BPR_write_0", rd(c, 0x08));
	wr(c, 0x08, 7); R("BPR_write_7", rd(c, 0x08));
	wr(c, 0x08, bpr);
	wr(d, prio_off, prio | (0xffU << sh)); R("IPRIORITY_write_ff", (rd(d, prio_off) >> sh) & 0xff);
	wr(d, prio_off, prio & ~(0xffU << sh)); R("IPRIORITY_write_00", (rd(d, prio_off) >> sh) & 0xff);
	wr(d, tgt_off, tgt | (0xffU << sh)); R("ITARGETS_write_ff", (rd(d, tgt_off) >> sh) & 0xff);
	wr(d, tgt_off, tgt & ~(0xffU << sh)); R("ITARGETS_write_00", (rd(d, tgt_off) >> sh) & 0xff);
	wr(d, cfg_off, cfg | (3U << (2 * (id % 16)))); R("ICFGR_write_3", (rd(d, cfg_off) >> (2 * (id % 16))) & 3);
	wr(d, cfg_off, cfg & ~(3U << (2 * (id % 16)))); R("ICFGR_write_0", (rd(d, cfg_off) >> (2 * (id % 16))) & 3);
	v = rd(d, 0xc04);
	wr(d, 0xc04, ~v); R("ICFGR1_ppi_write_inverted", rd(d, 0xc04)); wr(d, 0xc04, v);
	v = rd(d, 0xc00);
	wr(d, 0xc00, ~v); R("ICFGR0_sgi_write_inverted", rd(d, 0xc00)); wr(d, 0xc00, v);

	/* Common setup: level-sensitive, priority 0x40, targeted here. */
	wr(d, prio_off, (prio & ~(0xffU << sh)) | (0x40U << sh));
	wr(d, tgt_off, (tgt & ~(0xffU << sh)) | (self << sh));
	wr(d, cfg_off, cfg & ~cfg_bit);

	/* 3. Pending while disabled is not forwarded. */
	wr(d, 0x200 + 4 * word, bit);
	R("disabled_pend", pend()); R("disabled_hppir", rd(c, 0x18));
	wr(d, 0x280 + 4 * word, bit);
	R("disabled_pend_after_icpendr", pend());

	/* 4. ICPENDR removes a software-pended level-sensitive interrupt. */
	wr(d, 0x100 + 4 * word, bit);
	wr(d, 0x200 + 4 * word, bit);
	R("level_pend", pend()); R("level_hppir", rd(c, 0x18));
	wr(d, 0x280 + 4 * word, bit);
	R("level_pend_after_icpendr", pend()); R("level_hppir_after_icpendr", rd(c, 0x18));
	drain();

	/* 5. Priority mask: priority equal to PMR is not signalled. */
	wr(c, 0x04, 0x40);
	R("masked_PMR", rd(c, 0x04));
	wr(d, 0x200 + 4 * word, bit);
	R("masked_pend", pend()); R("masked_hppir", rd(c, 0x18));
	iar = rd(c, 0x0c); R("masked_iar", iar);
	if ((iar & 0x3ff) == id) { wr(c, 0x10, iar); wr(c, 0x1000, iar); }
	else if ((iar & 0x3ff) < 1020) { wr(c, 0x10, iar); wr(c, 0x1000, iar); R("masked_unexpected_ack", iar); }
	R("masked_pend_after_iar", pend()); R("masked_act_after_iar", act());
	wr(c, 0x04, 0x50);
	R("unmasked_PMR", rd(c, 0x04)); R("unmasked_hppir", rd(c, 0x18));
	wr(d, 0x280 + 4 * word, bit);
	wr(c, 0x04, pmr);
	drain();

	/* 6. Active state cleared through ICACTIVER after priority drop. */
	wr(d, 0x200 + 4 * word, bit);
	if ((rd(c, 0x18) & 0x3ff) == id) {
		iar = rd(c, 0x0c); R("icact_iar", iar);
		if ((iar & 0x3ff) == id) {
			R("icact_apr0_active", rd(c, 0xd0)); R("icact_nsapr0_active", rd(c, 0xe0));
			R("icact_rpr_active", rd(c, 0x14));
			wr(c, 0x10, iar);
			R("icact_apr0_after_eoir", rd(c, 0xd0)); R("icact_act_after_eoir", act());
			wr(d, 0x380 + 4 * word, bit);
			R("icact_act_after_icactiver", act());
			R("icact_rpr_final", rd(c, 0x14)); R("icact_hppir_final", rd(c, 0x18));
		} else if ((iar & 0x3ff) < 1020) { wr(c, 0x10, iar); wr(c, 0x1000, iar); }
	}

	/* 7. ISACTIVER sets active without an acknowledgement; DIR clears it. */
	quiesce();
	wr(d, 0x300 + 4 * word, bit);
	R("isact_act", act()); R("isact_rpr", rd(c, 0x14)); R("isact_hppir", rd(c, 0x18));
	wr(d, 0x200 + 4 * word, bit);
	wr(d, 0x100 + 4 * word, bit);
	R("isact_pend_while_active", pend()); R("isact_hppir_pend_while_active", rd(c, 0x18));
	wr(d, 0x180 + 4 * word, bit);
	wr(d, 0x280 + 4 * word, bit);
	wr(c, 0x1000, id);
	R("isact_act_after_dir", act());

	quiesce();
	R("final_pend", pend()); R("final_act", act());
	wr(d, prio_off, prio); wr(d, tgt_off, tgt); wr(d, cfg_off, cfg);
	wr(c, 0x04, pmr); wr(c, 0x08, bpr);
	R("final_PMR", rd(c, 0x04)); R("final_BPR", rd(c, 0x08)); R("final_RPR", rd(c, 0x14));
	local_irq_restore(flags);

	for (n = 0; n < nrec; n++)
		pr_info("gicprobe2: %s=0x%08x\n", rec[n].k, rec[n].v);
	pr_info("gicprobe2: done (%d records)\n", nrec);
unmap:
	iounmap(d); iounmap(c);
	return 0;
}

static void __exit gicprobe2_exit(void) { }
module_init(gicprobe2_init);
module_exit(gicprobe2_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("One-shot GIC-400 capability and corner-case probe");
