/*
 * Copyright (c) 1996 Poul-Henning Kamp
 * Copyright (c) 1994 Ugen J.S.Antsilevich
 * Idea and grammar partially left from:
 * Copyright (c) 1993 Daniel Boulet
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 * NEW command line interface for IP firewall facility
 *
 * $Id: ipfw.c,v 1.19 1996/02/23 15:52:28 phk Exp $
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <paths.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <kvm.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#define IPFIREWALL
#include <netinet/ip_fw.h>

#define MAXSTR	256

int 	lineno = -1;
char 		progname[MAXSTR];		/* Program name for errors */

int 		s;				/* main RAW socket 	   */
int 		do_resolv=0;			/* Would try to resolv all */
int		do_acct=0;			/* Show packet/byte count  */

int
mask_bits(m_ad)
	struct in_addr m_ad;
{
	int h_fnd=0,h_num=0,i;
	u_long mask;

	mask=ntohl(m_ad.s_addr);
	for (i=0;i<sizeof(u_long)*CHAR_BIT;i++) {
		if (mask & 1L) {
			h_fnd=1;
			h_num++;
		} else {
			if (h_fnd)
				return -1;
		}
		mask=mask>>1;
	}
	return h_num;
}                         

void
show_ipfw(chain)
	struct ip_fw *chain;
{
	char *comma;
	u_long adrt;
	struct hostent *he;
	int i,mb;


	printf("%05u ", chain->fw_number);

	if (do_acct) 
		printf("%10lu %10lu ",chain->fw_pcnt,chain->fw_bcnt);

	if (chain->fw_flg & IP_FW_F_ACCEPT)
		printf("accept");
	else if (chain->fw_flg & IP_FW_F_ICMPRPL)
		printf("reject");
	else if (chain->fw_flg & IP_FW_F_COUNT)
		printf("count");
	else
		printf("deny");
	if (chain->fw_flg & IP_FW_F_PRN)
		printf(" log");

	switch (chain->fw_flg & IP_FW_F_KIND) {
		case IP_FW_F_ICMP:
			printf(" icmp ");
			break;
		case IP_FW_F_TCP:
			printf(" tcp ");
			break;
		case IP_FW_F_UDP:
			printf(" udp ");
			break;
		case IP_FW_F_ALL:
			printf(" all ");
			break;
		default:
			break;
	}

	printf("from ");

	adrt=ntohl(chain->fw_smsk.s_addr);
	if (adrt==ULONG_MAX && do_resolv) {
		adrt=(chain->fw_src.s_addr);
		he=gethostbyaddr((char *)&adrt,sizeof(u_long),AF_INET);
		if (he==NULL) {
			printf(inet_ntoa(chain->fw_src));
		} else
			printf("%s",he->h_name);
	} else {
		if (adrt!=ULONG_MAX) {
			mb=mask_bits(chain->fw_smsk);
			if (mb == 0) {
				printf("any");
			} else {
				if (mb > 0) {
					printf(inet_ntoa(chain->fw_src));
					printf("/%d",mb);
				} else {
					printf(inet_ntoa(chain->fw_src));
					printf(":");
					printf(inet_ntoa(chain->fw_smsk));
				}
			}
		} else
			printf(inet_ntoa(chain->fw_src));
	}

	comma = " ";
	for (i=0;i<chain->fw_nsp; i++ ) {
		printf("%s%d",comma,chain->fw_pts[i]);
		if (i==0 && (chain->fw_flg & IP_FW_F_SRNG))
			comma = "-";
		else
			comma = ",";
	}

	printf(" to ");

	adrt=ntohl(chain->fw_dmsk.s_addr);
	if (adrt==ULONG_MAX && do_resolv) {
		adrt=(chain->fw_dst.s_addr);
		he=gethostbyaddr((char *)&adrt,sizeof(u_long),AF_INET);
		if (he==NULL) {
			printf(inet_ntoa(chain->fw_dst));
		} else
			printf("%s",he->h_name);
	} else {
		if (adrt!=ULONG_MAX) {
			mb=mask_bits(chain->fw_dmsk);
			if (mb == 0) {
				printf("any");
			} else {
				if (mb > 0) {
					printf(inet_ntoa(chain->fw_dst));
					printf("/%d",mb);
				} else {
					printf(inet_ntoa(chain->fw_dst));
					printf(":");
					printf(inet_ntoa(chain->fw_dmsk));
				}
			}
		} else
			printf(inet_ntoa(chain->fw_dst));
	}

	comma = " ";
	for (i=0;i<chain->fw_ndp;i++) {
		printf("%s%d",comma,chain->fw_pts[chain->fw_nsp+i]);
		if (i==chain->fw_nsp && (chain->fw_flg & IP_FW_F_DRNG))
			comma = "-";
		else
		    comma = ",";
	    }

	if ((chain->fw_flg & IP_FW_F_IN) && (chain->fw_flg & IP_FW_F_OUT))
		; 
	else if (chain->fw_flg & IP_FW_F_IN)
		printf(" in ");
	else if (chain->fw_flg & IP_FW_F_OUT)
		printf(" out ");

	if (chain->fw_flg&IP_FW_F_IFNAME && chain->fw_via_name[0]) {
		char ifnb[FW_IFNLEN+1];
		printf(" via ");
		strncpy(ifnb,chain->fw_via_name,FW_IFNLEN);
		ifnb[FW_IFNLEN]='\0';
		printf("%s%d",ifnb,chain->fw_via_unit);
	} else if (chain->fw_via_ip.s_addr) {
		printf(" via ");
		printf(inet_ntoa(chain->fw_via_ip));
	}

	if (chain->fw_flg & IP_FW_F_FRAG)
		printf("frag ");

	if (chain->fw_ipopt || chain->fw_ipnopt) {
		int 	_opt_printed = 0;
#define PRINTOPT(x)	{if (_opt_printed) printf(",");\
			printf(x); _opt_printed = 1;}

		printf(" ipopt ");
		if (chain->fw_ipopt  & IP_FW_IPOPT_SSRR) PRINTOPT("ssrr");
		if (chain->fw_ipnopt & IP_FW_IPOPT_SSRR) PRINTOPT("!ssrr");
		if (chain->fw_ipopt  & IP_FW_IPOPT_LSRR) PRINTOPT("lsrr");
		if (chain->fw_ipnopt & IP_FW_IPOPT_LSRR) PRINTOPT("!lsrr");
		if (chain->fw_ipopt  & IP_FW_IPOPT_RR)   PRINTOPT("rr");
		if (chain->fw_ipnopt & IP_FW_IPOPT_RR)   PRINTOPT("!rr");
		if (chain->fw_ipopt  & IP_FW_IPOPT_TS)   PRINTOPT("ts");
		if (chain->fw_ipnopt & IP_FW_IPOPT_TS)   PRINTOPT("!ts");
	} 

	if (chain->fw_tcpf == IP_FW_TCPF_SYN &&
	    chain->fw_tcpnf == IP_FW_TCPF_ACK)
		printf(" established");
	else if (chain->fw_tcpf || chain->fw_tcpnf) {
		int 	_flg_printed = 0;
#define PRINTFLG(x)	{if (_flg_printed) printf(",");\
			printf(x); _flg_printed = 1;}

		printf(" tcpflg ");
		if (chain->fw_tcpf  & IP_FW_TCPF_FIN)  PRINTFLG("fin");
		if (chain->fw_tcpnf & IP_FW_TCPF_FIN)  PRINTFLG("!fin");
		if (chain->fw_tcpf  & IP_FW_TCPF_SYN)  PRINTFLG("syn");
		if (chain->fw_tcpnf & IP_FW_TCPF_SYN)  PRINTFLG("!syn");
		if (chain->fw_tcpf  & IP_FW_TCPF_RST)  PRINTFLG("rst");
		if (chain->fw_tcpnf & IP_FW_TCPF_RST)  PRINTFLG("!rst");
		if (chain->fw_tcpf  & IP_FW_TCPF_PSH)  PRINTFLG("psh");
		if (chain->fw_tcpnf & IP_FW_TCPF_PSH)  PRINTFLG("!psh");
		if (chain->fw_tcpf  & IP_FW_TCPF_ACK)  PRINTFLG("ack");
		if (chain->fw_tcpnf & IP_FW_TCPF_ACK)  PRINTFLG("!ack");
		if (chain->fw_tcpf  & IP_FW_TCPF_URG)  PRINTFLG("urg");
		if (chain->fw_tcpnf & IP_FW_TCPF_URG)  PRINTFLG("!urg");
	} 
	printf("\n");
}

struct nlist nlf[]={ { "_ip_fw_chain" } };

void
list(ac, av)
	int	ac;
	char 	**av;
{
	kvm_t *kd;
	static char errb[_POSIX2_LINE_MAX];
	struct ip_fw b;
	struct ip_fw_chain *fcp,fc;

	if (!(kd=kvm_openfiles(NULL,NULL,NULL,O_RDONLY,errb))) {
     		fprintf(stderr,"%s: kvm_openfiles: %s\n",
					progname,kvm_geterr(kd));
     		exit(1);
	}

	if (kvm_nlist(kd,nlf)<0 || nlf[0].n_type==0) {
		fprintf(stderr,"%s: kvm_nlist: no namelist in %s\n",
						progname,getbootfile());
      		exit(1);
    	}

	kvm_read(kd,(u_long)nlf[0].n_value,&fcp,sizeof fcp);
	printf("FireWall chain entries:\n");
	while(fcp!=NULL) {
		kvm_read(kd,(u_long)fcp,&fc,sizeof fc);
		kvm_read(kd,(u_long)fc.rule,&b,sizeof b);
		show_ipfw(&b);
		fcp = fc.chain.le_next;
	}
}

void
show_usage(str)
	char	*str;
{
	if (str)
		fprintf(stderr,"%s: ERROR - %s\n",progname,str);
	else
		fprintf(stderr,"%s: ERROR - bad arguments\n",progname);
	fprintf(stderr,
"Usage:\n"
"\t%s [options]\n"
"\t\tflush\n"
"\t\tadd [number] rule\n"
"\t\tdelete number\n"
"\t\tlist [number]\n"
"\t\tzero [number]\n"
"\trule:\taction proto src dst extras...\n"
"\t\taction: {allow|deny|reject|count}[,log]\n"
"\t\tproto: {ip|tcp|udp|icmp}}\n"
"\t\tsrc: {any|ip[{/bits|:mask}]} [{port|port-port},...]\n"
"\t\tdst: {any|ip[{/bits|:mask}]} [{port|port-port},...]\n"
"\textras:\n"
"\t\tfragment\n"
"\t\t{in|out|inout}\n"
"\t\tvia {ifname|ip}\n"
"\t\ttcpflags [!]{syn|fin|rst|ack|psh},...\n"
, progname
);

		
	fprintf(stderr,"See man %s(8) for proper usage.\n",progname);
	exit (1);
}

void
fill_ip(ipno, mask, acp, avp)
	struct in_addr *ipno, *mask;
	int *acp;
	char ***avp;
{
	int ac = *acp;
	char **av = *avp;
	char *p = 0, md = 0;

	if (ac && !strncmp(*av,"any",strlen(*av))) {
		ipno->s_addr = mask->s_addr = 0; av++; ac--;
	} else {
		p = strchr(*av, '/');
		if (!p) 
			p = strchr(*av, ':');
		if (p) {
			md = *p;
			*p++ = '\0'; 
		}

		if (!inet_aton(*av,ipno))
			show_usage("ip number\n");
		if (md == ':' && !inet_aton(p,mask))
			show_usage("ip number\n");
		else if (md == '/') 
			mask->s_addr = htonl(0xffffffff << (32 - atoi(p)));
		else 
			mask->s_addr = htonl(0xffffffff);
		av++;
		ac--;
	}
	*acp = ac;
	*avp = av;
}

int
fill_port(cnt, ptr, off, av)
	u_short *cnt, *ptr, off;
	char **av;
{
	char *s, sc = 0;
	int i = 0;

	s = strchr(*av,'-');
	if (s) {
		sc = *s;
		*s++ = '\0';
		ptr[off+*cnt] = atoi(*av);
		(*cnt)++;
		*av = s;
		s = strchr(*av,',');
		if (s) {
			sc = *s;
			*s++ = '\0';
		} else
			sc = '\0';
		ptr[off+*cnt] = atoi(*av);
		(*cnt)++;
		if (sc && sc != ',') show_usage("Expected comma\n");
		*av = s;
		sc = 0;
		i = 1;
	}
	while (1) {
		s = strchr(*av,',');
		if (s) {
			sc = *s;
			*s++ = '\0';
		} else
			sc = '\0';
		ptr[off+*cnt] = atoi(*av);
		(*cnt)++;
		if (!sc)
			break;
		if (sc != ',') show_usage("Expected comma\n");
		*av = s;
	}
	return i;
}

void
fill_tcpflag(set, reset, vp)
	u_char *set, *reset;
	char **vp;
{
	char *p = *vp,*q;
	u_char *d;

	while (p && *p) {
		if (*p == '!') {
			p++;
			d = reset;
		} else {
			d = set;
		}
		q = strchr(p, ',');
		if (q) 
			*q++ = '\0';
		if (!strncmp(p,"syn",strlen(p))) *d |= IP_FW_TCPF_SYN;
		if (!strncmp(p,"fin",strlen(p))) *d |= IP_FW_TCPF_FIN;
		if (!strncmp(p,"ack",strlen(p))) *d |= IP_FW_TCPF_ACK;
		if (!strncmp(p,"psh",strlen(p))) *d |= IP_FW_TCPF_PSH;
		if (!strncmp(p,"rst",strlen(p))) *d |= IP_FW_TCPF_RST;
		p = q;
	}
}

void
fill_ipopt(set, reset, vp)
	u_char *set, *reset;
	char **vp;
{
	char *p = *vp,*q;
	u_char *d;

	while (p && *p) {
		if (*p == '!') {
			p++;
			d = reset;
		} else {
			d = set;
		}
		q = strchr(p, ',');
		if (q) 
			*q++ = '\0';
		if (!strncmp(p,"ssrr",strlen(p))) *d |= IP_FW_IPOPT_SSRR;
		if (!strncmp(p,"lsrr",strlen(p))) *d |= IP_FW_IPOPT_LSRR;
		if (!strncmp(p,"rr",strlen(p)))   *d |= IP_FW_IPOPT_RR;
		if (!strncmp(p,"ts",strlen(p)))   *d |= IP_FW_IPOPT_TS;
		p = q;
	}
}

void
delete(ac,av)
	int ac;
	char **av;
{
	struct ip_fw rule;
	int i;
	
	memset(&rule, 0, sizeof rule);

	av++; ac--;

	/* Rule number */
	if (ac && isdigit(**av)) {
		rule.fw_number = atoi(*av); av++; ac--;
	}

	i = setsockopt(s, IPPROTO_IP, IP_FW_DEL, &rule, sizeof rule);
	if (i)
		err(1,"setsockopt(Add)");
}

void
add(ac,av)
	int ac;
	char **av;
{
	struct ip_fw rule;
	int i;
	
	memset(&rule, 0, sizeof rule);

	av++; ac--;

	/* Rule number */
	if (ac && isdigit(**av)) {
		rule.fw_number = atoi(*av); av++; ac--;
	}

	/* Action */
	if (ac && !strncmp(*av,"accept",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_ACCEPT; av++; ac--;
	} else if (ac && !strncmp(*av,"pass",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_ACCEPT; av++; ac--;
	} else if (ac && !strncmp(*av,"count",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_COUNT; av++; ac--;
	} else if (ac && !strncmp(*av,"deny",strlen(*av))) {
		av++; ac--;
	} else if (ac && !strncmp(*av,"reject",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_ICMPRPL; av++; ac--;
	} else {
		show_usage("missing action\n");
	}

	/* [log] */
	if (ac && !strncmp(*av,"log",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_PRN; av++; ac--;
	}

	/* [protocol] */
	if (ac && !strncmp(*av,"protocol",strlen(*av))) { av++; ac--; }

	/* protocol */
	if (ac && !strncmp(*av,"ip",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_ALL; av++; ac--;
	} else if (ac && !strncmp(*av,"all",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_ALL; av++; ac--;
	} else if (ac && !strncmp(*av,"tcp",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_TCP; av++; ac--;
	} else if (ac && !strncmp(*av,"udp",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_UDP; av++; ac--;
	} else if (ac && !strncmp(*av,"icmp",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_ICMP; av++; ac--;
	} else {
		show_usage("missing protocol\n");
	}

	/* from */
	if (ac && !strncmp(*av,"from",strlen(*av))) { av++; ac--; }
	else show_usage("missing ``from''\n");

	fill_ip(&rule.fw_src, &rule.fw_smsk, &ac, &av);

	if (ac && isdigit(**av)) {
		if (fill_port(&rule.fw_nsp, &rule.fw_pts, 0, av))
			rule.fw_flg |= IP_FW_F_SRNG;
		av++; ac--;
	}

	/* to */
	if (ac && !strncmp(*av,"to",strlen(*av))) { av++; ac--; }
	else show_usage("missing ``to''\n");

	if (!ac) show_usage("Missing arguments\n");

	fill_ip(&rule.fw_dst, &rule.fw_dmsk, &ac, &av);

	if (ac && isdigit(**av)) {
		if (fill_port(&rule.fw_ndp, &rule.fw_pts, rule.fw_nsp, av))
			rule.fw_flg |= IP_FW_F_DRNG;
		av++; ac--;
	}

	while (ac) {
		if (!strncmp(*av,"frag",strlen(*av))) { 
			rule.fw_flg |= IP_FW_F_FRAG; av++; ac--; continue;
		}
		if (!strncmp(*av,"in",strlen(*av))) { 
			rule.fw_flg |= IP_FW_F_IN; av++; ac--; continue;
		}
		if (!strncmp(*av,"out",strlen(*av))) { 
			rule.fw_flg |= IP_FW_F_OUT; av++; ac--; continue;
		}
		if (!strncmp(*av,"established",strlen(*av))) { 
			rule.fw_tcpf  |= IP_FW_TCPF_SYN;
			rule.fw_tcpnf |= IP_FW_TCPF_ACK;
			av++; ac--; continue;
		}
		if (ac > 1 && !strncmp(*av,"tcpflags",strlen(*av))) { 
			av++; ac--; 
			fill_tcpflag(&rule.fw_tcpf, &rule.fw_tcpnf, av);
			av++; ac--; continue;
		}
		if (ac > 1 && !strncmp(*av,"ipoptions",strlen(*av))) { 
			av++; ac--; 
			fill_ipopt(&rule.fw_ipopt, &rule.fw_ipnopt, av);
			av++; ac--; continue;
		}
		printf("%d %s\n",ac,*av);
		show_usage("Unknown argument\n");
	}

	show_ipfw(&rule);
	i = setsockopt(s, IPPROTO_IP, IP_FW_ADD, &rule, sizeof rule);
	if (i)
		err(1,"setsockopt(Delete)");
}

int
ipfw_main(ac,av)
	int 	ac;
	char 	**av;
{

	char 		ch;
	extern int 	optind;


	if ( ac == 1 ) {
		show_usage(NULL);
	}

	while ((ch = getopt(ac, av ,"an")) != EOF)
	switch(ch) {
		case 'a':
			do_acct=1;
			break;
		case 'n':
	 		do_resolv=1;
        		break;
        	case '?':
         	default:
            		show_usage(NULL);
	}

	ac -= optind;
	if (*(av+=optind)==NULL) {
		 show_usage(NULL);
	}

	if (!strncmp(*av, "add", strlen(*av))) {
		add(ac,av);
	} else if (!strncmp(*av, "delete", strlen(*av))) {
		delete(ac,av);
	} else if (!strncmp(*av, "flush", strlen(*av))) {
		if (setsockopt(s,IPPROTO_IP,IP_FW_FLUSH,NULL,0)<0) {
			fprintf(stderr,"%s: setsockopt failed.\n",progname);
			exit(1);
		} 
		printf("Flushed all rules.\n");
	} else if (!strncmp(*av, "zero", strlen(*av))) {
		if (setsockopt(s,IPPROTO_IP,IP_FW_ZERO,NULL,0)<0) {
			fprintf(stderr,"%s: setsockopt failed.\n",progname);
			exit(1);
		} 
		printf("Accounting cleared.\n");
	} else if (!strncmp(*av, "print", strlen(*av))) {
		list(--ac,++av);
	} else if (!strncmp(*av, "list", strlen(*av))) {
		list(--ac,++av);
	} else {
		show_usage(NULL);
	}
        return 0;
}

int 
main(ac, av)
	int	ac;
	char	**av;
{
#define MAX_ARGS	32
	char	buf[_POSIX_ARG_MAX];
	char	*args[MAX_ARGS];
	char	linename[10];
	int 	i;
	FILE	*f;

	s = socket( AF_INET, SOCK_RAW, IPPROTO_RAW );
	if ( s < 0 ) {
		fprintf(stderr,"%s: Can't open raw socket.\n"
			"Must be root to use this programm. \n",progname);
		exit(1);
	}

	setbuf(stdout,0);

	strcpy(progname,*av);

	if (av[1] && !access(av[1], R_OK)) {
		lineno = 0;
		f = fopen(av[1], "r");
		while (fgets(buf, _POSIX_ARG_MAX, f)) {
			if (buf[strlen(buf)-1]=='\n')
				buf[strlen(buf)-1] = 0;

			lineno++;
			sprintf(linename, "Line %d", lineno);
			args[0] = linename;

			args[1] = buf;
			while(*args[1] == ' ')
				args[1]++;
			i = 2;
			while((args[i] = strchr(args[i-1],' '))) {
				*(args[i]++) = 0;
				while(*args[i] == ' ')
					args[i]++;
				i++;
			}
			if (*args[i-1] == 0)
				i--;
			args[i] = NULL;

			ipfw_main(i, args); 
		}
		fclose(f);
	} else
		ipfw_main(ac,av);
	return 0;
}
