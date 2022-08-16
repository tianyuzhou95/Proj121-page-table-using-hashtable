all: printk.o rand.o string.o

printk.o:printk.c				#将print.c 编译成print.o
	${GCC} ${CFLAG} -c printk.c
rand.o:rand.c
	${GCC} ${CFLAG} -c rand.c
string.o:string.c
	${GCC} ${CFLAG} -c string.c
clean:						#清理生成的.o中间文件
	$(shell rm *.o 2>/dev/null)
