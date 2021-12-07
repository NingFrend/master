/*
 * PIM for Quagga
 * Copyright (C) 2008  Everton da Silva Marques
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; see the file COPYING; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <zebra.h>

#include "linklist.h"
#include "thread.h"
#include "memory.h"
#include "if.h"
#include "vrf.h"
#include "hash.h"
#include "jhash.h"
#include "prefix.h"

#include "pimd.h"
#include "pim_str.h"
#include "pim_iface.h"
#include "pim_ifchannel.h"
#include "pim_zebra.h"
#include "pim_time.h"
#include "pim_msg.h"
#include "pim_pim.h"
#include "pim_join.h"
#include "pim_rpf.h"
#include "pim_macro.h"
#include "pim_oil.h"
#include "pim_upstream.h"
#include "pim_ssm.h"
#include "pim_rp.h"
#include "pim_mlag.h"

RB_GENERATE(pim_ifchannel_rb, pim_ifchannel, pim_ifp_rb, pim_ifchannel_compare);

int pim_ifchannel_compare(const struct pim_ifchannel *ch1,
			  const struct pim_ifchannel *ch2)
{
	struct pim_interface *pim_ifp1;
	struct pim_interface *pim_ifp2;

	pim_ifp1 = ch1->interface->info;
	pim_ifp2 = ch2->interface->info;

	if (pim_ifp1->mroute_vif_index < pim_ifp2->mroute_vif_index)
		return -1;

	if (pim_ifp1->mroute_vif_index > pim_ifp2->mroute_vif_index)
		return 1;

	if (ntohl(ch1->sg.grp.ipaddr_v4.s_addr)
	    < ntohl(ch2->sg.grp.ipaddr_v4.s_addr))
		return -1;

	if (ntohl(ch1->sg.grp.ipaddr_v4.s_addr)
	    > ntohl(ch2->sg.grp.ipaddr_v4.s_addr))
		return 1;

	if (ntohl(ch1->sg.src.ipaddr_v4.s_addr)
	    < ntohl(ch2->sg.src.ipaddr_v4.s_addr))
		return -1;

	if (ntohl(ch1->sg.src.ipaddr_v4.s_addr)
	    > ntohl(ch2->sg.src.ipaddr_v4.s_addr))
		return 1;

	return 0;
}

/*
 * A (*,G) or a (*,*) is going away
 * remove the parent pointer from
 * those pointing at us
 */
static void pim_ifchannel_remove_children(struct pim_ifchannel *ch)
{
	struct pim_ifchannel *child;

	if (!ch->sources)
		return;

	while (!list_isempty(ch->sources)) {
		child = listnode_head(ch->sources);
		child->parent = NULL;
		listnode_delete(ch->sources, child);
	}
}

/*
 * A (*,G) or a (*,*) is being created
 * find all the children that would point
 * at us.
 */
static void pim_ifchannel_find_new_children(struct pim_ifchannel *ch)
{
	struct pim_interface *pim_ifp = ch->interface->info;
	struct pim_ifchannel *child;

	// Basic Sanity that we are not being silly
	if ((ch->sg.src.ipaddr_v4.s_addr != INADDR_ANY)
	    && (ch->sg.grp.ipaddr_v4.s_addr != INADDR_ANY))
		return;

	if ((ch->sg.src.ipaddr_v4.s_addr == INADDR_ANY)
	    && (ch->sg.grp.ipaddr_v4.s_addr == INADDR_ANY))
		return;

	RB_FOREACH (child, pim_ifchannel_rb, &pim_ifp->ifchannel_rb) {
		if ((ch->sg.grp.ipaddr_v4.s_addr != INADDR_ANY)
		    && (child->sg.grp.ipaddr_v4.s_addr
			== ch->sg.grp.ipaddr_v4.s_addr)
		    && (child != ch)) {
			child->parent = ch;
			listnode_add_sort(ch->sources, child);
		}
	}
}

void pim_ifchannel_delete(struct pim_ifchannel *ch)
{
	struct pim_interface *pim_ifp;
	struct pim_upstream *up;

	pim_ifp = ch->interface->info;

	if (PIM_DEBUG_PIM_TRACE)
		zlog_debug("%s: ifchannel entry %s(%s) del start", __func__,
			   ch->sg_str, ch->interface->name);

	if (PIM_I_am_DualActive(pim_ifp)) {
		if (PIM_DEBUG_MLAG)
			zlog_debug(
				"%s: if-chnanel-%s is deleted from a Dual active Interface",
				__func__, ch->sg_str);
		/* Post Delete only if it is the last Dual-active Interface */
		if (ch->upstream->dualactive_ifchannel_count == 1) {
			pim_mlag_up_local_del(pim_ifp->pim, ch->upstream);
			PIM_UPSTREAM_FLAG_UNSET_MLAG_INTERFACE(
				ch->upstream->flags);
		}
		ch->upstream->dualactive_ifchannel_count--;
	}

	if (ch->upstream->channel_oil) {
		uint32_t mask = PIM_OIF_FLAG_PROTO_PIM;
		if (ch->upstream->flags & PIM_UPSTREAM_FLAG_MASK_SRC_IGMP)
			mask |= PIM_OIF_FLAG_PROTO_IGMP;

		/*
		 * A S,G RPT channel can have an empty oil, we also
		 * need to take into account the fact that a ifchannel
		 * might have been suppressing a *,G ifchannel from
		 * being inherited.  So let's figure out what
		 * needs to be done here
		 */
		if ((ch->sg.src.ipaddr_v4.s_addr != INADDR_ANY)
		    && pim_upstream_evaluate_join_desired_interface(
			       ch->upstream, ch, ch->parent))
			pim_channel_add_oif(ch->upstream->channel_oil,
					ch->interface,
					PIM_OIF_FLAG_PROTO_STAR,
					__func__);

		pim_channel_del_oif(ch->upstream->channel_oil,
					ch->interface, mask, __func__);
		/*
		 * Do we have any S,G's that are inheriting?
		 * Nuke from on high too.
		 */
		if (ch->upstream->sources) {
			struct pim_upstream *child;
			struct listnode *up_node;

			for (ALL_LIST_ELEMENTS_RO(ch->upstream->sources,
						  up_node, child))
				pim_channel_del_inherited_oif(
						child->channel_oil,
						ch->interface,
						__func__);
		}
	}

	/*
	 * When this channel is removed
	 * we need to find all our children
	 * and make sure our pointers are fixed
	 */
	pim_ifchannel_remove_children(ch);

	if (ch->sources)
		list_delete(&ch->sources);

	listnode_delete(ch->upstream->ifchannels, ch);

	up = ch->upstream;

	/* upstream is common across ifchannels, check if upstream's
	   ifchannel list is empty before deleting upstream_del
	   ref count will take care of it.
	*/
	if (ch->upstream->ref_count > 0)
		up = pim_upstream_del(pim_ifp->pim, ch->upstream, __func__);

	else {
		if (PIM_DEBUG_PIM_TRACE)
			zlog_debug(
				"%s: Avoiding deletion of upstream with ref_count %d from ifchannel(%s): %s",
				__func__, ch->upstream->ref_count,
				ch->interface->name, ch->sg_str);
	}

	ch->upstream = NULL;

	THREAD_OFF(ch->t_ifjoin_expiry_timer);
	THREAD_OFF(ch->t_ifjoin_prune_pending_timer);
	THREAD_OFF(ch->t_ifassert_timer);

	if (ch->parent) {
		listnode_delete(ch->parent->sources, ch);
		ch->parent = NULL;
	}

	RB_REMOVE(pim_ifchannel_rb, &pim_ifp->ifchannel_rb, ch);

	if (PIM_DEBUG_PIM_TRACE)
		zlog_debug("%s: ifchannel entry %s(%s) is deleted ", __func__,
			   ch->sg_str, ch->interface->name);

	XFREE(MTYPE_PIM_IFCHANNEL, ch);

	if (up)
		pim_upstream_update_join_desired(pim_ifp->pim, up);
}

void pim_ifchannel_delete_all(struct interface *ifp)
{
	struct pim_interface *pim_ifp;
	struct pim_ifchannel *ch;

	pim_ifp = ifp->info;
	if (!pim_ifp)
		return;

	while (!RB_EMPTY(pim_ifchannel_rb, &pim_ifp->ifchannel_rb)) {
		ch = RB_ROOT(pim_ifchannel_rb, &pim_ifp->ifchannel_rb);

		pim_ifchannel_ifjoin_switch(__func__, ch, PIM_IFJOIN_NOINFO);
		pim_ifchannel_delete(ch);
	}
}

void delete_on_noinfo(struct pim_ifchannel *ch)
{
	if (ch->local_ifmembership == PIM_IFMEMBERSHIP_NOINFO
	    && ch->ifjoin_state == PIM_IFJOIN_NOINFO
	    && ch->t_ifjoin_expiry_timer == NULL)
		pim_ifchannel_delete(ch);
}

void pim_ifchannel_ifjoin_switch(const char *caller, struct pim_ifchannel *ch,
				 enum pim_ifjoin_state new_state)
{
	enum pim_ifjoin_state old_state = ch->ifjoin_state;
	struct pim_interface *pim_ifp = ch->interface->info;
	struct pim_ifchannel *child_ch;

	if (PIM_DEBUG_PIM_EVENTS)
		zlog_debug(
			"PIM_IFCHANNEL(%s): %s is switching from %s to %s",
			ch->interface->name, ch->sg_str,
			pim_ifchannel_ifjoin_name(ch->ifjoin_state, ch->flags),
			pim_ifchannel_ifjoin_name(new_state, 0));


	if (old_state == new_state) {
		if (PIM_DEBUG_PIM_EVENTS) {
			zlog_debug(
				"%s calledby %s: non-transition on state %d (%s)",
				__func__, caller, new_state,
				pim_ifchannel_ifjoin_name(new_state, 0));
		}
		return;
	}

	ch->ifjoin_state = new_state;

	if (ch->sg.src.ipaddr_v4.s_addr == INADDR_ANY) {
		struct pim_upstream *up = ch->upstream;
		struct pim_upstream *child;
		struct listnode *up_node;

		if (up) {
			if (ch->ifjoin_state == PIM_IFJOIN_NOINFO) {
				for (ALL_LIST_ELEMENTS_RO(up->sources, up_node,
							  child)) {
					struct channel_oil *c_oil =
						child->channel_oil;

					if (PIM_DEBUG_PIM_TRACE)
						zlog_debug(
							"%s %s: Prune(S,G)=%s from %s",
							__FILE__, __func__,
							child->sg_str,
							up->sg_str);
					if (!c_oil)
						continue;

					/*
					 * If the S,G has no if channel and the
					 * c_oil still
					 * has output here then the *,G was
					 * supplying the implied
					 * if channel.  So remove it.
					 */
					if (c_oil->oil.mfcc_ttls
						    [pim_ifp->mroute_vif_index])
						pim_channel_del_inherited_oif(
							c_oil, ch->interface,
							__func__);
				}
			}
			if (ch->ifjoin_state == PIM_IFJOIN_JOIN) {
				for (ALL_LIST_ELEMENTS_RO(up->sources, up_node,
							  child)) {
					if (PIM_DEBUG_PIM_TRACE)
						zlog_debug(
							"%s %s: Join(S,G)=%s from %s",
							__FILE__, __func__,
							child->sg_str,
							up->sg_str);

					/* check if the channel can be
					 * inherited into the SG's OIL
					 */
					child_ch = pim_ifchannel_find(
							ch->interface,
							&child->sg);
					if (pim_upstream_eval_inherit_if(
						    child, child_ch, ch)) {
						pim_channel_add_oif(
							child->channel_oil,
							ch->interface,
							PIM_OIF_FLAG_PROTO_STAR,
							__func__);
						pim_upstream_update_join_desired(
							pim_ifp->pim, child);
					}
				}
			}
		}
	}
	/* Transition to/from NOINFO ? */
	if ((old_state == PIM_IFJOIN_NOINFO)
	    || (new_state == PIM_IFJOIN_NOINFO)) {

		if (PIM_DEBUG_PIM_EVENTS) {
			zlog_debug("PIM_IFCHANNEL_%s: (S,G)=%s on interface %s",
				   ((new_state == PIM_IFJOIN_NOINFO) ? "DOWN"
								     : "UP"),
				   ch->sg_str, ch->interface->name);
		}

		/*
		  Record uptime of state transition to/from NOINFO
		*/
		ch->ifjoin_creation = pim_time_monotonic_sec();

		pim_upstream_update_join_desired(pim_ifp->pim, ch->upstream);
		pim_ifchannel_update_could_assert(ch);
		pim_ifchannel_update_assert_tracking_desired(ch);
	}
}

const char *pim_ifchannel_ifjoin_name(enum pim_ifjoin_state ifjoin_state,
				      int flags)
{
	switch (ifjoin_state) {
	case PIM_IFJOIN_NOINFO:
		if (PIM_IF_FLAG_TEST_S_G_RPT(flags))
			return "SGRpt(NI)";
		else
			return "NOINFO";
	case PIM_IFJOIN_JOIN:
		return "JOIN";
	case PIM_IFJOIN_PRUNE:
		if (PIM_IF_FLAG_TEST_S_G_RPT(flags))
			return "SGRpt(P)";
		else
			return "PRUNE";
	case PIM_IFJOIN_PRUNE_PENDING:
		if (PIM_IF_FLAG_TEST_S_G_RPT(flags))
			return "SGRpt(PP)";
		else
			return "PRUNEP";
	case PIM_IFJOIN_PRUNE_TMP:
		if (PIM_IF_FLAG_TEST_S_G_RPT(flags))
			return "SGRpt(P')";
		else
			return "PRUNET";
	case PIM_IFJOIN_PRUNE_PENDING_TMP:
		if (PIM_IF_FLAG_TEST_S_G_RPT(flags))
			return "SGRpt(PP')";
		else
			return "PRUNEPT";
	}

	return "ifjoin_bad_state";
}

const char *pim_ifchannel_ifassert_name(enum pim_ifassert_state ifassert_state)
{
	switch (ifassert_state) {
	case PIM_IFASSERT_NOINFO:
		return "NOINFO";
	case PIM_IFASSERT_I_AM_WINNER:
		return "WINNER";
	case PIM_IFASSERT_I_AM_LOSER:
		return "LOSER";
	}

	return "ifassert_bad_state";
}

/*
  RFC 4601: 4.6.5.  Assert State Macros

  AssertWinner(S,G,I) defaults to NULL and AssertWinnerMetric(S,G,I)
  defaults to Infinity when in the NoInfo state.
*/
void reset_ifassert_state(struct pim_ifchannel *ch)
{
	struct in_addr any = {.s_addr = INADDR_ANY};

	THREAD_OFF(ch->t_ifassert_timer);

	pim_ifassert_winner_set(ch, PIM_IFASSERT_NOINFO, any,
				router->infinite_assert_metric);
}

struct pim_ifchannel *pim_ifchannel_find(struct interface *ifp,
					 struct prefix_sg *sg)
{
	struct pim_interface *pim_ifp;
	struct pim_ifchannel *ch;
	struct pim_ifchannel lookup;

	pim_ifp = ifp->info;

	if (!pim_ifp) {
		zlog_warn("%s: (S,G)=%s: multicast not enabled on interface %s",
			  __func__, pim_str_sg_dump(sg), ifp->name);
		return NULL;
	}

	lookup.sg = *sg;
	lookup.interface = ifp;
	ch = RB_FIND(pim_ifchannel_rb, &pim_ifp->ifchannel_rb, &lookup);

	return ch;
}

static void ifmembership_set(struct pim_ifchannel *ch,
			     enum pim_ifmembership membership)
{
	struct pim_interface *pim_ifp = ch->interface->info;

	if (ch->local_ifmembership == membership)
		return;

	if (PIM_DEBUG_PIM_EVENTS) {
		zlog_debug("%s: (S,G)=%s membership now is %s on interface %s",
			   __func__, ch->sg_str,
			   membership == PIM_IFMEMBERSHIP_INCLUDE ? "INCLUDE"
								  : "NOINFO",
			   ch->interface->name);
	}

	ch->local_ifmembership = membership;

	pim_upstream_update_join_desired(pim_ifp->pim, ch->upstream);
	pim_ifchannel_update_could_assert(ch);
	pim_ifchannel_update_assert_tracking_desired(ch);
}


void pim_ifchannel_membership_clear(struct interface *ifp)
{
	struct pim_interface *pim_ifp;
	struct pim_ifchannel *ch;

	pim_ifp = ifp->info;
	assert(pim_ifp);

	RB_FOREACH (ch, pim_ifchannel_rb, &pim_ifp->ifchannel_rb)
		ifmembership_set(ch, PIM_IFMEMBERSHIP_NOINFO);
}

void pim_ifchannel_delete_on_noinfo(struct interface *ifp)
{
	struct pim_interface *pim_ifp;
	struct pim_ifchannel *ch, *ch_tmp;

	pim_ifp = ifp->info;
	assert(pim_ifp);

	RB_FOREACH_SAFE (ch, pim_ifchannel_rb, &pim_ifp->ifchannel_rb, ch_tmp)
		delete_on_noinfo(ch);
}

/*
 * For a given Interface, if we are given a S,G
 * Find the *,G (If we have it).
 * If we are passed a *,G, find the *,* ifchannel
 * if we have it.
 */
static struct pim_ifchannel *pim_ifchannel_find_parent(struct pim_ifchannel *ch)
{
	struct prefix_sg parent_sg = ch->sg;
	struct pim_ifchannel *parent = NULL;

	// (S,G)
	if ((parent_sg.src.ipaddr_v4.s_addr != INADDR_ANY)
	    && (parent_sg.grp.ipaddr_v4.s_addr != INADDR_ANY)) {
		parent_sg.src.ipaddr_v4.s_addr = INADDR_ANY;
		parent = pim_ifchannel_find(ch->interface, &parent_sg);

		if (parent)
			listnode_add(parent->sources, ch);
		return parent;
	}

	return NULL;
}

struct pim_ifchannel *pim_ifchannel_add(struct interface *ifp,
					struct prefix_sg *sg,
					uint8_t source_flags, int up_flags)
{
	struct pim_interface *pim_ifp;
	struct pim_ifchannel *ch;
	struct pim_upstream *up;

	ch = pim_ifchannel_find(ifp, sg);
	if (ch) {
		if (up_flags == PIM_UPSTREAM_FLAG_MASK_SRC_PIM)
			PIM_IF_FLAG_SET_PROTO_PIM(ch->flags);

		if (up_flags == PIM_UPSTREAM_FLAG_MASK_SRC_IGMP)
			PIM_IF_FLAG_SET_PROTO_IGMP(ch->flags);

		if (ch->upstream)
			ch->upstream->flags |= up_flags;
		else if (PIM_DEBUG_EVENTS)
			zlog_debug("%s:%s No Upstream found", __func__,
				   pim_str_sg_dump(sg));

		return ch;
	}

	pim_ifp = ifp->info;

	ch = XCALLOC(MTYPE_PIM_IFCHANNEL, sizeof(*ch));

	ch->flags = 0;
	if ((source_flags & PIM_ENCODE_RPT_BIT)
	    && !(source_flags & PIM_ENCODE_WC_BIT))
		PIM_IF_FLAG_SET_S_G_RPT(ch->flags);

	ch->interface = ifp;
	ch->sg = *sg;
	pim_str_sg_set(sg, ch->sg_str);
	ch->parent = pim_ifchannel_find_parent(ch);
	if (ch->sg.src.ipaddr_v4.s_addr == INADDR_ANY) {
		ch->sources = list_new();
		ch->sources->cmp =
			(int (*)(void *, void *))pim_ifchannel_compare;
	} else
		ch->sources = NULL;

	pim_ifchannel_find_new_children(ch);
	ch->local_ifmembership = PIM_IFMEMBERSHIP_NOINFO;

	ch->ifjoin_state = PIM_IFJOIN_NOINFO;
	ch->t_ifjoin_expiry_timer = NULL;
	ch->t_ifjoin_prune_pending_timer = NULL;
	ch->ifjoin_creation = 0;

	RB_INSERT(pim_ifchannel_rb, &pim_ifp->ifchannel_rb, ch);

	up = pim_upstream_add(pim_ifp->pim, sg, NULL, up_flags, __func__, ch);

	ch->upstream = up;

	listnode_add_sort(up->ifchannels, ch);

	ch->ifassert_my_metric = pim_macro_ch_my_assert_metric_eval(ch);
	ch->ifassert_winner_metric = pim_macro_ch_my_assert_metric_eval(ch);

	ch->ifassert_winner.s_addr = INADDR_ANY;

	/* Assert state */
	ch->t_ifassert_timer = NULL;
	ch->ifassert_state = PIM_IFASSERT_NOINFO;
	reset_ifassert_state(ch);
	if (pim_macro_ch_could_assert_eval(ch))
		PIM_IF_FLAG_SET_COULD_ASSERT(ch->flags);
	else
		PIM_IF_FLAG_UNSET_COULD_ASSERT(ch->flags);

	if (pim_macro_assert_tracking_desired_eval(ch))
		PIM_IF_FLAG_SET_ASSERT_TRACKING_DESIRED(ch->flags);
	else
		PIM_IF_FLAG_UNSET_ASSERT_TRACKING_DESIRED(ch->flags);

	/*
	 * advertise MLAG Data to MLAG peer
	 */
	if (PIM_I_am_DualActive(pim_ifp)) {
		up->dualactive_ifchannel_count++;
		/* Sync once for upstream */
		if (up->dualactive_ifchannel_count == 1) {
			PIM_UPSTREAM_FLAG_SET_MLAG_INTERFACE(up->flags);
			pim_mlag_up_local_add(pim_ifp->pim, up);
		}
		if (PIM_DEBUG_MLAG)
			zlog_debug(
				"%s: New Dual active if-chnanel is added to upstream:%s count:%d, flags:0x%x",
				__func__, up->sg_str,
				up->dualactive_ifchannel_count, up->flags);
	}

	if (up_flags == PIM_UPSTREAM_FLAG_MASK_SRC_PIM)
		PIM_IF_FLAG_SET_PROTO_PIM(ch->flags);

	if (up_flags == PIM_UPSTREAM_FLAG_MASK_SRC_IGMP)
		PIM_IF_FLAG_SET_PROTO_IGMP(ch->flags);

	if (PIM_DEBUG_PIM_TRACE)
		zlog_debug("%s: ifchannel %s(%s) is created ", __func__,
			   ch->sg_str, ch->interface->name);

	return ch;
}

static void ifjoin_to_noinfo(struct pim_ifchannel *ch)
{
	pim_ifchannel_ifjoin_switch(__func__, ch, PIM_IFJOIN_NOINFO);
	pim_forward_stop(ch);

	if (ch->upstream)
		PIM_UPSTREAM_FLAG_UNSET_SRC_PIM(ch->upstream->flags);

	PIM_IF_FLAG_UNSET_PROTO_PIM(ch->flags);

	delete_on_noinfo(ch);
}

static int on_ifjoin_expiry_timer(struct thread *t)
{
	struct pim_ifchannel *ch;

	ch = THREAD_ARG(t);

	if (PIM_DEBUG_PIM_TRACE)
		zlog_debug("%s: ifchannel %s expiry timer", __func__,
			   ch->sg_str);

	ifjoin_to_noinfo(ch);
	/* ch may have been deleted */

	return 0;
}

static int on_ifjoin_prune_pending_timer(struct thread *t)
{
	struct pim_ifchannel *ch;
	int send_prune_echo; /* boolean */
	struct interface *ifp;
	struct pim_interface *pim_ifp;

	ch = THREAD_ARG(t);

	if (PIM_DEBUG_PIM_TRACE)
		zlog_debug(
			"%s: IFCHANNEL%s %s Prune Pending Timer Popped",
			__func__, pim_str_sg_dump(&ch->sg),
			pim_ifchannel_ifjoin_name(ch->ifjoin_state, ch->flags));

	if (ch->ifjoin_state == PIM_IFJOIN_PRUNE_PENDING) {
		ifp = ch->interface;
		pim_ifp = ifp->info;
		if (!PIM_IF_FLAG_TEST_S_G_RPT(ch->flags)) {
			/* Send PruneEcho(S,G) ? */
			send_prune_echo =
				(listcount(pim_ifp->pim_neighbor_list) > 1);

			if (send_prune_echo) {
				struct pim_rpf rpf;

				rpf.source_nexthop.interface = ifp;
				rpf.rpf_addr.u.prefix4 =
					pim_ifp->primary_address;
				pim_jp_agg_single_upstream_send(
					&rpf, ch->upstream, 0);
			}

			ifjoin_to_noinfo(ch);
		} else {
			/* If SGRpt flag is set on ifchannel, Trigger SGRpt
			 *  message on RP path upon prune timer expiry.
			 */
			ch->ifjoin_state = PIM_IFJOIN_PRUNE;
			if (ch->upstream) {
				struct pim_upstream *parent =
					ch->upstream->parent;

				pim_upstream_update_join_desired(pim_ifp->pim,
								 ch->upstream);

				pim_jp_agg_single_upstream_send(&parent->rpf,
								parent, true);
				/*
				 * SGRpt prune pending expiry has to install
				 * SG entry with empty olist to drop the SG
				 * traffic incase no other intf exists.
				 * On that scenario, SG entry wouldn't have
				 * got installed until Prune pending timer
				 * expired. So install now.
				 */
				pim_channel_del_oif(
					ch->upstream->channel_oil, ifp,
					PIM_OIF_FLAG_PROTO_STAR, __func__);
				if (!ch->upstream->channel_oil->installed)
					pim_upstream_mroute_add(
						ch->upstream->channel_oil,
						__func__);
			}
		}
		/* from here ch may have been deleted */
	}

	return 0;
}

static void check_recv_upstream(int is_join, struct interface *recv_ifp,
				struct in_addr upstream, struct prefix_sg *sg,
				uint8_t source_flags, int holdtime)
{
	struct pim_upstream *up;
	struct pim_interface *pim_ifp = recv_ifp->info;

	/* Upstream (S,G) in Joined state ? */
	up = pim_upstream_find(pim_ifp->pim, sg);
	if (!up)
		return;
	if (up->join_state != PIM_UPSTREAM_JOINED)
		return;

	/* Upstream (S,G) in Joined state */

	if (pim_rpf_addr_is_inaddr_any(&up->rpf)) {
		/* RPF'(S,G) not found */
		zlog_warn("%s %s: RPF'%s not found", __FILE__, __func__,
			  up->sg_str);
		return;
	}

	/* upstream directed to RPF'(S,G) ? */
	if (upstream.s_addr != up->rpf.rpf_addr.u.prefix4.s_addr) {
		char up_str[INET_ADDRSTRLEN];
		char rpf_str[PREFIX_STRLEN];
		pim_inet4_dump("<up?>", upstream, up_str, sizeof(up_str));
		pim_addr_dump("<rpf?>", &up->rpf.rpf_addr, rpf_str,
			      sizeof(rpf_str));
		zlog_warn(
			"%s %s: (S,G)=%s upstream=%s not directed to RPF'(S,G)=%s on interface %s",
			__FILE__, __func__, up->sg_str, up_str, rpf_str,
			recv_ifp->name);
		return;
	}
	/* upstream directed to RPF'(S,G) */

	if (is_join) {
		/* Join(S,G) to RPF'(S,G) */
		pim_upstream_join_suppress(up, up->rpf.rpf_addr.u.prefix4,
					   holdtime);
		return;
	}

	/* Prune to RPF'(S,G) */

	if (source_flags & PIM_RPT_BIT_MASK) {
		if (source_flags & PIM_WILDCARD_BIT_MASK) {
			/* Prune(*,G) to RPF'(S,G) */
			pim_upstream_join_timer_decrease_to_t_override(
				"Prune(*,G)", up);
			return;
		}

		/* Prune(S,G,rpt) to RPF'(S,G) */
		pim_upstream_join_timer_decrease_to_t_override("Prune(S,G,rpt)",
							       up);
		return;
	}

	/* Prune(S,G) to RPF'(S,G) */
	pim_upstream_join_timer_decrease_to_t_override("Prune(S,G)", up);
}

static int nonlocal_upstream(int is_join, struct interface *recv_ifp,
			     struct in_addr upstream, struct prefix_sg *sg,
			     uint8_t source_flags, uint16_t holdtime)
{
	struct pim_interface *recv_pim_ifp;
	int is_local; /* boolean */

	recv_pim_ifp = recv_ifp->info;
	assert(recv_pim_ifp);

	is_local = (upstream.s_addr == recv_pim_ifp->primary_address.s_addr);

	if (is_local)
		return 0;

	if (PIM_DEBUG_PIM_TRACE_DETAIL) {
		char up_str[INET_ADDRSTRLEN];
		pim_inet4_dump("<upstream?>", upstream, up_str, sizeof(up_str));
		zlog_warn("%s: recv %s (S,G)=%s to non-local upstream=%s on %s",
			  __func__, is_join ? "join" : "prune",
			  pim_str_sg_dump(sg), up_str, recv_ifp->name);
	}

	/*
	 * Since recv upstream addr was not directed to our primary
	 * address, check if we should react to it in any way.
	 */
	check_recv_upstream(is_join, recv_ifp, upstream, sg, source_flags,
			    holdtime);

	return 1; /* non-local */
}

static void pim_ifchannel_ifjoin_handler(struct pim_ifchannel *ch,
		struct pim_interface *pim_ifp)
{
	pim_ifchannel_ifjoin_switch(__func__, ch, PIM_IFJOIN_JOIN);
	PIM_IF_FLAG_UNSET_S_G_RPT(ch->flags);
	/* check if the interface qualifies as an immediate
	 * OIF
	 */
	if (pim_upstream_evaluate_join_desired_interface(
				ch->upstream, ch,
				NULL /*starch*/)) {
		pim_channel_add_oif(ch->upstream->channel_oil,
				ch->interface,
				PIM_OIF_FLAG_PROTO_PIM,
				__func__);
		pim_upstream_update_join_desired(pim_ifp->pim,
				ch->upstream);
	}
}


void pim_ifchannel_join_add(struct interface *ifp, struct in_addr neigh_addr,
			    struct in_addr upstream, struct prefix_sg *sg,
			    uint8_t source_flags, uint16_t holdtime)
{
	struct pim_interface *pim_ifp;
	struct pim_ifchannel *ch;

	if (nonlocal_upstream(1 /* join */, ifp, upstream, sg, source_flags,
			      holdtime)) {
		return;
	}

	ch = pim_ifchannel_add(ifp, sg, source_flags,
			       PIM_UPSTREAM_FLAG_MASK_SRC_PIM);

	/*
	  RFC 4601: 4.6.1.  (S,G) Assert Message State Machine

	  Transitions from "I am Assert Loser" State

	  Receive Join(S,G) on Interface I

	  We receive a Join(S,G) that has the Upstream Neighbor Address
	  field set to my primary IP address on interface I.  The action is
	  to transition to NoInfo state, delete this (S,G) assert state
	  (Actions A5 below), and allow the normal PIM Join/Prune mechanisms
	  to operate.

	  Notice: The nonlocal_upstream() test above ensures the upstream
	  address of the join message is our primary address.
	 */
	if (ch->ifassert_state == PIM_IFASSERT_I_AM_LOSER) {
		char neigh_str[INET_ADDRSTRLEN];
		pim_inet4_dump("<neigh?>", neigh_addr, neigh_str,
			       sizeof(neigh_str));
		zlog_warn("%s: Assert Loser recv Join%s from %s on %s",
			  __func__, ch->sg_str, neigh_str, ifp->name);

		assert_action_a5(ch);
	}

	pim_ifp = ifp->info;
	assert(pim_ifp);

	switch (ch->ifjoin_state) {
	case PIM_IFJOIN_NOINFO:
		pim_ifchannel_ifjoin_switch(__func__, ch, PIM_IFJOIN_JOIN);
		if (pim_macro_chisin_oiflist(ch)) {
			pim_upstream_inherited_olist(pim_ifp->pim,
						     ch->upstream);
			pim_forward_start(ch);
		}
		/*
		 * If we are going to be a LHR, we need to note it
		 */
		if (ch->upstream->parent &&
			(PIM_UPSTREAM_FLAG_TEST_CAN_BE_LHR(
						   ch->upstream->parent->flags))
		    && !(ch->upstream->flags
			 & PIM_UPSTREAM_FLAG_MASK_SRC_LHR)) {
			pim_upstream_ref(ch->upstream,
					 PIM_UPSTREAM_FLAG_MASK_SRC_LHR,
					 __func__);
			pim_upstream_keep_alive_timer_start(
				ch->upstream, pim_ifp->pim->keep_alive_time);
		}
		break;
	case PIM_IFJOIN_JOIN:
		assert(!ch->t_ifjoin_prune_pending_timer);

		/*
		  In the JOIN state ch->t_ifjoin_expiry_timer may be NULL due to
		  a
		  previously received join message with holdtime=0xFFFF.
		 */
		if (ch->t_ifjoin_expiry_timer) {
			unsigned long remain = thread_timer_remain_second(
				ch->t_ifjoin_expiry_timer);
			if (remain > holdtime) {
				/*
				  RFC 4601: 4.5.3.  Receiving (S,G) Join/Prune
				  Messages

				  Transitions from Join State

				  The (S,G) downstream state machine on
				  interface I remains in
				  Join state, and the Expiry Timer (ET) is
				  restarted, set to
				  maximum of its current value and the HoldTime
				  from the
				  triggering Join/Prune message.

				  Conclusion: Do not change the ET if the
				  current value is
				  higher than the received join holdtime.
				 */
				return;
			}
		}
		THREAD_OFF(ch->t_ifjoin_expiry_timer);
		break;
	case PIM_IFJOIN_PRUNE:
		if (source_flags & PIM_ENCODE_RPT_BIT) {
			pim_ifchannel_ifjoin_switch(__func__, ch,
						    PIM_IFJOIN_NOINFO);
			THREAD_OFF(ch->t_ifjoin_expiry_timer);
			delete_on_noinfo(ch);
			return;
		} else
			pim_ifchannel_ifjoin_handler(ch, pim_ifp);
		break;
	case PIM_IFJOIN_PRUNE_PENDING:
		/*
		 * Transitions from Prune-Pending State (Receive Join)
		 * RFC 7761 Sec 4.5.2:
		 *    The (S,G) downstream state machine on interface I
		 * transitions to the Join state.  The Prune-Pending Timer is
		 * canceled (without triggering an expiry event).  The
		 * Expiry Timer (ET) is restarted and is then set to the
		 * maximum of its current value and the HoldTime from the
		 * triggering Join/Prune message.
		 */
		THREAD_OFF(ch->t_ifjoin_prune_pending_timer);

		/* Check if SGRpt join Received */
		if ((source_flags & PIM_ENCODE_RPT_BIT)
		    && (sg->src.ipaddr_v4.s_addr != INADDR_ANY)) {
			/*
			 * Transitions from Prune-Pending State (Rcv SGRpt Join)
			 * RFC 7761 Sec 4.5.3:
			 * The (S,G,rpt) downstream state machine on interface
			 * I transitions to the NoInfo state.The ET and PPT are
			 * cancelled.
			 */
			THREAD_OFF(ch->t_ifjoin_expiry_timer);
			pim_ifchannel_ifjoin_switch(__func__, ch,
						    PIM_IFJOIN_NOINFO);
			return;
		}

		pim_ifchannel_ifjoin_handler(ch, pim_ifp);

		if (ch->t_ifjoin_expiry_timer) {
			unsigned long remain = thread_timer_remain_second(
				ch->t_ifjoin_expiry_timer);

			if (remain > holdtime)
				return;
		}
		THREAD_OFF(ch->t_ifjoin_expiry_timer);

		break;
	case PIM_IFJOIN_PRUNE_TMP:
		break;
	case PIM_IFJOIN_PRUNE_PENDING_TMP:
		break;
	}

	if (holdtime != 0xFFFF) {
		thread_add_timer(router->master, on_ifjoin_expiry_timer, ch,
				 holdtime, &ch->t_ifjoin_expiry_timer);
	}
}

void pim_ifchannel_prune(struct interface *ifp, struct in_addr upstream,
			 struct prefix_sg *sg, uint8_t source_flags,
			 uint16_t holdtime)
{
	struct pim_ifchannel *ch;
	struct pim_interface *pim_ifp;
	int jp_override_interval_msec;

	if (nonlocal_upstream(0 /* prune */, ifp, upstream, sg, source_flags,
			      holdtime)) {
		return;
	}

	ch = pim_ifchannel_find(ifp, sg);
	if (!ch && !(source_flags & PIM_ENCODE_RPT_BIT)) {
		if (PIM_DEBUG_PIM_TRACE)
			zlog_debug(
				"%s: Received prune with no relevant ifchannel %s%s state: %d",
				__func__, ifp->name, pim_str_sg_dump(sg),
				source_flags);
		return;
	}

	ch = pim_ifchannel_add(ifp, sg, source_flags,
			       PIM_UPSTREAM_FLAG_MASK_SRC_PIM);

	pim_ifp = ifp->info;

	switch (ch->ifjoin_state) {
	case PIM_IFJOIN_NOINFO:
		if (source_flags & PIM_ENCODE_RPT_BIT) {
			if (!(source_flags & PIM_ENCODE_WC_BIT))
				PIM_IF_FLAG_SET_S_G_RPT(ch->flags);

			ch->ifjoin_state = PIM_IFJOIN_PRUNE_PENDING;
			if (listcount(pim_ifp->pim_neighbor_list) > 1)
				jp_override_interval_msec =
					pim_if_jp_override_interval_msec(ifp);
			else
				jp_override_interval_msec =
					0; /* schedule to expire immediately */
			/* If we called ifjoin_prune() directly instead, care
			   should
			   be taken not to use "ch" afterwards since it would be
			   deleted. */

			THREAD_OFF(ch->t_ifjoin_prune_pending_timer);
			THREAD_OFF(ch->t_ifjoin_expiry_timer);
			thread_add_timer_msec(
				router->master, on_ifjoin_prune_pending_timer,
				ch, jp_override_interval_msec,
				&ch->t_ifjoin_prune_pending_timer);
			thread_add_timer(router->master, on_ifjoin_expiry_timer,
					 ch, holdtime,
					 &ch->t_ifjoin_expiry_timer);
			pim_upstream_update_join_desired(pim_ifp->pim,
							 ch->upstream);
		}
		break;
	case PIM_IFJOIN_PRUNE_PENDING:
		/* nothing to do */
		break;
	case PIM_IFJOIN_JOIN:
		/*
		 * The (S,G) downstream state machine on interface I
		 * transitions to the Prune-Pending state.  The
		 * Prune-Pending Timer is started.  It is set to the
		 * J/P_Override_Interval(I) if the router has more than one
		 * neighbor on that interface; otherwise, it is set to zero,
		 * causing it to expire immediately.
		 */

		pim_ifchannel_ifjoin_switch(__func__, ch,
					    PIM_IFJOIN_PRUNE_PENDING);

		if (listcount(pim_ifp->pim_neighbor_list) > 1)
			jp_override_interval_msec =
				pim_if_jp_override_interval_msec(ifp);
		else
			jp_override_interval_msec =
				0; /* schedule to expire immediately */
		/* If we called ifjoin_prune() directly instead, care should
		   be taken not to use "ch" afterwards since it would be
		   deleted. */
		THREAD_OFF(ch->t_ifjoin_prune_pending_timer);
		thread_add_timer_msec(router->master,
				      on_ifjoin_prune_pending_timer, ch,
				      jp_override_interval_msec,
				      &ch->t_ifjoin_prune_pending_timer);
		break;
	case PIM_IFJOIN_PRUNE:
		if (source_flags & PIM_ENCODE_RPT_BIT) {
			THREAD_OFF(ch->t_ifjoin_prune_pending_timer);
			/*
			 * While in Prune State, Receive SGRpt Prune.
			 * RFC 7761 Sec 4.5.3:
			 * The (S,G,rpt) downstream state machine on interface I
			 * remains in Prune state.  The Expiry Timer (ET) is
			 * restarted and is then set to the maximum of its
			 * current value and the HoldTime from the triggering
			 * Join/Prune message.
			 */
			if (ch->t_ifjoin_expiry_timer) {
				unsigned long rem = thread_timer_remain_second(
					ch->t_ifjoin_expiry_timer);

				if (rem > holdtime)
					return;
				THREAD_OFF(ch->t_ifjoin_expiry_timer);
			}

			thread_add_timer(router->master, on_ifjoin_expiry_timer,
					 ch, holdtime,
					 &ch->t_ifjoin_expiry_timer);
		}
		break;
	case PIM_IFJOIN_PRUNE_TMP:
		if (source_flags & PIM_ENCODE_RPT_BIT) {
			ch->ifjoin_state = PIM_IFJOIN_PRUNE;
			THREAD_OFF(ch->t_ifjoin_expiry_timer);
			thread_add_timer(router->master, on_ifjoin_expiry_timer,
					 ch, holdtime,
					 &ch->t_ifjoin_expiry_timer);
		}
		break;
	case PIM_IFJOIN_PRUNE_PENDING_TMP:
		if (source_flags & PIM_ENCODE_RPT_BIT) {
			ch->ifjoin_state = PIM_IFJOIN_PRUNE_PENDING;
			THREAD_OFF(ch->t_ifjoin_expiry_timer);
			thread_add_timer(router->master, on_ifjoin_expiry_timer,
					 ch, holdtime,
					 &ch->t_ifjoin_expiry_timer);
		}
		break;
	}
}

int pim_ifchannel_local_membership_add(struct interface *ifp,
				       struct prefix_sg *sg, bool is_vxlan)
{
	struct pim_ifchannel *ch, *starch;
	struct pim_interface *pim_ifp;
	struct pim_instance *pim;
	int up_flags;

	/* PIM enabled on interface? */
	pim_ifp = ifp->info;
	if (!pim_ifp) {
		if (PIM_DEBUG_EVENTS)
			zlog_debug("%s:%s Expected pim interface setup for %s",
				   __func__, pim_str_sg_dump(sg), ifp->name);
		return 0;
	}

	if (!PIM_IF_TEST_PIM(pim_ifp->options)) {
		if (PIM_DEBUG_EVENTS)
			zlog_debug(
				"%s:%s PIM is not configured on this interface %s",
				__func__, pim_str_sg_dump(sg), ifp->name);
		return 0;
	}

	pim = pim_ifp->pim;

	/* skip (*,G) ch creation if G is of type SSM */
	if (sg->src.ipaddr_v4.s_addr == INADDR_ANY) {
		if (pim_is_grp_ssm(pim, sg->grp.ipaddr_v4)) {
			if (PIM_DEBUG_PIM_EVENTS)
				zlog_debug(
					"%s: local membership (S,G)=%s ignored as group is SSM",
					__func__, pim_str_sg_dump(sg));
			return 1;
		}
	}

	/* vxlan term mroutes use ipmr-lo as local member to
	 * pull down multicast vxlan tunnel traffic
	 */
	up_flags = is_vxlan ? PIM_UPSTREAM_FLAG_MASK_SRC_VXLAN_TERM :
		PIM_UPSTREAM_FLAG_MASK_SRC_IGMP;
	ch = pim_ifchannel_add(ifp, sg, 0, up_flags);

	ifmembership_set(ch, PIM_IFMEMBERSHIP_INCLUDE);

	if (sg->src.ipaddr_v4.s_addr == INADDR_ANY) {
		struct pim_upstream *up = pim_upstream_find(pim, sg);
		struct pim_upstream *child;
		struct listnode *up_node;

		starch = ch;

		for (ALL_LIST_ELEMENTS_RO(up->sources, up_node, child)) {
			if (PIM_DEBUG_EVENTS)
				zlog_debug("%s %s: IGMP (S,G)=%s(%s) from %s",
					   __FILE__, __func__, child->sg_str,
					   ifp->name, up->sg_str);

			if (!child->rpf.source_nexthop.interface) {
				/* when iif unknown, do not inherit */
				if (PIM_DEBUG_EVENTS)
					zlog_debug(
						"Skipped (S,G)=%s(%s) from %s: no iif",
						child->sg_str, ifp->name,
						up->sg_str);
				continue;
			}

			ch = pim_ifchannel_find(ifp, &child->sg);
			if (pim_upstream_evaluate_join_desired_interface(
				    child, ch, starch)) {
				pim_channel_add_oif(child->channel_oil, ifp,
						    PIM_OIF_FLAG_PROTO_STAR,
							__func__);
				pim_upstream_update_join_desired(pim, child);
			}
		}

		if (pim->spt.switchover == PIM_SPT_INFINITY) {
			if (pim->spt.plist) {
				struct prefix_list *plist = prefix_list_lookup(
					AFI_IP, pim->spt.plist);
				struct prefix g;
				g.family = AF_INET;
				g.prefixlen = IPV4_MAX_BITLEN;
				g.u.prefix4 = up->sg.grp.ipaddr_v4;

				if (prefix_list_apply(plist, &g)
				    == PREFIX_DENY) {
					pim_channel_add_oif(
						up->channel_oil, pim->regiface,
						PIM_OIF_FLAG_PROTO_IGMP,
						__func__);
				}
			}
		} else
			pim_channel_add_oif(up->channel_oil, pim->regiface,
					PIM_OIF_FLAG_PROTO_IGMP,
					__func__);
	}

	return 1;
}

void pim_ifchannel_local_membership_del(struct interface *ifp,
					struct prefix_sg *sg)
{
	struct pim_ifchannel *starch, *ch, *orig;
	struct pim_interface *pim_ifp;

	/* PIM enabled on interface? */
	pim_ifp = ifp->info;
	if (!pim_ifp)
		return;
	if (!PIM_IF_TEST_PIM(pim_ifp->options))
		return;

	orig = ch = pim_ifchannel_find(ifp, sg);
	if (!ch)
		return;
	ifmembership_set(ch, PIM_IFMEMBERSHIP_NOINFO);

	if (sg->src.ipaddr_v4.s_addr == INADDR_ANY) {
		struct pim_upstream *up = pim_upstream_find(pim_ifp->pim, sg);
		struct pim_upstream *child;
		struct listnode *up_node, *up_nnode;

		starch = ch;

		for (ALL_LIST_ELEMENTS(up->sources, up_node, up_nnode, child)) {
			struct channel_oil *c_oil = child->channel_oil;
			struct pim_ifchannel *chchannel =
				pim_ifchannel_find(ifp, &child->sg);

			pim_ifp = ifp->info;

			if (PIM_DEBUG_EVENTS)
				zlog_debug("%s %s: Prune(S,G)=%s(%s) from %s",
					   __FILE__, __func__, up->sg_str,
					   ifp->name, child->sg_str);

			ch = pim_ifchannel_find(ifp, &child->sg);
			/*
			 * If the S,G has no if channel and the c_oil still
			 * has output here then the *,G was supplying the
			 * implied
			 * if channel.  So remove it.
			 */
			if (!pim_upstream_evaluate_join_desired_interface(
				child, ch, starch) ||
				(!chchannel &&
				 c_oil->oil.mfcc_ttls[pim_ifp->mroute_vif_index])) {
				pim_channel_del_inherited_oif(c_oil, ifp,
						__func__);
			}

			/* Child node removal/ref count-- will happen as part of
			 * parent' delete_no_info */
		}
	}

	/* Resettng the IGMP flags here */
	if (orig->upstream)
		PIM_UPSTREAM_FLAG_UNSET_SRC_IGMP(orig->upstream->flags);

	PIM_IF_FLAG_UNSET_PROTO_IGMP(orig->flags);

	delete_on_noinfo(orig);
}

void pim_ifchannel_update_could_assert(struct pim_ifchannel *ch)
{
	int old_couldassert =
		PIM_FORCE_BOOLEAN(PIM_IF_FLAG_TEST_COULD_ASSERT(ch->flags));
	int new_couldassert =
		PIM_FORCE_BOOLEAN(pim_macro_ch_could_assert_eval(ch));

	if (new_couldassert == old_couldassert)
		return;

	if (PIM_DEBUG_PIM_EVENTS) {
		char src_str[INET_ADDRSTRLEN];
		char grp_str[INET_ADDRSTRLEN];
		pim_inet4_dump("<src?>", ch->sg.src.ipaddr_v4, src_str,
			       sizeof(src_str));
		pim_inet4_dump("<grp?>", ch->sg.grp.ipaddr_v4, grp_str,
			       sizeof(grp_str));
		zlog_debug("%s: CouldAssert(%s,%s,%s) changed from %d to %d",
			   __func__, src_str, grp_str, ch->interface->name,
			   old_couldassert, new_couldassert);
	}

	if (new_couldassert) {
		/* CouldAssert(S,G,I) switched from false to true */
		PIM_IF_FLAG_SET_COULD_ASSERT(ch->flags);
	} else {
		/* CouldAssert(S,G,I) switched from true to false */
		PIM_IF_FLAG_UNSET_COULD_ASSERT(ch->flags);

		if (ch->ifassert_state == PIM_IFASSERT_I_AM_WINNER) {
			assert_action_a4(ch);
		}
	}

	pim_ifchannel_update_my_assert_metric(ch);
}

/*
  my_assert_metric may be affected by:

  CouldAssert(S,G)
  pim_ifp->primary_address
  rpf->source_nexthop.mrib_metric_preference;
  rpf->source_nexthop.mrib_route_metric;
 */
void pim_ifchannel_update_my_assert_metric(struct pim_ifchannel *ch)
{
	struct pim_assert_metric my_metric_new =
		pim_macro_ch_my_assert_metric_eval(ch);

	if (pim_assert_metric_match(&my_metric_new, &ch->ifassert_my_metric))
		return;

	if (PIM_DEBUG_PIM_EVENTS) {
		char src_str[INET_ADDRSTRLEN];
		char grp_str[INET_ADDRSTRLEN];
		char old_addr_str[INET_ADDRSTRLEN];
		char new_addr_str[INET_ADDRSTRLEN];
		pim_inet4_dump("<src?>", ch->sg.src.ipaddr_v4, src_str,
			       sizeof(src_str));
		pim_inet4_dump("<grp?>", ch->sg.grp.ipaddr_v4, grp_str,
			       sizeof(grp_str));
		pim_inet4_dump("<old_addr?>", ch->ifassert_my_metric.ip_address,
			       old_addr_str, sizeof(old_addr_str));
		pim_inet4_dump("<new_addr?>", my_metric_new.ip_address,
			       new_addr_str, sizeof(new_addr_str));
		zlog_debug(
			"%s: my_assert_metric(%s,%s,%s) changed from %u,%u,%u,%s to %u,%u,%u,%s",
			__func__, src_str, grp_str, ch->interface->name,
			ch->ifassert_my_metric.rpt_bit_flag,
			ch->ifassert_my_metric.metric_preference,
			ch->ifassert_my_metric.route_metric, old_addr_str,
			my_metric_new.rpt_bit_flag,
			my_metric_new.metric_preference,
			my_metric_new.route_metric, new_addr_str);
	}

	ch->ifassert_my_metric = my_metric_new;

	if (pim_assert_metric_better(&ch->ifassert_my_metric,
				     &ch->ifassert_winner_metric)) {
		assert_action_a5(ch);
	}
}

void pim_ifchannel_update_assert_tracking_desired(struct pim_ifchannel *ch)
{
	int old_atd = PIM_FORCE_BOOLEAN(
		PIM_IF_FLAG_TEST_ASSERT_TRACKING_DESIRED(ch->flags));
	int new_atd =
		PIM_FORCE_BOOLEAN(pim_macro_assert_tracking_desired_eval(ch));

	if (new_atd == old_atd)
		return;

	if (PIM_DEBUG_PIM_EVENTS) {
		char src_str[INET_ADDRSTRLEN];
		char grp_str[INET_ADDRSTRLEN];
		pim_inet4_dump("<src?>", ch->sg.src.ipaddr_v4, src_str,
			       sizeof(src_str));
		pim_inet4_dump("<grp?>", ch->sg.grp.ipaddr_v4, grp_str,
			       sizeof(grp_str));
		zlog_debug(
			"%s: AssertTrackingDesired(%s,%s,%s) changed from %d to %d",
			__func__, src_str, grp_str, ch->interface->name,
			old_atd, new_atd);
	}

	if (new_atd) {
		/* AssertTrackingDesired(S,G,I) switched from false to true */
		PIM_IF_FLAG_SET_ASSERT_TRACKING_DESIRED(ch->flags);
	} else {
		/* AssertTrackingDesired(S,G,I) switched from true to false */
		PIM_IF_FLAG_UNSET_ASSERT_TRACKING_DESIRED(ch->flags);

		if (ch->ifassert_state == PIM_IFASSERT_I_AM_LOSER) {
			assert_action_a5(ch);
		}
	}
}

/*
 * If we have a new pim interface, check to
 * see if any of the pre-existing channels have
 * their upstream out that way and turn on forwarding
 * for that ifchannel then.
 */
void pim_ifchannel_scan_forward_start(struct interface *new_ifp)
{
	struct pim_interface *new_pim_ifp = new_ifp->info;
	struct pim_instance *pim = new_pim_ifp->pim;
	struct interface *ifp;

	FOR_ALL_INTERFACES (pim->vrf, ifp) {
		struct pim_interface *loop_pim_ifp = ifp->info;
		struct pim_ifchannel *ch;

		if (!loop_pim_ifp)
			continue;

		if (new_pim_ifp == loop_pim_ifp)
			continue;

		RB_FOREACH (ch, pim_ifchannel_rb, &loop_pim_ifp->ifchannel_rb) {
			if (ch->ifjoin_state == PIM_IFJOIN_JOIN) {
				struct pim_upstream *up = ch->upstream;
				if ((!up->channel_oil)
				    && (up->rpf.source_nexthop
						.interface == new_ifp))
					pim_forward_start(ch);
			}
		}
	}
}

/*
 * Downstream per-interface (S,G,rpt) state machine
 * states that we need to move (S,G,rpt) items
 * into different states at the start of the
 * reception of a *,G join as well, when
 * we get End of Message
 */
void pim_ifchannel_set_star_g_join_state(struct pim_ifchannel *ch, int eom,
					 uint8_t join)
{
	bool send_upstream_starg = false;
	struct pim_ifchannel *child;
	struct listnode *ch_node, *nch_node;
	struct pim_instance *pim =
		((struct pim_interface *)ch->interface->info)->pim;
	struct pim_upstream *starup = ch->upstream;

	if (PIM_DEBUG_PIM_TRACE)
		zlog_debug(
			"%s: %s %s eom: %d join %u", __func__,
			pim_ifchannel_ifjoin_name(ch->ifjoin_state, ch->flags),
			ch->sg_str, eom, join);
	if (!ch->sources)
		return;

	for (ALL_LIST_ELEMENTS(ch->sources, ch_node, nch_node, child)) {
		if (!PIM_IF_FLAG_TEST_S_G_RPT(child->flags))
			continue;

		switch (child->ifjoin_state) {
		case PIM_IFJOIN_NOINFO:
		case PIM_IFJOIN_JOIN:
			break;
		case PIM_IFJOIN_PRUNE:
			if (!eom)
				child->ifjoin_state = PIM_IFJOIN_PRUNE_TMP;
			break;
		case PIM_IFJOIN_PRUNE_PENDING:
			if (!eom)
				child->ifjoin_state =
					PIM_IFJOIN_PRUNE_PENDING_TMP;
			break;
		case PIM_IFJOIN_PRUNE_TMP:
		case PIM_IFJOIN_PRUNE_PENDING_TMP:
			if (!eom)
				break;

			if (child->ifjoin_state == PIM_IFJOIN_PRUNE_PENDING_TMP)
				THREAD_OFF(child->t_ifjoin_prune_pending_timer);
			THREAD_OFF(child->t_ifjoin_expiry_timer);

			PIM_IF_FLAG_UNSET_S_G_RPT(child->flags);
			child->ifjoin_state = PIM_IFJOIN_NOINFO;

			if ((I_am_RP(pim, child->sg.grp.ipaddr_v4))
			    && (!pim_upstream_empty_inherited_olist(
				       child->upstream))) {
				pim_channel_add_oif(
					child->upstream->channel_oil,
					ch->interface, PIM_OIF_FLAG_PROTO_STAR,
					__func__);
				pim_upstream_update_join_desired(pim,
						child->upstream);
			}
			send_upstream_starg = true;

			delete_on_noinfo(child);
			break;
		}
	}

	if (send_upstream_starg)
		pim_jp_agg_single_upstream_send(&starup->rpf, starup, true);
}
