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

#include "fs.h"

#define POWEROF2(num)	(((num) & ((num) - 1)) == 0)
typedef unsigned int    u_int;
#define CGSIZEFUDGE 8

static int ilog2(int val)
{
	u_int n;

	for (n = 0; n < sizeof(n) * CHAR_BIT; n++)
		if (1 << n == val)
			return (n);
	errx(1, "ilog2: %d is not a power of 2\n", val);
}

static int
charsperline(void)
{
	int columns;
	char *cp;
	struct winsize ws;

	columns = 0;
	if (ioctl(0, TIOCGWINSZ, &ws) != -1)
		columns = ws.ws_col;
	if (columns == 0 && (cp = getenv("COLUMNS")))
		columns = atoi(cp);
	if (columns == 0)
		columns = 80;	/* last resort */
	return (columns);
}





void mkfs(char *fsys) {

	time_t utime;

	d_bsize = sectorsize;
	d_ufs = Oflag;
	if (Rflag)
		utime = 1000000000;
	else
		time(&utime);

   	if ((sblock.fs_si = (struct fs_summary_info *)malloc(sizeof(struct fs_summary_info))) == NULL) {
		printf("Superblock summary info allocation failed.\n");
		exit(18);
	}
	sblock.fs_old_flags = FS_FLAGS_UPDATED;
	sblock.fs_flags = 0;
	if (Uflag)
		sblock.fs_flags |= FS_DOSOFTDEP;
	if (Lflag)
		strlcpy((char *)sblock.fs_volname, volumelabel, MAXVOLLEN);
	if (Jflag)
		sblock.fs_flags |= FS_GJOURNAL;
	if (lflag)
		sblock.fs_flags |= FS_MULTILABEL;
	if (tflag)
		sblock.fs_flags |= FS_TRIM;
	/*
	 * Validate the given file system size.
	 * Verify that its last block can actually be accessed.
	 * Convert to file system fragment sized units.
	 */
	if (fssize <= 0) {
		printf("preposterous size %jd\n", (intmax_t)fssize);
		exit(13);
	}

    wtfs(fssize - (realsectorsize / DEV_BSIZE), realsectorsize,
		(char *)&sblock);
	/*
	 * collect and verify the file system density info
	 */
	sblock.fs_avgfilesize = avgfilesize;
	sblock.fs_avgfpdir = avgfilesperdir;
	if (sblock.fs_avgfilesize <= 0)
		printf("illegal expected average file size %d\n",
		    sblock.fs_avgfilesize), exit(14);
	if (sblock.fs_avgfpdir <= 0)
		printf("illegal expected number of files per directory %d\n",
		    sblock.fs_avgfpdir), exit(15);

restart:
	/*
	 * collect and verify the block and fragment sizes
	 */
	sblock.fs_bsize = bsize;
	sblock.fs_fsize = fsize;
	if (!POWEROF2(sblock.fs_bsize)) {
		printf("block size must be a power of 2, not %d\n",
		    sblock.fs_bsize);
		exit(16);
	}
	if (!POWEROF2(sblock.fs_fsize)) {
		printf("fragment size must be a power of 2, not %d\n",
		    sblock.fs_fsize);
		exit(17);
	}
	if (sblock.fs_fsize < sectorsize) {
		printf("increasing fragment size from %d to sector size (%d)\n",
		    sblock.fs_fsize, sectorsize);
		sblock.fs_fsize = sectorsize;
	}
	if (sblock.fs_bsize > MAXBSIZE) {
		printf("decreasing block size from %d to maximum (%d)\n",
		    sblock.fs_bsize, MAXBSIZE);
		sblock.fs_bsize = MAXBSIZE;
	}
	if (sblock.fs_bsize < MINBSIZE) {
		printf("increasing block size from %d to minimum (%d)\n",
		    sblock.fs_bsize, MINBSIZE);
		sblock.fs_bsize = MINBSIZE;
	}
	if (sblock.fs_fsize > MAXBSIZE) {
		printf("decreasing fragment size from %d to maximum (%d)\n",
		    sblock.fs_fsize, MAXBSIZE);
		sblock.fs_fsize = MAXBSIZE;
	}
	if (sblock.fs_bsize < sblock.fs_fsize) {
		printf("increasing block size from %d to fragment size (%d)\n",
		    sblock.fs_bsize, sblock.fs_fsize);
		sblock.fs_bsize = sblock.fs_fsize;
	}
	if (sblock.fs_fsize * MAXFRAG < sblock.fs_bsize) {
		printf(
		"increasing fragment size from %d to block size / %d (%d)\n",
		    sblock.fs_fsize, MAXFRAG, sblock.fs_bsize / MAXFRAG);
		sblock.fs_fsize = sblock.fs_bsize / MAXFRAG;
	}
	if (maxbsize == 0)
		maxbsize = bsize;
	if (maxbsize < bsize || !POWEROF2(maxbsize)) {
		sblock.fs_maxbsize = sblock.fs_bsize;
		printf("Extent size set to %d\n", sblock.fs_maxbsize);
	} else if (maxbsize > FS_MAXCONTIG * sblock.fs_bsize) {
		sblock.fs_maxbsize = FS_MAXCONTIG * sblock.fs_bsize;
		printf("Extent size reduced to %d\n", sblock.fs_maxbsize);
	} else {
		sblock.fs_maxbsize = maxbsize;
	}

	/*
   * Maxcontig sets the default for the maximum number of blocks
   * that may be allocated sequentially. With file system clustering
   * it is possible to allocate contiguous blocks up to the maximum
   * transfer size permitted by the controller or buffering.
   */

	if (maxcontig == 0)
		maxcontig = MAX(1, MAXPHYS / bsize);
	sblock.fs_maxcontig = maxcontig;
	if (sblock.fs_maxcontig < sblock.fs_maxbsize / sblock.fs_bsize) {
		sblock.fs_maxcontig = sblock.fs_maxbsize / sblock.fs_bsize;
		printf("Maxcontig raised to %d\n", sblock.fs_maxbsize);
	}
	if (sblock.fs_maxcontig > 1)
		sblock.fs_contigsumsize = MIN(sblock.fs_maxcontig,FS_MAXCONTIG);
	sblock.fs_bmask = ~(sblock.fs_bsize - 1);
	sblock.fs_fmask = ~(sblock.fs_fsize - 1);
	sblock.fs_qbmask = ~sblock.fs_bmask;
	sblock.fs_qfmask = ~sblock.fs_fmask;
	sblock.fs_bshift = ilog2(sblock.fs_bsize);
	sblock.fs_fshift = ilog2(sblock.fs_fsize);
	sblock.fs_frag = numfrags(&sblock, sblock.fs_bsize);
	sblock.fs_fragshift = ilog2(sblock.fs_frag);
	if (sblock.fs_frag > MAXFRAG) {
		printf("fragment size %d is still too small (can't happen)\n",
		    sblock.fs_bsize / MAXFRAG);
		exit(21);
	}
	sblock.fs_fsbtodb = ilog2(sblock.fs_fsize / sectorsize);
	sblock.fs_size = fssize = dbtofsb(&sblock, fssize);
	sblock.fs_providersize = dbtofsb(&sblock, mediasize / sectorsize);

/*
   * Before the filesystem is finally initialized, mark it
   * as incompletely initialized.
   */
	sblock.fs_magic = FS_BAD_MAGIC;

	if (Oflag == 1) {
		sblock.fs_sblockloc = SBLOCK_UFS1;
		sblock.fs_sblockactualloc = SBLOCK_UFS1;
		sblock.fs_nindir = sblock.fs_bsize / sizeof(ufs1_daddr_t);
		sblock.fs_inopb = sblock.fs_bsize / sizeof(struct ufs1_dinode);
		sblock.fs_maxsymlinklen = ((UFS_NDADDR + UFS_NIADDR) *
		    sizeof(ufs1_daddr_t));
		sblock.fs_old_inodefmt = FS_44INODEFMT;
		sblock.fs_old_cgoffset = 0;
		sblock.fs_old_cgmask = 0xffffffff;
		sblock.fs_old_size = sblock.fs_size;
		sblock.fs_old_rotdelay = 0;
		sblock.fs_old_rps = 60;
		sblock.fs_old_nspf = sblock.fs_fsize / sectorsize;
		sblock.fs_old_cpg = 1;
		sblock.fs_old_interleave = 1;
		sblock.fs_old_trackskew = 0;
		sblock.fs_old_cpc = 0;
		sblock.fs_old_postblformat = 1;
		sblock.fs_old_nrpos = 1;
	} else {
		sblock.fs_sblockloc = SBLOCK_UFS2;
		sblock.fs_sblockactualloc = SBLOCK_UFS2;
		sblock.fs_nindir = sblock.fs_bsize / sizeof(ufs2_daddr_t);
		sblock.fs_inopb = sblock.fs_bsize / sizeof(struct ufs2_dinode);
		sblock.fs_maxsymlinklen = ((UFS_NDADDR + UFS_NIADDR) *
		    sizeof(ufs2_daddr_t));
	}
	sblock.fs_sblkno =
	    roundup(howmany(sblock.fs_sblockloc + SBLOCKSIZE, sblock.fs_fsize),
		sblock.fs_frag);
	sblock.fs_cblkno = sblock.fs_sblkno +
	    roundup(howmany(SBLOCKSIZE, sblock.fs_fsize), sblock.fs_frag);
	sblock.fs_iblkno = sblock.fs_cblkno + sblock.fs_frag;
	sblock.fs_maxfilesize = sblock.fs_bsize * UFS_NDADDR - 1;

	int64_t sizepb, i;
	for (sizepb = sblock.fs_bsize; i < UFS_NIADDR; i++) {
		sizepb *= NINDIR(&sblock);
		sblock.fs_maxfilesize += sizepb;
	}

	/*
	 * It's impossible to create a snapshot in case that fs_maxfilesize
	 * is smaller than the fssize.
	 */
	if (sblock.fs_maxfilesize < (u_quad_t)fssize) {
		warnx("WARNING: You will be unable to create snapshots on this "
		      "file system.  Correct by using a larger blocksize.");
	}


	/*
	 * Calculate the number of blocks to put into each cylinder group.
	 *
	 * This algorithm selects the number of blocks per cylinder
	 * group. The first goal is to have at least enough data blocks
	 * in each cylinder group to meet the density requirement. Once
	 * this goal is achieved we try to expand to have at least
	 * MINCYLGRPS cylinder groups. Once this goal is achieved, we
	 * pack as many blocks into each cylinder group map as will fit.
	 *
	 * We start by calculating the smallest number of blocks that we
	 * can put into each cylinder group. If this is too big, we reduce
	 * the density until it fits.
	 */
	ino_t maxinum;
	int minfragsperinode, origdensity, fragsperinode, minfpg;

retry:
	maxinum = (((int64_t)(1)) << 32) - INOPB(&sblock);
	minfragsperinode = 1 + fssize / maxinum;
	if (density == 0) {
		density = MAX(NFPI, minfragsperinode) * fsize;
	} else if (density < minfragsperinode * fsize) {
		origdensity = density;
		density = minfragsperinode * fsize;
		fprintf(stderr, "density increased from %d to %d\n",
		    origdensity, density);
	}
	origdensity = density;

	for (;;) {
		fragsperinode = MAX(numfrags(&sblock, density), 1);
		if (fragsperinode < minfragsperinode) {
			bsize <<= 1;
			fsize <<= 1;
			printf("Block size too small for a file system %s %d\n",
			     "of this size. Increasing blocksize to", bsize);
			goto restart;
		}
		minfpg = fragsperinode * INOPB(&sblock);
		if (minfpg > sblock.fs_size)
			minfpg = sblock.fs_size;
		sblock.fs_ipg = INOPB(&sblock);
		sblock.fs_fpg = roundup(sblock.fs_iblkno +
		    sblock.fs_ipg / INOPF(&sblock), sblock.fs_frag);
		if (sblock.fs_fpg < minfpg)
			sblock.fs_fpg = minfpg;
		sblock.fs_ipg = roundup(howmany(sblock.fs_fpg, fragsperinode),
		    INOPB(&sblock));
		sblock.fs_fpg = roundup(sblock.fs_iblkno +
		    sblock.fs_ipg / INOPF(&sblock), sblock.fs_frag);
		if (sblock.fs_fpg < minfpg)
			sblock.fs_fpg = minfpg;
		sblock.fs_ipg = roundup(howmany(sblock.fs_fpg, fragsperinode),
		    INOPB(&sblock));
		if (CGSIZE(&sblock) < (unsigned long)sblock.fs_bsize -
		    CGSIZEFUDGE)
			break;
		density -= sblock.fs_fsize;
	}
	if (density != origdensity)
		printf("density reduced from %d to %d\n", origdensity, density);

	/*
	 * Start packing more blocks into the cylinder group until
	 * it cannot grow any larger, the number of cylinder groups
	 * drops below MINCYLGRPS, or we reach the size requested.
	 * For UFS1 inodes per cylinder group are stored in an int16_t
	 * so fs_ipg is limited to 2^15 - 1.
	 */
	for ( ; sblock.fs_fpg < maxblkspercg; sblock.fs_fpg += sblock.fs_frag) {
		sblock.fs_ipg = roundup(howmany(sblock.fs_fpg, fragsperinode),
		    INOPB(&sblock));
		if (Oflag > 1 || (Oflag == 1 && sblock.fs_ipg <= 0x7fff)) {
			if (sblock.fs_size / sblock.fs_fpg < MINCYLGRPS)
				break;
			if (CGSIZE(&sblock) < (unsigned long)sblock.fs_bsize -
			    CGSIZEFUDGE)
				continue;
			if (CGSIZE(&sblock) == (unsigned long)sblock.fs_bsize -
			    CGSIZEFUDGE)
				break;
		}
		sblock.fs_fpg -= sblock.fs_frag;
		sblock.fs_ipg = roundup(howmany(sblock.fs_fpg, fragsperinode),
		    INOPB(&sblock));
		break;
	}

	/*
	 * Check to be sure that the last cylinder group has enough blocks
	 * to be viable. If it is too small, reduce the number of blocks
	 * per cylinder group which will have the effect of moving more
	 * blocks into the last cylinder group.
	 */
	
	int optimalfpg = sblock.fs_fpg;
	int lastminfpg;
	for (;;) {
		sblock.fs_ncg = howmany(sblock.fs_size, sblock.fs_fpg);
		lastminfpg = roundup(sblock.fs_iblkno +
		    sblock.fs_ipg / INOPF(&sblock), sblock.fs_frag);
		if (sblock.fs_size < lastminfpg) {
			printf("Filesystem size %jd < minimum size of %d\n",
			    (intmax_t)sblock.fs_size, lastminfpg);
			exit(28);
		}
		if (sblock.fs_size % sblock.fs_fpg >= lastminfpg ||
		    sblock.fs_size % sblock.fs_fpg == 0)
			break;
		sblock.fs_fpg -= sblock.fs_frag;
		sblock.fs_ipg = roundup(howmany(sblock.fs_fpg, fragsperinode),
		    INOPB(&sblock));
	}
	if (optimalfpg != sblock.fs_fpg)
		printf("Reduced frags per cylinder group from %d to %d %s\n",
		   optimalfpg, sblock.fs_fpg, "to enlarge last cyl group");
	sblock.fs_cgsize = fragroundup(&sblock, CGSIZE(&sblock));
	sblock.fs_dblkno = sblock.fs_iblkno + sblock.fs_ipg / INOPF(&sblock);
	if (Oflag == 1) {
		sblock.fs_old_spc = sblock.fs_fpg * sblock.fs_old_nspf;
		sblock.fs_old_nsect = sblock.fs_old_spc;
		sblock.fs_old_npsect = sblock.fs_old_spc;
		sblock.fs_old_ncyl = sblock.fs_ncg;
	}

	/*
	 * fill in remaining fields of the super block
	 */
	sblock.fs_csaddr = cgdmin(&sblock, 0);
	sblock.fs_cssize =
	    fragroundup(&sblock, sblock.fs_ncg * sizeof(struct csum));
	fscs = (struct csum *)calloc(1, sblock.fs_cssize);
	if (fscs == NULL)
		errx(31, "calloc failed");
	sblock.fs_sbsize = fragroundup(&sblock, sizeof(struct fs));
	if (sblock.fs_sbsize > SBLOCKSIZE)
		sblock.fs_sbsize = SBLOCKSIZE;
	if (sblock.fs_sbsize < realsectorsize)
		sblock.fs_sbsize = realsectorsize;
	sblock.fs_minfree = minfree;
	if (metaspace > 0 && metaspace < sblock.fs_fpg / 2)
		sblock.fs_metaspace = blknum(&sblock, metaspace);
	else if (metaspace != -1)
		/* reserve half of minfree for metadata blocks */
		sblock.fs_metaspace = blknum(&sblock,
		    (sblock.fs_fpg * minfree) / 200);
	if (maxbpg == 0)
		sblock.fs_maxbpg = MAXBLKPG(sblock.fs_bsize);
	else
		sblock.fs_maxbpg = maxbpg;
	sblock.fs_optim = opt;
	sblock.fs_cgrotor = 0;
	sblock.fs_pendingblocks = 0;
	sblock.fs_pendinginodes = 0;
	sblock.fs_fmod = 0;
	sblock.fs_ronly = 0;
	sblock.fs_state = 0;
	sblock.fs_clean = 1;
	sblock.fs_id[0] = (long)utime;
	sblock.fs_id[1] = newfs_random();
	sblock.fs_fsmnt[0] = '\0';
	long csfrags = howmany(sblock.fs_cssize, sblock.fs_fsize);
	sblock.fs_dsize = sblock.fs_size - sblock.fs_sblkno -
	    sblock.fs_ncg * (sblock.fs_dblkno - sblock.fs_sblkno);
	sblock.fs_cstotal.cs_nbfree =
	    fragstoblks(&sblock, sblock.fs_dsize) -
	    howmany(csfrags, sblock.fs_frag);
	sblock.fs_cstotal.cs_nffree =
	    fragnum(&sblock, sblock.fs_size) +
	    (fragnum(&sblock, csfrags) > 0 ?
	     sblock.fs_frag - fragnum(&sblock, csfrags) : 0);
	sblock.fs_cstotal.cs_nifree =
	    sblock.fs_ncg * sblock.fs_ipg - UFS_ROOTINO;
	sblock.fs_cstotal.cs_ndir = 0;
	sblock.fs_dsize -= csfrags;
	sblock.fs_time = utime;
	if (Oflag == 1) {
		sblock.fs_old_time = utime;
		sblock.fs_old_dsize = sblock.fs_dsize;
		sblock.fs_old_csaddr = sblock.fs_csaddr;
		sblock.fs_old_cstotal.cs_ndir = sblock.fs_cstotal.cs_ndir;
		sblock.fs_old_cstotal.cs_nbfree = sblock.fs_cstotal.cs_nbfree;
		sblock.fs_old_cstotal.cs_nifree = sblock.fs_cstotal.cs_nifree;
		sblock.fs_old_cstotal.cs_nffree = sblock.fs_cstotal.cs_nffree;
	}

	/*
	 * Set flags for metadata that is being check-hashed.
	 *
	 * Metadata check hashes are not supported in the UFS version 1
	 * filesystem to keep it as small and simple as possible.
	 */
	if (Oflag > 1) {
		sblock.fs_flags |= FS_METACKHASH;
		//if (getosreldate() >= P_OSREL_CK_CYLGRP)
			sblock.fs_metackhash |= CK_CYLGRP;
		//if (getosreldate() >= P_OSREL_CK_SUPERBLOCK)
			sblock.fs_metackhash |= CK_SUPERBLOCK;
		//if (getosreldate() >= P_OSREL_CK_INODE)
			sblock.fs_metackhash |= CK_INODE;
	}

	/*
	 * Dump out summary information about file system.
	 */
#	define B2MBFACTOR (1 / (1024.0 * 1024.0))
	printf("%s: %.1fMB (%jd sectors) block size %d, fragment size %d\n",
	    fsys, (float)sblock.fs_size * sblock.fs_fsize * B2MBFACTOR,
	    (intmax_t)fsbtodb(&sblock, sblock.fs_size), sblock.fs_bsize,
	    sblock.fs_fsize);
	printf("\tusing %d cylinder groups of %.2fMB, %d blks, %d inodes.\n",
	    sblock.fs_ncg, (float)sblock.fs_fpg * sblock.fs_fsize * B2MBFACTOR,
	    sblock.fs_fpg / sblock.fs_frag, sblock.fs_ipg);
	if (sblock.fs_flags & FS_DOSOFTDEP)
		printf("\twith soft updates\n");
#	undef B2MBFACTOR

	if (Eflag && !Nflag) {
	/* 	printf("Erasing sectors [%jd...%jd]\n", 
		    sblock.fs_sblockloc / bsize,
		    fsbtodb(&sblock, sblock.fs_size) - 1);
		berase(&disk, sblock.fs_sblockloc / disk.d_bsize,
		    sblock.fs_size * sblock.fs_fsize - sblock.fs_sblockloc); */
	}

	/*
	 * Reference the summary information so it will also be written.
	 */
	sblock.fs_csp = fscs;
	if (!Nflag && sbwrite(0) != 0)
		err(1, "sbwrite: %s", d_err);
	if (Xflag == 1) {
		printf("** Exiting on Xflag 1\n");
		exit(0);
	}
	if (Xflag == 2)
		printf("** Leaving BAD MAGIC on Xflag 2\n");
	else
		sblock.fs_magic = (Oflag != 1) ? FS_UFS2_MAGIC : FS_UFS1_MAGIC;

	/*
	 * Now build the cylinders group blocks and
	 * then print out indices of cylinder groups.
	 */
	printf("super-block backups (for fsck_ffs -b #) at:\n");
	i = 0;
	int width = charsperline();


	/*
	 * Allocate space for two sets of inode blocks.
	 */
	iobufsize = 2 * sblock.fs_bsize;
	if ((iobuf = calloc(1, iobufsize)) == 0) {
		printf("Cannot allocate I/O buffer\n");
		exit(38);
	}

	/*
	 * Write out all the cylinder groups and backup superblocks.
	 */
	uint cg, j;
	char tmpbuf[100];
	for (cg = 0; cg < sblock.fs_ncg; cg++) {
		if (!Nflag)
			initcg(cg, utime);
		j = snprintf(tmpbuf, sizeof(tmpbuf), " %jd%s",
		    (intmax_t)fsbtodb(&sblock, cgsblock(&sblock, cg)),
		    cg < (sblock.fs_ncg-1) ? "," : "");
		if (j < 0)
			tmpbuf[j = 0] = '\0';
		if (i + j >= width) {
			printf("\n");
			i = 0;
		}
		i += j;
		printf("%s", tmpbuf);
		fflush(stdout);
	}
	printf("\n");
	if (Nflag)
		exit(0);


	/*
	 * Now construct the initial file system,
	 * then write out the super-block.
	 */
	fsinit(utime);
	if (Oflag == 1) {
		sblock.fs_old_cstotal.cs_ndir = sblock.fs_cstotal.cs_ndir;
		sblock.fs_old_cstotal.cs_nbfree = sblock.fs_cstotal.cs_nbfree;
		sblock.fs_old_cstotal.cs_nifree = sblock.fs_cstotal.cs_nifree;
		sblock.fs_old_cstotal.cs_nffree = sblock.fs_cstotal.cs_nffree;
	}
	if (Xflag == 3) {
		printf("** Exiting on Xflag 3\n");
		exit(0);
	}
	if (sbwrite(0) != 0)
		err(1, "sbwrite: %s", d_err);
	
	/*
	 * For UFS1 filesystems with a blocksize of 64K, the first
	 * alternate superblock resides at the location used for
	 * the default UFS2 superblock. As there is a valid
	 * superblock at this location, the boot code will use
	 * it as its first choice. Thus we have to ensure that
	 * all of its statistcs on usage are correct.
	 */
	if (Oflag == 1 && bsize == 65536)
		wtfs(fsbtodb(&sblock, cgsblock(&sblock, 0)),
		    bsize, (char *)&sblock);
	
	/*
	 * This should NOT happen. If it does complain loudly and
	 * take evasive action.
	 */
	if ((int32_t)CGSIZE(&sblock) > sblock.fs_bsize) {
		printf("INTERNAL ERROR: ipg %d, fpg %d, contigsumsize %d, ",
		    sblock.fs_ipg, sblock.fs_fpg, sblock.fs_contigsumsize);
		printf("old_cpg %d, size_cg %zu, CGSIZE %zu\n",
		    sblock.fs_old_cpg, sizeof(struct cg), CGSIZE(&sblock));
		printf("Please file a FreeBSD bug report and include this "
		    "output\n");
		maxblkspercg = fragstoblks(&sblock, sblock.fs_fpg) - 1;
		density = 0;
		goto retry;
	}
}
