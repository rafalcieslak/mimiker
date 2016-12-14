#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

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

uint8_t palette_buffer[256 * 3];
uint8_t frame_buffer[320 * 200];

int main() {
  int res;
  int fb = open("/dev/vga/fb", 0, O_WRONLY);
  int palette = open("/dev/vga/palette", 0, O_WRONLY);
  assert(fb >= 0 && palette >= 0);

  /* Prepare palette */
  for (int i = 0; i < 256; i++) {
    hue(i, palette_buffer + 3 * i + 0, palette_buffer + 3 * i + 1,
        palette_buffer + 3 * i + 2);
  }

  res = write(palette, palette_buffer, 256 * 3);
  assert(res == 256 * 3);

  int off = 0;
  int frames = 1000;
  while (frames--) {
    for (int i = 0; i < 320 * 200; i++)
      frame_buffer[i] = (i / 320 + off) % 256;

    res = write(fb, frame_buffer, 320 * 200);
    assert(res == 320 * 200);

    off++;
  }

  printf("Displayed 1000 frames.\n");

  return 0;
}
