int read_file(int fd, char *buf, int nbytes)
{
	OFT *oftp;
	MINODE *mip;
	INODE *ip;
	int blk, lbk, start, remain, avil, count, next;
	char *cq, *cp, rBuf[BLKSIZE];
	int bbuf[BLKSIZE / sizeof(int)];
	
	if (fd < 0 || fd >= NFD)
	{
		printf("Error, file descriptor %d out of range\n", fd);
		return -1;
	}
	
	oftp = running->fd[fd];
	
	if (oftp == NULL)
	{
		printf("Error, no such file desctiptor %d\n", fd);
		return -1;
	}

	if (oftp->mode != 0 && oftp->mode != 2)
	{
		printf("Error, file is not open for read mode\n");
		return -1;
	}

	oftp = running->fd[fd];
	mip = oftp->mptr;
	ip = &mip->INODE;
	avil = ip->i_size - oftp->offset;
	cq = buf;
	count = 0;
	while (nbytes > 0 && avil > 0)
	{
	
		lbk = oftp->offset / BLKSIZE;
		start = oftp->offset % BLKSIZE;

		// Deal with direct blockes
		if (lbk < 12)
		{
			blk = ip->i_block[lbk];
		}
		// Single indirect blocks
		else if (lbk >= 12 && lbk < 256 + 12)
		{
			get_block(mip->dev, ip->i_block[12], (char *)bbuf);
			blk = bbuf[lbk - 12];
		}
		// Double indirect blocks
		else
		{
			lbk -= (256 + 12);
			get_block(mip->dev, ip->i_block[13], (char *)bbuf);
			get_block(mip->dev, bbuf[lbk / 256], (char *)bbuf);
			blk = bbuf[lbk % 256];
		}


		get_block(mip->dev, blk, rBuf);
		cp = rBuf + start;
		remain = BLKSIZE - start;
		next = (nbytes < remain ? nbytes : remain);
		next = (next < avil ? next : avil);

		memcpy(&buf[count], cp, next);
		*cp += next;
		oftp->offset += next;
		count += next;
		avil -= next;
		nbytes -= next;
		remain -= next;

	}
	return count;
}

int cat(char *filename)
{
	char buf[BLKSIZE];
	int n;
	int fd = open_file(filename, 0);
	printf("Content of file %s:\n", filename);
	while (n = read_file(fd, buf, BLKSIZE)) {
		buf[n] = 0;
		printf("%s", buf);
	}

	printf("\n");
	close_file(fd);
}