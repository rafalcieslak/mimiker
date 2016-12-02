#include <mips/mips.h>
#include <stdc.h>
#include <mips/gt64120.h>
#include <pci.h>

#define B2B_PATTERN "%c%c%c%c%c%c%c%c"
#define B2B(byte)                                                              \
  (byte & 0x80 ? '1' : '0'), (byte & 0x40 ? '1' : '0'),                        \
    (byte & 0x20 ? '1' : '0'), (byte & 0x10 ? '1' : '0'),                      \
    (byte & 0x08 ? '1' : '0'), (byte & 0x04 ? '1' : '0'),                      \
    (byte & 0x02 ? '1' : '0'), (byte & 0x01 ? '1' : '0')

#define FB_ACC(x) (volatile uint8_t *)(MIPS_PHYS_TO_KSEG1(0x10000000) + (x))
#define FB_ACC2(x)                                                             \
  (volatile uint8_t *)(MIPS_PHYS_TO_KSEG1(0x10000000 + 0x000A0000) + (x))
#define FB_ACC3(x)                                                             \
  (volatile uint8_t *)(MIPS_PHYS_TO_KSEG1(0x10000000 + 0x0000A000) + (x))

#define FB_AQ(x) (volatile uint8_t *)(MIPS_PHYS_TO_KSEG1(0x12000000) + (x))
#define FB_AQB(x) (volatile uint8_t *)(MIPS_PHYS_TO_KSEG1(0x18000000) + (x))

#define VGA_REG(x) (volatile uint8_t *)(MIPS_PHYS_TO_KSEG1(0x18000000) + (x))

uint8_t vga_gr_read(uint8_t gr) {
  volatile uint8_t *AR = FB_AQB(0x3CE);
  volatile uint8_t *DR = FB_AQB(0x3CF);
  *AR = gr;
  return *DR;
}

void vga_gr_write(uint8_t gr, uint8_t val) {
  volatile uint8_t *AR = FB_AQB(0x3CE);
  volatile uint8_t *DR = FB_AQB(0x3CF);
  *AR = gr;
  *DR = val;
}

uint8_t vga_cr_read(uint8_t cr) {
  volatile uint8_t *AR = FB_AQB(0x3b4);
  volatile uint8_t *DR = FB_AQB(0x3b5);
  *AR = cr;
  return *DR;
}

void vga_cr_write(uint8_t cr, uint8_t val) {
  volatile uint8_t *AR = FB_AQB(0x3b4);
  volatile uint8_t *DR = FB_AQB(0x3b5);
  *AR = cr;
  *DR = val;
}

uint8_t vga_sr_read(uint8_t gr) {
  volatile uint8_t *AR = FB_AQB(0x3C4);
  volatile uint8_t *DR = FB_AQB(0x3C5);
  *AR = gr;
  return *DR;
}

void vga_sr_write(uint8_t gr, uint8_t val) {
  volatile uint8_t *AR = FB_AQB(0x3C4);
  volatile uint8_t *DR = FB_AQB(0x3C5);
  *AR = gr;
  *DR = val;
}

void vga_set_pallete_addr_source(uint8_t v) {
  volatile uint8_t *AR = FB_AQB(0x3C0);
  *AR |= v << 5;
}

void vga_palette_write(uint8_t offset, uint8_t r, uint8_t g, uint8_t b) {
  volatile uint8_t *PEL_AWR = FB_AQB(0x3C8);
  volatile uint8_t *PEL_DR = FB_AQB(0x3C9);
  *PEL_AWR = offset;
  *PEL_DR = r >> 2;
  *PEL_DR = g >> 2;
  *PEL_DR = b >> 2;
}

/* height only up to 255, but this will do for now */
void vga_set_resolution(int w, int h) {
  uint8_t wq = w / 8 - 1;

  volatile uint8_t *AR = FB_AQB(0x3b4);
  volatile uint8_t *DR = FB_AQB(0x3b5);
  *AR = 0x01;
  *DR = wq;
  *AR = 0x12;
  *DR = h - 1;
}

extern pci_bus_t pci_bus[1];

#define PCI0_CFG_ADDR_R GT_R(GT_PCI0_CFG_ADDR)
#define PCI0_CFG_DATA_R GT_R(GT_PCI0_CFG_DATA)
#define PCI0_CFG_REG_SHIFT 2
#define PCI0_CFG_FUNCT_SHIFT 8
#define PCI0_CFG_DEV_SHIFT 11
#define PCI0_CFG_BUS_SHIFT 16
#define VGA_REG_EXT_MOR_READ VGA_REG(0x3CC)
#define VGA_REG_EXT_MOR_WRITE VGA_REG(0x3C2)
#define PCI0_CFG_ENABLE 0x80000000
#define PCI0_CFG_REG(dev, funct, reg)                                          \
  (((dev) << PCI0_CFG_DEV_SHIFT) | ((funct) << PCI0_CFG_FUNCT_SHIFT) |         \
   ((reg) << PCI0_CFG_REG_SHIFT))

int main() {

  /* Get the cirrus logic VGA adapter PCI device. I trust it's the 5th device!
   */
  pci_device_t *dev = pci_bus->dev + 5;
  kprintf("[pci:%02x:%02x.%02x]", dev->addr.bus, dev->addr.device,
          dev->addr.function);

  /* Access the PCI command register of the device. Enable memory space. */
  PCI0_CFG_ADDR_R =
    PCI0_CFG_ENABLE | PCI0_CFG_REG(dev->addr.device, dev->addr.function, 1);
  uint32_t status_command = PCI0_CFG_DATA_R;
  uint32_t command = status_command & 0x0000ffff;
  uint32_t status = status_command & 0xffff0000;
  command |= 0x0003; /* Memory Space enable */
  status_command = status | command;
  PCI0_CFG_DATA_R = status_command;

  /* Now, VGA setup. */

  /* Set RAM_ENABLE */
  uint8_t MOR = *VGA_REG_EXT_MOR_READ;
  kprintf("MOR: " B2B_PATTERN "\n", B2B(MOR));
  *VGA_REG_EXT_MOR_WRITE = 0x2;
  MOR = *VGA_REG_EXT_MOR_READ;
  kprintf("MOR: " B2B_PATTERN "\n", B2B(MOR));

  // Set memory map select to 0
  uint8_t memory_map_select = vga_gr_read(0x06) >> 2 & 0x3;
  kprintf("memory map select: " B2B_PATTERN "\n", B2B(memory_map_select));
  memory_map_select = 0;
  vga_gr_write(0x06, memory_map_select << 2);

  memory_map_select = vga_gr_read(0x06) >> 2 & 0x3;
  kprintf("memory map select: " B2B_PATTERN "\n", B2B(memory_map_select));

  // Selected memory range A0000h-BFFFFh. Probably irrelevant, as we use PCI LFB

  // Set chain 4 enable (? probably unnecessary for PCI LFB writing
  // vga_sr_write(0x04, vga_sr_read(0x04) | 0x08);

  // Enable 256-color palette
  vga_gr_write(0x05, vga_gr_read(0x05) | 0x40);

  // Set both Map Display Addresses to 1
  vga_cr_write(0x17, vga_cr_read(0x17) | 0x03);

  // Switch from text mode to graphics mode
  vga_gr_write(0x06, vga_gr_read(0x06) | 0x01);

  // Apply resolution
  vga_set_resolution(320, 200);

  // Set line offset register
  vga_cr_write(0x13, 320 / 8);
  // Set line compare register
  vga_cr_write(0x18, 200);

  // Prepare palette
  for (int i = 0; i < 256; i++) {
    vga_palette_write(i, i, i, i);
  }

  vga_palette_write(0x00, 0x04, 0x04, 0x04);
  vga_palette_write(0x01, 0xff, 0x00, 0x00);
  vga_palette_write(0x02, 0x00, 0xff, 0x00);
  vga_palette_write(0x03, 0x00, 0x00, 0xff);

  // Disable access to palette controller
  vga_set_pallete_addr_source(1);

  // Fill with black
  for (int i = 0; i < 320 * 200; i++) {
    volatile uint8_t *ptr = (uint8_t *)0xb0000000 + i;
    *ptr = 0x00;
  }

  int off = 0;
  int frames = 1000;
  while (frames--) {
    for (int i = 0; i < 320 * 200; i++) {
      volatile uint8_t *ptr = (uint8_t *)0xb0000000 + i;
      *ptr = (i / 320 + off) % 256;
    }
    off++;
  }
  kprintf("OK.\n");
  return 0;
}
