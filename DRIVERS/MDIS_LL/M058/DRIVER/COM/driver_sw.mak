#***************************  M a k e f i l e  *******************************
#
#         Author: ds
#
#    Description: Makefile definitions for the M58 driver
#
#-----------------------------------------------------------------------------
#   Copyright 2000-2019, MEN Mikro Elektronik GmbH
#*****************************************************************************
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

MAK_NAME=m58_sw
# the next line is updated during the MDIS installation
STAMPED_REVISION="13M058-06_02_03-1-g70c719c-dirty_2019-05-10"

DEF_REVISION=MAK_REVISION=$(STAMPED_REVISION)

MAK_SWITCH=$(SW_PREFIX)MAC_MEM_MAPPED \
		$(SW_PREFIX)$(DEF_REVISION) \
		   $(SW_PREFIX)MAC_BYTESWAP \
		   $(SW_PREFIX)M58_VARIANT=M58_SW \
		   $(SW_PREFIX)ID_SW

MAK_LIBS=$(LIB_PREFIX)$(MEN_LIB_DIR)/desc$(LIB_SUFFIX) \
         $(LIB_PREFIX)$(MEN_LIB_DIR)/mbuf$(LIB_SUFFIX) \
         $(LIB_PREFIX)$(MEN_LIB_DIR)/oss$(LIB_SUFFIX)  \
         $(LIB_PREFIX)$(MEN_LIB_DIR)/id_sw$(LIB_SUFFIX)   \
         $(LIB_PREFIX)$(MEN_LIB_DIR)/dbg$(LIB_SUFFIX) 


MAK_INCL=$(MEN_INC_DIR)/m58_drv.h     \
         $(MEN_INC_DIR)/men_typs.h    \
         $(MEN_INC_DIR)/oss.h         \
         $(MEN_INC_DIR)/mdis_err.h    \
         $(MEN_INC_DIR)/mbuf.h        \
         $(MEN_INC_DIR)/maccess.h     \
         $(MEN_INC_DIR)/desc.h        \
         $(MEN_INC_DIR)/mdis_api.h    \
         $(MEN_INC_DIR)/mdis_com.h    \
         $(MEN_INC_DIR)/modcom.h      \
         $(MEN_INC_DIR)/ll_defs.h     \
         $(MEN_INC_DIR)/ll_entry.h    \
         $(MEN_INC_DIR)/dbg.h

MAK_INP1=m58_drv$(INP_SUFFIX)

MAK_INP=$(MAK_INP1)
