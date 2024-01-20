#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/inet.h>
#include "http.h"
#include "header.h"

MODULE_LICENSE("GPL");

void fix_string(const char *str, char *result, int len) {
  int i = 0;
  while (i < len) {
    result[i] = '%';
    sprintf(result + i + 1, "%02x", str[i / 3]);
    i += 3;
  }
}


struct dentry* networkfs_lookup(struct inode *parent_inode, struct dentry *dentry, unsigned int flag) {
    ino_t root = parent_inode->i_ino;
    const char *name = dentry->d_name.name;
    printk(KERN_INFO "Looking up %s\n", name);
    char *answer = kzalloc(2048, GFP_KERNEL);
    char my_ino[30];
    snprintf(my_ino, 30, "%lu", root);
    char enc_str[256 * 3 + 1];
    fix_string(name, enc_str, strlen(name) * 3);
    int err = networkfs_http_call("lookup", answer,
                            2048, 2, "inode", my_ino, "name",
                            enc_str);

    if (err < 0) {
        printk(KERN_INFO "Request for lookup failed, status: %d\n", err);
        return NULL;
    }

    char entry_type = answer[0];
    int ino;
    kstrtoint(answer + 1, 0, &ino);
    printk(KERN_INFO "Got type %c inode %d\n", entry_type, ino);

    struct inode *inode = networkfs_get_inode(parent_inode->i_sb, NULL,
                                entry_type == 'f' ? S_IFREG : S_IFDIR,
                                ino);
    d_add(dentry, inode);
    return NULL;
}

int create(struct inode *parent_inode, struct dentry *child_dentry, const char *type, int flag) {
    ino_t root = parent_inode->i_ino;
    const char *name = child_dentry->d_name.name;
    char *answer = kzalloc(2048, GFP_KERNEL);
    char my_ino[30];
    snprintf(my_ino, 30, "%lu", root);
    char enc_str[256 * 3 + 1];
    fix_string(name, enc_str, strlen(name) * 3);
    int err = networkfs_http_call("create", answer, sizeof(answer), 3, "parent_inode", my_ino, "name", enc_str, "create_type", type);

    if (err < 0) {
        printk(KERN_INFO "Request for create failed, status: %d\n", err);
        return -1;
    }

    uint64_t ino;
    kstrtou64(answer, 0, &ino);

    struct inode* inode = networkfs_get_inode(parent_inode->i_sb, NULL, flag, ino);
    d_add(child_dentry, inode);
    return 0;
}

int unlink(struct inode *parent_inode, struct dentry *child_dentry, const char *type) {
    ino_t root = parent_inode->i_ino;
    const char *name = child_dentry->d_name.name;
    char *answer = kzalloc(2048, GFP_KERNEL);
    char my_ino[30];
    snprintf(my_ino, 30, "%lu", root);
    char enc_str[256 * 3 + 1];
    fix_string(name, enc_str, strlen(name) * 3);
    int err = networkfs_http_call("unlink", answer, sizeof(answer), 3, "parent_inode", my_ino, "name", enc_str, "delete_type", type);

    if (err < 0) {
        printk(KERN_INFO "Request for unlink failed, status: %d\n", err);
        return -ENOTEMPTY;
    }
    return 0;
}

int networkfs_create(struct user_namespace *fs_namespace,
                     struct inode *parent_inode, struct dentry *child_dentry,
                     umode_t mode, bool b) {
  return create(parent_inode, child_dentry, "f", S_IFREG | S_IRWXUGO);
}

int networkfs_mkdir(struct user_namespace *fs_namespace,
                     struct inode *parent_inode, struct dentry *child_dentry,
                     umode_t mode) {
  return create(parent_inode, child_dentry, "d", S_IFDIR | S_IRWXUGO);
}

int networkfs_unlink(struct inode *parent_inode, struct dentry *child_dentry) {
    return unlink(parent_inode, child_dentry, "f");
}

int networkfs_unlink_dir(struct inode *parent_inode, struct dentry *child_dentry) {
    return unlink(parent_inode, child_dentry, "d");
}


struct inode_operations networkfs_inode_ops = {
    .lookup = networkfs_lookup,
    .create = networkfs_create,
    .unlink = networkfs_unlink,
    .rmdir  = networkfs_unlink_dir,
    .mkdir  = networkfs_mkdir,
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

    if (ino == 1000) {
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
        ctx->pos++;
        dir_emit(ctx, fsname, strlen(fsname), dino, ftype);
        return 3;
    } else {
        return stored;
    }
}

int network_list(struct file *filp, struct dir_context *ctx) {
    struct dentry *dentry = filp->f_path.dentry;
    struct inode *inode = dentry->d_inode;
    printk(KERN_INFO "offset: %lu\n", filp->f_pos);
    printk(KERN_INFO "listing inode: %lu\n", inode->i_ino);
    
    char* resp = kzalloc(8192, GFP_KERNEL);
    char my_ino[30];
    snprintf(my_ino, 30, "%lu", inode->i_ino);
    int err = networkfs_http_call("list", resp, 8192, 1, "inode", my_ino);

    if (err < 0) {
        printk(KERN_INFO "Failed while listing with status %d\n", err);
        return -1;
    }

    char *entries_count_s = strsep(&resp, "\n");
    printk(KERN_INFO "entires_count_s: %s\n", entries_count_s);
    int entries_count;
    kstrtoint(entries_count_s, 0, &entries_count);
    printk(KERN_INFO "Found %d entires\n", entries_count);

    if (ctx->pos >= 2 + entries_count) {
        return 0;
    }

    // offset += 1;
    ctx->pos += 1;
    dir_emit(ctx, ".", 1, inode->i_ino, DT_DIR);

    // offset += 1;
    ctx->pos += 1;
    dir_emit(ctx, "..", 2, dentry->d_parent->d_inode->i_ino, DT_DIR);

    for (int i = 0; i < entries_count; i++) {
        char *line = strsep(&resp, "\n");
        char *type = strsep(&line, " ");
        char *inode_s = strsep(&line, " ");
        uint64_t inode;
        kstrtou64(inode_s, 0, &inode);
        printk(KERN_INFO "Found entry, type: %c inode_s: %s inode: %d name: %s\n", type[0], inode_s, inode, line);
        ctx->pos += 1;
        dir_emit(ctx, line, strlen(line), inode, type[0] == 'f' ? S_IFREG : S_IFDIR);
    }

    return 2 + entries_count;
}

ssize_t read_data(struct file *filep, char __user *buff, size_t buffer_size, loff_t *offp) {
    ino_t inode = filep->f_inode->i_ino;
    printk(KERN_INFO "Reading file %s buffer size: %lu off: %lld\n", filep->f_path.dentry->d_name.name, buffer_size, *offp);

    char my_ino[30];
    snprintf(my_ino, 30, "%lu", inode);

    char size_buffer[30];
    int err = networkfs_http_call("read_size", size_buffer, 30, 1, "inode", my_ino);
    if (err < 0) {
        printk(KERN_INFO "Failed to read size for file %s\n", filep->f_path.dentry->d_name.name);
        return -1;
    }

    uint64_t file_size;
    kstrtou64(size_buffer, 0, &file_size);

    char *file_buffer = kzalloc(file_size, GFP_KERNEL);    
    err = networkfs_http_call("read", file_buffer, file_size, 1, "inode", my_ino);
    if (err < 0) {
        printk(KERN_INFO "Failed to read file %s\n", filep->f_path.dentry->d_name);
        return -1;
    }

    ssize_t cnt = file_size;
    ssize_t ret;

    ret = copy_to_user(buff, file_buffer, cnt);
    *offp += cnt - ret;
    
    if (*offp > cnt) 
        return 0;
    else
        return cnt;
}

ssize_t write_data(struct file *filep, const char __user *buff, size_t buffer_size, loff_t *offp) {
    ino_t inode = filep->f_inode->i_ino;
    printk(KERN_INFO "Writing file %s, buffer: %s buffer size: %lu off: %lld\n", filep->f_path.dentry->d_name.name, buff, buffer_size, *offp);
    char my_ino[30];
    snprintf(my_ino, 30, "%lu", inode);

    char *fixed_content = kzalloc(buffer_size * 3 + 1, GFP_KERNEL);
    fix_string(buff, fixed_content, buffer_size * 3);
    char *answer = kzalloc(10, GFP_KERNEL);

    int err = networkfs_http_call("write", answer, 10, 2, "inode", my_ino, "content", fixed_content);

    if (err < 0) {
        printk(KERN_INFO "Write request failed with status %d\n", err);
        return -1;
    }

    *offp += buffer_size;
    return buffer_size;
}

struct file_operations networkfs_inode_fops = {
    .iterate = network_list,
    .read    = read_data,
    .write   = write_data,
};

void networkfs_kill_sb(struct super_block *sb) {
    printk(KERN_INFO "networkfs super block is destroyed. Unmount successfully.\n");
}

struct inode *networkfs_get_inode(struct super_block *sb, const struct inode *dir, umode_t mode, int i_ino) {
    struct inode *inode = new_inode(sb);
    if (inode != NULL) {
        inode_init_owner(sb->s_user_ns, inode, dir, mode);
        inode->i_op = &networkfs_inode_ops;
        inode->i_fop = &networkfs_inode_fops;
        inode->i_ino = i_ino;
        inode->i_mode |= 0777;
    }
    return inode;
}

int networkfs_fill_super(struct super_block *sb, void *data, int silent) {
    struct inode *inode = networkfs_get_inode(sb, NULL, S_IFDIR, 1000);
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
        printk(KERN_ERR "Can't mount file system\n");
    } else { 
        printk(KERN_INFO "Mounted successfuly\n"); 
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
