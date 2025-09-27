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
 * Copyright (c) 1983, 1989, 1993, 1994
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

#include "crc32.c"
#include "fs.h"
#include "mkfsufs.h"
#include "sblock.c"
#include "cg.c"
#include "root.c"
#include "newfs.c"



void usage(char *name)
{
	fprintf(stderr,
	    "usage: %s [ -fsoptions ] special-device%s\n",
	    name,
	    " [device-type]");
	fprintf(stderr, "where fsoptions are:\n");
	fprintf(stderr, "\t-J Enable journaling via gjournal\n");
	fprintf(stderr, "\t-L volume label to add to superblock\n");
	fprintf(stderr,
	    "\t-N do not create file system, just print out parameters\n");
	fprintf(stderr, "\t-O file system format: 1 => UFS1, 2 => UFS2\n");
	fprintf(stderr, "\t-R regression test, suppress random factors\n");
	fprintf(stderr, "\t-S sector size\n");
	fprintf(stderr, "\t-U enable soft updates\n");
	fprintf(stderr, "\t-a maximum contiguous blocks\n");
	fprintf(stderr, "\t-b block size\n");
	fprintf(stderr, "\t-c blocks per cylinders group\n");
	fprintf(stderr, "\t-d maximum extent size\n");
	fprintf(stderr, "\t-e maximum blocks per file in a cylinder group\n");
	fprintf(stderr, "\t-f frag size\n");
	fprintf(stderr, "\t-g average file size\n");
	fprintf(stderr, "\t-h average files per directory\n");
	fprintf(stderr, "\t-i number of bytes per inode\n");
	fprintf(stderr, "\t-j enable soft updates journaling\n");
	fprintf(stderr, "\t-k space to hold for metadata blocks\n");
	fprintf(stderr, "\t-l enable multilabel MAC\n");
	fprintf(stderr, "\t-n do not create .snap directory\n");
	fprintf(stderr, "\t-m minimum free space %%\n");
	fprintf(stderr, "\t-o optimization preference (`space' or `time')\n");
	fprintf(stderr, "\t-r reserved sectors at the end of device\n");
	fprintf(stderr, "\t-s file system size (sectors)\n");
	fprintf(stderr, "\t-t enable TRIM\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	static char	device[MAXPATHLEN];
	char *cp, *special;
	intmax_t reserved;
	int ch;
	size_t i;
	char *prog_name = argv[0];


    while ((ch = getopt(argc, argv,
	    "EJL:NO:RS:T:UXa:b:c:d:e:f:g:h:i:jk:lm:no:p:r:s:t")) != -1) {
	switch (ch) {
		case 'E':
			Eflag = 1;
			break;
		case 'J':
			Jflag = 1;
			break;
		case 'L':
			volumelabel = optarg;
			for (i = 0; isalnum(volumelabel[i]) ||
			    volumelabel[i] == '_' || volumelabel[i] == '-';
			    i++)
				continue;
			if (volumelabel[i] != '\0') {
				errx(1, "bad volume label. Valid characters "
				    "are alphanumerics, dashes, and underscores.");
			}
			if (strlen(volumelabel) >= MAXVOLLEN) {
				errx(1, "bad volume label. Length is longer than %d.",
				    MAXVOLLEN);
			}
			Lflag = 1;
			break;
		case 'N':
			Nflag = 1;
			break;
		case 'O':
			if ((Oflag = atoi(optarg)) < 1 || Oflag > 2)
				errx(1, "%s: bad file system format value",
				    optarg);
			break;
		case 'R':
			Rflag = 1;
			break;
		case 'S':
			sectorsize = atoi(optarg);
           		break;
		case 'T':
			break;
		case 'j':
			jflag = 1;
			/* fall through to enable soft updates */
			/* FALLTHROUGH */
		case 'U':
			Uflag = 1;
			break;
		case 'X':
			Xflag++;
			break;
		case 'a':
			maxcontig = atoi(optarg);
			break;
		case 'b':
			bsize = atoi(optarg);
			if (bsize < MINBSIZE)
				errx(1, "%s: block size too small, min is %d",
				    optarg, MINBSIZE);
			if (bsize > MAXBSIZE)
				errx(1, "%s: block size too large, max is %d",
				    optarg, MAXBSIZE);
			break;
		case 'c':
			maxblkspercg = atoi(optarg);
			break;
		case 'd':
			maxbsize = atoi(optarg);
			if (maxbsize < MINBSIZE)
				errx(1, "%s: bad extent block size", optarg);
			break;
		case 'e':
			maxbpg = atoi(optarg);
			break;
		case 'f':
			fsize = atoi(optarg);
			break;
		case 'g':
			avgfilesize = atoi(optarg);
			break;
		case 'h':
			avgfilesperdir = atoi(optarg);
			break;
		case 'i':
			density = atoi(optarg);
			break;
		case 'l':
			lflag = 1;
			break;
		case 'k':
			if ((metaspace = atoi(optarg)) < 0)
				errx(1, "%s: bad metadata space %%", optarg);
			if (metaspace == 0)
				/* force to stay zero in mkfs */
				metaspace = -1;
			break;
		case 'm':
			if ((minfree = atoi(optarg)) < 0 || minfree > 99)
				errx(1, "%s: bad free space %%", optarg);
			break;
		case 'n':
			nflag = 1;
			break;
		case 'o':
			if (strcmp(optarg, "space") == 0)
				opt = FS_OPTSPACE;
			else if (strcmp(optarg, "time") == 0)
				opt = FS_OPTTIME;
			else
				errx(1, "%s: unknown optimization preference: use `space' or `time'",
				    optarg);
			break;
		case 'r':
			reserved = atoi(optarg);
			break;
		case 'p':
			break;

		case 's':
			fssize = atoi(optarg);
			break;
		case 't':
			tflag = 1;
			break;
		case '?':
		default:
			usage(prog_name);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage(prog_name);

	special = argv[0];
	if (!special[0])
		err(1, "empty file/special name");
	cp = strrchr(special, '/');
	if (cp == NULL) {
		/*
		 * No path prefix; try prefixing _PATH_DEV.
		 */
		snprintf(device, sizeof(device), "%s%s", _PATH_DEV, special);
		special = device;
	}


	d_name = special;

	d_fd = open(special, O_RDWR);
	if (d_fd < 0 && !Nflag)
		errx(1, "failed to open disk for writing %s: ", special);

	#ifndef __linux__
		#define BLKSSZGET 1
		#define BLKGETSIZE64 2
	#endif

	if (sectorsize == 0)
		if (ioctl(d_fd, BLKSSZGET, &sectorsize) == -1)
		    	err(1, "can't get sector size");	
	   
	if (mediasize == 0)
		if(ioctl(d_fd, BLKGETSIZE64, &mediasize) == -1)
			err(1, "can't get media size");

	fssize = mediasize / sectorsize - reserved;
	if (fsize <= 0)
		fsize = MAX(DFL_FRAGSIZE, sectorsize);
	if (bsize <= 0)
		bsize = MIN(DFL_BLKSIZE, 8 * fsize);
	
	/* Use soft updates by default for UFS2 and above */
	if (Oflag > 1)
		Uflag = 1;
	realsectorsize = sectorsize;

	mkfs(d_name);

	close(d_fd);
	

    return 0;


}



