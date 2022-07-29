#include "kernel/riscv.h"
#include "kernel/process.h"
#include "spike_interface/spike_utils.h"
#include "util/string.h"

static void handle_instruction_access_fault() { panic("Instruction access fault!"); }

static void handle_load_access_fault() { panic("Load access fault!"); }

static void handle_store_access_fault() { panic("Store/AMO access fault!"); }

static void handle_illegal_instruction(){
  addr_line* templine=current->line;
  riscv_regs* last_frame=(riscv_regs*)read_csr(mscratch);
  //sprint("ra:%x\n",last_frame->ra);
  while(templine){
    //sprint("line:%lld code:%x\n",templine->line,templine->addr);
    if(last_frame->ra == templine->addr){
      //sprint("find error code\n");
      uint64 error_code_line=templine->line;
      uint64 file_index=templine->file;
      uint64 dir_index=current->file[file_index].dir;
      sprint("Runtime error at %s/%s:%lld\n",current->dir[dir_index],current->file[file_index].file,templine->line);

      char filepath[256];
      char code_oneline[256];
      memset(filepath,0,sizeof(filepath));
      memcpy(filepath,current->dir[dir_index],strlen(current->dir[dir_index]));
      filepath[strlen(current->dir[dir_index])]='/';
      memcpy(filepath+1+strlen(current->dir[dir_index]),current->file[file_index].file,strlen(current->file[file_index].file));
      //sprint("%s\n",filepath);
      spike_file_t *f;
      f = spike_file_open(filepath, O_RDONLY, 0);
      for(int i=1;i<templine->line;){
        spike_file_read(f,code_oneline,1);
        //sprint("%c",code_oneline[0]);
        if(code_oneline[0]=='\n')
        {
          i++;
        }
      }
      for(;;){
        spike_file_read(f,code_oneline,1);
        sprint("%c",code_oneline[0]);
        if(code_oneline[0]=='\n')
        break;
      }
      spike_file_close(f);
      panic("Illegal instruction!");
    }
    templine++;
  } 
  panic("Illegal instruction!");
}

static void handle_misaligned_load() { panic("Misaligned Load!"); }

static void handle_misaligned_store() { panic("Misaligned AMO!"); }

// added @lab1_3
static void handle_timer() {
  int cpuid = 0;
  // setup the timer fired at next time (TIMER_INTERVAL from now)
  *(uint64*)CLINT_MTIMECMP(cpuid) = *(uint64*)CLINT_MTIMECMP(cpuid) + TIMER_INTERVAL;

  // setup a soft interrupt in sip (S-mode Interrupt Pending) to be handled in S-mode
  write_csr(sip, SIP_SSIP);
}

//
// handle_mtrap calls a handling function according to the type of a machine mode interrupt (trap).
//
void handle_mtrap() {
  uint64 mcause = read_csr(mcause);
  switch (mcause) {
    case CAUSE_MTIMER:
      handle_timer();
      break;
    case CAUSE_FETCH_ACCESS:
      handle_instruction_access_fault();
      break;
    case CAUSE_LOAD_ACCESS:
      handle_load_access_fault();
    case CAUSE_STORE_ACCESS:
      handle_store_access_fault();
      break;
    case CAUSE_ILLEGAL_INSTRUCTION:
      // TODO (lab1_2): call handle_illegal_instruction to implement illegal instruction
      // interception, and finish lab1_2.
      handle_illegal_instruction();

      break;
    case CAUSE_MISALIGNED_LOAD:
      handle_misaligned_load();
      break;
    case CAUSE_MISALIGNED_STORE:
      handle_misaligned_store();
      break;

    default:
      sprint("machine trap(): unexpected mscause %p\n", mcause);
      sprint("            mepc=%p mtval=%p\n", read_csr(mepc), read_csr(mtval));
      panic( "unexpected exception happened in M-mode.\n" );
      break;
  }
}
