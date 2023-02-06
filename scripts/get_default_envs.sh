#! /bin/bash
# SPDX-License-Identifier: GPL-2.0+
#
# This file extracts default envs from built u-boot
# usage: get_default_envs.sh [build dir] > u-boot-env-default.txt
set -ue

: "${OBJCOPY:=${CROSS_COMPILE:-}objcopy}"
: "${OBJDUMP:=${CROSS_COMPILE:-}objdump}"

echoerr() { echo "$@" 1>&2; }

if [ "$#" -eq 1 ]; then
    path=${1}
else
    path=$(readlink -f $0)
fi

ENV_OBJ_FILE="${path}/u-boot"
ENV_OBJ_FILE_RODATA="${path}/rodata_u_boot"
ENV_OBJ_FILE_ENV="${path}/defaut_env_u_boot"

if [ ! -f ${ENV_OBJ_FILE} ]; then
    echoerr "File '${ENV_OBJ_FILE}' not found!"
    exit 1
fi

${OBJCOPY} --dump-section .rodata=${ENV_OBJ_FILE_RODATA} ${ENV_OBJ_FILE}
set -- $(${OBJDUMP} -x ${ENV_OBJ_FILE} |grep \.rodata$)
start=${1}

set -- $(${OBJDUMP} -x ${ENV_OBJ_FILE} |grep default_environment$)
stop=${1}
length=$((0x${5}))

count=$((0x${stop} - 0x${start}))

dd skip=${count} count=${length} if=${ENV_OBJ_FILE_RODATA} of=${ENV_OBJ_FILE_ENV} bs=1

# Replace default '\0' with '\n', remove blank lines and sort entries
tr '\0' '\n' < ${ENV_OBJ_FILE_ENV} | sed -e '/^\s*$/d' | sort --field-separator== -k1,1 --stable

rm -f ${ENV_OBJ_FILE_RODATA} ${ENV_OBJ_FILE_ENV}

exit 0
