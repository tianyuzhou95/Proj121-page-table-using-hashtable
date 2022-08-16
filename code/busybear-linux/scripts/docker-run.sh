../qemu/build/riscv64-softmmu/qemu-system-riscv64 -nographic -machine virt -m 2G \
  -kernel ../build-riscv64/arch/riscv/boot/Image -append "root=/dev/vda ro console=ttyS0" \
  -drive file=busybear.bin,format=raw,id=hd0 \
  -device virtio-blk-device,drive=hd0 \
  -netdev type=tap,script=scripts/ifup.sh,downscript=scripts/ifdown.sh,id=net0 \
  -device virtio-net-device,netdev=net0
