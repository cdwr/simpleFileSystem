int link(char *oldpath, char *newpath)
{
	int oldino, newino;
	char *parent, *child, pathcpy[64];
	MINODE *oldmip, *newmip;
	INODE *oldip, *newip;
	char *cp, buf[BLKSIZE];
	DIR *dp;

	// Check if old path is in different folder.
	if (oldpath[0] == '/')
	{
		dev = root->dev;
	}
	else
	{
		dev = running->cwd->dev;
	}
	
	// Get old ino, mip and ip.
	oldino = getino(oldpath);
	oldmip = iget(dev, oldino);
	oldip = &oldmip->INODE;
	

	// Check if oldnode is regular file and is already a link
	if (!S_ISREG(oldip->i_mode) && !S_ISLNK(oldip->i_mode))
	{
		printf("%s is an invalid file type\n", oldpath);
		iput(oldmip);
		return -1;
	}

	// Get child and parent names.
	strcpy(pathcpy, newpath);
	child = basename(pathcpy);
	parent = dirname(newpath);

	// Check if parent of new path is in different folder.
	if (parent[0] == '/')
	{
		dev = root->dev;
	}
	else
	{
		dev = running->cwd->dev;
	}
	
	// Get new ino, mip and ip.
	newino = getino(parent);
	newmip = iget(dev, newino);
	newip = &newmip->INODE;

	// Check if new path is not a directory.
	if (!S_ISDIR(newip->i_mode))
	{
		printf("%s is not a directory");
		iput(oldmip);
		iput(newmip);
		return -1;
	}

	// Search if newmip already exists.
	if (search(newmip, child)) {
		printf("%s/%s already exists", parent, child);
		iput(oldmip);
		iput(newmip);
		return -1;
	}

	//permissions check
	if (!maccess(oldmip, 'w')){
      printf("makedir: Access Denied\n");
      iput(oldmip);
	  iput(newmip);
	  return -1;
	}

	// Check if they are in the same filesystem.
	if (oldmip->dev != newmip->dev)
	{
		printf("%s and %s/%s must be on same device\n", oldpath, parent, child);
		iput(oldmip);
		iput(newmip);
		return -1;
	}

	// Enter name of new link pointing to old ino.
	enter_name(newmip, oldino, child);
	oldip->i_links_count++; // increase count of links.
	oldmip->dirty = 1; // mark it as dirty to be written to disk.
	iput(oldmip);
	iput(newmip);
}

int unlink(char *pathname)
{
	int ino, pino;
	MINODE *mip, *pip;
	INODE *ip;
	char *parent, *child;

	// Get child name and dirname.
	child = basename(pathname);
	parent = dirname(pathname);

	// Check if old path is in different folder.
	if (parent[0] == '/')
	{
		dev = root->dev;
	}
	else
	{
		dev = running->cwd->dev;
	}
	
	// Get number of parent ino and parent ino itself.
	pino = getino(parent);
	pip = iget(dev, pino);

	// Check if node is a directory type.
	if (!S_ISDIR(pip->INODE.i_mode))
	{
		printf("%s must be a directory\n");
		return -1;
	}

		//check if permissions are met
	if(running->uid != 0)
	{
		if(running->uid != pip->INODE.i_uid)
		{
			printf("Invalid file permissions; user:%d, inode->uid:%d\n", running->uid, pip->INODE.i_uid);
			iput(pip);
			return -1;
		}
	}

	// Get child ino number and inode itself.
	ino = search(pip, child);
	mip = iget(dev, ino);
	ip = &mip->INODE;

	// Check if the child we are removing is a link type and not a folder.
	if (!S_ISREG(ip->i_mode) && !S_ISLNK(ip->i_mode))
	{
		printf("%s is an invalid file type\n", pathname);
		return -1;
	}

	ip->i_links_count--; // decrease link counts.

	// If there are no links left, we need to remove the fild from the disk.
	if (ip->i_links_count == 0) 
	{
		truncate(mip);
	}

	// finally remove child.
	rm_child(pip, child);
	iput(mip);
	iput(pip);
}


int truncate(MINODE *mip)
{
	INODE *ip;
	int i;
	int buf1[BLKSIZE / sizeof(int)], buf2[BLKSIZE / sizeof(int)], buf3[BLKSIZE / sizeof(int)];
	int *block_pointer1, *block_pointer2, *block_pointer3;
	ip = &mip->INODE;

	// deal with direct blocks
	for(i=0; i<12; i++)
	{
		if (ip->i_block[i])
		{
			continue;
		}

		bdealloc(mip->dev, ip->i_block[i]);
	}

	// deal with single indirect blocks
	if (ip->i_block[12]) {
		get_block(mip->dev, ip->i_block[12], (char *)buf1);
		block_pointer1 = buf1;
		while (block_pointer1 < &buf1[BLKSIZE / sizeof(int)])
		{
			if (*block_pointer1) bdealloc(mip->dev, *block_pointer1);
			{
				block_pointer1++;
			}
		}
		bdealloc(mip->dev, ip->i_block[12]);
	}
	// deal with double indirect blocks
	if (ip->i_block[13]) {
		get_block(mip->dev, ip->i_block[13], (char *)buf1);
		block_pointer1 = buf1;
		while (block_pointer1 < &buf1[BLKSIZE / sizeof(int)])
		{
			if (*block_pointer1)
			{
				get_block(mip->dev, *block_pointer1, (char *)buf2);
				block_pointer2 = buf2;
				while (block_pointer2 < &buf2[BLKSIZE / sizeof(int)])
				{
					if (*block_pointer2) bdealloc(mip->dev, *block_pointer2);
					{
						block_pointer2++;
					}
				}

				bdealloc(mip->dev, *block_pointer1);
			}

			block_pointer1++;
		}

		bdealloc(mip->dev, ip->i_block[13]);
	}
	// deal with triple indirect blocks
	if (ip->i_block[14])
	{
		get_block(mip->dev, ip->i_block[14], (char *)buf1);
		block_pointer1 = buf1;
		while (block_pointer1 < &buf1[BLKSIZE / sizeof(int)])
		{
			if (*block_pointer1)
			{
				get_block(mip->dev, *block_pointer1, (char *)buf2);
				block_pointer2 = buf2;
				while (block_pointer2 < &buf2[BLKSIZE / sizeof(int)])
				{
					if (*block_pointer2)
					{
						get_block(mip->dev, *block_pointer2, (char *)buf3);
						block_pointer3 = buf3;
						while (block_pointer3 < &buf3[BLKSIZE / sizeof(int)])
						{
							if (*block_pointer3) bdealloc(mip->dev, *block_pointer3);
							{
								block_pointer3++;
							}
						}

						bdealloc(mip->dev, *block_pointer2);
					}

					block_pointer2++;
				}

				bdealloc(mip->dev, *block_pointer1);
			}

			block_pointer1++;
		}

		bdealloc(mip->dev, ip->i_block[14]);
	}
}