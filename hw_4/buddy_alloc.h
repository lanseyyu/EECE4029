#include <linux/ioctl.h>

#define MAJOR_NUM  100

#define ALLOC_SIZE 16777216
#define BUFF_SIZE  4096

#define DEVICE_FILE_NAME "/dev/mem_dev"
#define DEVICE_NAME      "mem_dev"

#define IOCTL_ALLOC_MEM _IOR(MAJOR_NUM, 0, char *)
#define IOCTL_WRITE_REF _IOW(MAJOR_NUM, 1, int)
#define IOCTL_READ_REF  _IOR(MAJOR_NUM, 2, char *)
#define IOCTL_FILL_WBUF _IOW(MAJOR_NUM, 3, int)
#define IOCTL_FILL_RBUF _IOW(MAJOR_NUM, 4, int)
#define IOCTL_FREE_MEM  _IOR(MAJOR_NUM, 5, char *)
