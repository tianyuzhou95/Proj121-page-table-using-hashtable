# 一、交叉编译工具

> 参考网站：[riscv交叉编译](https://github.com/riscv-collab/riscv-gnu-toolchain)

### 获取源码

```shell
git clone https://gitee.com/riscv-mcu/riscv-gnu-toolchain.git
```

### 依赖安装

构建工具链需要若干标准包。
在 Ubuntu 上，只执行以下命令：

```shell
apt-get install autoconf automake autotools-dev curl python3 libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev libexpat-dev
```



### 安装 (Newlib)

`--prefix`指定安装路径。

```shell
./configure --prefix=/opt/riscv 
make -j64 
```



### 安装 (Linux)

`--prefix`指定安装路径。

```shell
./configure --prefix=/opt/riscv
make linux -j64
```

这里我们选用的是`GNU`的交叉编译链：`make linux -j64`



# 二、KVM RISCV64 on QEMU

## 1. Build QEMU with RISC-V Hypervisor Extension Emulation

#### 若缺失依赖，安装对应依赖

```
apt-get install pkg-config,ninja-build,libglib2.0-dev  binutils-dev libboost-all-dev  libssl-dev libpixman-1-dev 
```


#### 安装Qemu

> ps:支持16K页粒度页表的qemu源码位于code/16k/qemu下，之后所有的操作建议在code/16k目录下完成

```shell
cd qemu
git submodule init
git submodule update --recursive
./configure --target-list="riscv64-softmmu"
make -j64
cd ..
```



## 2. Build OpenSBI Firmware

```shell
git clone https://github.com/riscv/opensbi.git
cd opensbi
export CROSS_COMPILE=riscv64-unknown-linux-gnu-
make PLATFORM=generic -j64
cd ..
```

## 3. Build Common Host & Guest Linux Kernel Image

我们可以使用与 Guest 和 Host 内核相同的 RISC-V 64 位 Linux 内核，因此无需单独编译它们。

> ps:支持16K页粒度页表的linux源码位于code/16k/linux下，之后所有的操作建议在code/16k目录下完成

```shell
export ARCH=riscv
export CROSS_COMPILE=riscv64-unknown-linux-gnu-
mkdir build-riscv64
make -C linux O=`pwd`/build-riscv64 defconfig
make -C linux O=`pwd`/build-riscv64 -j64
```

以上命令执行后会生成:

1. `build-riscv64/arch/riscv/boot/Image` 作为我们 Guest 和 Host 的 kernel
2. `build-riscv64/arch/riscv/kvm/kvm.ko` 作为我们的 KVM RISC-V module

## 4. Add libfdt library to CROSS_COMPILE SYSROOT directory

我们需要交叉编译工具链中的 libfdt 库来编译 KVMTOOL RISC-V（在下一步中描述）。

libfdt 库在交叉编译工具链中一般不可用，因此我们需要从 DTC 项目中显式编译 libfdt 并将其添加到 CROSS_COMPILE SYSROOT 目录中。

```shell
git clone git://git.kernel.org/pub/scm/utils/dtc/dtc.git
cd dtc
export ARCH=riscv
export CROSS_COMPILE=riscv64-unknown-linux-gnu-
export CC="${CROSS_COMPILE}gcc -mabi=lp64d -march=rv64gc"
TRIPLET=$($CC -dumpmachine)
SYSROOT=$($CC -print-sysroot)
make libfdt
make NO_PYTHON=1 NO_YAML=1 DESTDIR=$SYSROOT PREFIX=/usr LIBDIR=/usr/lib64/lp64d install-lib install-includes
cd ..
```

## 5. Build KVMTOOL

### 克隆源码
```shell
git clone https://git.kernel.org/pub/scm/linux/kernel/git/will/kvmtool.git
```
### 修改KVMTOOL使其支持16K页大小

* 修改kvm-arch.h中的宏定义
```makefile
-- kvmtool/riscv/include/kvm/kvm-arch.h
#define MAX_PAGE_SIZE		SZ_16K
```

* 修改Makefile，增加编译选项
```makefile
-- kvmtool/Makefile
CC	:= $(CROSS_COMPILE)gcc  
CFLAGS	:= -Wl,-z,max-page-size=16384
```

* 编译kvmtool
```shell
export ARCH=riscv
export CROSS_COMPILE=riscv64-unknown-linux-gnu-
cd kvmtool
make clean
make lkvm-static -j16
${CROSS_COMPILE}strip lkvm-static
cd ..
```

## 6. Build Host RootFS containing KVM RISC-V module, KVMTOOL and Guest Linux

#### busybox

* 从github仓库中获取源码
```shell
export ARCH=riscv
export CROSS_COMPILE=riscv64-unknown-linux-gnu-
git clone https://github.com/kvm-riscv/howto.git
wget https://busybox.net/downloads/busybox-1.33.1.tar.bz2
tar -C . -xvf ./busybox-1.33.1.tar.bz2
mv ./busybox-1.33.1 ./busybox-1.33.1-kvm-riscv64
```

* 编译busybox
```sh
cp -f ./howto/configs/busybox-1.33.1_defconfig busybox-1.33.1-kvm-riscv64/.config
make -C busybox-1.33.1-kvm-riscv64 oldconfig
make -C busybox-1.33.1-kvm-riscv64 install -j64 CFLAGS=-Wl,-z,max-page-size=16384
mkdir -p busybox-1.33.1-kvm-riscv64/_install/etc/init.d
mkdir -p busybox-1.33.1-kvm-riscv64/_install/dev
mkdir -p busybox-1.33.1-kvm-riscv64/_install/proc
mkdir -p busybox-1.33.1-kvm-riscv64/_install/sys
mkdir -p busybox-1.33.1-kvm-riscv64/_install/apps
ln -sf /sbin/init busybox-1.33.1-kvm-riscv64/_install/init
cp -f ./howto/configs/busybox/fstab busybox-1.33.1-kvm-riscv64/_install/etc/fstab
cp -f ./howto/configs/busybox/rcS busybox-1.33.1-kvm-riscv64/_install/etc/init.d/rcS
cp -f ./howto/configs/busybox/motd busybox-1.33.1-kvm-riscv64/_install/etc/motd
```

* 为Guest OS 提供根文件系统
```sh
cd busybox-1.33.1-kvm-riscv64
cp -r _install _install_guest
mv _install _install_host
mv _install_guest _install 
cd ..
cd busybox-1.33.1-kvm-riscv64/_install; find ./ | cpio -o -H newc > ../../guest.img; cd -
```


* 为Host OS 提供根文件系统
```sh
cd busybox-1.33.1-kvm-riscv64/
mv _install _install_guest
mv _install_host _install 
cd ..
cp -f guest.img busybox-1.33.1-kvm-riscv64/_install/apps
cd busybox-1.33.1-kvm-riscv64/_install; find ./ | cpio -o -H newc > ../../rootfs_kvm_riscv64.img; cd -
cp -f ./kvmtool/lkvm-static busybox-1.33.1-kvm-riscv64/_install/apps
cp -f ./build-riscv64/arch/riscv/boot/Image busybox-1.33.1-kvm-riscv64/_install/apps
cp -f ./build-riscv64/arch/riscv/kvm/kvm.ko busybox-1.33.1-kvm-riscv64/_install/apps
cd busybox-1.33.1-kvm-riscv64/_install; find ./ | cpio -o -H newc > ../../rootfs_kvm_riscv64.img; cd -
```

上述命令将创建 `rootfs_kvm_riscv64.img`，这将是我们包含 KVMTOOL 和 Guest Linux 的主机 RootFS。

## 7. Run RISC-V KVM on QEMU

* 在 QEMU 上使用 Host RootFS 运行主机 Linux

```shell
./qemu/build/riscv64-softmmu/qemu-system-riscv64  -M virt -m 2G -nographic -bios opensbi/build/platform/generic/firmware/fw_jump.bin -kernel ./build-riscv64/arch/riscv/boot/Image -initrd ./rootfs_kvm_riscv64.img -append "root=/dev/ram rw console=ttyS0 earlycon=sbi"
```

* 启用 KVM RISC-V module

```shell
insmod apps/kvm.ko
```

* 启用 KVM RISC-V 模块后使用 KVMTOOL 运行 Guest Linux

```shell
insmod apps/kvm.ko
./apps/lkvm-static run -m 512 -c1 --console serial -i ./apps/guest.img  -p "console=ttyS0 earlycon=uart8250,mmio,0x3f8" -k ./apps/Image --debug
```

## 8、启动信息

* Host OS
```sh
OpenSBI v1.0-81-gcb8271c
   ____                    _____ ____ _____
  / __ \                  / ____|  _ \_   _|
 | |  | |_ __   ___ _ __ | (___ | |_) || |
 | |  | | '_ \ / _ \ '_ \ \___ \|  _ < | |
 | |__| | |_) |  __/ | | |____) | |_) || |_
  \____/| .__/ \___|_| |_|_____/|____/_____|
        | |
        |_|

Platform Name             : riscv-virtio,qemu
Platform Features         : medeleg
Platform HART Count       : 1
Platform IPI Device       : aclint-mswi
Platform Timer Device     : aclint-mtimer @ 10000000Hz
Platform Console Device   : uart8250
Platform HSM Device       : ---
Platform Reboot Device    : sifive_test
Platform Shutdown Device  : sifive_test
Firmware Base             : 0x80000000
Firmware Size             : 284 KB
Runtime SBI Version       : 0.3

Domain0 Name              : root
Domain0 Boot HART         : 0
Domain0 HARTs             : 0*
Domain0 Region00          : 0x0000000002000000-0x000000000200ffff (I)
Domain0 Region01          : 0x0000000080000000-0x000000008007ffff ()
Domain0 Region02          : 0x0000000000000000-0xffffffffffffffff (R,W,X)
Domain0 Next Address      : 0x0000000080200000
Domain0 Next Arg1         : 0x0000000082200000
Domain0 Next Mode         : S-mode
Domain0 SysReset          : yes

Boot HART ID              : 0
Boot HART Domain          : root
Boot HART Priv Version    : v1.10
Boot HART Base ISA        : rv64imafdch
Boot HART ISA Extensions  : time
Boot HART PMP Count       : 16
Boot HART PMP Granularity : 4
Boot HART PMP Address Bits: 54
Boot HART MHPM Count      : 0
Boot HART MIDELEG         : 0x0000000000001666
Boot HART MEDELEG         : 0x0000000000f0b509
[    0.000000] Linux version 5.17.0 (root@02f0386970a2) (riscv64-unknown-linux-gnu-gcc (g5964b5cd727) 11.1.0, GNU ld (GNU Binutils) 2.38) #1 SMP Sun Jun 5 17:26:53 CST 2022
[    0.000000] OF: fdt: Ignoring memory range 0x80000000 - 0x80200000
[    0.000000] Machine model: riscv-virtio,qemu
[    0.000000] efi: UEFI not found.
[    0.000000] Zone ranges:
[    0.000000]   DMA32    [mem 0x0000000080200000-0x00000000ffffffff]
[    0.000000]   Normal   empty
[    0.000000] Movable zone start for each node
[    0.000000] Early memory node ranges
[    0.000000]   node   0: [mem 0x0000000080200000-0x00000000ffffffff]
[    0.000000] Initmem setup node 0 [mem 0x0000000080200000-0x00000000ffffffff]
[    0.000000] On node 0, zone DMA32: 128 pages in unavailable ranges
[    0.000000] SBI specification v0.3 detected
[    0.000000] SBI implementation ID=0x1 Version=0x10000
[    0.000000] SBI TIME extension detected
[    0.000000] SBI IPI extension detected
[    0.000000] SBI RFENCE extension detected
[    0.000000] SBI SRST extension detected
[    0.000000] SBI HSM extension detected
[    0.000000] riscv: ISA extensions acdfhimsu
[    0.000000] riscv: ELF capabilities acdfim
[    0.000000] percpu: Embedded 5 pages/cpu s30824 r8192 d42904 u81920
[    0.000000] Built 1 zonelists, mobility grouping on.  Total pages: 130496
[    0.000000] Kernel command line: root=/dev/ram rw console=ttyS0 earlycon=sbi
[    0.000000] Dentry cache hash table entries: 262144 (order: 7, 2097152 bytes, linear)
[    0.000000] Inode-cache hash table entries: 131072 (order: 6, 1048576 bytes, linear)
[    0.000000] mem auto-init: stack:off, heap alloc:off, heap free:off
[    0.000000] Virtual kernel memory layout:
[    0.000000]       fixmap : 0xfe5fff7ffe000000 - 0xfe5fff8000000000   (32768 kB)
[    0.000000]       pci io : 0xfe5fff8000000000 - 0xfe5fff8001000000   (  16 MB)
[    0.000000]      vmemmap : 0xfe5fff8001000000 - 0xfe60000001000000   (524288 MB)
[    0.000000]      vmalloc : 0xfe60000001000000 - 0xfee0000001000000   (34359738368 MB)
[    0.000000]       lowmem : 0xfee0000001000000 - 0xfee0000080e00000   (2046 MB)
[    0.000000]       kernel : 0xffffffff80200000 - 0xffffffffffffffff   (2045 MB)
[    0.000000] Memory: 1891296K/2095104K available (6132K kernel code, 33517K rwdata, 16384K rodata, 16574K init, 448K bss, 203808K reserved, 0K cma-reserved)
[    0.000000] SLUB: HWalign=64, Order=0-3, MinObjects=0, CPUs=1, Nodes=1
[    0.000000] rcu: Hierarchical RCU implementation.
[    0.000000] rcu:     RCU restricting CPUs from NR_CPUS=8 to nr_cpu_ids=1.
[    0.000000] rcu:     RCU debug extended QS entry/exit.
[    0.000000]  Tracing variant of Tasks RCU enabled.
[    0.000000] rcu: RCU calculated value of scheduler-enlistment delay is 25 jiffies.
[    0.000000] rcu: Adjusting geometry for rcu_fanout_leaf=16, nr_cpu_ids=1
[    0.000000] NR_IRQS: 64, nr_irqs: 64, preallocated irqs: 0
[    0.000000] riscv-intc: 64 local interrupts mapped
[    0.000000] plic: plic@c000000: mapped 53 interrupts with 1 handlers for 2 contexts.
[    0.000000] random: get_random_bytes called from start_kernel+0x4c0/0x6ec with crng_init=0
[    0.000000] riscv_timer_init_dt: Registering clocksource cpuid [0] hartid [0]
[    0.000000] clocksource: riscv_clocksource: mask: 0xffffffffffffffff max_cycles: 0x24e6a1710, max_idle_ns: 440795202120 ns
[    0.000255] sched_clock: 64 bits at 10MHz, resolution 100ns, wraps every 4398046511100ns
[    0.018197] Console: colour dummy device 80x25
[    0.023742] Calibrating delay loop (skipped), value calculated using timer frequency.. 20.00 BogoMIPS (lpj=40000)
[    0.024207] pid_max: default: 32768 minimum: 301
[    0.028497] Mount-cache hash table entries: 4096 (order: 1, 32768 bytes, linear)
[    0.028633] Mountpoint-cache hash table entries: 4096 (order: 1, 32768 bytes, linear)
[    0.126177] cblist_init_generic: Setting adjustable number of callback queues.
[    0.126671] cblist_init_generic: Setting shift to 0 and lim to 1.
[    0.128554] ASID allocator using 16 bits (65536 entries)
[    0.131723] rcu: Hierarchical SRCU implementation.
[    0.137100] EFI services will not be available.
[    0.144752] smp: Bringing up secondary CPUs ...
[    0.145039] smp: Brought up 1 node, 1 CPU
[    0.175559] devtmpfs: initialized
[    0.197563] clocksource: jiffies: mask: 0xffffffff max_cycles: 0xffffffff, max_idle_ns: 7645041785100000 ns
[    0.198095] futex hash table entries: 256 (order: 0, 16384 bytes, linear)
[    0.229021] NET: Registered PF_NETLINK/PF_ROUTE protocol family
[    0.340204] iommu: Default domain type: Translated
[    0.340355] iommu: DMA domain TLB invalidation policy: strict mode
[    0.349363] vgaarb: loaded
[    0.352848] SCSI subsystem initialized
[    0.357888] usbcore: registered new interface driver usbfs
[    0.358786] usbcore: registered new interface driver hub
[    0.359210] usbcore: registered new device driver usb
[    0.405182] clocksource: Switched to clocksource riscv_clocksource
[    0.452862] NET: Registered PF_INET protocol family
[    0.456914] IP idents hash table entries: 32768 (order: 4, 262144 bytes, linear)
[    0.471274] tcp_listen_portaddr_hash hash table entries: 1024 (order: 1, 40960 bytes, linear)
[    0.471724] TCP established hash table entries: 16384 (order: 3, 131072 bytes, linear)
[    0.472729] TCP bind hash table entries: 16384 (order: 5, 524288 bytes, linear)
[    0.475777] TCP: Hash tables configured (established 16384 bind 16384)
[    0.479088] UDP hash table entries: 1024 (order: 2, 98304 bytes, linear)
[    0.480790] UDP-Lite hash table entries: 1024 (order: 2, 98304 bytes, linear)
[    0.485015] NET: Registered PF_UNIX/PF_LOCAL protocol family
[    0.498188] RPC: Registered named UNIX socket transport module.
[    0.498377] RPC: Registered udp transport module.
[    0.498431] RPC: Registered tcp transport module.
[    0.498482] RPC: Registered tcp NFSv4.1 backchannel transport module.
[    0.498868] PCI: CLS 0 bytes, default 64
[    0.515543] Unpacking initramfs...
[    0.519240] workingset: timestamp_bits=62 max_order=17 bucket_order=0
[    0.603864] NFS: Registering the id_resolver key type
[    0.606891] Key type id_resolver registered
[    0.607030] Key type id_legacy registered
[    0.608357] nfs4filelayout_init: NFSv4 File Layout Driver Registering...
[    0.608660] nfs4flexfilelayout_init: NFSv4 Flexfile Layout Driver Registering...
[    0.620592] 9p: Installing v9fs 9p2000 file system support
[    0.624999] NET: Registered PF_ALG protocol family
[    0.626316] Block layer SCSI generic (bsg) driver version 0.4 loaded (major 251)
[    0.626738] io scheduler mq-deadline registered
[    0.626998] io scheduler kyber registered
[    0.668681] pci-host-generic 30000000.pci: host bridge /soc/pci@30000000 ranges:
[    0.671261] pci-host-generic 30000000.pci:       IO 0x0003000000..0x000300ffff -> 0x0000000000
[    0.672835] pci-host-generic 30000000.pci:      MEM 0x0040000000..0x007fffffff -> 0x0040000000
[    0.673054] pci-host-generic 30000000.pci:      MEM 0x0400000000..0x07ffffffff -> 0x0400000000
[    0.674717] pci-host-generic 30000000.pci: Memory resource size exceeds max for 32 bits
[    0.676545] pci-host-generic 30000000.pci: ECAM at [mem 0x30000000-0x3fffffff] for [bus 00-ff]
[    0.680264] pci-host-generic 30000000.pci: PCI host bridge to bus 0000:00
[    0.681042] pci_bus 0000:00: root bus resource [bus 00-ff]
[    0.681311] pci_bus 0000:00: root bus resource [io  0x0000-0xffff]
[    0.681517] pci_bus 0000:00: root bus resource [mem 0x40000000-0x7fffffff]
[    0.681574] pci_bus 0000:00: root bus resource [mem 0x400000000-0x7ffffffff]
[    0.685497] pci 0000:00:00.0: [1b36:0008] type 00 class 0x060000
[    1.081785] Serial: 8250/16550 driver, 4 ports, IRQ sharing disabled
[    1.116486] printk: console [ttyS0] disabled
[    1.121940] 10000000.uart: ttyS0 at MMIO 0x10000000 (irq = 2, base_baud = 230400) is a 16550A
[    1.311521] printk: console [ttyS0] enabled
[    1.433075] loop: module loaded
[    1.446682] e1000e: Intel(R) PRO/1000 Network Driver
[    1.447352] e1000e: Copyright(c) 1999 - 2015 Intel Corporation.
[    1.449019] ehci_hcd: USB 2.0 'Enhanced' Host Controller (EHCI) Driver
[    1.450099] ehci-pci: EHCI PCI platform driver
[    1.451210] ehci-platform: EHCI generic platform driver
[    1.452421] ohci_hcd: USB 1.1 'Open' Host Controller (OHCI) Driver
[    1.453330] ohci-pci: OHCI PCI platform driver
[    1.454355] ohci-platform: OHCI generic platform driver
[    1.485029] usbcore: registered new interface driver uas
[    1.486223] usbcore: registered new interface driver usb-storage
[    1.496765] mousedev: PS/2 mouse device common for all mice
[    1.512972] goldfish_rtc 101000.rtc: registered as rtc0
[    1.515355] goldfish_rtc 101000.rtc: setting system clock to 2022-06-05T11:36:57 UTC (1654429017)
[    1.523833] syscon-poweroff soc:poweroff: pm_power_off already claimed for sbi_srst_power_off
[    1.525751] syscon-poweroff: probe of soc:poweroff failed with error -16
[    1.528005] sdhci: Secure Digital Host Controller Interface driver
[    1.528970] sdhci: Copyright(c) Pierre Ossman
[    1.530056] sdhci-pltfm: SDHCI platform and OF driver helper
[    1.533213] usbcore: registered new interface driver usbhid
[    1.533958] usbhid: USB HID core driver
[    1.539504] NET: Registered PF_INET6 protocol family
[    1.662889] Freeing initrd memory: 96080K
[    1.679318] Segment Routing with IPv6
[    1.680405] In-situ OAM (IOAM) with IPv6
[    1.681978] sit: IPv6, IPv4 and MPLS over IPv4 tunneling driver
[    1.690553] NET: Registered PF_PACKET protocol family
[    1.694518] 9pnet: Installing 9P2000 support
[    1.695782] Key type dns_resolver registered
[    1.701546] debug_vm_pgtable: [debug_vm_pgtable         ]: Validating architecture page table helpers
[    1.986572] Freeing unused kernel image (initmem) memory: 16560K
[    1.989660] Run /init as init process
           _  _
          | ||_|
          | | _ ____  _   _  _  _
          | || |  _ \| | | |\ \/ /
          | || | | | | |_| |/    \
          |_||_|_| |_|\____|\_/\_/

               Busybox Rootfs
```

* Guest OS
```shell
/ # ls
apps     dev      init     proc     sbin     usr
bin      etc      linuxrc  root     sys
/ # insmod apps/kvm.ko
[   16.743514] kvm [43]: hypervisor extension available
[   16.745017] kvm [43]: using Sv48x4 G-stage page table format
[   16.745759] kvm [43]: VMID 14 bits available
/ # insmod apps/kvm.ko
insmod: can't insert 'apps/kvm.ko': File exists
/ # ./apps/lkvm-static run -m 512 -c1 --[   23.284021] random: fast init done
console serial -i ./apps/guest.img  -p "
console=ttyS0 earlycon=uart8250,mmio,0x3f8" -k ./apps/Image --debug
  # lkvm run -k ./apps/Image -m 512 -c 1 --name guest-45
  Info: (riscv/kvm.c) kvm__arch_load_kernel_image:115: Loaded kernel to 0x80200000 (82752000 bytes)
  Info: (riscv/kvm.c) kvm__arch_load_kernel_image:126: Placing fdt at 0x85800000 - 0x8fffffff
  Info: (riscv/kvm.c) kvm__arch_load_kernel_image:155: Loaded initrd to 0x8fe283f8 (1932288 bytes)
  Info: (virtio/mmio.c) virtio_mmio_init:349: virtio-mmio.devices=0x200@0x10000000:5
[    0.000000] Linux version 5.17.0 (root@02f0386970a2) (riscv64-unknown-linux-gnu-gcc (g5964b5cd727) 11.1.0, GNU ld (GNU Binutils) 2.38) #1 SMP Sun Jun 5 17:26:53 CST 2022
[    0.000000] OF: fdt: Ignoring memory range 0x80000000 - 0x80200000
[    0.000000] Machine model: linux,dummy-virt
[    0.000000] earlycon: uart8250 at MMIO 0x00000000000003f8 (options '')
[    0.000000] printk: bootconsole [uart8250] enabled
[    0.000000] efi: UEFI not found.
[    0.000000] Zone ranges:
[    0.000000]   DMA32    [mem 0x0000000080200000-0x000000009fffffff]
[    0.000000]   Normal   empty
[    0.000000] Movable zone start for each node
[    0.000000] Early memory node ranges
[    0.000000]   node   0: [mem 0x0000000080200000-0x000000009fffffff]
[    0.000000] Initmem setup node 0 [mem 0x0000000080200000-0x000000009fffffff]
[    0.000000] On node 0, zone DMA32: 128 pages in unavailable ranges
[    0.000000] SBI specification v0.2 detected
[    0.000000] SBI implementation ID=0x3 Version=0x51100
[    0.000000] SBI TIME extension detected
[    0.000000] SBI IPI extension detected
[    0.000000] SBI RFENCE extension detected
[    0.000000] SBI HSM extension detected
[    0.000000] riscv: ISA extensions acdfimsu
[    0.000000] riscv: ELF capabilities acdfim
[    0.000000] percpu: Embedded 5 pages/cpu s30824 r8192 d42904 u81920
[    0.000000] Built 1 zonelists, mobility grouping on.  Total pages: 32528
[    0.000000] Kernel command line:  console=ttyS0 root=/dev/vda rw  console=ttyS0 earlycon=uart8250,mmio,0x3f8
[    0.000000] Dentry cache hash table entries: 65536 (order: 5, 524288 bytes, linear)
[    0.000000] Inode-cache hash table entries: 32768 (order: 4, 262144 bytes, linear)
[    0.000000] mem auto-init: stack:off, heap alloc:off, heap free:off
[    0.000000] Virtual kernel memory layout:
[    0.000000]       fixmap : 0xfe5fff7ffe000000 - 0xfe5fff8000000000   (32768 kB)
[    0.000000]       pci io : 0xfe5fff8000000000 - 0xfe5fff8001000000   (  16 MB)
[    0.000000]      vmemmap : 0xfe5fff8001000000 - 0xfe60000001000000   (524288 MB)
[    0.000000]      vmalloc : 0xfe60000001000000 - 0xfee0000001000000   (34359738368 MB)
[    0.000000]       lowmem : 0xfee0000001000000 - 0xfee0000020e00000   ( 510 MB)
[    0.000000]       kernel : 0xffffffff80200000 - 0xffffffffffffffff   (2045 MB)
[    0.000000] Memory: 421088K/522240K available (6132K kernel code, 33517K rwdata, 16384K rodata, 16574K init, 448K bss, 101152K reserved, 0K cma-reserved)
[    0.000000] SLUB: HWalign=64, Order=0-3, MinObjects=0, CPUs=1, Nodes=1
[    0.000000] rcu: Hierarchical RCU implementation.
[    0.000000] rcu:     RCU restricting CPUs from NR_CPUS=8 to nr_cpu_ids=1.
[    0.000000] rcu:     RCU debug extended QS entry/exit.
[    0.000000]  Tracing variant of Tasks RCU enabled.
[    0.000000] rcu: RCU calculated value of scheduler-enlistment delay is 25 jiffies.
[    0.000000] rcu: Adjusting geometry for rcu_fanout_leaf=16, nr_cpu_ids=1
[    0.000000] NR_IRQS: 64, nr_irqs: 64, preallocated irqs: 0
[    0.000000] riscv-intc: 64 local interrupts mapped
[    0.000000] plic: interrupt-controller@0c000000: mapped 1023 interrupts with 1 handlers for 2 contexts.
[    0.000000] random: get_random_bytes called from start_kernel+0x4c0/0x6ec with crng_init=0
[    0.000000] riscv_timer_init_dt: Registering clocksource cpuid [0] hartid [0]
[    0.000000] clocksource: riscv_clocksource: mask: 0xffffffffffffffff max_cycles: 0x24e6a1710, max_idle_ns: 440795202120 ns
[    0.000280] sched_clock: 64 bits at 10MHz, resolution 100ns, wraps every 4398046511100ns
[    0.078872] Console: colour dummy device 80x25
[    0.119039] Calibrating delay loop (skipped), value calculated using timer frequency.. 20.00 BogoMIPS (lpj=40000)
[    0.196201] pid_max: default: 32768 minimum: 301
[    0.236652] Mount-cache hash table entries: 2048 (order: 0, 16384 bytes, linear)
[    0.292252] Mountpoint-cache hash table entries: 2048 (order: 0, 16384 bytes, linear)
[    0.497235] cblist_init_generic: Setting adjustable number of callback queues.
[    0.552025] cblist_init_generic: Setting shift to 0 and lim to 1.
[    0.602335] ASID allocator using 16 bits (65536 entries)
[    0.646830] rcu: Hierarchical SRCU implementation.
[    0.692612] EFI services will not be available.
[    0.741669] smp: Bringing up secondary CPUs ...
[    0.775120] smp: Brought up 1 node, 1 CPU
[    0.862539] devtmpfs: initialized
[    0.920596] clocksource: jiffies: mask: 0xffffffff max_cycles: 0xffffffff, max_idle_ns: 7645041785100000 ns
[    0.992908] futex hash table entries: 256 (order: 0, 16384 bytes, linear)
[    1.066541] NET: Registered PF_NETLINK/PF_ROUTE protocol family
[    1.716902] iommu: Default domain type: Translated
[    1.786590] iommu: DMA domain TLB invalidation policy: strict mode
[    1.850525] vgaarb: loaded
[    1.889629] SCSI subsystem initialized
[    1.931779] usbcore: registered new interface driver usbfs
[    1.973569] usbcore: registered new interface driver hub
[    2.013225] usbcore: registered new device driver usb
[    2.121869] clocksource: Switched to clocksource riscv_clocksource
[    2.284422] NET: Registered PF_INET protocol family
[    2.330292] IP idents hash table entries: 8192 (order: 2, 65536 bytes, linear)
[    2.405181] tcp_listen_portaddr_hash hash table entries: 512 (order: 0, 20480 bytes, linear)
[    2.468486] TCP established hash table entries: 4096 (order: 1, 32768 bytes, linear)
[    2.527445] TCP bind hash table entries: 4096 (order: 3, 131072 bytes, linear)
[    2.584022] TCP: Hash tables configured (established 4096 bind 4096)
[    2.638060] UDP hash table entries: 256 (order: 0, 24576 bytes, linear)
[    2.688679] UDP-Lite hash table entries: 256 (order: 0, 24576 bytes, linear)
[    2.748632] NET: Registered PF_UNIX/PF_LOCAL protocol family
[    2.836506] RPC: Registered named UNIX socket transport module.
[    2.880271] RPC: Registered udp transport module.
[    2.915479] RPC: Registered tcp transport module.
[    2.949840] RPC: Registered tcp NFSv4.1 backchannel transport module.
[    2.997692] PCI: CLS 0 bytes, default 64
[    3.079977] Unpacking initramfs...
[    3.300936] workingset: timestamp_bits=62 max_order=15 bucket_order=0
[    3.587970] NFS: Registering the id_resolver key type
[    3.629396] Key type id_r[   36.264676] random: crng init done
esolver registered
[    3.679714] Key type id_legacy registered
[    3.711636] nfs4filelayout_init: NFSv4 File Layout Driver Registering...
[    3.760987] nfs4flexfilelayout_init: NFSv4 Flexfile Layout Driver Registering...
[    4.007155] Freeing initrd memory: 1856K
[    4.037517] 9p: Installing v9fs 9p2000 file system support
[    4.086114] NET: Registered PF_ALG protocol family
[    4.124430] Block layer SCSI generic (bsg) driver version 0.4 loaded (major 251)
[    4.192230] io scheduler mq-deadline registered
[    4.225891] io scheduler kyber registered
[    4.307720] pci-host-generic 30000000.pci: host bridge /smb/pci ranges:
[    4.360743] pci-host-generic 30000000.pci:       IO 0x0000000000..0x000000ffff -> 0x0000000000
[    4.425823] pci-host-generic 30000000.pci:      MEM 0x0040000000..0x007fffffff -> 0x0040000000
[    4.496597] pci-host-generic 30000000.pci: ECAM at [mem 0x30000000-0x3fffffff] for [bus 00-01]
[    4.578122] pci-host-generic 30000000.pci: PCI host bridge to bus 0000:00
[    4.631573] pci_bus 0000:00: root bus resource [bus 00-01]
[    4.672322] pci_bus 0000:00: root bus resource [io  0x0000-0xffff]
[    4.717581] pci_bus 0000:00: root bus resource [mem 0x40000000-0x7fffffff]
[    5.244716] Serial: 8250/16550 driver, 4 ports, IRQ sharing disabled
[    5.324515] printk: console [ttyS0] disabled
[    5.363822] 3f8.U6_16550A: ttyS0 at MMIO 0x3f8 (irq = 2, base_baud = 115200) is a 16550A
[    5.432447] printk: console [ttyS0] enabled
[    5.432447] printk: console [ttyS0] enabled
[    5.495172] printk: bootconsole [uart8250] disabled
[    5.495172] printk: bootconsole [uart8250] disabled
[    5.573751] 2f8.U6_16550A: ttyS1 at MMIO 0x2f8 (irq = 3, base_baud = 115200) is a 16550A
[    5.640677] 3e8.U6_16550A: ttyS2 at MMIO 0x3e8 (irq = 4, base_baud = 115200) is a 16550A
[    5.705424] 2e8.U6_16550A: ttyS3 at MMIO 0x2e8 (irq = 6, base_baud = 115200) is a 16550A
[    5.985168] loop: module loaded
[    6.252851] net eth0: Fail to set guest offload.
[    6.288110] virtio_net virtio0 eth0: set_features() failed (-22); wanted 0x0000000000134829, left 0x0080000000134829
[    6.586081] e1000e: Intel(R) PRO/1000 Network Driver
[    6.623591] e1000e: Copyright(c) 1999 - 2015 Intel Corporation.
[    6.668273] ehci_hcd: USB 2.0 'Enhanced' Host Controller (EHCI) Driver
[    6.716385] ehci-pci: EHCI PCI platform driver
[    6.750270] ehci-platform: EHCI generic platform driver
[    6.789625] ohci_hcd: USB 1.1 'Open' Host Controller (OHCI) Driver
[    6.835825] ohci-pci: OHCI PCI platform driver
[    6.869678] ohci-platform: OHCI generic platform driver
[    6.916365] usbcore: registered new interface driver uas
[    6.956725] usbcore: registered new interface driver usb-storage
[    7.005395] mousedev: PS/2 mouse device common for all mice
[    7.051755] sdhci: Secure Digital Host Controller Interface driver
[    7.097032] sdhci: Copyright(c) Pierre Ossman
[    7.130312] sdhci-pltfm: SDHCI platform and OF driver helper
[    7.177400] usbcore: registered new interface driver usbhid
[    7.218990] usbhid: USB HID core driver
[    7.256303] NET: Registered PF_INET6 protocol family
[    7.331699] Segment Routing with IPv6
[    7.360274] In-situ OAM (IOAM) with IPv6
[    7.391629] sit: IPv6, IPv4 and MPLS over IPv4 tunneling driver
[    7.445234] NET: Registered PF_PACKET protocol family
[    7.488029] 9pnet: Installing 9P2000 support
[    7.520762] Key type dns_resolver registered
[    7.560824] debug_vm_pgtable: [debug_vm_pgtable         ]: Validating architecture page table helpers
[    8.213543] Freeing unused kernel image (initmem) memory: 16560K
[    8.261447] Run /init as init process
           _  _
          | ||_|
          | | _ ____  _   _  _  _
          | || |  _ \| | | |\ \/ /
          | || | | | | |_| |/    \
          |_||_|_| |_|\____|\_/\_/

               Busybox Rootfs

Please press Enter to activate this console.
```