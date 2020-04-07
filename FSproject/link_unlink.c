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