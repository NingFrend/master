/*
 * Copyright (C) 2016 by Open Source Routing.
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <zebra.h>

#include "prefix.h"
#include "stream.h"
#include "memory.h"
#include "zclient.h"
#include "command.h"
#include "network.h"
#include "linklist.h"
#include "mpls.h"

#include "ldpd.h"
#include "ldpe.h"
#include "lde.h"
#include "log.h"
#include "ldp_debug.h"

static void	 ifp2kif(struct interface *, struct kif *);
static void	 ifc2kaddr(struct interface *, struct connected *,
		    struct kaddr *);
static int	 zebra_send_mpls_labels(int, struct kroute *);
static int	 ldp_router_id_update(int, struct zclient *, zebra_size_t,
		    vrf_id_t);
static int	 ldp_interface_add(int, struct zclient *, zebra_size_t,
		    vrf_id_t);
static int	 ldp_interface_delete(int, struct zclient *, zebra_size_t,
		    vrf_id_t);
static int	 ldp_interface_status_change(int command, struct zclient *,
		    zebra_size_t, vrf_id_t);
static int	 ldp_interface_address_add(int, struct zclient *, zebra_size_t,
		    vrf_id_t);
static int	 ldp_interface_address_delete(int, struct zclient *,
		    zebra_size_t, vrf_id_t);
static int	 ldp_zebra_read_route(int, struct zclient *, zebra_size_t,
		    vrf_id_t);
static void	 ldp_zebra_connected(struct zclient *);

static struct zclient	*zclient;

static void
ifp2kif(struct interface *ifp, struct kif *kif)
{
	memset(kif, 0, sizeof(*kif));
	strlcpy(kif->ifname, ifp->name, sizeof(kif->ifname));
	kif->ifindex = ifp->ifindex;
	kif->operative = if_is_operative(ifp);
	if (ifp->ll_type == ZEBRA_LLT_ETHER)
		memcpy(kif->mac, ifp->hw_addr, ETHER_ADDR_LEN);
}

static void
ifc2kaddr(struct interface *ifp, struct connected *ifc, struct kaddr *ka)
{
	memset(ka, 0, sizeof(*ka));
	strlcpy(ka->ifname, ifp->name, sizeof(ka->ifname));
	ka->ifindex = ifp->ifindex;
	ka->af = ifc->address->family;
	ka->prefixlen = ifc->address->prefixlen;

	switch (ka->af) {
	case AF_INET:
		ka->addr.v4 = ifc->address->u.prefix4;
		if (ifc->destination)
			ka->dstbrd.v4 = ifc->destination->u.prefix4;
		break;
	case AF_INET6:
		ka->addr.v6 = ifc->address->u.prefix6;
		if (ifc->destination)
			ka->dstbrd.v6 = ifc->destination->u.prefix6;
		break;
	default:
		break;
	}
}

static int
zebra_send_mpls_labels(int cmd, struct kroute *kr)
{
	struct stream		*s;

	if (kr->local_label < MPLS_LABEL_RESERVED_MAX ||
	    kr->remote_label == NO_LABEL)
		return (0);

	debug_zebra_out("prefix %s/%u nexthop %s ifindex %u labels %s/%s (%s)",
	    log_addr(kr->af, &kr->prefix), kr->prefixlen,
	    log_addr(kr->af, &kr->nexthop), kr->ifindex,
	    log_label(kr->local_label), log_label(kr->remote_label),
	    (cmd == ZEBRA_MPLS_LABELS_ADD) ? "add" : "delete");

	/* Reset stream. */
	s = zclient->obuf;
	stream_reset(s);

	zclient_create_header(s, cmd, VRF_DEFAULT);
	stream_putc(s, ZEBRA_LSP_LDP);
	stream_putl(s, kr->af);
	switch (kr->af) {
	case AF_INET:
		stream_put_in_addr(s, &kr->prefix.v4);
		stream_putc(s, kr->prefixlen);
		stream_put_in_addr(s, &kr->nexthop.v4);
		break;
	case AF_INET6:
		stream_write(s, (u_char *)&kr->prefix.v6, 16);
		stream_putc(s, kr->prefixlen);
		stream_write(s, (u_char *)&kr->nexthop.v6, 16);
		break;
	default:
		fatalx("kr_change: unknown af");
	}
	stream_putl(s, kr->ifindex);
	stream_putc(s, kr->priority);
	stream_putl(s, kr->local_label);
	stream_putl(s, kr->remote_label);

	/* Put length at the first point of the stream. */
	stream_putw_at(s, 0, stream_get_endp(s));

	return (zclient_send_message(zclient));
}

int
kr_change(struct kroute *kr)
{
	return (zebra_send_mpls_labels(ZEBRA_MPLS_LABELS_ADD, kr));
}

int
kr_delete(struct kroute *kr)
{
	return (zebra_send_mpls_labels(ZEBRA_MPLS_LABELS_DELETE, kr));
}

static int
zebra_send_kpw(u_char cmd, struct zclient *zclient, struct kpw *kpw)
{
	struct stream		*s;

	debug_zebra_out("ILM %s PW %u (%s) ifindex %hu, type %d. %s -> nexthop %s label %s",
					(cmd == ZEBRA_KPW_ADD) ? "add" : "delete",
					kpw->pwid, kpw->vpn_name, kpw->ifindex,
					kpw->pw_type, log_label(kpw->local_label),
					log_addr(kpw->af, &kpw->nexthop), log_label(kpw->remote_label));

	/* Reset stream. */
	s = zclient->obuf;
	stream_reset(s);

	zclient_create_header(s, cmd, VRF_DEFAULT);
	stream_write (s, kpw->ifname, IF_NAMESIZE);
	stream_putw(s, kpw->ifindex);
	stream_putl(s, kpw->pw_type);
	stream_putl(s, kpw->lsr_id.s_addr);
	stream_putl(s, kpw->af);
	switch (kpw->af) {
	case AF_INET:
		stream_put_in_addr(s, &kpw->nexthop.v4);
		break;
	case AF_INET6:
		stream_write (s, (u_char *)&kpw->nexthop.v6, 16);
		break;
	default:
		fatalx("zebra_send_kpw: unknown af");
	}
	stream_putl(s, kpw->local_label);
	stream_putl(s, kpw->remote_label);
	stream_putc(s, kpw->flags);
	stream_putl(s, kpw->pwid);
	stream_write(s, kpw->vpn_name, L2VPN_NAME_LEN);
	stream_putw(s, kpw->ac_port_ifindex);

	/* Put length at the first point of the stream. */
	stream_putw_at(s, 0, stream_get_endp(s));

	return (zclient_send_message(zclient));
}

int
kmpw_set(struct kpw *kpw)
{
	zebra_send_kpw (ZEBRA_KPW_ADD, zclient, kpw);
	/* TODO */

	return (0);
}

int
kmpw_unset(struct kpw *kpw)
{
	zebra_send_kpw (ZEBRA_KPW_DELETE, zclient, kpw);
	/* TODO */

	return (0);
}

void
kif_redistribute(const char *ifname)
{
	struct listnode		*node, *cnode;
	struct interface	*ifp;
	struct connected	*ifc;
	struct kif		 kif;
	struct kaddr		 ka;

	for (ALL_LIST_ELEMENTS_RO(vrf_iflist(VRF_DEFAULT), node, ifp)) {
		if (ifname && strcmp(ifname, ifp->name) != 0)
			continue;

		ifp2kif(ifp, &kif);
		main_imsg_compose_both(IMSG_IFSTATUS, &kif, sizeof(kif));

		for (ALL_LIST_ELEMENTS_RO(ifp->connected, cnode, ifc)) {
			ifc2kaddr(ifp, ifc, &ka);
			main_imsg_compose_ldpe(IMSG_NEWADDR, 0, &ka,
			    sizeof(ka));
		}
	}
}

static int
ldp_router_id_update(int command, struct zclient *zclient, zebra_size_t length,
    vrf_id_t vrf_id)
{
	struct prefix	 router_id;

	zebra_router_id_update_read(zclient->ibuf, &router_id);

	if (bad_addr_v4(router_id.u.prefix4))
		return (0);

	debug_zebra_in("router-id update %s", inet_ntoa(router_id.u.prefix4));

	global.rtr_id.s_addr = router_id.u.prefix4.s_addr;
	main_imsg_compose_ldpe(IMSG_RTRID_UPDATE, 0, &global.rtr_id,
	    sizeof(global.rtr_id));

	return (0);
}

static int
ldp_interface_add(int command, struct zclient *zclient, zebra_size_t length,
    vrf_id_t vrf_id)
{
	struct interface	*ifp;
	struct kif		 kif;

	ifp = zebra_interface_add_read(zclient->ibuf, vrf_id);
	debug_zebra_in("interface add %s index %d mtu %d", ifp->name,
	    ifp->ifindex, ifp->mtu);

	ifp2kif(ifp, &kif);
	main_imsg_compose_both(IMSG_IFSTATUS, &kif, sizeof(kif));

	return (0);
}

static int
ldp_interface_delete(int command, struct zclient *zclient, zebra_size_t length,
    vrf_id_t vrf_id)
{
	struct interface	*ifp;
	struct kif		 kif;

	/* zebra_interface_state_read() updates interface structure in iflist */
	ifp = zebra_interface_state_read(zclient->ibuf, vrf_id);
	if (ifp == NULL)
		return (0);

	debug_zebra_in("interface delete %s index %d mtu %d", ifp->name,
	    ifp->ifindex, ifp->mtu);

	/* To support pseudo interface do not free interface structure.  */
	/* if_delete(ifp); */
	ifp->ifindex = IFINDEX_DELETED;

	ifp2kif(ifp, &kif);
	main_imsg_compose_both(IMSG_IFSTATUS, &kif, sizeof(kif));

	return (0);
}

static int
ldp_interface_status_change(int command, struct zclient *zclient,
    zebra_size_t length, vrf_id_t vrf_id)
{
	struct interface	*ifp;
	struct listnode		*node;
	struct connected	*ifc;
	struct kif		 kif;
	struct kaddr		 ka;

	/*
	 * zebra_interface_state_read() updates interface structure in
	 * iflist.
	 */
	ifp = zebra_interface_state_read(zclient->ibuf, vrf_id);
	if (ifp == NULL)
		return (0);

	debug_zebra_in("interface %s state update", ifp->name);

	ifp2kif(ifp, &kif);
	main_imsg_compose_both(IMSG_IFSTATUS, &kif, sizeof(kif));

	if (if_is_operative(ifp)) {
		for (ALL_LIST_ELEMENTS_RO(ifp->connected, node, ifc)) {
			ifc2kaddr(ifp, ifc, &ka);
			main_imsg_compose_ldpe(IMSG_NEWADDR, 0, &ka,
			    sizeof(ka));
		}
	} else {
		for (ALL_LIST_ELEMENTS_RO(ifp->connected, node, ifc)) {
			ifc2kaddr(ifp, ifc, &ka);
			main_imsg_compose_ldpe(IMSG_DELADDR, 0, &ka,
			    sizeof(ka));
		}
	}

	return (0);
}

static int
ldp_interface_address_add(int command, struct zclient *zclient,
    zebra_size_t length, vrf_id_t vrf_id)
{
	struct connected	*ifc;
	struct interface	*ifp;
	struct kaddr		 ka;

	ifc = zebra_interface_address_read(command, zclient->ibuf, vrf_id);
	if (ifc == NULL)
		return (0);

	ifp = ifc->ifp;
	ifc2kaddr(ifp, ifc, &ka);

	/* Filter invalid addresses.  */
	if (bad_addr(ka.af, &ka.addr))
		return (0);

	debug_zebra_in("address add %s/%u interface %s",
	    log_addr(ka.af, &ka.addr), ka.prefixlen, ifp->name);

	/* notify ldpe about new address */
	main_imsg_compose_ldpe(IMSG_NEWADDR, 0, &ka, sizeof(ka));

	return (0);
}

static int
ldp_interface_address_delete(int command, struct zclient *zclient,
    zebra_size_t length, vrf_id_t vrf_id)
{
	struct connected	*ifc;
	struct interface	*ifp;
	struct kaddr		 ka;

	ifc = zebra_interface_address_read(command, zclient->ibuf, vrf_id);
	if (ifc == NULL)
		return (0);

	ifp = ifc->ifp;
	ifc2kaddr(ifp, ifc, &ka);
	connected_free(ifc);

	/* Filter invalid addresses.  */
	if (bad_addr(ka.af, &ka.addr))
		return (0);

	debug_zebra_in("address delete %s/%u interface %s",
	    log_addr(ka.af, &ka.addr), ka.prefixlen, ifp->name);

	/* notify ldpe about removed address */
	main_imsg_compose_ldpe(IMSG_DELADDR, 0, &ka, sizeof(ka));

	return (0);
}

static int
ldp_zebra_read_route(int command, struct zclient *zclient, zebra_size_t length,
    vrf_id_t vrf_id)
{
	struct stream		*s;
	u_char			 type;
	u_char			 message_flags;
	struct kroute		 kr;
	int			 nhnum = 0, nhlen;
	size_t			 nhmark;

	memset(&kr, 0, sizeof(kr));
	s = zclient->ibuf;

	type = stream_getc(s);
	switch (type) {
	case ZEBRA_ROUTE_CONNECT:
		kr.flags |= F_CONNECTED;
		break;
	case ZEBRA_ROUTE_BGP:
		/* LDP should follow the IGP and ignore BGP routes */
		return (0);
	default:
		break;
	}

	stream_getl(s); /* flags, unused */
	stream_getw(s); /* instance, unused */
	message_flags = stream_getc(s);

	switch (command) {
	case ZEBRA_REDISTRIBUTE_IPV4_ADD:
	case ZEBRA_REDISTRIBUTE_IPV4_DEL:
		kr.af = AF_INET;
		nhlen = sizeof(struct in_addr);
		break;
	case ZEBRA_REDISTRIBUTE_IPV6_ADD:
	case ZEBRA_REDISTRIBUTE_IPV6_DEL:
		kr.af = AF_INET6;
		nhlen = sizeof(struct in6_addr);
		break;
	default:
		fatalx("ldp_zebra_read_route: unknown command");
	}
	kr.prefixlen = stream_getc(s);
	stream_get(&kr.prefix, s, PSIZE(kr.prefixlen));

	if (bad_addr(kr.af, &kr.prefix) ||
	    (kr.af == AF_INET6 && IN6_IS_SCOPE_EMBED(&kr.prefix.v6)))
		return (0);

	if (kr.af == AF_INET6 &&
	    CHECK_FLAG(message_flags, ZAPI_MESSAGE_SRCPFX)) {
		uint8_t src_prefixlen;

		src_prefixlen = stream_getc(s);

		/* we completely ignore srcdest routes for now. */
		if (src_prefixlen)
			return (0);
	}

	if (CHECK_FLAG(message_flags, ZAPI_MESSAGE_NEXTHOP)) {
		nhnum = stream_getc(s);
		nhmark = stream_get_getp(s);
		stream_set_getp(s, nhmark + nhnum * (nhlen + 5));
	}

	if (CHECK_FLAG(message_flags, ZAPI_MESSAGE_DISTANCE))
		kr.priority = stream_getc(s);
	if (CHECK_FLAG(message_flags, ZAPI_MESSAGE_METRIC))
		stream_getl(s);	/* metric, not used */

	if (CHECK_FLAG(message_flags, ZAPI_MESSAGE_NEXTHOP))
		stream_set_getp(s, nhmark);

	if (nhnum == 0) {
		switch (command) {
		case ZEBRA_REDISTRIBUTE_IPV4_ADD:
		case ZEBRA_REDISTRIBUTE_IPV6_ADD:
			return (0);
		case ZEBRA_REDISTRIBUTE_IPV4_DEL:
		case ZEBRA_REDISTRIBUTE_IPV6_DEL:
			debug_zebra_in("route delete %s/%d (%s)",
			    log_addr(kr.af, &kr.prefix), kr.prefixlen,
			    zebra_route_string(type));
			break;
		default:
			fatalx("ldp_zebra_read_route: unknown command");
		}
	}

	/* loop through all the nexthops */
	for (; nhnum > 0; nhnum--) {
		switch (kr.af) {
		case AF_INET:
			kr.nexthop.v4.s_addr = stream_get_ipv4(s);
			break;
		case AF_INET6:
			stream_get(&kr.nexthop.v6, s, sizeof(kr.nexthop.v6));
			break;
		default:
			break;
		}
		stream_getc(s);	/* ifindex_num, unused. */
		kr.ifindex = stream_getl(s);

		switch (command) {
		case ZEBRA_REDISTRIBUTE_IPV4_ADD:
		case ZEBRA_REDISTRIBUTE_IPV6_ADD:
			debug_zebra_in("route add %s/%d nexthop %s "
			    "ifindex %u (%s)", log_addr(kr.af, &kr.prefix),
			    kr.prefixlen, log_addr(kr.af, &kr.nexthop),
			    kr.ifindex, zebra_route_string(type));
			main_imsg_compose_lde(IMSG_NETWORK_ADD, 0, &kr,
			    sizeof(kr));
			break;
		default:
			break;
		}
	}

	main_imsg_compose_lde(IMSG_NETWORK_UPDATE, 0, &kr, sizeof(kr));

	return (0);
}

/**
 * Receive PW status update from Zebra and send it to LDE process.
 *
 * Params and return type are the ones required by zclient interface.
 *
 * @param command It will always be ZEBRA_PW_STATUS_UPDATE
 * @param zclient To get input stream from
 * @param length
 * @param vrf_id
 * @return 0 on success
 */
static int
ldp_zebra_read_pw_status_update (int command, struct zclient *zclient,
		zebra_size_t length, vrf_id_t vrf_id)
{
	struct stream		*s;
	uint8_t status;
	struct kpw kpw;

	memset (&kpw, 0, sizeof (struct kpw));

	/* Get input stream.  */
	s = zclient->ibuf;

	/* Get data. */
	stream_get (kpw.ifname, s, IF_NAMESIZE);
	/* ifindex = stream_getw (s); */
	/* pwid = stream_getl (s); */
	stream_get (kpw.vpn_name, s, L2VPN_NAME_LEN);
	status = stream_getc (s);
	if (status)
		kpw.flags |= F_PW_STATUS_UP;
	else
		kpw.flags &= ~F_PW_STATUS_UP;

	main_imsg_compose_lde (IMSG_PW_UPDATE, 0, &kpw, sizeof (kpw));

	return 0;
}

static void
ldp_zebra_connected(struct zclient *zclient)
{
	zclient_send_reg_requests(zclient, VRF_DEFAULT);
	zebra_redistribute_send(ZEBRA_REDISTRIBUTE_ADD, zclient, AFI_IP,
	    ZEBRA_ROUTE_ALL, 0, VRF_DEFAULT);
	zebra_redistribute_send(ZEBRA_REDISTRIBUTE_ADD, zclient, AFI_IP6,
	    ZEBRA_ROUTE_ALL, 0, VRF_DEFAULT);
}

void
ldp_zebra_init(struct thread_master *master)
{
	/* Set default values. */
	zclient = zclient_new(master);
	zclient_init(zclient, ZEBRA_ROUTE_LDP, 0);

	/* set callbacks */
	zclient->zebra_connected = ldp_zebra_connected;
	zclient->router_id_update = ldp_router_id_update;
	zclient->interface_add = ldp_interface_add;
	zclient->interface_delete = ldp_interface_delete;
	zclient->interface_up = ldp_interface_status_change;
	zclient->interface_down = ldp_interface_status_change;
	zclient->interface_address_add = ldp_interface_address_add;
	zclient->interface_address_delete = ldp_interface_address_delete;
	zclient->redistribute_route_ipv4_add = ldp_zebra_read_route;
	zclient->redistribute_route_ipv4_del = ldp_zebra_read_route;
	zclient->redistribute_route_ipv6_add = ldp_zebra_read_route;
	zclient->redistribute_route_ipv6_del = ldp_zebra_read_route;
	zclient->pw_status_update = ldp_zebra_read_pw_status_update;
}

void
ldp_zebra_destroy(void)
{
	zclient_stop(zclient);
	zclient_free(zclient);
	zclient = NULL;
}
