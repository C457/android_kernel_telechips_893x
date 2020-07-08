/****************************************************************************
  *	 TCC Version 0.6
  *	 Copyright (c) telechips, Inc.
  *	 ALL RIGHTS RESERVED
  *
****************************************************************************/

#define TRUE   1
#define FALSE  0


static int tccxxx_wdma_mmap(struct file *file, struct vm_area_struct *vma);
long tccxxx_wdma_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
int tccxxx_wdma_release(struct inode *inode, struct file *filp);
int tccxxx_wdma_open(struct inode *inode, struct file *filp);

