/*	$OpenBSD: if_pflow.c,v 1.9 2009/01/03 21:47:32 gollo Exp $	*/

/*
 * Copyright (c) 2008 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2008 Joerg Goltermann <jg@osn.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef __FreeBSD__
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_bpf.h"
#include "opt_pf.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef DEV_BPF
#define NBPFILTER       DEV_BPF
#else
#define NBPFILTER       0
#endif

#endif /* __FreeBSD__ */

#include <sys/types.h>
#ifdef __FreeBSD__
#include <sys/priv.h>
#endif
#include <sys/malloc.h>
#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#ifdef __FreeBSD__
#include <sys/module.h>
#include <sys/sockio.h>
#include <sys/taskqueue.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#else
#include <sys/ioctl.h>
#endif
#include <sys/kernel.h>
#include <sys/sysctl.h>
#ifndef __FreeBSD__
#include <dev/rndvar.h>
#endif

#include <net/if.h>
#ifdef __FreeBSD__
#include <net/if_clone.h>
#endif
#include <net/if_types.h>
#include <net/bpf.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/tcp.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/in_pcb.h>
#endif /* INET */

#include <net/pfvar.h>
#include <net/if_pflow.h>

#ifndef __FreeBSD__
#include "bpfilter.h"
#include "pflow.h"
#endif

#ifdef __FreeBSD__
#include <machine/in_cksum.h>
#endif

#define PFLOW_MINMTU	\
    (sizeof(struct pflow_header) + sizeof(struct pflow_flow))

#ifdef PFLOWDEBUG
#define DPRINTF(x)	do { printf x ; } while (0)
#else
#define DPRINTF(x)
#endif

SLIST_HEAD(, pflow_softc) pflowif_list;
struct pflowstats	 pflowstats;

void	pflowattach(int);
#ifdef __FreeBSD__
int     pflow_clone_create(struct if_clone *, int, caddr_t);
void    pflow_clone_destroy(struct ifnet *);
void	pflow_senddef(void *, int);
#else
int	pflow_clone_create(struct if_clone *, int);
int	pflow_clone_destroy(struct ifnet *);
#endif
void	pflow_setmtu(struct pflow_softc *, int);
int	pflowoutput(struct ifnet *, struct mbuf *, struct sockaddr *,
#ifdef __FreeBSD__
	    struct route *);
#else
	    struct rtentry *);
#endif
int	pflowioctl(struct ifnet *, u_long, caddr_t);
void	pflowstart(struct ifnet *);

struct mbuf *pflow_get_mbuf(struct pflow_softc *);
int	pflow_sendout(struct pflow_softc *);
int	pflow_sendout_mbuf(struct pflow_softc *, struct mbuf *);
void	pflow_timeout(void *);
void	copy_flow_data(struct pflow_flow *, struct pflow_flow *,
	struct pf_state *, int, int);
int	pflow_pack_flow(struct pf_state *, struct pflow_softc *);
int	pflow_get_dynport(void);
int	export_pflow_if(struct pf_state*, struct pflow_softc *);
int	copy_flow_to_m(struct pflow_flow *flow, struct pflow_softc *sc);

#ifdef __FreeBSD__
IFC_SIMPLE_DECLARE(pflow, 1);
#else
struct if_clone	pflow_cloner =
    IF_CLONE_INITIALIZER("pflow", pflow_clone_create,
    pflow_clone_destroy);
#endif

#ifndef __FreeBSD__
/* from in_pcb.c */
extern int ipport_hifirstauto;
extern int ipport_hilastauto;

/* from kern/kern_clock.c; incremented each clock tick. */
extern int ticks;
#endif

void
pflowattach(int npflow)
{
	SLIST_INIT(&pflowif_list);
	if_clone_attach(&pflow_cloner);
}

int
#ifdef __FreeBSD__
pflow_clone_create(struct if_clone *ifc, int unit, caddr_t param)
#else
pflow_clone_create(struct if_clone *ifc, int unit)
#endif
{
	struct ifnet		*ifp;
	struct pflow_softc	*pflowif;

	if ((pflowif = malloc(sizeof(*pflowif),
	    M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL)
		return (ENOMEM);

	pflowif->sc_sender_ip.s_addr = INADDR_ANY;
	pflowif->sc_sender_port = pflow_get_dynport();

#ifdef __FreeBSD__
        pflowif->sc_imo.imo_membership = (struct in_multi **)malloc(
            (sizeof(struct in_multi *) * IP_MIN_MEMBERSHIPS), M_DEVBUF,
            M_NOWAIT | M_ZERO);
        pflowif->sc_imo.imo_multicast_vif = -1;
#else
	pflowif->sc_imo.imo_membership = malloc(
	    (sizeof(struct in_multi *) * IP_MIN_MEMBERSHIPS), M_IPMOPTS,
	    M_WAITOK|M_ZERO);
#endif
	pflowif->sc_imo.imo_max_memberships = IP_MIN_MEMBERSHIPS;
	pflowif->sc_receiver_ip.s_addr = 0;
	pflowif->sc_receiver_port = 0;
	pflowif->sc_sender_ip.s_addr = INADDR_ANY;
	pflowif->sc_sender_port = pflow_get_dynport();
#ifdef __FreeBSD__
	ifp = pflowif->sc_ifp = if_alloc(IFT_PFLOW);
        if (ifp == NULL) {
                free(pflowif->sc_imo.imo_membership, M_DEVBUF);
                free(pflowif, M_DEVBUF);
                return (ENOSPC);
        }
        if_initname(ifp, ifc->ifc_name, unit);
#else
	ifp = &pflowif->sc_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "pflow%d", unit);
#endif
	ifp->if_softc = pflowif;
	ifp->if_ioctl = pflowioctl;
	ifp->if_output = pflowoutput;
	ifp->if_start = pflowstart;
	ifp->if_type = IFT_PFLOW;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
#ifdef __FreeBSD__
	mtx_init(&pflowif->sc_ifq.ifq_mtx, ifp->if_xname,
	    "pflow send queue", MTX_DEF);
	TASK_INIT(&pflowif->sc_send_task, 0, pflow_senddef, pflowif);
#endif
	ifp->if_hdrlen = PFLOW_HDRLEN;
	ifp->if_flags = IFF_UP;
#ifdef __FreeBSD__
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
#else
	ifp->if_flags &= ~IFF_RUNNING;	/* not running, need receiver */
#endif
	pflow_setmtu(pflowif, ETHERMTU);
#ifdef __FreeBSD__
	callout_init(&pflowif->sc_tmo, CALLOUT_MPSAFE);
#else
	timeout_set(&pflowif->sc_tmo, pflow_timeout, pflowif);
#endif
	if_attach(ifp);
#ifndef __FreeBSD__
	if_alloc_sadl(ifp);
#endif

#if NBPFILTER > 0
#ifdef __FreeBSD__
        bpfattach(ifp, DLT_RAW, 0);
#else
	bpfattach(&pflowif->sc_if.if_bpf, ifp, DLT_RAW, 0);
#endif
#endif
	/* Insert into list of pflows */
#ifdef __FreeBSD__
	PF_LOCK();
#endif
	SLIST_INSERT_HEAD(&pflowif_list, pflowif, sc_next);
#ifdef __FreeBSD__
        PF_UNLOCK();
#endif
	return (0);
}

#ifdef __FreeBSD__
void
#else
int
#endif
pflow_clone_destroy(struct ifnet *ifp)
{
	struct pflow_softc	*sc = ifp->if_softc;
	int			 s;

	s = splnet();
	pflow_sendout(sc);
#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	if_detach(ifp);
#ifdef __FreeBSD__
        PF_LOCK();
#endif
	SLIST_REMOVE(&pflowif_list, sc, pflow_softc, sc_next);
#ifdef __FreeBSD__
        PF_UNLOCK();
#endif
#ifdef __FreeBSD__
	free(sc->sc_imo.imo_membership, M_DEVBUF);
#else
	free(sc->sc_imo.imo_membership, M_IPMOPTS);
#endif
	free(sc, M_DEVBUF);
	splx(s);
#ifndef __FreeBSD__
	return (0);
#endif
}

/*
 * Start output on the pflow interface.
 */
void
pflowstart(struct ifnet *ifp)
{
	struct mbuf	*m;
	int		 s;

	for (;;) {
		s = splnet();
#ifdef __FreeBSD__
		IF_LOCK(&ifp->if_snd);
		_IF_DROP(&ifp->if_snd);
                _IF_DEQUEUE(&ifp->if_snd, m);
#else
		IF_DROP(&ifp->if_snd);
		IF_DEQUEUE(&ifp->if_snd, m);
#endif
#ifdef __FreeBSD__
		IF_UNLOCK(&ifp->if_snd);
#endif
		splx(s);

		if (m == NULL)
			return;
		m_freem(m);
	}
}

int
pflowoutput(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
#ifdef __FreeBSD__
	struct route *rt)
#else
	struct rtentry *rt)
#endif
{
	m_freem(m);
	return (0);
}

/* ARGSUSED */
int
pflowioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
#ifndef __FreeBSD__
	struct proc		*p = curproc;
#endif
	struct pflow_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *)data;
	struct pflowreq		 pflowr;
	int			 s, error;

	switch (cmd) {
	case SIOCSIFADDR:
	case SIOCAIFADDR:
	case SIOCSIFDSTADDR:
	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) &&
		    sc->sc_receiver_ip.s_addr != 0 &&
		    sc->sc_receiver_port != 0) {
#ifdef __FreeBSD__
			ifp->if_drv_flags |= IFF_DRV_RUNNING;
#else
			ifp->if_flags |= IFF_RUNNING;
#endif
			sc->sc_gcounter=pflowstats.pflow_flows;
		} else
#ifdef __FreeBSD__
                        ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
#else
			ifp->if_flags &= ~IFF_RUNNING;
#endif
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < PFLOW_MINMTU)
			return (EINVAL);
		if (ifr->ifr_mtu > MCLBYTES)
			ifr->ifr_mtu = MCLBYTES;
		s = splnet();
		if (ifr->ifr_mtu < ifp->if_mtu)
			pflow_sendout(sc);
		pflow_setmtu(sc, ifr->ifr_mtu);
		splx(s);
		break;

	case SIOCGETPFLOW:
		bzero(&pflowr, sizeof(pflowr));

		pflowr.sender_ip = sc->sc_sender_ip;
		pflowr.receiver_ip = sc->sc_receiver_ip;
		pflowr.receiver_port = sc->sc_receiver_port;

		if ((error = copyout(&pflowr, ifr->ifr_data,
		    sizeof(pflowr))))
			return (error);
		break;

	case SIOCSETPFLOW:
#ifdef __FreeBSD__
                if ((error = priv_check(curthread, PRIV_NETINET_PF)) != 0)
#else
		if ((error = suser(p, p->p_acflag)) != 0)
#endif
			return (error);
		if ((error = copyin(ifr->ifr_data, &pflowr,
		    sizeof(pflowr))))
			return (error);

		s = splnet();
		pflow_sendout(sc);
		splx(s);

		if (pflowr.addrmask & PFLOW_MASK_DSTIP)
			sc->sc_receiver_ip = pflowr.receiver_ip;
		if (pflowr.addrmask & PFLOW_MASK_DSTPRT)
			sc->sc_receiver_port = pflowr.receiver_port;
		if (pflowr.addrmask & PFLOW_MASK_SRCIP)
			sc->sc_sender_ip.s_addr = pflowr.sender_ip.s_addr;

		if ((ifp->if_flags & IFF_UP) &&
		    sc->sc_receiver_ip.s_addr != 0 &&
		    sc->sc_receiver_port != 0) {
#ifdef __FreeBSD__
                        ifp->if_drv_flags |= IFF_DRV_RUNNING;
#else
			ifp->if_flags |= IFF_RUNNING;
#endif
			sc->sc_gcounter=pflowstats.pflow_flows;
		} else
#ifdef __FreeBSD__
                        ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
#else
			ifp->if_flags &= ~IFF_RUNNING;
#endif

		break;

	default:
		return (ENOTTY);
	}
	return (0);
}

void
pflow_setmtu(struct pflow_softc *sc, int mtu_req)
{
	int	mtu;

	if (sc->sc_pflow_ifp && sc->sc_pflow_ifp->if_mtu < mtu_req)
		mtu = sc->sc_pflow_ifp->if_mtu;
	else
		mtu = mtu_req;

	sc->sc_maxcount = (mtu - sizeof(struct pflow_header) -
	    sizeof (struct udpiphdr)) / sizeof(struct pflow_flow);
	if (sc->sc_maxcount > PFLOW_MAXFLOWS)
	    sc->sc_maxcount = PFLOW_MAXFLOWS;
#ifdef __FreeBSD__
	sc->sc_ifp->if_mtu = sizeof(struct pflow_header) +
#else
	sc->sc_if.if_mtu = sizeof(struct pflow_header) +
#endif
	    sizeof (struct udpiphdr) + 
	    sc->sc_maxcount * sizeof(struct pflow_flow);
}

struct mbuf *
pflow_get_mbuf(struct pflow_softc *sc)
{
	struct pflow_header	 h;
	struct mbuf		*m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		pflowstats.pflow_onomem++;
		return (NULL);
	}

	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_free(m);
		pflowstats.pflow_onomem++;
		return (NULL);
	}

	m->m_len = m->m_pkthdr.len = 0;
	m->m_pkthdr.rcvif = NULL;

	/* populate pflow_header */
	h.reserved1 = 0;
	h.reserved2 = 0;
	h.count = 0;
	h.version = htons(PFLOW_VERSION);
	h.flow_sequence = htonl(sc->sc_gcounter);
	h.engine_type = PFLOW_ENGINE_TYPE;
	h.engine_id = PFLOW_ENGINE_ID;
#ifdef __FreeBSD__
	m_copyback(m, 0, PFLOW_HDRLEN, (caddr_t)&h);
#else
	m_copyback(m, 0, PFLOW_HDRLEN, &h);
#endif

	sc->sc_count = 0;
#ifdef __FreeBSD__
	callout_reset(&sc->sc_tmo, PFLOW_TIMEOUT * hz,
	    pflow_timeout, sc);
#else
	timeout_add_sec(&sc->sc_tmo, PFLOW_TIMEOUT);
#endif
	return (m);
}

void
copy_flow_data(struct pflow_flow *flow1, struct pflow_flow *flow2,
    struct pf_state *st, int src, int dst)
{
	struct pf_state_key	*sk = st->key[PF_SK_WIRE];

	flow1->src_ip = flow2->dest_ip = sk->addr[src].v4.s_addr;
	flow1->src_port = flow2->dest_port = sk->port[src];
	flow1->dest_ip = flow2->src_ip = sk->addr[dst].v4.s_addr;
	flow1->dest_port = flow2->src_port = sk->port[dst];

	flow1->dest_as = flow2->src_as =
	    flow1->src_as = flow2->dest_as = 0;
	flow1->if_index_out = flow2->if_index_in =
	    flow1->if_index_in = flow2->if_index_out = 0;
	flow1->dest_mask = flow2->src_mask =
	    flow1->src_mask = flow2->dest_mask = 0;

	flow1->flow_packets = htonl(st->packets[0]);
	flow2->flow_packets = htonl(st->packets[1]);
	flow1->flow_octets = htonl(st->bytes[0]);
	flow2->flow_octets = htonl(st->bytes[1]);

	flow1->flow_start = flow2->flow_start = htonl(st->creation * 1000);
	flow1->flow_finish = flow2->flow_finish = htonl(time_second * 1000);
	flow1->tcp_flags = flow2->tcp_flags = 0;
	flow1->protocol = flow2->protocol = sk->proto;
	flow1->tos = flow2->tos = st->rule.ptr->tos;
}

int
export_pflow(struct pf_state *st)
{
	struct pflow_softc	*sc = NULL;
	struct pf_state_key	*sk = st->key[PF_SK_WIRE];

	if (sk->af != AF_INET)
		return (0);

	SLIST_FOREACH(sc, &pflowif_list, sc_next) {
		export_pflow_if(st, sc);
	}

	return (0);
}

int
export_pflow_if(struct pf_state *st, struct pflow_softc *sc)
{
	struct pf_state		 pfs_copy;
#ifdef __FreeBSD__
	struct ifnet		*ifp = sc->sc_ifp;
#else
	struct ifnet		*ifp = &sc->sc_if;
#endif
	u_int64_t		 bytes[2];
	int			 ret = 0;

#ifdef __FreeBSD__
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
#else
	if (!(ifp->if_flags & IFF_RUNNING))
#endif
		return (0);

	if ((st->bytes[0] < (u_int64_t)PFLOW_MAXBYTES)
	    && (st->bytes[1] < (u_int64_t)PFLOW_MAXBYTES))
		return (pflow_pack_flow(st, sc));

	/* flow > PFLOW_MAXBYTES need special handling */
	bcopy(st, &pfs_copy, sizeof(pfs_copy));
	bytes[0] = pfs_copy.bytes[0];
	bytes[1] = pfs_copy.bytes[1];

	while (bytes[0] > PFLOW_MAXBYTES) {
		pfs_copy.bytes[0] = PFLOW_MAXBYTES;
		pfs_copy.bytes[1] = 0;

		if ((ret = pflow_pack_flow(&pfs_copy, sc)) != 0)
			return (ret);
		if ((bytes[0] - PFLOW_MAXBYTES) > 0)
			bytes[0] -= PFLOW_MAXBYTES;
	}

	while (bytes[1] > (u_int64_t)PFLOW_MAXBYTES) {
		pfs_copy.bytes[1] = PFLOW_MAXBYTES;
		pfs_copy.bytes[0] = 0;

		if ((ret = pflow_pack_flow(&pfs_copy, sc)) != 0)
			return (ret);
		if ((bytes[1] - PFLOW_MAXBYTES) > 0)
			bytes[1] -= PFLOW_MAXBYTES;
	}

	pfs_copy.bytes[0] = bytes[0];
	pfs_copy.bytes[1] = bytes[1];

	return (pflow_pack_flow(&pfs_copy, sc));
}

int
copy_flow_to_m(struct pflow_flow *flow, struct pflow_softc *sc)
{
	int		s, ret = 0;

	s = splnet();
	if (sc->sc_mbuf == NULL) {
		if ((sc->sc_mbuf = pflow_get_mbuf(sc)) == NULL) {
			splx(s);
			return (ENOBUFS);
		}
	}
	m_copyback(sc->sc_mbuf, PFLOW_HDRLEN +
	    (sc->sc_count * sizeof (struct pflow_flow)),
#ifdef __FreeBSD__
	    sizeof (struct pflow_flow), (caddr_t)flow);
#else
	    sizeof (struct pflow_flow), flow);
#endif

	if (pflowstats.pflow_flows == sc->sc_gcounter)
		pflowstats.pflow_flows++;
	sc->sc_gcounter++;
	sc->sc_count++;

	if (sc->sc_count >= sc->sc_maxcount)
		ret = pflow_sendout(sc);

	splx(s);
	return(ret);
}

int
pflow_pack_flow(struct pf_state *st, struct pflow_softc *sc)
{
	struct pflow_flow	 flow1;
	struct pflow_flow	 flow2;
	int			 ret = 0;

	bzero(&flow1, sizeof(flow1));
	bzero(&flow2, sizeof(flow2));

	if (st->direction == PF_OUT)
		copy_flow_data(&flow1, &flow2, st, 1, 0);
	else
		copy_flow_data(&flow1, &flow2, st, 0, 1);

	if (st->bytes[0] != 0) /* first flow from state */
		ret = copy_flow_to_m(&flow1, sc);

	if (st->bytes[1] != 0) /* second flow from state */
		ret = copy_flow_to_m(&flow2, sc);

	return (ret);
}

void
pflow_timeout(void *v)
{
	struct pflow_softc	*sc = v;
	int			 s;

	s = splnet();
	pflow_sendout(sc);
	splx(s);
}

/* This must be called in splnet() */
int
pflow_sendout(struct pflow_softc *sc)
{
	struct mbuf		*m = sc->sc_mbuf;
	struct pflow_header	*h;
#ifdef __FreeBSD__
	struct ifnet		*ifp = sc->sc_ifp;
#else
	struct ifnet		*ifp = &sc->sc_if;
#endif

#ifdef __FreeBSD__
	callout_stop(&sc->sc_tmo);
#else
	timeout_del(&sc->sc_tmo);
#endif

	if (m == NULL)
		return (0);

	sc->sc_mbuf = NULL;
#ifdef __FreeBSD__
        if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
#else
	if (!(ifp->if_flags & IFF_RUNNING)) {
#endif
		m_freem(m);
		return (0);
	}

	pflowstats.pflow_packets++;
	h = mtod(m, struct pflow_header *);
	h->count = htons(sc->sc_count);

	/* populate pflow_header */
	h->uptime_ms = htonl(time_uptime * 1000);
	h->time_sec = htonl(time_second);
	h->time_nanosec = htonl(ticks);

	return (pflow_sendout_mbuf(sc, m));
}

int
pflow_sendout_mbuf(struct pflow_softc *sc, struct mbuf *m)
{
	struct udpiphdr	*ui;
	u_int16_t	 len = m->m_pkthdr.len;
#ifdef __FreeBSD__
        struct ifnet            *ifp = sc->sc_ifp;
#else
	struct ifnet	*ifp = &sc->sc_if;
#endif
	struct ip	*ip;
#ifndef __FreeBSD__
	int		 err;
#endif

	/* UDP Header*/
	M_PREPEND(m, sizeof(struct udpiphdr), M_DONTWAIT);
	if (m == NULL) {
		pflowstats.pflow_onomem++;
		return (ENOBUFS);
	}

	ui = mtod(m, struct udpiphdr *);
	ui->ui_pr = IPPROTO_UDP;
	ui->ui_src = sc->sc_sender_ip;
	ui->ui_sport = sc->sc_sender_port;
	ui->ui_dst = sc->sc_receiver_ip;
	ui->ui_dport = sc->sc_receiver_port;
	ui->ui_ulen = htons(sizeof (struct udphdr) + len);

	ip = (struct ip *)ui;
	ip->ip_v = IPVERSION;
	ip->ip_hl = sizeof(struct ip) >> 2;
	ip->ip_id = htons(ip_randomid());
	ip->ip_off = htons(IP_DF);
	ip->ip_tos = IPTOS_LOWDELAY;
	ip->ip_ttl = IPDEFTTL;
	ip->ip_len = htons(sizeof (struct udpiphdr) + len);

	/*
	 * Compute the pseudo-header checksum; defer further checksumming
	 * until ip_output() or hardware (if it exists).
	 */
#ifndef __FreeBSD__
	/* XXX */
	m->m_pkthdr.csum_flags |= M_UDPV4_CSUM_OUT;
	ui->ui_sum = in_cksum_phdr(ui->ui_src.s_addr,
	    ui->ui_dst.s_addr, htons(len + sizeof(struct udphdr) +
	    IPPROTO_UDP));
#endif

#if NBPFILTER > 0
	if (ifp->if_bpf) {
		ip->ip_sum = in_cksum(m, ip->ip_hl << 2);
#ifdef __FreeBSD__
                BPF_MTAP(ifp, m);
#else
		bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
	}
#endif

#ifdef __FreeBSD__
	sc->sc_ifp->if_opackets++;
	sc->sc_ifp->if_obytes += m->m_pkthdr.len;
#else
	sc->sc_if.if_opackets++;
	sc->sc_if.if_obytes += m->m_pkthdr.len;
#endif

#ifdef __FreeBSD__
	if (!IF_HANDOFF(&sc->sc_ifq, m, NULL))
		pflowstats.pflow_oerrors++;
	taskqueue_enqueue(taskqueue_thread, &sc->sc_send_task);
#else
	if ((err = ip_output(m, NULL, NULL, IP_RAWOUTPUT, &sc->sc_imo, NULL))) {
		pflowstats.pflow_oerrors++;
		sc->sc_if.if_oerrors++;
	}
#endif
#ifdef __FreeBSD__
	return (0);
#else
	return (err);
#endif
}

int
pflow_get_dynport(void)
{
#ifdef __FreeBSD__
	u_int16_t	low, high, cut;
#else
	u_int16_t	tmp, low, high, cut;
#endif

#ifdef __FreeBSD__
	low = V_ipport_firstauto; /* sysctl */
        high = V_ipport_lastauto;
#else
	low = ipport_hifirstauto;     /* sysctl */
	high = ipport_hilastauto;
#endif

#ifdef __FreeBSD__
	cut = low + (arc4random() % (1 + high - low));
#else
	cut = arc4random_uniform(1 + high - low) + low;
#endif

#ifdef __FreeBSD__
	return (cut);
#else
	for (tmp = cut; tmp <= high; ++(tmp)) {
		if (!in_baddynamic(tmp, IPPROTO_UDP))
			return (htons(tmp));
	}

	for (tmp = cut - 1; tmp >= low; --(tmp)) {
		if (!in_baddynamic(tmp, IPPROTO_UDP))
			return (htons(tmp));
	}

	return (htons(ipport_hilastauto)); /* XXX */
#endif
}

#ifdef notyet
int
pflow_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen)
{
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case NET_PFLOW_STATS:
		if (newp != NULL)
			return (EPERM);
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    &pflowstats, sizeof(pflowstats)));
	default:
		return (EOPNOTSUPP);
	}
	return (0);
}
#endif

#ifdef __FreeBSD__
void
pflow_senddef(void *arg, __unused int pending)
{
	struct pflow_softc *sc = (struct pflow_softc *)arg;
	struct mbuf *m;

	for(;;) {
		IF_DEQUEUE(&sc->sc_ifq, m);
		if (m == NULL)
			break;
		if (ip_output(m, NULL, NULL, IP_RAWOUTPUT, &sc->sc_imo, NULL)) {
			pflowstats.pflow_oerrors++;
			sc->sc_ifp->if_oerrors++;
		}
	}
}

static int
pflow_modevent(module_t mod, int type, void *data)
{
        int error = 0;

        switch (type) {
        case MOD_LOAD:
                pflowattach(0);
		export_pflow_ptr = export_pflow;
                break;
        case MOD_UNLOAD:
                if_clone_detach(&pflow_cloner);
		export_pflow_ptr = NULL;
                break;
        default:
                error = EINVAL;
                break;
        }

        return error;
}

static moduledata_t pflow_mod = {
        "pflow",
        pflow_modevent,
        0
};

#define PFLOW_MODVER 1

DECLARE_MODULE(pflow, pflow_mod, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY);
MODULE_VERSION(pflow, PFLOW_MODVER);
MODULE_DEPEND(pflow, pf, PF_MODVER, PF_MODVER, PF_MODVER);
#endif /* __FreeBSD__ */
