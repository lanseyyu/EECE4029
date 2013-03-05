/*  ioctl.c 
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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>      
#include <unistd.h>     
#include <sys/ioctl.h>      
#include "buddy_alloc.h"

int mem;
int ref;

char buffer[BUFF_SIZE];

int get_mem(int mem, int size)
{
    int ref_block;

    ref_block = ioctl(mem, IOCTL_ALLOC_MEM, size);
    printf("memory size is: [%d], ref_block: [%d]\n", size, ref_block);

    if (ref_block < 0)
    {
        printf("requesting a block of memory has failed: [%d]\n", ref_block);
        exit(EXIT_FAILURE);
    }

    return ref_block;
}

int write_mem(int mem, int ref, char *buf)
{
    int ref_val;
    int bytes_wr;

    ref_val  = ioctl(mem, IOCTL_WRITE_REF, ref);
    bytes_wr = ioctl(mem, IOCTL_FILL_WBUF, buf);

    if (ref_val < 0 || bytes_wr < 0)
    {
        printf("writing to a block of memory has failed.\n");
        exit(EXIT_FAILURE);
    }

    printf("write buffer:  [%s]\n", buf);
    printf("bytes written: [%d]\n", bytes_wr);
    return bytes_wr;
}

int read_mem(int mem, int ref, char *buf, int size)
{
    int ref_val;
    int bytes_rd;

    ref_val  = ioctl(mem, IOCTL_READ_REF,  ref);
    bytes_rd = ioctl(mem, IOCTL_FILL_RBUF, buf);

    if (ref_val < 0 || bytes_rd < 0)
    {
        printf("reading a block of memory has failed.\n");
        exit(EXIT_FAILURE);
    }

    printf("bytes read: [%d]\n", bytes_rd);
    return bytes_rd;
}

int free_mem(int mem, int ref)
{
    int ref_val;
    ref_val = ioctl(mem, IOCTL_FREE_MEM, ref);

    if (ref_val < 0)
    {
        printf("freeing a block of memory has failed.\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}

void main(void)
{
    mem = open(DEVICE_FILE_NAME, 0);
    if (mem < 0) 
    {
        printf("Can't open device file: [%s]\n", DEVICE_FILE_NAME);
        exit(EXIT_FAILURE);
    }

    ref = get_mem(mem, 100);
    sprintf(buffer, "Hello buddy");
    write_mem(mem, ref, buffer);
    read_mem(mem, ref+3, buffer, 10);
    printf("buffer: %s\n", buffer);
    free_mem(mem, ref);
    close(mem);
}
