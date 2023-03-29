
#include "spike_interface/spike_utils.h"
#include "process.h"
#include "sched.h"

extern process * procs;
process* block_queue_head = NULL;

void check_blocked_queue(){
    process* p = block_queue_head;
    while (p) {
        sprint("\nblock %d for %d\n", p->pid, p->blocked_for_semaphore);
        p=p->queue_next;
    }
}

// insert a process, proc, into the END of block queue.
void insert_to_block_queue(process* proc) {
  // sprint( "going to insert process %d to block queue.\n", proc->pid );
  // if the queue is empty in the beginning
  if( block_queue_head == NULL ){
    proc->status = BLOCKED;
    proc->queue_next = NULL;
    block_queue_head = proc;
    return;
  }

  // block queue is not empty
  process *p;
  // browse the block queue to see if proc is already in-queue
  for( p=block_queue_head; p->queue_next!=NULL; p=p->queue_next )
    if( p == proc ) return;  //already in queue

  // p points to the last element of the ready queue
  if( p==proc ) return;
  p->queue_next = proc;
  proc->status = BLOCKED;
  proc->queue_next = NULL;

  return;
}

process* remove_from_block_queue(int which_sem){
    if(block_queue_head == NULL){
        panic("blocked queue empty!\n");
    }
    process* p, *pre;
    pre = block_queue_head;
    if(pre->blocked_for_semaphore == which_sem){
        p = pre;
        block_queue_head = pre->queue_next;
        p->queue_next = NULL;
        //sprint("remove process %d for %d\n", p->pid, p->blocked_for_semaphore);
        return p;
    }
    p=pre->queue_next;
    while(p){
        if(p->blocked_for_semaphore == which_sem){
            pre->queue_next = p->queue_next;
            p->queue_next = NULL;
            return p;
        }else{
            p=p->queue_next;
            pre=pre->queue_next;
        }
    }
    panic("do not get the blocked proc\n");
}

#define sem_num 10
int semaphore[sem_num];
int sem_has_allocted = 1;

int new_a_semaphore(int original_num){
    if(original_num < 0){
        panic("semaphore original num < 0!\n");
    }
    if(sem_has_allocted > sem_num){
        panic("alloc semaphore too much!\n");
    }
    //sprint("semaphore %d get original num %d\n", sem_has_allocted, original_num);
    semaphore[sem_has_allocted++] = original_num;
    return sem_has_allocted - 1;
}

void sem_P(int which_sem){
    //sprint("P %d\n", which_sem);
    semaphore[which_sem]--;
    if(semaphore[which_sem] >= 0){
        //sprint("process %d pass P\n", current->pid);
        return;
    }
    //sprint("process %d blocked for semaphore %d\n", current->pid, which_sem);
    current->blocked_for_semaphore = which_sem;
    insert_to_block_queue(current);
    // check_blocked_queue();
    schedule();
}

void sem_V(int which_sem){
    //sprint("V %d\n", which_sem);
    if(semaphore[which_sem] < 0){
        process* p = remove_from_block_queue(which_sem);
        // check_blocked_queue();
        p->blocked_for_semaphore = -1;
        //sprint("insert process %d to ready queue\n", p->pid);
        insert_to_ready_queue(p);
    }
    semaphore[which_sem]++;
}