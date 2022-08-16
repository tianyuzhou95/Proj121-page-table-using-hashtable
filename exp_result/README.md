本文档将描述如何交叉编译MBW,STREAM,LMbench和Redis-benchmark等测试集。

# MBW

**mbw** 通常用来评估用户层应用程序进行内存拷贝操作所能够达到的带宽。

Github主页：[https://github.com/raas/mbw/](https://github.com/raas/mbw/)

### 编译步骤

从Github主页上克隆源码后，执行下面的编译命令即可在riscv架构上运行MBW测试程序。

```shell
riscv64-unknown-linux-gnu-gcc -static -Wl,-z,max-page-size=16384 mbw.c -o mbw
```

# STREAM

**STREAM** 是用于测量持续内存带宽的行业标准基准。

官网：[STREAM Benchmark Reference Information (virginia.edu)](https://www.cs.virginia.edu/stream/ref.html)

### 编译步骤

从官网获取源码后，修改Makefile为：

```makefile
CC = riscv64-unknown-linux-gnu-gcc
CFLAGS = -O2 -fopenmp -DSTREAM_ARRAY_SIZE=10000000 -static -Wl,-z,max-page-size=16384

FC = gfortran
FFLAGS = -O2 -fopenmp

all: stream_c.exe

stream_c.exe: stream.c
	$(CC) $(CFLAGS) stream.c -o origin_10M

clean:
	rm -f stream_f.exe stream_c.exe *.o
```

使用`make`运行即可在riscv架构上运行STREAM测试程序。

# LMbench

**一、构建并安装libtirpc库**

首先, 在https://sourceforge.net/projects/libtirpc/下载libtirpc库的源代码, 并解压。然后, 进入libtirpc库源代码目录, 执行如下命令:

```
./configure --host=riscv64-unknown-linux-gnu --prefix=/opt/riscv/sysroot/usr --disable-gssapi
make
make install
```

这会将编译好的libtirpc库安装至`/opt/riscv`中。

**二、构建LMbench**

在https://gitlab.eduxiji.net/facinating/lmbench-riscv.git可以获得我修改过的LMbench。

下载上述仓库并进入, 然后执行`make`即可。编译好的LMbench位于`bin/riscv64-linux-gnu`目录中。

# Redis-benchmark

1. 从Redis中文官网下载源码：[redis 6.0.6 下载 -- Redis中国用户组（CRUG）](http://www.redis.cn/download.html)

2. 进入redis/src目录，使用以下的编译命令进行编译：

```makefile
make MALLOC=libc CC="riscv64-unknown-linux-gnu-gcc -Wl,-z,max-page-size=16384" CXX=riscv64-unknown-linux-gnu-g++ AR=riscv64-unknown-linux-gnu-ar RANLIB=riscv64-unknown-linux-gnu-ranlib NM=riscv64-unknown-linux-gnu-nm CFLAGS="-static" LDFLAGS=-static -j64 
```







