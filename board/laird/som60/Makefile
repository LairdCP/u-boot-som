#
#
# SPDX-License-Identifier:	GPL-2.0+
#

obj-y += som60.o
obj-$(CONFIG_SYS_EEPROM_SETUP) += som60_eeprom.o
obj-$(CONFIG_SPL_SYS_MENU) += spl_menu.o

UBOOTINCLUDE += -Iboard/laird/som60

ifndef CONFIG_SPL_BUILD
obj-$(CONFIG_TARGET_IG60) += ig60.o
endif
