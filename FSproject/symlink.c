int mysymlink(char *target, char *linkpath)
{
	int tino, lino;
	MINODE *tmip, *lmip;
	INODE *tip, *lip;
	char linkpathcpy[64], targetcpy[64];

	// Check if target is in different folder.
	if (target[0] == '/')
	{
		dev = root->dev;
	}
	else
	{
		dev = running->cwd->dev;
	}

	// Get target INODE, MINODE and ip.
	strcpy(targetcpy, target);
	tino = getino(target);
	tmip = iget(dev, tino);
	tip = &tmip->INODE;

	// Check if target is a directory or a regular file.
	if (!S_ISDIR(tip->i_mode) && !S_ISREG(tip->i_mode)) 
	{
		printf("error: target must be a directory or regular file\n");
		iput(tmip);
		return -1;
	}

	strcpy(linkpathcpy, linkpath);
	create_file(linkpath);
	printf("linkpath=%s\n", linkpath);

	// Get link INODE, MINODE and ip.
	lino = getino(linkpathcpy);
	lmip = iget(dev, lino);
	lip = &lmip->INODE;

	// Set i_mode of link ip.
	lip->i_mode &= ~(0170000);
	lip->i_mode |= 0120000;
	strcpy((char *)lip->i_block, targetcpy);
	lmip->dirty = 1; // mark as dirty to write it.
	iput(lmip);
	iput(tmip);

	printf("symlink created\n");
}

int myreadlink(char *pathname, char *linkname)
{
	int ino;
	MINODE *mip;
	INODE *ip;

	if (pathname[0] == '/')
		dev = root->dev;
	else
		dev = running->cwd->dev;

	ino = getino(pathname);
	mip = iget(dev, ino);


	if (!S_ISLNK(mip->INODE.i_mode)) {
		printf("file must be a symbolic link\n");
		iput(mip);
		return -1;
	}
	strcpy(linkname, (char *)mip->INODE.i_block);
	return 0;
}