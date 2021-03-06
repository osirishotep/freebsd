/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_SYS_DMU_TRAVERSE_H
#define	_SYS_DMU_TRAVERSE_H

#include <sys/zfs_context.h>
#include <sys/spa.h>
#include <sys/zio.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct dnode_phys;
struct dsl_dataset;

typedef int (blkptr_cb_t)(spa_t *spa, blkptr_t *bp,
    const zbookmark_t *zb, const struct dnode_phys *dnp, void *arg);

#define	TRAVERSE_PRE			(1<<0)
#define	TRAVERSE_POST			(1<<1)
#define	TRAVERSE_PREFETCH_METADATA	(1<<2)
#define	TRAVERSE_PREFETCH_DATA		(1<<3)
#define	TRAVERSE_PREFETCH (TRAVERSE_PREFETCH_METADATA | TRAVERSE_PREFETCH_DATA)

int traverse_dataset(struct dsl_dataset *ds, uint64_t txg_start,
    int flags, blkptr_cb_t func, void *arg);
int traverse_pool(spa_t *spa, blkptr_cb_t func, void *arg);

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DMU_TRAVERSE_H */
