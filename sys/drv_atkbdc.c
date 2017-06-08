#define KL_LOG KL_DEV
#include <stdc.h>
#include <vnode.h>
#include <devfs.h>
#include <klog.h>
#include <condvar.h>
#include <pci.h>
#include <dev/isareg.h>
#include <dev/atkbdcreg.h>
#include <interrupt.h>
#include <sysinit.h>

#define KBD_DATA (IO_KBD + KBD_DATA_PORT)
#define KBD_STATUS (IO_KBD + KBD_STATUS_PORT)
#define KBD_COMMAND (IO_KBD + KBD_COMMAND_PORT)

#define KBD_BUFSIZE 128

typedef struct atkbdc_state {
  mtx_t mtx;
  condvar_t nonempty;
  unsigned head, tail, count;
  uint8_t buffer[KBD_BUFSIZE];
  resource_t *regs;
} atkbdc_state_t;

static bool atkbdc_put(atkbdc_state_t *atkbdc, uint8_t data) {
  if (atkbdc->count == KBD_BUFSIZE)
    return false;
  atkbdc->buffer[atkbdc->head++] = data;
  if (atkbdc->head >= KBD_BUFSIZE)
    atkbdc->head = 0;
  atkbdc->count++;
  return true;
}

static bool atkbdc_get(atkbdc_state_t *atkbdc, uint8_t *data) {
  if (atkbdc->count == 0)
    return false;
  *data = atkbdc->buffer[atkbdc->tail++];
  if (atkbdc->tail >= KBD_BUFSIZE)
    atkbdc->tail = 0;
  atkbdc->count--;
  return true;
}

/* For now, this is the only keyboard driver we'll want to have, so the
   interface is not very flexible. */

/* NOTE: These blocking wait helper functions can't use an interrupt, as the
   PS/2 controller does not generate interrupts for these events. However, this
   is not a major problem, since pretty much always the controller is
   immediately ready to proceed, so the we don't loop in practice. */
static inline void wait_before_read(resource_t *regs) {
  while (!(bus_space_read_1(regs, KBD_STATUS) & KBDS_KBD_BUFFER_FULL))
    ;
}

static inline void wait_before_write(resource_t *regs) {
  while (bus_space_read_1(regs, KBD_STATUS) & KBDS_INPUT_BUFFER_FULL)
    ;
}

static void write_command(resource_t *regs, uint8_t command) {
  wait_before_write(regs);
  bus_space_write_1(regs, KBD_COMMAND, command);
}

static uint8_t read_data(resource_t *regs) {
  wait_before_read(regs);
  return bus_space_read_1(regs, KBD_DATA);
}

static void write_data(resource_t *regs, uint8_t byte) {
  wait_before_write(regs);
  bus_space_write_1(regs, KBD_DATA, byte);
}

static int scancode_read(vnode_t *v, uio_t *uio) {
  atkbdc_state_t *atkbdc = v->v_data;
  int error;
  uint8_t data;

  uio->uio_offset = 0; /* This device does not support offsets. */

  WITH_MTX_LOCK (&atkbdc->mtx) {
    while (atkbdc->count == 0)
      cv_wait(&atkbdc->nonempty, &atkbdc->mtx);
    /* For simplicity, copy to the user space one byte at a time. */
    atkbdc_get(atkbdc, &data);
    if ((error = uiomove_frombuf(&data, 1, uio)))
      return error;
  }

  return 0;
}

static vnodeops_t dev_scancode_ops = {
  .v_lookup = vnode_lookup_nop,
  .v_readdir = vnode_readdir_nop,
  .v_open = vnode_open_generic,
  .v_write = vnode_write_nop,
  .v_read = scancode_read,
};

/* Reset keyboard and perform a self-test. */
static bool kbd_reset(resource_t *regs) {
  write_data(regs, KBDC_RESET_KBD);
  uint8_t response = read_data(regs);
  if (response != KBD_ACK)
    return false;
  return (read_data(regs) == KBD_RESET_DONE);
}

static intr_filter_t atkbdc_intr(void *data) {
  atkbdc_state_t *atkbdc = data;

  if (!(bus_space_read_1(atkbdc->regs, KBD_STATUS) & KBDS_KBD_BUFFER_FULL))
    return IF_STRAY;

  /* TODO: Some locking mechanism will be necessary if we want to sent extra
     commands to the ps/2 controller and/or other ps/2 devices, because this
     thread would interfere with commands/responses. */
  uint8_t code = read_data(atkbdc->regs);
  uint8_t code2 = 0;
  bool extended = (code == 0xe0);

  if (extended)
    code2 = read_data(atkbdc->regs);

  WITH_MTX_LOCK (&atkbdc->mtx) {
    /* TODO: There's no logic for processing scancodes. */
    atkbdc_put(atkbdc, code);
    if (extended)
      atkbdc_put(atkbdc, code2);
    cv_signal(&atkbdc->nonempty);
  }

  return IF_FILTERED;
}

static INTR_HANDLER_DEFINE(atkbdc_intr_handler, atkbdc_intr, NULL, NULL,
                           "AT keyboard controller", 0);

static int atkbdc_probe(device_t *dev) {
  assert(dev->parent->bus == DEV_BUS_PCI);
  pci_bus_state_t *pcib = dev->parent->state;
  resource_t *regs = pcib->io_space;

  if (!kbd_reset(regs)) {
    klog("Keyboard self-test failed.");
    return 0;
  }

  /* Enable scancode table #1 */
  write_data(regs, KBDC_SET_SCANCODE_SET);
  write_data(regs, 1);
  if (read_data(regs) != KBD_ACK)
    return 0;

  /* Start scanning. */
  write_data(regs, KBDC_ENABLE_KBD);
  if (read_data(regs) != KBD_ACK)
    return 0;

  return 1;
}

static int atkbdc_attach(device_t *dev) {
  assert(dev->parent->bus == DEV_BUS_PCI);

  pci_bus_state_t *pcib = dev->parent->state;
  atkbdc_state_t *atkbdc = dev->state;

  mtx_init(&atkbdc->mtx, MTX_DEF);
  cv_init(&atkbdc->nonempty, "AT keyboard buffer non-empty");
  atkbdc->regs = pcib->io_space;

  atkbdc_intr_handler->ih_argument = atkbdc;
  bus_intr_setup(dev, 1, atkbdc_intr_handler);

  /* Enable interrupt */
  write_command(atkbdc->regs, KBDC_SET_COMMAND_BYTE);
  write_data(atkbdc->regs, KBD_ENABLE_KBD_INT);

  /* Prepare /dev/scancode interface. */
  vnode_t *scancode_device = vnode_new(V_DEV, &dev_scancode_ops);
  scancode_device->v_data = atkbdc;
  devfs_install("scancode", scancode_device);

  return 0;
}

static driver_t atkbdc_driver = {
  .desc = "AT keyboard controller driver",
  .size = sizeof(atkbdc_state_t),
  .probe = atkbdc_probe,
  .attach = atkbdc_attach,
};

extern device_t *gt_pci;

static void atkbdc_init(void) {
  (void)make_device(gt_pci, &atkbdc_driver);
}

SYSINIT_ADD(atkbdc, atkbdc_init, DEPS("rootdev"));
