build:
	pmbootstrap --details-to-stdout checksum device-fyde-fydetab-duo
	pmbootstrap --details-to-stdout checksum u-boot-fydetab-duo
	pmbootstrap --details-to-stdout checksum linux-fydetab-duo
#	pmbootstrap --details-to-stdout build --arch aarch64 u-boot-fydetab-duo --force
	pmbootstrap --details-to-stdout build --arch aarch64 device-fyde-fydetab-duo --force
	pmbootstrap --details-to-stdout build --arch aarch64 linux-fydetab-duo --force
	yes | pmbootstrap zap
	pmbootstrap --details-to-stdout install --no-split --password 123123123
	pmbootstrap export
	# Flash device
	doas rkdeveloptool db /home/hugo/src/__bredos/rk3588_spl_loader_v1.15.113.bin
	doas rkdeveloptool wl 0x0 /tmp/postmarketOS-export/fyde-fydetab-duo.img
	doas rkdeveloptool reboot
