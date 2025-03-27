/*
        ----------------------------------------------------------------------

        --- CAEN SpA - Computing Systems Division ---

        a3818.h

        Header file for the CAEN A3818 HS CONET board driver.

        September 2010:


        ----------------------------------------------------------------------
*/
#ifndef _a3818_H
#define _a3818_H

#ifndef VERSION
	#define VERSION(ver,rel,seq) (((ver)<<16) | ((rel)<<8) | (seq))
#endif

//        Defines for the a3818

#define MIN_DMA_SIZE                    (80)
#define MAX_MINOR                       (256)
#define PFX                             "a3818: "
#define MAX_V2718                       (8)
#define A3818_MAX_PKT_SZ                (0x20000)
#define A3818_RXFIFO_SZ                 (0x2000)
#define A3818_REGION_SIZE               (0x100)
#define PCI_VENDOR_XILINX_ID      		(0x10ee) // XILINX
#define PCI_DEVICE_ID_CAEN      		(0x1015) // CAEN
#define PCI_SUBSYSTEM_VENDOR_ID_XILINX  (0x10ee)
#define PCI_SUBSYSTEM_ID_VIRTEX         (0x0007)
#define A3818_MAGIC                     '8'

#define IOCTL_RESET						_IO(A3818_MAGIC, 0)
#define IOCTL_COMM						_IOWR(A3818_MAGIC, 1, a3818_comm_t)
#define IOCTL_REG_WR					_IOW(A3818_MAGIC, 2, a3818_reg_t)
#define IOCTL_REG_RD					_IOWR(A3818_MAGIC, 3, a3818_reg_t)
#define IOCTL_IRQ_WAIT					_IOW(A3818_MAGIC, 4, a3818_intr_t)
#define IOCTL_SEND						_IOWR(A3818_MAGIC, 5, a3818_comm_t)
#define IOCTL_RECV						_IOWR(A3818_MAGIC, 6, int)
#define IOCTL_REV						_IOWR(A3818_MAGIC, 7, a3818_rev_t)
#define IOCTL_COMM_REG_WR				_IOW(A3818_MAGIC, 8, a3818_reg_t)
#define IOCTL_COMM_REG_RD				_IOWR(A3818_MAGIC, 9, a3818_reg_t)

#define PCI_IOCTL_REG_RD			    _IOWR(A3818_MAGIC, 11, a3818_reg_t)     // AM per conoscere i registro del PCI
#define A3818_IOCTL_PCIE_BRIDGE_RESET   _IO(A3818_MAGIC, 12)

#define MAX_OPT_LINK 	          		(0x05)
#define A3818_COMMON_REG        		(0x05)

#define AT45DB321D_PAGE_SIZE 		264
#define MAX_TLP32_DATA_PAYLOAD		32

/************************************************/
/* A3818 ERROR MESSAGES */
#define A3818_OK               0
#define A3818_ERR_NOT_READY    -1

/************************************************
        A3818 Registers offsets & bits
*************************************************/
/************************************************/
/* LINK REGISTERS */
#define A3818_TXFIFO                    (0x00)
#define A3818_RXFIFO                    (0x04)
#define IOCTL                     		(0x08)
#define A3818_LNKRST                    (1)     // Link Reset
#define A3818_RDYINTDIS                 (1 << 1)// Disable interrupt for data ready
#define A3818_SERVICE                   (1 << 2)// SERV mode del CONET
#define A3818_VINTDIS                   (1 << 3)// Disable interrupt from VME
#define A3818_RES_LOCINT                (1 << 4)// Reset Local Interrupt
#define A3818_LINK_SR                   (0x0C)   // link status register
#define A3818_RXFIFO_EMPTY              (1)
#define A3818_RXFIFO_ALMFULL            (1 << 1)
#define A3818_TXFIFO_FULL               (1 << 2)
#define A3818_VMEINT                    (1 << 3)
#define A3818_LINKON                    (1 << 5)
#define A3818_LINKRST_STAT              (1 << 6)
#define A3818_SERV                      (1 << 7)
#define A3818_LINK_FAIL                 (1 << 8)
#define A3818_LINT                      (1 << 9)
#define A3818_LINK_TX_RES               (1 << 16)
#define A3818_LINK_RX_RES               (1 << 17)

#define A3818_LINK_TRS                  (0x18)   // link transfer status register
#define A3818_DEBUG                     (0x20)
#define A3818_IRQSTAT_0					(0x24)	// IRQ Stat dagli slave 0 to 3
#define A3818_IRQSTAT_1  				(0x28)  // IRQ Stat dagli slave 4 to 7
#define A3818_IRQMASK_0_S				(0x30)	// IRQ Mask slave 0 to 3 - Set
#define A3818_IRQMASK_0_C				(0x34)	// IRQ Mask slave 0 to 3 - Clear
#define A3818_IRQMASK_1_S				(0x38)	// IRQ Mask slave 4 to 7 - Set
#define A3818_IRQMASK_1_C				(0x3C)	// IRQ Mask slave 4 to 7 - Clear
#define A3818_IOCTL_S					(0xA0)	// - IOCTL register set
#define A3818_IOCTL_C					(0xA4)	// - IOCTL register clear

/* DMA LINK REGISTERS*/
#define A3818_DMACSR                    (0x100)   // DMA Control Status Register
#define A3818_DMAREADSTART      		(1 << 0)  // DMA Memory Read Start
#define A3818_DMAREADDONE       		(1 << 8)  // DMA Memory Read Done
#define A3818_RES_DMAINT	        	(1 << 31) // DMA Initiator Reset
#define A3818_RDMATLPA                  (0x104)   // Read DMA Memory Address
#define A3818_RDMALADR                  (0x108)   // Read DMA Local Address
#define A3818_RDMASIZE                  (0x10C)   // Read DMA Size
#define A3818_RDMAMODE                  (0x110)   // Read DMA Mode
#define A3818_DMAMODE_INTDIS			(1 << 6)  // Read Interrupt Disable
#define A3818_WDMAPERF                  (0x114)   // DMA Performances Write to PC
#define A3818_DMACSR_S					(0x118)   // DMA Control Status Register - register set

/************************************************/
/* A3818 COMMON REGISTER */
#define A3818_DLWSTAT                   (0x00)   // Device Link Width Status
#define A3818_DLTRSSTAT                 (0x04)   // Device Link Transaction Size Status
#define A3818_DMISCCS                   (0x08)   // Device Miscellaneous Control Status

#define  A3818_DMISCCS_CPL_STREAM        (1 << 0) // Completion Streaming Enable (default 1)
#define  A3818_DMISCCS_RD_METERING       (1 << 1) //  Read Metering Enable (default 0)
#define  A3818_DMISCCS_REC_NON_POSTED_OK (1 << 2) //  Receive Non-Posted OK (default 0)

#define  A3818_DMISCCS_SPI_BPI_FLASH_SEL (1 << 4) //  BPI/SPI FLASH access select (default 0 = SPI FLASH)
#define  A3818_DMISCCS_SPI_FLASH_RDY     (1 << 5) //  ATMEL SPI Flash Memory Ready (RO)
#define  A3818_DMISCCS_ADC_CLK_SEL       (1 << 6) //  ADC CLK Select (default 1 = SI571 clock)
#define  A3818_DMISCCS_V_PB_EN           (1 << 7) //  PB Power enable (default 1 = PB Power ON)
 
#define  A3818_DMISCCS_RDDMA_DONE0       (1 << 16)//  Link 0 DMA READ Done (RO)
#define  A3818_DMISCCS_RDDMA_DONE1       (1 << 17)//  Link 1 DMA READ Done (RO)
#define  A3818_DMISCCS_RDDMA_DONE2       (1 << 18)//  Link 2 DMA READ Done (RO)
#define  A3818_DMISCCS_RDDMA_DONE3       (1 << 19)//  Link 3 DMA READ Done (RO)
#define  A3818_DMISCCS_RDDMA_DONE4       (1 << 20)//  Link 4 Memory Write Done (RO)
 
#define  A3818_DMISCCS_LOC_INT0          (1 << 24)//  Link 0 Local Interrupt   (RO)
#define  A3818_DMISCCS_LOC_INT1          (1 << 25)//  Link 1 Local Interrupt   (RO)
#define  A3818_DMISCCS_LOC_INT2          (1 << 26)//  Link 2 Local Interrupt   (RO)
#define  A3818_DMISCCS_LOC_INT3          (1 << 27)//  Link 3 Local Interrupt   (RO)
#define  A3818_DMISCCS_LOC_INT4          (1 << 28)//  Link 4 Local Interrupt   (RO)

#define A3818_GTPRES                    (0x0C)   // W   - GTP reset

#define A3818_SPI_FLASH                 (0x10)   // R/W - ATMEL SPI Flash Memory access
#define A3818_SPI_FLEN                  (0x14)   // R/W - ATMEL SPI Flash Memory enable
#define A3818_SPI_RELOAD                (0x18)   // W   - Re-read the SPI flash content

#define A3818_BPI_FLASH_AD              (0x20)   // R/W - BPI_FLASH TEST
#define A3818_BPI_FLASH_DT              (0x24)   // R/W
#define A3818_BPI_FLASH_CMD             (0x28)   // R/W

#define A3818_PHY_I2C_COMM              (0x30)   // W
#define A3818_PHY_I2C_DAT               (0x34)   // R

#define A3818_ADC_I2C_COMM              (0x38)   // W
#define A3818_ADC_I2C_DAT               (0x3C)   // R

#define A3818_FWREV                     (0x40)   // R   - Firmware Revision
#define A3818_BOARD_ID                  (0x44)   // R   - Board ID
#define A3818_BOARD_VERS_ID             (0x48)   // R   - Board Version ID
#define A3818_BOARD_SERNUM              (0x4C)   // R   - Board Serial Number
#define A3818_TEMP                      (0x50)   // R   - rilettura del sensore di temp. TMP422

#define A3818_SSRAM_AD                  (0x60)   // R/W - SSRAM TEST
#define A3818_SSRAM_DT_L                (0x64)   // R/W
#define A3818_SSRAM_DT_M                (0x68)   // R/W
#define A3818_SSRAM_DT_H                (0x6C)   // R/W
#define A3818_SSRAM_CMD                 (0x70)   // R/W - SSRAM TEST
/************************************************/


#define A3818BOARD                      (0x0eea)
#define ONE_LINK                     	(0x0a)
#define TWO_LINK                     	(0x0b)
#define FOUR_LINK                    	(0x0c)

#define A3818DIGIT                      (0x0d)
#define A3818RAW						(0xffffff)

// Registri PCIE
#define PCIE_DMA_CONF_REG               (0x68)
#define PCIE_MSI_CAPAB_REG				(0x48)
#define PCIE_CONF_STAT				(0x06)
#define PCIE_DEV_STAT				(0x6A)


#ifdef DEBUG
#define DPRINTK(format,args...) 		printk(format,##args)
#else
#define DPRINTK(format,args...)
#endif


//#ifdef __KERNEL__

/*
        ----------------------------------------------------------------------

        Types

        ----------------------------------------------------------------------
*/
struct a3818_state {
        /* Common globals */
        unsigned char           *baseaddr[MAX_OPT_LINK + 1];  // reindirizzamento dei 5 base address del pci
        unsigned long            phys[MAX_OPT_LINK + 1];      // indirizzo fisico dei 5 base address del pci
        struct pci_dev	        *pcidev;
        int                      irq;
		int 					 major;
        int                      CardNumber;
        int                      timeout;
        int       				 NumOfLink;
        int						 TypeOfBoard;
        struct workqueue_struct *a3818_WorkQueue;
        struct work_struct	 	*a3818_work;
        int						 a3818EndWorkQueue;
        wait_queue_head_t        WorkQueue_wait;
        int						 WorkQueue_MustWakeUp;
        int						 SlaveOnLink[MAX_OPT_LINK];		// per conoscere lo slave associato al link utilizzato
        int						 Occurency[MAX_OPT_LINK];
        unsigned char           *buff_dma_in[MAX_OPT_LINK];            // buffer per i dma. CPU address.
        dma_addr_t               phy_dma_addr_in[MAX_OPT_LINK];  // PCI address of buff_dma_in.
        int                      ndata_dma_in[MAX_OPT_LINK];
        uint32_t                 tr_stat[MAX_OPT_LINK];
        unsigned int             ioctls[MAX_OPT_LINK];
        unsigned char			 GTPReset;
		unsigned char			 LocalInterrupt[MAX_OPT_LINK];
		unsigned char			 DMADone[MAX_OPT_LINK];
        unsigned int             DMAInProgress[MAX_OPT_LINK];
		uint32_t				 irq0[MAX_OPT_LINK];
		uint32_t				 irq1[MAX_OPT_LINK];
        spinlock_t               DataLinklock[MAX_OPT_LINK];
		spinlock_t				 CardLock;
        struct semaphore         ioctl_lock[MAX_OPT_LINK];
        /* Per slave globals */
        uint32_t                *buff_dma_out[MAX_OPT_LINK][MAX_V2718];
        dma_addr_t               phy_dma_addr_out[MAX_OPT_LINK][MAX_V2718];
        wait_queue_head_t        read_wait[MAX_OPT_LINK][MAX_V2718];
        wait_queue_head_t        intr_wait[MAX_OPT_LINK][MAX_V2718];
        unsigned char           *app_dma_in[MAX_OPT_LINK][MAX_V2718];
        int                      pos_app_dma_in[MAX_OPT_LINK][MAX_V2718];
        int                      ndata_app_dma_in[MAX_OPT_LINK][MAX_V2718];
        int                      rx_ready[MAX_OPT_LINK][MAX_V2718];
        int                      rx_status[MAX_OPT_LINK][MAX_V2718];
        unsigned int             intr[MAX_OPT_LINK][MAX_V2718];
        unsigned int			 FwRel;
        uint32_t                 LocReg_A3818_DMISCCS;
        /* we keep a3818 cards in a linked list */
        struct                   a3818_state *next;
};
/*
	Struct for communication argument in ioctl calls
*/
typedef struct a3818_comm {
        const char *out_buf;
        int         out_count;
        char       *in_buf;
        int         in_count;
        int        *status;
} a3818_comm_t;

/*
	Struct for register argument in ioctl calls
*/
typedef struct a3818_reg {
	uint32_t address;
	uint32_t value;
} a3818_reg_t;

/*
	Struct for interrupt argument in ioctl calls
*/
typedef struct a3818_intr {
        unsigned int Mask;
        unsigned int Timeout;
} a3818_intr_t;

/*
	Struct for revision argument in ioctl calls
*/
#define A3818_DRIVER_VERSION_LEN	20
typedef struct a3818_rev {
        char 		rev_buf[A3818_DRIVER_VERSION_LEN];
} a3818_rev_t;

#endif
