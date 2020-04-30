/************* funcs.c file **************/

int chdir(char *pathname)
{
	// READ Chapter 11.7.3 HOW TO chdir
	MINODE *mip;
	printf("cwd:[%d %d]\n", running->cwd->dev, running->cwd->ino);
	if(pathname[0]<=0)
	{
		running->cwd = iget(root->dev, 2);
		return 0;
	}

	if(strcmp(pathname, "/") == 0)
	{
		printf("CDing to root\n");
		running->cwd = iget(root->dev, 2);
		return 0;
	}

	int ino = getino(pathname);

	if(ino==0)
	{
		printf("directory doesn't exist\n");
		return 0;
	}

	mip = iget(dev, ino);
	if((mip->INODE.i_mode & 0100000) == 0100000)
	{
		iput(mip);
		printf("cannot cd to non dir\n");
		return -1;
	}

	iput(running->cwd);
	running->cwd = mip;
	printf("cwd:[%d %d]\n", running->cwd->dev, running->cwd->ino);
	return 0;
}

int ls_file(MINODE *mip, char *name)
{
	INODE *ip = &mip->INODE;
	//printf("ls_file: to be done: READ textbook for HOW TO!!!!\n");
	if(S_ISDIR(mip->INODE.i_mode))
		printf("d");
	if(S_ISREG(mip->INODE.i_mode))
		printf("r");
	if(S_ISLNK(mip->INODE.i_mode))
		printf("l");

	printf((mip->INODE.i_mode & 1 << 8) ? "r" : "-");
	printf((mip->INODE.i_mode & 1 << 7) ? "w" : "-");
	printf((mip->INODE.i_mode & 1 << 6) ? "x" : "-");

	printf((mip->INODE.i_mode & 1 << 5) ? "r" : "-");
	printf((mip->INODE.i_mode & 1 << 4) ? "w" : "-");
	printf((mip->INODE.i_mode & 1 << 3) ? "x" : "-");

	printf((mip->INODE.i_mode & 1 << 2) ? "r" : "-");
	printf((mip->INODE.i_mode & 1 << 1) ? "w" : "-");
	printf((mip->INODE.i_mode & 1 << 0) ? "x" : "-");

	char time[64];
	ctime_r((time_t *)&mip->INODE.i_mtime, time);
	time[strlen(time)-1]=0;
	printf(" %3d\t%3d\t%3d\t%20s\t%6d ", ip->i_links_count,  mip->INODE.i_uid, mip->INODE.i_gid, time, mip->INODE.i_size);

	if(S_ISLNK(mip->INODE.i_mode))
	{
		printf("\t%s -> %s", name, mip->INODE.i_block);
	}
	else
	{
		printf("\t%s", name);
	}
	printf("\t%4s[%d %d]\n", "", mip->dev, mip->ino);
}

int ls_dir(MINODE *mip)
{
	char buf[BLKSIZE], temp[256];
	DIR *dp;
	char *cp;
	MINODE *file;
	// Assume DIR has only one data block i_block[0]
	get_block(mip->dev, mip->INODE.i_block[0], buf); 
	dp = (DIR *)buf;
	cp = buf;

	while (cp < buf + BLKSIZE)
	{
		strncpy(temp, dp->name, dp->name_len);
		temp[dp->name_len] = 0;
		int original_dev = dev;
		int ino = getino(temp);

		file = iget(original_dev, ino);
		//printf("[%d %d]", dev, original_dev);

		// Special case of mounting point
		if (dev != original_dev)
		{
			// We have mounting up
			if (strcmp(temp, "..") == 0)
			{
				dev = original_dev;
			}
			else
			{

				// mounting down
				for (int i = 0; i < NMTABLE; i++)
				{
					MTABLE *mt = &mtable[i];
					if (original_dev == mt->dev)
					{
						//printf("dev=%d ino=%d ", file->dev, file->ino);
						file = mt->mntDirPtr;
						//printf("dev=%d ino=%d ", file->dev, file->ino);
						dev = file->dev;
						break;
					}
				}
			}

		}
		//printf("dev=%d ino=%d ", file->dev, file->ino);
		ls_file(file, temp);
		iput(file);
		
		cp += dp->rec_len;
		dp = (DIR *)cp;
	}

	printf("\n");
}

int ls(char *pathname)
{
	if(strcmp(pathname, "\0") != 0)
	{
		printf("ls %s\n", pathname);
		chdir(pathname);
		ls_dir(running->cwd);
		chdir("..");
	}
	else{
		ls_dir(running->cwd);
	}
	return 0;
}

void pwd(MINODE *wd, int child)
{
	char buf[512];
	char *cpy;
	char name[64];
	
	if (wd->ino == root->ino)
	{
		if (wd->dev != root->dev)
		{
			//printf("UP cross mounting point\n");

			// Get mountpoint MINODE
			for (int i = 0; i < NMTABLE; i++)
			{ 
				MTABLE *table = &mtable[i];
				if (table->dev == wd->dev)
				{
					printf("%s/", table->mntName);
					return;
				}
			}
		}

		printf("/");
		return;
	}

	MINODE *pip;

	pip = iget(wd->dev, findino(wd, 0));
	findmyname(pip, wd->ino, &name);
	

	if(wd->ino != root->ino)
	{
		pwd(pip, wd->ino);
		iput(pip);
		printf("%s/", name);
	}
	
  return;
}
