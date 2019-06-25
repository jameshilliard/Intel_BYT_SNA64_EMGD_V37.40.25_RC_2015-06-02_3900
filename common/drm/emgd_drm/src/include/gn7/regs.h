/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: regs.h
 *-----------------------------------------------------------------------------
 * Copyright (c) 2002-2014, Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 *-----------------------------------------------------------------------------
 * Description:
 *  This file includes register definitions.
 *  Compiler:
 *  This file contains general constants, structures and should be included
 *  in all noninclude files.
 *-----------------------------------------------------------------------------
 */

#ifndef _VLV_REGS_H_
#define _VLV_REGS_H_


/*-----------------------------------------------------------------------------
 * Useful utility MACROSs
 *---------------------------------------------------------------------------*/
#define _MASKED_BIT_ENABLE(a) (((a) << 16) | (a))
#define _MASKED_BIT_DISABLE(a) ((a) << 16)


/*-----------------------------------------------------------------------------
 * PCI Register Definitions / Device #0
 *---------------------------------------------------------------------------*/
/* FIXME: These are both 64 bit regs, but using as a 32 bit reg */

/* Basic PCI Config Space stuff*/
#define INTEL_OFFSET_BRIDGE_RID     0x08
#define INTEL_OFFSET_BRIDGE_CAPREG  0xE0

/* GTT related */
#define INTEL_OFFSET_VGA_GTTMMADR0  0x10
#define INTEL_OFFSET_VGA_GTTMMADR1  0x14
#define VLV_GTT_GTTMMADR_SIZE       (4*1024*1024)


/* Graphics memory aperture */
#define VLV_PCI_MSAC_REG            0x62
#define VLV_PCI_GMADR_REG0          0x18
#define VLV_PCI_GMADR_REG1          0x1C

/* Stolen memory related */
#define VLV_PCI_BDSM_REG    0x5C /* GMCH Base of Data Stolen Vid-Memory register */
#define VLV_PCI_BGSM_REG    0x70 /* GMCH Base of GTT Stolen Vid-Memory register */

#define VLV_PCI_IOBAR       0x20 /* GMCH IO BAR-hi for register IO space access */
#define VLV_PCI_GGC_REG     0x50 /* GMCH Graphics Control Register */

/* BELOW PCI CONFIG SPACE REGS NOT SCRUBBED YET */
#define PCI_CAPREG_4      0x44 /* Capability Identification Reg[39:31] */
#define PCI_MOBILE_BIT    BIT0
#define PCI_GMS_MASK      BIT6 + BIT5 + BIT4 /* GFX Mode Select Bits Mask */
#define PCI_LOCAL		  BIT4 /* Local memory enabled */
#define PCI_DVMT_512K     BIT5 /* 512KB DVMT */
#define PCI_DVMT_1M       BIT5 + BIT4 /* 1MB DVMT */
#define PCI_DVMT_8M       BIT6 /* 8MB DVMT */
#define PCI_DRB_REG       0x60 /* DRAM row boundary Register */
#define PCI_DRC_REG       0x7C /* DRAM Controller Mode Register */
#define PCI_ESMRAMC_REG   0x91 /* Extended System Management RAM Reg */
#define PCI_TSEG_SZ		  BIT1 /* TSEG size bit */
#define PCI_TSEG_512K	  0    /* 512K TSEG */
#define PCI_TSEG_1M       BIT1 /* 1MB TSEG */
#define PCI_GCLKIO_REG    0xC0 /* GMCH Clock and IO Control Register */
#define PCI_GMCHCFG_REG   0xC6 /* GMCH Configuration Register */
#define PCI_CONFIG_LMINT  0xE0


/*-----------------------------------------------------------------------------
 * General VGA Register Definitions
 *---------------------------------------------------------------------------*/
#define FEATURE_CONT_REG      0x3DA  /* Feature Control Register */
#define FEATURE_CONT_REG_MONO 0x3BA  /* Feature Control Register (Mono) */
#define FEATURE_CONT_REG_READ 0x3CA  /* Feature Control Register (Read) */
#define MSR_PORT              0x3C2  /* Miscellaneous Output Port */
#define MSR_PORT_LSB          0xC2   /* Miscellaneous Output Port LSB */
#define MSR_READ_PORT         0x3CC  /* Miscellaneous Output Reg (Read) */
#define STATUS_REG_00         0x3C2  /* Input Status Register 0 */
#define STATUS_REG_01         0x3DA  /* Input Status Register 1 */
#define STATUS_REG_01_MONO    0x3BA  /* Input Status Register 1 (Mono) */

/* DAC Register Definitions. */
#define DAC_PEL_MASK          0x3C6  /* Color Palette Pixel Mask Register */
#define DAC_READ_INDEX        0x3C7  /* Color Palette Read-Mode Index Reg */
#define DAC_STATE             0x3C7  /* Color Palette State Register */
#define DAC_WRITE_INDEX       0x3C8  /* Color Palette Index Register */
#define DAC_DATA_REG          0x3C9  /* Color Palette Data Register */

/* GMBus Clock register*/
#define GCDGMBUS_OFFSET       0xCC  /* graphics clock frequency register for Gmbus*/
#define GCDGMBUS_VALUE        0x1BC  /* Eagle Lake graphics core clock frequency=444Mhz*/

/*-----------------------------------------------------------------------------
 * Attribute Controller Register Definitions
 *---------------------------------------------------------------------------*/
#define AR_PORT_LSB  0xC0  /*Attribute Controller Index Port LSB */

#define AR00  0x00  /* Color Data Register */
#define AR01  0x01	/* Color Data Register */
#define AR02  0x02	/* Color Data Register */
#define AR03  0x03	/* Color Data Register */
#define AR04  0x04	/* Color Data Register */
#define AR05  0x05	/* Color Data Register */
#define AR06  0x06	/* Color Data Register */
#define AR07  0x07	/* Color Data Register */
#define AR08  0x08	/* Color Data Register */
#define AR09  0x09	/* Color Data Register */
#define AR0A  0x0A	/* Color Data Register */
#define AR0B  0x0B	/* Color Data Register */
#define AR0C  0x0C	/* Color Data Register */
#define AR0D  0x0D	/* Color Data Register */
#define AR0E  0x0E	/* Color Data Register */
#define AR0F  0x0F	/* Color Data Register */
#define AR10  0x10	/* Mode Control Register */
#define AR11  0x11	/* Overscan Color Register */
#define AR12  0x12	/* Color Plane Enable Register */
#define AR13  0x13	/* Horizontal Pixel Panning Register */
#define AR14  0x14	/* Pixel Pad Register */


/*-----------------------------------------------------------------------------
 * CRT Controller Register Definitions
 *---------------------------------------------------------------------------*/
#define CR_PORT_LSB     0xD4  /* CRT Controller Index Port LSB */
#define CRT_3D4	        0x3D4 /* Color CRTC Index Port */
#define CRT_3B4	        0x3B4 /* Monochrome CRTC Index Port */

#define CR00            0x00  /* Horizontal Total Register */
#define CR01            0x01  /* Horizontal Display Enable End Reg */
#define CR02            0x02  /* Horizontal Blank Start Register */
#define CR03            0x03  /* Horizontal Blank End Register */
#define CR04            0x04  /* Horizontal Sync Start Register */
#define CR05            0x05  /* Horizontal Sync End Register */
#define CR06            0x06  /* Vertical Total Register */
#define CR07            0x07  /* Overflow Register */
#define CR08            0x08  /* Preset Row Scan Register */
#define CR09            0x09  /* Maximum Scan Line Register */
#define DOUBLE_SCANLINE	BIT7  /* Double scan ( 1 = Enable ) */
#define LCOMP_BIT9      BIT6  /* Bit 9 of line compare register */
#define VBLANK_BIT9     BIT5  /* Bit 9 of vertical blank start */
#define CR0A            0x0A  /* Cursor Start Scan Line Register */
#define CR0B            0x0B  /* Cursor End Scan Line Register */
#define CR0C            0x0C  /* Start Address High Register */
#define CR0D            0x0D  /* Start Address Low Register */
#define CR0E            0x0E  /* Cursor Location High Register */
#define CR0F            0x0F  /* Cursor Location Low Register */
#define CR10            0x10  /* Vertical Sync Start Register */
#define CR11            0x11  /* Vertical Sync End Register */
#define CR12            0x12  /* Vertical Display Enable End Reg */
#define CR13            0x13  /* Offset Register */
#define CR14            0x14  /* Underline Row Register */
#define CR15            0x15  /* Vertical Blank Start Register */
#define CR16            0x16  /* Vertical Blank End Register */
#define CR17            0x17  /* CRT Mode Control Register */
#define CR18            0x18  /* Line Compare Register */
#define CR22            0x22  /* Memory Data Latches Register */
#define CR24            0x24  /* Attribute Controller Toggle Reg */


/*-----------------------------------------------------------------------------
 * Graphics Controller Register Definitions
 *---------------------------------------------------------------------------*/
#define GR_PORT_LSB    0xCE   /* Graphics Controller Index Port LSB */

#define GR00           0x00   /* Set/Reset Register */
#define GR01           0x01   /* Enable Set/Reset Register */
#define GR02           0x02   /* Color Compare Register */
#define GR03           0x03   /* Data Rotate Register */
#define GR04           0x04   /* Read Map Select Register */
#define GR05           0x05   /* Graphics Mode Register */
#define GR06           0x06   /* Micsellaneous Register */
#define RANGE_MAP_MASK BIT3 + BIT2  /* Address range to map mask */
#define A0_BF_RANGE    000h   /* Map to A0000h-BFFFFh range */
#define GRAF_MODE      BIT0   /* 1 = Grahics mode, 0 = Text mode */
#define GR07           0x07   /* Color Don't Care Register */
#define GR08           0x08   /* Bit Mask Register */
#define GR10           0x10   /* Address Mapping */
#define PAGING_TARGET  BIT2 + BIT1 /* 00 = Local/Stolen, 01 = Memory mapped regs */
#define PAGE_MODE      BIT0   /* Page Map allow access to all FB mem */
#define GR11           0x11   /* Page Selector */
#define	GR18           0x18   /* Software Flag */


/*-----------------------------------------------------------------------------
 * Sequencer Register Definitions
 *---------------------------------------------------------------------------*/
#define SR_PORT_DATA      0x3C5 /* Sequencer Data Port */
#define SR_PORT_LSB       0xC4  /* Sequencer Index Port LSB */

#define SR00              0x00  /* Reset Register */
#define SR01              0x01  /* Clocking Mode Register */
#define DOT_CLOCK_DIVIDE  BIT3	/* Divide pixel clock by 2 */
#define SR02              0x02  /* Plane/Map Mask Register */
#define SR03              0x03  /* Character Font Register */
#define SR04              0x04  /* Memory Mode Register */
#define SR07              0x07  /* Horizontal Character Counter Reset */


/*-----------------------------------------------------------------------------
 * Memory mapped I/O Registers Definitions
 *---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
 * Instruction and Interrupt Control Registers (01000h - 02FFFh)
 *---------------------------------------------------------------------------*/
#define PGTBL_CTL         0x02020  /* Page Table Control Register */
#define PRB0_TAIL         0x02030  /* Primary RB 0 tail register */
#define PRB0_HEAD         0x02034  /* Primary RB 0 head register */
#define PRB0_START        0x02038  /* Primary RB 0 start register */
#define PRB0_CTL          0x0203C  /* Primary RB 0 control register */
#define HWS_PGA           0x02080  /* Hardware Status Page Address register */
#define HWSTAM            0x02098  /* Hardware Status Mask */
#define MI_RDRET_STATE    0x020FC  /* Memory Interface Read Return State */
#define CCID0             0x02180  /* Current Context ID 0 */

/*-----------------------------------------------------------------------------
 * GUNIT IOSF Sideband Access Register
 *---------------------------------------------------------------------------*/
#define SB_PACKET_REG     0x02100
#define SB_DATA_REG       0x02104
#define SB_ADDRESS_REG    0x02108
#define DPIO_RESET_REG    0x02110

/*-----------------------------------------------------------------------------
 * FENCE and Per Process GTT Control Registers (03000h - 031FFh)
 *
 * The FENCE registers are now 64-bits but we can only read/write 32-bits
 * at a time.  As a result, each register has aliases for the whole 64-bits,
 * the low DWORD, and the high DWORD.
 *
 * This is important for restoring the registers, since we must always write
 * the high DWORD first.
 * --------------------------------------------------------------------------*/
#define FENCE0            0x03000 /* Fence table registers */
#define FENCE0l           0x03000
#define FENCE0h           0x03004
#define FENCE1            0x03008
#define FENCE1l           0x03008
#define FENCE1h           0x0300C
#define FENCE2            0x03010
#define FENCE2l           0x03010
#define FENCE2h           0x03014
#define FENCE3            0x03018
#define FENCE3l           0x03018
#define FENCE3h           0x0301C
#define FENCE4            0x03020
#define FENCE4l           0x03020
#define FENCE4h           0x03024
#define FENCE5            0x03028
#define FENCE5l           0x03028
#define FENCE5h           0x0302C
#define FENCE6            0x03030
#define FENCE6l           0x03030
#define FENCE6h           0x03034
#define FENCE7            0x03038
#define FENCE7l           0x03038
#define FENCE7h           0x0303C
#define FENCE8            0x03040
#define FENCE8l           0x03040
#define FENCE8h           0x03044
#define FENCE9            0x03048
#define FENCE9l           0x03048
#define FENCE9h           0x0304C
#define FENCE10           0x03050
#define FENCE10l          0x03050
#define FENCE10h          0x03054
#define FENCE11           0x03058
#define FENCE11l          0x03058
#define FENCE11h          0x0305C
#define FENCE12           0x03060
#define FENCE12l          0x03060
#define FENCE12h          0x03064
#define FENCE13           0x03068
#define FENCE13l          0x03068
#define FENCE13h          0x0306C
#define FENCE14           0x03070
#define FENCE14l          0x03070
#define FENCE14h          0x03074
#define FENCE15           0x03078
#define FENCE15l          0x03078
#define FENCE15h          0x0307C

/*-----------------------------------------------------------------------------
 * MISC I/0 Contol Register ( 05000h - 05FFFh )
 *---------------------------------------------------------------------------*/
#define IO_OFF          0x05000         /* Register group offset */

#define IO00            0x05000         /* Hsync / Vsync control register */
#define GPIO0           0x05010         /* GPIO register 0 (DDC1) */
#define	DDC1_SCL_PIN    GPIO0_SCL_PIN   /* DDC1 SCL GPIO pin # */
#define	DDC1_SDA_PIN    GPIO0_SDA_PIN   /* DDC1 SDA CPIO pin # */
#define GPIO1           0x05014         /* GPIO register 1 (I2C) */
#define	I2C_SCL_PIN     GPIO1_SCL_PIN   /* I2C SCL GPIO pin # */
#define	I2C_SDA_PIN     GPIO1_SDA_PIN   /* I2C SDA CPIO pin # */
#define GPIO2           0x05018         /* GPIO register 2 (DDC2) */
#define	DDC2_SCL_PIN    GPIO2_SCL_PIN   /* DDC2 SCL GPIO pin # */
#define	DDC2_SDA_PIN    GPIO2_SDA_PIN   /* DDC2 SDA CPIO pin # */
#define GPIO3           0x0501C         /* GPIO register 3 (AGP mux DVI DDC) */
#define GPIO4           0x05020         /* GPIO register 4 (AGP mux I2C) */
#define GPIO5           0x05024         /* GPIO register 5 (AGP mux DDC2/I2C) */

#define GPIOPIN0        GPIO0
#define GPIOPIN1        GPIO0+1
#define GPIOPIN2        GPIO1
#define GPIOPIN3        GPIO1+1
#define GPIOPIN4        GPIO2
#define GPIOPIN5        GPIO2+1
#define GPIOPIN6        GPIO3
#define GPIOPIN7        GPIO3+1
#define GPIOPIN8        GPIO4
#define GPIOPIN9        GPIO4+1
#define GPIOPIN10       GPIO5
#define GPIOPIN11       GPIO5+1
#define GPIOPINMAX      12

#define GMBUS0          0x5100 /* GMBUS clock/device select register */
#define GMBUS1          0x5104 /* GMBUS command/status register */
#define GMBUS2          0x5108 /* GMBUS status register */
#define GMBUS3          0x510c /* GMBUS data buffer register */
#define GMBUS4          0x5110 /* GMBUS REQUEST_INUSE register */
#define GMBUS5          0x5120 /* GMBUS 2-Byte Index register */
#define GMBUS7          0x5134 /* GMBUS 7 */

#define	GTFIFOCTL		0x120008
#define GT_IOSFSB_READ_POLICY	BIT31

#define	CZCLK_CDCLK_FREQ_RATIO	0x6508
#define	GMBUSFREQ				0x6510

/*  GMBUS1 Bits */
#define SW_CLR_INT      BIT31
#define SW_RDY          BIT30
#define ENT             BIT29
#define STO             BIT27
#define ENIDX           BIT26
#define STA             BIT25

/*  GMBUS2 Bits */
#define INUSE           BIT15
#define HW_WAIT         BIT14
#define HW_TMOUT        BIT13
#define HW_INT          BIT12
#define HW_RDY          BIT11
#define HW_BUS_ERR      BIT10
#define GA              BIT9

typedef struct _sku_ctrl_reg_t{
	union{
		unsigned int reg;
		struct{
			unsigned int vco_clk		: 2;
			unsigned int reserved		: 28;
			unsigned int soc_sku		: 2;
		};
	};
}sku_ctrl_reg_t;

typedef struct _czclk_cdclk_freq_ratio_reg_t{
	union{
		unsigned int reg;
		struct{
			unsigned int czclk			: 4;
			unsigned int cdclk			: 5;
			unsigned int reserved		: 23;
		};
	};
}czclk_cdclk_freq_ratio_reg_t;

typedef enum
{
	SOC_200,
	SOC_200_1,
	SOC_266,
	SOC_333
} SKU_SOC;

typedef enum
{
	VCO_CLOCK_800M,
	VCO_CLOCK_1600M,
	VCO_CLOCK_2000M,
	VCO_CLOCK_2400M
} SKU_VCO_CLOCK;

/*-----------------------------------------------------------------------------
 * Clock Control and PM Register ( 06000h - 06FFFh )
 *---------------------------------------------------------------------------*/
#define VGA0_DIVISOR    0x06000  /* VGA 0  Divisor */
#define VGA1_DIVISOR    0x06004  /* VGA 1 Divisor */
#define DPLLACNTR       0x06014  /* Display PLL A Control */
#define DPLLBCNTR       0x06018  /* Display PLL B Control */
#define DPLLAMD         0x0601C  /* Display PLL A SDVO Multiplier/Divisor */
#define DPLLBMD         0x06020  /* Display PLL B SDVO Multiplier/Divisor */
#define FPA0            0x06040  /* DPLL A Divisor 0 */
#define FPA1            0x06044  /* DPLL A Divisor 1 */
#define FPB0            0x06048  /* DPLL B Divisor 0 */
#define FPB1            0x0604C  /* DPLL B Divisor 1 */

#define STD_DREFCLK     0x0 /* standard int ref clock for CRT and SDVO? */
#define DREFCLK         0x0
#define TVCLKINBC       0x4000
#define CLOCK_2X        0x40000000

#define RENCLK_GATE_D   0x06204  /* Clock Gating Display for Render 1 */
#define RENDCLK_GATE_D2 0x06208  /* Clock Gating Disable for Render 2 */

#define IVB_CHICKEN3    0x4200c
#define CHICKEN3_DGMG_REQ_OUT_FIX_DISABLE  (1 << 5)
#define CHICKEN3_DGMG_DONE_FIX_DISABLE     (1 << 2)
#define PCH_DSPCLK_GATE_D   0x42020
#define WM1_LP_ILK      0x45108
#define WM2_LP_ILK      0x4510c
#define WM3_LP_ILK      0x45110

#define DSP_ARB         0x70030
#define FW_BLC_AB       0x70034
#define FW_BLC_C        0x70038

#define   DISPPLANE_TRICKLE_FEED_DISABLE    (1<<14)

#define GEN6_UCGCTL1				0x9400
# define GEN6_BLBUNIT_CLOCK_GATE_DISABLE	(1 << 5)
# define GEN6_CSUNIT_CLOCK_GATE_DISABLE		(1 << 7)

#define GEN6_UCGCTL2				0x9404
# define GEN7_VDSUNIT_CLOCK_GATE_DISABLE	(1 << 30)
# define GEN7_TDLUNIT_CLOCK_GATE_DISABLE	(1 << 22)
# define GEN6_RCZUNIT_CLOCK_GATE_DISABLE	(1 << 13)
# define GEN6_RCPBUNIT_CLOCK_GATE_DISABLE	(1 << 12)
# define GEN6_RCCUNIT_CLOCK_GATE_DISABLE	(1 << 11)

#define GEN7_UCGCTL4				0x940c
#define  GEN7_L3BANK2X_CLOCK_GATE_DISABLE	(1<<25)


/*-----------------------------------------------------------------------------
 * Render Engine Configuration Register Definitions
 *---------------------------------------------------------------------------*/
/* GEN7 chicken */
#define GEN7_COMMON_SLICE_CHICKEN1		0x7010
# define GEN7_CSC1_RHWO_OPT_DISABLE_IN_RCC	((1<<10) | (1<<26))

#define GEN7_L3CNTLREG1				0xB01C
#define  GEN7_WA_FOR_GEN7_L3_CONTROL		0x3C4FFF8C

#define  GEN7_L3AGDIS                          (1<<19)
#define GEN7_L3SQCREG4                          0xb034

#define GEN7_L3_CHICKEN_MODE_REGISTER		0xB030
#define  GEN7_WA_L3_CHICKEN_MODE		0x20000000

#define GEN7_HALF_SLICE_CHICKEN1       0xe100 /* IVB GT1 + VLV */
#define GEN7_HALF_SLICE_CHICKEN1_IVB   0xf100

#define   GEN7_MAX_PS_THREAD_DEP               (8<<12)
#define   GEN7_PSD_SINGLE_PORT_DISPATCH_ENABLE (1<<3)

#define GEN7_ROW_CHICKEN2              0xe4f4
#define GEN7_ROW_CHICKEN2_GT2          0xf4f4
#define   DOP_CLOCK_GATING_DISABLE     (1<<0)

#define GFX_FLSH_CNTL_GEN6            0x101008


/* WaCatErrorRejectionIssue */
#define GEN7_SQ_CHICKEN_MBCUNIT_CONFIG		0x9030
#define  GEN7_SQ_CHICKEN_MBCUNIT_SQINTMOB	(1<<11)

#define CACHE_MODE_1            0x7004 /* IVB+ */
#define   PIXEL_SUBSPAN_COLLECT_OPT_DISABLE (1<<6)


/*-----------------------------------------------------------------------------
 * MBC Unit Space Register Definitions (9000h - 91B4h)
 *---------------------------------------------------------------------------*/
#define GEN6_MBCTL              0x0907c
#define   GEN6_MBCTL_ENABLE_BOOT_FETCH  (1 << 4)
#define   GEN6_MBCTL_CTX_FETCH_NEEDED   (1 << 3)
#define   GEN6_MBCTL_BME_UPDATE_ENABLE  (1 << 2)
#define   GEN6_MBCTL_MAE_UPDATE_ENABLE  (1 << 1)
#define   GEN6_MBCTL_BOOT_FETCH_MECH    (1 << 0)


/*-----------------------------------------------------------------------------
 * Display Palette Register Definitions (0A000h - 0AFFFh)
 *---------------------------------------------------------------------------*/
								  /* The register has same offset with GT Geyserville
 								   * registers.  Add VLV_DISPLAY_BASE 
 								   */		 
#define DPALETTE_A      0x0A000 + VLV_DISPLAY_BASE  /* Display Pipe A Palette */
#define DPALETTE_B      0x0A800 + VLV_DISPLAY_BASE /* Display Pipe B Palette */


/*-----------------------------------------------------------------------------
 * Display Pipeline / Port Register Definitions (60000h - 6FFFFh)
 *---------------------------------------------------------------------------*/
#define DP_OFFSET       0x60000  /* register group offset */
#define PIPEA_TIMINGS   0x60000
#define HTOTAL_A        0x60000  /* Pipe A Horizontal Total Register */
#define ACTIVE_DISPLAY  0x7FF    /* bit [ 10:0 ] */
#define HBLANK_A        0x60004  /* Pipe A Horizontal Blank Register */
#define HSYNC_A         0x60008  /* Pipe A Horizontal Sync Register */
#define VTOTAL_A        0x6000C  /* Pipe A Vertical Total Register */
#define VBLANK_A        0x60010  /* Pipe A Vertical Blank Register */
#define VSYNC_A         0x60014  /* Pipe A Vertical Sync Register */
#define PIPEASRC        0x6001C  /* Pipe A Source Image Size Register */
#define BCLRPAT_A       0x60020  /* Pipe A Border Color Pattern Register */
#define CRCCTRLREDA     0x60050  /* Pipe A CRC Red Control Register */
#define CRCCTRLGREENA   0x60054  /* Pipe A CRC Green Control Register */
#define CRCCTRLBLUEA    0x60058  /* Pipe A CRC Blue Control Register */
#define CRCCTRLRESA     0x6005C  /* Pipe A CRC Alpha Control Register */

#define PIPEB_TIMINGS   0x61000
#define HTOTAL_B        0x61000  /* Pipe B Horizontal Total Register */
#define HBLANK_B        0x61004  /* Pipe B Horizontal Blank Register */
#define HSYNC_B         0x61008  /* Pipe B Horizontal Sync Register */
#define VTOTAL_B        0x6100C  /* Pipe B Vertical Total Register */
#define VBLANK_B        0x61010  /* Pipe B Vertical Blank Register */
#define VSYNC_B         0x61014  /* Pipe B Vertical Sync Register */
#define PIPEBSRC        0x6101C  /* Pipe B Source Image Size Register */
#define BCLRPAT_B       0x61020  /* Pipe B Border Color Pattern Register */
#define CRCCTRLREDB     0x61050  /* Pipe B CRC Red Control Register */
#define CRCCTRLGREENB   0x61054  /* Pipe B CRC Green Control Register */
#define CRCCTRLBLUEB    0x61058  /* Pipe B CRC Blue Control Register */
#define CRCCTRLRESB     0x6105C  /* Pipe B CRC Alpha Control Register */

#define ADPACNTR        0x61100  /* Analog Display Port A Control */
#define CRTIO_DFX       0x61104  /* */

#define SDVOBCNTR       0x61140  /* Digital Display Port B Control */
#define SDVOCCNTR       0x61160  /* Digital Display Port C Control */
#define LVDSCNTR        0x61180  /* Digital Display Port Control */
#define DPBCNTR         0x64100  /* Display Port B Control */
#define DPCCNTR         0x64200  /* Display Port C Control */

#define DPBAUXCNTR      0x64110  /* Display Port B AUX Channel Control */
#define DPCAUXCNTR      0x64210  /* Display Port C AUX Channel Control */



/* CRT port related registers */

/* Panel Power Sequencing */
#define LVDS_PNL_PWR_STS  0x61200  /* LVDS Panel Power Status Register */
#define LVDS_PNL_PWR_CTL  0x61204  /* LVDS Panel Power Control Register */
#define PP_ON_DELAYS      0x61208  /* Panel Power On Sequencing Delays */
#define PP_DIVISOR        0x61210  /* Panel Power Cycle Delay and Reference */

/* Panel Fitting */
#define PFIT_CONTROL      0x61230  /* Panel Fitting Control */
#define PFIT_PGM_RATIOS   0x61234  /* Programmed Panel Fitting Ratios */
#define PFA_CTL_1         0x68080
#define PFB_CTL_1         0x68880
#define PF_ENABLE         (1<<31)

/* Backlight control */
#define BLC_PWM_CTL2      0x61250  /* Backlight PWM Control 2 */
#define BLC_PWM_CTL       0x61254  /* Backlight PWM Control */
#define BLM_HIST_CTL      0x61260  /* Image BLM Histogram Control */

#define PORT_EN           BIT31
#define PORT_PIPE_SEL     BIT30
#define PORT_PIPE_SEL_POS 30
#define PORT_PIPE_A       0      /* 0 = Pipe A */
#define PORT_PIPE_B       BIT30  /* 1 = Pipe B */
#define STALL_MASK        BIT29 + BIT28
#define STALL_ENABLE      BIT28
#define SYNC_MASK         BIT15 + BIT11 + BIT10 + BIT4 + BIT3
#define SYNC_POLARITY     BIT15  /* 1 = Use VGA register */
#define VSYNC_OUTPUT      BIT11
#define HSYNC_OUTPUT      BIT10
#define VSYNC_POLARITY    BIT4
#define HSYNC_POLARITY    BIT3
#define FP_DATA_ORDER     BIT14
#define SUBDATA_ORDER     BIT6
#define BORDER_EN         BIT7
#define DISPLAY_EN        BIT2
#define INTERLACED_BIT    0x00100000
#define RGBA_BITS         0x00030000


#define	TRANS_A_DATA_M1		0x60030
#define	TRANS_A_DATA_N1		0x60034
#define	TRANS_A_DATA_M2		0x60038
#define	TRANS_A_DATA_N2		0x6003C

#define	TRANS_A_DPLINK_M1	0x60040
#define	TRANS_A_DPLINK_N1	0x60044
#define	TRANS_A_DPLINK_M2	0x60048
#define	TRANS_A_DPLINK_N2	0x6004C


#define	TRANS_B_DATA_M1		0x61030
#define	TRANS_B_DATA_N1		0x61034
#define	TRANS_B_DATA_M2		0x61038
#define	TRANS_B_DATA_N2		0x6103C

#define	TRANS_B_DPLINK_M1	0x61040
#define	TRANS_B_DPLINK_N1	0x61044
#define	TRANS_B_DPLINK_M2	0x61048
#define	TRANS_B_DPLINK_N2	0x6104C




/*-----------------------------------------------------------------------------
 * Display Pipeline A Register ( 70000h - 70024h )
 *---------------------------------------------------------------------------*/
#define PIPEA_SCANLINE_COUNT   0x70000  /* Pipe A Scan Line Count (RO) */
#define PIPEA_SCANLINE_COMPARE 0x70004  /* Pipe A SLC Range Compare (RO) */
#define PIPE_STATUS_OFFSET     0x1C
#define PIPEAGCMAXRED          0x70010  /* Pipe A Gamma Correct. Max Red */
#define PIPEAGCMAXGRN          0x70014  /* Pipe A Gamma Correct. Max Green */
#define PIPEAGCMAXBLU          0x70018  /* Pipe A Gamma Correct. Max Blue */
#define PIPEA_STAT             0x70024  /* Pipe A Display Status */
#define PIPEA_DISP_ARB_CTRL    0x70030  /* Display Arbitration Control */
#define PIPEA_FRAME_HIGH       0x70040 /*Pipe A Frame Count High*/
#define PIPEA_FRAME_PIXEL      0x70044 /*Pipe A Frame Count Low and Pixel Count*/

#define PIPE_PIXEL_MASK	       0x00ffffff
#define PIPE_FRAME_HIGH_MASK   0x0000ffff
#define PIPE_FRAME_LOW_MASK    0xff000000
#define PIPE_FRAME_LOW_SHIFT   24

/* following bit flag defs can be re-used for Pipe-B */
#define PIPE_ENABLE       BIT31
#define PIPE_LOCK         BIT25
#define GAMMA_MODE        BIT24
#define HOT_PLUG_EN       BIT26
#define VSYNC_STS_EN      BIT25
#define INTERLACE_EN	  BIT23|BIT22
#define VBLANK_STS_EN     BIT17
#define HOT_PLUG_STS      BIT10
#define VSYNC_STS         BIT9
#define VBLANK_STS        BIT1

/*-----------------------------------------------------------------------------
 * Hardware Cursor Register Definitions (70080h - 7009Ch)
 *---------------------------------------------------------------------------*/
#define CUR_A_CNTR        0x70080  /*Cursor A Control */
#define CUR_B_CNTR        0x700C0
#define CUR_BASE_OFFSET   0x4
#define CUR_POS_OFFSET    0x8
#define CUR_PAL0_OFFSET   0x10
#define CUR_PAL1_OFFSET   0x14
#define CUR_PAL2_OFFSET   0x18
#define CUR_PAL3_OFFSET   0x1C

/* Define these for ease of reference */
#define CURSOR_A_BASE     CUR_A_CNTR + CUR_BASE_OFFSET
#define CURSOR_A_POS      CUR_A_CNTR + CUR_POS_OFFSET
#define CURSOR_A_PAL0     CUR_A_CNTR + CUR_PAL0_OFFSET
#define CURSOR_A_PAL1     CUR_A_CNTR + CUR_PAL1_OFFSET
#define CURSOR_A_PAL2     CUR_A_CNTR + CUR_PAL2_OFFSET
#define CURSOR_A_PAL3     CUR_A_CNTR + CUR_PAL3_OFFSET
#define CURSOR_B_BASE     CUR_B_CNTR + CUR_BASE_OFFSET
#define CURSOR_B_POS      CUR_B_CNTR + CUR_POS_OFFSET
#define CURSOR_B_PAL0     CUR_B_CNTR + CUR_PAL0_OFFSET
#define CURSOR_B_PAL1     CUR_B_CNTR + CUR_PAL1_OFFSET
#define CURSOR_B_PAL2     CUR_B_CNTR + CUR_PAL2_OFFSET
#define CURSOR_B_PAL3     CUR_B_CNTR + CUR_PAL3_OFFSET

/*-----------------------------------------------------------------------------
 * Display Plane A Register Definitions (70180h - 70188h)
 *---------------------------------------------------------------------------*/
#define DSPACNTR        0x70180  /* Display Plane A */
#define DSPALINOFF      0x70184  /* Display A Linear Offset */
#define DSPASTRIDE      0x70188  /* Display A Stride */
#define DSPAKEYVAL      0x70194  /* Sprite color key value */
#define DSPAKEYMASK     0x70198  /* Sprite color key mask */
#define DSPASURF        0x7019C  /* Display A Suface base address */
#define DSPATILEOFF     0x701A4  /* Display A Tiled Offset */

/* Display Plane pixel format reg bits. These 4bits need to be
 * shifted << 26. (Plane Ctrl register are 29:26). Applies to
 * Display A and Display B
 */

#define DSP_PF_MASK     0x3C000000
#define DSP_PF_RGB8_IDX 0x08000000	/*(0x2 << 26)*/
#define DSP_PF_RGB565   0x14000000	/*(0x5 << 26)*/
#define DSP_PF_xRGB32   0x18000000  /*(0x6 << 26)*/
#define DSP_PF_ARGB32   0x1C000000  /*(0x7 << 26)*/
#define DSP_PF_xBGR32   0x38000000  /*(0xE << 26)*/
#define DSP_PF_ABGR32   0x3C000000  /*(0xF << 26) */
#define DSP_PF_xBGR2101010  0x20000000 /*(0x8 << 26)*/
#define DSP_PF_ABGR2101010  0x24000000 /*(0x9 << 26)*/
/*this is values does not supported in sprite, only in display*/
#define DSP_PF_xRGB2101010   (0xA << 26)
#define DSP_PF_ARGB2101010   (0xB << 26)
#define DSP_PF_xBGR16161616  (0xC << 26)
#define DSP_PF_ABGR16161616  (0xD << 26)

/* Using BIT 26 to indicate ALPHA channel in display plane */
#define DSP_ALPHA BIT26

/*-----------------------------------------------------------------------------
 * VBIOS Software flags  00h - 0Fh
 *---------------------------------------------------------------------------*/
#define KFC             0x70400  /* Chicken Bit */
#define SWFABASE        0x70410  /* Software flags A Base Addr */
#define SWF00           0x70410
#define SWF01           0x70414
#define SWF02           0x70418
#define SWF03           0x7041C
#define SWF04           0x70420
#define SWF05           0x70424
#define SWF06           0x70428
#define SWF07           0x7042C
#define SWF08           0x70430
#define SWF09           0x70434
#define SWF0A           0x70438
#define SWF0B           0x7043C
#define SWF0C           0x70440
#define SWF0D           0x70444
#define SWF0E           0x70448
#define SWF0F           0x7044C

/*-----------------------------------------------------------------------------
 * Display Pipeline B Register ( 71000h - 71024h )
 *---------------------------------------------------------------------------*/
#define PIPEB_SCANLINE_COUNT   0x71000 /* Pipe B Disp Scan Line Count Register */
#define PIPEB_SCANLINE_COMPARE 0x71004 /* Pipe B Disp Scan Line Count Range Compare */
#define PIPEBGCMAXRED          0x71010 /* Pipe B Gamma Correct. Max Red */
#define PIPEBGCMAXGRN          0x71014 /* Pipe B Gamma Correct. Max Green */
#define PIPEBGCMAXBLU          0x71018 /* Pipe B Gamma Correct. Max Blue */
#define PIPEB_STAT             0x71024 /* Display Status Select Register */
#define PIPEB_FRAME_HIGH       0x71040 /*Pipe B Frame Count High*/
#define PIPEB_FRAME_PIXEL      0x71044 /*Pipe B Frame Count Low and Pixel Count*/


#define VBLANK_EVN_STS_EN   BIT20
#define VBLANK_ODD_STS_EN   BIT21
#define VBLANK_EVN_STS      BIT4
#define VBLANK_ODD_STS      BIT5


/*-----------------------------------------------------------------------------
 * Display Plane B Register Definitions (71180h - 71188h)
 *---------------------------------------------------------------------------*/
#define DSPBCNTR        0x71180  /* Display Plane B */
#define DSPBLINOFF      0x71184  /* Display B Linear Offset */
#define DSPBSTRIDE      0x71188  /* Display B Stride */
#define DSPBKEYVAL      0x71194  /* Sprite color key value */
#define DSPBKEYMASK     0x71198  /* Sprite color key mask */
#define DSPBSURF        0x7119C  /* Display B Suface base address */
#define DSPBTILEOFF     0x711A4  /* Display B Tiled Offset */


/*-----------------------------------------------------------------------------
 * VBIOS Software flags  10h - 1Fh
 *---------------------------------------------------------------------------*/
#define SWFBBASE        0x71410  /* Software flags B Base Addr */
#define SWF10           0x71410
#define SWF11           0x71414
#define SWF12           0x71418
#define SWF13           0x7141C
#define SWF14           0x71420
#define SWF15           0x71424
#define SWF16           0x71428
#define SWF17           0x7142C
#define SWF18           0x71430
#define SWF19           0x71434
#define SWF1A           0x71438
#define SWF1B           0x7143C
#define SWF1C           0x71440
#define SWF1D           0x71444
#define SWF1E           0x71448
#define SWF1F           0x7144C


/*-----------------------------------------------------------------------------
 * Display Video Sprite A Register Definitions (72180h - 721FFh)
 *---------------------------------------------------------------------------*/
#define SPACNTR        0x72180  /* Sprite A */
#define SPALINOFF      0x72184  /* Sprite A Linear Offset */
#define SPASTRIDE      0x72188  /* Sprite A Stride */
#define SPAPOS         0x7218C  /* Sprite A Position */
#define SPASIZE        0x72190  /* Sprite A Size */
#define SPAKEYMINVAL   0x72194  /* Sprite A color key Min */
#define SPAKEYMASK     0x72198  /* Sprite A color key mask */
#define SPASURF        0x7219C  /* Sprite A Suface base addr */
#define SPAKEYMAXVAL   0x721A0  /* Sprite A color key Max */
#define SPATILEOFF     0x721A4  /* Sprite A Tiled Offset */
#define SPACONSTALPHA  0x721A8  /* Sprite A Constant Alpha */
#define SPACLRC0       0x721D0  /* Sprite A Color Correction 0 */
#define SPACLRC1       0x721D4  /* Sprite A Color Correction 1 */
#define SPAGAMC0       0x721F4  /* Sprite A Gamma Correction 0 */
#define SPAGAMC1       0x721F0  /* Sprite A Gamma Correction 1 */
#define SPAGAMC2       0x721EC  /* Sprite A Gamma Correction 2 */
#define SPAGAMC3       0x721E8  /* Sprite A Gamma Correction 3 */
#define SPAGAMC4       0x721E4  /* Sprite A Gamma Correction 4 */
#define SPAGAMC5       0x721E0  /* Sprite A Gamma Correction 5 */

/*-----------------------------------------------------------------------------
 * Display Video Sprite B Register Definitions (72280h - 722FFh)
 *---------------------------------------------------------------------------*/
#define SPBCNTR        0x72280  /* Sprite B */
#define SPBLINOFF      0x72284  /* Sprite B Linear Offset */
#define SPBSTRIDE      0x72288  /* Sprite B Stride */
#define SPBPOS         0x7228C  /* Sprite B Position */
#define SPBSIZE        0x72290  /* Sprite B Size */
#define SPBKEYMINVAL   0x72294  /* Sprite B color key Min */
#define SPBKEYMASK     0x72298  /* Sprite B color key mask */
#define SPBSURF        0x7229C  /* Sprite B Suface base addr */
#define SPBKEYMAXVAL   0x722A0  /* Sprite B color key Max */
#define SPBTILEOFF     0x722A4  /* Sprite B Tiled Offset */
#define SPBCONSTALPHA  0x722A8  /* Sprite B Constant Alpha */
#define SPBCLRC0       0x722D0  /* Sprite B Color Correction 0 */
#define SPBCLRC1       0x722D4  /* Sprite B Color Correction 1 */
#define SPBGAMC0       0x722F4  /* Sprite B Gamma Correction 0 */
#define SPBGAMC1       0x722F0  /* Sprite B Gamma Correction 1 */
#define SPBGAMC2       0x722EC  /* Sprite B Gamma Correction 2 */
#define SPBGAMC3       0x722E8  /* Sprite B Gamma Correction 3 */
#define SPBGAMC4       0x722E4  /* Sprite B Gamma Correction 4 */
#define SPBGAMC5       0x722E0  /* Sprite B Gamma Correction 5 */

/*-----------------------------------------------------------------------------
 * Display Video Sprite C Register Definitions (72380h - 723FFh)
 *---------------------------------------------------------------------------*/
#define SPCCNTR        0x72380  /* Sprite C */
#define SPCLINOFF      0x72384  /* Sprite C Linear Offset */
#define SPCSTRIDE      0x72388  /* Sprite C Stride */
#define SPCPOS         0x7238C  /* Sprite C Position */
#define SPCSIZE        0x72390  /* Sprite C Size */
#define SPCKEYMINVAL   0x72394  /* Sprite C color key Min */
#define SPCKEYMASK     0x72398  /* Sprite C color key mask */
#define SPCSURF        0x7239C  /* Sprite C Suface base addr */
#define SPCKEYMAXVAL   0x723A0  /* Sprite C color key Max */
#define SPCTILEOFF     0x723A4  /* Sprite C Tiled Offset */
#define SPCCONSTALPHA  0x723A8  /* Sprite C Constant Alpha */
#define SPCCLRC0       0x723D0  /* Sprite C Color Correction 0 */
#define SPCCLRC1       0x723D4  /* Sprite C Color Correction 1 */
#define SPCGAMC0       0x723F4  /* Sprite C Gamma Correction 0 */
#define SPCGAMC1       0x723F0  /* Sprite C Gamma Correction 1 */
#define SPCGAMC2       0x723EC  /* Sprite C Gamma Correction 2 */
#define SPCGAMC3       0x723E8  /* Sprite C Gamma Correction 3 */
#define SPCGAMC4       0x723E4  /* Sprite C Gamma Correction 4 */
#define SPCGAMC5       0x723E0  /* Sprite C Gamma Correction 5 */

/*-----------------------------------------------------------------------------
 * Display Video Sprite D Register Definitions (72480h - 724FFh)
 *---------------------------------------------------------------------------*/
#define SPDCNTR        0x72480  /* Sprite D */
#define SPDLINOFF      0x72484  /* Sprite D Linear Offset */
#define SPDSTRIDE      0x72488  /* Sprite D Stride */
#define SPDPOS         0x7248C  /* Sprite D Position */
#define SPDSIZE        0x72490  /* Sprite D Size */
#define SPDKEYMINVAL   0x72494  /* Sprite D color key Min */
#define SPDKEYMASK     0x72498  /* Sprite D color key mask */
#define SPDSURF        0x7249C  /* Sprite D Suface base addr */
#define SPDKEYMAXVAL   0x724A0  /* Sprite D color key Max */
#define SPDTILEOFF     0x724A4  /* Sprite D Tiled Offset */
#define SPDCONSTALPHA  0x724A8  /* Sprite D Constant Alpha */
#define SPDCLRC0       0x724D0  /* Sprite D Color Correction 0 */
#define SPDCLRC1       0x724D4  /* Sprite D Color Correction 1 */
#define SPDGAMC0       0x724F4  /* Sprite D Gamma Correction 0 */
#define SPDGAMC1       0x724F0  /* Sprite D Gamma Correction 1 */
#define SPDGAMC2       0x724EC  /* Sprite D Gamma Correction 2 */
#define SPDGAMC3       0x724E8  /* Sprite D Gamma Correction 3 */
#define SPDGAMC4       0x724E4  /* Sprite D Gamma Correction 4 */
#define SPDGAMC5       0x724E0  /* Sprite D Gamma Correction 5 */

#define _SPRITE(spr_indx, a) (a + (spr_indx*0x100))
#define SP_CNTR(spr_indx)        _SPRITE(spr_indx, SPACNTR)
#define SP_LINOFF(spr_indx)      _SPRITE(spr_indx, SPALINOFF)
#define SP_STRIDE(spr_indx)      _SPRITE(spr_indx, SPASTRIDE)
#define SP_POS(spr_indx)         _SPRITE(spr_indx, SPAPOS)
#define SP_SIZE(spr_indx)        _SPRITE(spr_indx, SPASIZE)
#define SP_KEYMINVAL(spr_indx)   _SPRITE(spr_indx, SPAKEYMINVAL)
#define SP_KEYMASK(spr_indx)     _SPRITE(spr_indx, SPAKEYMASK)
#define SP_SURF(spr_indx)        _SPRITE(spr_indx, SPASURF)
#define SP_KEYMAXVAL(spr_indx)   _SPRITE(spr_indx, SPAKEYMAXVAL)
#define SP_TILEOFF(spr_indx)     _SPRITE(spr_indx, SPATILEOFF)
#define SP_CONSTALPHA(spr_indx)  _SPRITE(spr_indx, SPACONSTALPHA)
#define SP_CLRC0(spr_indx)       _SPRITE(spr_indx, SPACLRC0)
#define SP_CLRC1(spr_indx)       _SPRITE(spr_indx, SPACLRC1)
#define SP_GAMC0(spr_indx)       _SPRITE(spr_indx, SPAGAMC0)
#define SP_GAMC1(spr_indx)       _SPRITE(spr_indx, SPAGAMC1)
#define SP_GAMC2(spr_indx)       _SPRITE(spr_indx, SPAGAMC2)
#define SP_GAMC3(spr_indx)       _SPRITE(spr_indx, SPAGAMC3)
#define SP_GAMC4(spr_indx)       _SPRITE(spr_indx, SPAGAMC4)
#define SP_GAMC5(spr_indx)       _SPRITE(spr_indx, SPAGAMC5)

#define PLANE_ENABLE    BIT31
#define GAMMA_ENABLE    BIT30
#define BPP_MASK        BIT29 + BIT28 + BIT27 + BIT26
#define BPP_POS         26
#define STEREO_ENABLE   BIT25
#define PIPE_SEL        BIT24
#define PIPE_SEL_POS    24
#define PIXEL_MULTIPLY  BIT21 + BIT20
#define STEREO_POLARITY BIT18
#define KEY_ENABLE	BIT22

/* Common offsets for all Display Pipeline registers */
#define DSP_LINEAR_OFFSET 0x04  /* Offset from the control reg */
#define DSP_STRIDE_OFFSET 0x08  /* Offset from the control reg */
#define DSP_SIZE_OFFSET   0x10  /* Offset from the control reg */
#define DSP_START_OFFSET  0x1c  /* Offset from the control reg */
#define DSP_TOFF_OFFSET   0x24  /* Offset from the control reg */



/*-----------------------------------------------------------------------------
 * VGA Display Plane Control Register Definitions (71400h)
 *---------------------------------------------------------------------------*/
#define VP00             0x71400
#define VGACNTRL         0x71400  /* VGA Display Plane Control Register */
#define VGA_DOUBLE       BIT30
#define VGA_PIPE         BIT29
#define VGA_CENTER_MASK  BIT25 + BIT24
#define VGA_CENTER_1     BIT25
#define VGA_CENTER_0     BIT24
#define VGA_PAL_READ     BIT23
#define VGA_PAL_MASK     BIT22 + BIT21
#define VGA_PALA_DISABLE BIT22
#define VGA_PALB_DISABLE BIT21
#define DAC_8_BIT	     BIT20
#define VGA_8_DOT	     BIT18

#define ADD_ID           0x71408  /* ADD Card ID Register*/

#define OVADD            0x30000  /* Overlay Control */

/*-----------------------------------------------------------------------------
 * Software Flag Registers (71410h - 71428h)
 *---------------------------------------------------------------------------*/

/* Map old software flag names to their new Gen7 names */
#define SF00  SWF10
#define SF01  SWF11
#define SF02  SWF12
#define SF03  SWF13
#define SF04  SWF14
#define SF05  SWF15
#define SF06  SWF16


/*-----------------------------------------------------------------------------
 * PCI Registers Definitions
 *---------------------------------------------------------------------------*/
#define PCI_DEV_CONTROL_REG  0x04		 /* Device control register */
#define PCI_LINEAR_BASE_REG  0x10		 /* Graphics memory base register */
#define LINEAR_BASE_MEM_MASK 0x0FC000000 /* Mask */
#define PCI_MMIO_BASE_REG    0x14        /* Memory mapped I/O base register */
#define MMIO_BASE_MEM_MASK   0x0FFF80000 /* Mask */
#define PCI_SUB_VID_REG	     0x2C        /* Offset subvendor VID in PCI space */
#define PCI_SUB_DID_REG      0x2E        /* Offset subvendor DID in PCI space */
#define PCI_REV_ID_REG       0x2E        /* Offset revison ID in PCI space */
#define PCI_SMRAM_REG        0x70        /* SMRAM in device 0 space */
#define PCI_STOLEN_MEM_MASK  BIT7 + BIT6 /* Stolen memory allocated for VGA */
#define PCI_STOLEN_MEM_512K  BIT7
#define PCI_STOLEN_MEM_1M	 BIT7 + BIT6

/*-----------------------------------------------------------------------------
 * ID's for use with table driven register loading mechanism
 ----------------------------------------------------------------------------*/

#define BDA_INFO_ID          0xFA /* ID to skip Bios Data Area information */
#define CODE_UPDATE_ID       0xFB /* ID for Conditional Update Registers */
#define INDEX_PORT_ID        0xFC /* ID for Non-Indexed Registers */
#define INDEX_PORT_MASK_ID   0xFD /* ID for Non-Indexed Registers with mask */
#define NON_INDEX_PORT_ID    0xFE /* ID for Non-Indexed Registers */
#define END_TABLE_ID         0xFF /* ID for start of a register table */

/* Code for Table loading for util_Load_MMIO_Tables function. */

#define TABLE_END            0xFFFFFFFF  /* End of table */
#define TABLE_MMIO           0xFFFFFFFC  /* MMIO register table */
#define TABLE_MMIO_BITS      0xFFFFFFFB  /* MMIO register table with mask bits */



/*-----------------------------------------------------------------------------
 * Scratch register to be used as additional parameter in SMI
 *---------------------------------------------------------------------------*/
#define SCRATCH_SWF6         0x71428

/*-----------------------------------------------------------------------------
 * Miscellaneous registers
 ----------------------------------------------------------------------------*/
#define HW_ST_PAGE_ADDR      0x02080
#define GUNIT_CLK_GATING_DISABLE1 0x182060
#define GUNIT_CLK_GATING_DISABLE2 0x182064


/*
 * Ring buffer base and offsets
 */
#define RINGBUF 0x2030
#define PRIMARY_0   0

/*-----------------------------------------------------------------------------
 * GMBUS : I2C Bus Types
 ----------------------------------------------------------------------------*/



/* definitions for GMBUS conversion from IGD_PARAM_GPIO_PAIR
 * to Register Port Select bits - see BITS 2:0 of GMBUS0 register*/
#define GMBUS_PINS_NONE         0x0 /* NONE */
#define GMBUS_PINS_ANALOG       0x2 /* Analog */
#define GMBUS_PINS_DP_HDMI_C	0x4 /* DP/DVI-HDMI Port C*/
#define GMBUS_PINS_DP_HDMI_B    0x5 /* DP/DVI-HDMI Port B */

/* definitions for GPIO conversion from IGD_PARAM_GPIO_PAIR
 * to GPIO Control Register Offsets - see GPIO_CTL register array*/
#define GPIO_OFFSET_ANALOG       0x5010 /* LVDS */
#define GPIO_OFFSET_DP_HDMI_C	 0x501C /* DP/DVI-HDMI Port C*/
#define GPIO_OFFSET_DP_HDMI_B    0x5020 /* SDVO / DVI-HDMI B */

#define  FORCEWAKE              0xA18C
#define  GTLC_WAKE_CTRL         0x130090
#define  FORCEWAKE_REQ_RENDR_VLV 0x1300b0
#define  FORCEWAKE_ACK_RENDR_VLV 0x1300b4
#define  FORCEWAKE_REQ_MEDIA_VLV 0x1300b8
#define  FORCEWAKE_ACK_MEDIA_VLV 0x1300bc

/* drain latency register values*/
#define DRAIN_LATENCY_PRECISION_32  32
#define DRAIN_LATENCY_PRECISION_16  16
#define VLV_DDL1            0x70050
#define DDL_CURSORA_PRECISION_32    (1<<31)
#define DDL_CURSORA_PRECISION_16    (0<<31)
#define DDL_CURSORA_SHIFT       24
#define DDL_PLANEA_PRECISION_32     (1<<7)
#define DDL_PLANEA_PRECISION_16     (0<<7)
#define VLV_DDL2            0x70054
#define DDL_CURSORB_PRECISION_32    (1<<31)
#define DDL_CURSORB_PRECISION_16    (0<<31)
#define DDL_CURSORB_SHIFT       24
#define DDL_PLANEB_PRECISION_32     (1<<7)
#define DDL_PLANEB_PRECISION_16     (0<<7)

/* FIFO watermark sizes etc */
#define G4X_FIFO_LINE_SIZE		64
#define VALLEYVIEW_FIFO_SIZE    255
#define G4X_FIFO_SIZE			127
#define VALLEYVIEW_MAX_WM		0xff
#define VALLEYVIEW_CURSOR_MAX_WM 64
#define I965_CURSOR_FIFO		64
#define I965_CURSOR_DFT_WM		8
#define FW_BLC_SELF_VLV 0x6500 /* VLV only */
#define   FW_BLC_SELF_EN_VLV           (1<<15) /* 945 only */

#define DSPFW1          0x70034
#define   DSPFW_SR_SHIFT    23
#define   DSPFW_SR_MASK     (0x1ff<<23)
#define   DSPFW_CURSORB_SHIFT   16
#define   DSPFW_CURSORB_MASK    (0x3f<<16)
#define   DSPFW_PLANEB_SHIFT    8
#define   DSPFW_PLANEB_MASK (0x7f<<8)
#define   DSPFW_PLANEA_MASK (0x7f)
#define DSPFW2          0x70038
#define   DSPFW_CURSORA_MASK    0x00003f00
#define   DSPFW_CURSORA_SHIFT   8
#define   DSPFW_PLANEC_MASK (0x7f)
#define DSPFW3          0x7003c
#define   DSPFW_HPLL_SR_EN  (1<<31)
#define   DSPFW_CURSOR_SR_SHIFT 24
#define   PINEVIEW_SELF_REFRESH_EN  (1<<30)
#define   DSPFW_CURSOR_SR_MASK      (0x3f<<24)
#define   DSPFW_HPLL_CURSOR_SHIFT   16
#define   DSPFW_HPLL_CURSOR_MASK    (0x3f<<16)
#define   DSPFW_HPLL_SR_MASK        (0x1ff)

#endif /* _REGS_H_ */

/*----------------------------------------------------------------------------
 * File Revision History
 * $Id: regs.h,v 1.1.2.22 2012/06/01 10:21:01 aalteres Exp $
 * $Source: /nfs/sie/disks/sie-cidgit_disk001/git_repos/GFX/cvs_fri22/koheo/linux/egd_drm/emgd/include/gn7/Attic/regs.h,v $
 *----------------------------------------------------------------------------
 */
