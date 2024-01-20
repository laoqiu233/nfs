//
// Created by big l on 20.01.2024.
//

#ifndef NETWORKFS
#define NETWORKFS
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

struct inode *networkfs_get_inode(struct super_block *sb, const struct inode *dir, umode_t mode, int i_ino);

#endif