.extern trap_handler
.extern dummy
    .section .text.entry
    .align 2
    .globl _traps 
    .globl __dummy
    .globl __switch_to

__dummy:
    la t0, dummy
    csrw sepc,t0
    sret
    
__switch_to:         #a0 current a1 next
    sd ra,40(a0)
    sd sp,48(a0)
    sd s0,56(a0)
    sd s1,64(a0)
    sd s2,72(a0)
    sd s3,80(a0)
    sd s4,88(a0)
    sd s5,96(a0)
    sd s6,104(a0)
    sd s7,112(a0)
    sd s8,120(a0)
    sd s9,128(a0)
    sd s10,136(a0)
    sd s11,144(a0)

    ld ra,40(a1)
    ld sp,48(a1)
    ld s0,56(a1)
    ld s1,64(a1)
    ld s2,72(a1)
    ld s3,80(a1)
    ld s4,88(a1)
    ld s5,96(a1)
    ld s6,104(a1)
    ld s7,112(a1)
    ld s8,120(a1)
    ld s9,128(a1)
    ld s10,136(a1)
    ld s11,144(a1)
 
    ret
     
    
_traps:
    # YOUR CODE HERE
    # -----------

        # 1. save 32 registers and sepc to stack
    addi sp,sp,-256	#将30个寄存器压栈（除去x0和x2(sp)）
    sd x1,0(sp)
    sd x3,16(sp)
    sd x4,24(sp)
    sd x5,32(sp)
    sd x6,40(sp)
    sd x7,48(sp)

    sd x10,72(sp)
    sd x11,80(sp)
    sd x12,88(sp)
    sd x13,96(sp)
    sd x14,104(sp)
    sd x15,112(sp)
    sd x16,120(sp)
    sd x17,128(sp)

    sd x28,216(sp)
    sd x29,224(sp)
    sd x30,232(sp)
    sd x31,240(sp)
    
    csrr t0,sepc		#取出sepc的值
    sd t0,248(sp)		#并压栈
    # -----------

        # 2. call trap_handler
    csrr a0,scause	#给trap_handler传入第一个参数
    csrr a1,sepc	#给trap_handler传入第二个参数
    call trap_handler   #根据函数调用规范，参数较少时，参数将直接传入a0-a7寄存器
    
    
    # -----------

        # 3. restore sepc and 32 registers (x2(sp) should be restore last) from stack
    ld t0,248(sp)	#取出sepc的值
    csrw sepc,t0	#恢复sepc的值
    
    ld x1,0(sp)	#恢复30个寄存器的值
    
    ld x4,24(sp)
    ld x5,32(sp)
    ld x6,40(sp)
    ld x7,48(sp)

    ld x10,72(sp)
    ld x11,80(sp)
    ld x12,88(sp)
    ld x13,96(sp)
    ld x14,104(sp)
    ld x15,112(sp)
    ld x16,120(sp)
    ld x17,128(sp)

    ld x28,216(sp)
    ld x29,224(sp)
    ld x30,232(sp)
    ld x31,240(sp)
    
    
        
        
    addi sp,sp,256		#恢复sp指针，完成出栈

    # -----------

        # 4. return from trap
    sret			#使用sret进行中断返回
    # -----------
