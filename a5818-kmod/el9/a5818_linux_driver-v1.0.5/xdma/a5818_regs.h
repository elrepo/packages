// -----------------------------------------------------------------------------
// 'a5818' Register Definitions
// Revision: 137
// -----------------------------------------------------------------------------
// Generated on 2023-09-12 at 12:30 (UTC) by airhdl version 2023.07.1-936312266
// -----------------------------------------------------------------------------
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// -----------------------------------------------------------------------------

#ifndef A5818_REGS_H
#define A5818_REGS_H

/* Revision number of the 'a5818' register map */
#define A5818_REVISION 137

/* Default base address of the 'a5818' register map */
#define A5818_DEFAULT_BASEADDR 0x00000000

/* Size of the 'a5818' register map, in bytes */
#define A5818_RANGE_BYTES 16408

/* Register 'L0_TXFIFO' */
#define L0_TXFIFO_OFFSET 0x00000000 /* address offset of the 'L0_TXFIFO' register */

/* Field  'L0_TXFIFO.value' */
#define L0_TXFIFO_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L0_TXFIFO_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L0_TXFIFO_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L0_TXFIFO_VALUE_RESET 0x10101010 /* reset value of the 'value' field */

/* Register 'L0_RXFIFO' */
#define L0_RXFIFO_OFFSET 0x00000004 /* address offset of the 'L0_RXFIFO' register */

/* Field  'L0_RXFIFO.value' */
#define L0_RXFIFO_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L0_RXFIFO_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L0_RXFIFO_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L0_RXFIFO_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L0_IOCTL' */
#define L0_IOCTL_OFFSET 0x00000008 /* address offset of the 'L0_IOCTL' register */

/* Field  'L0_IOCTL.value' */
#define L0_IOCTL_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L0_IOCTL_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L0_IOCTL_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L0_IOCTL_VALUE_RESET 0xB /* reset value of the 'value' field */

/* Register 'L0_LINK_SR' */
#define L0_LINK_SR_OFFSET 0x0000000C /* address offset of the 'L0_LINK_SR' register */

/* Field  'L0_LINK_SR.value' */
#define L0_LINK_SR_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L0_LINK_SR_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L0_LINK_SR_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L0_LINK_SR_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L0_TSR' */
#define L0_TSR_OFFSET 0x00000018 /* address offset of the 'L0_TSR' register */

/* Field  'L0_TSR.value' */
#define L0_TSR_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L0_TSR_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L0_TSR_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L0_TSR_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L0_DEBUG' */
#define L0_DEBUG_OFFSET 0x00000020 /* address offset of the 'L0_DEBUG' register */

/* Field  'L0_DEBUG.value' */
#define L0_DEBUG_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L0_DEBUG_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L0_DEBUG_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L0_DEBUG_VALUE_RESET 0xAA550000 /* reset value of the 'value' field */

/* Register 'L0_IRQSTAT_0' */
#define L0_IRQSTAT_0_OFFSET 0x00000024 /* address offset of the 'L0_IRQSTAT_0' register */

/* Field  'L0_IRQSTAT_0.value' */
#define L0_IRQSTAT_0_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L0_IRQSTAT_0_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L0_IRQSTAT_0_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L0_IRQSTAT_0_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L0_IRQSTAT_1' */
#define L0_IRQSTAT_1_OFFSET 0x00000028 /* address offset of the 'L0_IRQSTAT_1' register */

/* Field  'L0_IRQSTAT_1.value' */
#define L0_IRQSTAT_1_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L0_IRQSTAT_1_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L0_IRQSTAT_1_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L0_IRQSTAT_1_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L0_IRQ_OR7' */
#define L0_IRQ_OR7_OFFSET 0x0000002C /* address offset of the 'L0_IRQ_OR7' register */

/* Field  'L0_IRQ_OR7.value' */
#define L0_IRQ_OR7_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L0_IRQ_OR7_VALUE_BIT_WIDTH 8 /* bit width of the 'value' field */
#define L0_IRQ_OR7_VALUE_BIT_MASK 0x000000FF /* bit mask of the 'value' field */
#define L0_IRQ_OR7_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L0_IRQMASK_0' */
#define L0_IRQMASK_0_OFFSET 0x00000030 /* address offset of the 'L0_IRQMASK_0' register */

/* Field  'L0_IRQMASK_0.value' */
#define L0_IRQMASK_0_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L0_IRQMASK_0_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L0_IRQMASK_0_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L0_IRQMASK_0_VALUE_RESET 0x7F7F7F7F /* reset value of the 'value' field */

/* Register 'L0_IRQMASK_1' */
#define L0_IRQMASK_1_OFFSET 0x00000038 /* address offset of the 'L0_IRQMASK_1' register */

/* Field  'L0_IRQMASK_1.value' */
#define L0_IRQMASK_1_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L0_IRQMASK_1_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L0_IRQMASK_1_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L0_IRQMASK_1_VALUE_RESET 0x7F7F7F7F /* reset value of the 'value' field */

/* Register 'L0_PERF' */
#define L0_PERF_OFFSET 0x00000040 /* address offset of the 'L0_PERF' register */

/* Field  'L0_PERF.value' */
#define L0_PERF_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L0_PERF_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L0_PERF_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L0_PERF_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L0_I2C_COM' */
#define L0_I2C_COM_OFFSET 0x00000044 /* address offset of the 'L0_I2C_COM' register */

/* Field  'L0_I2C_COM.value' */
#define L0_I2C_COM_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L0_I2C_COM_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L0_I2C_COM_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L0_I2C_COM_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L0_I2C_DAT' */
#define L0_I2C_DAT_OFFSET 0x00000048 /* address offset of the 'L0_I2C_DAT' register */

/* Field  'L0_I2C_DAT.value' */
#define L0_I2C_DAT_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L0_I2C_DAT_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L0_I2C_DAT_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L0_I2C_DAT_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L1_TXFIFO' */
#define L1_TXFIFO_OFFSET 0x00001000 /* address offset of the 'L1_TXFIFO' register */

/* Field  'L1_TXFIFO.value' */
#define L1_TXFIFO_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L1_TXFIFO_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L1_TXFIFO_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L1_TXFIFO_VALUE_RESET 0x10101010 /* reset value of the 'value' field */

/* Register 'L1_RXFIFO' */
#define L1_RXFIFO_OFFSET 0x00001004 /* address offset of the 'L1_RXFIFO' register */

/* Field  'L1_RXFIFO.value' */
#define L1_RXFIFO_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L1_RXFIFO_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L1_RXFIFO_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L1_RXFIFO_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L1_IOCTL' */
#define L1_IOCTL_OFFSET 0x00001008 /* address offset of the 'L1_IOCTL' register */

/* Field  'L1_IOCTL.value' */
#define L1_IOCTL_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L1_IOCTL_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L1_IOCTL_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L1_IOCTL_VALUE_RESET 0xB /* reset value of the 'value' field */

/* Register 'L1_LINK_SR' */
#define L1_LINK_SR_OFFSET 0x0000100C /* address offset of the 'L1_LINK_SR' register */

/* Field  'L1_LINK_SR.value' */
#define L1_LINK_SR_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L1_LINK_SR_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L1_LINK_SR_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L1_LINK_SR_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L1_TSR' */
#define L1_TSR_OFFSET 0x00001018 /* address offset of the 'L1_TSR' register */

/* Field  'L1_TSR.value' */
#define L1_TSR_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L1_TSR_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L1_TSR_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L1_TSR_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L1_DEBUG' */
#define L1_DEBUG_OFFSET 0x00001020 /* address offset of the 'L1_DEBUG' register */

/* Field  'L1_DEBUG.value' */
#define L1_DEBUG_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L1_DEBUG_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L1_DEBUG_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L1_DEBUG_VALUE_RESET 0xAA550001 /* reset value of the 'value' field */

/* Register 'L1_IRQSTAT_0' */
#define L1_IRQSTAT_0_OFFSET 0x00001024 /* address offset of the 'L1_IRQSTAT_0' register */

/* Field  'L1_IRQSTAT_0.value' */
#define L1_IRQSTAT_0_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L1_IRQSTAT_0_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L1_IRQSTAT_0_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L1_IRQSTAT_0_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L1_IRQSTAT_1' */
#define L1_IRQSTAT_1_OFFSET 0x00001028 /* address offset of the 'L1_IRQSTAT_1' register */

/* Field  'L1_IRQSTAT_1.value' */
#define L1_IRQSTAT_1_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L1_IRQSTAT_1_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L1_IRQSTAT_1_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L1_IRQSTAT_1_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L1_IRQ_OR7' */
#define L1_IRQ_OR7_OFFSET 0x0000102C /* address offset of the 'L1_IRQ_OR7' register */

/* Field  'L1_IRQ_OR7.value' */
#define L1_IRQ_OR7_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L1_IRQ_OR7_VALUE_BIT_WIDTH 8 /* bit width of the 'value' field */
#define L1_IRQ_OR7_VALUE_BIT_MASK 0x000000FF /* bit mask of the 'value' field */
#define L1_IRQ_OR7_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L1_IRQMASK_0' */
#define L1_IRQMASK_0_OFFSET 0x00001030 /* address offset of the 'L1_IRQMASK_0' register */

/* Field  'L1_IRQMASK_0.value' */
#define L1_IRQMASK_0_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L1_IRQMASK_0_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L1_IRQMASK_0_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L1_IRQMASK_0_VALUE_RESET 0x7F7F7F7F /* reset value of the 'value' field */

/* Register 'L1_IRQMASK_1' */
#define L1_IRQMASK_1_OFFSET 0x00001038 /* address offset of the 'L1_IRQMASK_1' register */

/* Field  'L1_IRQMASK_1.value' */
#define L1_IRQMASK_1_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L1_IRQMASK_1_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L1_IRQMASK_1_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L1_IRQMASK_1_VALUE_RESET 0x7F7F7F7F /* reset value of the 'value' field */

/* Register 'L1_PERF' */
#define L1_PERF_OFFSET 0x00001040 /* address offset of the 'L1_PERF' register */

/* Field  'L1_PERF.value' */
#define L1_PERF_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L1_PERF_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L1_PERF_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L1_PERF_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L1_I2C_COM' */
#define L1_I2C_COM_OFFSET 0x00001044 /* address offset of the 'L1_I2C_COM' register */

/* Field  'L1_I2C_COM.value' */
#define L1_I2C_COM_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L1_I2C_COM_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L1_I2C_COM_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L1_I2C_COM_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L1_I2C_DAT' */
#define L1_I2C_DAT_OFFSET 0x00001048 /* address offset of the 'L1_I2C_DAT' register */

/* Field  'L1_I2C_DAT.value' */
#define L1_I2C_DAT_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L1_I2C_DAT_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L1_I2C_DAT_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L1_I2C_DAT_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L2_TXFIFO' */
#define L2_TXFIFO_OFFSET 0x00002000 /* address offset of the 'L2_TXFIFO' register */

/* Field  'L2_TXFIFO.value' */
#define L2_TXFIFO_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L2_TXFIFO_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L2_TXFIFO_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L2_TXFIFO_VALUE_RESET 0x10101010 /* reset value of the 'value' field */

/* Register 'L2_RXFIFO' */
#define L2_RXFIFO_OFFSET 0x00002004 /* address offset of the 'L2_RXFIFO' register */

/* Field  'L2_RXFIFO.value' */
#define L2_RXFIFO_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L2_RXFIFO_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L2_RXFIFO_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L2_RXFIFO_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L2_IOCTL' */
#define L2_IOCTL_OFFSET 0x00002008 /* address offset of the 'L2_IOCTL' register */

/* Field  'L2_IOCTL.value' */
#define L2_IOCTL_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L2_IOCTL_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L2_IOCTL_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L2_IOCTL_VALUE_RESET 0xB /* reset value of the 'value' field */

/* Register 'L2_LINK_SR' */
#define L2_LINK_SR_OFFSET 0x0000200C /* address offset of the 'L2_LINK_SR' register */

/* Field  'L2_LINK_SR.value' */
#define L2_LINK_SR_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L2_LINK_SR_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L2_LINK_SR_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L2_LINK_SR_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L2_TSR' */
#define L2_TSR_OFFSET 0x00002018 /* address offset of the 'L2_TSR' register */

/* Field  'L2_TSR.value' */
#define L2_TSR_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L2_TSR_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L2_TSR_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L2_TSR_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L2_DEBUG' */
#define L2_DEBUG_OFFSET 0x00002020 /* address offset of the 'L2_DEBUG' register */

/* Field  'L2_DEBUG.value' */
#define L2_DEBUG_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L2_DEBUG_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L2_DEBUG_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L2_DEBUG_VALUE_RESET 0xAA550002 /* reset value of the 'value' field */

/* Register 'L2_IRQSTAT_0' */
#define L2_IRQSTAT_0_OFFSET 0x00002024 /* address offset of the 'L2_IRQSTAT_0' register */

/* Field  'L2_IRQSTAT_0.value' */
#define L2_IRQSTAT_0_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L2_IRQSTAT_0_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L2_IRQSTAT_0_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L2_IRQSTAT_0_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L2_IRQSTAT_1' */
#define L2_IRQSTAT_1_OFFSET 0x00002028 /* address offset of the 'L2_IRQSTAT_1' register */

/* Field  'L2_IRQSTAT_1.value' */
#define L2_IRQSTAT_1_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L2_IRQSTAT_1_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L2_IRQSTAT_1_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L2_IRQSTAT_1_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L2_IRQ_OR7' */
#define L2_IRQ_OR7_OFFSET 0x0000202C /* address offset of the 'L2_IRQ_OR7' register */

/* Field  'L2_IRQ_OR7.value' */
#define L2_IRQ_OR7_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L2_IRQ_OR7_VALUE_BIT_WIDTH 8 /* bit width of the 'value' field */
#define L2_IRQ_OR7_VALUE_BIT_MASK 0x000000FF /* bit mask of the 'value' field */
#define L2_IRQ_OR7_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L2_IRQMASK_0' */
#define L2_IRQMASK_0_OFFSET 0x00002030 /* address offset of the 'L2_IRQMASK_0' register */

/* Field  'L2_IRQMASK_0.value' */
#define L2_IRQMASK_0_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L2_IRQMASK_0_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L2_IRQMASK_0_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L2_IRQMASK_0_VALUE_RESET 0x7F7F7F7F /* reset value of the 'value' field */

/* Register 'L2_IRQMASK_1' */
#define L2_IRQMASK_1_OFFSET 0x00002038 /* address offset of the 'L2_IRQMASK_1' register */

/* Field  'L2_IRQMASK_1.value' */
#define L2_IRQMASK_1_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L2_IRQMASK_1_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L2_IRQMASK_1_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L2_IRQMASK_1_VALUE_RESET 0x7F7F7F7F /* reset value of the 'value' field */

/* Register 'L2_PERF' */
#define L2_PERF_OFFSET 0x00002040 /* address offset of the 'L2_PERF' register */

/* Field  'L2_PERF.value' */
#define L2_PERF_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L2_PERF_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L2_PERF_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L2_PERF_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L2_I2C_COM' */
#define L2_I2C_COM_OFFSET 0x00002044 /* address offset of the 'L2_I2C_COM' register */

/* Field  'L2_I2C_COM.value' */
#define L2_I2C_COM_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L2_I2C_COM_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L2_I2C_COM_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L2_I2C_COM_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L2_I2C_DAT' */
#define L2_I2C_DAT_OFFSET 0x00002048 /* address offset of the 'L2_I2C_DAT' register */

/* Field  'L2_I2C_DAT.value' */
#define L2_I2C_DAT_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L2_I2C_DAT_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L2_I2C_DAT_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L2_I2C_DAT_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L3_TXFIFO' */
#define L3_TXFIFO_OFFSET 0x00003000 /* address offset of the 'L3_TXFIFO' register */

/* Field  'L3_TXFIFO.value' */
#define L3_TXFIFO_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L3_TXFIFO_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L3_TXFIFO_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L3_TXFIFO_VALUE_RESET 0x10101010 /* reset value of the 'value' field */

/* Register 'L3_RXFIFO' */
#define L3_RXFIFO_OFFSET 0x00003004 /* address offset of the 'L3_RXFIFO' register */

/* Field  'L3_RXFIFO.value' */
#define L3_RXFIFO_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L3_RXFIFO_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L3_RXFIFO_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L3_RXFIFO_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L3_IOCTL' */
#define L3_IOCTL_OFFSET 0x00003008 /* address offset of the 'L3_IOCTL' register */

/* Field  'L3_IOCTL.value' */
#define L3_IOCTL_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L3_IOCTL_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L3_IOCTL_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L3_IOCTL_VALUE_RESET 0xB /* reset value of the 'value' field */

/* Register 'L3_LINK_SR' */
#define L3_LINK_SR_OFFSET 0x0000300C /* address offset of the 'L3_LINK_SR' register */

/* Field  'L3_LINK_SR.value' */
#define L3_LINK_SR_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L3_LINK_SR_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L3_LINK_SR_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L3_LINK_SR_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L3_TSR' */
#define L3_TSR_OFFSET 0x00003018 /* address offset of the 'L3_TSR' register */

/* Field  'L3_TSR.value' */
#define L3_TSR_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L3_TSR_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L3_TSR_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L3_TSR_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L3_DEBUG' */
#define L3_DEBUG_OFFSET 0x00003020 /* address offset of the 'L3_DEBUG' register */

/* Field  'L3_DEBUG.value' */
#define L3_DEBUG_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L3_DEBUG_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L3_DEBUG_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L3_DEBUG_VALUE_RESET 0xAA550003 /* reset value of the 'value' field */

/* Register 'L3_IRQSTAT_0' */
#define L3_IRQSTAT_0_OFFSET 0x00003024 /* address offset of the 'L3_IRQSTAT_0' register */

/* Field  'L3_IRQSTAT_0.value' */
#define L3_IRQSTAT_0_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L3_IRQSTAT_0_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L3_IRQSTAT_0_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L3_IRQSTAT_0_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L3_IRQSTAT_1' */
#define L3_IRQSTAT_1_OFFSET 0x00003028 /* address offset of the 'L3_IRQSTAT_1' register */

/* Field  'L3_IRQSTAT_1.value' */
#define L3_IRQSTAT_1_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L3_IRQSTAT_1_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L3_IRQSTAT_1_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L3_IRQSTAT_1_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L3_IRQ_OR7' */
#define L3_IRQ_OR7_OFFSET 0x0000302C /* address offset of the 'L3_IRQ_OR7' register */

/* Field  'L3_IRQ_OR7.value' */
#define L3_IRQ_OR7_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L3_IRQ_OR7_VALUE_BIT_WIDTH 8 /* bit width of the 'value' field */
#define L3_IRQ_OR7_VALUE_BIT_MASK 0x000000FF /* bit mask of the 'value' field */
#define L3_IRQ_OR7_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L3_IRQMASK_0' */
#define L3_IRQMASK_0_OFFSET 0x00003030 /* address offset of the 'L3_IRQMASK_0' register */

/* Field  'L3_IRQMASK_0.value' */
#define L3_IRQMASK_0_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L3_IRQMASK_0_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L3_IRQMASK_0_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L3_IRQMASK_0_VALUE_RESET 0x7F7F7F7F /* reset value of the 'value' field */

/* Register 'L3_IRQMASK_1' */
#define L3_IRQMASK_1_OFFSET 0x00003038 /* address offset of the 'L3_IRQMASK_1' register */

/* Field  'L3_IRQMASK_1.value' */
#define L3_IRQMASK_1_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L3_IRQMASK_1_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L3_IRQMASK_1_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L3_IRQMASK_1_VALUE_RESET 0x7F7F7F7F /* reset value of the 'value' field */

/* Register 'L3_PERF' */
#define L3_PERF_OFFSET 0x00003040 /* address offset of the 'L3_PERF' register */

/* Field  'L3_PERF.value' */
#define L3_PERF_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L3_PERF_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L3_PERF_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L3_PERF_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L3_I2C_COM' */
#define L3_I2C_COM_OFFSET 0x00003044 /* address offset of the 'L3_I2C_COM' register */

/* Field  'L3_I2C_COM.value' */
#define L3_I2C_COM_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L3_I2C_COM_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L3_I2C_COM_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L3_I2C_COM_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'L3_I2C_DAT' */
#define L3_I2C_DAT_OFFSET 0x00003048 /* address offset of the 'L3_I2C_DAT' register */

/* Field  'L3_I2C_DAT.value' */
#define L3_I2C_DAT_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define L3_I2C_DAT_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define L3_I2C_DAT_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define L3_I2C_DAT_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'C_FW_REVISION' */
#define C_FW_REVISION_OFFSET 0x00004000 /* address offset of the 'C_FW_REVISION' register */

/* Field  'C_FW_REVISION.value' */
#define C_FW_REVISION_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define C_FW_REVISION_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define C_FW_REVISION_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define C_FW_REVISION_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'DSTAT' */
#define DSTAT_OFFSET 0x00004004 /* address offset of the 'DSTAT' register */

/* Field  'DSTAT.value' */
#define DSTAT_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define DSTAT_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define DSTAT_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define DSTAT_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'DCTRL' */
#define DCTRL_OFFSET 0x00004008 /* address offset of the 'DCTRL' register */

/* Field  'DCTRL.value' */
#define DCTRL_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define DCTRL_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define DCTRL_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define DCTRL_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'USER_IRQ_GEN' */
#define USER_IRQ_GEN_OFFSET 0x0000400C /* address offset of the 'USER_IRQ_GEN' register */

/* Field  'USER_IRQ_GEN.value' */
#define USER_IRQ_GEN_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define USER_IRQ_GEN_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define USER_IRQ_GEN_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define USER_IRQ_GEN_VALUE_RESET 0x0 /* reset value of the 'value' field */

/* Register 'REBOOT_FWU' */
#define REBOOT_FWU_OFFSET 0x00004010 /* address offset of the 'REBOOT_FWU' register */

/* Field  'REBOOT_FWU.eos' */
#define REBOOT_FWU_EOS_BIT_OFFSET 0 /* bit offset of the 'eos' field */
#define REBOOT_FWU_EOS_BIT_WIDTH 1 /* bit width of the 'eos' field */
#define REBOOT_FWU_EOS_BIT_MASK 0x00000001 /* bit mask of the 'eos' field */
#define REBOOT_FWU_EOS_RESET 0x0 /* reset value of the 'eos' field */

/* Register 'C_FW_BUILD' */
#define C_FW_BUILD_OFFSET 0x00004014 /* address offset of the 'C_FW_BUILD' register */

/* Field  'C_FW_BUILD.value' */
#define C_FW_BUILD_VALUE_BIT_OFFSET 0 /* bit offset of the 'value' field */
#define C_FW_BUILD_VALUE_BIT_WIDTH 32 /* bit width of the 'value' field */
#define C_FW_BUILD_VALUE_BIT_MASK 0xFFFFFFFF /* bit mask of the 'value' field */
#define C_FW_BUILD_VALUE_RESET 0x0 /* reset value of the 'value' field */

#endif  /* A5818_REGS_H */
