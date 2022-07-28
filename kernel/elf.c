/*
 * routines that scan and load a (host) Executable and Linkable Format (ELF) file
 * into the (emulated) memory.
 */

#include "elf.h"
#include "string.h"
#include "riscv.h"
#include "spike_interface/spike_utils.h"

//
// the implementation of allocater. allocates memory space for later segment loading
//
static void *elf_alloc_mb(elf_ctx *ctx, uint64 elf_pa, uint64 elf_va, uint64 size) {
  // directly returns the virtual address as we are in the Bare mode in lab1_x
  return (void *)elf_va;
}

//
// actual file reading, using the spike file interface.
//
static uint64 elf_fpread(elf_ctx *ctx, void *dest, uint64 nb, uint64 offset) {
  elf_info *msg = (elf_info *)ctx->info;
  // call spike file utility to load the content of elf file into memory.
  // spike_file_pread will read the elf file (msg->f) from offset to memory (indicated by
  // *dest) for nb bytes.
  return spike_file_pread(msg->f, dest, nb, offset);
}

//
// init elf_ctx, a data structure that loads the elf.
//
elf_status elf_init(elf_ctx *ctx, void *info) {
  ctx->info = info;

  // load the elf header
  if (elf_fpread(ctx, &ctx->ehdr, sizeof(ctx->ehdr), 0) != sizeof(ctx->ehdr)) return EL_EIO;

  // check the signature (magic value) of the elf
  if (ctx->ehdr.magic != ELF_MAGIC) return EL_NOTELF;

  return EL_OK;
}

//
// load the elf segments to memory regions as we are in Bare mode in lab1
//
elf_status elf_load(elf_ctx *ctx) {
  // elf_prog_header structure is defined in kernel/elf.h
  elf_prog_header ph_addr;
  int i, off;

  // traverse the elf program segment headers
  for (i = 0, off = ctx->ehdr.phoff; i < ctx->ehdr.phnum; i++, off += sizeof(ph_addr)) {
    // read segment headers
    if (elf_fpread(ctx, (void *)&ph_addr, sizeof(ph_addr), off) != sizeof(ph_addr)) return EL_EIO;

    if (ph_addr.type != ELF_PROG_LOAD) continue;
    if (ph_addr.memsz < ph_addr.filesz) return EL_ERR;
    if (ph_addr.vaddr + ph_addr.memsz < ph_addr.vaddr) return EL_ERR;

    // allocate memory block before elf loading
    void *dest = elf_alloc_mb(ctx, ph_addr.vaddr, ph_addr.vaddr, ph_addr.memsz);

    // actual loading
    if (elf_fpread(ctx, dest, ph_addr.memsz, ph_addr.off) != ph_addr.memsz)
      return EL_EIO;
  }

  return EL_OK;
}

//
// returns the number (should be 1) of string(s) after PKE kernel in command line.
// and store the string(s) in arg_bug_msg.
//
static size_t parse_args(arg_buf *arg_bug_msg) {
  // HTIFSYS_getmainvars frontend call reads command arguments to (input) *arg_bug_msg
  long r = frontend_syscall(HTIFSYS_getmainvars, (uint64)arg_bug_msg,
      sizeof(*arg_bug_msg), 0, 0, 0, 0, 0);
  kassert(r == 0);

  size_t pk_argc = arg_bug_msg->buf[0];
  uint64 *pk_argv = &arg_bug_msg->buf[1];

  int arg = 1;  // skip the PKE OS kernel string, leave behind only the application name
  for (size_t i = 0; arg + i < pk_argc; i++)
    arg_bug_msg->argv[i] = (char *)(uintptr_t)pk_argv[arg + i];

  //returns the number of strings after PKE kernel in command line
  return pk_argc - arg;
}

//
// load the elf of user application, by using the spike file interface.
//
void load_bincode_from_host_elf(process *p) {
  arg_buf arg_bug_msg;

  // retrieve command line arguements
  size_t argc = parse_args(&arg_bug_msg);
  if (!argc) panic("You need to specify the application program!\n");

  sprint("Application: %s\n", arg_bug_msg.argv[0]);

  //elf loading. elf_ctx is defined in kernel/elf.h, used to track the loading process.
  elf_ctx elfloader;
  // elf_info is defined above, used to tie the elf file and its corresponding process.
  elf_info info;

  info.f = spike_file_open(arg_bug_msg.argv[0], O_RDONLY, 0);
  info.p = p;
  // IS_ERR_VALUE is a macro defined in spike_interface/spike_htif.h
  if (IS_ERR_VALUE(info.f)) panic("Fail on openning the input application program.\n");

  // init elfloader context. elf_init() is defined above.
  if (elf_init(&elfloader, &info) != EL_OK)
    panic("fail to init elfloader.\n");

  // load elf. elf_load() is defined above.
  if (elf_load(&elfloader) != EL_OK) panic("Fail on loading elf.\n");

  // entry (virtual, also physical in lab1_x) address
  p->trapframe->epc = elfloader.ehdr.entry;

  // close the host spike file
  spike_file_close( info.f );

  sprint("Application program entry point (virtual address): 0x%lx\n", p->trapframe->epc);
}

char* get_file_name(){
  arg_buf arg_bug_msg;

  // retrieve command line arguements
  size_t argc = parse_args(&arg_bug_msg);
  if (!argc) panic("You need to specify the application program!\n");

  //sprint("Application: %s\n", arg_bug_msg.argv[0]);
  return arg_bug_msg.argv[0];
}

void get_name_from_addr(uint64 addr,char* funcname){

  char * appname=get_file_name();

  //elf loading. elf_ctx is defined in kernel/elf.h, used to track the loading process.
  elf_ctx elfloader;
  // elf_info is defined above, used to tie the elf file and its corresponding process.
  elf_info info;

  info.f = spike_file_open(appname, O_RDONLY, 0);
  // IS_ERR_VALUE is a macro defined in spike_interface/spike_htif.h
  if (IS_ERR_VALUE(info.f)) panic("Fail on openning the input application program.\n");

  // init elfloader context. elf_init() is defined above.
  if (elf_init(&elfloader, &info) != EL_OK)
    panic("fail to init elfloader.\n");

  //sprint("shstrndx:%d\n",elfloader.ehdr.shstrndx);

  int i, off;
  Elf64_Shdr ph_addr;
  Elf64_Shdr symtab;
  char stringtable[256];
  Elf64_Sym symentry;
  //sprint("string table index:%d\n",elfloader.ehdr.shstrndx);

  //get the string table(.shstrtab) section header
  off=elfloader.ehdr.shoff+(elfloader.ehdr.shstrndx)*elfloader.ehdr.shentsize;
  if (elf_fpread(&elfloader, (void *)&ph_addr, sizeof(ph_addr), off) != sizeof(ph_addr)){
    panic("sys_user_backtrace elf_fpread1");
  }
  //get the string table setiong
  //sprint("string table offset:%lld\n",ph_addr.sh_offset);
  if (elf_fpread(&elfloader,(void*)stringtable,ph_addr.sh_size,ph_addr.sh_offset) != ph_addr.sh_size){
    panic("sys_user_backtrace elf_fpread2");
  }

  for(i = 0, off = elfloader.ehdr.shoff; i < elfloader.ehdr.shnum; i++, off += elfloader.ehdr.shentsize){
    // find .symtab
    if(elf_fpread(&elfloader,(void*)&symtab,sizeof(symtab),off) != sizeof(symtab)){
      panic("sys_user_backtrace elf_fpread5");
    }
    if(strcmp(stringtable+symtab.sh_name,".symtab") == 0){
      //sprint("find .symtab\n");
      break;
    }
  }

  for (i = 0, off = elfloader.ehdr.shoff; i < elfloader.ehdr.shnum; i++, off += elfloader.ehdr.shentsize) {
    // find .strtab
    if(elf_fpread(&elfloader,(void*)&ph_addr,sizeof(ph_addr),off) != sizeof(ph_addr)){
      panic("sys_user_backtrace elf_fpread3");
    }
    //sprint("%s\n",stringtable+ph_addr.sh_name);
    if(strcmp(stringtable+ph_addr.sh_name,".strtab") == 0){
      //sprint("find .strtab\n");
      memset(stringtable,0,sizeof(stringtable));
      if (elf_fpread(&elfloader,(void*)stringtable,ph_addr.sh_size,ph_addr.sh_offset) != ph_addr.sh_size){
        panic("sys_user_backtrace elf_fpread4");
      }
      break;
    }
    //sprint("%d %lld\n",ph_addr.sh_name,ph_addr.sh_offset);
  }

  uint64 mindis=0x8000000000000000,index=-1;
  //sprint("%d %d\n",symentry.st_size)
  for(i=0,off=symtab.sh_offset;i<(symtab.sh_size/symtab.sh_entsize);i++,off+=sizeof(Elf64_Sym)){
    if(elf_fpread(&elfloader,(void*)&symentry,sizeof(symentry),off) != sizeof(symentry)){
      panic("sys_user_backtrace elf_fpread6");
    }
    //sprint("index:%d,type:%d,value:%x\n",i,symentry.st_info&0xf,symentry.st_value);
    if((symentry.st_info&0xf)==2 && symentry.st_value<=addr && addr-symentry.st_value<mindis){
      mindis=addr-symentry.st_value;
      index=i;
      if(elf_fpread(&elfloader,(void*)&symentry,sizeof(symentry),off) != sizeof(symentry)){
        panic("sys_user_backtrace elf_fpread7");
      }
      //sprint("index:%d  %s\n",index, stringtable+symentry.st_name);
    }
  }
  if(index==-1){
    panic("do not find");
  }
  off=symtab.sh_offset+index*sizeof(Elf64_Sym);
  if(elf_fpread(&elfloader,(void*)&symentry,sizeof(symentry),off) != sizeof(symentry)){
    panic("sys_user_backtrace elf_fpread7");
  }
  //sprint("%s\n",stringtable+symentry.st_name);
  strcpy(funcname,stringtable+symentry.st_name);
  spike_file_close( info.f );
}