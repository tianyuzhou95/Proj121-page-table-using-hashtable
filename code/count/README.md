为了在程序运行过程中统计访存次数，我们在原有QEMU代码的基础上增加了记录访存次数的功能。
## 内容索引
三个目录（4k-hash,16k,16k-hash）分别对应着支持4K哈希页表，支持16K多级页表，支持16K哈希页表的QEMU部分源文件。
## 使用说明
使用时，仅需在qemu源文件中替换掉原有的cpu_helper.c(`qemu/target/riscv/cpu_helper.c`)与cputlb.c( `qemu/accel/tcg/cputlb.c`)再重新编译即可。

如果需要关闭响应时间测试，可以选择取消宏[TEST_TIME](https://gitlab.eduxiji.net/facinating/project788067-115237/-/blob/master/code/count/4k-hash/cputlb.c#L46)的定义。