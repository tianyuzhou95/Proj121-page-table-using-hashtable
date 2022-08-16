#include "proc.h"
#include "defs.h"
#include "mm.h"
#include "rand.h"
#include "printk.h"

extern void __dummy();
extern void __switch_to(struct task_struct* prev, struct task_struct* next);


struct task_struct* idle;           // idle process
struct task_struct* current;        // 指向当前运行线程的 `task_struct`
struct task_struct* task[NR_TASKS]; // 线程数组，所有的线程都保存在此

void task_init() {
    // 1. 调用 kalloc() 为 idle 分配一个物理页
    // 2. 设置 state 为 TASK_RUNNING;
    // 3. 由于 idle 不参与调度 可以将其 counter / priority 设置为 0
    // 4. 设置 idle 的 pid 为 0
    // 5. 将 current 和 task[0] 指向 idle
    
    //initialize idle thread
    task[0]=(struct task_struct *)kalloc();
    task[0]->thread_info=0;
    task[0]->state=TASK_RUNNING;
    task[0]->counter=0;
    task[0]->priority=0;
    task[0]->pid=0;
    
    idle=task[0];
    current=task[0];
    
    
    
    /* YOUR CODE HERE */

    // 1. 参考 idle 的设置, 为 task[1] ~ task[NR_TASKS - 1] 进行初始化
    // 2. 其中每个线程的 state 为 TASK_RUNNING, counter 为 0, priority 使用 rand() 来设置, pid 为该线程在线程数组中的下标。
    // 3. 为 task[1] ~ task[NR_TASKS - 1] 设置 `thread_struct` 中的 `ra` 和 `sp`,
    // 4. 其中 `ra` 设置为 __dummy （见 4.3.2）的地址， `sp` 设置为 该线程申请的物理页的高地址

    /* YOUR CODE HERE */
    
    for(int i=1;i<NR_TASKS;i++)
    {
    	task[i]=(struct task_struct *)kalloc();
    	task[i]->thread_info=0;
    	task[i]->state=TASK_RUNNING;
    	task[i]->counter=0;
    	task[i]->priority=rand();
    	task[i]->pid=i;
    	
    	task[i]->thread.ra=(uint64)__dummy;
    	task[i]->thread.sp=(uint64)task[i]+PGSIZE;
    	//printk("PID:%d PGADDR:%x SP:%x s11ADDR:%x\n",task[i]->pid,task[i],task[i]->thread.sp,&(task[i]->thread.s[11]));
    }
    
    printk("...proc_init done!\n");
}


void do_timer(void) {
    /* 1. 如果当前线程是 idle 线程 直接进行调度 */
    /* 2. 如果当前线程不是 idle 对当前线程的运行剩余时间减 1 
          若剩余时间任然大于0 则直接返回 否则进行调度 */
    if(current->pid==idle->pid){
        printk("idle process is running!\n");
    	schedule();
    }
    else{
    	current->counter--;
    	if(current->counter > 0)
    	    return;
    	else
    	    schedule();
    }
    
    /* YOUR CODE HERE */
}

void switch_to(struct task_struct* next) {
    /* YOUR CODE HERE */
    struct task_struct* prev=current;
    current=next;
    if(next->pid == prev->pid)
    	return;
    else{
	#ifdef SJF
    	printk("switch to [PID = %lu COUNTER = %lu]\n",next->pid,next->counter);
	#endif
	
	#ifdef PRIORITY
	printk("switch to [PID = %lu PRIORITY=%lu COUNTER = %lu]\n",next->pid,next->priority,next->counter);
	#endif

    	__switch_to(prev,next);
    }
    
    return;
}

void dummy() {
    uint64 MOD = 1000000007;
    uint64 auto_inc_local_var = 0;
    int last_counter = -1;
    while(1) {
        if (last_counter == -1 || current->counter != last_counter) {
            last_counter = current->counter;
            auto_inc_local_var = (auto_inc_local_var + 1) % MOD;
            printk("[PID = %d] is running. thread space begin at %llx\n", current->pid, (uint64)current);
        }
    }
}

#ifdef SJF
void schedule(void) {
    /* YOUR CODE HERE */
    uint64 min_rem=0xffffffffffffffff;
    int flag=0;
    while(flag==0)
    {
    	for(int i=NR_TASKS-1;i>=1;i--)
    	{	
    	    if(task[i]->counter < min_rem && task[i]->counter)
    	    {
    	    	min_rem=task[i]->counter;
    	    	flag=i;
    	    }
    	}
    	if(flag==0)
    	{
    	    for(int i=1;i<NR_TASKS;i++)
    	    {
    	    	task[i]->counter=rand();
    	    	printk("SET [ PID = %lu COUNTER = %lu]\n",task[i]->pid,task[i]->counter);
    	    }
    	}
    }
    struct task_struct* next;
    next=task[flag];
    
    switch_to(next);
}
#endif

#ifdef PRIORITY
void schedule(void) {
    struct task_struct* next=task[NR_TASKS-1];
    uint64 c=task[NR_TASKS-1]->counter;
    while(1)
    {
    	for(int i=NR_TASKS-1;i>=1;i--)
    	{
    	    if(task[i]->counter > c)
    	    {
    	    	next=task[i];
    	    	c=task[i]->counter;
    	    }
    	}
    	if(c) break;
    	for(int i=1;i<NR_TASKS;i++)
    	{
    	    task[i]->counter=(task[i]->counter >> 1) + task[i]->priority;
    	    printk("SET [ PID = %lu PRIORITY = %lu COUNTER = %lu]\n",task[i]->pid,task[i]->priority,task[i]->counter);
    	}
    }
    
    switch_to(next);
}
#endif
