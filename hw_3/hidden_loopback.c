/*                                                                                                                                                                                 
*  hidden_loopback.c 
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
*  resources used: http://www.xml.com/ldd/chapter/book/ch14.html
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

#include <linux/string.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/ip.h>

#include <linux/in6.h>
#include <asm/checksum.h>

#define OS_RX_INT 0x0001
#define OS_TX_INT 0x0002

#define KERNEL_AUTH     "Samir Silbak"
#define KERNEL_DESC     "kernel network 'driver' implementing loopback  <silbak04@gmail.com>"

MODULE_LICENSE("GPL");
MODULE_AUTHOR(KERNEL_AUTH);
MODULE_DESCRIPTION(KERNEL_DESC); 

struct net_device *os0;
struct net_device *os1;

struct os_priv
{
    struct net_device_stats stats;
    struct os_packet *packet;
    struct net_device *dev;
    struct sk_buff *skb;
    int status;
    int rx_int_en;
    int tx_pckt_len;
    u8 *tx_pckt_data;
    spinlock_t lock;
};

struct os_packet
{
    struct net_device *dev;
    int data_len;
    u8 data[ETH_DATA_LEN];
};

int os_open(struct net_device *dev) 
{ 
    netif_start_queue(dev);
    return 0; 
}

int os_stop(struct net_device *dev) 
{ 
    netif_stop_queue(dev);
    return 0; 
}

int os_start_xmit(struct sk_buff *skb, struct net_device *dev) 
{ 
    int len;
    char *buff;
    char short_pkt[ETH_ZLEN];

    struct os_priv *priv_dev = netdev_priv(dev);
    struct iphdr *ih;
    struct net_device *dest;
    struct os_priv *priv;
    struct os_packet *tx_buff;
    struct sk_buff *skb_rx;

    u32 *s_addr;
    u32 *d_addr;

    buff = skb->data;
    len  = skb->len;

    /* have we received a packet less than 
       the desired length? if so, pad it with 0's */
    if (len < ETH_ZLEN)
    {
        memset(short_pkt, 0, ETH_ZLEN);
        memcpy(short_pkt, skb->data, skb->len);

        len  = ETH_ZLEN;
        buff = short_pkt;
    }
    dev->trans_start = jiffies;
    priv_dev->skb = skb;

    /* get ready for hardware tranmist */
    ih = (struct iphdr *)(buff + sizeof(struct ethhdr));
    s_addr = &ih->saddr;
    d_addr = &ih->daddr;

    /* toggle third octet bit for 
       both source and destinatin address */
    ((u8 *)s_addr)[2] ^= 1;
    ((u8 *)d_addr)[2] ^= 1;

    ih->check = 0;
    ih->check = ip_fast_csum((unsigned char *)ih, ih->ihl);

    dest = (dev == os0) ? os1 : os0;
    priv = netdev_priv(dest);

    tx_buff = priv->packet;
    memcpy(tx_buff->data, buff, len);

    dev = (struct net_device *)dest;

    priv = netdev_priv(dev);
    spin_lock(&priv->lock);

    tx_buff = priv->packet;
    skb_rx = dev_alloc_skb(len + 2);

    if (!skb_rx) printk(KERN_NOTICE "packet dropped\n");

    skb_reserve(skb_rx, 2);
    memcpy(skb_put(skb_rx, len), tx_buff->data, len);

    skb_rx->dev = dev;
    skb_rx->protocol = eth_type_trans(skb_rx, dev);
    skb_rx->ip_summed = CHECKSUM_UNNECESSARY;

    netif_rx(skb_rx);
    dev_kfree_skb(priv->skb);

    spin_unlock(&priv->lock);

    return NETDEV_TX_OK; 
}

struct net_device_stats *os_stats(struct net_device *dev)
{
    return &(((struct os_priv*)netdev_priv(dev))->stats);
}

static const struct net_device_ops os_device_ops = 
{
    .ndo_open       = os_open,
    .ndo_stop       = os_stop,
    .ndo_start_xmit = os_start_xmit,
    .ndo_get_stats  = os_stats,
};

int os_header(struct sk_buff *skb, struct net_device *dev,
              unsigned short type, const void *daddr, 
              const void *saddr, unsigned int len)
{
    struct ethhdr *eth = (struct ethhdr *)skb_push(skb,ETH_HLEN);
    eth->h_proto = htons(type);

    memcpy(eth->h_source, dev->dev_addr, dev->addr_len);
    memcpy(eth->h_dest, eth->h_source, dev->addr_len);

    eth->h_dest[ETH_ALEN-1] = (eth->h_dest[ETH_ALEN-1] == 5) ? 6 : 5;

    return dev->hard_header_len;
}

static const struct header_ops os_header_ops =
{
    .create = os_header,
};

static int init_mod(void)
{
    int i;

    struct os_priv *priv0;
    struct os_priv *priv1;
    
    os0 = alloc_etherdev(sizeof(struct os_priv));
    os1 = alloc_etherdev(sizeof(struct os_priv));

    for (i = 0; i < 6; i++)
    {
        os0->dev_addr [i] = (unsigned char)i;
    }
    for (i = 0; i < 5; i++)
    {
        os1->dev_addr [i] = (unsigned char)i;
    }
    os1->dev_addr [5] = (unsigned char)6;

    os0->hard_header_len = 14;
    os1->hard_header_len = 14;

    strcpy(os0->name, "os0");
    strcpy(os1->name, "os1");

    os0->netdev_ops = &os_device_ops;
    os0->header_ops = &os_header_ops;
    os1->netdev_ops = &os_device_ops;
    os1->header_ops = &os_header_ops;

    /* disable ARP */
    os0->flags |= IFF_NOARP;
    os1->flags |= IFF_NOARP;

    priv0 = netdev_priv(os0);
    priv1 = netdev_priv(os1);

    memset(priv0, 0, sizeof(struct os_priv));
    memset(priv1, 0, sizeof(struct os_priv));

    priv0->dev = os0;
    priv1->dev = os1;

    spin_lock_init(&priv0->lock);
    spin_lock_init(&priv1->lock);

    priv0->rx_int_en = 1;
    priv1->rx_int_en = 1;

    priv0->packet = kmalloc(sizeof(struct os_packet), GFP_KERNEL);
    priv1->packet = kmalloc(sizeof(struct os_packet), GFP_KERNEL);

    priv0->packet->dev = os0;
    priv1->packet->dev = os1;
   
    if (register_netdev(os0))
        printk(KERN_INFO "error registering %s device\n", os0->name);
    else 
        printk(KERN_INFO "registering %s device\n", os0->name);

    if (register_netdev(os1))
        printk(KERN_INFO "error registering %s device\n", os1->name);
    else 
        printk(KERN_INFO "registering %s device\n", os1->name);

    return 0;
}

void exit_mod(void) 
{
    struct os_priv *priv;

    if(os0)
    {
        priv = netdev_priv(os0);
        kfree(priv->packet);
        unregister_netdev(os0);
        printk("unregisterd %s device\n", os0->name);
    }
    if(os1)
    {
        priv = netdev_priv(os1);
        kfree(priv->packet);
        unregister_netdev(os1);
        printk("unregisterd %s device\n", os1->name);
    }
}

module_init(init_mod);
module_exit(exit_mod);
