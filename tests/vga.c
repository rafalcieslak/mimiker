#include <stdc.h>
#include <pci.h>
#include <vga.h>

static void hue(uint8_t q, uint8_t *r, uint8_t *g, uint8_t *b) {
#define CH(x, y, z)                                                            \
  {                                                                            \
    x = r;                                                                     \
    y = g;                                                                     \
    x = b;                                                                     \
  }
  uint8_t x_, *x = &x_;
  uint8_t s = q / 42;
  uint8_t t = q % 42;
  /* Clip far end. Red stripe will be a few pixels too wide, but nobody will
     notice. */
  if (s == 6) {
    s = 5;
    t = 41;
  }
  assert(0 <= s && s <= 5);
  uint8_t *max = (uint8_t *[]){r, g, g, b, b, r}[s];
  uint8_t *min = (uint8_t *[]){b, b, r, r, g, g}[s];
  uint8_t *rise = (uint8_t *[]){g, x, b, x, r, x}[s];
  uint8_t *fall = (uint8_t *[]){x, r, x, g, x, b}[s];
  *max = 255;
  *min = 0;
  *rise = t * 6;
  *fall = (42 - t) * 6;
}

int main() {
  /* Cirrus Logic VGA adapter PCI device. I hope it's the 5th device! */
  pci_device_t *dev = pci_get_nth_device(5);

  vga_control_t vga_, *vga = &vga_;
  int error = vga_control_pci_init(dev, vga);
  if (error)
    return error;

  /* Now, VGA setup. */
  vga_init(vga);

  /* Prepare palette */
  uint8_t palette_buffer[256 * 3];
  for (int i = 0; i < 256; i++) {
    hue(i, palette_buffer + 3 * i + 0, palette_buffer + 3 * i + 1,
        palette_buffer + 3 * i + 2);
  }
  vga_palette_write(vga, palette_buffer);

  /* Framebuffer */
  uint8_t frame_buffer[320 * 200];

  int off = 0;
  int frames = 1000;
  while (frames--) {
    for (int i = 0; i < 320 * 200; i++)
      frame_buffer[i] = (i / 320 + off) % 256;
    vga_fb_write(vga, frame_buffer);
    off++;
  }
  kprintf("OK.\n");
  return 0;
}
