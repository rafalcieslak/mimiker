#include <common.h>
#include <exec.h>
#include <embedfs.h>

EMBED_FILE("doom.wad", doom_wad);
EMBED_FILE("DDQ-EP1.LMP", DDQ_EP1_LMP);

int main() {
  exec_args_t exec_args;
  exec_args.prog_name = "DOOM_benchmark";
  exec_args.argv = (char *[]){"DOOM.EXE"};
  exec_args.argc = 1;

  do_exec(&exec_args);

  return 0;
}
