/* mgmt memory type definitions
 * Copyright (C) 2021  Vmware, Inc.
 *		       Pushpasis Sarkar <spushpasis@vmware.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; see the file COPYING; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mgmt_memory.h"

/* this file is temporary in nature;  definitions should be moved to the
 * files they're used in
 */

DEFINE_MGROUP(MGMTD, "mgmt");
DEFINE_MTYPE(MGMTD, MGMTD, "MGMTD instance");
DEFINE_MTYPE(MGMTD, MGMTD_BCKND_ADPATER, "MGMTD backend adapter");
DEFINE_MTYPE(MGMTD, MGMTD_FRNTND_ADPATER, "MGMTD Frontend adapter");
DEFINE_MTYPE(MGMTD, MGMTD_FRNTND_SESSN, "MGMTD Frontend Client Session");
