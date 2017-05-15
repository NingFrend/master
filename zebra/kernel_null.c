/*
 * NULL kernel methods for testing. 
 * Copyright (C) 2006 Sun Microsystems, Inc.
 *
 * This file is part of Quagga.
 *
 * Quagga is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * Quagga is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; see the file COPYING; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <zebra.h>
#include <log.h>

#include "vty.h"
#include "vxlan.h"
#include "zebra/zserv.h"
#include "zebra/rt.h"
#include "zebra/redistribute.h"
#include "zebra/connected.h"
#include "zebra/rt_netlink.h"
#include "zebra/rib.h"

int kernel_route_rib (struct prefix *a, struct prefix *b,
                      struct rib *old, struct rib *new) { return 0; }

int kernel_address_add_ipv4 (struct interface *a, struct connected *b)
{
  zlog_debug ("%s", __func__);
  SET_FLAG (b->conf, ZEBRA_IFC_REAL);
  connected_add_ipv4 (a, 0, &b->address->u.prefix4, b->address->prefixlen, 
                      (b->destination ? &b->destination->u.prefix4 : NULL), 
                      NULL);
  return 0;
}

int kernel_address_delete_ipv4 (struct interface *a, struct connected *b)
{
  zlog_debug ("%s", __func__);
  connected_delete_ipv4 (a, 0, &b->address->u.prefix4, b->address->prefixlen, 
                         (b->destination ? &b->destination->u.prefix4 : NULL));
  return 0;
}

int kernel_neigh_update (int a, int b, uint32_t c, char *d, int e)
{
  return 0;
}

void kernel_init (struct zebra_ns *zns) { return; }
void kernel_terminate (struct zebra_ns *zns) { return; }
void route_read (struct zebra_ns *zns) { return; }

int kernel_get_ipmr_sg_stats (void *m) { return 0; }

int
kernel_add_vtep (vni_t vni, struct interface *ifp, struct in_addr *vtep_ip)
{
  return 0;
}

int
kernel_del_vtep (vni_t vni, struct interface *ifp, struct in_addr *vtep_ip)
{
  return 0;
}

int
kernel_add_mac (struct interface *ifp, vlanid_t vid,
                struct ethaddr *mac, struct in_addr vtep_ip)
{
  return 0;
}

int
kernel_del_mac (struct interface *ifp, vlanid_t vid,
                struct ethaddr *mac, struct in_addr vtep_ip, int local)
{
  return 0;
}

void macfdb_read (struct zebra_ns *zns)
{
}

void macfdb_read_for_bridge (struct zebra_ns *zns, struct interface *ifp,
                             struct interface *br_if)
{
}

void neigh_read (struct zebra_ns *zns)
{
}

void neigh_read_for_vlan (struct zebra_ns *zns, struct interface *vlan_if)
{
}

int
kernel_add_neigh (struct interface *ifp, struct ipaddr *ip,
                  struct ethaddr *mac)
{
  return 0;
}

int kernel_del_neigh (struct interface *ifp, struct ipaddr *ip)
{
  return 0;
}
