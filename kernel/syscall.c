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
#include "pmm.h"
#include "vmm.h"
#include "spike_interface/spike_utils.h"

//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char* buf, size_t n) {
  // buf is now an address in user space of the given app's user stack,
  // so we have to transfer it into phisical address (kernel is running in direct mapping).
  assert( current );
  char* pa = (char*)user_va_to_pa((pagetable_t)(current->pagetable), (void*)buf);
  sprint(pa);
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

//
// maybe, the simplest implementation of malloc in the world ... added @lab2_2
//
uint64 sys_user_allocate_page(int num) {
  if(current->alloc_num >= VMA_SIZE){
    panic("too many heap alloc");
  }
  int new_alloc = 0;
  int this_area = -1;
  for(int i = 0; i < VMA_SIZE; i++){
    if(current->vmarea[i].has_alloc == 0 && (current->vmarea[i].size >= num || current->vmarea[i].size == 0)){
      if(current->vmarea[i].size == 0){
        new_alloc = 1;
      }
      this_area = i;
      break;
    }
  }
  if(this_area == -1){
    panic("index -1");
  }
  current->vmarea[this_area].has_alloc = 1;
  current->vmarea[this_area].size = num;
  if(new_alloc){
    current->vmarea[this_area].va = g_ufree_page;
    uint64 g_pg_up = (0xfffffffffffff000 & (g_ufree_page + PGSIZE -1));
    if(g_ufree_page + num >= (0xfffffffffffff000 & (g_ufree_page + PGSIZE -1))){
      void* pa = alloc_page();
      uint64 va = g_pg_up;
      user_vm_map((pagetable_t)current->pagetable, va, PGSIZE, (uint64)pa,
          prot_to_type(PROT_WRITE | PROT_READ, 1));
    }
    g_ufree_page += num;
  }else{
    uint64 start_vm, end_vm;
    start_vm = (0xfffffffffffff000 & (current->vmarea[this_area].va + PGSIZE -1));
    end_vm = start_vm + current->vmarea[this_area].size;
    for(uint64 vm_p = start_vm; vm_p < end_vm; vm_p += PGSIZE){
      if(lookup_pa(current->pagetable, vm_p) == 0){
        void* pa = alloc_page();
        user_vm_map((pagetable_t)current->pagetable, vm_p, PGSIZE, (uint64)pa, prot_to_type(PROT_WRITE|PROT_READ, 1));
      }
    }
  }

  return current->vmarea[this_area].va;
}

//
// reclaim a page, indicated by "va". added @lab2_2
//
uint64 sys_user_free_page(uint64 va) {

  uint64 va_down, va_up;
  if(va % PGSIZE == 0){
    va_down = va;
    va_up = va + PGSIZE;
  }else{
    va_down = va - va % PGSIZE;
    va_up = va + PGSIZE;
  }
  int can_free = 1;
  for(int i = 0 ;i < VMA_SIZE; i++){
    if(current->vmarea[i].has_alloc && current->vmarea[i].va == va){
      current->vmarea[i].has_alloc = 0;
    }else if(current->vmarea[i].has_alloc && current->vmarea[i].va < va_up && current->vmarea[i].va >= va_down){
      can_free = 0;
    }
  }
  if(can_free){
    user_vm_unmap((pagetable_t)current->pagetable, va_down, PGSIZE, 1);
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
    // added @lab2_2
    case SYS_user_allocate_page:
      return sys_user_allocate_page(a1);
    case SYS_user_free_page:
      return sys_user_free_page(a1);
    default:
      panic("Unknown syscall %ld \n", a0);
  }
}
