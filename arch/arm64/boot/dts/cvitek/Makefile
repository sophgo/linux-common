dtb-$(CONFIG_ARCH_CVITEK) += $(patsubst $(src)/%.dts,%.dtb,$(wildcard $(src)/*.dts))

always          := $(dtb-y)
subdir-y        := $(dts-dirs)
clean-files     := *.dtb
