int open_file(char *pathname, int mode)
{
	printf("opening pathname=%s, with mode=%d\n", pathname, mode);

	int ino, i;
	MINODE *mip;
	INODE *ip;
	OFT *oftp;

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

	// Check whether the file is ALREADY opened with INCOMPATIBLE mode:
	// If it's already opened for W, RW, APPEND : reject.
	// (that is, only multiple R are OK)
	if (!can_open(pathname, mode)) {
		iput(mip);
		return -1;
	}

}