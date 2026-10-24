// SPDX-License-Identifier: GPL-2.0+
/*
 * taskit PortuxG20 (Stamp9G20 module, AT91SAM9G20)
 *
 * SPL part based on board/siemens/taurus and board/siemens/smartweb,
 * board part on the former board/taskit/stamp9g20 (removed in v2015.10):
 *   (C) Copyright 2010 Christian Glindkamp <christian.glindkamp@taskit.de>
 *   (C) Copyright 2012-2014 Markus Hubig <mhubig@imko.de>, IMKO GmbH
 *   (C) Copyright 2013-2014 Heiko Schocher <hs@denx.de>
 */

#include <config.h>
#include <init.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/arch/at91sam9_sdramc.h>
#include <asm/arch/at91sam9260_matrix.h>
#include <asm/arch/at91sam9_smc.h>
#include <asm/arch/at91_common.h>
#include <asm/arch/at91_rstc.h>
#include <asm/arch/clk.h>
#include <asm/arch/gpio.h>
#include <time.h>
#include <linux/delay.h>

DECLARE_GLOBAL_DATA_PTR;

/* PC9 high enables the RS-232 transceiver of X6/X4 (DBGU and USART2) */
#define PORTUXG20_RS232_EN	AT91_PIN_PC9
/* PA26 low switches on the 50 MHz RMII reference clock of the PHY */
#define PORTUXG20_PHY_EN	AT91_PIN_PA26

static void portuxg20_rs232_enable(void)
{
	at91_set_gpio_output(PORTUXG20_RS232_EN, 1);
}

static void portuxg20_nand_hw_init(void)
{
	struct at91_smc *smc = (struct at91_smc *)ATMEL_BASE_SMC;
	struct at91_matrix *matrix = (struct at91_matrix *)ATMEL_BASE_MATRIX;

	/* Assign CS3 to NAND/SmartMedia Interface */
	writel(readl(&matrix->ebicsa) | AT91_MATRIX_CS3A_SMC_SMARTMEDIA,
	       &matrix->ebicsa);

	/* SMC CS3 timings of the taskit board code, MCK = 132 MHz */
	writel(AT91_SMC_SETUP_NWE(2) | AT91_SMC_SETUP_NCS_WR(0) |
	       AT91_SMC_SETUP_NRD(2) | AT91_SMC_SETUP_NCS_RD(0),
	       &smc->cs[3].setup);
	writel(AT91_SMC_PULSE_NWE(4) | AT91_SMC_PULSE_NCS_WR(4) |
	       AT91_SMC_PULSE_NRD(4) | AT91_SMC_PULSE_NCS_RD(4),
	       &smc->cs[3].pulse);
	writel(AT91_SMC_CYCLE_NWE(7) | AT91_SMC_CYCLE_NRD(7),
	       &smc->cs[3].cycle);
	writel(AT91_SMC_MODE_RM_NRD | AT91_SMC_MODE_WM_NWE |
	       AT91_SMC_MODE_EXNW_DISABLE | AT91_SMC_MODE_DBW_8 |
	       AT91_SMC_MODE_TDF_CYCLE(3),
	       &smc->cs[3].mode);

	/* Configure RDY/BSY */
	at91_set_gpio_input(CFG_SYS_NAND_READY_PIN, 1);

	/* Enable NandFlash */
	at91_set_gpio_output(CFG_SYS_NAND_ENABLE_PIN, 1);
}

#if defined(CONFIG_XPL_BUILD)
#include <spl.h>

void matrix_init(void)
{
	struct at91_matrix *mat = (struct at91_matrix *)ATMEL_BASE_MATRIX;

	writel((readl(&mat->scfg[3]) & (~AT91_MATRIX_SLOT_CYCLE))
			| AT91_MATRIX_SLOT_CYCLE_(0x40),
			&mat->scfg[3]);

	/*
	 * Runs before the console is set up: without PC9 high the
	 * RS-232 level shifter on X6 stays off and SPL output is lost.
	 * Driving a PIO output does not need the PIO clock.
	 */
	portuxg20_rs232_enable();
}

void at91_spl_board_init(void)
{
	portuxg20_nand_hw_init();
}

/*
 * 64 MiB, 32 bit, 4 banks, 13 rows, 9 columns, CAS 3; timings at
 * MCK = 132 MHz as used by at91bootstrap's at91sam9g20ek profile, which
 * taskit's own bootstrap configuration selected.
 */
#define SDRAM_BASE_CONF	(AT91_SDRAMC_NC_9 | AT91_SDRAMC_NR_13 \
			 | AT91_SDRAMC_CAS_3 \
			 | AT91_SDRAMC_NB_4 | AT91_SDRAMC_DBW_32 \
			 | AT91_SDRAMC_TWR_VAL(3) | AT91_SDRAMC_TRC_VAL(9) \
			 | AT91_SDRAMC_TRP_VAL(3) | AT91_SDRAMC_TRCD_VAL(3) \
			 | AT91_SDRAMC_TRAS_VAL(6) | AT91_SDRAMC_TXSR_VAL(10))

void at91_mem_init(void)
{
	struct at91_matrix *ma = (struct at91_matrix *)ATMEL_BASE_MATRIX;
	struct sdramc_reg setting;
	u32 csa;

	at91_sdram_hw_init();

	setting.cr = SDRAM_BASE_CONF;
	setting.mdr = AT91_SDRAMC_MD_SDRAM;
	setting.tr = (CFG_SYS_MASTER_CLOCK * 7) / 1000000;

	csa = readl(&ma->ebicsa) | AT91_MATRIX_CS1A_SDRAMC;
	if (IS_ENABLED(CONFIG_PORTUXG20_VDDIOM_1V8))
		csa &= ~AT91_MATRIX_VDDIOMSEL_3_3V;
	writel(csa, &ma->ebicsa);

	sdramc_initialize(ATMEL_BASE_CS1, &setting);
}
#endif /* CONFIG_XPL_BUILD */

#ifdef CONFIG_MACB
/*
 * The PHY (DM9161 on the Stamp Adaptor 2 reference design) samples its
 * address straps from the RMII receive lines while NRST is low, so their
 * pull-ups must be off during the reset pulse (address 0).  The pulse is
 * ERSTL(5), about 2 ms, as in taskit's 2013 "reset-fix" for this board,
 * instead of the generic 500 ms of at91_phy_reset().
 */
static void portuxg20_phy_reset(void)
{
	struct at91_port *pioa = (struct at91_port *)ATMEL_BASE_PIOA;
	struct at91_rstc *rstc = (struct at91_rstc *)ATMEL_BASE_RSTC;
	const u32 straps = pin_to_mask(AT91_PIN_PA14) |
			   pin_to_mask(AT91_PIN_PA15) |
			   pin_to_mask(AT91_PIN_PA17) |
			   pin_to_mask(AT91_PIN_PA18) |
			   pin_to_mask(AT91_PIN_PA28);
	unsigned long start;
	u32 erstl;

	at91_set_gpio_output(PORTUXG20_PHY_EN, 0);

	writel(straps, &pioa->pudr);

	erstl = readl(&rstc->mr) & AT91_RSTC_MR_ERSTL_MASK;
	writel(AT91_RSTC_KEY | AT91_RSTC_MR_ERSTL(5) | AT91_RSTC_MR_URSTEN,
	       &rstc->mr);
	writel(AT91_RSTC_KEY | AT91_RSTC_CR_EXTRST, &rstc->cr);

	start = get_timer(0);
	while (!(readl(&rstc->sr) & AT91_RSTC_SR_NRSTL)) {
		if (get_timer(start) >= 100) {
			puts("*** ERROR: Timeout waiting for PHY reset!\n");
			break;
		}
	}
	writel(AT91_RSTC_KEY | erstl | AT91_RSTC_MR_URSTEN, &rstc->mr);

	writel(straps, &pioa->puer);
}
#endif

int board_early_init_f(void)
{
	at91_periph_clk_enable(ATMEL_ID_PIOA);
	at91_periph_clk_enable(ATMEL_ID_PIOB);
	at91_periph_clk_enable(ATMEL_ID_PIOC);

	/* also needed when chain-loaded by another loader */
	portuxg20_rs232_enable();
	at91_seriald_hw_init();

	return 0;
}

int board_init(void)
{
	/* address of boot parameters */
	gd->bd->bi_boot_params = CFG_SYS_SDRAM_BASE + 0x100;

#ifdef CONFIG_CMD_NAND
	portuxg20_nand_hw_init();
#endif
#ifdef CONFIG_MACB
	at91_periph_clk_enable(ATMEL_ID_EMAC0);
	portuxg20_phy_reset();
#endif
	return 0;
}

int dram_init(void)
{
	gd->ram_size = get_ram_size((void *)CFG_SYS_SDRAM_BASE,
				    CFG_SYS_SDRAM_SIZE);
	return 0;
}
