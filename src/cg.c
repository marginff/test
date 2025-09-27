/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003 Juli Mallett.  All rights reserved.
 *
 * This software was written by Juli Mallett <jmallett@FreeBSD.org> for the
 * FreeBSD project.  Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistribution of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistribution in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

struct unionacg {
	struct cg d_cg;
	char d_buf[MAXBSIZE];
};
struct unionacg d_acg;
#define acg d_acg.d_cg

int cgput(int devfd, struct fs *fs, struct cg *cgp)
{
	size_t cnt;

	if ((fs->fs_metackhash & CK_CYLGRP) != 0) {
		cgp->cg_ckhash = 0;
		cgp->cg_ckhash =
		    calculate_crc32c(~0L, (void *)cgp, fs->fs_cgsize);
	}
	failmsg = NULL;
	if ((cnt = pwrite(devfd, cgp, fs->fs_cgsize,
	    fsbtodb(fs, cgtod(fs, cgp->cg_cgx)) *
	    (fs->fs_fsize / fsbtodb(fs,1)))) < 0)
		return (-1);
	if (cnt != fs->fs_cgsize) {
		failmsg = "short write to block device";
		return (-1);
	}
	return (0);
}

int
cgwrite()
{

		
	if (cgput(d_fd, &sblock, &acg) == 0)
		return (0);
	d_err = NULL;

	if (failmsg != NULL) {
		d_err = failmsg;
		return (-1);
	}

	switch(errno){
		case EIO: 
			d_err = "unable to write cylinder group";
			break;
		default:
			d_err = strerror(errno);
	}

	return (-1);

}


/*
 * put a block into the map
 */
static void
setblock(struct fs *fs, unsigned char *cp, int h)
{
	switch (fs->fs_frag) {
	case 8:
		cp[h] = 0xff;
		return;
	case 4:
		cp[h >> 1] |= (0x0f << ((h & 0x1) << 2));
		return;
	case 2:
		cp[h >> 2] |= (0x03 << ((h & 0x3) << 1));
		return;
	case 1:
		cp[h >> 3] |= (0x01 << (h & 0x7));
		return;
	default:
		fprintf(stderr, "setblock bad fs_frag %d\n", fs->fs_frag);
		return;
	}
}



/*
 * Initialize a cylinder group.
 */
void
initcg(int cylno, time_t utime)
{

	long blkno, start;
	off_t savedactualloc;
	uint i, j, d, dlower, dupper;
	ufs2_daddr_t cbase, dmax;
	struct ufs1_dinode *dp1;
	struct ufs2_dinode *dp2;
	struct csum *cs;

	/*
	 * Determine block bounds for cylinder group.
	 * Allow space for super block summary information in first
	 * cylinder group.
	 */
	cbase = cgbase(&sblock, cylno);
	dmax = cbase + sblock.fs_fpg;
	if (dmax > sblock.fs_size)
		dmax = sblock.fs_size;
	dlower = cgsblock(&sblock, cylno) - cbase;
	dupper = cgdmin(&sblock, cylno) - cbase;
	if (cylno == 0)
		dupper += howmany(sblock.fs_cssize, sblock.fs_fsize);
	cs = &fscs[cylno];
	memset(&acg, 0, sblock.fs_cgsize);
	acg.cg_time = utime;
	acg.cg_magic = CG_MAGIC;
	acg.cg_cgx = cylno;
	acg.cg_niblk = sblock.fs_ipg;
	acg.cg_initediblk = MIN(sblock.fs_ipg, 2 * INOPB(&sblock));
	acg.cg_ndblk = dmax - cbase;
	if (sblock.fs_contigsumsize > 0)
		acg.cg_nclusterblks = acg.cg_ndblk / sblock.fs_frag;
	start = sizeof(acg);
	if (Oflag == 2) {
		acg.cg_iusedoff = start;
	} else {
		acg.cg_old_ncyl = sblock.fs_old_cpg;
		acg.cg_old_time = acg.cg_time;
		acg.cg_time = 0;
		acg.cg_old_niblk = acg.cg_niblk;
		acg.cg_niblk = 0;
		acg.cg_initediblk = 0;
		acg.cg_old_btotoff = start;
		acg.cg_old_boff = acg.cg_old_btotoff +
		    sblock.fs_old_cpg * sizeof(int32_t);
		acg.cg_iusedoff = acg.cg_old_boff +
		    sblock.fs_old_cpg * sizeof(u_int16_t);
	}
	acg.cg_freeoff = acg.cg_iusedoff + howmany(sblock.fs_ipg, CHAR_BIT);
	acg.cg_nextfreeoff = acg.cg_freeoff + howmany(sblock.fs_fpg, CHAR_BIT);
	if (sblock.fs_contigsumsize > 0) {
		acg.cg_clustersumoff =
		    roundup(acg.cg_nextfreeoff, sizeof(u_int32_t));
		acg.cg_clustersumoff -= sizeof(u_int32_t);
		acg.cg_clusteroff = acg.cg_clustersumoff +
		    (sblock.fs_contigsumsize + 1) * sizeof(u_int32_t);
		acg.cg_nextfreeoff = acg.cg_clusteroff +
		    howmany(fragstoblks(&sblock, sblock.fs_fpg), CHAR_BIT);
	}
	if (acg.cg_nextfreeoff > (unsigned)sblock.fs_cgsize) {
		printf("Panic: cylinder group too big by %d bytes\n",
		    acg.cg_nextfreeoff - (unsigned)sblock.fs_cgsize);
		exit(37);
	}
	acg.cg_cs.cs_nifree += sblock.fs_ipg;
	if (cylno == 0)
		for (i = 0; i < (long)UFS_ROOTINO; i++) {
			setbit(cg_inosused(&acg), i);
			acg.cg_cs.cs_nifree--;
		}
	if (cylno > 0) {
		/*
		 * In cylno 0, beginning space is reserved
		 * for boot and super blocks.
		 */
		for (d = 0; d < dlower; d += sblock.fs_frag) {
			blkno = d / sblock.fs_frag;
			setblock(&sblock, cg_blksfree(&acg), blkno);
			if (sblock.fs_contigsumsize > 0)
				setbit(cg_clustersfree(&acg), blkno);
			acg.cg_cs.cs_nbfree++;
		}
	}
	if ((i = dupper % sblock.fs_frag)) {
		acg.cg_frsum[sblock.fs_frag - i]++;
		for (d = dupper + sblock.fs_frag - i; dupper < d; dupper++) {
			setbit(cg_blksfree(&acg), dupper);
			acg.cg_cs.cs_nffree++;
		}
	}
	for (d = dupper; d + sblock.fs_frag <= acg.cg_ndblk;
	     d += sblock.fs_frag) {
		blkno = d / sblock.fs_frag;
		setblock(&sblock, cg_blksfree(&acg), blkno);
		if (sblock.fs_contigsumsize > 0)
			setbit(cg_clustersfree(&acg), blkno);
		acg.cg_cs.cs_nbfree++;
	}
	if (d < acg.cg_ndblk) {
		acg.cg_frsum[acg.cg_ndblk - d]++;
		for (; d < acg.cg_ndblk; d++) {
			setbit(cg_blksfree(&acg), d);
			acg.cg_cs.cs_nffree++;
		}
	}
	if (sblock.fs_contigsumsize > 0) {
		int32_t *sump = cg_clustersum(&acg);
		u_char *mapp = cg_clustersfree(&acg);
		int map = *mapp++;
		int bit = 1;
		int run = 0;

		for (i = 0; i < acg.cg_nclusterblks; i++) {
			if ((map & bit) != 0)
				run++;
			else if (run != 0) {
				if (run > sblock.fs_contigsumsize)
					run = sblock.fs_contigsumsize;
				sump[run]++;
				run = 0;
			}
			if ((i & (CHAR_BIT - 1)) != CHAR_BIT - 1)
				bit <<= 1;
			else {
				map = *mapp++;
				bit = 1;
			}
		}
		if (run != 0) {
			if (run > sblock.fs_contigsumsize)
				run = sblock.fs_contigsumsize;
			sump[run]++;
		}
	}
	*cs = acg.cg_cs;
	/*
	 * Write out the duplicate super block. Then write the cylinder
	 * group map and two blocks worth of inodes in a single write.
	 */
	savedactualloc = sblock.fs_sblockactualloc;
	sblock.fs_sblockactualloc =
	    (fsbtodb(&sblock, cgsblock(&sblock, cylno))) /sectorsize;

	if (sbwrite(0) != 0)
		err(1, "sbwrite:");
	sblock.fs_sblockactualloc = savedactualloc;

	if (cgwrite() != 0)
		err(1, "initcg: cgwrite: %s", d_err);
	start = 0;
	dp1 = (struct ufs1_dinode *)(&iobuf[start]);
	dp2 = (struct ufs2_dinode *)(&iobuf[start]);
	for (i = 0; i < acg.cg_initediblk; i++) {
		if (sblock.fs_magic == FS_UFS1_MAGIC) {
			dp1->di_gen = newfs_random();
			dp1++;
		} else {
			dp2->di_gen = newfs_random();
			dp2++;
		}
	}
	wtfs(fsbtodb(&sblock, cgimin(&sblock, cylno)), iobufsize, iobuf);
	/*
	 * For the old file system, we have to initialize all the inodes.
	 */
	if (Oflag == 1) {
		for (i = 2 * sblock.fs_frag;
		     i < sblock.fs_ipg / INOPF(&sblock);
		     i += sblock.fs_frag) {
			dp1 = (struct ufs1_dinode *)(&iobuf[start]);
			for (j = 0; j < INOPB(&sblock); j++) {
				dp1->di_gen = newfs_random();
				dp1++;
			}
			wtfs(fsbtodb(&sblock, cgimin(&sblock, cylno) + i),
			    sblock.fs_bsize, &iobuf[start]);
		}
	}
}
