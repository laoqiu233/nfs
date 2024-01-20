#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include "http.h"

MODULE_LICENSE("GPL");

struct dentry* networkfs_lookup(struct inode *parent_inode, struct dentry *dentry, unsigned int flag) {
    printk("lookup\n");
    printk(dentry->d_name.name);
    return dentry;
}

struct inode_operations networkfs_inode_ops = {
    .lookup = networkfs_lookup,
};

int networkfs_iterate(struct file *filp, struct dir_context *ctx) {
    char fsname[10];
    struct dentry *dentry = filp->f_path.dentry;
    struct inode *inode = dentry->d_inode;
    unsigned long offset = filp->f_pos;
    int stored = 0;
    unsigned char ftype;
    ino_t ino = inode->i_ino;
    ino_t dino;

    if (ino == 1000)
    {
        if (offset == 0) {
            strcpy(fsname, ".");
            ftype = DT_DIR;
            dino = ino;
        }
        else if (offset == 1) {
            strcpy(fsname, "..");
            ftype = DT_DIR;
            dino = dentry->d_parent->d_inode->i_ino;
        }
        else if (offset == 2) {
            strcpy(fsname, "test.txt");
            ftype = DT_REG;
            dino = 101;
        }
        ctx->pos++;
        dir_emit(ctx, fsname, strlen(fsname), dino, ftype);
        return 3;
    } else {
        return stored;
    }
}

struct file_operations networkfs_inode_fops = {
    .iterate = networkfs_iterate
};

void networkfs_kill_sb(struct super_block *sb) {
    printk(KERN_INFO "networkfs super block is destroyed. Unmount successfully.\n");
}

struct inode *networkfs_get_inode(struct super_block *sb, const struct inode *dir, umode_t mode, int i_ino) {
    struct inode *inode = new_inode(sb);
    inode->i_op = &networkfs_inode_ops;
    inode->i_ino = i_ino;
    if (inode != NULL) {
        inode_init_owner(sb->s_user_ns, inode, dir, mode);
    }
    return inode;
}

int networkfs_fill_super(struct super_block *sb, void *data, int silent) {
    struct inode *inode = networkfs_get_inode(sb, NULL, S_IFDIR | 0777, 1000);
    networkfs_get_inode(sb, inode, S_IFREG | 0777, 101);
    inode->i_fop = &networkfs_inode_fops;
    sb->s_root = d_make_root(inode);
    if (sb->s_root == NULL) {
        return -ENOMEM;
    }
    printk(KERN_INFO "return 0\n");
    return 0;
}

struct dentry *networkfs_mount(struct file_system_type *fs_type, int flags, const char *token, void *data) {
    struct dentry *ret = mount_nodev(fs_type, flags, data, networkfs_fill_super); 
    if (ret == NULL) { 
        printk(KERN_ERR "Can't mount file system");
    } else { 
        printk(KERN_INFO "Mounted successfuly"); 
    }
    return ret;
}

struct file_system_type networkfs_type = {
    .name = "networkfs",
    .mount = networkfs_mount,
    .kill_sb = networkfs_kill_sb,
};

int init_module(void) 
{ 
    pr_info("Mounting module\n");
    register_filesystem(&networkfs_type);

    return 0; 
} 
 
void cleanup_module(void) 
{ 
    pr_info("Umounting module\n"); 
    unregister_filesystem(&networkfs_type);
}
