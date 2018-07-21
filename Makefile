default :
	make img_cd

run :
	make img_cd
	qemu -k ja --std-vga -cdrom lp49-boot.cd	

runvga :
	make img_cd
	qemu -k ja -cdrom lp49-boot.cd	

p9 :
	qemu -k ja --std-vga -hda /home/hkawai/qemu/plan9.qcow.img &

img_cd :
	make -C src/cmd/aux/vga
	make -C src/cmd/rio install
	make -C src/9/pc
	make -C src/9/qsh
	make -C src/9/init
	mkisofs -R -b boot/grub/stage2_eltorito -no-emul-boot -boot-load-size 4 -boot-info-table -o lp49-boot.cd  rootfs

