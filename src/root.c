/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Marshall
 * Kirk McKusick and Network Associates Laboratories, the Security
 * Research Division of Network Associates, Inc. under DARPA/SPAWAR
 * contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA CHATS
 * research program.
 *
 * Copyright (c) 1980, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

struct direct {
	uint32_t d_ino;		/* inode number of entry */
	uint16_t d_reclen;		/* length of this record */
	uint8_t  d_type; 		/* file type, see below */
	uint8_t  d_namlen;		/* length of string in d_name */
	char	  d_name[UFS_MAXNAMLEN + 1];
					/* name with length <= UFS_MAXNAMLEN */
};

static struct direct root_dir[] = {
	{ UFS_ROOTINO, sizeof(struct direct), DT_DIR, 1, "." },
	{ UFS_ROOTINO, sizeof(struct direct), DT_DIR, 2, ".." },
	{ UFS_ROOTINO + 1, sizeof(struct direct), DT_DIR, 5, ".snap" },
};

static struct direct snap_dir[] = {
	{ UFS_ROOTINO + 1, sizeof(struct direct), DT_DIR, 1, "." },
	{ UFS_ROOTINO, sizeof(struct direct), DT_DIR, 2, ".." },
};


/*
 * take a block out of the map
 */
static void
clrblock(struct fs *fs, unsigned char *cp, int h)
{
	switch ((fs)->fs_frag) {
	case 8:
		cp[h] = 0;
		return;
	case 4:
		cp[h >> 1] &= ~(0x0f << ((h & 0x1) << 2));
		return;
	case 2:
		cp[h >> 2] &= ~(0x03 << ((h & 0x3) << 1));
		return;
	case 1:
		cp[h >> 3] &= ~(0x01 << (h & 0x7));
		return;
	default:
		fprintf(stderr, "clrblock bad fs_frag %d\n", fs->fs_frag);
		return;
	}
}

/*
 * check if a block is available
 */
static int
isblock(struct fs *fs, unsigned char *cp, int h)
{
	unsigned char mask;

	switch (fs->fs_frag) {
	case 8:
		return (cp[h] == 0xff);
	case 4:
		mask = 0x0f << ((h & 0x1) << 2);
		return ((cp[h >> 1] & mask) == mask);
	case 2:
		mask = 0x03 << ((h & 0x3) << 1);
		return ((cp[h >> 2] & mask) == mask);
	case 1:
		mask = 0x01 << (h & 0x7);
		return ((cp[h >> 3] & mask) == mask);
	default:
		fprintf(stderr, "isblock bad fs_frag %d\n", fs->fs_frag);
		return (0);
	}
}

/*
 * allocate a block or frag
 */
ufs2_daddr_t
alloc(int size, int mode)
{
	int i, blkno, frag;
	uint d;

	bread(part_ofs + fsbtodb(&sblock, cgtod(&sblock, 0)), (char *)&acg,
	    sblock.fs_cgsize);
	if (acg.cg_magic != CG_MAGIC) {
		printf("cg 0: bad magic number\n");
		exit(38);
	}
	if (acg.cg_cs.cs_nbfree == 0) {
		printf("first cylinder group ran out of space\n");
		exit(39);
	}
	for (d = 0; d < acg.cg_ndblk; d += sblock.fs_frag)
		if (isblock(&sblock, cg_blksfree(&acg), d / sblock.fs_frag))
			goto goth;
	printf("internal error: can't find block in cyl 0\n");
	exit(40);
goth:
	blkno = fragstoblks(&sblock, d);
	clrblock(&sblock, cg_blksfree(&acg), blkno);
	if (sblock.fs_contigsumsize > 0)
		clrbit(cg_clustersfree(&acg), blkno);
	acg.cg_cs.cs_nbfree--;
	sblock.fs_cstotal.cs_nbfree--;
	fscs[0].cs_nbfree--;
	if (mode & IFDIR) {
		acg.cg_cs.cs_ndir++;
		sblock.fs_cstotal.cs_ndir++;
		fscs[0].cs_ndir++;
	}
	if (size != sblock.fs_bsize) {
		frag = howmany(size, sblock.fs_fsize);
		fscs[0].cs_nffree += sblock.fs_frag - frag;
		sblock.fs_cstotal.cs_nffree += sblock.fs_frag - frag;
		acg.cg_cs.cs_nffree += sblock.fs_frag - frag;
		acg.cg_frsum[sblock.fs_frag - frag]++;
		for (i = frag; i < sblock.fs_frag; i++)
			setbit(cg_blksfree(&acg), d + i);
	}
	if (cgwrite() != 0)
		err(1, "alloc: cgwrite: %s", d_err);
	return ((ufs2_daddr_t)d);
}

/*
 * Update an inode check-hash.
 */
void
ffs_update_dinode_ckhash(struct fs *fs, struct ufs2_dinode *dip)
{

	if (dip->di_mode == 0 || (fs->fs_metackhash & CK_INODE) == 0)
		return;
	/*
	 * Exclude old di_ckhash from the crc32 calculation, e.g., always use
	 * a check-hash value of zero when calculating the new check-hash.
	 */
	dip->di_ckhash = 0;
	dip->di_ckhash = calculate_crc32c(~0L, (void *)dip, sizeof(*dip));
}


/*
 * Allocate an inode on the disk
 */
void
iput(union dinode *ip, ino_t ino)
{

	bread(part_ofs + fsbtodb(&sblock, cgtod(&sblock, 0)), (char *)&acg,
	    sblock.fs_cgsize);
	if (acg.cg_magic != CG_MAGIC) {
		printf("cg 0: bad magic number\n");
		exit(31);
	}
	acg.cg_cs.cs_nifree--;
	setbit(cg_inosused(&acg), ino);
	if (cgwrite() != 0)
		err(1, "iput: cgwrite: %s", d_err);
	sblock.fs_cstotal.cs_nifree--;
	fscs[0].cs_nifree--;


	if (d_ufs == 2)
		ffs_update_dinode_ckhash(&sblock, &ip->dp2);


	void *inoblock = malloc(sblock.fs_bsize);;
	bread(fsbtodb(&sblock, ino_to_fsba(&sblock, ino)), inoblock,
	    sblock.fs_bsize);

	if (sblock.fs_magic == FS_UFS1_MAGIC)
		((struct ufs1_dinode *)inoblock)[ino] = ip->dp1;
	else
		((struct ufs2_dinode *)inoblock)[ino] = ip->dp2;


	if (bwrite(fsbtodb(&sblock, ino_to_fsba(&sblock, 0)),
	    inoblock, sblock.fs_bsize) <= 0)
		err(1, "iput: bwrite");
}


/*
 * construct a set of directory entries in "iobuf".
 * return size of directory.
 */
int
makedir(struct direct *protodir, int entries)
{
	char *cp;
	int i, spcleft;

	spcleft = DIRBLKSIZ;
	memset(iobuf, 0, DIRBLKSIZ);
	for (cp = iobuf, i = 0; i < entries - 1; i++) {
		protodir[i].d_reclen = DIRSIZ(0, &protodir[i]);
		memmove(cp, &protodir[i], protodir[i].d_reclen);
		cp += protodir[i].d_reclen;
		spcleft -= protodir[i].d_reclen;
	}
	protodir[i].d_reclen = spcleft;
	memmove(cp, &protodir[i], DIRSIZ(0, &protodir[i]));
	return (DIRBLKSIZ);
}

void
fsinit(time_t utime)
{
	union dinode node;
	struct group *grp;
	gid_t gid;
	int entries;

	memset(&node, 0, sizeof node);
	if ((grp = getgrnam("operator")) != NULL) {
		gid = grp->gr_gid;
	} else {
		warnx("Cannot retrieve operator gid, using gid 0.");
		gid = 0;
	}
	entries = (nflag) ? ROOTLINKCNT - 1: ROOTLINKCNT;
	if (sblock.fs_magic == FS_UFS1_MAGIC) {
		/*
		 * initialize the node
		 */
		node.dp1.di_atime = utime;
		node.dp1.di_mtime = utime;
		node.dp1.di_ctime = utime;
		/*
		 * create the root directory
		 */
		node.dp1.di_mode = IFDIR | UMASK;
		node.dp1.di_nlink = entries;
		node.dp1.di_size = makedir(root_dir, entries);
		node.dp1.di_db[0] = alloc(sblock.fs_fsize, node.dp1.di_mode);
		node.dp1.di_blocks =
		    fragroundup(&sblock, node.dp1.di_size)/sectorsize;
		wtfs(fsbtodb(&sblock, node.dp1.di_db[0]), sblock.fs_fsize,
		    iobuf);
		iput(&node, UFS_ROOTINO);
		if (!nflag) {
			/*
			 * create the .snap directory
			 */
			node.dp1.di_mode |= 020;
			node.dp1.di_gid = gid;
			node.dp1.di_nlink = SNAPLINKCNT;
			node.dp1.di_size = makedir(snap_dir, SNAPLINKCNT);
				node.dp1.di_db[0] =
				    alloc(sblock.fs_fsize, node.dp1.di_mode);
			node.dp1.di_blocks =
			    fragroundup(&sblock, node.dp1.di_size)/sectorsize;
			node.dp1.di_dirdepth = 1;
			wtfs(fsbtodb(&sblock, node.dp1.di_db[0]),
				    sblock.fs_fsize, iobuf);
			iput(&node, UFS_ROOTINO + 1);
		}
	} else {
		/*
		 * initialize the node
		 */
		node.dp2.di_atime = utime;
		node.dp2.di_mtime = utime;
		node.dp2.di_ctime = utime;
		node.dp2.di_birthtime = utime;
		/*
		 * create the root directory
		 */
		node.dp2.di_mode = IFDIR | UMASK;
		node.dp2.di_nlink = entries;
		node.dp2.di_size = makedir(root_dir, entries);
		node.dp2.di_db[0] = alloc(sblock.fs_fsize, node.dp2.di_mode);
		node.dp2.di_blocks =
		    /*fsbtodb*/fragroundup(&sblock, node.dp2.di_size)/sectorsize;
		
		wtfs(fsbtodb(&sblock, node.dp2.di_db[0]), sblock.fs_fsize,
		    iobuf);
		iput(&node, UFS_ROOTINO);
		if (!nflag) {
			/*
			 * create the .snap directory
			 */
			node.dp2.di_mode |= 020;
			node.dp2.di_gid = gid;
			node.dp2.di_nlink = SNAPLINKCNT;
			node.dp2.di_size = makedir(snap_dir, SNAPLINKCNT);
				node.dp2.di_db[0] =
				    alloc(sblock.fs_fsize, node.dp2.di_mode);
			node.dp2.di_blocks =
			    fragroundup(&sblock, node.dp2.di_size)/sectorsize;
			node.dp2.di_dirdepth = 1;
			wtfs(fsbtodb(&sblock, node.dp2.di_db[0]), 
				    sblock.fs_fsize, iobuf);
			iput(&node, UFS_ROOTINO + 1);
		}
	}
}
