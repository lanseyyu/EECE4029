/*  buddy_alloc.c 
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or (at
*  your option) any later version.
*
*  This program is distributed in the hope that it will be useful, but
*  WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*  General Public License for more details.
*
*  You should have received a copy of the GNU General Public License along
*  with this program; if not, write to the Free Software Foundation, Inc.,
*  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
*
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/vmalloc.h>	
#include <linux/slab.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include "buddy_alloc.h"

#define KERNEL_AUTH     "Samir Silbak"
#define KERNEL_DESC     "kernel 'driver' implementing buddy allocator  <silbak04@gmail.com>"

MODULE_LICENSE("GPL");
MODULE_AUTHOR(KERNEL_AUTH);
MODULE_DESCRIPTION(KERNEL_DESC); 

static int dev_open = 0;
char *buddy_alloc;
int ref;

struct buddy
{
    int free;
    int split;
    int page_ref_blk;
    int page_sized_blk;

    struct buddy *left;
    struct buddy *right;
};

struct buddy *root = NULL;

static int buddy_mem_alloc(struct buddy *node, int mem_size)
{
    int ret_val = 0;
    if (node->split)
    {
        ret_val = buddy_mem_alloc(node->left, mem_size);
        if (ret_val < 0)
            return buddy_mem_alloc(node->right, mem_size);
        else
            return ret_val;
    }
    if (!node->free || node->page_sized_blk < mem_size)
        return -1;
 
    /* traverse through the linked list 
       allocating blocks of memory
       until we hit the smallest block
       needed - all are free */
    if (node->page_sized_blk > 2*mem_size)
    {                                                                               /* ----------------- */
        node->split = 1;                                                            /*    BINARY TREE    */
        node->right = (struct buddy *)vmalloc(sizeof(struct buddy));                /* ----------------- */
        node->right->free  = 1;                                                     /*        _|_        */
        node->right->split = 0;                                                     /*       /   \       */
        node->right->page_sized_blk = node->page_sized_blk / 2;                     /*      /\   /\      */
        node->right->page_ref_blk = node->page_ref_blk + node->page_sized_blk / 2;  /*     /\/\ /\/\     */

        node->left = (struct buddy *)vmalloc(sizeof(struct buddy));                 
        node->left->free  = 1;                                                      
        node->left->split = 0;                                                      
        node->left->page_sized_blk = node->page_sized_blk / 2;
        node->left->page_ref_blk = node->page_ref_blk;

        return buddy_mem_alloc(node->left, mem_size);
    }

    /* we can now assign our data
       to the smallest block found 
       and this node will no longer
       be free */
    node->free = 0;

    return node->page_ref_blk; 
}

static int buddy_mem_free(struct buddy *node, int block_ref)
{
    /* are we trying to free
       space that is already free? */
    if (!node->split && node->free ||
        block_ref > ref             )
        return -1;

    /* checks to see if we have allocated
       maximum size given from the operating
       system */
    if (!node->split && !node->free && 
        node->page_ref_blk == block_ref) 
    {
        vfree(node);
        node = NULL;
        node->split = 0;
        node->free  = 1;
    }

    /* if nodes are split, let's go ahead
       and traverse through the link list
       to find which node to free */
    if (node->split)
    {
        if (node->left->split) 
        {
            buddy_mem_free(node->left, block_ref);
        }

        if (!node->left->free && 
            node->left->page_ref_blk == block_ref)
        {
            node->left->free = 1;
        }

        if (node->right->split) 
        {
            buddy_mem_free(node->right, block_ref);
        }

        if (!node->right->free &&
            node->right->page_ref_blk == block_ref)
        {
            node->right->free = 1;
        }

        /* have we found two free buddies next
           to each other? let's go ahead and
           coalesce the two together working
           our way back up the tree */
        if (node->right->free == 1  &&
            node->left->free  == 1   )
        {
            vfree(node->right);
            vfree(node->left);

            node->left  = NULL;
            node->right = NULL;
            node->split = 0;
            node->free  = 1;
        }
    }

    return 0;
}

static void destroy_buddies(struct buddy *node)
{
    if (node->split)
    {
        destroy_buddies(node->left);
        destroy_buddies(node->right);
    }
    vfree(node);
    node = NULL;
}

static int open(struct inode *ip, struct file *fp)
{
    printk(KERN_INFO "opening file\n");

    if (dev_open) return -EBUSY;
    else dev_open++;

    return 0;
}

static int release(struct inode *ip, struct file *fp)
{
    printk(KERN_INFO "releasing file\n");
    dev_open--;

    return 0;
}

long ioctl(struct file *fp, unsigned int ioctl_num,
           unsigned long ioctl_param)
{
    int i;
    int bytes_writ = 0;
    int bytes_read = 0;
    char *buff;

    switch (ioctl_num) 
    {
        case IOCTL_ALLOC_MEM:

            printk(KERN_INFO "Allocating [%d] bytes\n", (int)ioctl_param);
            return buddy_mem_alloc(root, (int)ioctl_param);

        case IOCTL_WRITE_REF: 

            ref = (int)ioctl_param;
            return ref;

        case IOCTL_FILL_WBUF:

            buff = (char *)ioctl_param;
            for (i = 0; i < BUFF_SIZE; i++)
            {
                get_user(*(buddy_alloc + i + ref), buff);

                if (*buff == '\0') break;
                buff++;
                bytes_writ++;
            }
            return bytes_writ;

        case IOCTL_READ_REF: 

            ref = (int)ioctl_param;
            return ref;

        case IOCTL_FILL_RBUF:

            buff = (char *)ioctl_param;
            for (i = 0; i < BUFF_SIZE; i++)
            {
                put_user(*(buddy_alloc + i + ref), buff);

                if (*buff == '\0') break;
                buff++;
                bytes_read++;
            }
            return bytes_read;

        case IOCTL_FREE_MEM:

            printk(KERN_INFO "Freeing bytes at block reference: [%d]\n", (int)ioctl_param);
            return buddy_mem_free(root, (int)ioctl_param);

        default:
            return -1;
    }

    return 0;
}

static struct file_operations file_ops =
{
    .open           = open,
    .release        = release,
    .unlocked_ioctl = ioctl
};

int init_budd_alloc(void)
{
    int ret_val = 0;

    ret_val = register_chrdev(MAJOR_NUM, DEVICE_NAME, &file_ops);
    if (ret_val < 0)
    {
        printk(KERN_INFO "We have failed to register device\n");
        return ret_val;
    }
    else
        printk(KERN_INFO "%s has been registered\n", DEVICE_NAME);

    /* allocate the desired memory pool size */
    buddy_alloc = (char *)vmalloc(ALLOC_SIZE * sizeof(*buddy_alloc));

    /* set the root of our block */
    root = (struct buddy *)vmalloc(sizeof(struct buddy));
    root->page_sized_blk = ALLOC_SIZE;
    root->free = 1;
    root->split = 0;
    root->page_ref_blk = 0;

    root->left  = NULL;
    root->right = NULL;

    return 0;
}

void exit_budd_alloc(void)
{
    vfree(buddy_alloc);
    destroy_buddies(root);

    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
    printk(KERN_INFO "%s has been unregistered\n", DEVICE_NAME);
}

module_init(init_budd_alloc);
module_exit(exit_budd_alloc);
