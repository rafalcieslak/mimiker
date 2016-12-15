TESTS = \
	callout.elf \
	crash.elf \
	embedfs.elf \
	exec.elf \
	exec_doom.elf \
	exec_fd_test.elf \
	exec_misbehave.elf \
	exec_vgatest.elf \
	linker_set.elf \
	malloc.elf \
	mutex.elf \
	physmem.elf \
	pmap.elf \
	rtc.elf \
	sched.elf \
	sleepq.elf \
	syscall.elf \
	thread.elf \
	uiomove.elf \
	vm_map.elf \
	vga.elf \
	vfs.elf
SOURCES_C = $(patsubst %.elf,%.c,$(TESTS))
SOURCES_ASM =

all: $(TESTS)

LD_EMBED_FILES += embedfs.txt.eo

include ../Makefile.common

LD_EMBED_FILES += $(TOPDIR)/user/headless_doom/doom.wad.eo
LD_EMBED_FILES += $(TOPDIR)/user/headless_doom/DDQ-EP1.LMP.eo
LD_EMBED_FILES += $(TOPDIR)/user/headless_doom/DDQ-EP2.LMP.eo
LD_EMBED_FILES += $(TOPDIR)/user/headless_doom/DDQ-EP3.LMP.eo
LD_EMBED_FILES += $(TOPDIR)/user/headless_doom/DDQ-EP4.LMP.eo

clean:
	$(RM) -f .*.D *.o *.elf *.map *~ *.eo
