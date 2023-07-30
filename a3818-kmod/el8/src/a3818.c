/*
        ----------------------------------------------------------------------

        --- Caen SpA - Computing Systems Division ---

        a3818.c

        Source file for the CAEN A3818 HS CONET board driver.

        September   2010 : Created.
		November     2013 : Last Release.

        ----------------------------------------------------------------------
*/

#define USE_MIDAS 0

#define USE_MSI_IRQ 1

#ifndef VERSION
	#define VERSION(ver,rel,seq) (((ver)<<16) | ((rel)<<8) | (seq))
#endif
/*
        Version Information
*/
#define DRIVER_VERSION "v1.6.8s"
#define DRIVER_AUTHOR "CAEN Computing Division  support.computing@caen.it"
#define DRIVER_DESC "CAEN A3818 PCI Express CONET2 board driver"

#ifdef CONFIG_SMP
#  define __SMP__
#endif
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/pagemap.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/pci.h>
#if LINUX_VERSION_CODE >= VERSION(2,6,26)
#include <linux/vmalloc.h>
#ifdef CAEN_ASPM_EXISTS 
#include <linux/pci-aspm.h>
#endif
#endif
#include <linux/poll.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/sched.h>
#include "a3818.h"

/*
        ----------------------------------------------------------------------
        a3818_mmiowb
        ----------------------------------------------------------------------
*/

#ifndef VERSION
	#define VERSION(ver,rel,seq) (((ver)<<16) | ((rel)<<8) | (seq))
#endif

#define a3818_mmiowb()


#define ENABLE_RX_INT(opt_link)\
{\
	writel(A3818_RDYINTDIS, s->baseaddr[opt_link] + A3818_IOCTL_C);\
	a3818_mmiowb();\
}

#define DISABLE_RX_INT(opt_link)\
{\
	writel(A3818_RDYINTDIS, s->baseaddr[opt_link] + A3818_IOCTL_S);\
	a3818_mmiowb();\
}

/*      ----------------------------------------------------------------------

        Function prototypes

        ----------------------------------------------------------------------
*/

static int a3818_open(struct inode *, struct file *);
static int a3818_release(struct inode *, struct file *);
static int a3818_ioctl(struct inode *, struct file *, unsigned int,unsigned long);
#if LINUX_VERSION_CODE >= VERSION(2,6,11)
static long a3818_ioctl_unlocked(struct file *, unsigned int, unsigned long);
#endif
static struct class *a3818_class;
#if LINUX_VERSION_CODE >= VERSION(3,10,0)
static ssize_t a3818_procinfo(struct file* filp, char* buf, size_t count, loff_t* pos);
#else
static int a3818_procinfo(char *, char **, off_t, int, int *,void *);
#endif
static void a3818_handle_rx_pkt(struct a3818_state *s, int opt_link, int pp);
static void a3818_dispatch_pkt(struct a3818_state *s, int opt_link);
static void ReleaseBoards(void);
static int finished = 0;

/*
  ----------------------------------------------------------------------
  
  Global variables
  
  ----------------------------------------------------------------------
*/

static int a3818_major = 0;
static struct a3818_state *devs;
static struct proc_dir_entry *a3818_procdir;

#if  LINUX_VERSION_CODE >= VERSION(5,6,0)
		static struct proc_ops a3818_procdir_fops = {
		.proc_read = a3818_procinfo
		};
#else
        #if LINUX_VERSION_CODE >= VERSION(3,10,0)
        static struct file_operations a3818_procdir_fops = {
         read: a3818_procinfo
        };
        #endif
#endif


static struct file_operations a3818_fops =
{
#if LINUX_VERSION_CODE >= VERSION(2,6,11)
        unlocked_ioctl: 	a3818_ioctl_unlocked,
#else
		ioctl:				a3818_ioctl,
#endif
        open:     			a3818_open,
        release:  			a3818_release
};

static struct pci_device_id a3818_pci_tbl[] =
{
  { PCI_VENDOR_XILINX_ID, PCI_DEVICE_ID_CAEN, PCI_SUBSYSTEM_VENDOR_ID_XILINX, PCI_SUBSYSTEM_ID_VIRTEX },
  { 0 }
};

/*
  -----------------------------------------------------------------------
  
  Static functions
  
  -----------------------------------------------------------------------
*/

static void a3818_dma_conf(struct a3818_state *s) {
	uint i;
	int ret;

	for( i = 0; i < s->NumOfLink; i++ ) {
		// Set DMA registry with memory address
		writel(s->phy_dma_addr_in[i], s->baseaddr[i] + A3818_RDMATLPA);
		ret = readl(s->baseaddr[i] + A3818_RDMATLPA);
		writel(A3818_RXFIFO, s->baseaddr[i] + A3818_RDMALADR);
		ret = readl(s->baseaddr[i] + A3818_RDMALADR);
		writel(MAX_TLP32_DATA_PAYLOAD, s->baseaddr[i] + A3818_RDMASIZE);
		ret = readl(s->baseaddr[i] + A3818_RDMASIZE);
		// Enable the IRQ
		writel(0x0, s->baseaddr[i] + A3818_RDMAMODE);
		ret = readl(s->baseaddr[i] + A3818_RDMAMODE);
		writel(A3818_RES_DMAINT, s->baseaddr[i] + A3818_DMACSR_S);
	}
	a3818_mmiowb();
}

/*
        ----------------------------------------------------------------------

        a3818_reset_comm

        ----------------------------------------------------------------------
*/
static int a3818_reset_comm(struct a3818_state *s, int o_link)
{
	int val;
	uint ret = A3818_OK;

	//Link Reset
	writel(A3818_LNKRST, s->baseaddr[o_link] + A3818_IOCTL_S);
	msleep(1);  // Wait 1 msec
	// Remove Link Reset
	writel(A3818_LNKRST, s->baseaddr[o_link] + A3818_IOCTL_C);
    	// Check RX e TX
	val = readl(s->baseaddr[o_link] + A3818_LINK_SR);
	if( !(val & A3818_LINK_TX_RES) && !(val & A3818_LINK_RX_RES) ) {
		return A3818_OK;
	}
	else {
		ret = A3818_ERR_NOT_READY;
		return ret;
	}
}

/*
        ----------------------------------------------------------------------

        a3818_reset

        ----------------------------------------------------------------------
*/

static int a3818_reset_onopen(struct a3818_state *s) {
	int val;

	uint i, ret = A3818_OK;
	DPRINTK("ResetOnOpen\n");
	// put all active links in reset
	for(i = 0; i< s->NumOfLink; i++){
		writel(A3818_LNKRST, s->baseaddr[i] + A3818_IOCTL_S);
		DPRINTK("LINK %d LINKRES ON\n",i);
	}
    	// reset all GTP
	val = 1;
	DPRINTK("GTPs reset \n" );
	writel(val, s->baseaddr[A3818_COMMON_REG] + A3818_GTPRES);

	// wait the GTP reset.
	msleep(10);
	// check each link
	for( i = 0; i < s->NumOfLink; i++) {
		val = readl(s->baseaddr[i] + A3818_LINK_SR);
		if( !(val & A3818_LINK_TX_RES) ) {
			DPRINTK("LINK %d TX OK\n",i );
		}
		else {
			DPRINTK("LINK %d TX KO = %x\n",i, val );
			ret = A3818_ERR_NOT_READY;
			return ret;
		}
	}

	// force a linkres_fsm_n to conet_master
    	// remove link reset
	for(i = 0; i< s->NumOfLink; i++){
		writel(A3818_LNKRST, s->baseaddr[i] + A3818_IOCTL_C);
		DPRINTK("LINK %d LINKRES OFF\n",i);
	}
	msleep(1);
	return A3818_OK;
}

static int a3818_reset(struct a3818_state *s) {
	int val;

	uint i, ret = A3818_OK;
	val = readl(s->baseaddr[A3818_COMMON_REG] + A3818_DMISCCS);
	if( val & A3818_DMISCCS_SPI_FLASH_RDY )
      DPRINTK("A3818 SPI FLASH READY %x\n", val );
	else {
      DPRINTK("A3818 SPI FLASH NOT READY %x\n", val );
      ret = A3818_ERR_NOT_READY;
      return ret;
	}


    // reset all GTP
	val = 1;
	DPRINTK("GTPs reset \n" );
	writel(val, s->baseaddr[A3818_COMMON_REG] + A3818_GTPRES);

	// wait the GTP reset.
	msleep(10);
	// check each link
	for( i = 0; i < s->NumOfLink; i++) {
		val = readl(s->baseaddr[i] + A3818_LINK_SR);
		if( !(val & A3818_LINK_TX_RES) ) {
			DPRINTK("LINK %d TX OK\n",i );
		}
		else {
			DPRINTK("LINK %d TX KO = %x\n",i, val );
			ret = A3818_ERR_NOT_READY;
			return ret;
		}
	}

	// force a linkres_fsm_n to conet_master
    	// remove link reset
	for(i = 0; i< s->NumOfLink; i++){
		writel(A3818_LNKRST, s->baseaddr[i] + A3818_IOCTL_C);
		DPRINTK("LINK %d LINKRES OFF\n",i);
	}
	msleep(1);
    	// put all active links in reset
	for(i = 0; i< s->NumOfLink; i++){
		writel(A3818_LNKRST, s->baseaddr[i] + A3818_IOCTL_S);
		DPRINTK("LINK %d LINKRES ON\n",i);
	}
	return A3818_OK;
}

static void a3818_PCIE_reset(struct a3818_state *s) {
	u8 ctrl;
	u16  vendor;
	u16  device;
	u8 header_type;
	int i;
	struct pci_dev *dev;
	dev = s->pcidev;


	for (i = 0; i < 16; i++) {		
	   pci_read_config_dword(dev->bus->self, i * 4, &dev->saved_config_space[i]);
	   DPRINTK("dev->bus->self config[%d] = %8X\n", i, dev->saved_config_space[i]);
	}
	DPRINTK("pci_name(dev) = %s\n", pci_name(dev));
	DPRINTK("pci_name(dev->bus->self) = %s\n", pci_name(dev->bus->self));
	DPRINTK("pci_name(dev->bus->parent->self) = %s\n", pci_name(dev->bus->parent->self));
	pci_read_config_word(dev->bus->self, PCI_VENDOR_ID, &vendor);
	pci_read_config_word(dev->bus->self, PCI_DEVICE_ID, &device);
	pci_read_config_byte(dev->bus->self, PCI_HEADER_TYPE, &header_type);
	DPRINTK("hot reset start\n");
	DPRINTK("pci_dev name = %s\n", dev->bus->name);
	DPRINTK("pci_dev VendorID = %X\n", vendor);
	DPRINTK("pci_dev DeviceID = %X\n", device);
	DPRINTK("pci_dev HeaderType = %X\n", header_type);
	DPRINTK("pci_dev self bus number = %d\n", dev->bus->number);
	DPRINTK("pci_dev self bus primary = %d\n", dev->bus->primary);
	DPRINTK("pci_dev self bus secondary = %d\n", dev->bus->secondary);
	DPRINTK("pci_dev self bus subordinate = %d\n", dev->bus->subordinate);
	pci_read_config_word(dev->bus->parent->self, PCI_VENDOR_ID, &vendor);
	pci_read_config_word(dev->bus->parent->self, PCI_DEVICE_ID, &device);
	pci_read_config_byte(dev->bus->self, PCI_HEADER_TYPE, &header_type);
	DPRINTK("pci_dev parent name = %s\n", dev->bus->parent->name);
	DPRINTK("pci_dev VendorID = %X\n", vendor);
	DPRINTK("pci_dev DeviceID = %X\n", device);
	DPRINTK("pci_dev HeaderType = %X\n", header_type);
	DPRINTK("pci_dev parent self bus number = %d\n", dev->bus->parent->number);
	DPRINTK("pci_dev parent self bus primary = %d\n", dev->bus->parent->primary);
	DPRINTK("pci_dev parent self bus secondary = %d\n", dev->bus->parent->secondary);
	DPRINTK("pci_dev parent self bus subordinate = %d\n", dev->bus->parent->subordinate);
	/* read BUS RESET bit to implements the Hot Reset */
	pci_read_config_byte(dev->bus->self, PCI_BRIDGE_CONTROL, &ctrl);
	DPRINTK("pci_dev ctrl value before reset = %X\n", ctrl);
	ctrl |= PCI_BRIDGE_CTL_BUS_RESET;
	pci_write_config_byte(dev->bus->self, PCI_BRIDGE_CONTROL, ctrl);
	msleep(200);
	ctrl &= ~PCI_BRIDGE_CTL_BUS_RESET;
	pci_write_config_byte(dev->bus->self, PCI_BRIDGE_CONTROL, ctrl);
	DPRINTK("pci_dev ctrl value after reset = %X\n", ctrl);
	msleep(200);
    DPRINTK("hot reset end\n");
	return;
}

/*
        ----------------------------------------------------------------------

        a3818_send_pkt

        ----------------------------------------------------------------------
*/
static int a3818_send_pkt(struct a3818_state *s, int slave, int opt_link, const char *buf, int count) {
	int nlw, i;

	DISABLE_RX_INT(opt_link);

	/* 32-bit word alignment */
	nlw = (count >> 2) + (count & 3 ? 1 : 0) + 1;
	/* Send data without DMA (A3818 doesn't support DMA write facilitie) */
	for( i = 0; i < nlw; i++ ) {
		writel(s->buff_dma_out[opt_link][slave][i], s->baseaddr[opt_link] + A3818_TXFIFO);
		DPRINTK("Pck[%d] : %x\n", i, s->buff_dma_out[opt_link][slave][i]);
	}
	return count;
}

/*
        ----------------------------------------------------------------------

        a3818_recv_pkt

        ----------------------------------------------------------------------
*/
static int a3818_recv_pkt(struct a3818_state *s, int slave, int opt_link, int *status) {
	int ret;
	int i;
	uint32_t tr_stat;
	int startDMA = 0;
//	DISABLE_RX_INT(opt_link);
	DPRINTK("a3818_recv_pkt: DISABLE_RX_INT\n");

	spin_lock_irq( &s->DataLinklock[opt_link]);

	if( !s->rx_ready[opt_link][slave] && !s->DMAInProgress[opt_link] && !s->LocalInterrupt[opt_link]) {
		for( i = 0; i < 15; i++ ) {
			tr_stat = readl(s->baseaddr[opt_link] + A3818_LINK_TRS);
			if( tr_stat == 0xFFFFFFFF ) {
				DPRINTK("rcv-pkt: Error: tr_stat = %x, i= %d\n", s->tr_stat[opt_link],i);
				break;
			}
			else if( tr_stat & 0xFFFF0000 ) {
				s->tr_stat[opt_link] = tr_stat;
				DPRINTK("rcv-pkt (few data): tr_stat = %x, i= %d\n", s->tr_stat[opt_link],i);
				a3818_handle_rx_pkt(s, opt_link, 0);
				startDMA = 1;
				break;
			}
		}
	}
	DPRINTK("rcv-pkt: tr_stat = %x, i= %d\n", tr_stat,i);
	spin_unlock_irq( &s->DataLinklock[opt_link]);


	if (!startDMA) ENABLE_RX_INT(opt_link);

	DPRINTK("rcv-pkt: ENABLE_RX_INT\n");

#if USE_MIDAS
	ret = wait_event_timeout(
#else
	ret = wait_event_interruptible_timeout(
#endif		
					s->read_wait[opt_link][slave],
					s->rx_ready[opt_link][slave],
					s->timeout);

	DPRINTK("rcv-pkt: Wake-up = %d\n", ret );
	if( ret == 0 ) {
		/* Timeout reached */
		ret = -ETIMEDOUT;
		printk(KERN_ERR PFX "rcv-pkt: Timeout on RX -> Link %d\n",opt_link);
        goto err_recv;
	}
	else if( ret < 0 ) {
			/* Interrupted by a signal */
			ret = -EINTR;
			goto err_recv;
	}
	ret = s->ndata_app_dma_in[opt_link][slave];
	DPRINTK("rcv-pkt: read-ret = %d\n", ret );
	DPRINTK("rcv-pkt: s->rx_status[%d] = %x\n", slave, s->rx_status[opt_link][slave]  );

err_recv:

	DPRINTK("rcv-pkt: Data in buff_dma_in :\n");
	

	return ret;
}

/*
        ----------------------------------------------------------------------

        a3818_handle_rx_pkt

        ----------------------------------------------------------------------
*/
static void a3818_handle_rx_pkt(struct a3818_state *s, int opt_link, int pp)
{
	int DMA_size, i, DMA_size_word;
	u32 mia_var;


	DISABLE_RX_INT(opt_link);
	DPRINTK("a3818_handle_rx_pkt: DISABLE_RX_INT\n");

	/* Check on the length */
	DMA_size_word = (s->tr_stat[opt_link] >> 16);
	DMA_size = DMA_size_word * 4;

	DPRINTK("a3818_handle_rx_pkt: OLDDMA_size = %d\n", DMA_size_word );

	if( DMA_size < MIN_DMA_SIZE ) {
		DPRINTK("a3818_handle_rx_pkt NO DMA: SIZE = %d\n",DMA_size_word);
		for( i = 0; i < DMA_size_word; i++ ){
			mia_var = readl(s->baseaddr[opt_link] + A3818_RXFIFO);
			DPRINTK("a3818_handle_rx_pkt: i = %d, rx fifo = %x\n",i,mia_var);
			*(u32 *)(s->buff_dma_in[opt_link] + i*4) = mia_var;
		}
		s->ndata_dma_in[opt_link] = DMA_size;
		// Dispatch global buffer in per-slave buffers
		a3818_dispatch_pkt(s, opt_link);
		ENABLE_RX_INT(opt_link);
		DPRINTK("ENABLE_RX_INT\n");
	}
	else {
		
		if( DMA_size <= A3818_MAX_PKT_SZ ) {
			s->DMAInProgress[opt_link] = 1;

			writel(DMA_size_word, s->baseaddr[opt_link] + A3818_RDMASIZE); // AUG secondo me come sotto
			s->ndata_dma_in[opt_link] = DMA_size_word * 4;
			DPRINTK("[%d]a3818_handle_rx_pkt: HANDLE_RX: NEWDMA SIZE WORD= %d\n",opt_link,DMA_size_word);
			//printk("[%d]%d,",opt_link,DMA_size_word);
			DPRINTK("a3818_handle_rx_pkt: DMA start...\n");
	        // DMA start
			writel(A3818_DMAREADSTART, s->baseaddr[opt_link] + A3818_DMACSR_S);
			a3818_mmiowb();
		}
		else {
			DPRINTK("a3818_handle_rx_pkt: PROBLEM ON DMA_Size %d\n", DMA_size );
		}
	}
}


/*
        ----------------------------------------------------------------------

        a3818_dispatch_pkt

        ----------------------------------------------------------------------
*/
static void a3818_dispatch_pkt(struct a3818_state *s, int opt_link)
{
	int i, pkt_sz, slave, nlw, last_pkt, pos;
	u32  mask = 0;
	char *buff_dma_in = s->buff_dma_in[opt_link];
	char *iobuf;
	int ResidualDMASize = s->ndata_dma_in[opt_link];
	//i is the DMA buffer index with an header of 4 byte
	i = 0;
	do {
		DPRINTK("a3818_dispatch_pkt: header %d: %x %x %x %x\n",i,buff_dma_in[i+3],buff_dma_in[i+2],buff_dma_in[i+1],buff_dma_in[i]);
		pkt_sz = (buff_dma_in[i] & 0xFF) + ((buff_dma_in[i+1] << 8) & 0x0100);
		last_pkt = buff_dma_in[i+1] & 0x02;
		i += 3;
		slave = buff_dma_in[i++] & 0xF;
		mask |= 1 << slave;
		nlw = (pkt_sz >> 1) + (pkt_sz & 1);
		DPRINTK("a3818_dispatch_pkt: Numero di word %d\n", nlw);
		if( last_pkt ) {
			pkt_sz -= 1;    // per togliere lo status
			s->rx_status[opt_link][slave] = (buff_dma_in[i + pkt_sz * 2 - 1] << 8) + buff_dma_in[i + pkt_sz * 2];
			DPRINTK("a3818_dispatch_pkt: status = %02x slave = %02x\n", s->rx_status[opt_link][slave], slave);
		}
		if( slave >= MAX_V2718 ) {
            DPRINTK("a3818_dispatch_pkt: Wrong packet %04x %02x\n", pkt_sz, slave);
			return;
		}

		// slave buffer
		iobuf = s->app_dma_in[opt_link][slave];
		// starting position
		pos = s->pos_app_dma_in[opt_link][slave];
		/* Some integrity checks */
		if( (nlw <= (ResidualDMASize / 4)) &&
				(slave < MAX_V2718) && (pkt_sz >= 0) &&
				(pkt_sz <= 0x100) ) {

			// Copy of data ready
			memcpy(&(iobuf[pos]), &(buff_dma_in[i]), pkt_sz * 2);
			ResidualDMASize -= pkt_sz * 2;
			s->ndata_app_dma_in[opt_link][slave] += pkt_sz * 2;
			i += pkt_sz * 2;
			s->pos_app_dma_in[opt_link][slave] += pkt_sz * 2;
			if( last_pkt ) {
				i += 2; // to wipe out the status word
				s->rx_ready[opt_link][slave] = 1;
#if USE_MIDAS
				wake_up(&s->read_wait[opt_link][slave]);
#else
				wake_up_interruptible(&s->read_wait[opt_link][slave]);
#endif						
			}
			i += i % 4;  // 32-bit alignment
		}
		else {	
			DPRINTK("[%d]disp_pkt: Unhandled packet - slave %d, pkt_sz %d\n", opt_link, slave, pkt_sz);
			break;
		}
	} while( i < s->ndata_dma_in[opt_link] );
	return;
}


/*      ----------------------------------------------------------------------

        a3818_handle_vme_irq

        ----------------------------------------------------------------------
*/
static void a3818_handle_vme_irq(u32 irq0, u32 irq1, struct a3818_state *s, int opt_link) {
        int i;
        DPRINTK("a3818_handle_vme_irq: irq0 0x%08x - irq1 0x%08x\n", irq0, irq1);
        for( i = 0; i < MAX_V2718; i++ ) {
            /* Save IRQ levels on global struct */
					s->intr[opt_link][i] = ((i < 4 ? irq0 : irq1) & (0xFF << (i * 8))) >> (i * 8);
					if( s->intr[opt_link][i] )
					/* Wake up sleeping processes */
#if USE_MIDAS
					wake_up(&s->intr_wait[opt_link][i]);
#else
						wake_up_interruptible(&s->intr_wait[opt_link][i]);				
#endif								
        }
				// maschero i bit delle irq VME che ho gestito 
				writel(irq0,s->baseaddr[opt_link] + A3818_IRQMASK_0_C);
				writel(irq1,s->baseaddr[opt_link] + A3818_IRQMASK_1_C);
				//riabilito le interruzioni VME su questo link
				writel(A3818_VINTDIS,s->baseaddr[opt_link] + A3818_IOCTL_C);
}


/*
        ----------------------------------------------------------------------

        a3818_procinfo

        ----------------------------------------------------------------------
*/
#if LINUX_VERSION_CODE >= VERSION(3,10,0)
static ssize_t a3818_procinfo(struct file* filp, char __user *buf, size_t count, loff_t* pos) {
    char *tbuf;
	int len=0;
	struct a3818_state* s = devs;
    int i=0, j=0, k;
    tbuf = kmalloc(0x1000,GFP_KERNEL);
    if( !tbuf ) {
        finished = 0;
        return 0;
    }

	if(*pos > 0 || count < 0x1000)
		return 0;
    
    len = sprintf(tbuf+len,"\nCAEN A3818 driver %s\n\n", DRIVER_VERSION);

    
    while( s ) {
        k = 0;
        len += sprintf(tbuf+len, "  CAEN A3818 PCI Expresss CONET2 Board found.\n");
        len += sprintf(tbuf+len, "  Card  number                  = %d\n", (int)s->CardNumber);
        len += sprintf(tbuf+len, "  Firmware Release              = %x\n", (int)(s->FwRel));
        for (j=0; j<s->NumOfLink; j++) {
          len += sprintf(tbuf+len, "  Physical address Link %d       = %p\n",j, (void *)(s->phys)[j]);
          len += sprintf(tbuf+len, "  Virtual  address Link %d       = %p\n",j, s->baseaddr[j]);
        }
        len += sprintf(tbuf+len, "  Physical address Common Space = %p\n", (void *)(s->phys)[5]);
        len += sprintf(tbuf+len, "  Virtual  address Common Space = %p\n", s->baseaddr[5]);
        len += sprintf(tbuf+len, "  IRQ line                      = %d\n", (int)s->irq);
        len += sprintf(tbuf+len, "  Opened Links                  = %d\n", k);
        for (j=0; j<s->NumOfLink; j++) {
          len += sprintf(tbuf+len, "  Ioctls on Link %d              = %i\n", j, s->ioctls[j]);
        }
        len += sprintf(tbuf+len,"\n");
        s = s->next;
        i++;
    }
    len += sprintf(tbuf+len,"\n%d CAEN A3818 board(s) found.\n\n", i);
	if(copy_to_user(buf,tbuf,len)) {
		kfree(tbuf);
		return -EFAULT;
	}
	*pos = len;
	kfree(tbuf);
	return len;
#else
static int a3818_procinfo(char *buf, char **start, off_t fpos, int lenght, int *eof, void *data) {
    char *p;
    struct a3818_state* s = devs;
    int i = 0,j=0,k;

    p = buf;
    p += sprintf(p,"\nCAEN A3818 driver %s\n\n", DRIVER_VERSION);
    while( s ) {
        k = 0;
        p += sprintf(p, "  CAEN A3818 PCI Expresss CONET2 Board found.\n");
        p += sprintf(p, "  Card  number                  = %d\n", (int)s->CardNumber);
        p += sprintf(p, "  Firmware Release              = %x\n", (int)(s->FwRel));
        for (j=0;j<s->NumOfLink;j++) {
        p += sprintf(p, "  Physical address Link %d       = %p\n",j, (void *)(s->phys)[j]);
        p += sprintf(p, "  Virtual  address Link %d       = %p\n",j, s->baseaddr[j]);
        }
        p += sprintf(p, "  Physical address Common Space = %p\n", (void *)(s->phys)[5]);
        p += sprintf(p, "  Virtual  address Common Space = %p\n", s->baseaddr[5]);
        p += sprintf(p, "  IRQ line                      = %d\n", (int)s->irq);
        p += sprintf(p, "  Opened Links                  = %d\n", k);
        for (j=0;j<s->NumOfLink;j++) {
        p += sprintf(p, "  Ioctls on Link %d              = %i\n", j, s->ioctls[j]);
        }
        p += sprintf(p,"\n");
        s = s->next;
        i++;
    }
    p += sprintf(p,"%d CAEN A3818 board(s) found.\n\n", i);
    *eof = 1;
    return p - buf;
#endif	
}
/*
  ----------------------------------------------------------------------
  
  a3818_register_proc
  
  ----------------------------------------------------------------------
*/
static void a3818_register_proc(void) {
#if LINUX_VERSION_CODE >= VERSION(3,10,0)
	a3818_procdir = proc_create("a3818", 0, NULL, &a3818_procdir_fops);
#else
	a3818_procdir = create_proc_entry("a3818", S_IFREG | S_IRUGO, 0);
	a3818_procdir->read_proc = a3818_procinfo;
#endif
}

/*
  ----------------------------------------------------------------------
  
  a3818_unregister_proc
  
  ----------------------------------------------------------------------
*/
static void a3818_unregister_proc(void) {
	remove_proc_entry("a3818", 0);
}

/*
  ----------------------------------------------------------------------
  
  a3818_open
  
  minor meaning:
  
  
  bit 0-2 => Slave VME
  bit 3-5 => Optical link number
  bit 6-7 => PCIe card index
  
  ----------------------------------------------------------------------
*/
static int a3818_open(struct inode *inode, struct file *file) {

	unsigned int minor = MINOR(inode->i_rdev);
	struct a3818_state *s = devs;
	int slave, opt_link, card, ret = A3818_OK;

	slave = minor & 0x7;
	opt_link = (minor >> 3) & 0x7;
	card = (minor >> 6) & 0x3;

	DPRINTK("open: minor = %d\n",minor);
	DPRINTK("open: MAX_MINOR  = %d\n",MAX_MINOR );
	DPRINTK("SLAVE %d\n",slave);
	DPRINTK("LINK %d\n",opt_link);
	DPRINTK("CARD %d\n",card);

	/* If minor is out of range, return an error */
	if( minor > MAX_MINOR ) {
	  return -ENODEV;
	}
	/* Search for the device linked to the minor */
	while( s && s->CardNumber != (card) )
		 s = s->next;

	if( !s ) {
	  return -ENODEV;
	}
	if( (s->NumOfLink != 0) && (opt_link >= s->NumOfLink) ) { // trying to open an non-existent optical link
		return -ENODEV;
	}
		
	if( s->TypeOfBoard == A3818BOARD ) {
		if (!s->GTPReset) {
			spin_lock( &s->CardLock);
			if (a3818_reset_onopen(s) == A3818_OK) s->GTPReset = 1;
			spin_unlock( &s->CardLock);
		}
	}
	
	if ( s->TypeOfBoard != A3818RAW) {
		if (s->Occurency[opt_link] == 0) {
			s->ndata_app_dma_in[opt_link][slave] = 0;
			s->pos_app_dma_in[opt_link][slave] = 0;
			s->rx_ready[opt_link][slave] = 0;
			if( a3818_reset_comm(s, opt_link) == A3818_ERR_NOT_READY ) return A3818_ERR_NOT_READY;
		}
	}
	
	s->Occurency[opt_link]++;
	file->private_data = s;
	DPRINTK("Open success!\n");
	try_module_get(THIS_MODULE);
	return ret;
}

/*
        ----------------------------------------------------------------------

        a3818_release

        ----------------------------------------------------------------------
*/
static int a3818_release(struct inode *inode, struct file *file) {
  unsigned int minor = MINOR(inode->i_rdev);
  struct a3818_state *s = devs;
  int card = (minor >> 6) & 0x3;
  int opt_link = (minor >> 3) & 0x7;

  /* If minor is out of range, return an error */
  if( minor > MAX_MINOR ) {
      return -ENODEV;
  }

  /* Search for the device linked to the minor */
  while( s && s->CardNumber != card )
        s = s->next;

  if( !s )
    return -ENODEV;
  spin_lock( &s->DataLinklock[opt_link]);
  if (s->Occurency[opt_link]) {
	s->Occurency[opt_link]--;
	spin_unlock( &s->DataLinklock[opt_link]);
	module_put(THIS_MODULE);
  }
  else {
	spin_unlock( &s->DataLinklock[opt_link]);
  }
  return 0;
}

/*
        ----------------------------------------------------------------------

        a3818_ioctl

        ----------------------------------------------------------------------
*/
static int a3818_ioctl(struct inode *inode,struct file *file,unsigned int cmd, unsigned long arg)
{
  unsigned int minor = MINOR(inode->i_rdev);
  struct a3818_state *s = (struct a3818_state *)file->private_data;
  int ret = 0, slave, opt_link;
  int i; // for debug

  a3818_reg_t reg;
  a3818_comm_t comm;
  a3818_intr_t intr;

  minor = MINOR(inode->i_rdev);

  slave = minor & 0x7;
  opt_link = (minor >> 3) & 0x7;

  s->ioctls[opt_link]++;

  switch (cmd) {
		case A3818_IOCTL_PCIE_BRIDGE_RESET:
			a3818_PCIE_reset(s);
			break;
		case IOCTL_RESET:
			a3818_reset_comm(s, opt_link);
			break;
		case IOCTL_COMM:
			if ((s->NumOfLink == 0) && (s->TypeOfBoard == A3818RAW || s->TypeOfBoard == A3818DIGIT ))
				return -EFAULT; // To avoid to use optical link of a not initialized board

			if( copy_from_user(&comm, (a3818_comm_t *)arg, sizeof(comm)) > 0 ) {
				ret = -EFAULT;
				break;
			}
			down(&s->ioctl_lock[opt_link]);
			if( copy_from_user(&(s->buff_dma_out[opt_link][slave][1]), comm.out_buf, comm.out_count) > 0) {
				ret = -EFAULT;
				up(&s->ioctl_lock[opt_link]);
				break;
			}

			for( i = 0; i < comm.out_count; i += 4 ) DPRINTK("Dump pkt: %08lX\n", *((long *)(&comm.out_buf[i])));

			if( readl(s->baseaddr[opt_link] + A3818_LINK_SR) & A3818_LINK_FAIL ) {
				DPRINTK("Link fail detected! Trying to reset ...\n");
				a3818_reset_comm(s, opt_link);
				mdelay(10);  // Wait 10 ms
				if( readl(s->baseaddr[opt_link] + A3818_LINK_SR) & A3818_LINK_FAIL ) {
					ret = -EFAULT;
					// Before there was a break and it was a bad error handling
					goto err_comm;
				}
			}
			DPRINTK("Accessing Card %d...\n",s->CardNumber);
			if( comm.out_count > 0 ) {
				/* Build the header */
				s->buff_dma_out[opt_link][slave][0] = (slave << 24) | (slave << 16) | ((comm.out_count >> 1) & 0xFFFF);
				ret = a3818_send_pkt(s, slave, opt_link, comm.out_buf, comm.out_count);
				if( ret < 0 ) {
					ret = -EFAULT;
					goto err_comm;
				}
				if( comm.in_count > 0 ) {
				ret = a3818_recv_pkt(s, slave, opt_link, comm.status);
				if( ret < 0 ) {
					for(i=1;i<6;i++) {     // try to repeat 5 times (thanks to Evgueny Vlassov from CMS CERN)
					  a3818_reset_comm(s, opt_link);
					  ret = a3818_send_pkt(s, slave, opt_link, comm.out_buf, comm.out_count);
					  if( ret < 0 ) {
							  ret = -EFAULT;
							  goto err_comm;
					  }
					  ret = a3818_recv_pkt(s, slave, opt_link, comm.status);
					  if( ret >=0) break;
					}
					if(ret < 0) {
							  ret = -EFAULT;
							  goto err_comm;
					} else {
							  printk(KERN_ERR PFX "rcv-pkt: Link %d recovered in %d attempt(s)\n",opt_link,i);
					}
				}
			}
			}
			put_user(s->rx_status[opt_link][slave], comm.status);
			if( copy_to_user(comm.in_buf, s->app_dma_in[opt_link][slave], s->ndata_app_dma_in[opt_link][slave]) > 0) {
				ret = -EFAULT;
				goto err_comm;
			}
			
err_comm:
			s->pos_app_dma_in[opt_link][slave] = 0;
			s->rx_ready[opt_link][slave] = 0;
			s->ndata_app_dma_in[opt_link][slave] = 0;
			up(&s->ioctl_lock[opt_link]);
			break;
		case IOCTL_REG_WR:
			if( copy_from_user(&reg, (a3818_reg_t *)arg, sizeof(reg)) > 0 ) {
				ret = -EFAULT;
				break;
			}
			if ((!(reg.address & 0x1000)) && (s->NumOfLink == 0) && (s->TypeOfBoard == A3818RAW || s->TypeOfBoard == A3818DIGIT ))
				return -EFAULT; // To avoid to use optical link of a not initialized board

			// check if it is an access to the common space (address > 0x999)
			if( reg.address & 0x1000 ) {
				reg.address = reg.address & 0xfff;
				// write to common space
				down(&s->ioctl_lock[opt_link]);
				writel(reg.value, s->baseaddr[A3818_COMMON_REG] + reg.address);
			}
			else {
				// write the link space
				down(&s->ioctl_lock[opt_link]);
				writel(reg.value, s->baseaddr[opt_link] + reg.address);
			}
			up(&s->ioctl_lock[opt_link]);
			a3818_mmiowb();
			break;
		case IOCTL_REG_RD:
			if( copy_from_user(&reg, (a3818_reg_t *)arg, sizeof(reg)) > 0 ) {
				ret = -EFAULT;
				break;
			}
			if ((!(reg.address & 0x1000)) && (s->NumOfLink == 0) && (s->TypeOfBoard == A3818RAW || s->TypeOfBoard == A3818DIGIT ))
				return -EFAULT; // To avoid to use optical link of a not initialized board
			// check if it is an access to the common space (address > 0x999)
			if( reg.address & 0x1000 ) {
				reg.address = reg.address & 0xfff;
				// read to common space
				down(&s->ioctl_lock[opt_link]);
				reg.value = readl(s->baseaddr[A3818_COMMON_REG] + reg.address);
			}
			else {
				down(&s->ioctl_lock[opt_link]);
				reg.value = readl(s->baseaddr[opt_link] + reg.address);
				// read the link space
			}
			up(&s->ioctl_lock[opt_link]);
			if( copy_to_user((a3818_reg_t *)arg, &reg, sizeof(reg)) > 0) {
				ret = -EFAULT;
				break;
			}
			break;
		case IOCTL_COMM_REG_WR:
			if( copy_from_user(&reg, (a3818_reg_t *)arg, sizeof(reg)) > 0 ) {
				ret = -EFAULT;
				break;
			}
			down(&s->ioctl_lock[opt_link]);
			writel(reg.value, s->baseaddr[A3818_COMMON_REG] + reg.address);
			up(&s->ioctl_lock[opt_link]);
			a3818_mmiowb();
			break;
		case IOCTL_COMM_REG_RD:
			if( copy_from_user(&reg, (a3818_reg_t *)arg, sizeof(reg)) > 0 ) {
				ret = -EFAULT;
				break;
			}
			down(&s->ioctl_lock[opt_link]);
			reg.value = readl(s->baseaddr[A3818_COMMON_REG] + reg.address);
			up(&s->ioctl_lock[opt_link]);
			if( copy_to_user((a3818_reg_t *)arg, &reg, sizeof(reg)) > 0) {
				ret = -EFAULT;
				break;
			}
			break;
		case IOCTL_IRQ_WAIT:
			if( copy_from_user(&intr, (a3818_intr_t *)arg, sizeof(intr)) > 0 ) {
				ret = -EFAULT;
				break;
			}
			if( slave < 4 ) {
#if USE_MIDAS
				ret = wait_event_timeout(
#else
				ret = wait_event_interruptible_timeout(
#endif			
						s->intr_wait[opt_link][slave],            // wait queue
						((readl(s->baseaddr[opt_link] + A3818_IRQSTAT_0) >> (slave * 8)) & 0xFF) & intr.Mask,     // condition
						(intr.Timeout * HZ) / 1000     // timeout from ms to jiffies
						);
			} else {
#if USE_MIDAS
				ret = wait_event_timeout(
#else
				ret = wait_event_interruptible_timeout(
#endif			
						s->intr_wait[opt_link][slave],            // wait queue
						((readl(s->baseaddr[opt_link] + A3818_IRQSTAT_1) >> (slave * 8)) & 0xFF) & intr.Mask,     // condition
						(intr.Timeout * HZ) / 1000     // timeout from ms to jiffies
						);
			}
			if( ret == 0 ) {
				/* Timeout reached */
				/* !!!!! WARNING !!!!!!
				 * The CAENVMElib needs to handle the ioctl return value to detect
				 * IRQ timeout, so we must return a positive value to avoid Linux to
				 * wrap the ioctl's return code to -1
				 */
				ret = ETIMEDOUT;
			} else if( ret < 0 ) {
				/* Interrupted by a signal */
				ret = -EINTR;
			} else if( ret > 0 )
				ret = 0;
			break;
		case IOCTL_SEND:
			if ((s->NumOfLink == 0) && (s->TypeOfBoard == A3818RAW || s->TypeOfBoard == A3818DIGIT ))
				return -EFAULT; // To avoid to use optical link of a not initialized board

			if( copy_from_user(&comm, (a3818_comm_t *)arg, sizeof(comm)) > 0 ) {
				ret = -EFAULT;
				break;
			}
			down(&s->ioctl_lock[opt_link]);
			if( copy_from_user(&(s->buff_dma_out[opt_link][slave][1]), comm.out_buf, comm.out_count) > 0) {
				ret = -EFAULT;
				up(&s->ioctl_lock[opt_link]);
				break;
			}
			for( i = 0; i < comm.out_count; i += 4 ) DPRINTK("Dump pkt: %08lX\n", *((long *)(&comm.out_buf[i])));
			// Link fail handling.
			if( readl(s->baseaddr[opt_link] + A3818_LINK_SR) & A3818_LINK_FAIL ) {
				DPRINTK("Link fail detected! Trying to reset ...\n");
				a3818_reset_comm(s, opt_link);
				mdelay(10);  // Wait 10 ms
				if( readl(s->baseaddr[opt_link] + A3818_LINK_SR) & A3818_LINK_FAIL ) {
					ret = -EACCES;					
					goto err_send;
				}
			}
			if( comm.out_count > 0 ) {
				/* Build the header */
				s->buff_dma_out[opt_link][slave][0] = (slave << 24) | (slave << 16) | ((comm.out_count >> 1) & 0xFFFF);
				ret = a3818_send_pkt(s, slave, opt_link, comm.out_buf, comm.out_count);
				if( ret < 0 ) {
					ret = -EFAULT;					
					goto err_send;
				}
			}
			if( copy_to_user(comm.in_buf, s->app_dma_in[opt_link][slave], comm.in_count) ) {
				ret = -EFAULT;
				up(&s->ioctl_lock[opt_link]);
				break;
			}
			up(&s->ioctl_lock[opt_link]);
err_send:
			break;
		case IOCTL_RECV:
			if ((s->NumOfLink == 0) && (s->TypeOfBoard == A3818RAW || s->TypeOfBoard == A3818DIGIT ))
				return -EFAULT; // To avoid to use optical link of a not initialized board
			ret = a3818_recv_pkt(s, slave, opt_link, (int *)arg);
			up(&s->ioctl_lock[opt_link]);
			if( ret < 0 ) {
				ret = -EFAULT;
			}
			break;

		case IOCTL_REV:
				ret = ENOSYS;
				break;
		case PCI_IOCTL_REG_RD:
			if( copy_from_user(&reg, (a3818_reg_t *)arg, sizeof(reg)) > 0 ) {
				ret = -EFAULT;
				break;
			}
			pci_read_config_dword(s->pcidev, reg.address, &(reg.value));
			DPRINTK("address %x value %x\n", reg.address, reg.value);
			if( copy_to_user((a3818_reg_t *)arg, &reg, sizeof(reg)) > 0) {
				ret = -EFAULT;
				break;
			}
			break;
		default:
			ret = ENOSYS;
			break;
  	  }
  return ret;
}

/*
        ----------------------------------------------------------------------

        a3818_ioctl_unlocked (Called in preference to a3818_ioctl on newer kernels)

        ----------------------------------------------------------------------
*/
#if LINUX_VERSION_CODE >= VERSION(2,6,11)
static long a3818_ioctl_unlocked(struct file *file, unsigned int cmd, unsigned long arg)
{
#if LINUX_VERSION_CODE >= VERSION(3,19,0)
  struct inode *inode = file->f_path.dentry->d_inode;
#else	
  struct inode *inode = file->f_dentry->d_inode;
#endif
  struct a3818_state *s = (struct a3818_state *)file->private_data;
  long ret;
  unsigned int minor;
  
	minor = MINOR(inode->i_rdev);
	s = (struct a3818_state *)file->private_data;
	
	if( s->TypeOfBoard != A3818BOARD ) {
		ret = (long) a3818_ioctl(inode, file, cmd, arg);
		return ret;
	}
	ret = (long) a3818_ioctl(inode, file, cmd, arg);
	return ret;
}
#endif

/*		----------------------------------------------------------------------

        a3818_interrupt

        ----------------------------------------------------------------------
*/

// Rev 1.5

#if  LINUX_VERSION_CODE < VERSION(2,6,23)
static irqreturn_t a3818_interrupt(int irq, void *dev_id, struct pt_regs *regs)
#else
static irqreturn_t a3818_interrupt(int irq, void *dev_id)
#endif
{
	struct a3818_state *s = (struct a3818_state *)dev_id;
	uint32_t app;
	int i;
	u32 irq0, irq1;
	uint32_t tr_stat;
	
	DPRINTK("\n### START INTERRUPT ###\n");
	while (s != NULL) {
		spin_lock( &s->CardLock);
		app = readl(s->baseaddr[A3818_COMMON_REG] + A3818_DMISCCS);
		if (app == 0xffffffff) {
			DPRINTK("esco dall IRQ\n"); 
			spin_unlock( &s->CardLock);
			s = s->next;
			continue; 
		}
		DPRINTK("A3818_DMISCCS %x\n", app);
		if( !(app & ( A3818_DMISCCS_RDDMA_DONE0 | A3818_DMISCCS_RDDMA_DONE1 |
			  A3818_DMISCCS_RDDMA_DONE2 | A3818_DMISCCS_RDDMA_DONE3 |
			  A3818_DMISCCS_LOC_INT0 | A3818_DMISCCS_LOC_INT1 |
			  A3818_DMISCCS_LOC_INT2 | A3818_DMISCCS_LOC_INT3 )) ) {

			spin_unlock( &s->CardLock);
			DPRINTK("### NOT MINE EXIT FROM INTERRUPT ###\n");
			s = s->next;
			continue; 
		}
		for( i = s->NumOfLink; i > -1 ; i-- ) {
			if( app & (A3818_DMISCCS_RDDMA_DONE0 << i) ) {
				//DMA controller reset
				writel(A3818_RES_DMAINT, s->baseaddr[i] + A3818_DMACSR_S);
				s->DMAInProgress[i] = 0;
				DPRINTK("ENABLE_RX_INT\n");
				spin_lock( &s->DataLinklock[i]);
				a3818_dispatch_pkt(s, i);
				spin_unlock( &s->DataLinklock[i]);
				ENABLE_RX_INT(i);
				DPRINTK("...DMA done...\n");
			}
			if( app & (A3818_DMISCCS_LOC_INT0 << i) ) {
				DISABLE_RX_INT(i);
				// Mask (Disable) VME IRQ received 
				irq0 = readl(s->baseaddr[i] + A3818_IRQSTAT_0);
				if (irq0 != 0) writel(irq0, s->baseaddr[i] + A3818_IRQMASK_0_C);
				irq1 = readl(s->baseaddr[i] + A3818_IRQSTAT_1);
				if (irq1 != 0) writel(irq1, s->baseaddr[i] + A3818_IRQMASK_1_C);
				if( irq0 | irq1 ) {
					writel(A3818_VINTDIS,s->baseaddr[i] + A3818_IOCTL_S);
					a3818_handle_vme_irq(irq0, irq1, s, i);
				}
				if( !(s->DMAInProgress[i]) ) {
					/*
					Check if the interrupt is due to the receive of a packet or
					if during the previous DMA transfer a packet is arrived
					*/
					tr_stat = readl(s->baseaddr[i] + A3818_LINK_TRS);
					//DPRINTK("trstat = %#lx\n", (long)tr_stat);
					if( tr_stat & 0xFFFF0000 ) {
						s->tr_stat[i] = tr_stat;
						a3818_handle_rx_pkt(s, i, 0);
					}
				}
			}
		}
	spin_unlock( &s->CardLock);
	s = s->next;
	}
	return IRQ_HANDLED;
}

/*
  ----------------------------------------------------------------------
  
  a3818_init_board
  
  ----------------------------------------------------------------------
*/
static int a3818_init_board(struct pci_dev *pcidev, int index) {
	struct a3818_state *s;
	int i, j, z=0, nol,tob;
	int ret = 0;
	a3818_reg_t reg;
	short value;
	DPRINTK("sono in init board\n");

	if( pci_enable_device(pcidev) ) {
		printk("init_board: pci_enable_device failed");
		return -1;
	}

////////////////////////////////////
// rileggo il registro del PCI Configuration Status 
            reg.address = PCIE_CONF_STAT;
	        pci_read_config_word(pcidev, reg.address, &value);
	        DPRINTK("\nConfiguration Status Reg = %x\n", value);
            // rileggo il registro del PCI Device Status 
            reg.address = PCIE_DEV_STAT;
	        pci_read_config_word(pcidev, reg.address, &value);
	        DPRINTK("\nDevice Status Reg = %x\n", value);

////////////////////////////////////
	
	
	reg.address = PCIE_MSI_CAPAB_REG;
	pci_read_config_dword(pcidev, reg.address, &(reg.value));
	DPRINTK("\nPCIE_MSI_CAPAB_REG = %x\n", reg.value);
#if USE_MSI_IRQ	
	pci_enable_msi(pcidev); // uncomment to enable MSI
#endif
	pci_set_master(pcidev); 		// Enable bus mastering DMA on PCI Board

	if( pcidev->irq == 0 ) {
		printk("Invalid IRQ\n");
		return -1;
	}
	
#if LINUX_VERSION_CODE >= VERSION(2,6,26)
	// Xilinx bug. Force to disable ASPM 
	pci_disable_link_state(pcidev, PCIE_LINK_STATE_L0S | PCIE_LINK_STATE_L1 |PCIE_LINK_STATE_CLKPM);
#endif	
	
	s = kmalloc(sizeof(*s), GFP_KERNEL);
	if( !s ) {
		printk(KERN_ERR PFX "out of memory\n");
		return -2;
	}

	memset(s, 0, sizeof(struct a3818_state));

	s->pcidev = pcidev;

	// Enable BAR 5
	s->phys[A3818_COMMON_REG] = pci_resource_start(pcidev, A3818_COMMON_REG);
	DPRINTK("phys[%d]=%x\n",A3818_COMMON_REG,s->phys[A3818_COMMON_REG]);
	if( s->phys[A3818_COMMON_REG] == 0 )
	   goto err_kmalloc;

	if( !request_mem_region( s->phys[A3818_COMMON_REG], A3818_REGION_SIZE, "a3818") ) {
		DPRINTK(KERN_ERR PFX "io mem %#lx-%#lx in use\n",
		s->phys[A3818_COMMON_REG], s->phys[A3818_COMMON_REG] + A3818_REGION_SIZE - 1);
		goto err_common_region_mem;
	}

	s->baseaddr[A3818_COMMON_REG] = (char *)ioremap(s->phys[A3818_COMMON_REG], A3818_REGION_SIZE);
	printk("Found A3818 - Common BAR at iomem %p irq %u\n", s->baseaddr[A3818_COMMON_REG], s->irq);

	s->FwRel = readl(s->baseaddr[A3818_COMMON_REG] + A3818_FWREV);
	
	// Retrieve Board type: (A3818 or Digitizer PCIe)
	if( (tob = readl(s->baseaddr[A3818_COMMON_REG] + A3818_BOARD_ID)) == A3818BOARD ) {
	  printk("Found A3818 ");
	  s->TypeOfBoard = A3818BOARD;
	  if( (nol = readl(s->baseaddr[A3818_COMMON_REG] + A3818_BOARD_VERS_ID)) == ONE_LINK )
		  s->NumOfLink = 1;
	  else if( nol == TWO_LINK )
		  s->NumOfLink = 2;
	  else
		  s->NumOfLink = 4;

	  printk("with %d link(s)\n", s->NumOfLink );
	  for (i=0;i<s->NumOfLink;i++) {
		s->DMADone[i] = 0;
		s->LocalInterrupt[i] = 0;
	  }
	}
	else if( tob == A3818DIGIT ) {
	  /* To implement! */
		printk("Found Digitizer PCIe\n");
	  s->TypeOfBoard = A3818DIGIT;
	  s->irq = pcidev->irq;
#if LINUX_VERSION_CODE < VERSION(2,6,23)
	if( request_irq(s->irq, a3818_interrupt, SA_SHIRQ, "a3818", s) )
#else
	if( request_irq(s->irq, a3818_interrupt, IRQF_SHARED , "a3818", s) )
#endif
{
		  printk(KERN_ERR PFX "irq %u in use\n", s->irq);
		  goto err_common_region_map;
	  }

	  s->CardNumber = index;
	  s->timeout= msecs_to_jiffies( 15000);
	  s->NumOfLink = 0;
	  s->TypeOfBoard = A3818DIGIT;
	  /* queue it for later freeing */
	  s->next = devs;
	  devs = s;
	  return ret;
	}
	else { // empty board => to be programmed
	  s->irq = pcidev->irq;
#if LINUX_VERSION_CODE < VERSION(2,6,23)
	if( request_irq(s->irq, a3818_interrupt, SA_SHIRQ, "a3818", s) )
#else
	if( request_irq(s->irq, a3818_interrupt, IRQF_SHARED , "a3818", s) )
#endif
{
		  printk(KERN_ERR PFX "irq %u in use\n", s->irq);
		  goto err_common_region_map;
	  }
	  s->NumOfLink = 0;
	  s->TypeOfBoard = A3818RAW;
	  s->CardNumber = index;
	  s->timeout= msecs_to_jiffies( 15000);
	  /* queue it for later freeing */
	  s->next = devs;
	  devs = s;
	  spin_lock_init( &s->DataLinklock[0]);
	  spin_lock_init( &s->CardLock);
	  printk("Raw PCIe Board\n");
	  return ret;
	}
	
	spin_lock_init( &s->CardLock);
	for( i = 0; i < s->NumOfLink; i++ ) {
	  spin_lock_init( &s->DataLinklock[i]);
	  s->ioctls[i] = 0;
	  s->Occurency[i] = 0;
	  sema_init(&s->ioctl_lock[i],1);
	  s->phys[i] = pci_resource_start(pcidev, i);
	  DPRINTK("phys[%d]=%lx\n",i,s->phys[i]);
	  if( s->phys[i] == 0 )
		  goto err_common_region_map;
	}

	for( z = 0; z < s->NumOfLink; z++ ) {
	  if( !request_mem_region( s->phys[z], A3818_REGION_SIZE, "a3818") ) {
		  printk(KERN_ERR PFX "io mem %#lx-%#lx in use\n", s->phys[z], s->phys[z] + A3818_REGION_SIZE - 1);
		  goto err_region_mem;
	  }
	}

	for( i = 0; i < s->NumOfLink; i++ ) {
	  s->baseaddr[i] = (char *)ioremap(s->phys[i], A3818_REGION_SIZE);
	  printk("found A3818 Link %d BAR at iomem %p irq %u\n",i, s->baseaddr[i], s->irq);
	}

	for( i = 0; i < s->NumOfLink; i++ )
	  s->DMAInProgress[i] = 0;

	/* Alloc maximum size for DMA (in) */
	for( i = 0; i < s->NumOfLink; i++ ) {
	  s->buff_dma_in[i] = dma_alloc_coherent(&pcidev->dev, A3818_MAX_PKT_SZ, &s->phy_dma_addr_in[i], GFP_KERNEL);
	  if( !(s->buff_dma_in[i]) ) {
		  printk(KERN_ERR PFX "out of DMA memory s->buff_dma_in[%d]\n",i);
		  goto err_ibuf;
	  }

	  DPRINTK(KERN_ERR PFX "s->buff_dma_in[%d]: %x\n",i, s->buff_dma_in[i]);
	  DPRINTK(KERN_ERR PFX "s->phy_dma_addr_in[%d]: %x\n",i,  s->phy_dma_addr_in[i]);
	}

	for( j = 0; j < s->NumOfLink; j++ )
	  for( i = 0; i < MAX_V2718; i++ ) {
		  init_waitqueue_head(&s->read_wait[j][i]);
		  init_waitqueue_head(&s->intr_wait[j][i]);
		  s->buff_dma_out[j][i] = dma_alloc_coherent(&pcidev->dev, A3818_MAX_PKT_SZ,
												 &s->phy_dma_addr_out[j][i], GFP_KERNEL);
		  if( !(s->buff_dma_out[j][i]) ) {
			  DPRINTK(KERN_ERR PFX "out of DMA memory s->buff_dma_out i: %d\n", i);
			  goto err_obuf;
		  }
	  }

	/* request irq */

	s->irq = pcidev->irq;

#if LINUX_VERSION_CODE < VERSION(2,6,23)
	if( request_irq(s->irq, a3818_interrupt, SA_SHIRQ, "a3818", s) )
#else
	if( request_irq(s->irq, a3818_interrupt, IRQF_SHARED , "a3818", s) )
#endif
{
		printk("irq %u in use\n", s->irq);
		goto err_irq;
	}
	s->GTPReset = 0;
	if( a3818_reset(s) != A3818_OK ) {
	  s->TypeOfBoard = A3818RAW;
	  goto err_irq;
	}

	a3818_dma_conf(s);

	printk("  CAEN A3818 Loaded.\n");

	for( i = 0; i < s->NumOfLink; i++ ) {
		s->ndata_dma_in[i] = 0;
	    writel(A3818_VINTDIS,s->baseaddr[i] + A3818_IOCTL_C);
	}
	s->CardNumber = index;
	s->timeout= msecs_to_jiffies( 100);

	for( i = 0; i < s->NumOfLink; i++ )
	  for( j = 0; j  < MAX_V2718; j++ ) {
		  s->app_dma_in[i][j] = (u8 *)vmalloc(1024*1024);
		  if( s->app_dma_in[i][j]== 0) {
			  printk("unable to alloc vmalloc buffers\n");
			  goto err_app_dma_in;
		  }
	  }
	
	/* queue it for later freeing */
	s->next = devs;
	devs = s;
	return ret;

err_app_dma_in:
	if( !ret ) ret = -9;
	for( i = 0; i < s->NumOfLink; i++ )
	  for( j  = 0; j < MAX_V2718; j++)
		  if (s->app_dma_in[i][j]) vfree( s->app_dma_in[i][j]);

err_irq:
err_obuf:
	if( !ret ) ret = -8;
	for( j = 0; j < s->NumOfLink; j++ )
	  for( i = 0; i < MAX_V2718; i++ )
		  if( (s->buff_dma_out[j][i]) )
			  dma_free_coherent(&pcidev->dev, A3818_MAX_PKT_SZ, s->buff_dma_out[j][i], s->phy_dma_addr_out[j][i]);

err_ibuf:
  if( !ret ) ret = -7;
	for( i = 0; i < s->NumOfLink; i++ ) {
	  if( s->buff_dma_in[i] )
		  dma_free_coherent(&pcidev->dev, A3818_MAX_PKT_SZ, s->buff_dma_in[i], s->phy_dma_addr_in[i]);
	  iounmap(s->baseaddr[i]);
	}

err_region_mem:
	if( !ret ) ret = -6;
	for( j = z - 1; j >= 0; j-- )
	  release_mem_region(s->phys[j], A3818_REGION_SIZE);

err_common_region_map:
	if( s->TypeOfBoard == A3818RAW ) {  // a3818 sputtanata ma si sitorna OK x consentire l'accesso al registro comune e ripristinarla
		s->NumOfLink = 0;
		s->CardNumber = index;
		s->timeout= msecs_to_jiffies( 15000);
		s->next = devs;
		devs = s;
		printk("A3818 CORRUPTED!!!  \n");
		return 0;
	}
	a3818_unregister_proc();
	if( !ret ) ret = -5;
	iounmap(s->baseaddr[A3818_COMMON_REG]);

err_common_region_mem:
	if( !ret ) ret = -4;

	release_mem_region(s->phys[A3818_COMMON_REG], A3818_REGION_SIZE);

err_kmalloc:
	if( !ret ) ret = -3;
	kfree(s);

  return ret;
}

/*
        ----------------------------------------------------------------------

        a3818_init

        ----------------------------------------------------------------------
*/
static int __init a3818_init(void) {
        struct pci_dev *pcidev = NULL;
		struct a3818_state *s;
        int index = 0,j,i;
		char DevEntryName[15];

        printk("CAEN A3818 PCI Express CONET2 controller driver %s\n", DRIVER_VERSION);
        printk("  Copyright 2013, CAEN SpA\n");

        while( index < MAX_MINOR && (
	    	 (pcidev = pci_get_subsys(PCI_VENDOR_XILINX_ID,
									  PCI_DEVICE_ID_CAEN,
									  PCI_SUBSYSTEM_VENDOR_ID_XILINX,
									  PCI_SUBSYSTEM_ID_VIRTEX,
									  pcidev))) ) {
			if( a3818_init_board(pcidev, index) ) {
			  ReleaseBoards();
			  return -ENODEV;
			}
			index++;
        }
  if( !index)
			// No PCI device found
      return -ENODEV;

/*
	Register device only if HW is present otherwise the kernel
	crashes with a cat /proc/devices
*/
  printk("  CAEN PCIe: %d device(s) found.\n", index);
  if( index > 0 ) {
      /* register device only if HW is present*/
      a3818_register_proc();
      a3818_major = register_chrdev(0, "a3818", &a3818_fops);
      if( a3818_major < 0 ) {
          printk("  Error getting Major Number.\n");
          return -ENODEV;
      }
	  a3818_class = class_create(THIS_MODULE, "a3818");
	  s = devs;
	  while(s) {
		for (i=0;i< s->NumOfLink;i++) {
			for (j=0;j<MAX_V2718;j++) {
				index = (s->CardNumber << 6) + (i << 3) + j;
				sprintf(DevEntryName,"a3818_%d",index);
#if LINUX_VERSION_CODE < VERSION(2,6,27)
				device_create(a3818_class, NULL, MKDEV(a3818_major, index), DevEntryName);
#else
				device_create(a3818_class, NULL, MKDEV(a3818_major, index), NULL, DevEntryName);
#endif
			}
		}
		if (s->NumOfLink == 0) {
			sprintf(DevEntryName,"a3818_%d",(s->CardNumber << 6));
#if LINUX_VERSION_CODE < VERSION(2,6,27)
			device_create(a3818_class, NULL, MKDEV(a3818_major, index), DevEntryName);
#else
			device_create(a3818_class, NULL, MKDEV(a3818_major, index), NULL, DevEntryName);
#endif
		}
		s->major = a3818_major;
		s = s->next;
	 }
  }
  return 0;
}

/*
        ----------------------------------------------------------------------

        ReleaseBoards

        ----------------------------------------------------------------------
*/
static void ReleaseBoards(void)
{
	struct a3818_state *s;
	int i, j,index;

	while( (s = devs) ) {
        devs = devs->next;
        DPRINTK("ReleaseBoards \n");
        free_irq(s->irq, s);
#if USE_MSI_IRQ
		pci_disable_msi(s->pcidev);
#endif
        for( i = 0; i < s->NumOfLink; i++ )
            for( j  = 0; j < MAX_V2718; j++)
                if( s->app_dma_in[i][j]) vfree( s->app_dma_in[i][j] );
        for( j = 0; j < s->NumOfLink; j++ )
            for( i = 0; i < MAX_V2718; i++ )
                if( (s->buff_dma_out[j][i]) )
                    dma_free_coherent(&s->pcidev->dev, A3818_MAX_PKT_SZ, s->buff_dma_out[j][i], s->phy_dma_addr_out[j][i]);
        for( i = 0; i < s->NumOfLink; i++ ) {
            if( s->buff_dma_in[i] )
                dma_free_coherent(&s->pcidev->dev, A3818_MAX_PKT_SZ, s->buff_dma_in[i], s->phy_dma_addr_in[i]);
            iounmap(s->baseaddr[i]);
        }
        for( i = 0; i < s->NumOfLink; i++ )
            release_mem_region(s->phys[i], A3818_REGION_SIZE);
        iounmap(s->baseaddr[A3818_COMMON_REG]);
        release_mem_region(s->phys[A3818_COMMON_REG], A3818_REGION_SIZE);
		
		for (i=0;i< s->NumOfLink;i++) {
			for (j=0;j<MAX_V2718;j++) {
				index = (s->CardNumber << 6) + (i << 3) + j;
				device_destroy(a3818_class, MKDEV(s->major, index));
			}
		}
		if (s->NumOfLink == 0) {
			device_destroy(a3818_class, MKDEV(s->major, (s->CardNumber << 6)));
		}
        kfree(s);
	}
	a3818_unregister_proc();
	class_destroy(a3818_class);
}

/*
        ----------------------------------------------------------------------

        a3818_cleanup

        ----------------------------------------------------------------------
*/
static void __exit a3818_cleanup(void) {
  ReleaseBoards();
  unregister_chrdev(a3818_major, "a3818");
  printk("CAEN A3818: unloading.\n");
}

module_init(a3818_init);
module_exit(a3818_cleanup);

MODULE_LICENSE("GPL");

MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );

#if LINUX_VERSION_CODE >= VERSION(2,6,26)
MODULE_DEVICE_TABLE( pci, a3818_pci_tbl );
#endif
