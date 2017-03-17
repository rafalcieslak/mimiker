#include <common.h>
#include <mips/malta.h>
#include <mips/uart_cbus.h>

#define QEMU_MALTA_EXIT_REQ (MALTA_FPGA_BASE + 0x100)
#define QEMU_EXIT_REQ                                            \
  *(volatile uint8_t *)(MIPS_PHYS_TO_KSEG1(QEMU_MALTA_EXIT_REQ))

void qemu_request_exit(uint8_t val){
  QEMU_EXIT_REQ = val;
}
