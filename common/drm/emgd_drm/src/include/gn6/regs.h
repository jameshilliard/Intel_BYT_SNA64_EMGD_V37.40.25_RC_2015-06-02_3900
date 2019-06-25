/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: regs.h
 *-----------------------------------------------------------------------------
 * Copyright (c) 2002-2013, Intel Corporation.
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

#ifndef _SNB_REGS_H_
#define _SNB_REGS_H_


/*-----------------------------------------------------------------------------
 * Useful utility MACROSs
 *---------------------------------------------------------------------------*/
#define _MASKED_BIT_ENABLE(a) (((a) << 16) | (a))
#define _MASKED_BIT_DISABLE(a) ((a) << 16)


/*-----------------------------------------------------------------------------
 * MMIO REGISTER and GTT Table SIZE
 *---------------------------------------------------------------------------*/
#define DEVICE_MMIO_SIZE            0x80000 /* 512KB */
#define DEVICE_GTTADR_SIZE          0x80000 /* 512KB */
#define DEVICE_CTG_GTTADR_SIZE     0x100000 /* 1MB */


/*-----------------------------------------------------------------------------
 * PCI Register Definitions / Device #0
 *---------------------------------------------------------------------------*/
#define INTEL_OFFSET_VGA_RID        0x08

/* FIXME: These are both 64 bit regs, but using as a 32 bit reg */
#define INTEL_OFFSET_VGA_MMADR      0x10
#define INTEL_OFFSET_VGA_GMADR      0x18

#define INTEL_OFFSET_VGA_IOBAR      0x20
#define INTEL_OFFSET_VGA_MSAC_SNB   0x62
#define INTEL_OFFSET_VGA_CORECLK    0xF0

#define INTEL_OFFSET_BRIDGE_RID     0x08
#define INTEL_OFFSET_BRIDGE_CAPREG  0xE0
#define INTEL_OFFSET_BRIDGE_MMADR   0x14

#define PCI_CAPREG_4      0x44 /* Capability Identification Reg[39:31] */
#define PCI_MOBILE_BIT    BIT0

#define PCI_GCC1_REG      0x50 /* GMCH Control Register #1 */
#define PCI_BSM_REG       0x5C /* GMCH Base of Stolen Memory register */
#define PCI_GMS_MASK      BIT6 + BIT5 + BIT4 /* GFX Mode Select Bits Mask */
#define PCI_LOCAL		  BIT4 /* Local memory enabled */
#define PCI_DVMT_512K     BIT5 /* 512KB DVMT */
#define PCI_DVMT_1M       BIT5 + BIT4 /* 1MB DVMT */
#define PCI_DVMT_8M       BIT6 /* 8MB DVMT */

#define PCI_DRB_REG       0x60 /* DRAM row boundary Register */
#define PCI_DRC_REG       0x7C /* DRAM Controller Mode Register */
#define PCI_DT_MASK       BIT0 + BIT1  /* Select SDRAM types.
                                        *  = 00:  Single data rate SDRAM
                                        *  = 01:  Dual data rate SDRAM
                                        *  = Other:  Reserved
                                        */
#define DT_SDR_SDRAM      00   /* Single data rate SDRAM */
#define DT_DDR_SDRAM      01   /* Dual data rate SDRAM */

#define PCI_ESMRAMC_REG   0x91 /* Extended System Management RAM Reg */
#define PCI_TSEG_SZ		  BIT1 /* TSEG size bit */
#define PCI_TSEG_512K	  0    /* 512K TSEG */
#define PCI_TSEG_1M       BIT1 /* 1MB TSEG */

#define PCI_GCLKIO_REG    0xC0 /* GMCH Clock and IO Control Register */
#define PCI_AGP_Bit       BIT9 /* AGP/DVO Mux Select:
								*  = 0, DVO/ZV
								*  = 1, AGP
								*/
#define PCI_GMCHCFG_REG   0xC6 /* GMCH Configuration Register */
#define PCI_SMFREQ_MASK   BIT10 + BIT11
						   /* System Mem Frequency Select
                            * = 00:  Intel Reserved
                            * = 01:  System Memory Frequency is 166Mhz (DDR333) - Intel Reserved
                            * = 10:  System Memory Frequency is 133Mhz (SDR133, DDR266)
                            * = 11:  System Memory Frequency is 100Mhz (DDR200)
                            */
#define SYS_MEM_FREQ_166  1  /* System Memory Frequency is 166Mhz */
#define SYS_MEM_FREQ_133  2  /* System Memory Frequency is 133Mhz */
#define SYS_MEM_FREQ_100  3  /* System Memory Frequency is 100Mhz */
#define PCI_SMFREQ_POS    10 /* System Memory Frequency position  */

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
#define GPIO0           PCH_GPIOA         /* GPIO register 0 (DDC1) */
#define	DDC1_SCL_PIN    GPIO0_SCL_PIN   /* DDC1 SCL GPIO pin # */
#define	DDC1_SDA_PIN    GPIO0_SDA_PIN   /* DDC1 SDA CPIO pin # */
#define GPIO1           PCH_GPIOB         /* GPIO register 1 (I2C) */
#define	I2C_SCL_PIN     GPIO1_SCL_PIN   /* I2C SCL GPIO pin # */
#define	I2C_SDA_PIN     GPIO1_SDA_PIN   /* I2C SDA CPIO pin # */
#define GPIO2           PCH_GPIOC         /* GPIO register 2 (DDC2) */
#define	DDC2_SCL_PIN    GPIO2_SCL_PIN   /* DDC2 SCL GPIO pin # */
#define	DDC2_SDA_PIN    GPIO2_SDA_PIN   /* DDC2 SDA CPIO pin # */
#define GPIO3           PCH_GPIOD         /* GPIO register 3 (AGP mux DVI DDC) */
#define GPIO4           PCH_GPIOE         /* GPIO register 4 (AGP mux I2C) */
#define GPIO5           PCH_GPIOF         /* GPIO register 5 (AGP mux DDC2/I2C) */

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

#define GMBUS0          PCH_GMBUS0 /* GMBUS clock/device select register */
#define GMBUS1          PCH_GMBUS1 /* GMBUS command/status register */
#define GMBUS2          PCH_GMBUS2 /* GMBUS status register */
#define GMBUS3          PCH_GMBUS3 /* GMBUS data buffer register */
#define GMBUS4          PCH_GMBUS4 /* GMBUS REQUEST_INUSE register */
#define GMBUS5          PCH_GMBUS5 /* GMBUS 2-Byte Index register */

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

#define DREFCLK         0x0
#define SSC_CLK         0x2000
#define SUPER_SSC_CLK   0x6000
#define CLOCK_2X        0x40000000

# define DPUNIT_B_CLOCK_GATE_DISABLE		(1 << 30) /* 965 */
# define VSUNIT_CLOCK_GATE_DISABLE		(1 << 29) /* 965 */
# define VRDUNIT_CLOCK_GATE_DISABLE		(1 << 27) /* 965 */
# define AUDUNIT_CLOCK_GATE_DISABLE		(1 << 26) /* 965 */
# define DPUNIT_A_CLOCK_GATE_DISABLE		(1 << 25) /* 965 */
# define DPCUNIT_CLOCK_GATE_DISABLE		(1 << 24) /* 965 */
# define TVRUNIT_CLOCK_GATE_DISABLE		(1 << 23) /* 915-945 */
# define TVCUNIT_CLOCK_GATE_DISABLE		(1 << 22) /* 915-945 */
# define TVFUNIT_CLOCK_GATE_DISABLE		(1 << 21) /* 915-945 */
# define TVEUNIT_CLOCK_GATE_DISABLE		(1 << 20) /* 915-945 */
# define DVSUNIT_CLOCK_GATE_DISABLE		(1 << 19) /* 915-945 */
# define DSSUNIT_CLOCK_GATE_DISABLE		(1 << 18) /* 915-945 */
# define DDBUNIT_CLOCK_GATE_DISABLE		(1 << 17) /* 915-945 */
# define DPRUNIT_CLOCK_GATE_DISABLE		(1 << 16) /* 915-945 */
# define DPFUNIT_CLOCK_GATE_DISABLE		(1 << 15) /* 915-945 */
# define DPBMUNIT_CLOCK_GATE_DISABLE		(1 << 14) /* 915-945 */
# define DPLSUNIT_CLOCK_GATE_DISABLE		(1 << 13) /* 915-945 */
# define DPLUNIT_CLOCK_GATE_DISABLE		(1 << 12) /* 915-945 */
# define DPOUNIT_CLOCK_GATE_DISABLE		(1 << 11)
# define DPBUNIT_CLOCK_GATE_DISABLE		(1 << 10)
# define DCUNIT_CLOCK_GATE_DISABLE		(1 << 9)
# define DPUNIT_CLOCK_GATE_DISABLE		(1 << 8)
# define VRUNIT_CLOCK_GATE_DISABLE		(1 << 7) /* 915+: reserved */
# define OVHUNIT_CLOCK_GATE_DISABLE		(1 << 6) /* 830-865 */
# define DPIOUNIT_CLOCK_GATE_DISABLE		(1 << 6) /* 915-945 */
# define OVFUNIT_CLOCK_GATE_DISABLE		(1 << 5)
# define OVBUNIT_CLOCK_GATE_DISABLE		(1 << 4)


#define RENCLK_GATE_D   0x06204  /* Clock Gating Display for Render 1 */
#define RENDCLK_GATE_D2 0x06208  /* Clock Gating Disable for Render 2 */

#define FW_BLC_SELF     0x020E0
#define FW_BLC_SELF_EN  (1<<15)
#define DSP_ARB         0x70030
#define FW_BLC_AB       0x70034
#define FW_BLC_C        0x70038


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
 * Render engine configuration registers
 *---------------------------------------------------------------------------*/
#define CACHE_MODE_0	0x02120 /* 915+ only */
#define   CM0_IZ_OPT_DISABLE      (1<<6)
#define   CM0_ZR_OPT_DISABLE      (1<<5)
#define	  CM0_STC_EVICT_DISABLE_LRA_SNB	(1<<5)
#define   CM0_DEPTH_EVICT_DISABLE (1<<4)
#define   CM0_COLOR_EVICT_DISABLE (1<<3)
#define   CM0_DEPTH_WRITE_DISABLE (1<<1)
#define   CM0_RC_OP_FLUSH_DISABLE (1<<0)
#define BB_ADDR		0x02140 /* 8 bytes */
#define GFX_FLSH_CNTL	0x02170 /* 915+ only */
#define ECOSKPD		0x021d0
#define   ECO_GATING_CX_ONLY	(1<<3)
#define   ECO_FLIP_DONE		(1<<0)
#define GFX_FLSH_CNTL_GEN6            0x101008

#define CACHE_MODE_1		0x7004 /* IVB+ */
#define   PIXEL_SUBSPAN_COLLECT_OPT_DISABLE (1<<6)

/* GM45+ chicken bits -- debug workaround bits that may be required
 *  * for various sorts of correct behavior.  The top 16 bits of each are
 *   * the enables for writing to the corresponding low bit.
 *    */
#define _3D_CHICKEN	0x02084
#define _3D_CHICKEN2	0x0208c
/* Disables pipelining of read flushes past the SF-WIZ interface.
 *  * Required on all Ironlake steppings according to the B-Spec, but the
 *   * particular danger of not doing so is not specified.
 *    */
# define _3D_CHICKEN2_WM_READ_PIPELINED			(1 << 14)
#define _3D_CHICKEN3	0x02090
#define  _3D_CHICKEN_SF_DISABLE_FASTCLIP_CULL		(1 << 5)


/*-----------------------------------------------------------------------------
 * MBC Unit Space Register Definitions (9000h - 91B4h)
 *---------------------------------------------------------------------------*/
#define GEN6_MBCTL		0x0907c
#define   GEN6_MBCTL_ENABLE_BOOT_FETCH	(1 << 4)
#define   GEN6_MBCTL_CTX_FETCH_NEEDED	(1 << 3)
#define   GEN6_MBCTL_BME_UPDATE_ENABLE	(1 << 2)
#define   GEN6_MBCTL_MAE_UPDATE_ENABLE	(1 << 1)
#define   GEN6_MBCTL_BOOT_FETCH_MECH	(1 << 0)


/*-----------------------------------------------------------------------------
 * Display Palette Register Definitions (0A000h - 0AFFFh)
 *---------------------------------------------------------------------------*/
#define DPALETTE_A      0x0A000  /* Display Pipe A Palette */
#define DPALETTE_B      0x0A800  /* Display Pipe B Palette */
#define LGC_PALETTE_A   0x4a000  /* Display Pipe A Legacy 8-bit Palette */
#define LGC_PALETTE_B   0x4a800  /* Display Pipe B Legacy 8-bit Palette */

/*-----------------------------------------------------------------------------
 * Display Interrupt Status Registers (40000h > )
 *---------------------------------------------------------------------------*/
#define DEIIR           0x44008 /* Display Engine Interrupt Identity Register*/

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
#define CRCCTRLREDB     0x61050  /* Pipe B CRC Red Control Register */
#define CRCCTRLGREENB   0x61054  /* Pipe B CRC Green Control Register */
#define CRCCTRLBLUEB    0x61058  /* Pipe B CRC Blue Control Register */
#define CRCCTRLRESB     0x6105C  /* Pipe B CRC Alpha Control Register */

#define ADPACNTR        0x61100  /* Analog Display Port A Control */
#define SDVOBCNTR       0x61140  /* Digital Display Port B Control */
#define SDVOCCNTR       0x61160  /* Digital Display Port C Control */
#define LVDSCNTR        0x61180  /* Digital Display Port Control */

/* Panel Power Sequencing */
#define LVDS_PNL_PWR_STS  0x61200  /* LVDS Panel Power Status Register */
#define LVDS_PNL_PWR_CTL  0x61204  /* LVDS Panel Power Control Register */
#define PP_ON_DELAYS      0x61208  /* Panel Power On Sequencing Delays */
#define PP_DIVISOR        0x61210  /* Panel Power Cycle Delay and Reference */

/* Panel Fitting */
#define PFIT_CONTROL      0x61230  /* Panel Fitting Control */
#define PFIT_PGM_RATIOS   0x61234  /* Programmed Panel Fitting Ratios */

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


/*-----------------------------------------------------------------------------
 * Display Pipeline A Register ( 70000h - 70024h )
 *---------------------------------------------------------------------------*/
#define PIPEA_SCANLINE_COUNT   0x70000  /* Pipe A Scan Line Count (RO) */
#define PIPEA_SCANLINE_COMPARE 0x70004  /* Pipe A SLC Range Compare (RO) */
#define PIPE_STATUS_OFFSET     0x1C
#define PIPEAGCMAXRED          0x4d000  /* Pipe A Gamma Correct. Max Red */
#define PIPEAGCMAXGRN          0x4d004  /* Pipe A Gamma Correct. Max Green */
#define PIPEAGCMAXBLU          0x4d008  /* Pipe A Gamma Correct. Max Blue */
#define PIPEA_STAT             0x70024  /* Pipe A Display Status */
#define PIPEA_FRAME_COUNT      0x70040  /* Pipe A Frame Count */
#define PIPEA_FLIP_COUNT       0x70044  /* Pipe A Flip Count */
#define PIPEA_FRAME_HIGH       0x70040  /* Pipe A Frame Count High */
#define PIPEA_FRAME_PIXEL      0x70044  /* Pipe A Frame Cnt Low & pixel cnt */

#define PIPE_PIXEL_MASK        0x00ffffff
#define PIPE_FRAME_HIGH_MASK   0x0000ffff
#define PIPE_FRAME_LOW_MASK    0xff000000
#define PIPE_FRAME_LOW_SHIFT   24


/* following bit flag defs can be re-used for Pipe-B */
#define PIPE_ENABLE       BIT31
#define PIPE_STATE        BIT30
#define PIPE_LOCK         BIT25
#define GAMMA_MODE        BIT24
#define HOT_PLUG_EN       BIT26
#define VSYNC_STS_EN      BIT25
#define INTERLACE_EN	  BIT23|BIT22
#define VBLANK_STS_EN     BIT17
#define HOT_PLUG_STS      BIT10
#define VSYNC_STS         BIT9
#define VBLANK_STS        BIT1

#define	PIPE_BPC_MASK	(7 << 5) /* Ironlake */
#define PIPE_8BPC		(0 << 5)
#define PIPE_10BPC		(1 << 5)
#define PIPE_6BPC		(2 << 5)
#define PIPE_12BPC		(3 << 5)

#define	PIPE_INTERLACE_MASK	(7 << 21) /* Ironlake */
#define PROGRESSIVE_FETCH_PROGRESSIVE_DISPLAY     0
#define PROGRESSIVE_FETCH_INTERLACE_DISPLAY       1
#define INTERLACE_FETCH_INTERLACE_DISPLAY         3
#define INTERLACE_FETCH_INTERLACE_EMBDISP_PORT    4
#define PROGRESSIVE_FETCH_INTERLACE_EMBDISP_PORT  5

#define PALETTE_TYPE_MASK (3 << 24)
#define PALETTE_8BIT_LEGACY         0
#define PALETTE_10BIT_PRECISION     1
#define PALETTE_12BIT_INTERPOLATED  2

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

/*-----------------------------------------------------------------------------
 * VBIOS Software flags  00h - 0Fh
 *---------------------------------------------------------------------------*/
#define KFC             0x70400  /* Chicken Bit */
#define SWFABASE        0x4F000  /* Software flags A Base Addr */
#define SWF00           0x4F000
#define SWF01           0x4F004
#define SWF02           0x4F008
#define SWF03           0x4F00C
#define SWF04           0x4F010
#define SWF05           0x4F014
#define SWF06           0x4F018
#define SWF07           0x4F01C
#define SWF08           0x4F020
#define SWF09           0x4F024
#define SWF0A           0x4F028
#define SWF0B           0x4F02C
#define SWF0C           0x4F030
#define SWF0D           0x4F034
#define SWF0E           0x4F038
#define SWF0F           0x4F03C


/*-----------------------------------------------------------------------------
 * Display Pipeline B Register ( 71000h - 71024h )
 *---------------------------------------------------------------------------*/
#define PIPEB_SCANLINE_COUNT   0x71000 /* Pipe B Disp Scan Line Count Register */
#define PIPEB_SCANLINE_COMPARE 0x71004 /* Pipe B Disp Scan Line Count Range Compare */
#define PIPEBGCMAXRED          0x4d010 /* Pipe B Gamma Correct. Max Red */
#define PIPEBGCMAXGRN          0x4d014 /* Pipe B Gamma Correct. Max Green */
#define PIPEBGCMAXBLU          0x4d018 /* Pipe B Gamma Correct. Max Blue */
#define PIPEB_STAT             0x71024 /* Display Status Select Register */
#define PIPEB_FRAME_HIGH       0x71040 /* Pipe B Frame Count High */
#define PIPEB_FRAME_PIXEL      0x71044 /* Pipe B Frame Cnt Low and pixel cnt */


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
 * Drain Latencies, VBIOS Software flags  10h - 1Fh
 *---------------------------------------------------------------------------*/
#define WM0_PIPEA_ILK		0x45100
#define  WM0_PIPE_PLANE_MASK	(0x7f<<16)
#define  WM0_PIPE_PLANE_SHIFT	16
#define  WM0_PIPE_SPRITE_MASK	(0x3f<<8)
#define  WM0_PIPE_SPRITE_SHIFT	8
#define  WM0_PIPE_CURSOR_MASK	(0x1f)

#define WM0_PIPEB_ILK		0x45104
#define WM0_PIPEC_IVB		0x45200
#define WM1_LP_ILK		0x45108
#define  WM1_LP_SR_EN		(1<<31)
#define  WM1_LP_LATENCY_SHIFT	24
#define  WM1_LP_LATENCY_MASK	(0x7f<<24)
#define  WM1_LP_FBC_MASK	(0xf<<20)
#define  WM1_LP_FBC_SHIFT	20
#define  WM1_LP_SR_MASK		(0x1ff<<8)
#define  WM1_LP_SR_SHIFT	8
#define  WM1_LP_CURSOR_MASK	(0x3f)
#define WM2_LP_ILK		0x4510c
#define  WM2_LP_EN		(1<<31)
#define WM3_LP_ILK		0x45110
#define  WM3_LP_EN		(1<<31)
#define WM1S_LP_ILK		0x45120
#define WM2S_LP_IVB		0x45124
#define WM3S_LP_IVB		0x45128
#define  WM1S_LP_EN		(1<<31)


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
 * Display Video Sprite A Register Definitions (72180h - 721F4h)
 *---------------------------------------------------------------------------*/
#define DVSACNTR        0x72180  /* Display Video Sprite A */
#define DVSALINOFF      0x72184  /* Display Video Sprite A Linear Offset */
#define DVSASTRIDE      0x72188  /* Display Video Sprite A Stride */
#define DVSAPOS         0x7218C  /* Display Video Sprite A Position */
#define DVSASIZE        0x72190  /* Display Video Sprite A Size */
#define DVSAKEYMINVAL   0x72194  /* Display Video Sprite A color key Min */
#define DVSAKEYMASK     0x72198  /* Display Video Sprite A color key mask */
#define DVSASURF        0x7219C  /* Display Video Sprite A Suface base addr */
#define DVSAKEYMAXVAL   0x721A0  /* Display Video Sprite A color key Max */
#define DVSATILEOFF     0x721A4  /* Display Video Sprite A Tiled Offset */
#define DVSASURFLIVE    0x721AC  /* Display Video Sprite A Live Surf BsAddr Reg */
#define DVSASCALE       0x72204  /* Display Video Sprite A Scaler Control */
#define DVSAGAMCBASE    0x72300  /* Display Video Sprite A Gamma Correction 0 */
/*-----------------------------------------------------------------------------
 * Display Video Sprite B Register Definitions (73180h - 731F4h)
 *---------------------------------------------------------------------------*/
#define DVSBCNTR        0x73180  /* Display Video Sprite B */
#define DVSBLINOFF      0x73184  /* Display Video Sprite B Linear Offset */
#define DVSBSTRIDE      0x73188  /* Display Video Sprite B Stride */
#define DVSBPOS         0x7318C  /* Display Video Sprite B Position */
#define DVSBSIZE        0x73190  /* Display Video Sprite B Size */
#define DVSBKEYMINVAL   0x73194  /* Display Video Sprite B color key Min */
#define DVSBKEYMASK     0x73198  /* Display Video Sprite B color key mask */
#define DVSBSURF        0x7319C  /* Display Video Sprite B Suface base addr */
#define DVSBKEYMAXVAL   0x731A0  /* Display Video Sprite B color key Max */
#define DVSBTILEOFF     0x731A4  /* Display Video Sprite B Tiled Offset */
#define DVSBSURFLIVE    0x731AC  /* Display Video Sprite B Live Surf BsAddr Reg */
#define DVSBSCALE       0x73204  /* Display Video Sprite B Scaler Control */
#define DVSBGAMCBASE    0x73300  /* Display Video Sprite B Gamma Correction 0 */

#define _SPRITE(pipe, a, b) ((a) + (pipe)*((b)-(a)))
#define DVS_CNTR(pipe)        _SPRITE(pipe, DVSACNTR,      DVSBCNTR)
#define DVS_LINOFF(pipe)      _SPRITE(pipe, DVSALINOFF,    DVSBLINOFF)
#define DVS_STRIDE(pipe)      _SPRITE(pipe, DVSASTRIDE,    DVSBSTRIDE)
#define DVS_POS(pipe)         _SPRITE(pipe, DVSAPOS,       DVSBPOS)
#define DVS_SIZE(pipe)        _SPRITE(pipe, DVSASIZE,      DVSBSIZE)
#define DVS_KEYMINVAL(pipe)   _SPRITE(pipe, DVSAKEYMINVAL, DVSBKEYMINVAL)
#define DVS_KEYMASK(pipe)     _SPRITE(pipe, DVSAKEYMASK,   DVSBKEYMASK)
#define DVS_SURF(pipe)        _SPRITE(pipe, DVSASURF,      DVSBSURF)
#define DVS_KEYMAXVAL(pipe)   _SPRITE(pipe, DVSAKEYMAXVAL, DVSBKEYMAXVAL)
#define DVS_TILEOFF(pipe)     _SPRITE(pipe, DVSATILEOFF,   DVSBTILEOFF)
#define DVS_SURFLIVE(pipe)    _SPRITE(pipe, DVSASURFLIVE,  DVSBSURFLIVE)
#define DVS_SCALE(pipe)       _SPRITE(pipe, DVSASCALE,     DVSBSCALE)
#define DVS_GAMCBASE(pipe)    _SPRITE(pipe, DVSAGAMCBASE,  DVSBGAMCBASE)


#define PLANE_ENABLE    BIT31
#define GAMMA_ENABLE    BIT30
#define BPP_MASK        BIT29 + BIT28 + BIT27 + BIT26
#define BPP_POS         26
#define STEREO_ENABLE   BIT25
#define PIPE_SEL        BIT24
#define PIPE_SEL_POS    24
#define PIXEL_MULTIPLY  BIT21 + BIT20
#define STEREO_POLARITY BIT18

/* Common offsets for all Display Pipeline registers */
#define DSP_LINEAR_OFFSET 0x04  /* Offset from the control reg */
#define DSP_STRIDE_OFFSET 0x08  /* Offset from the control reg */
#define DSP_SIZE_OFFSET   0x10  /* Offset from the control reg */
#define DSP_START_OFFSET  0x1c  /* Offset from the control reg */
#define DSP_TOFF_OFFSET   0x24  /* Offset from the control reg */



/*-----------------------------------------------------------------------------
 * VGA Display Plane Control Register Definitions (41000h)
 *---------------------------------------------------------------------------*/
/*#define VP00             0x71400*/
#define VGACNTRL         0x41000  /* VGA Display Plane Control Register */
/*#define VGA_DOUBLE       BIT30*/
#define VGA_PIPE         BIT29
/*#define VGA_CENTER_MASK  BIT25 + BIT24
#define VGA_CENTER_1     BIT25
#define VGA_CENTER_0     BIT24*/
#define VGA_COL_CONVERT  BIT24
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

/* Map old software flag names to their new Gen4 names */
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

/*
 * Ring buffer base and offsets
 */
#define RINGBUF 0x2030
#define PRIMARY_0   0

/*-----------------------------------------------------------------------------
 * GMBUS / GPIO : I2C Bus Types vs PinPair Selection / Register Offsets
 ----------------------------------------------------------------------------*/

/*
#define SNB_GMBUS_ANALOG_DDC    1
#define SNB_GMBUS_INT_LVDS_DDC  2
#define SNB_GMBUS_DVO_REG       3
#define SNB_GMBUS_DVOB_DDC      4
#define SNB_GMBUS_DVOC_DDC      5
#define SNB_GMBUS_HDMI_B		6
#define SNB_GMBUS_HDMI_C		7
#define SNB_GMBUS_HDMI_D		8
*/

/* definitions for GMBUS conversion from IGD_PARAM_GPIO_PAIR
 * to Register Port Select bits - see BITS 2:0 of GMBUS0 register*/
#define GMBUS_PINS_NONE         0x0 /* NONE */
#define GMBUS_PINS_LCTRCLK      0x1 /* SSC */
#define GMBUS_PINS_ANALOG       0x2 /* Analog */
#define GMBUS_PINS_LVDS         0x3 /* LVDS */
#define GMBUS_PINS_DP_HDMI_C	0x4 /* DP/DVI-HDMI Port C*/
#define GMBUS_PINS_SDVO_HDMI_B  0x5 /* SDVO / DVI-HDMI B */
#define GMBUS_PINS_DP_HDMI_D 	0x6 /* DP/DVI-HDMI Port D*/

/* definitions for GPIO conversion from IGD_PARAM_GPIO_PAIR
 * to GPIO Control Register Offsets - see GPIO_CTL register array*/
#define GPIO_OFFSET_LCTRCLK      0xC5014 /* SSC */
#define GPIO_OFFSET_ANALOG       0xC5010 /* LVDS */
#define GPIO_OFFSET_LVDS         0xC5018 /* Analog */
#define GPIO_OFFSET_DP_HDMI_C	 0xC501C /* DP/DVI-HDMI Port C*/
#define GPIO_OFFSET_SDVO_HDMI_B  0xC5020 /* SDVO / DVI-HDMI B */
#define GPIO_OFFSET_DP_HDMI_D 	 0xC5024 /* DP/DVI-HDMI Port D*/



/*
#define DSPSTRIDE(plane) _PIPE(plane, DSPASTRIDE, DSPBSTRIDE)
#define DSPPOS(plane) _PIPE(plane, DSPAPOS, DSPBPOS)
#define DSPSIZE(plane) _PIPE(plane, DSPASIZE, DSPBSIZE)
#define DSPTILEOFF(plane) _PIPE(plane, DSPATILEOFF, DSPBTILEOFF)
*/

#define   DISPLAY_PLANE_ENABLE			(1<<31)
#define   DISPLAY_PLANE_DISABLE			0
#define   DISPPLANE_GAMMA_ENABLE		(1<<30)
#define   DISPPLANE_GAMMA_DISABLE		0
#define   DISPPLANE_PIXFORMAT_MASK		(0xf<<26)
#define   DISPPLANE_8BPP			(0x2<<26)
#define   DISPPLANE_15_16BPP			(0x4<<26)
#define   DISPPLANE_16BPP			(0x5<<26)
#define   DISPPLANE_32BPP_NO_ALPHA		(0x6<<26)
#define   DISPPLANE_32BPP			(0x7<<26)
#define   DISPPLANE_32BPP_30BIT_NO_ALPHA	(0xa<<26)
#define   DISPPLANE_STEREO_ENABLE		(1<<25)
#define   DISPPLANE_STEREO_DISABLE		0
#define   DISPPLANE_SEL_PIPE_SHIFT		24
#define   DISPPLANE_SEL_PIPE_MASK		(3<<DISPPLANE_SEL_PIPE_SHIFT)
#define   DISPPLANE_SEL_PIPE_A			0
#define   DISPPLANE_SEL_PIPE_B			(1<<DISPPLANE_SEL_PIPE_SHIFT)
#define   DISPPLANE_SRC_KEY_ENABLE		(1<<22)
#define   DISPPLANE_SRC_KEY_DISABLE		0
#define   DISPPLANE_LINE_DOUBLE			(1<<20)
#define   DISPPLANE_NO_LINE_DOUBLE		0
#define   DISPPLANE_STEREO_POLARITY_FIRST	0
#define   DISPPLANE_STEREO_POLARITY_SECOND	(1<<18)
#define   DISPPLANE_TRICKLE_FEED_DISABLE	(1<<14) /* Ironlake */
#define   DISPPLANE_TILED			(1<<10)



#define CPU_VGACNTRL	0x41000
#define ILK_DISPLAY_CHICKEN1	0x42000
#define   ILK_FBCQ_DIS		(1<<22)
#define	  ILK_PABSTRETCH_DIS	(1<<21)
#define ILK_DISPLAY_CHICKEN2	0x42004

#define DIGITAL_PORT_HOTPLUG_CNTRL      0x44030
#define  DIGITAL_PORTA_HOTPLUG_ENABLE           (1 << 4)
#define  DIGITAL_PORTA_SHORT_PULSE_2MS          (0 << 2)
#define  DIGITAL_PORTA_SHORT_PULSE_4_5MS        (1 << 2)
#define  DIGITAL_PORTA_SHORT_PULSE_6MS          (2 << 2)
#define  DIGITAL_PORTA_SHORT_PULSE_100MS        (3 << 2)
#define  DIGITAL_PORTA_NO_DETECT                (0 << 0)
#define  DIGITAL_PORTA_LONG_PULSE_DETECT_MASK   (1 << 1)
#define  DIGITAL_PORTA_SHORT_PULSE_DETECT_MASK  (1 << 0)

/* refresh rate hardware control */
#define RR_HW_CTL       0x45300
#define  RR_HW_LOW_POWER_FRAMES_MASK    0xff
#define  RR_HW_HIGH_POWER_FRAMES_MASK   0xff00

#define FDI_PLL_BIOS_0  0x46000
#define  FDI_PLL_FB_CLOCK_MASK  0xff
#define FDI_PLL_BIOS_1  0x46004
#define FDI_PLL_BIOS_2  0x46008
#define DISPLAY_PORT_PLL_BIOS_0         0x4600c
#define DISPLAY_PORT_PLL_BIOS_1         0x46010
#define DISPLAY_PORT_PLL_BIOS_2         0x46014

#define PCH_DSPCLK_GATE_D	0x42020
# define DPFCUNIT_CLOCK_GATE_DISABLE		(1 << 9)
# define DPFCRUNIT_CLOCK_GATE_DISABLE		(1 << 8)
# define DPFDUNIT_CLOCK_GATE_DISABLE		(1 << 7)
# define DPARBUNIT_CLOCK_GATE_DISABLE		(1 << 5)

#define PCH_3DCGDIS0		0x46020
# define MARIUNIT_CLOCK_GATE_DISABLE		(1 << 18)
# define SVSMUNIT_CLOCK_GATE_DISABLE		(1 << 1)

#define PCH_3DCGDIS1		0x46024
# define VFMUNIT_CLOCK_GATE_DISABLE		(1 << 11)

#define FDI_PLL_FREQ_CTL        0x46030
#define  FDI_PLL_FREQ_CHANGE_REQUEST    (1<<24)
#define  FDI_PLL_FREQ_LOCK_LIMIT_MASK   0xfff00
#define  FDI_PLL_FREQ_DISABLE_COUNT_LIMIT_MASK  0xff


#define PIPEA_DATA_M1           0x60030
#define  TU_SIZE(x)             (((x)-1) << 25) /* default size 64 */
#define  TU_SIZE_MASK           0x7e000000
#define  PIPE_DATA_M1_OFFSET    0
#define PIPEA_DATA_N1           0x60034
#define  PIPE_DATA_N1_OFFSET    0

#define PIPEA_DATA_M2           0x60038
#define  PIPE_DATA_M2_OFFSET    0
#define PIPEA_DATA_N2           0x6003c
#define  PIPE_DATA_N2_OFFSET    0

#define PIPEA_LINK_M1           0x60040
#define  PIPE_LINK_M1_OFFSET    0
#define PIPEA_LINK_N1           0x60044
#define  PIPE_LINK_N1_OFFSET    0

#define PIPEA_LINK_M2           0x60048
#define  PIPE_LINK_M2_OFFSET    0
#define PIPEA_LINK_N2           0x6004c
#define  PIPE_LINK_N2_OFFSET    0

/* PIPEB timing regs are same start from 0x61000 */

#define PIPEB_DATA_M1           0x61030
#define PIPEB_DATA_N1           0x61034

#define PIPEB_DATA_M2           0x61038
#define PIPEB_DATA_N2           0x6103c

#define PIPEB_LINK_M1           0x61040
#define PIPEB_LINK_N1           0x61044

#define PIPEB_LINK_M2           0x61048
#define PIPEB_LINK_N2           0x6104c

/* CPU panel fitter */
#define PFA_CTL_1               0x68080
#define PFB_CTL_1               0x68880
#define  PF_ENABLE              (1<<31)
#define  PF_FILTER_MASK         (3<<23)
#define  PF_FILTER_PROGRAMMED	(0<<23)
#define  PF_FILTER_MED_3x3      (1<<23)
#define  PF_FILTER_EDGE_ENHANCE	(2<<23)
#define  PF_FILTER_EDGE_SOFTEN	(3<<23)
#define PFA_WIN_SZ              0x68074
#define PFB_WIN_SZ              0x68874
#define PFA_WIN_POS             0x68070
#define PFB_WIN_POS             0x68870
#define PFA_PWR_GATE_CTRL       0x68060
#define PFB_PWR_GATE_CTRL       0x68860

/* interrupts */
#define DE_MASTER_IRQ_CONTROL   (1 << 31)
#define DE_SPRITEB_FLIP_DONE    (1 << 29)
#define DE_SPRITEA_FLIP_DONE    (1 << 28)
#define DE_PLANEB_FLIP_DONE     (1 << 27)
#define DE_PLANEA_FLIP_DONE     (1 << 26)
#define DE_PCU_EVENT            (1 << 25)
#define DE_GTT_FAULT            (1 << 24)
#define DE_POISON               (1 << 23)
#define DE_PERFORM_COUNTER      (1 << 22)
#define DE_PCH_EVENT            (1 << 21)
#define DE_AUX_CHANNEL_A        (1 << 20)
#define DE_DP_A_HOTPLUG         (1 << 19)
#define DE_GSE                  (1 << 18)
#define DE_PIPEB_VBLANK         (1 << 15)
#define DE_PIPEB_EVEN_FIELD     (1 << 14)
#define DE_PIPEB_ODD_FIELD      (1 << 13)
#define DE_PIPEB_LINE_COMPARE   (1 << 12)
#define DE_PIPEB_VSYNC          (1 << 11)
#define DE_PIPEB_FIFO_UNDERRUN  (1 << 8)
#define DE_PIPEA_VBLANK         (1 << 7)
#define DE_PIPEA_EVEN_FIELD     (1 << 6)
#define DE_PIPEA_ODD_FIELD      (1 << 5)
#define DE_PIPEA_LINE_COMPARE   (1 << 4)
#define DE_PIPEA_VSYNC          (1 << 3)
#define DE_PIPEA_FIFO_UNDERRUN  (1 << 0)

#define DEISR   0x44000
#define DEIMR   0x44004
#define DEIIR   0x44008
#define DEIER   0x4400c

/* GT interrupt */
#define GT_PIPE_NOTIFY		(1 << 4)
#define GT_SYNC_STATUS          (1 << 2)
#define GT_USER_INTERRUPT       (1 << 0)
#define GT_BSD_USER_INTERRUPT   (1 << 5)
#define GT_GEN6_BSD_USER_INTERRUPT	(1 << 12)
#define GT_BLT_USER_INTERRUPT	(1 << 22)

#define GTISR   0x44010
#define GTIMR   0x44014
#define GTIIR   0x44018
#define GTIER   0x4401c

#define ILK_DISPLAY_CHICKEN2	0x42004
/* Required on all Ironlake and Sandybridge according to the B-Spec. */
#define  ILK_ELPIN_409_SELECT	(1 << 25)
#define  ILK_DPARB_GATE	(1<<22)
#define  ILK_VSDPFD_FULL	(1<<21)
#define ILK_DISPLAY_CHICKEN_FUSES	0x42014
#define  ILK_INTERNAL_GRAPHICS_DISABLE	(1<<31)
#define  ILK_INTERNAL_DISPLAY_DISABLE	(1<<30)
#define  ILK_DISPLAY_DEBUG_DISABLE	(1<<29)
#define  ILK_HDCP_DISABLE		(1<<25)
#define  ILK_eDP_A_DISABLE		(1<<24)
#define  ILK_DESKTOP			(1<<23)
#define ILK_DSPCLK_GATE		0x42020
#define  ILK_DPARB_CLK_GATE	(1<<5)
#define  ILK_DPFD_CLK_GATE	(1<<7)

/* According to spec this bit 7/8/9 of 0x42020 should be set to enable FBC */
#define   ILK_CLK_FBC		(1<<7)
#define   ILK_DPFC_DIS1		(1<<8)
#define   ILK_DPFC_DIS2		(1<<9)

#define DISP_ARB_CTL	0x45000
#define  DISP_TILE_SURFACE_SWIZZLING	(1<<13)
#define  DISP_FBC_WM_DIS		(1<<15)

/* PCH */

/* south display engine interrupt */
#define SDE_AUDIO_POWER_D	(1 << 27)
#define SDE_AUDIO_POWER_C	(1 << 26)
#define SDE_AUDIO_POWER_B	(1 << 25)
#define SDE_AUDIO_POWER_SHIFT	(25)
#define SDE_AUDIO_POWER_MASK	(7 << SDE_AUDIO_POWER_SHIFT)
#define SDE_GMBUS		(1 << 24)
#define SDE_AUDIO_HDCP_TRANSB	(1 << 23)
#define SDE_AUDIO_HDCP_TRANSA	(1 << 22)
#define SDE_AUDIO_HDCP_MASK	(3 << 22)
#define SDE_AUDIO_TRANSB	(1 << 21)
#define SDE_AUDIO_TRANSA	(1 << 20)
#define SDE_AUDIO_TRANS_MASK	(3 << 20)
#define SDE_POISON		(1 << 19)
/* 18 reserved */
#define SDE_FDI_RXB		(1 << 17)
#define SDE_FDI_RXA		(1 << 16)
#define SDE_FDI_MASK		(3 << 16)
#define SDE_AUXD		(1 << 15)
#define SDE_AUXC		(1 << 14)
#define SDE_AUXB		(1 << 13)
#define SDE_AUX_MASK		(7 << 13)
/* 12 reserved */
#define SDE_CRT_HOTPLUG         (1 << 11)
#define SDE_PORTD_HOTPLUG       (1 << 10)
#define SDE_PORTC_HOTPLUG       (1 << 9)
#define SDE_PORTB_HOTPLUG       (1 << 8)
#define SDE_SDVOB_HOTPLUG       (1 << 6)
#define SDE_HOTPLUG_MASK	(0xf << 8)
#define SDE_TRANSB_CRC_DONE	(1 << 5)
#define SDE_TRANSB_CRC_ERR	(1 << 4)
#define SDE_TRANSB_FIFO_UNDER	(1 << 3)
#define SDE_TRANSA_CRC_DONE	(1 << 2)
#define SDE_TRANSA_CRC_ERR	(1 << 1)
#define SDE_TRANSA_FIFO_UNDER	(1 << 0)
#define SDE_TRANS_MASK		(0x3f)
/* CPT */
#define SDE_CRT_HOTPLUG_CPT	(1 << 19)
#define SDE_PORTD_HOTPLUG_CPT	(1 << 23)
#define SDE_PORTC_HOTPLUG_CPT	(1 << 22)
#define SDE_PORTB_HOTPLUG_CPT	(1 << 21)
#define SDE_HOTPLUG_MASK_CPT	(SDE_CRT_HOTPLUG_CPT |		\
				 SDE_PORTD_HOTPLUG_CPT |	\
				 SDE_PORTC_HOTPLUG_CPT |	\
				 SDE_PORTB_HOTPLUG_CPT)

#define SDEISR  0xc4000
#define SDEIMR  0xc4004
#define SDEIIR  0xc4008
#define SDEIER  0xc400c

/* digital port hotplug */
#define PCH_PORT_HOTPLUG        0xc4030
#define PORTD_HOTPLUG_ENABLE            (1 << 20)
#define PORTD_PULSE_DURATION_2ms        (0)
#define PORTD_PULSE_DURATION_4_5ms      (1 << 18)
#define PORTD_PULSE_DURATION_6ms        (2 << 18)
#define PORTD_PULSE_DURATION_100ms      (3 << 18)
#define PORTD_HOTPLUG_NO_DETECT         (0)
#define PORTD_HOTPLUG_SHORT_DETECT      (1 << 16)
#define PORTD_HOTPLUG_LONG_DETECT       (1 << 17)
#define PORTC_HOTPLUG_ENABLE            (1 << 12)
#define PORTC_PULSE_DURATION_2ms        (0)
#define PORTC_PULSE_DURATION_4_5ms      (1 << 10)
#define PORTC_PULSE_DURATION_6ms        (2 << 10)
#define PORTC_PULSE_DURATION_100ms      (3 << 10)
#define PORTC_HOTPLUG_NO_DETECT         (0)
#define PORTC_HOTPLUG_SHORT_DETECT      (1 << 8)
#define PORTC_HOTPLUG_LONG_DETECT       (1 << 9)
#define PORTB_HOTPLUG_ENABLE            (1 << 4)
#define PORTB_PULSE_DURATION_2ms        (0)
#define PORTB_PULSE_DURATION_4_5ms      (1 << 2)
#define PORTB_PULSE_DURATION_6ms        (2 << 2)
#define PORTB_PULSE_DURATION_100ms      (3 << 2)
#define PORTB_HOTPLUG_NO_DETECT         (0)
#define PORTB_HOTPLUG_SHORT_DETECT      (1 << 0)
#define PORTB_HOTPLUG_LONG_DETECT       (1 << 1)

#define PCH_GPIOA               0xc5010
#define PCH_GPIOB               0xc5014
#define PCH_GPIOC               0xc5018
#define PCH_GPIOD               0xc501c
#define PCH_GPIOE               0xc5020
#define PCH_GPIOF               0xc5024

#define PCH_GMBUS0		0xc5100
#define PCH_GMBUS1		0xc5104
#define PCH_GMBUS2		0xc5108
#define PCH_GMBUS3		0xc510c
#define PCH_GMBUS4		0xc5110
#define PCH_GMBUS5		0xc5120

typedef struct _fdi_m_n {
	unsigned long num_lanes;
	unsigned long tu;
	unsigned long gmch_m;
	unsigned long gmch_n;
	unsigned long link_m;
	unsigned long link_n;
	unsigned long port_mult;
} fdi_m_n;
extern fdi_m_n * fdi_mn_new_pipea;
extern fdi_m_n * fdi_mn_new_pipeb;
extern fdi_m_n * fdi_mn_curr_pipea;
extern fdi_m_n * fdi_mn_curr_pipeb;

#define PCH_DPLL_A              0xc6014
#define PCH_DPLL_B              0xc6018
#define   DPLL_VCO_ENABLE		(1 << 31)

#define PCH_FPA0                0xc6040
#define FP_CB_TUNE              (0x3<<22)
#define PCH_FPA1                0xc6044
#define PCH_FPB0                0xc6048
#define PCH_FPB1                0xc604c

#define PCH_DPLL_TEST           0xc606c

#define PCH_DREF_CONTROL        0xC6200
#define  DREF_CONTROL_MASK      0x7fc3
#define  DREF_CPU_SOURCE_OUTPUT_DISABLE         (0<<13)
#define  DREF_CPU_SOURCE_OUTPUT_DOWNSPREAD      (2<<13)
#define  DREF_CPU_SOURCE_OUTPUT_NONSPREAD       (3<<13)
#define  DREF_CPU_SOURCE_OUTPUT_MASK		(3<<13)
#define  DREF_SSC_SOURCE_DISABLE                (0<<11)
#define  DREF_SSC_SOURCE_ENABLE                 (2<<11)
#define  DREF_SSC_SOURCE_MASK			(3<<11)
#define  DREF_NONSPREAD_SOURCE_DISABLE          (0<<9)
#define  DREF_NONSPREAD_CK505_ENABLE		(1<<9)
#define  DREF_NONSPREAD_SOURCE_ENABLE           (2<<9)
#define  DREF_NONSPREAD_SOURCE_MASK		(3<<9)
#define  DREF_SUPERSPREAD_SOURCE_DISABLE        (0<<7)
#define  DREF_SUPERSPREAD_SOURCE_ENABLE         (2<<7)
#define  DREF_SSC4_DOWNSPREAD                   (0<<6)
#define  DREF_SSC4_CENTERSPREAD                 (1<<6)
#define  DREF_SSC1_DISABLE                      (0<<1)
#define  DREF_SSC1_ENABLE                       (1<<1)
#define  DREF_SSC4_DISABLE                      (0)
#define  DREF_SSC4_ENABLE                       (1)

#define PCH_RAWCLK_FREQ         0xc6204
#define  FDL_TP1_TIMER_SHIFT    12
#define  FDL_TP1_TIMER_MASK     (3<<12)
#define  FDL_TP2_TIMER_SHIFT    10
#define  FDL_TP2_TIMER_MASK     (3<<10)
#define  RAWCLK_FREQ_MASK       0x3ff

#define PCH_DPLL_TMR_CFG        0xc6208

#define PCH_SSC4_PARMS          0xc6210
#define PCH_SSC4_AUX_PARMS      0xc6214

#define PCH_DPLL_SEL		0xc7000
#define  TRANSA_DPLL_ENABLE	(1<<3)
#define	 TRANSA_DPLLB_SEL	(1<<0)
#define	 TRANSA_DPLLA_SEL	0
#define  TRANSB_DPLL_ENABLE	(1<<7)
#define	 TRANSB_DPLLB_SEL	(1<<4)
#define	 TRANSB_DPLLA_SEL	(0)
#define  TRANSC_DPLL_ENABLE	(1<<11)
#define	 TRANSC_DPLLB_SEL	(1<<8)
#define	 TRANSC_DPLLA_SEL	(0)

/* transcoder */

#define TRANS_HTOTAL_A          0xe0000
#define  TRANS_HTOTAL_SHIFT     16
#define  TRANS_HACTIVE_SHIFT    0
#define TRANS_HBLANK_A          0xe0004
#define  TRANS_HBLANK_END_SHIFT 16
#define  TRANS_HBLANK_START_SHIFT 0
#define TRANS_HSYNC_A           0xe0008
#define  TRANS_HSYNC_END_SHIFT  16
#define  TRANS_HSYNC_START_SHIFT 0
#define TRANS_VTOTAL_A          0xe000c
#define  TRANS_VTOTAL_SHIFT     16
#define  TRANS_VACTIVE_SHIFT    0
#define TRANS_VBLANK_A          0xe0010
#define  TRANS_VBLANK_END_SHIFT 16
#define  TRANS_VBLANK_START_SHIFT 0
#define TRANS_VSYNC_A           0xe0014
#define  TRANS_VSYNC_END_SHIFT  16
#define  TRANS_VSYNC_START_SHIFT 0

#define TRANSA_DATA_M1          0xe0030
#define TRANSA_DATA_N1          0xe0034
#define TRANSA_DATA_M2          0xe0038
#define TRANSA_DATA_N2          0xe003c
#define TRANSA_DP_LINK_M1       0xe0040
#define TRANSA_DP_LINK_N1       0xe0044
#define TRANSA_DP_LINK_M2       0xe0048
#define TRANSA_DP_LINK_N2       0xe004c

#define TRANS_HTOTAL_B          0xe1000
#define TRANS_HBLANK_B          0xe1004
#define TRANS_HSYNC_B           0xe1008
#define TRANS_VTOTAL_B          0xe100c
#define TRANS_VBLANK_B          0xe1010
#define TRANS_VSYNC_B           0xe1014

#define TRANSB_DATA_M1          0xe1030
#define TRANSB_DATA_N1          0xe1034
#define TRANSB_DATA_M2          0xe1038
#define TRANSB_DATA_N2          0xe103c
#define TRANSB_DP_LINK_M1       0xe1040
#define TRANSB_DP_LINK_N1       0xe1044
#define TRANSB_DP_LINK_M2       0xe1048
#define TRANSB_DP_LINK_N2       0xe104c

#define TRANSACONF              0xf0008
#define TRANSBCONF              0xf1008
#define  TRANS_DISABLE          (0<<31)
#define  TRANS_ENABLE           (1<<31)
#define  TRANS_STATE_MASK       (1<<30)
#define  TRANS_STATE_DISABLE    (0<<30)
#define  TRANS_STATE_ENABLE     (1<<30)
#define  TRANS_FSYNC_DELAY_HB1  (0<<27)
#define  TRANS_FSYNC_DELAY_HB2  (1<<27)
#define  TRANS_FSYNC_DELAY_HB3  (2<<27)
#define  TRANS_FSYNC_DELAY_HB4  (3<<27)
#define  TRANS_DP_AUDIO_ONLY    (1<<26)
#define  TRANS_DP_VIDEO_AUDIO   (0<<26)
#define  TRANS_PROGRESSIVE      (0<<21)
#define  TRANS_8BPC             (0<<5)
#define  TRANS_10BPC            (1<<5)
#define  TRANS_6BPC             (2<<5)
#define  TRANS_12BPC            (3<<5)

#define  FDI_RX_PHASE_SYNC_POINTER_ENABLE       (1)

#define SOUTH_DSPCLK_GATE_D	0xc2020
#define  PCH_DPLSUNIT_CLOCK_GATE_DISABLE (1<<29)

/* CPU: FDI_TX */
#define  FDI_TX_DISABLE         (0<<31)
#define  FDI_TX_ENABLE          (1<<31)
#define  FDI_LINK_TRAIN_PATTERN_1       (0<<28)
#define  FDI_LINK_TRAIN_PATTERN_2       (1<<28)
#define  FDI_LINK_TRAIN_PATTERN_IDLE    (2<<28)
#define  FDI_LINK_TRAIN_NONE            (3<<28)
#define  FDI_LINK_TRAIN_VOLTAGE_0_4V    (0<<25)
#define  FDI_LINK_TRAIN_VOLTAGE_0_6V    (1<<25)
#define  FDI_LINK_TRAIN_VOLTAGE_0_8V    (2<<25)
#define  FDI_LINK_TRAIN_VOLTAGE_1_2V    (3<<25)
#define  FDI_LINK_TRAIN_PRE_EMPHASIS_NONE (0<<22)
#define  FDI_LINK_TRAIN_PRE_EMPHASIS_1_5X (1<<22)
#define  FDI_LINK_TRAIN_PRE_EMPHASIS_2X   (2<<22)
#define  FDI_LINK_TRAIN_PRE_EMPHASIS_3X   (3<<22)
/* ILK always use 400mV 0dB for voltage swing and pre-emphasis level.
   SNB has different settings. */
/* SNB A-stepping */
#define  FDI_LINK_TRAIN_400MV_0DB_SNB_A		(0x38<<22)
#define  FDI_LINK_TRAIN_400MV_6DB_SNB_A		(0x02<<22)
#define  FDI_LINK_TRAIN_600MV_3_5DB_SNB_A	(0x01<<22)
#define  FDI_LINK_TRAIN_800MV_0DB_SNB_A		(0x0<<22)
/* SNB B-stepping */
#define  FDI_LINK_TRAIN_400MV_0DB_SNB_B		(0x0<<22)
#define  FDI_LINK_TRAIN_400MV_6DB_SNB_B		(0x3a<<22)
#define  FDI_LINK_TRAIN_600MV_3_5DB_SNB_B	(0x39<<22)
#define  FDI_LINK_TRAIN_800MV_0DB_SNB_B		(0x38<<22)
#define  FDI_LINK_TRAIN_VOL_EMP_MASK		(0x3f<<22)
#define  FDI_DP_PORT_WIDTH_X1           (0<<19)
#define  FDI_DP_PORT_WIDTH_X2           (1<<19)
#define  FDI_DP_PORT_WIDTH_X3           (2<<19)
#define  FDI_DP_PORT_WIDTH_X4           (3<<19)
#define  FDI_TX_ENHANCE_FRAME_ENABLE    (1<<18)
/* Ironlake: hardwired to 1 */
#define  FDI_TX_PLL_ENABLE              (1<<14)
/* both Tx and Rx */
#define  FDI_SCRAMBLING_ENABLE          (0<<7)
#define  FDI_SCRAMBLING_DISABLE         (1<<7)

/* FDI_RX, FDI_X is hard-wired to Transcoder_X */
#define  FDI_RX_ENABLE          (1<<31)
/* train, dp width same as FDI_TX */
#define  FDI_DP_PORT_WIDTH_X8           (7<<19)
#define  FDI_8BPC                       (0<<16)
#define  FDI_10BPC                      (1<<16)
#define  FDI_6BPC                       (2<<16)
#define  FDI_12BPC                      (3<<16)
#define  FDI_LINK_REVERSE_OVERWRITE     (1<<15)
#define  FDI_DMI_LINK_REVERSE_MASK      (1<<14)
#define  FDI_RX_PLL_ENABLE              (1<<13)
#define  FDI_FS_ERR_CORRECT_ENABLE      (1<<11)
#define  FDI_FE_ERR_CORRECT_ENABLE      (1<<10)
#define  FDI_FS_ERR_REPORT_ENABLE       (1<<9)
#define  FDI_FE_ERR_REPORT_ENABLE       (1<<8)
#define  FDI_RX_ENHANCE_FRAME_ENABLE    (1<<6)
#define  FDI_PCDCLK	                (1<<4)
/* CPT */
#define  FDI_AUTO_TRAINING			(1<<10)
#define  FDI_LINK_TRAIN_PATTERN_1_CPT		(0<<8)
#define  FDI_LINK_TRAIN_PATTERN_2_CPT		(1<<8)
#define  FDI_LINK_TRAIN_PATTERN_IDLE_CPT	(2<<8)
#define  FDI_LINK_TRAIN_NORMAL_CPT		(3<<8)
#define  FDI_LINK_TRAIN_PATTERN_MASK_CPT	(3<<8)

/* FDI_RX interrupt register format */
#define FDI_RX_INTER_LANE_ALIGN         (1<<10)
#define FDI_RX_SYMBOL_LOCK              (1<<9) /* train 2 */
#define FDI_RX_BIT_LOCK                 (1<<8) /* train 1 */
#define FDI_RX_TRAIN_PATTERN_2_FAIL     (1<<7)
#define FDI_RX_FS_CODE_ERR              (1<<6)
#define FDI_RX_FE_CODE_ERR              (1<<5)
#define FDI_RX_SYMBOL_ERR_RATE_ABOVE    (1<<4)
#define FDI_RX_HDCP_LINK_FAIL           (1<<3)
#define FDI_RX_PIXEL_FIFO_OVERFLOW      (1<<2)
#define FDI_RX_CROSS_CLOCK_OVERFLOW     (1<<1)
#define FDI_RX_SYMBOL_QUEUE_OVERFLOW    (1<<0)

#define FDI_PLL_CTL_1           0xfe000
#define FDI_PLL_CTL_2           0xfe004

/* CRT */
#define PCH_ADPA                0xe1100
#define  ADPA_TRANS_SELECT_MASK (1<<30)
#define  ADPA_TRANS_A_SELECT    0
#define  ADPA_TRANS_B_SELECT    (1<<30)
#define  ADPA_CRT_HOTPLUG_MASK  0x03ff0000 /* bit 25-16 */
#define  ADPA_CRT_HOTPLUG_MONITOR_NONE  (0<<24)
#define  ADPA_CRT_HOTPLUG_MONITOR_MASK  (3<<24)
#define  ADPA_CRT_HOTPLUG_MONITOR_COLOR (3<<24)
#define  ADPA_CRT_HOTPLUG_MONITOR_MONO  (2<<24)
#define  ADPA_CRT_HOTPLUG_ENABLE        (1<<23)
#define  ADPA_CRT_HOTPLUG_PERIOD_64     (0<<22)
#define  ADPA_CRT_HOTPLUG_PERIOD_128    (1<<22)
#define  ADPA_CRT_HOTPLUG_WARMUP_5MS    (0<<21)
#define  ADPA_CRT_HOTPLUG_WARMUP_10MS   (1<<21)
#define  ADPA_CRT_HOTPLUG_SAMPLE_2S     (0<<20)
#define  ADPA_CRT_HOTPLUG_SAMPLE_4S     (1<<20)
#define  ADPA_CRT_HOTPLUG_VOLTAGE_40    (0<<18)
#define  ADPA_CRT_HOTPLUG_VOLTAGE_50    (1<<18)
#define  ADPA_CRT_HOTPLUG_VOLTAGE_60    (2<<18)
#define  ADPA_CRT_HOTPLUG_VOLTAGE_70    (3<<18)
#define  ADPA_CRT_HOTPLUG_VOLREF_325MV  (0<<17)
#define  ADPA_CRT_HOTPLUG_VOLREF_475MV  (1<<17)
#define  ADPA_CRT_HOTPLUG_FORCE_TRIGGER (1<<16)

/* or SDVOB */
#define HDMIB   0xe1140
#define  PORT_ENABLE    (1 << 31)
#define  TRANSCODER_A   (0)
#define  TRANSCODER_B   (1 << 30)
#define  COLOR_FORMAT_8bpc      (0)
#define  COLOR_FORMAT_12bpc     (3 << 26)
#define  SDVOB_HOTPLUG_ENABLE   (1 << 23)
#define  SDVO_ENCODING          (0)
#define  TMDS_ENCODING          (2 << 10)
#define  NULL_PACKET_VSYNC_ENABLE       (1 << 9)
/* CPT */
#define  HDMI_MODE_SELECT	(1 << 9)
#define  DVI_MODE_SELECT	(0)
#define  SDVOB_BORDER_ENABLE    (1 << 7)
#define  AUDIO_ENABLE           (1 << 6)
#define  VSYNC_ACTIVE_HIGH      (1 << 4)
#define  HSYNC_ACTIVE_HIGH      (1 << 3)
#define  PORT_DETECTED          (1 << 2)

/* PCH SDVOB multiplex with HDMIB */
#define PCH_SDVOB	HDMIB

#define HDMIC   0xe1150
#define HDMID   0xe1160

#define PCH_LVDS	0xe1180
#define  LVDS_DETECTED	(1 << 1)

#define BLC_PWM_CPU_CTL2	0x48250
#define  PWM_ENABLE		(1 << 31)
#define  PWM_PIPE_A		(0 << 29)
#define  PWM_PIPE_B		(1 << 29)
#define BLC_PWM_CPU_CTL		0x48254

#define BLC_PWM_PCH_CTL1	0xc8250
#define  PWM_PCH_ENABLE		(1 << 31)
#define  PWM_POLARITY_ACTIVE_LOW	(1 << 29)
#define  PWM_POLARITY_ACTIVE_HIGH	(0 << 29)
#define  PWM_POLARITY_ACTIVE_LOW2	(1 << 28)
#define  PWM_POLARITY_ACTIVE_HIGH2	(0 << 28)

#define BLC_PWM_PCH_CTL2	0xc8254

#define PCH_PP_STATUS		0xc7200
#define PCH_PP_CONTROL		0xc7204
#define  PANEL_UNLOCK_REGS	(0xabcd << 16)
#define  EDP_FORCE_VDD		(1 << 3)
#define  EDP_BLC_ENABLE		(1 << 2)
#define  PANEL_POWER_RESET	(1 << 1)
#define  PANEL_POWER_OFF	(0 << 0)
#define  PANEL_POWER_ON		(1 << 0)
#define PCH_PP_ON_DELAYS	0xc7208
#define  EDP_PANEL		(1 << 30)
#define PCH_PP_OFF_DELAYS	0xc720c
#define PCH_PP_DIVISOR		0xc7210

#define PCH_DP_B		0xe4100
#define PCH_DPB_AUX_CH_CTL	0xe4110
#define PCH_DPB_AUX_CH_DATA1	0xe4114
#define PCH_DPB_AUX_CH_DATA2	0xe4118
#define PCH_DPB_AUX_CH_DATA3	0xe411c
#define PCH_DPB_AUX_CH_DATA4	0xe4120
#define PCH_DPB_AUX_CH_DATA5	0xe4124

#define PCH_DP_C		0xe4200
#define PCH_DPC_AUX_CH_CTL	0xe4210
#define PCH_DPC_AUX_CH_DATA1	0xe4214
#define PCH_DPC_AUX_CH_DATA2	0xe4218
#define PCH_DPC_AUX_CH_DATA3	0xe421c
#define PCH_DPC_AUX_CH_DATA4	0xe4220
#define PCH_DPC_AUX_CH_DATA5	0xe4224

#define PCH_DP_D		0xe4300
#define PCH_DPD_AUX_CH_CTL	0xe4310
#define PCH_DPD_AUX_CH_DATA1	0xe4314
#define PCH_DPD_AUX_CH_DATA2	0xe4318
#define PCH_DPD_AUX_CH_DATA3	0xe431c
#define PCH_DPD_AUX_CH_DATA4	0xe4320
#define PCH_DPD_AUX_CH_DATA5	0xe4324

/* CPT */
#define  PORT_TRANS_A_SEL_CPT	0
#define  PORT_TRANS_B_SEL_CPT	(1<<29)
#define  PORT_TRANS_C_SEL_CPT	(2<<29)
#define  PORT_TRANS_SEL_MASK	(3<<29)

#define TRANS_DP_CTL_A		0xe0300
#define TRANS_DP_CTL_B		0xe1300
#define TRANS_DP_CTL_C		0xe2300
#define TRANS_DP_CTL(pipe)	(TRANS_DP_CTL_A + (pipe) * 0x01000)
#define  TRANS_DP_OUTPUT_ENABLE	(1<<31)
#define  TRANS_DP_PORT_SEL_B	(0<<29)
#define  TRANS_DP_PORT_SEL_C	(1<<29)
#define  TRANS_DP_PORT_SEL_D	(2<<29)
#define  TRANS_DP_PORT_SEL_MASK	(3<<29)
#define  TRANS_DP_AUDIO_ONLY	(1<<26)
#define  TRANS_DP_ENH_FRAMING	(1<<18)
#define  TRANS_DP_8BPC		(0<<9)
#define  TRANS_DP_10BPC		(1<<9)
#define  TRANS_DP_6BPC		(2<<9)
#define  TRANS_DP_12BPC		(3<<9)
#define  TRANS_DP_BPC_MASK	(3<<9)
#define  TRANS_DP_VSYNC_ACTIVE_HIGH	(1<<4)
#define  TRANS_DP_VSYNC_ACTIVE_LOW	0
#define  TRANS_DP_HSYNC_ACTIVE_HIGH	(1<<3)
#define  TRANS_DP_HSYNC_ACTIVE_LOW	0
#define  TRANS_DP_SYNC_MASK	(3<<3)

/* SNB eDP training params */
/* SNB A-stepping */
#define  EDP_LINK_TRAIN_400MV_0DB_SNB_A		(0x38<<22)
#define  EDP_LINK_TRAIN_400MV_6DB_SNB_A		(0x02<<22)
#define  EDP_LINK_TRAIN_600MV_3_5DB_SNB_A	(0x01<<22)
#define  EDP_LINK_TRAIN_800MV_0DB_SNB_A		(0x0<<22)
/* SNB B-stepping */
#define  EDP_LINK_TRAIN_400_600MV_0DB_SNB_B	(0x0<<22)
#define  EDP_LINK_TRAIN_400MV_3_5DB_SNB_B	(0x1<<22)
#define  EDP_LINK_TRAIN_400_600MV_6DB_SNB_B	(0x3a<<22)
#define  EDP_LINK_TRAIN_600_800MV_3_5DB_SNB_B	(0x39<<22)
#define  EDP_LINK_TRAIN_800_1200MV_0DB_SNB_B	(0x38<<22)
#define  EDP_LINK_TRAIN_VOL_EMP_MASK_SNB	(0x3f<<22)

#define  FORCEWAKE				0xA18C
#define  FORCEWAKE_ACK				0x130090

#define  GT_FIFO_FREE_ENTRIES			0x120008

#define GEN6_RPNSWREQ				0xA008
#define   GEN6_TURBO_DISABLE			(1<<31)
#define   GEN6_FREQUENCY(x)			((x)<<25)
#define   GEN6_OFFSET(x)			((x)<<19)
#define   GEN6_AGGRESSIVE_TURBO			(0<<15)
#define GEN6_RC_VIDEO_FREQ			0xA00C
#define GEN6_RC_CONTROL				0xA090
#define   GEN6_RC_CTL_RC6pp_ENABLE		(1<<16)
#define   GEN6_RC_CTL_RC6p_ENABLE		(1<<17)
#define   GEN6_RC_CTL_RC6_ENABLE		(1<<18)
#define   GEN6_RC_CTL_RC1e_ENABLE		(1<<20)
#define   GEN6_RC_CTL_RC7_ENABLE		(1<<22)
#define   GEN6_RC_CTL_EI_MODE(x)		((x)<<27)
#define   GEN6_RC_CTL_HW_ENABLE			(1<<31)
#define GEN6_RP_DOWN_TIMEOUT			0xA010
#define GEN6_RP_INTERRUPT_LIMITS		0xA014
#define GEN6_RPSTAT1				0xA01C
#define GEN6_RP_CONTROL				0xA024
#define   GEN6_RP_MEDIA_TURBO			(1<<11)
#define   GEN6_RP_USE_NORMAL_FREQ		(1<<9)
#define   GEN6_RP_MEDIA_IS_GFX			(1<<8)
#define   GEN6_RP_ENABLE			(1<<7)
#define   GEN6_RP_UP_BUSY_MAX			(0x2<<3)
#define   GEN6_RP_DOWN_BUSY_MIN			(0x2<<0)
#define GEN6_RP_UP_THRESHOLD			0xA02C
#define GEN6_RP_DOWN_THRESHOLD			0xA030
#define GEN6_RP_UP_EI				0xA068
#define GEN6_RP_DOWN_EI				0xA06C
#define GEN6_RP_IDLE_HYSTERSIS			0xA070
#define GEN6_RC_STATE				0xA094
#define GEN6_RC1_WAKE_RATE_LIMIT		0xA098
#define GEN6_RC6_WAKE_RATE_LIMIT		0xA09C
#define GEN6_RC6pp_WAKE_RATE_LIMIT		0xA0A0
#define GEN6_RC_EVALUATION_INTERVAL		0xA0A8
#define GEN6_RC_IDLE_HYSTERSIS			0xA0AC
#define GEN6_RC_SLEEP				0xA0B0
#define GEN6_RC1e_THRESHOLD			0xA0B4
#define GEN6_RC6_THRESHOLD			0xA0B8
#define GEN6_RC6p_THRESHOLD			0xA0BC
#define GEN6_RC6pp_THRESHOLD			0xA0C0
#define GEN6_PMINTRMSK				0xA168

#define GEN6_PMISR				0x44020
#define GEN6_PMIMR				0x44024
#define GEN6_PMIIR				0x44028
#define GEN6_PMIER				0x4402C
#define  GEN6_PM_MBOX_EVENT			(1<<25)
#define  GEN6_PM_THERMAL_EVENT			(1<<24)
#define  GEN6_PM_RP_DOWN_TIMEOUT		(1<<6)
#define  GEN6_PM_RP_UP_THRESHOLD		(1<<5)
#define  GEN6_PM_RP_DOWN_THRESHOLD		(1<<4)
#define  GEN6_PM_RP_UP_EI_EXPIRED		(1<<2)
#define  GEN6_PM_RP_DOWN_EI_EXPIRED		(1<<1)

#define GEN6_PCODE_MAILBOX			0x138124
#define   GEN6_PCODE_READY			(1<<31)
#define   GEN6_READ_OC_PARAMS			0xc
#define GEN6_PCODE_DATA				0x138128

#endif /* _REGS_H_ */

/*----------------------------------------------------------------------------
 * File Revision History
 * $Id: regs.h,v 1.1.2.20 2012/05/16 23:49:38 astead Exp $
 * $Source: /nfs/sie/disks/sie-cidgit_disk001/git_repos/GFX/cvs_fri22/koheo/linux/egd_drm/emgd/include/gn6/Attic/regs.h,v $
 *----------------------------------------------------------------------------
 */
