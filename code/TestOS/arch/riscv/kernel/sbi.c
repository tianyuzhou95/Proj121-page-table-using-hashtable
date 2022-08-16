#include "types.h"
#include "sbi.h"


struct sbiret sbi_ecall(int ext, int fid, uint64 arg0,
			            uint64 arg1, uint64 arg2,
			            uint64 arg3, uint64 arg4,
			            uint64 arg5) 
{
    struct sbiret sbr;
    __asm__ volatile (			//内联汇编部分
    	"mv a7,%[ext]\n"		//将与ext绑定的寄存器传值给a7
    	"mv a6,%[fid]\n"		//将与fid绑定的寄存器传值给a6
    	"mv a5,%[arg5]\n"		//将与arg5绑定的寄存器传值给a5
    	"mv a4,%[arg4]\n"		//将与arg4绑定的寄存器传值给a4
    	"mv a3,%[arg3]\n"		//将与arg3绑定的寄存器传值给a3
    	"mv a2,%[arg2]\n"		//将与arg2绑定的寄存器传值给a2
    	"mv a1,%[arg1]\n"		//将与arg1绑定的寄存器传值给a1
    	"mv a0,%[arg0]\n"		//将与arg0绑定的寄存器传值给a0
    	"ecall\n"			//ecall进入sbi的异常处理服务
    	"mv %[err],a0\n"		//将a0传值给与err绑定的寄存器
    	"mv %[val],a1\n"		//将a1传值给与val绑定的寄存器
    	: [err]"=r"(sbr.error),[val]"=r"(sbr.value)		//将输出绑定到寄存器
    	: [ext]"r"(ext), [fid]"r"(fid), [arg0]"r"(arg0), [arg1]"r"(arg1), [arg2]"r"(arg2), [arg3]"r"(arg3), [arg4]"r"(arg4), [arg5]"r"(arg5)	//将输入绑定到寄存器
    	:"memory","a0","a1","a2","a3","a4","a5","a6","a7" 	//告知编译器会对内存以及相应的寄存器进行修改
    );
    return sbr;
}
