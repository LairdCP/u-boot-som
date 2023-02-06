#
#
# SPDX-License-Identifier:	GPL-2.0+
#

obj-y += som60.o
ifdef CONFIG_SYS_SPL_MENU
obj-y += spl_menu.o
endif
UBOOTINCLUDE += -Iboard/laird/som60

ifndef CONFIG_SPL_BUILD
obj-$(CONFIG_TARGET_IG60) += ig60.o
obj-$(CONFIG_ID_EEPROM) += sys_eeprom.o
endif
