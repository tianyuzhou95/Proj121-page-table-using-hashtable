// trap.c 
#include "printk.h"
#include "proc.h"

extern void clock_set_next_event();

void trap_handler(unsigned long scause, unsigned long sepc) {
    // 通过 `scause` 判断trap类型
    // 如果是interrupt 判断是否是timer interrupt
    // 如果是timer interrupt 则打印输出相关信息, 并通过 `clock_set_next_event()` 设置下一次时钟中断
    // `clock_set_next_event()` 见 4.5 节
    // 其他interrupt / exception 可以直接忽略
    if(scause!=0x8000000000000005) //因为是时钟中断，所以最高位为1；而时钟中断对应的Exception code 为5
    	return;
    //printk("[S] Supervisor Mode Timer Interrupt\n");
    //printk("student_id:3190102973\n"); //输出一些信息
    clock_set_next_event(); 	    //重新设置mtimecmp寄存器
    do_timer();
}


