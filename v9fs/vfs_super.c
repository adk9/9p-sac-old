/*
 *  linux/fs/9p/vfs_super.c
 *
 * This file contians superblock ops for 9P2000. It is intended that
 * you mount this file system on directories.
 *
 *  Copyright (C) 2004 by Eric Van Hensbergen <ericvh@gmail.com>
 *  Copyright (C) 2002 by Ron Minnich <rminnich@lanl.gov>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/inet.h>
#include <linux/pagemap.h>
#include <linux/seq_file.h>
#include <linux/mount.h>
#include <linux/idr.h>
#include <linux/sched.h>
#include "9p.h"
#include "client.h"

#include "v9fs.h"
#include "v9fs_vfs.h"
#include "fid.h"

static void v9fs_clear_inode(struct inode *);
static const struct super_operations v9fs_super_ops;

/**
 * v9fs_clear_inode - release an inode
 * @inode: inode to release
 *
 */

static void v9fs_clear_inode(struct inode *inode)
{
	filemap_fdatawrite(inode->i_mapping);
}

/**
 * v9fs_set_super - set the superblock
 * @s: super block
 * @data: file system specific data
 *
 */

static int v9fs_set_super(struct super_block *s, void *data)
{
	s->s_fs_info = data;
	return set_anon_super(s, data);
}

/**
 * v9fs_fill_super - populate superblock with info
 * @sb: superblock
 * @v9ses: session information
 * @flags: flags propagated from v9fs_get_sb()
 *
 */

static int
v9fs_fill_super(struct super_block *sb, struct v9fs_session_info *v9ses,
		int flags, const char *dev_name, void *data)
{
	struct inode *inode = NULL;
	struct dentry *root = NULL;
	int mode = S_IRWXUGO | S_ISVTX;
	struct p9_wstat *st = NULL;
	struct p9_fid *fid;

	sb->s_maxbytes = MAX_LFS_FILESIZE;
	sb->s_blocksize_bits = fls(v9ses->maxdata - 1);
	sb->s_blocksize = 1 << sb->s_blocksize_bits;
	sb->s_magic = V9FS_MAGIC;
	sb->s_op = &v9fs_super_ops;

	sb->s_flags = flags | MS_ACTIVE | MS_SYNCHRONOUS | MS_DIRSYNC |
	    MS_NOATIME;

	inode = v9fs_get_inode(sb, S_IFDIR | mode);
	if (IS_ERR(inode))
		return PTR_ERR(inode);

	root = d_alloc_root(inode);
	if (!root) {
		iput(inode);
		return -ENOMEM;
	}

	fid = v9fs_session_init(v9ses, dev_name, data);
	if (IS_ERR(fid)) {
		iput(inode);
		return PTR_ERR(fid);
	}

	st = p9_client_stat(fid);
	if (IS_ERR(st)) {
		iput(inode);
		p9_client_clunk(fid);
		return PTR_ERR(st);
	}

	root->d_inode->i_ino = v9fs_qid2ino(&st->qid);
	v9fs_stat2inode(st, root->d_inode, sb);

	v9fs_fid_add(root, fid);
	p9stat_free(st);
	kfree(st);

	sb->s_root = root;
	save_mount_options(sb, data);
	return 0;
}

/**
 * v9fs_get_sb - mount a superblock
 * @fs_type: file system type
 * @flags: mount flags
 * @dev_name: device name that was mounted
 * @data: mount options
 * @mnt: mountpoint record to be instantiated
 *
 */

static int v9fs_get_sb(struct file_system_type *fs_type, int flags,
		       const char *dev_name, void *data,
		       struct vfsmount *mnt)
{
	struct super_block *sb = NULL;
	struct v9fs_session_info *v9ses = NULL;
	int ret = 0;

	P9_DPRINTK(P9_DEBUG_VFS, " \n");

	v9ses = v9fs_session_new(dev_name, data);
	if (IS_ERR(v9ses))
		return PTR_ERR(v9ses);

	sb = sget(fs_type, NULL, v9fs_set_super, v9ses);
	if (IS_ERR(sb)) {
		v9fs_session_close(v9ses);
		return PTR_ERR(sb);
	}

	if (!sb->s_root) {
		ret = v9fs_fill_super(sb, v9ses, flags, dev_name, data);
		if (ret < 0) {
			deactivate_locked_super(sb);
			return ret;
		}
	}

	P9_DPRINTK(P9_DEBUG_VFS, " simple set mount, return 0\n");
	sb->s_flags |= MS_ACTIVE;
	simple_set_mnt(mnt, sb);
	return 0;
}

/**
 * v9fs_kill_super - Kill Superblock
 * @s: superblock
 *
 */

static void v9fs_kill_super(struct super_block *s)
{
	struct v9fs_session_info *v9ses = s->s_fs_info;

	P9_DPRINTK(P9_DEBUG_VFS, " %p\n", s);
	if (s->s_root)
		v9fs_dentry_release(s->s_root);	/* clunk root */
	kill_anon_super(s);

	v9fs_session_close(v9ses);
	s->s_fs_info = NULL;
	P9_DPRINTK(P9_DEBUG_VFS, "exiting kill_super\n");
}

static void
v9fs_umount_begin(struct super_block *sb)
{
	struct v9fs_session_info *v9ses;

	v9ses = sb->s_fs_info;
	v9fs_session_cancel(v9ses);
}

static const struct super_operations v9fs_super_ops = {
	.statfs = simple_statfs,
	.clear_inode = v9fs_clear_inode,
	.show_options = generic_show_options,
	.umount_begin = v9fs_umount_begin,
};

struct file_system_type v9fs_fs_type = {
	.name = "9p",
	.get_sb = v9fs_get_sb,
	.kill_sb = v9fs_kill_super,
	.owner = THIS_MODULE,
};
