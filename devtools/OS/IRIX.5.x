#	$Id: IRIX.5.x,v 8.16 2002/03/21 23:59:25 gshapiro Exp $

dnl	DO NOT EDIT THIS FILE.
dnl	Place personal settings in devtools/Site/site.config.m4

define(`confCC', `cc -mips2 -OPT:Olimit=1400')
define(`confMAPDEF', `-DNDBM -DNIS')
define(`confLIBS', `-lmld -lmalloc')
define(`confSM_OS_HEADER', `sm_os_irix')
define(`confMBINDIR', `/usr/lib')
define(`confSBINDIR', `/usr/etc')
define(`confUBINDIR', `/usr/bsd')
define(`confEBINDIR', `/usr/lib')
define(`confSBINGRP', `sys')
define(`confSTDIR', `/var')
define(`confINSTALL', `${BUILDBIN}/install.sh')
define(`confDEPEND_TYPE', `CC-M')
