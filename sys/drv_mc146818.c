#include <isa.h>
#include <common.h>
#include <stdc.h>
#include <vnode.h>
#include <devfs.h>
#include <errno.h>

#define MC146818_ADDR 0x70
#define MC146818_DATA 0x71

#define MC146818_REG_SECOND 0x00
#define MC146818_REG_MINUTE 0x02
#define MC146818_REG_HOUR 0x04
#define MC146818_REG_DAY 0x07
#define MC146818_REG_MONTH 0x08
#define MC146818_REG_YEAR 0x09
#define MC146818_REG_B 0x0B

#define MC146818_REG_B_BCD 0x04
#define MC146818_REG_B_24H 0x02

typedef struct mc146818_state {
  resource_t *io;
  resource_t regs;
} mc146818_state_t;

static int mc146818_probe(device_t *dev) {
  isa_device_t *isad = isa_device_of(dev);

  if (!isad)
    return 0;

  /* TODO: Actually do some very basic test to see if the RTC is present. */

  return 1;
}

static void mc146817_regs_write_1(resource_t *handle, unsigned offset,
                                  uint8_t val) {
  mc146818_state_t *mc146818 = handle->r_owner;
  bus_space_write_1(mc146818->io, MC146818_ADDR, offset);
  bus_space_write_1(mc146818->io, MC146818_DATA, val);
}
static uint8_t mc146817_regs_read_1(resource_t *handle, unsigned offset) {
  mc146818_state_t *mc146818 = handle->r_owner;
  bus_space_write_1(mc146818->io, MC146818_ADDR, offset);
  return bus_space_read_1(mc146818->io, MC146818_DATA);
}

static bus_space_t mc146818_regs_bus_space = {.read_1 = mc146817_regs_read_1,
                                              .write_1 = mc146817_regs_write_1};

static void mc146818_read_time(mc146818_state_t *mc146818, int *year,
                               int *month, int *day, int *hour, int *minute,
                               int *second) {
  *second = bus_space_read_1(&mc146818->regs, MC146818_REG_SECOND);
  *minute = bus_space_read_1(&mc146818->regs, MC146818_REG_MINUTE);
  *hour = bus_space_read_1(&mc146818->regs, MC146818_REG_HOUR);
  *day = bus_space_read_1(&mc146818->regs, MC146818_REG_DAY);
  *month = bus_space_read_1(&mc146818->regs, MC146818_REG_MONTH);
  *year = bus_space_read_1(&mc146818->regs, MC146818_REG_YEAR) + 2000;
}

#define DEV_MC146818_BUFFER_SIZE 32
static int dev_mc146818_read(vnode_t *v, uio_t *uio) {
  uio->uio_offset = 0; /* This device does not support offsets. */
  mc146818_state_t *mc146818 = v->v_data;
  int year, month, day, hour, minute, second;
  mc146818_read_time(mc146818, &year, &month, &day, &hour, &minute, &second);
  char buffer[DEV_MC146818_BUFFER_SIZE];
  int count = snprintf(buffer, DEV_MC146818_BUFFER_SIZE, "%d %d %d %d %d %d",
                       year, month, day, hour, minute, second);
  if (count >= DEV_MC146818_BUFFER_SIZE)
    return -EINVAL;
  int error = uiomove_frombuf(buffer, DEV_MC146818_BUFFER_SIZE, uio);
  if (error)
    return error;
  return 0;
}

static vnodeops_t dev_mc146818_vnodeops = {
  .v_lookup = vnode_op_notsup,
  .v_readdir = vnode_op_notsup,
  .v_open = vnode_op_notsup,
  .v_write = vnode_op_notsup,
  .v_read = dev_mc146818_read,
};

static int mc146818_attach(device_t *dev) {
  isa_device_t *isad = isa_device_of(dev);
  mc146818_state_t *mc146818 = dev->state;

  /* TODO: bus_allocate_resource */
  /* TODO: Only allocate 0070-007f!  */
  mc146818->io = isad->isa_bus;

  mc146818->regs = (resource_t){.r_owner = mc146818,
                                .r_bus_space = &mc146818_regs_bus_space,
                                .r_type = RT_IOPORTS,
                                .r_start = 0,
                                .r_end = 0x3f};

  bus_space_write_1(&mc146818->regs, MC146818_REG_B,
                    MC146818_REG_B_24H | MC146818_REG_B_BCD);

  static int installed = 0;
  if (!installed++) {
    /* Only the first instance gets to devfs. */
    vnode_t *dev_mc146818 = vnode_new(V_DEV, &dev_mc146818_vnodeops);
    dev_mc146818->v_data = mc146818;
    devfs_install("rtc", dev_mc146818);
  }

  return 0;
}

static driver_t mc146818 = {
  .desc = "MC146818 Real-Time Clock driver",
  .size = sizeof(mc146818_state_t),
  .probe = mc146818_probe,
  .attach = mc146818_attach,
};

DRIVER_ADD(mc146818);