/*
 * V9FS cache definitions.
 *
 *  Copyright (C) 2009 by Abhishek Kulkarni <adkulkar@umail.iu.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 *  Free Software Foundation
 *  51 Franklin Street, Fifth Floor
 *  Boston, MA  02111-1301  USA
 *
 */

#ifndef _V9FS_CACHE_H
#ifdef CONFIG_9P_FSCACHE
#include <linux/fscache.h>

struct v9fs_cookie {
	spinlock_t lock;
	struct inode inode;
	struct fscache_cookie *fscache;
};

struct fscache_netfs v9fs_cache_netfs = {
	.name 		= "9p",
	.version 	= 0,
};

struct fscache_cookie_def v9fs_cache_session_index_def = {
	.name 		= "9P.session",
	.type 		= FSCACHE_COOKIE_TYPE_INDEX,
	.get_key 	= v9fs_cache_session_get_key,
};

struct fscache_cookie_def v9fs_cache_inode_index_def = {
	.name		= "9p.inode",
	.type		= FSCACHE_COOKIE_TYPE_DATAFILE,
	.get_key	= v9fs_cache_inode_get_key,
	.get_attr	= v9fs_cache_inode_get_attr,
	.get_aux	= v9fs_cache_inode_get_aux,
	.check_aux	= v9fs_cache_inode_check_aux,
	.now_uncached	= v9fs_cache_inode_now_uncached,
};

static inline struct v9fs_cookie *v9fs_inode2cookie(const struct inode *inode)
{
	return container_of(inode, struct v9fs_cookie, inode);
}

/**
 * v9fs_cache_register - Register v9fs file system with the cache
 */
static inline int v9fs_cache_register(void)
{
	return __v9fs_cache_register();
}

/**
 * v9fs_cache_unregister - Unregister v9fs from the cache
 */
static inline void v9fs_cache_unregister(void)
{
	__v9fs_cache_unregister();
}

static inline int v9fs_readpage_from_fscache(struct inode *inode,
					     struct page *page)
{
	return __v9fs_readpage_from_fscache(inode, page);
}

static inline int v9fs_readpages_from_fscache(struct inode *inode,
					      struct address_space *mapping,
					      struct list_head *pages
					      unsigned *nr_pages)
{
	return __v9fs_readpages_from_fscache(inode, mapping, pages,
					     nr_pages);
}

static inline void v9fs_readpage_to_fscache(struct inode *inode,
					    struct page *page)
{
	if (PageFsCache(page))
		__v9fs_readpage_to_fscache(inode, page);
}

static inline void v9fs_uncache_page(struct inode *inode, struct page *page)
{
	struct v9fs_cookie *vcookie = v9fs_inode2cookie(inode);
	fscache_uncache_page(vcookie->fscache, page);
	BUG_ON(PageFsCache(page));
}

#else /* CONFIG_9P_FSCACHE */
static inline int v9fs_cache_register(void)
{
	return 0;
}

static inline void v9fs_cache_unregister(void) {}

static inline int v9fs_readpage_from_fscache(struct inode *inode,
					     struct page *page)
{
	return -ENOBUFS;
}

static inline int v9fs_readpages_from_fscache(struct inode *inode,
					      struct address_space *mapping,
					      struct list_head *pages,
					      unsigned *nr_pages)
{
	return -ENOBUFS;
}

static inline void v9fs_readpage_to_fscache(struct inode *inode,
					    struct page *page)
{}

static inline void v9fs_uncache_page(struct inode *inode, struct page *page)
{}


#endif /* CONFIG_9P_FSCACHE */
#endif /* _V9FS_CACHE_H */
