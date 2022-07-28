/*
 * contains the implementation of all syscalls.
 */

#include <stdint.h>
#include <errno.h>

#include "util/types.h"
#include "syscall.h"
#include "string.h"
#include "process.h"
#include "util/functions.h"
#include "elf.h"
#include "spike_interface/spike_utils.h"

//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char* buf, size_t n) {
  sprint(buf);
  return 0;
}

//
// implement the SYS_user_exit syscall
//
ssize_t sys_user_exit(uint64 code) {
  sprint("User exit with code:%d.\n", code);
  // in lab1, PKE considers only one app (one process). 
  // therefore, shutdown the system when the app calls exit()
  shutdown(code);
}

ssize_t sys_user_backtrace(uint64 depth,riscv_regs* regp){
  if(depth<0){
    panic("backtrace depth below 0");
  }
  if(depth==0)
    return 0;
  
  char funname[256];
  uint64 fp=regp->s0-16;
  // for(int i=0;i<16;i++){
  //   sprint("%x %x\n",fp+8*i,*(uint64*)(fp+8*i));
  // }

  fp=regp->s0;
  uint64 faddr;

  //sprint("%x\n",fp-8);
  fp=*(uint64*)(fp-8);
  //sprint("%x\n",fp-8);
  faddr=*(uint64*)(fp-8);
  //sprint("func addr :%x\n",faddr);
  //get_name_from_addr(regp->ra,funname);
  get_name_from_addr(faddr,funname);
  sprint("%s\n",funname);
  if(strcmp("main",funname)==0)
    return 0;
  depth--;

  while(depth){
    fp=*(uint64*)(fp-16);
    //sprint("%x\n",fp);
    faddr=*(uint64*)(fp-8);
    //sprint("func addr :%x\n",faddr);
    //get_name_from_addr(regp->ra,funname);
    get_name_from_addr(faddr,funname);
    sprint("%s\n",funname);
    if(strcmp("main",funname)==0)
      return 0;

    depth--;
  }



  return 0;
}

//
// [a0]: the syscall number; [a1] ... [a7]: arguments to the syscalls.
// returns the code of success, (e.g., 0 means success, fail for otherwise)
//
long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
  switch (a0) {
    case SYS_user_print:
      return sys_user_print((const char*)a1, a2);
    case SYS_user_exit:
      return sys_user_exit(a1);
    case SYS_user_backtrace:
      return sys_user_backtrace(a1,(riscv_regs*)a2);
    default:
      panic("Unknown syscall %ld \n", a0);
  }
}
