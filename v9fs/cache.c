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

#include <linux/jiffies.h>
#include "cache.h"

#define CACHETAG_LEN  11

static struct kmem_cache *vcookie_cache;

static void init_once(void *foo)
{
	struct v9fs_cookie *vcookie = (struct v9fs_cookie *) foo;
	vcookie->fscache = NULL;
	inode_init_once(&vcookie->inode);
}

static int v9fs_init_vcookiecache(void)
{
	vcookie_cache = kmem_cache_create("vcookie_cache",
					  sizeof(struct v9fs_cookie),
					  0, (SLAB_RECLAIM_ACCOUNT|
					      SLAB_MEM_SPREAD),
					  init_once);
	if (!vcookie_cache)
		return -ENOMEM;

	return 0;
}

static void v9fs_destroy_vcookiecache(void)
{
	kmem_cache_destroy(vcookie_cache);
}

int __v9fs_cache_register(void)
{
	int ret;
	ret = v9fs_init_vcookiecache();
	if (ret < 0)
		return ret;

	return fscache_register_netfs(&v9fs_cache_netfs);
}

void __v9fs_cache_unregister(void)
{
	v9fs_destroy_vcookiecache();
	fscache_unregister_netfs(&v9fs_cache_netfs);
}

void v9fs_cache_session_get_cookie(struct v9fs_session_info *v9ses)
{
	v9ses->fscache = fscache_acquire_cookie(v9fs_cache_netfs.primary_index,
						&v9fs_cache_session_index_def,
						v9ses);
	P9_DPRINTK(P9_DEBUG_FSC, "session %p got cookie %p", v9ses,
		   v9ses->fscache);
}

void v9fs_cache_session_put_cookie(struct v9fs_session_info *v9ses)
{
	P9_DPRINTK(P9_DEBUG_FSC, "session %p put cookie %p", v9ses,
		   v9ses->fscache);
	fscache_relinquish_cookie(v9ses->fscache, 0);
	v9ses->fscache = NULL;
}

/**
 * TODO: Need a way to export cachetags to the userspace.
 */

static
int v9fs_random_cachetag(struct v9fs_session_info *v9ses)
{
	v9ses->cachetag = kmalloc(CACHETAG_LEN, GFP_KERNEL);
	if (!v9ses->cachetag)
		return -ENOMEM;

	return scnprintf(v9ses->cachetag, CACHETAG_LEN, "%lu", jiffies);
}

uint16_t v9fs_cache_session_get_key(const void *cookie_netfs_data,
				    void *buffer, uint16_t bufmax)
{
	const struct v9fs_session_info *v9ses = cookie_netfs_data;
	uint16_t klen;

	P9_DPRINTK(P9_DEBUG_FSC, "session %p buf %p size %u", v9ses, 
		   buffer, bufmax);

	/* If no cache session tag was specified, we generate a random one. */
	if (!v9ses->cachetag) {
		klen = v9fs_random_cachetag(v9ses);
		if (klen < 0)
			return 0;
	} else {
		klen = strlen(v9ses->cachetag);
	}

	if (klen > bufmax)
		return 0;

	P9_DPRINTK(P9_DEBUG_FSC, "cache session tag %s", v9ses->cachetag);
	memcpy(buffer, v9ses, klen);
	return klen;
}

void v9fs_cache_inode_get_cookie(struct inode *inode)
{
	struct v9fs_cookie *vcookie;
	struct v9fs_session_info *v9ses;

	if (!S_IREG(inode->i_mode))
		return;

	vcookie = v9fs_inode2cookie(inode);
	if (vcookie->fscache)
		return;

	v9ses = v9fs_inode2v9ses(inode);
	vcookie->fscache = fscache_acquire_cookie(v9ses->fscache,
						  &v9fs_cache_inode_index_def,
						  vcookie);

	P9_DPRINTK(P9_DEBUG_FSC, "inode %p got cookie %p", inode,
		   vcookie->fscache);
}

void v9fs_cache_inode_put_cookie(struct inode *inode)
{
	struct v9fs_cookie *vcookie;

	vcookie = v9fs_inode2cookie(inode);
	if (!vcookie->fscache)
		return;
	P9_DPRINTK(P9_DEBUG_FSC, "inode %p put cookie %p", inode,
		   vcookie->fscache);

	fscache_relinquish_cookie(vcookie->fscache, 0);
	vcookie->fscache = NULL;
}

void v9fs_cache_inode_flush_cookie(struct inode *inode)
{
	struct v9fs_cookie *vcookie;

	vcookie = v9fs_inode2cookie(inode);
	if (!vcookie->fscache)
		return;
	P9_DPRINTK(P9_DEBUG_FSC, "inode %p put cookie %p", inode,
		   vcookie->fscache);

	if (inode->i_mapping && inode->i_mapping->nrpages)
		invalidate_inode_pages2(inode->i_mapping);

	fscache_relinquish_cookie(vcookie->fscache, 1);
	vcookie->fscache = NULL;
}

void v9fs_cache_inode_set_cookie(struct inode *inode, struct file *filp)
{
	struct v9fs_cookie *vcookie;
	struct p9_fid *fid;

	vcookie = v9fs_inode2cookie(inode);
	if (!vcookie->fscache)
		v9fs_cache_inode_get_cookie(inode);

	spin_lock(&vcookie->lock);
	fid = filp->private_data;
	if ((filp->flags & O_ACCMODE) != O_RDONLY)
		v9fs_cache_inode_put_cookie(inode);
	else
		v9fs_cache_inode_get_cookie(inode);

	spin_unlock(&vcookie->lock);
}

void v9fs_cache_inode_reset_cookie(struct inode *inode)
{
	struct v9fs_cookie *vcookie;
	struct v9fs_session_info *v9ses;
	struct fscache_cookie *old;
	
	vcookie = v9fs_inode2cookie(inode);
	if (!vcookie->fscache)
		return;

	old = vcookie->fscache;

	spin_lock(&vcookie->lock);
	fscache_relinquish_cookie(vcookie->fscache, 1);

	v9ses = v9fs_inode2v9ses(inode);
	vcookie->fscache = fscache_acquire_cookie(v9ses->cache,
						  &v9fs_cache_inode_index_def,
						  vcookie);

	P9_DPRINTK(P9_DEBUG_FSC, "inode %p revalidating cookie old %p new %p", 
		   inode, old, vcookie->fscache);

	spin_unlock(&vcookie->lock);
}

static void v9fs_vfs_readpage_complete(struct page *page,
				       void *data,
				       int error)
{
	if (!error)
		SetPageUptodate(page);

	unlock_page(page);
}

int __v9fs_readpage_from_fscache(struct inode *inode, struct page *page)
{
	int ret;
	struct v9fs_cookie *vcookie = v9fs_inode2cookie(inode);

	P9_DPRINTK(P9_DEBUG_FSC, "inode %p page %p", inode, page);
	if (!vcookie->fscache)
		return -ENOBUFS;

	ret = fscache_read_or_alloc_page(vcookie->fscache,
					 page,
					 v9fs_vfs_readpage_complete,
					 NULL,
					 GFP_KERNEL);
	switch(ret) {
	case -ENOBUFS:
	case -ENODATA:
		P9_DPRINTK(P9_DEBUG_FSC, "page/inode not in cache %d", ret);
		return 1;
	case 0:
		P9_DPRINTK(P9_DEBUG_FSC, "BIO submitted");
		return ret;
	default:
		P9_DPRINTK(P9_DEBUG_FSC, "ret %d", ret);
		return ret;
	}
}

int __v9fs_readpages_from_fscache(struct inode *inode,
				  struct address_space *mapping,
				  struct list_head *pages
				  unsigned *nr_pages)
{
	int ret;
	struct v9fs_cookie *vcookie = v9fs_inode2cookie(inode);

	P9_DPRINTK(P9_DEBUG_FSC, "inode %p pages %u", inode, *nr_pages);
	if (!vcookie->fscache)
		return -ENOBUFS;

	ret = fscache_read_or_alloc_pages(vcookie->fscache,
					  mapping, pages, nr_pages,
					  v9fs_vfs_readpage_complete,
					  NULL,
					  mapping_gfp_mask(mapping));
	switch(ret) {
	case -ENOBUFS:
	case -ENODATA:
		P9_DPRINTK(P9_DEBUG_FSC, "pages/inodes not in cache %d", ret);
		return 1;
	case 0:
		BUG_ON(!list_empty(pages));
		BUG_ON(nr_pages != 0);
		P9_DPRINTK(P9_DEBUG_FSC, "BIO submitted");
		return ret;
	default:
		P9_DPRINTK(P9_DEBUG_FSC, "ret %d", ret);
		return ret;
	}
}

void __v9fs_readpage_to_fscache(struct inode *inode, struct page *page)
{
	int ret;
	struct v9fs_cookie *vcookie = v9fs_inode2cookie(inode);

	P9_DPRINTK(P9_DEBUG_FSC, "inode %p page %p", inode, page);
	ret = fscache_write_page(vcookie->fscache, page, GFP_KERNEL);
	P9_DPRINTK(P9_DEBUG_FSC, "ret =  %d", ret);
	if (ret != 0)
		v9fs_uncache_page(inode, page);
}
