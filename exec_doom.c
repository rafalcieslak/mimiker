#include <common.h>
#include <exec.h>

int main() {
  exec_args_t exec_args;
  exec_args.prog_name = "headless_doom_headless_doom_benchmark";
  exec_args.argv = (char *[]){"DOOM.EXE"};
  exec_args.argc = 1;

  do_exec(&exec_args);

  return 0;
}
