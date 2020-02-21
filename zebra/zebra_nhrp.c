/**
 * zebra_nhrp.c: nhrp 6wind detector file
 *
 * Copyright 2020 6WIND S.A.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; see the file COPYING; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <zebra.h>

#ifdef HAVE_NETNS
#undef _GNU_SOURCE
#define _GNU_SOURCE

#include <sched.h>
#endif

#include "json.h"
#include "lib/version.h"
#include "hook.h"
#include "memory.h"
#include "hash.h"
#include "libfrr.h"
#include "command.h"
#include "vty.h"
#include "jhash.h"
#include "ns.h"
#include "vrf.h"
#include "log.h"
#include "resolver.h"
#include <string.h>

#include "zebra/rib.h"
#include "zebra/zapi_msg.h"
#include "zebra/interface.h"
#include "zebra/zebra_router.h"
#include "zebra/debug.h"
#include "zebra/zebra_vrf.h"

#ifndef VTYSH_EXTRACT_PL
#include "zebra/zebra_nhrp_clippy.c"
#endif

/* control socket */
struct zebra_nhrp_header {
	uint32_t iface_idx;
	uint16_t packet_length; /* size of the whole packet */
	uint16_t strip_size;    /* size of the non copy data */
	uint16_t protocol_type;
	uint16_t vrfid;
}zebra_nhrp_header_t;

#define ZEBRA_GRE_NHRP_6WIND_PORT 36344
#define ZEBRA_GRE_NHRP_6WIND_ADDRESS "127.0.0.1"

#define ZEBRA_GRE_NHRP_6WIND_RCV_BUF 500

DEFINE_MTYPE_STATIC(ZEBRA, ZEBRA_NHRP, "Gre Nhrp Notify Information");

/* api routines */
static int zebra_nhrp_6wind_init(struct thread_master *t);
static int zebra_nhrp_6wind_write_config(struct vty *vty);
static int zebra_nhrp_6wind_write_config_iface(struct vty *vty, struct interface *ifp);
static int zebra_nhrp_6wind_nflog_configure(int nflog_group, struct zebra_vrf *zvrf);
static int zebra_nhrp_6wind_if_delete_hook(struct interface *ifp);
static int zebra_nhrp_6wind_if_new_hook(struct interface *ifp);

/* internal */
static void zebra_nhrp_configure(bool nhrp_6wind, bool is_ipv4,
				 bool on, struct interface *ifp,
				 int nflog_group);
static int zebra_nhrp_6wind_connection(bool on, uint16_t port);
/* vty */
#define GRE_NHRP_STR	  "Nhrp Notification Mecanism\n"
#define GRE_NHRP_6WIND_STR "Nhrp 6wind fast-path notification\n"
#define IP_STR		"IP information\n"
#define IPV6_STR	"IPv6 information\n"
#define AFI_STR		IP_STR IPV6_STR

#ifndef CLONE_NEWNET
#define CLONE_NEWNET 0x40000000
/* New network namespace (lo, device, names sockets, etc) */
#endif

#ifndef HAVE_SETNS
static inline int setns(int fd, int nstype)
{
#ifdef __NR_setns
	return syscall(__NR_setns, fd, nstype);
#else
	errno = EINVAL;
	return -1;
#endif
}
#endif /* !HAVE_SETNS */

static afi_t cmd_to_afi(const char *tok)
{
	return strcmp(tok, "ipv6") == 0 ? AFI_IP6 : AFI_IP;
}

static const char *afi_to_cmd(afi_t afi)
{
	if (afi == AFI_IP6)
		return "ipv6";
	return "ip";
}

struct hash *zebra_nhrp_list;
static int zebra_nhrp_6wind_port;
static int zebra_nhrp_6wind_fd;
static struct thread *zebra_nhrp_log_thread;

struct zebra_nhrp_ctx {
	struct interface *ifp; /* backpointer and key */
	bool nhrp_6wind_notify[AFI_MAX];
	bool nflog_notify[AFI_MAX];
	int nflog_group;
};

static uint32_t zebra_nhrp_hash_key(const void *arg)
{
	const struct zebra_nhrp_ctx *ctx = arg;

	return jhash(&ctx->ifp->name, sizeof(ctx->ifp->name), 0);
}

static bool zebra_nhrp_hash_cmp(const void *n1, const void *n2)
{
	const struct zebra_nhrp_ctx *a1 = n1;
	const struct zebra_nhrp_ctx *a2 = n2;

	if (a1->ifp != a2->ifp)
		return false;
	return true;
}

static void zebra_nhrp_list_init(void)
{
	if (!zebra_nhrp_list)
		zebra_nhrp_list = hash_create_size(8, zebra_nhrp_hash_key,
						   zebra_nhrp_hash_cmp,
						   "Nhrp Hash");
	return;
}

static void zebra_nhrp_flush_entry(struct zebra_nhrp_ctx *ctx)
{
	afi_t afi;

	for (afi = 0; afi < AFI_MAX; afi++) {
		if (ctx->nhrp_6wind_notify[afi]) {
			zebra_nhrp_configure(true, afi == AFI_IP ? true : false,
					     false, ctx->ifp, ctx->nflog_group);
			ctx->nhrp_6wind_notify[afi] = false;
		}
		if (ctx->nflog_notify[afi]) {
			zebra_nhrp_configure(false, afi == AFI_IP ? true : false,
					     false, ctx->ifp, ctx->nflog_group);
			ctx->nflog_notify[afi] = false;
		}
	}
}

static void zebra_nhrp_list_remove(struct hash_bucket *backet, void *ctxt)
{
	struct zebra_nhrp_ctx *ctx;

	ctx = (struct zebra_nhrp_ctx *)backet->data;
	if (!ctx)
		return;
	zebra_nhrp_flush_entry(ctx);
	hash_release(zebra_nhrp_list, ctx);
	XFREE(MTYPE_ZEBRA_NHRP, ctx);
}

static int zebra_nhrp_6wind_end(void)
{
	if (!zebra_nhrp_list)
		return 0;

	zebra_nhrp_6wind_connection(false, (uint16_t)0);

	hash_iterate(zebra_nhrp_list,
		     zebra_nhrp_list_remove, NULL);
	hash_clean(zebra_nhrp_list, NULL);
	return 1;
}

static int zebra_nhrp_6wind_module_init(void)
{
	hook_register(frr_late_init, zebra_nhrp_6wind_init);
	hook_register(zebra_nflog_configure,
		      zebra_nhrp_6wind_nflog_configure);
	hook_register(zebra_if_config_wr,
		      zebra_nhrp_6wind_write_config_iface);
	hook_register(zebra_vty_config_write,
		      zebra_nhrp_6wind_write_config);
	hook_register(if_add, zebra_nhrp_6wind_if_new_hook);
	hook_register(if_del, zebra_nhrp_6wind_if_delete_hook);
	hook_register(frr_fini, zebra_nhrp_6wind_end);
	return 0;
}

FRR_MODULE_SETUP(
		 .name = "zebra_gre_nhrp_6wind",
		 .version = FRR_VERSION,
		 .description = "gre nhrp 6wind module",
		 .init = zebra_nhrp_6wind_module_init
		 );

static struct zebra_nhrp_ctx *zebra_nhrp_lookup(struct interface *ifp)
{
	struct zebra_nhrp_ctx ctx;

	memset(&ctx, 0, sizeof(struct zebra_nhrp_ctx));
	ctx.ifp = ifp;
	return hash_lookup(zebra_nhrp_list, &ctx);
}

static void *zebra_nhrp_alloc(void *arg)
{
	void *ctx_to_allocate;

	ctx_to_allocate = XCALLOC(MTYPE_ZEBRA_NHRP,
				  sizeof(struct zebra_nhrp_ctx));
	if (!ctx_to_allocate)
		return NULL;
	memcpy(ctx_to_allocate, arg, sizeof(struct zebra_nhrp_ctx));
	return ctx_to_allocate;
}

struct zebra_vrf_nflog_ctx {
	vrf_id_t vrf_id;
	int nflog_group;
};

static void zebra_nhrp_update_nfgroup(int nflog_group,
				      struct zebra_nhrp_ctx *ctxt)
{
	afi_t afi;

	if (nflog_group == ctxt->nflog_group)
		return;
	for (afi = 0; afi < AFI_MAX; afi++) {
		/* suppress */
		if (ctxt->nflog_notify[afi])
			zebra_nhrp_configure(false, afi == AFI_IP ? true : false,
					     false, ctxt->ifp, ctxt->nflog_group);
	}
	ctxt->nflog_group = nflog_group;
	if (!nflog_group)
		return;
	for (afi = 0; afi < AFI_MAX; afi++) {
		/* readd */
		if (ctxt->nflog_notify[afi])
			zebra_nhrp_configure(false, afi == AFI_IP ? true : false,
					     true, ctxt->ifp, ctxt->nflog_group);
	}
}

static int zebra_nhrp_6wind_nflog_walker(struct hash_bucket *b, void *data)
{
	struct zebra_vrf_nflog_ctx *nflog = (struct zebra_vrf_nflog_ctx *)data;
	struct zebra_nhrp_ctx *ctxt = (struct zebra_nhrp_ctx *)b->data;

	if (!ctxt->ifp || !nflog)
		return HASHWALK_CONTINUE;
	if (ctxt->ifp->vrf_id != nflog->vrf_id)
		return HASHWALK_CONTINUE;
	/* update nflog group */
	if (ctxt->nflog_group == nflog->nflog_group)
		return HASHWALK_CONTINUE;
	zebra_nhrp_update_nfgroup(nflog->nflog_group,
				  ctxt);
	return HASHWALK_CONTINUE;
}

static int zebra_nhrp_6wind_nflog_configure(int nflog_group,
					    struct zebra_vrf *zvrf)
{
	struct zebra_vrf_nflog_ctx ctx;

	if (!zvrf->vrf)
		return 0;

	ctx.vrf_id = zvrf->vrf->vrf_id;
	ctx.nflog_group = nflog_group;

	hash_walk(zebra_nhrp_list, zebra_nhrp_6wind_nflog_walker, &ctx);
	return 1;
}

static int zebra_nhrp_6wind_write_config(struct vty *vty)
{
	if (zebra_nhrp_6wind_port) {
		vty_out(vty, "nhrp 6wind %u\n", zebra_nhrp_6wind_port);
		return 1;
	}
	return 0;
}

static int zebra_nhrp_6wind_write_config_iface(struct vty *vty, struct interface *ifp)
{
	struct zebra_nhrp_ctx *ctx;
	const char *aficmd;
	int ret = 0;
	afi_t afi;

	ctx = zebra_nhrp_lookup(ifp);
	if (!ctx)
		return ret;
	for (afi = 0; afi < AFI_MAX; afi++) {
		aficmd = afi_to_cmd(afi);

		if (ctx->nhrp_6wind_notify[afi]) {
			vty_out(vty, " %s nhrp 6wind\n",
				aficmd);
			ret++;
		}
		if (ctx->nflog_notify[afi]) {
			vty_out(vty, " %s nhrp nflog\n", aficmd);
			ret++;
		}
	}
	return ret;
}

static int zebra_nhrp_6wind_if_new_hook(struct interface *ifp)
{
	struct zebra_nhrp_ctx ctx;
	int i;

	memset(&ctx, 0, sizeof(struct zebra_nhrp_ctx));
	ctx.ifp = ifp;
	for (i = 0; i < AFI_MAX; i++) {
		ctx.nhrp_6wind_notify[i] = false;
		ctx.nflog_notify[i] = false;
	}
	zebra_nhrp_list_init();
	hash_get(zebra_nhrp_list, &ctx,
		 zebra_nhrp_alloc);
	return 1;
}

static int zebra_nhrp_6wind_if_delete_hook(struct interface *ifp)
{
	struct zebra_nhrp_ctx *ctx;

	ctx = zebra_nhrp_lookup(ifp);
	if (!ctx)
		return 0;

	zebra_nhrp_flush_entry(ctx);
	hash_release(zebra_nhrp_list, ctx);
	XFREE(MTYPE_ZEBRA_NHRP, ctx);
	return 0;
}

static int zebra_nhrp_6wind_log_recv(struct thread *t)
{
	int fd = THREAD_FD(t);
	char buf[ZEBRA_GRE_NHRP_6WIND_RCV_BUF];
	unsigned int len;
	struct zebra_nhrp_header *ctxt;
	ifindex_t iface_idx;
	uint32_t packet_length;
	uint32_t strip_size;
	uint32_t protocol_type;
	uint8_t *data;
	vrf_id_t vrf_id;
	struct interface *ifp;

	zebra_nhrp_log_thread = NULL;
	thread_add_read(zrouter.master, zebra_nhrp_6wind_log_recv,
			NULL, fd,
			&zebra_nhrp_log_thread);

	len = read(fd, buf, ZEBRA_GRE_NHRP_6WIND_RCV_BUF);
	if (len <= 0) {
		zlog_err("%s(): len negative. retry", __func__);
		return 0;
	}
	ctxt = (struct zebra_nhrp_header *)buf;
	packet_length = ntohs(ctxt->packet_length);
	strip_size = ntohs(ctxt->strip_size);
	if (len != sizeof(struct zebra_nhrp_header) + packet_length - strip_size) {
		zlog_err("%s(): %u bytes received on nhrp 6wind port, expected %u",
			 __func__, len,
			 (unsigned int)(sizeof(struct zebra_nhrp_header) +
					packet_length - strip_size));
		return 0;
	}
	iface_idx = (ifindex_t)ntohl(ctxt->iface_idx);
	vrf_id = (vrf_id_t)(ctxt->vrfid);
	protocol_type = ntohs(ctxt->protocol_type);
	data = (uint8_t *)(ctxt + 1);
	ifp = if_lookup_by_index(iface_idx, vrf_id);
	if (!ifp) {
		zlog_err("%s(): unknown interface idx %u vrf_id %u",
			 __func__, iface_idx, vrf_id);
		return 0;
	}
	zsend_nflog_notify(ZEBRA_NFLOG_TRAFFIC_INDICATION, ifp,
			   protocol_type, data,
			   packet_length - strip_size);
	return 0;
}

static int zebra_nhrp_6wind_configure_listen_port(uint16_t port)
{
	struct sockaddr_in srvaddr;
	int ret = 0, flags, fd, orig;
	int rcvbuf;
	socklen_t rcvbufsz;

	if (zebra_nhrp_6wind_fd >= 0) {
		THREAD_OFF(zebra_nhrp_log_thread);
		close(zebra_nhrp_6wind_fd);
		zebra_nhrp_6wind_fd = -1;
	}
	if (!port)
		return 0;

	frr_with_privs(&zserv_privs) {
		/* try to open fd fo fast-path */
		fd = open("/var/run/fast-path/namespaces/net", O_RDONLY | O_CLOEXEC);
		orig = ns_lookup(NS_DEFAULT)->fd;
	}
	if (fd < 0 || orig < 0) {
		zlog_err("%s(): netns fast-path (%d) or self vrf (%d) could not be read",
			   __func__, fd, orig);
		if (fd > 0)
			close(fd);
		return -1;
	}
	frr_with_privs(&zserv_privs) {
		ret = setns(fd, CLONE_NEWNET);
	}
	if (ret >= 0) {
		frr_with_privs(&zserv_privs) {
			zebra_nhrp_6wind_fd = socket(AF_INET, SOCK_DGRAM,
						     IPPROTO_UDP);
		}
	} else {
		zlog_err("%s(): setns(%u, CLONE_NEWNET) failed: %s",
			 __func__, fd, strerror(errno));
		close(fd);
		return -1;
	}
	frr_with_privs(&zserv_privs) {
		ret = setns(orig, CLONE_NEWNET);
	}
	if (ret < 0) {
		zlog_err("%s(): setns(%u, CLONE_NEWNET) failed: %s",
			   __func__, orig, strerror(errno));
		close(fd);
		return -1;
	}
	if (zebra_nhrp_6wind_fd < 0) {
		close(fd);
		return -1;
	}
	/* set the socket to non-blocking */
	frr_with_privs(&zserv_privs) {
		flags = fcntl(zebra_nhrp_6wind_fd, F_GETFL);
		flags |= O_NONBLOCK;
		ret = fcntl(zebra_nhrp_6wind_fd, F_SETFL, flags);
	}
	if (ret < 0) {
		zlog_err("%s(): fcntl(O_NONBLOCK) failed: %s", __func__, strerror(errno));
		close(zebra_nhrp_6wind_fd);
		close(fd);
		return -1;
	}
	frr_with_privs(&zserv_privs) {
		flags = fcntl(zebra_nhrp_6wind_fd, F_GETFD);
		flags |= FD_CLOEXEC;
		ret = fcntl(zebra_nhrp_6wind_fd, F_SETFD, flags);
	}
	if (ret < 0) {
		zlog_err("%s(): fcntl(F_SETFD CLOEXEC) failed: %s",
			 __func__, strerror(errno));
		close(zebra_nhrp_6wind_fd);
		close(fd);
		return -1;
	}
	memset(&srvaddr, 0, sizeof(srvaddr));
	srvaddr.sin_family = AF_INET;
	srvaddr.sin_port = htons(port);
	srvaddr.sin_addr.s_addr = inet_addr(ZEBRA_GRE_NHRP_6WIND_ADDRESS);

	frr_with_privs(&zserv_privs) {
		ret = setns(fd, CLONE_NEWNET);
		if (ret >= 0) {
			ret = bind(zebra_nhrp_6wind_fd, &srvaddr, sizeof(srvaddr));
			if (ret < 0) {
				zlog_err("%s(): bind(%u, 127.0.0.1) failed : %s",
					 __func__, zebra_nhrp_6wind_fd, strerror(errno));
			}
		}
		ret = setns(orig, CLONE_NEWNET);
	}
	if (ret < 0) {
		zlog_err("%s(): setns(%u, CLONE_NEWNET) failed: %s",
			 __func__, orig, strerror(errno));
		close(zebra_nhrp_6wind_fd);
		close(fd);
		return -1;
	}
	rcvbuf = 0;
	rcvbufsz = sizeof(rcvbuf);
	ret = getsockopt(zebra_nhrp_6wind_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &rcvbufsz);
	if (ret < 0) {
		zlog_err("%s(): getsockopt(RCVBUF) failed: %s", __func__,
			 strerror(errno));
		close(zebra_nhrp_6wind_fd);
		close(fd);
		return -1;
	}
	if (rcvbuf < ZEBRA_GRE_NHRP_6WIND_RCV_BUF) {
		rcvbuf = ZEBRA_GRE_NHRP_6WIND_RCV_BUF;
		ret = setsockopt(zebra_nhrp_6wind_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, rcvbufsz);
		if (ret < 0) {
			zlog_err("%s(): getsockopt(RCVBUF) failed: %s", __func__,
				 strerror(errno));
			close(zebra_nhrp_6wind_fd);
			close(fd);
			return -1;
		}
		ret = getsockopt(zebra_nhrp_6wind_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &rcvbufsz);
		if (ret < 0) {
			zlog_err("%s(): getsockopt(RCVBUF) failed: %s", __func__,
				 strerror(errno));
			close(zebra_nhrp_6wind_fd);
			close(fd);
			return -1;
		}
	}
	thread_add_read(zrouter.master, zebra_nhrp_6wind_log_recv,
			NULL,
			zebra_nhrp_6wind_fd,
			&zebra_nhrp_log_thread);
	close(fd);
	return ret;
}

static int zebra_nhrp_call_only(const char *script, vrf_id_t vrf_id,
				char *buf_response, int len_buf)
{
	FILE *fp;
	char *current_str = NULL;

	if (IS_ZEBRA_DEBUG_KERNEL_MSGDUMP_SEND)
		zlog_debug("NHRP : %s", script);

	vrf_switch_to_netns(vrf_id);

	fp = popen(script, "r");

	if (!fp) {
		zlog_err("NHRP: error calling %s", script);
		vrf_switchback_to_initial();
		return -1;
	}
	if (buf_response) {
		do {
			current_str = fgets(buf_response, len_buf, fp);
		} while (current_str != NULL);
	}
	vrf_switchback_to_initial();

	pclose(fp);

	return 0;
}

static int zebra_nhrp_6wind_connection(bool on, uint16_t port)
{
	char buf[100];
	int ret = 0;

	if (!on)
		ret = zebra_nhrp_6wind_configure_listen_port(0);
	else
		ret = zebra_nhrp_6wind_configure_listen_port(port);
	if (ret < 0)
		return ret;

	/* fp-cli nhrp-port <port> <vrfid> */
	snprintf(buf, sizeof(buf), "/usr/bin/fp-cli nhrp-port %d",
		 on ? port : 0);

	zebra_nhrp_call_only(buf, VRF_DEFAULT, NULL, 0);
	return 0;
}

static void zebra_nhrp_configure(bool nhrp_6wind, bool is_ipv4,
				 bool on, struct interface *ifp,
				 int nflog_group)
{
	char buf[500], buf2[100], buf3[110], buf4_ipv4[100], buf4_ipv6[100], buf5_vrf[55];
	struct vrf *vrf = NULL;
	char buf_vrf[1000];

	memset(buf5_vrf, 0, sizeof(buf5_vrf));
	/* iptables : /sbin/iptables  -A FORWARD -i gre5 -o gre5 -j NFLOG
	 *      --nflog-group 6 --nflog-threshold 10
	 * ip6tables : /sbin/iptables  -A FORWARD -i gre5 -o gre5 -j NFLOG
	 *      --nflog-group 6 --nflog-threshold 10
	 */
	vrf = vrf_lookup_by_id(ifp->vrf_id);
	if (!vrf)
		return;
	if (!nhrp_6wind) {
		snprintf(buf3, sizeof(buf3), " %s%s%s",
			 "-m hashlimit --hashlimit-name nflog",
			 ifp->name,
			 " --hashlimit-upto 4/minute --hashlimit-burst 1");
		snprintf(buf4_ipv4, sizeof(buf4_ipv4), "%s %s",
			 " --hashlimit-mode srcip,dstip --hashlimit-srcmask 24",
			 "--hashlimit-dstmask 24");
		snprintf(buf4_ipv6, sizeof(buf4_ipv6), "%s %s",
			 " --hashlimit-mode srcip,dstip --hashlimit-srcmask 64",
			 "--hashlimit-dstmask 64");
		snprintf(buf2, sizeof(buf2), "--nflog-threshold 10");

		if (vrf->vrf_id != VRF_DEFAULT)
			snprintf(buf5_vrf, sizeof(buf5_vrf), "ip netns exec %s ", vrf->name);
		snprintf(buf, sizeof(buf), "%s%s %s FORWARD -i %s -o %s %s %u %s%s%s",
			 buf5_vrf,
			 is_ipv4 ? "/sbin/iptables" : "/sbin/ip6tables",
			 on ? "-A" : "-D",
			 ifp->name, ifp->name,
			 "-j NFLOG --nflog-group",
			 nflog_group,
			 buf2, buf3, is_ipv4 ? buf4_ipv4 : buf4_ipv6);
	} else {
		uint32_t vrid = 0;

		if (vrf->vrf_id != VRF_DEFAULT) {
			snprintf(buf, sizeof(buf), "/usr/bin/vrfctl list vrfname %s",
				 vrf->name);
			memset(buf_vrf, 0, sizeof(buf_vrf));
			zebra_nhrp_call_only(buf, ifp->vrf_id, buf_vrf, sizeof(buf_vrf));
			if (memcmp(buf_vrf, "vrf", 3) == 0)
				vrid = atoi(&buf_vrf[3]);
			else {
				zlog_err("%s(): could not retrieve id from vrf %s (%s)",
					 __func__, vrf->name, buf_vrf);
				return;
			}
		}
		snprintf(buf, sizeof(buf), "/usr/bin/fp-cli nhrp-iface-set %s %s %s %u",
			 ifp->name,
			 is_ipv4 ? "ipv4" : "ipv6",
			 on ? "on" : "off",
			 vrid);
	}
	zebra_nhrp_call_only(buf, ifp->vrf_id, NULL, 0);
}

DEFPY (iface_nhrp_6wind_onoff,
       iface_nhrp_6wind_onoff_cmd,
       "[no$no] <ip$ipv4|ipv6$ipv6> nhrp 6wind",
       NO_STR
       AFI_STR
       GRE_NHRP_STR
       GRE_NHRP_6WIND_STR)
{
	struct zebra_nhrp_ctx *ctx;
	VTY_DECLVAR_CONTEXT(interface, ifp);
	bool action_to_set = true;
	afi_t afi;

	if (ipv4)
		afi = cmd_to_afi(ipv4);
	else
		afi = cmd_to_afi(ipv6);
	ctx = zebra_nhrp_lookup(ifp);
	if (!ctx)
		return CMD_WARNING;
	if (no) {
		action_to_set = false;
	}
	if (action_to_set == ctx->nhrp_6wind_notify[afi])
		return CMD_SUCCESS;
	ctx->nhrp_6wind_notify[afi] = action_to_set;
	zebra_nhrp_configure(true, ipv4 ? true : false,
			     action_to_set, ctx->ifp,
			     ctx->nflog_group);
	return CMD_SUCCESS;
}

DEFPY (iface_nflog_onoff,
       iface_nflog_onoff_cmd,
       "[no$no] <ip$ipv4|ipv6$ipv6> nhrp nflog",
       NO_STR
       AFI_STR
       GRE_NHRP_STR
       "Netfilter log notification\n")
{
	struct zebra_nhrp_ctx *ctx;
	VTY_DECLVAR_CONTEXT(interface, ifp);
	bool action_to_set = true;
	afi_t afi;

	if (ipv4)
		afi = cmd_to_afi(ipv4);
	else
		afi = cmd_to_afi(ipv6);

	ctx = zebra_nhrp_lookup(ifp);
	if (!ctx)
		return CMD_WARNING;
	if (no)
		action_to_set = false;
	if (action_to_set == ctx->nflog_notify[afi])
		return CMD_SUCCESS;
	ctx->nflog_notify[afi] = action_to_set;
	/* call */
	zebra_nhrp_configure(false, ipv4 ? true : false, action_to_set,
			     ifp, ctx->nflog_group);
	return CMD_SUCCESS;
}

DEFPY(zebra_nhrp_6wind_connect, zebra_nhrp_6wind_connect_cmd,
      "[no$no] nhrp 6wind [(1-65535)$port]",
      NO_STR
      GRE_NHRP_STR
      GRE_NHRP_6WIND_STR
      "Port number to connect to\n")
{
	int ret;

	if (no) {
		/* close */
		zebra_nhrp_6wind_connection(false, (uint16_t)0);
		zebra_nhrp_6wind_port = 0;
		return CMD_SUCCESS;
	}
	if (port == zebra_nhrp_6wind_port)
		return CMD_SUCCESS;
	ret = zebra_nhrp_6wind_connection(true, (uint16_t)port);
	if (ret < 0) {
		vty_out(vty, "Failed to connect to Fast-Path with port %ld\r\n", port);
		return CMD_WARNING;
	}
	zebra_nhrp_6wind_port = port;
	return CMD_SUCCESS;
}

static int zebra_nhrp_6wind_init(struct thread_master *t)
{
	zebra_nhrp_list_init();
	zebra_nhrp_6wind_fd = -1;
	install_element(INTERFACE_NODE, &iface_nflog_onoff_cmd);
	install_element(INTERFACE_NODE, &iface_nhrp_6wind_onoff_cmd);
	install_element(CONFIG_NODE, &zebra_nhrp_6wind_connect_cmd);
	return 0;
}
