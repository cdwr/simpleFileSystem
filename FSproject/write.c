int write_file(int fd, char *buf, int nbytes)
{
	char *cp, *cq;
	int block_12, block_13, *indexDoubleBlock, *doubleIndirectIndex1, *doubleIndirectIndex2;
	char indirectBuf[BLKSIZE], directBuf1[BLKSIZE], directBuf2[BLKSIZE], writeBuf[BLKSIZE], utilBuf[BLKSIZE];
	int pblk, lblk, start, remain, count = 0;
	OFT *oftp;
	MINODE *mip;
	INODE *ip;

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

	memset(writeBuf, 0,BLKSIZE);
	cq = buf;

	// get fd and MINODE of OFTP
	oftp = running->fd[fd];
	mip = oftp->mptr;

	while (nbytes)
	{
		lblk = oftp->offset / BLKSIZE;
		start = oftp->offset % BLKSIZE;

		// direct block
		if (lblk < 12)
		{
			if (mip->INODE.i_block[lblk] == 0)
			{
				// Allocate new block
				mip->INODE.i_block[lblk] = balloc(mip->dev);
			}

			pblk = mip->INODE.i_block[lblk];
		}
		// single indirect block
		else if (lblk >= 12 && lblk < 256 + 12)
		{
			block_12 = mip->INODE.i_block[12];
			if (block_12 == 0)
			{
				block_12 = mip->INODE.i_block[12] = balloc(mip->dev);

				// zero out the block
				memset(utilBuf, 0, BLKSIZE);
				put_block(mip->dev, block_12, utilBuf);
			}

			get_block(dev, block_12, indirectBuf);
			indexDoubleBlock = (int *)indirectBuf;
			
			pblk = indexDoubleBlock[lblk - 12];
			// no data present
			if (pblk == 0)
			{
				indexDoubleBlock[lblk - 12] = balloc(mip->dev);
				pblk = indexDoubleBlock[lblk - 12];
				put_block(mip->dev, block_12, indirectBuf);
			}
		}
		// double indirect block
		else
		{
			block_13 = mip->INODE.i_block[13];

			if (block_13 == 0)
			{
				mip->INODE.i_block[13] = balloc(mip->dev);
				block_13 = mip->INODE.i_block[13];

				// zero out buffer
				memset(utilBuf, 0, BLKSIZE);
				put_block(mip->dev, block_13, utilBuf);
			}
		
			get_block(dev, block_13, directBuf1);
			doubleIndirectIndex1 = (int *)directBuf1;
			lblk -= (256 + 12);

			if (doubleIndirectIndex1[lblk / 256] == 0)
			{
				doubleIndirectIndex1[lblk / 256] = balloc(mip->dev);

				put_block(mip->dev, block_13, directBuf1);
			}

			get_block(dev, doubleIndirectIndex1[lblk / 256], directBuf2);
			doubleIndirectIndex2 = (int *)directBuf2;

			if (doubleIndirectIndex2[lblk % 256] == 0)
			{
				doubleIndirectIndex2[lblk % 256] = balloc(mip->dev);
				pblk = doubleIndirectIndex2[lblk % 256];

				put_block(mip->dev, doubleIndirectIndex1[lblk / 256], directBuf2);
			}
		}

		get_block(mip->dev, pblk, writeBuf);
		cp = writeBuf + start;
		remain = BLKSIZE - start;

		while (remain > 0 && nbytes > 0)
		{
			if (remain <= nbytes)
			{
				memcpy(cp, cq, remain);
				oftp->offset += remain;
				count += remain;
				nbytes -= remain;
				if (oftp->offset > mip->INODE.i_size)
				{
					mip->INODE.i_size += remain;
				}

				remain -= remain;
			}
			else
			{
				memcpy(cp, cq, nbytes);
				oftp->offset += nbytes;
				count += nbytes;
				remain -= nbytes;
				if (oftp->offset > mip->INODE.i_size)
				{
					mip->INODE.i_size += nbytes;
				}

				nbytes -= nbytes;
			}

			// no more bytes to write
			if (nbytes <= 0)
			{
				break;
			}
		}

		put_block(mip->dev, pblk, writeBuf);

	}

	mip->dirty = 1;
	printf("wrote %d char into file descriptor fd=%d\n", count, fd);
	return nbytes;
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
		cbuf[n] = 0;
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