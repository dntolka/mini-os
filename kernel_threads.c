#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "util.h"
#include "kernel_cc.h"

PTCB* search(Tid_t tid){

	PTCB* ptcb = (PTCB*)tid;

	if(rlist_find(&CURPROC->PTCB_list, ptcb, NULL) == NULL){
		return NULL;
	}else{
		return ptcb;
	}
}

void start_another_thread()
{
	PTCB* ptcb = CURTHREAD->ptcb;
  int exitval;

  Task call =  ptcb->task;
  int argl = ptcb->argl;
  void* args = ptcb->args;

  exitval = call(argl,args);
  ThreadExit(exitval);
}

/**
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
	if(task != NULL){
		PTCB* ptcb= spawn_ptcb(CURPROC, task, argl, args);
		TCB* tcb = spawn_thread(CURPROC, start_another_thread);

		ptcb->tcb=tcb;
		tcb->ptcb = ptcb;

		if(ptcb != NULL)
			wakeup(tcb);
		  return (Tid_t)ptcb;
	}
	return NOTHREAD;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) CURTHREAD;
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
		PTCB* ptcb = search(tid);

		if(ptcb==NULL || ptcb->detached == DETACHED || (Tid_t)CURTHREAD == tid){
			return -1;
		}

		ptcb->ref_count++; // increase joints PTCB_list

		while(ptcb->exited != ISEXITED && ptcb->detached != DETACHED){
			kernel_wait(&ptcb->exit_cv, SCHED_USER);
		}

		ptcb->ref_count--; // decrease joints PTCB_list

		if(exitval != NULL){
			*exitval=ptcb->exitval;
		}

		if(ptcb->ref_count==0){
			rlist_remove(&ptcb->thread_list_node);
			free(ptcb);
		}


	return 0;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
	TCB* tcb = (TCB*)tid;
	PTCB* ptcb = search(tid);

	if(tcb != NULL){

		if(ptcb != NULL || ptcb->exited != ISEXITED){
			ptcb->detached=DETACHED;
			kernel_broadcast(&ptcb->exit_cv);
			return 0;
		}
	}

	return -1;

}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{
	if(CURTHREAD->ptcb!= NULL){
		CURPROC->ptcb_count--;

		CURTHREAD->ptcb->exitval = exitval;
 	  CURTHREAD->ptcb->exited = ISEXITED;
		kernel_broadcast(&CURTHREAD->ptcb->exit_cv);

		kernel_sleep(EXITED,SCHED_USER);
	}
}
