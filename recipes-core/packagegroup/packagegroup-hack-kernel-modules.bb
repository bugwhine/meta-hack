DESCRIPTION = "Custom package group for kernel modules"

inherit packagegroup

PACKAGES = "packagegroup-wdt"

RDEPENDS_packagegroup-wdt = "\
	kernel-module-itco-wdt \
	kernel-module-itco-vendor-support \
	kernel-module-wdat-wdt \
	kernel-module-lpc-ich \
	"
