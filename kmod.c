#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/mount.h>
#include <linux/sched.h>
#include <asm/pgtable.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/fs_struct.h>
#include "kmod.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sergey Astashin");
MODULE_DESCRIPTION("Kernel module for getting page and dentry information on PID via debugfs");
MODULE_VERSION("1.0");

struct page_info {
    unsigned long flags;
    unsigned long vm_addr;
    void* mapping;
};

struct dentry_info {
    const unsigned char* name;
    unsigned int inode_uid;
    unsigned long inode_ino;
    unsigned int inode_flags;
};

static int flags;
static struct page_info* page_info;
static struct dentry_info* dentry_info;
static DEFINE_MUTEX(custom_lock);

static void reset_flags_and_info(void) {
    if (page_info) {
        kfree(page_info);
    }
    if (dentry_info) {
        kfree(dentry_info);
    }
    page_info = NULL;
    dentry_info = NULL;
    flags = 0;
}

static struct page* get_page_by_addr(struct mm_struct* mm, unsigned long addr) {
    pgd_t * pgd;
    p4d_t * p4d;
    pud_t * pud;
    pmd_t * pmd;
    pte_t * pte;

    pgd = pgd_offset(mm, addr);
    if (!pgd_present(*pgd)) {
        return NULL;
    }

    p4d = p4d_offset(pgd, addr);
    if (!p4d_present(*p4d)) {
        return NULL;
    }

    pud = pud_offset(p4d, addr);
    if (!pud_present(*pud)) {
        return NULL;
    }

    pmd = pmd_offset(pud, addr);
    if (!pmd_present(*pmd)) {
        return NULL;
    }

    pte = pte_offset_map(pmd, addr);
    if (!pte_present(*pte)) {
        return NULL;
    }
    return pte_page(*pte);
}

static int get_page_info(struct task_struct* task) {
    struct vm_area_struct* vma;
    unsigned long addr, end;
    struct page* page;

    printk(KERN_INFO "[" KMOD_NAME "] Getting page...\n");
    if (!task->mm) {
        printk(KERN_ERR "[" KMOD_NAME "] Error - mm is NULL!\n");
        return -1;
    }
    vma = task->mm->mmap;
    if (!vma) {
        printk(KERN_ERR "[" KMOD_NAME "] Error - vma is NULL!\n");
        return -1;
    }

    addr = vma->vm_start;
    end = vma->vm_end;
    while (addr <= end) {
        page = get_page_by_addr(task->mm, addr);
        if (page) {
            page_info = kmalloc(sizeof(struct page_info), GFP_KERNEL);
            if (!page_info) {
                printk(KERN_ERR "[" KMOD_NAME "] Error - page_info is NULL!\n");
                return -1;
            }
            page_info->flags = page->flags;
            page_info->vm_addr = addr;
            page_info->mapping = page->mapping;
            printk(KERN_INFO "[" KMOD_NAME "] Page was found!\n");
            return 0;
        }
        addr += PAGE_SIZE;
    }
    printk(KERN_ERR "[" KMOD_NAME "] Error - page wasn't found!\n");
    return -1;
}

static int get_dentry_info(struct task_struct* task) {
    struct file* exe_file;
    struct dentry* dentry;

    printk(KERN_INFO "[" KMOD_NAME "] Getting dentry...\n");
    if (!task->mm) {
        printk(KERN_ERR "[" KMOD_NAME "] Error - fs is NULL!\n");
        return -1;
    }
    exe_file = get_mm_exe_file(task->mm);
    if (!exe_file) {
        printk(KERN_ERR "[" KMOD_NAME "] Error - exe_file is NULL!\n");
        return -1;
    }
    dentry = exe_file->f_path.dentry;
    if (!dentry) {
        printk(KERN_ERR "[" KMOD_NAME "] Error - dentry is NULL!\n");
        return -1;
    }

    dentry_info = kmalloc(sizeof(struct dentry_info), GFP_KERNEL);
    if (!dentry_info) {
        printk(KERN_ERR "[" KMOD_NAME "] Error - dentry_info is NULL!\n");
        return -1;
    }
    dentry_info->name = dentry->d_name.name;
    dentry_info->inode_uid = dentry->d_inode->i_uid.val;
    dentry_info->inode_ino = dentry->d_inode->i_ino;
    dentry_info->inode_flags = dentry->d_inode->i_flags;
    printk(KERN_INFO "[" KMOD_NAME "] Dentry was found!\n");
    return 0;
}

static int custom_show(struct seq_file* file, void* v) {
    if ((flags & PRINT_PAGE_INFO) == PRINT_PAGE_INFO) {
        if (page_info) {
            seq_printf(file, "Page:\n");
            seq_printf(file, "\tFlags: %lx\n", page_info->flags);
            seq_printf(file, "\tVirtual address: %lx\n", page_info->vm_addr);
            seq_printf(file, "\tMapping: %p\n", page_info->mapping);
        } else {
            seq_printf(file, "An error occurred while getting page info!\n");
        }
        printk(KERN_INFO "[" KMOD_NAME "] Sent page info to user.\n");
    }
    if ((flags & PRINT_DENTRY_INFO) == PRINT_DENTRY_INFO) {
        if (page_info) {
            seq_printf(file, "Dentry:\n");
            seq_printf(file, "\tName: %s\n", dentry_info->name);
            seq_printf(file, "\tInode UID: %u\n", dentry_info->inode_uid);
            seq_printf(file, "\tInode number: %lu\n", dentry_info->inode_ino);
            seq_printf(file, "\tInode flags: %x\n", dentry_info->inode_flags);
        } else {
            seq_printf(file, "An error occurred while getting dentry info!\n");
        }
        printk(KERN_INFO "[" KMOD_NAME "] Sent dentry info to user.\n");
    }
    return 0;
}

static int custom_open(struct inode* inode, struct file* file) {
    if (!mutex_trylock(&custom_lock)) {
        return -EBUSY;
    }
    return single_open(file, custom_show, NULL);
}

static int custom_release(struct inode* inode, struct file* file) {
    mutex_unlock(&custom_lock);
    return seq_release(inode, file);
}

static ssize_t custom_write(struct file* file, const char __user* buff, size_t size, loff_t* fpos) {
    int pid;
    struct task_struct* task;
    reset_flags_and_info();

    // get pid:
    if (copy_from_user(&pid, buff, sizeof(int))) {
        printk(KERN_ERR "[" KMOD_NAME "] Couldn't receive PID from user!\n");
        return -EFAULT;
    }
    if (pid < 0) {
        printk(KERN_ERR "[" KMOD_NAME "] Invalid PID!\n");
        return -EINVAL;
    }
    printk(KERN_INFO "[" KMOD_NAME "] Received PID: %d\n", pid);

    // get flags:
    if (copy_from_user(&flags, buff + sizeof(int), sizeof(int))) {
        printk(KERN_ERR "[" KMOD_NAME "] Couldn't receive flags from user!\n");
        return -EFAULT;
    }
    if (flags & ~(PRINT_PAGE_INFO | PRINT_DENTRY_INFO)) {
        printk(KERN_ERR "[" KMOD_NAME "] Invalid flags!\n");
        return -EINVAL;
    }
    printk(KERN_INFO "[" KMOD_NAME "] Received flags: %d\n", flags);

    // get task_struct by pid:
    task = get_pid_task(find_get_pid(pid), PIDTYPE_PID);
    if (task == NULL) {
        printk(KERN_ERR "[" KMOD_NAME "] There is no task with PID=%d!\n", pid);
        return -EINVAL;
    }
    printk(KERN_INFO "[" KMOD_NAME "] Successfully found task_struct.\n");

    // get page and dentry from task:
    if ((flags & PRINT_PAGE_INFO) == PRINT_PAGE_INFO && get_page_info(task) == -1) {
        return -EFAULT;
    }
    if ((flags & PRINT_DENTRY_INFO) == PRINT_DENTRY_INFO && get_dentry_info(task) == -1) {
        return -EFAULT;
    }
    return (ssize_t) size;
}

static struct dentry* root;
static const struct file_operations custom_ops = {
        .owner = THIS_MODULE,
        .open = custom_open,
        .write = custom_write,
        .read = seq_read,
        .release = custom_release
};

static int __init kmod_init(void) {
    printk(KERN_INFO "[" KMOD_NAME "] Loading kernel module %s...\n", KMOD_NAME);
    root = debugfs_create_dir(KMOD_NAME, NULL);
    if (!root) {
        printk(KERN_ERR "[" KMOD_NAME "] Couldn't create module's root directory in debugfs!\n");
        return -1;
    }
    if (!debugfs_create_file(DEBUGFS_FILE_NAME, 0666, root, NULL, &custom_ops)) {
        printk(KERN_ERR "[" KMOD_NAME "] Couldn't create file for getting info in debugfs!\n");
        return -1;
    }
    printk(KERN_INFO "[" KMOD_NAME "] Kernel module %s was loaded!\n", KMOD_NAME);
    return 0;
}

static void __exit kmod_exit(void) {
    debugfs_remove_recursive(root);
    printk(KERN_INFO "[" KMOD_NAME "] Kernel module %s was unloaded!\n", KMOD_NAME);
}

module_init(kmod_init);
module_exit(kmod_exit);