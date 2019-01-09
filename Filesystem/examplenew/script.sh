rm -rf disk.img
dd if=/dev/zero of=disk.img bs=1M count=1024
rm -rf mnt
mkdir mnt
make
./objfs mnt -o use_ino
mount | grep objfs

