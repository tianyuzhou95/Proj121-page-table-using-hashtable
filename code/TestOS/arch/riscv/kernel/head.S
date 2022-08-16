.extern start_kernel				#向前申明start_kernel函数
.extern mm_init
.extern task_init
.extern setup_vm
.extern early_pgtbl
.extern setup_vm_final
.extern printk
    .section .text.init			#代码段
    .globl _start				#声明全局符号_start
_start:
    # YOUR CODE HERE
    la sp,boot_stack_top			#设置函数调用栈
    call setup_vm				#进行1G的初步映射
    call relocate				#重定位
    
    call mm_init				#初始化内存
    # call setup_vm_final			#建立三级页表
    
    call task_init				#初始化线程
    
    # ------------------
        # set stvec = _traps			#mode==0 DIRECT; mode==1 VECTOR
	la t0,_traps				#由于地址的低二位一定是00，所以mode默认就是0
	csrw stvec,t0				#设置stvec
    # ------------------
        # set sie[STIE] = 1			#在sie寄存器中，打开时钟中断的使能
	addi t0,x0,32
	csrs sie,t0
    # ------------------
        # set first time interrupt
	mv a7,x0				#写入function id
    	mv a6,x0				#写入extend id
    	
    	mv a0,x0				#直接将mtimecmp的值设为0，这将必然引发第一次时钟中断
    	mv a1,x0
    	mv a2,x0
    	mv a3,x0
    	mv a4,x0
    	mv a5,x0
	
    	ecall					#进入M模式，交给open_sbi的中断服务处理	
    	
    # ------------------
        # set sstatus[SIE] = 1
	csrsi sstatus,2			#将sstatus的SIE域置1
    # ------------------
    # ------------------
    # - your lab1 code -
	
	call t0,start_kernel			#调用start_kernel（函数调用栈的设置移到了最开始）
    # ------------------

relocate:
    
    li t0,0xffffffe000000000			
    li t1,0x0000000080000000
    sub t0,t0,t1				#虚拟地址与物理地址的差值
    
    add sp,sp,t0				#sp重定位
    add ra,ra,t0				#ra重定位
 
 #set stvec to change pc, while 
    la t1,change_pc
    add t1,t1,t0
    csrw stvec,t1
    
    
    la t1,early_pgtbl				#将一级映射页表装入satp
    srli t1,t1,14
    addi t2,zero,9				#Sv39分页方案
    slli t2,t2,60
    or t1,t1,t2
    
    sfence.vma zero,zero
    csrw satp,t1
#Trap into S-mode here, then pc = stvec = change_pc
change_pc:
    sfence.vma zero,zero			#清除TLB
    
    addi sp,sp,-16
    sd ra,0(sp)				#ra 属于caller saved 寄存器，调用前保存
    la a0,string1
    csrr a1,scause
    call printk				#输出scause错误信息
    ld ra,0(sp)				#恢复ra
    addi sp,sp,16
    ret					# pc = ra，完成pc重定位
.balign 2
string1:
    .string "PC changed!\nThe value of scause is:%llx\n"


    .section .bss.stack
    .globl boot_stack				#栈地址
boot_stack:
    .space 4096				#设置栈的大小为4096
    .globl boot_stack_top
boot_stack_top:				#声明栈顶符号
