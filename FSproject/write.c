int write_file(int fd, char *buf, int nbytes)
{
	OFT *oftp;
	MINODE *mip;
	INODE *ip;
	int blk, lbk, ptrblk, start, remain, count, next;
	char *cq, *cp, wBuf[BLKSIZE];
	int ibuf[BLKSIZE / sizeof(int)];

	// Check file descriptor to be within range.
	if (fd < 0 || fd >= NFD)
	{
		printf("Error, file descriptor %d out of range\n", fd);
		return -1;
	}

	oftp = running->fd[fd];

	// Check if we recieved corrent descripted with corrent fd.
	if (oftp == NULL)
	{
		printf("Error, no such file desctiptor\n");
		return -1;
	}

	// Check if we have the correct mode.
	if (oftp->mode != WRITE && oftp->mode != READWRITE && oftp->mode != APPEND)
	{
		printf("Error, file is not open for a modify mode\n");
		return -1;
	}

	// Get OFTP structure again, MINODE, INODE.
	oftp = running->fd[fd];
	mip = oftp->mptr;
	ip = &mip->INODE;
	cq = buf;
	count = 0;
	while (nbytes > 0)
	{
		lbk = oftp->offset / BLKSIZE;
		start = oftp->offset % BLKSIZE;

		// Deal with direct blockes
		if (lbk < 12)
		{
			if (ip->i_block[lbk] == 0)
			{
				ip->i_block[lbk] = balloc(mip->dev);
				memset(wBuf, 0, BLKSIZE);
				put_block(mip->dev, ip->i_block[lbk], wBuf);
			}
			blk = ip->i_block[blk];
		}
		// Single indirect blocks
		else if (lbk >= 12 && lbk < 256 + 12)
		{
			if (ip->i_block[12] == 0)
			{
				ip->i_block[12] = balloc(mip->dev);
			}
			get_block(mip->dev, ip->i_block[12], (char *)ibuf);
			if (ibuf[lbk - 12] == 0)
			{
				ibuf[lbk - 12] = balloc(mip->dev);
				put_block(mip->dev, ip->i_block[12], (char *)ibuf);
			}
			blk = ibuf[lbk - 12];
		}
		// Double indirect blocks
		else
		{
			lbk -= 256 + 12;
			if (ip->i_block[13] == 0)
			{
				ip->i_block[13] = balloc(mip->dev);
			}

			get_block(mip->dev, ip->i_block[13], (char *)ibuf);
			if (ibuf[lbk / 256] == 0)
			{
				ibuf[lbk / 256] = balloc(mip->dev);
				put_block(mip->dev, ip->i_block[13], (char *)ibuf);
			}

			ptrblk = ibuf[lbk / 256];
			get_block(mip->dev, ptrblk, (char *)ibuf);
			if (ibuf[lbk % 256] == 0)
			{
				ibuf[lbk %256] = balloc(mip->dev);
				put_block(mip->dev, ptrblk, (char *)ibuf);
			}

			blk = ibuf[lbk % 256];
		}

		get_block(mip->dev, blk, wBuf);
		cp = wBuf + start;
		remain = BLKSIZE - start;
		next = nbytes < remain ? nbytes : remain;
		memcpy(&wBuf[count], cq, next);
		cq += next;
		nbytes -= next;
		remain -= next;
		count += next;
		oftp->offset += next;
		if (oftp->offset > ip->i_size)
		{
			ip->i_size = oftp->offset;
		}
		put_block(mip->dev, blk, wBuf);
	}
	mip->dirty = 1;
	printf("wrote %d char into file descriptor fd=%d\n", count, fd);
	return count;
}

int cp(char *src, char *dest)
{
	int fd, gd, n;
	char cbuf[BLKSIZE];
	fd = open_file(src, READ);
	create_file(dest);
	gd = open_file(dest, READWRITE);
	while (n = read_file(fd, cbuf, BLKSIZE))
	{
		printf("buf=%s\n n=%d", cbuf, n);
		write_file(gd, cbuf, n);
	}

	close_file(fd);
	close_file(gd);
	return 0;
}

int mv(char *src, char *dest)
{
	int sdev, ddev;
	if (src[0] == '/')
	{
		sdev = root->dev;
	}
	else
	{
		sdev = running->cwd->dev;
	}

	if (getino(src) == 0) {
		printf("%s must exist\n", src);
		return -1;
	}

	if (dest[0] == '/')
	{
		ddev = root->dev;
	}
	else
	{
		ddev = running->cwd->dev;
	}

	getino(dest);
	if(sdev == ddev)
		link(src, dest);
	else
		cp(src, dest);
	unlink(src);
	return 0;
}

int write_to_file(char *pathname, char *what_to_write)
{
	int gd = open_file(pathname, READWRITE);
	write_file(gd, what_to_write, strlen(what_to_write));
	close_file(gd);
	printf("Wrote %s into %s file.", what_to_write, pathname);
	return 0;
}


