int open_file(char *pathname, int mode)
{
	printf("opening pathname=%s, with mode=%d\n", pathname, mode);

	int ino;
	MINODE *mip;
	INODE *ip;
	OFT *oftp;
	char oMode;

	// get pathname's inumber:
	if (pathname[0] == '/')
	{
		dev = root->dev;
	}
	else
	{
		dev = running->cwd->dev;
	}
	
	// Get ino and parent INO
	ino = getino(pathname);
	mip = iget(dev, ino);

	// Check if our file is a regular file.
	if (!S_ISREG(mip->INODE.i_mode)) {
		printf("%s is not a regular file\n", pathname);
		return -1;
	}


	if(mode == 'r' || mode == 'R')
		oMode = 'r';
	else
		oMode = 'w';


	if (!maccess(mip, oMode)){ // char = 'r' for R; 'w' for W, RW, APPEND
      iput(mip); 
	  return -1;
   }

	// Check whether the file is ALREADY opened with INCOMPATIBLE mode:
	// If it's already opened for W, RW, APPEND : reject.
	// (that is, only multiple R are OK)
	if (!can_open(pathname, mode)) {
		iput(mip);
		return -1;
	}

	// Now we need to allocate free OFT.
	oftp = malloc(sizeof(OFT));
	oftp->mode = mode;
	oftp->refCount = 1;
	oftp->mptr = mip;

	// Depending on the open mode 0|1|2|3, set the OFT's offset accordingly:
	switch(mode)
	{
		case 0 : 
			oftp->offset = 0;     // R: offset = 0
			break;
		case 1 : 
			truncate(mip);        // W: truncate file to 0 size
			oftp->offset = 0;
			break;
		case 2 : 
			oftp->offset = 0;     // RW: do NOT truncate file
			break;
		case 3 : 
			oftp->offset =  mip->INODE.i_size;  // APPEND mode
			break;
		default: 
			printf("invalid mode\n");
			return(-1);
	}

	int i;
	for (i = 0; i < NFD; i++)
	{
		if (running->fd[i] == NULL)
			break;
	}

	if (running->fd[i] != NULL) {
		printf("too many open instances of %s\n", pathname);
	}

	running->fd[i] = oftp;
	if (mode == READ)
	{
		mip->INODE.i_atime = mode;
	}

	if (mip->INODE.i_mtime > READ)
	{
		mip->INODE.i_mtime = time(0L);
	}

	mip->dirty = 1;
	printf("File=%s is now open with mode=%s\n", pathname, mode_to_string(mode));
	return i;
}

int close_file(int fd)
{
	OFT *oftp;
	// Check if given descript is within range.
	if (fd < 0 || fd >= NFD)
	{
		printf("Error, file descript %d is not in range\n", fd);
	}

	// Check if OFT entry exist in running.
	if (running->fd[fd] == NULL)
	{
		printf("Error, no such OFT entry\n");
		return -1;
	}

	// close.
	oftp = running->fd[fd];
	running->fd[fd] = 0;
	oftp->refCount--;
	if (oftp->refCount > 0)
	{
		return 0;
	}

	// dispose of MINODE
	iput(oftp->mptr);
	printf("fd=%d, mode=%s, INODE=%d is now closed\n", fd, mode_to_string(oftp->mode), oftp->mptr->ino);
	return 0;
}

int lseek_file(int fd, int position)
{
	OFT *oftp;
	int original; // original position

	// Check if given descript is within range.
	if (fd < 0 || fd >= NFD)
	{
		printf("Error, file descript %d is not in range\n", fd);
	}

	// Check if OFT entry exist in running.
	if (running->fd[fd] == NULL)
	{
		printf("Error, no such OFT entry\n");
		return -1;
	}

	oftp = running->fd[fd];
	original = oftp->offset;

	if (position < 0)
	{
		oftp->offset = 0;
	}
	else if (position >= oftp->mptr->INODE.i_size)
	{
		oftp->offset = oftp->mptr->INODE.i_size - 1;
	}
	else
	{
		oftp->offset = position;
	}

	return original;
}

// Duplicate given file descriptor
int dup(int fd)
{
	OFT *oftp;
	int i;
	if (fd < 0 || fd >= NFD)
	{
		printf("Error, file descriptor %d not in range\n", fd);
	}

	if (running->fd[fd] == NULL)
	{
		printf("Erro, OFT entry does not exist\n");
		return -1;
	}

	oftp = running->fd[fd];
	for (i=0; i<NFD; i++)
	{
		if (running->fd[i] == NULL)
		{
			running->fd[i] = oftp;
			oftp->refCount++;
			return i;
		}
	}

	printf("Error, file is open multiple times!\n");
	return -1;	
}

// Duplicate given file descriptor into another given
int dup2(int fd, int gd)
{
	OFT *oftp;
	int i;
	if (fd < 0 || fd >= NFD)
	{
		printf("Error, file descriptor %d not in range\n", fd);
	}

	if (running->fd[fd] == NULL)
	{
		printf("Erro, OFT entry does not exist\n");
		return -1;
	}

	oftp = running->fd[fd];
	if (gd < 0 || gd >= NFD) {
		printf("FD copy not in range\n");
		return -1;
	}

	if (running->fd[gd] != NULL)
		close_file(gd);
	running->fd[gd] = oftp;
	oftp->refCount++;
	return 0;
}