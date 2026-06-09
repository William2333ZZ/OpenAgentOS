#include "netstack.h"
#include "virtio_net.h"
#include "printf.h"
#include "../include/agentos.h"

int dns_doh_resolve_a(const char *host, uint32_t *out_ip, int timeout_ms)
    __attribute__((weak));

#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_IP   0x0800
#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2
#define IP_PROTO_ICMP  1
#define IP_PROTO_TCP   6
#define IP_PROTO_UDP   17
#define TCP_FIN        0x01
#define TCP_SYN        0x02
#define TCP_RST        0x04
#define TCP_PSH        0x08
#define TCP_ACK        0x10

struct eth_hdr {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t type;
} __attribute__((packed));

struct arp_pkt {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    uint8_t sha[6];
    uint8_t spa[4];
    uint8_t tha[6];
    uint8_t tpa[4];
} __attribute__((packed));

struct ip_hdr {
    uint8_t ver_ihl;
    uint8_t tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t frag;
    uint8_t ttl;
    uint8_t proto;
    uint16_t csum;
    uint32_t src;
    uint32_t dst;
} __attribute__((packed));

struct tcp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t data_off;
    uint8_t flags;
    uint16_t window;
    uint16_t csum;
    uint16_t urg;
} __attribute__((packed));

struct tcp_conn {
    int active;
    uint32_t remote_ip;
    uint16_t remote_port;
    uint16_t local_port;
    uint32_t iss;
    uint32_t snd_nxt;
    uint32_t rcv_nxt;
    int state;
};

static uint32_t our_ip;
static uint32_t dns_server;
static uint8_t gw_mac[6];
static int gw_mac_valid;
static uint8_t peer_mac[6];
static uint32_t peer_mac_ip;
static int peer_mac_valid;

#ifdef PLATFORM_X86_64_PC
static void net_slirp_fallback_mac(void) {
    static const uint8_t slirp_mac[6] = {0x52, 0x55, 0x0a, 0x00, 0x02, 0x02};
    int i;

    for (i = 0; i < 6; i++)
        gw_mac[i] = slirp_mac[i];
    gw_mac_valid = 1;
    for (i = 0; i < 6; i++)
        peer_mac[i] = slirp_mac[i];
    peer_mac_ip = NET_GW_HOST;
    peer_mac_valid = 1;
}
#endif

static uint8_t rx_frame[NET_FRAME_MAX];
static uint8_t tx_udp_buf[NET_FRAME_MAX];
static uint8_t tx_ip_buf[NET_FRAME_MAX];
static uint8_t tx_eth_frame[NET_FRAME_MAX];
static struct tcp_conn tcp;
static uint16_t dhcp_xid = 0x1234;
static int dhcp_done;
static uint32_t dhcp_offered_ip;
static uint32_t dns_result_ip;
static int dns_result_ok;
static int dns_saw_bogus;
static char dns_cache_host[64];
static uint32_t dns_cache_ip;
static int dns_cache_valid;
static const uint8_t bcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

#define TCP_RX_SIZE 16384
static uint8_t tcp_rx[TCP_RX_SIZE];
static int tcp_rx_len;

struct udp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;
    uint16_t csum;
} __attribute__((packed));

static uint16_t htons16(uint16_t v) {
    return (uint16_t)((v >> 8) | (v << 8));
}

static uint16_t ntohs16(uint16_t v) {
    return htons16(v);
}

static uint32_t htonl32(uint32_t v) {
    return ((v & 0xffUL) << 24) | ((v & 0xff00UL) << 8) | ((v & 0xff0000UL) >> 8) |
           ((v & 0xff000000UL) >> 24);
}

static uint32_t ntohl32(uint32_t v) {
    return htonl32(v);
}

static uint16_t csum_fold(uint32_t sum) {
    while (sum >> 16)
        sum = (sum & 0xffffUL) + (sum >> 16);
    return (uint16_t)(~sum);
}

static uint16_t csum16(const void *data, int len) {
    const uint16_t *p = (const uint16_t *)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += ntohs16(*p++);
        len -= 2;
    }
    if (len == 1)
        sum += (uint32_t)(*(const uint8_t *)p) << 8;
    return csum_fold(sum);
}

static uint16_t tcp_csum(uint32_t src, uint32_t dst, const void *tcp_seg, int len) {
    uint32_t sum = 0;
    const uint16_t *p;
    int i;

    sum += (src >> 16) + (src & 0xffff);
    sum += (dst >> 16) + (dst & 0xffff);
    sum += IP_PROTO_TCP;
    sum += (uint16_t)len;

    p = (const uint16_t *)tcp_seg;
    for (i = 0; i + 1 < len; i += 2)
        sum += ntohs16(p[i / 2]);
    if (len & 1)
        sum += (uint32_t)((const uint8_t *)tcp_seg)[len - 1] << 8;
    return csum_fold(sum);
}

static int eth_send(const uint8_t dst[6], uint16_t type, const void *payload, int len) {
    int pos = 0;
    const uint8_t *src = virtio_net_mac();

    if (!virtio_net_ready() || len + 14 > (int)sizeof(tx_eth_frame))
        return EINVAL;
    for (int i = 0; i < 6; i++)
        tx_eth_frame[pos++] = dst[i];
    for (int i = 0; i < 6; i++)
        tx_eth_frame[pos++] = src[i];
    tx_eth_frame[pos++] = (uint8_t)(type >> 8);
    tx_eth_frame[pos++] = (uint8_t)(type & 0xff);
    for (int i = 0; i < len; i++)
        tx_eth_frame[pos++] = ((const uint8_t *)payload)[i];
    return virtio_net_send_frame(tx_eth_frame, pos);
}

static int arp_send(uint16_t op, const uint8_t target_mac[6], uint32_t target_ip,
                    uint32_t sender_ip) {
    struct arp_pkt pkt;

    pkt.htype = htons16(1);
    pkt.ptype = htons16(ETHERTYPE_IP);
    pkt.hlen = 6;
    pkt.plen = 4;
    pkt.oper = htons16(op);
    for (int i = 0; i < 6; i++)
        pkt.sha[i] = virtio_net_mac()[i];
    pkt.spa[0] = (uint8_t)(sender_ip >> 24);
    pkt.spa[1] = (uint8_t)(sender_ip >> 16);
    pkt.spa[2] = (uint8_t)(sender_ip >> 8);
    pkt.spa[3] = (uint8_t)sender_ip;
    for (int i = 0; i < 6; i++)
        pkt.tha[i] = target_mac[i];
    pkt.tpa[0] = (uint8_t)(target_ip >> 24);
    pkt.tpa[1] = (uint8_t)(target_ip >> 16);
    pkt.tpa[2] = (uint8_t)(target_ip >> 8);
    pkt.tpa[3] = (uint8_t)target_ip;
    return eth_send((const uint8_t[]){0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, ETHERTYPE_ARP,
                    &pkt, (int)sizeof(pkt));
}

static void arp_handle(const struct arp_pkt *pkt, int len) {
    uint32_t spa;
    uint32_t tpa;

    if (len < (int)sizeof(struct arp_pkt))
        return;
    if (ntohs16(pkt->htype) != 1 || ntohs16(pkt->ptype) != ETHERTYPE_IP)
        return;
    spa = ((uint32_t)pkt->spa[0] << 24) | ((uint32_t)pkt->spa[1] << 16) |
          ((uint32_t)pkt->spa[2] << 8) | (uint32_t)pkt->spa[3];
    tpa = ((uint32_t)pkt->tpa[0] << 24) | ((uint32_t)pkt->tpa[1] << 16) |
          ((uint32_t)pkt->tpa[2] << 8) | (uint32_t)pkt->tpa[3];

    if (ntohs16(pkt->oper) == ARP_OP_REPLY) {
        for (int i = 0; i < 6; i++)
            peer_mac[i] = pkt->sha[i];
        peer_mac_ip = spa;
        peer_mac_valid = 1;
        if (spa == NET_GW_HOST) {
            for (int i = 0; i < 6; i++)
                gw_mac[i] = pkt->sha[i];
            gw_mac_valid = 1;
            kprintf("[net] arp gw resolved\n");
        } else {
            kprintf("[net] arp reply ip=%u.%u.%u.%u\n", pkt->spa[0], pkt->spa[1], pkt->spa[2],
                    pkt->spa[3]);
        }
    }

    if (ntohs16(pkt->oper) == ARP_OP_REQUEST && tpa == our_ip) {
        arp_send(ARP_OP_REPLY, pkt->sha, spa, our_ip);
    }
}

static uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static void write_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static int ip_send(uint32_t dst, uint8_t proto, const void *payload, int len) {
    int iplen = 20 + len;
    const uint8_t *dst_mac;
    uint16_t csum;

    if (iplen > (int)sizeof(tx_ip_buf))
        return EINVAL;

    if (dst == 0xffffffffUL) {
        dst_mac = bcast_mac;
    } else if ((dst & NET_MASK_HOST) == (our_ip & NET_MASK_HOST)) {
        if (!peer_mac_valid || peer_mac_ip != dst)
            return EINVAL;
        dst_mac = peer_mac;
    } else {
        if (!gw_mac_valid)
            return EINVAL;
        dst_mac = gw_mac;
    }

    tx_ip_buf[0] = 0x45;
    tx_ip_buf[1] = 0;
    tx_ip_buf[2] = (uint8_t)(iplen >> 8);
    tx_ip_buf[3] = (uint8_t)iplen;
    tx_ip_buf[4] = 0;
    tx_ip_buf[5] = 1;
    tx_ip_buf[6] = 0;
    tx_ip_buf[7] = 0;
    tx_ip_buf[8] = 64;
    tx_ip_buf[9] = proto;
    tx_ip_buf[10] = 0;
    tx_ip_buf[11] = 0;
    write_be32(tx_ip_buf + 12, our_ip);
    write_be32(tx_ip_buf + 16, dst);
    csum = csum16(tx_ip_buf, 20);
    tx_ip_buf[10] = (uint8_t)(csum >> 8);
    tx_ip_buf[11] = (uint8_t)csum;

    for (int i = 0; i < len; i++)
        tx_ip_buf[20 + i] = ((const uint8_t *)payload)[i];
    return eth_send(dst_mac, ETHERTYPE_IP, tx_ip_buf, iplen);
}

static int udp_send(uint32_t dst, uint16_t sport, uint16_t dport, const void *payload, int len) {
    struct udp_hdr *uh = (struct udp_hdr *)tx_udp_buf;
    int udplen = (int)sizeof(struct udp_hdr) + len;

    if (udplen > (int)sizeof(tx_udp_buf))
        return EINVAL;
    uh->src_port = htons16(sport);
    uh->dst_port = htons16(dport);
    uh->len = htons16((uint16_t)udplen);
    uh->csum = 0;
    for (int i = 0; i < len; i++)
        tx_udp_buf[(int)sizeof(struct udp_hdr) + i] = ((const uint8_t *)payload)[i];
    return ip_send(dst, IP_PROTO_UDP, tx_udp_buf, udplen);
}

static uint8_t dhcp_tx_pkt[300];
static uint8_t dns_query_pkt[512];

static int dns_accept_bogus;

static int dns_ip_bogus(uint32_t ip) {
    uint8_t o1 = (uint8_t)((ip >> 24) & 0xff);
    uint8_t o2 = (uint8_t)((ip >> 16) & 0xff);

    /* 198.18.0.0/15 — common Surge/Clash fake resolver range. */
    if (o1 == 198 && (o2 == 18 || o2 == 19))
        return 1;
    return 0;
}

static int __attribute__((noinline)) dhcp_send_discover(void) {
    int pos = 0;

    dhcp_tx_pkt[pos++] = 1;
    dhcp_tx_pkt[pos++] = 1;
    dhcp_tx_pkt[pos++] = 6;
    dhcp_tx_pkt[pos++] = 0;
    dhcp_tx_pkt[pos++] = (uint8_t)(dhcp_xid >> 24);
    dhcp_tx_pkt[pos++] = (uint8_t)(dhcp_xid >> 16);
    dhcp_tx_pkt[pos++] = (uint8_t)(dhcp_xid >> 8);
    dhcp_tx_pkt[pos++] = (uint8_t)dhcp_xid;
    pos += 12;
    for (int i = 0; i < 6; i++)
        dhcp_tx_pkt[pos++] = virtio_net_mac()[i];
    pos += 10;
    pos += 192;
    dhcp_tx_pkt[pos++] = 99;
    dhcp_tx_pkt[pos++] = 130;
    dhcp_tx_pkt[pos++] = 83;
    dhcp_tx_pkt[pos++] = 99;
    dhcp_tx_pkt[pos++] = 53;
    dhcp_tx_pkt[pos++] = 1;
    dhcp_tx_pkt[pos++] = 1;
    dhcp_tx_pkt[pos++] = 255;
    return udp_send(0xffffffffUL, 68, 67, dhcp_tx_pkt, pos);
}

static int __attribute__((noinline)) dhcp_send_request(uint32_t req_ip) {
    int pos = 0;

    dhcp_tx_pkt[pos++] = 1;
    dhcp_tx_pkt[pos++] = 1;
    dhcp_tx_pkt[pos++] = 6;
    dhcp_tx_pkt[pos++] = 0;
    dhcp_tx_pkt[pos++] = (uint8_t)(dhcp_xid >> 24);
    dhcp_tx_pkt[pos++] = (uint8_t)(dhcp_xid >> 16);
    dhcp_tx_pkt[pos++] = (uint8_t)(dhcp_xid >> 8);
    dhcp_tx_pkt[pos++] = (uint8_t)dhcp_xid;
    pos += 12;
    for (int i = 0; i < 6; i++)
        dhcp_tx_pkt[pos++] = virtio_net_mac()[i];
    pos += 10;
    dhcp_tx_pkt[pos++] = (uint8_t)(req_ip >> 24);
    dhcp_tx_pkt[pos++] = (uint8_t)(req_ip >> 16);
    dhcp_tx_pkt[pos++] = (uint8_t)(req_ip >> 8);
    dhcp_tx_pkt[pos++] = (uint8_t)req_ip;
    pos += 192;
    dhcp_tx_pkt[pos++] = 99;
    dhcp_tx_pkt[pos++] = 130;
    dhcp_tx_pkt[pos++] = 83;
    dhcp_tx_pkt[pos++] = 99;
    dhcp_tx_pkt[pos++] = 53;
    dhcp_tx_pkt[pos++] = 1;
    dhcp_tx_pkt[pos++] = 3;
    dhcp_tx_pkt[pos++] = 50;
    dhcp_tx_pkt[pos++] = 4;
    dhcp_tx_pkt[pos++] = (uint8_t)(req_ip >> 24);
    dhcp_tx_pkt[pos++] = (uint8_t)(req_ip >> 16);
    dhcp_tx_pkt[pos++] = (uint8_t)(req_ip >> 8);
    dhcp_tx_pkt[pos++] = (uint8_t)req_ip;
    dhcp_tx_pkt[pos++] = 54;
    dhcp_tx_pkt[pos++] = 4;
    dhcp_tx_pkt[pos++] = (uint8_t)(NET_GW_HOST >> 24);
    dhcp_tx_pkt[pos++] = (uint8_t)(NET_GW_HOST >> 16);
    dhcp_tx_pkt[pos++] = (uint8_t)(NET_GW_HOST >> 8);
    dhcp_tx_pkt[pos++] = (uint8_t)NET_GW_HOST;
    dhcp_tx_pkt[pos++] = 255;
    return udp_send(0xffffffffUL, 68, 67, dhcp_tx_pkt, pos);
}

static void dhcp_handle(const uint8_t *pkt, int len) {
    int pos = 236;
    uint8_t msg_type = 0;
    uint32_t yiaddr;

    if (len < 240)
        return;
    if (pkt[0] != 2)
        return;
    if (((uint32_t)pkt[4] << 24 | (uint32_t)pkt[5] << 16 | (uint32_t)pkt[6] << 8 |
         (uint32_t)pkt[7]) != (uint32_t)dhcp_xid)
        return;

    yiaddr = ((uint32_t)pkt[16] << 24) | ((uint32_t)pkt[17] << 16) |
             ((uint32_t)pkt[18] << 8) | (uint32_t)pkt[19];

    while (pos + 1 < len && pkt[pos] != 255) {
        uint8_t opt = pkt[pos++];
        uint8_t olen = pos < len ? pkt[pos++] : 0;
        if (opt == 53 && olen >= 1)
            msg_type = pkt[pos];
        if (opt == 6 && olen >= 4) {
            /* Ignore DHCP DNS (slirp 10.0.2.3 often hijacks to 198.18.x). */
        }
        if (olen == 0)
            break;
        pos += olen;
    }

    if (msg_type == 2 && yiaddr != 0) {
        dhcp_offered_ip = yiaddr;
        dhcp_send_request(yiaddr);
        kprintf("[net] dhcp offer ip=%u.%u.%u.%u\n", (yiaddr >> 24) & 0xff,
                (yiaddr >> 16) & 0xff, (yiaddr >> 8) & 0xff, yiaddr & 0xff);
    } else if (msg_type == 5) {
        our_ip = dhcp_offered_ip ? dhcp_offered_ip : yiaddr;
        dns_server = NET_DNS_HOST;
        dhcp_done = 1;
        kprintf("[net] dhcp ack ip=%u.%u.%u.%u dns=%u.%u.%u.%u\n",
                (our_ip >> 24) & 0xff, (our_ip >> 16) & 0xff, (our_ip >> 8) & 0xff,
                our_ip & 0xff, (dns_server >> 24) & 0xff, (dns_server >> 16) & 0xff,
                (dns_server >> 8) & 0xff, dns_server & 0xff);
    }
}

static int __attribute__((noinline)) dns_send_query(const char *host) {
    int pos = 0;
    int i = 0;
    int lab_start = 0;

    dns_query_pkt[pos++] = 0x12;
    dns_query_pkt[pos++] = 0x34;
    dns_query_pkt[pos++] = 0x01;
    dns_query_pkt[pos++] = 0x00;
    dns_query_pkt[pos++] = 0x00;
    dns_query_pkt[pos++] = 0x01;
    dns_query_pkt[pos++] = 0x00;
    dns_query_pkt[pos++] = 0x00;
    dns_query_pkt[pos++] = 0x00;
    dns_query_pkt[pos++] = 0x00;
    dns_query_pkt[pos++] = 0x00;
    dns_query_pkt[pos++] = 0x00;
    lab_start = pos;
    while (host[i]) {
        int lab_len = 0;
        lab_start = pos++;
        while (host[i] && host[i] != '.' && lab_len < 63) {
            dns_query_pkt[pos++] = host[i++];
            lab_len++;
        }
        dns_query_pkt[lab_start] = (uint8_t)lab_len;
        if (host[i] == '.')
            i++;
    }
    dns_query_pkt[pos++] = 0;
    dns_query_pkt[pos++] = 0;
    dns_query_pkt[pos++] = 1;
    dns_query_pkt[pos++] = 0;
    dns_query_pkt[pos++] = 1;
    return udp_send(dns_server, 49153, 53, dns_query_pkt, pos);
}

static void dns_handle(const uint8_t *pkt, int len) {
    int pos = 12;
    int ancount;
    int rdlen;

    if (len < 12)
        return;
    if (pkt[0] != 0x12 || pkt[1] != 0x34)
        return;
    ancount = ((int)pkt[6] << 8) | (int)pkt[7];
    if (ancount <= 0)
        return;

    while (pos < len && pkt[pos] != 0)
        pos += 1 + pkt[pos];
    pos += 5;
    if (pos >= len)
        return;

    if ((pkt[pos] & 0xc0) == 0xc0)
        pos += 2;
    else {
        while (pos < len && pkt[pos] != 0)
            pos += 1 + pkt[pos];
        pos++;
    }
    if (pos + 10 > len)
        return;
    if (pkt[pos] != 0 || pkt[pos + 1] != 1)
        return;
    pos += 8;
    rdlen = ((int)pkt[pos] << 8) | (int)pkt[pos + 1];
    pos += 2;
    if (rdlen != 4 || pos + 4 > len)
        return;
    dns_result_ip = ((uint32_t)pkt[pos] << 24) | ((uint32_t)pkt[pos + 1] << 16) |
                    ((uint32_t)pkt[pos + 2] << 8) | (uint32_t)pkt[pos + 3];
    if (dns_ip_bogus(dns_result_ip)) {
        if (dns_accept_bogus) {
            kprintf("[net] dns udp slirp %u.%u.%u.%u (accepted)\n",
                    (dns_result_ip >> 24) & 0xff, (dns_result_ip >> 16) & 0xff,
                    (dns_result_ip >> 8) & 0xff, dns_result_ip & 0xff);
            dns_result_ok = 1;
            return;
        }
        kprintf("[net] dns udp bogus %u.%u.%u.%u (ignored)\n", (dns_result_ip >> 24) & 0xff,
                (dns_result_ip >> 16) & 0xff, (dns_result_ip >> 8) & 0xff,
                dns_result_ip & 0xff);
        dns_saw_bogus = 1;
        return;
    }
    dns_result_ok = 1;
}

static int __attribute__((noinline)) net_dhcp_acquire(int timeout_ms) {
    dhcp_done = 0;
    our_ip = 0;
    for (int i = 0; i < 3; i++)
        dhcp_send_discover();
    for (int t = 0; t < timeout_ms * 20; t++) {
        netstack_poll();
        if (dhcp_done)
            return 0;
        for (volatile int j = 0; j < 400; j++)
            ;
    }
    our_ip = NET_IP_HOST;
    dns_server = NET_DNS_HOST;
    kprintf("[net] dhcp timeout, fallback static ip\n");
    return ETIMEDOUT;
}

static void udp_handle(const struct ip_hdr *ip, int iplen) {
    const struct udp_hdr *uh;
    const uint8_t *payload;
    int udplen;
    int plen;
    uint16_t sport;
    uint16_t dport;

    if (iplen < (int)(sizeof(struct ip_hdr) + sizeof(struct udp_hdr)))
        return;
    uh = (const struct udp_hdr *)((const uint8_t *)ip + sizeof(struct ip_hdr));
    udplen = ntohs16(uh->len);
    if (udplen < (int)sizeof(struct udp_hdr) || udplen > iplen - (int)sizeof(struct ip_hdr))
        udplen = iplen - (int)sizeof(struct ip_hdr);
    payload = (const uint8_t *)uh + sizeof(struct udp_hdr);
    plen = udplen - (int)sizeof(struct udp_hdr);
    sport = ntohs16(uh->src_port);
    dport = ntohs16(uh->dst_port);

    if (dport == 68 && sport == 67)
        dhcp_handle(payload, plen);
    else if (dport == 49153 && sport == 53)
        dns_handle(payload, plen);
}

static int arp_resolve(uint32_t ip, int timeout_ms) {
    int loops = timeout_ms * 20;

    if ((ip & NET_MASK_HOST) != (our_ip & NET_MASK_HOST))
        ip = NET_GW_HOST;

    if (ip == NET_GW_HOST && gw_mac_valid)
        return 0;
    if ((ip & NET_MASK_HOST) == (our_ip & NET_MASK_HOST) && peer_mac_valid &&
        peer_mac_ip == ip)
        return 0;

    arp_send(ARP_OP_REQUEST, (const uint8_t[]){0, 0, 0, 0, 0, 0}, ip, our_ip);
    for (int i = 0; i < loops; i++) {
        netstack_poll();
        if (ip == NET_GW_HOST && gw_mac_valid)
            return 0;
        if ((ip & NET_MASK_HOST) == (our_ip & NET_MASK_HOST) && peer_mac_valid &&
            peer_mac_ip == ip)
            return 0;
        for (volatile int j = 0; j < 500; j++)
            ;
    }
    return ETIMEDOUT;
}

static void tcp_rx_clear(void) {
    tcp_rx_len = 0;
}

static void tcp_rx_append(const uint8_t *data, int len) {
    if (len <= 0)
        return;
    if (tcp_rx_len + len > TCP_RX_SIZE)
        len = TCP_RX_SIZE - tcp_rx_len;
    for (int i = 0; i < len; i++)
        tcp_rx[tcp_rx_len++] = data[i];
}

static int tcp_rx_take(char *buf, int buflen) {
    int n = tcp_rx_len;

    if (n > buflen)
        n = buflen;
    for (int i = 0; i < n; i++)
        buf[i] = (char)tcp_rx[i];
    for (int i = n; i < tcp_rx_len; i++)
        tcp_rx[i - n] = tcp_rx[i];
    tcp_rx_len -= n;
    return n;
}

static void tcp_reset(void) {
    tcp.active = 0;
    tcp.state = 0;
    tcp_rx_clear();
}

static int tcp_send_raw(uint32_t dst, uint16_t sport, uint16_t dport, uint32_t seq,
                        uint32_t ack, uint8_t flags, const void *data, int dlen) {
    uint8_t seg[NET_FRAME_MAX];
    struct tcp_hdr *th = (struct tcp_hdr *)seg;
    int tcplen = (int)sizeof(struct tcp_hdr) + dlen;

    if (tcplen > (int)sizeof(seg))
        return EINVAL;
    th->src_port = htons16(sport);
    th->dst_port = htons16(dport);
    th->seq = htonl32(seq);
    th->ack = htonl32(ack);
    th->data_off = (uint8_t)(5 << 4);
    th->flags = flags;
    th->window = htons16(8192);
    th->urg = 0;
    th->csum = 0;
    for (int i = 0; i < dlen; i++)
        seg[(int)sizeof(struct tcp_hdr) + i] = ((const uint8_t *)data)[i];
    th->csum = htons16(tcp_csum(our_ip, dst, seg, tcplen));
    return ip_send(dst, IP_PROTO_TCP, seg, tcplen);
}

static void tcp_handle(const struct ip_hdr *ip, int iplen) {
    const struct tcp_hdr *th;
    int iphdr_len;
    int tcplen;
    const uint8_t *payload;
    int plen;
    uint32_t seq;
    uint32_t ack;
    uint8_t flags;
    uint16_t sport;
    uint16_t dport;
    uint32_t src;

    if (!tcp.active || iplen < (int)sizeof(struct ip_hdr))
        return;
    iphdr_len = (ip->ver_ihl & 0x0f) * 4;
    if (iphdr_len < 20 || iplen < iphdr_len + (int)sizeof(struct tcp_hdr))
        return;
    th = (const struct tcp_hdr *)((const uint8_t *)ip + iphdr_len);
    tcplen = iplen - iphdr_len;
    payload = (const uint8_t *)th + ((th->data_off >> 4) * 4);
    plen = tcplen - (int)(payload - (const uint8_t *)th);
    src = read_be32((const uint8_t *)ip + 12);
    sport = ntohs16(th->src_port);
    dport = ntohs16(th->dst_port);
    seq = ntohl32(th->seq);
    ack = ntohl32(th->ack);
    flags = th->flags;
    (void)ack;

    if (src != tcp.remote_ip || sport != tcp.remote_port || dport != tcp.local_port)
        return;

    if (flags & TCP_RST) {
        tcp_reset();
        return;
    }

    if (flags & TCP_SYN) {
        if (tcp.state == 1) {
            tcp.rcv_nxt = seq + 1;
            tcp.snd_nxt = tcp.iss + 1;
            tcp.state = 2;
            tcp_send_raw(tcp.remote_ip, tcp.local_port, tcp.remote_port, tcp.snd_nxt,
                         tcp.rcv_nxt, TCP_ACK, 0, 0);
        }
        return;
    }

    if (flags & TCP_ACK) {
        if (tcp.state == 1 && (flags & TCP_SYN))
            return;
        if (tcp.state == 1)
            tcp.state = 2;
    }

    if (plen > 0) {
        tcp_rx_append(payload, plen);
        tcp.rcv_nxt = seq + (uint32_t)plen;
        tcp_send_raw(tcp.remote_ip, tcp.local_port, tcp.remote_port, tcp.snd_nxt,
                     tcp.rcv_nxt, TCP_ACK, 0, 0);
    }

    if (flags & TCP_FIN) {
        tcp.rcv_nxt++;
        tcp_send_raw(tcp.remote_ip, tcp.local_port, tcp.remote_port, tcp.snd_nxt,
                     tcp.rcv_nxt, TCP_ACK | TCP_FIN, 0, 0);
        tcp_reset();
    }
}

static int tcp_connect(uint32_t ip, uint16_t port, int timeout_ms) {
    tcp_reset();
    tcp.active = 1;
    tcp.remote_ip = ip;
    tcp.remote_port = port;
    tcp.local_port = 49152;
    tcp.iss = 0x12345000;
    tcp.snd_nxt = tcp.iss;
    tcp.rcv_nxt = 0;
    tcp.state = 1;

    if (arp_resolve(ip, timeout_ms) != 0)
        return ETIMEDOUT;

    if (tcp_send_raw(ip, tcp.local_port, port, tcp.iss, 0, TCP_SYN, 0, 0) < 0)
        return EIO;

    for (int i = 0; i < timeout_ms * 30; i++) {
        netstack_poll();
        if (tcp.state == 2)
            return 0;
        for (volatile int j = 0; j < 500; j++)
            ;
    }
    tcp_reset();
    return ETIMEDOUT;
}

static int tcp_send_data(const void *data, int len) {
    if (!tcp.active || tcp.state != 2)
        return EINVAL;
    if (tcp_send_raw(tcp.remote_ip, tcp.local_port, tcp.remote_port, tcp.snd_nxt, tcp.rcv_nxt,
                     TCP_ACK | TCP_PSH, data, len) < 0)
        return EIO;
    tcp.snd_nxt += (uint32_t)len;
    return len;
}

static int tcp_recv_accumulate(char *buf, int buflen, int timeout_ms) {
    int pos = 0;
    int idle = 0;

    for (int i = 0; i < timeout_ms * 40; i++) {
        int n = tcp_rx_take(buf + pos, buflen - pos);
        if (n > 0) {
            pos += n;
            idle = 0;
            if (pos >= buflen)
                return pos;
            continue;
        }
        netstack_poll();
        idle++;
        if (pos > 0 && idle > timeout_ms * 5)
            return pos;
        for (volatile int j = 0; j < 400; j++)
            ;
    }
    return pos > 0 ? pos : ETIMEDOUT;
}

static void tcp_close_conn(void) {
    if (tcp.active && tcp.state == 2) {
        tcp_send_raw(tcp.remote_ip, tcp.local_port, tcp.remote_port, tcp.snd_nxt, tcp.rcv_nxt,
                     TCP_FIN | TCP_ACK, 0, 0);
    }
    tcp_reset();
}

static int parse_ipv4(const char *s, uint32_t *out) {
    uint32_t parts[4];
    int part = 0;
    int i = 0;
    uint32_t acc = 0;

    *out = 0;
    while (s[i]) {
        if (s[i] >= '0' && s[i] <= '9') {
            acc = acc * 10 + (uint32_t)(s[i] - '0');
            if (acc > 255)
                return 0;
        } else if (s[i] == '.') {
            if (part >= 3)
                return 0;
            parts[part++] = acc;
            acc = 0;
        } else {
            return 0;
        }
        i++;
    }
    if (part != 3)
        return 0;
    parts[3] = acc;
    *out = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
    return 1;
}

static int parse_url(const char *url, char *host, int host_len, uint16_t *port,
                     char *path, int path_len) {
    int i = 0;
    int hi = 0;
    int pi = 0;
    int has_port = 0;
    uint32_t port_acc = 0;

    if (!url || !host || !port || !path)
        return 0;
    host[0] = path[0] = '\0';
    *port = 80;

    if (url[0] != 'h' || url[1] != 't' || url[2] != 't' || url[3] != 'p')
        return 0;
    i = 4;
    if (url[i] == 's') {
        i++;
        *port = 443;
    }
    if (url[i] != ':' || url[i + 1] != '/' || url[i + 2] != '/')
        return 0;
    i += 3;

    while (url[i] && url[i] != '/' && url[i] != ':') {
        if (hi >= host_len - 1)
            return 0;
        host[hi++] = url[i++];
    }
    host[hi] = '\0';
    if (hi == 0)
        return 0;

    if (url[i] == ':') {
        i++;
        has_port = 1;
        port_acc = 0;
        while (url[i] >= '0' && url[i] <= '9') {
            port_acc = port_acc * 10 + (uint32_t)(url[i] - '0');
            if (port_acc > NET_TCP_PORT_MAX)
                return 0;
            i++;
        }
        *port = (uint16_t)port_acc;
    }
    if (!has_port && url[4] == 's')
        *port = 443;

    if (url[i] == '/') {
        while (url[i] && pi < path_len - 1)
            path[pi++] = url[i++];
    } else {
        path[0] = '/';
        path[1] = '\0';
        return 1;
    }
    path[pi] = '\0';
    return 1;
}

void netstack_init(void) {
    gw_mac_valid = 0;
    tcp_reset();
    virtio_net_init();
    if (!virtio_net_ready())
        return;
    our_ip = NET_IP_HOST;
    dns_server = NET_DNS_HOST;
    (void)net_dhcp_acquire(5000);
    kprintf("[net] stack init ip=%u.%u.%u.%u gw=10.0.2.2 dns=%u.%u.%u.%u\n",
            (our_ip >> 24) & 0xff, (our_ip >> 16) & 0xff, (our_ip >> 8) & 0xff,
            our_ip & 0xff, (dns_server >> 24) & 0xff, (dns_server >> 16) & 0xff,
            (dns_server >> 8) & 0xff, dns_server & 0xff);
    if (!gw_mac_valid && arp_resolve(NET_GW_HOST, 3000) != 0) {
        kprintf("[net] arp gw timeout (continuing)\n");
#ifdef PLATFORM_X86_64_PC
        net_slirp_fallback_mac();
        kprintf("[net] slirp gw mac fallback ok\n");
#endif
    } else
        kprintf("[net] arp gw ok\n");
}

int netstack_ready(void) {
    return virtio_net_ready();
}

void netstack_poll(void) {
    int len;
    int rc;

    if (!virtio_net_ready())
        return;
    rc = virtio_net_poll_frame(rx_frame, (int)sizeof(rx_frame), &len);
    if (rc != 0)
        return;
    if (len < 14)
        return;
    {
        struct eth_hdr *eth = (struct eth_hdr *)rx_frame;
        uint16_t type = ntohs16(eth->type);
        if (type == ETHERTYPE_ARP)
            arp_handle((const struct arp_pkt *)(rx_frame + 14), len - 14);
        else if (type == ETHERTYPE_IP && len >= 34) {
            struct ip_hdr *ip = (struct ip_hdr *)(rx_frame + 14);
            int iplen = ntohs16(ip->total_len);
            if (iplen > len - 14)
                iplen = len - 14;
            if (ip->proto == IP_PROTO_TCP)
                tcp_handle(ip, iplen);
            else if (ip->proto == IP_PROTO_UDP)
                udp_handle(ip, iplen);
        }
    }
}

int net_http_get(const char *url, char *body, int body_len) {
    char host[64];
    char path[128];
    char req[384];
    char resp[2048];
    uint16_t port;
    uint32_t ip;
    int rlen;
    int pos;
    int i;
    int status = 0;

    if (!netstack_ready() || !url || !body || body_len <= 0)
        return EINVAL;

    if (!parse_url(url, host, (int)sizeof(host), &port, path, (int)sizeof(path)))
        return EINVAL;
    if (port == 443)
        return EIO;

    if (!parse_ipv4(host, &ip)) {
        if (net_dns_resolve(host, &ip, 5000) != 0)
            return ETIMEDOUT;
    }

    if (!peer_mac_valid || peer_mac_ip != ip) {
        if (arp_resolve(ip, 3000) != 0)
            return ETIMEDOUT;
    }

    if (tcp_connect(ip, port, 8000) != 0)
        return EIO;

    pos = 0;
    {
        const char *pfx = "GET ";
        while (pfx[pos])
            req[pos] = pfx[pos], pos++;
    }
    for (i = 0; path[i] && pos < (int)sizeof(req) - 48; i++)
        req[pos++] = path[i];
    {
        const char *mid = " HTTP/1.1\r\nHost: ";
        for (i = 0; mid[i] && pos < (int)sizeof(req) - 32; i++)
            req[pos++] = mid[i];
    }
    for (i = 0; host[i] && pos < (int)sizeof(req) - 24; i++)
        req[pos++] = host[i];
    if (port != 80) {
        req[pos++] = ':';
        {
            char pbuf[8];
            int pi = 0;
            uint16_t p = port;
            char tmp[8];
            int ti = 0;
            if (p == 0)
                return EINVAL;
            while (p > 0) {
                tmp[ti++] = '0' + (p % 10);
                p /= 10;
            }
            while (ti > 0)
                pbuf[pi++] = tmp[--ti];
            for (int k = 0; k < pi && pos < (int)sizeof(req) - 16; k++)
                req[pos++] = pbuf[k];
        }
    }
    {
        const char *sfx = "\r\nConnection: close\r\n\r\n";
        for (i = 0; sfx[i] && pos < (int)sizeof(req) - 2; i++)
            req[pos++] = sfx[i];
    }
    req[pos] = '\0';

    if (tcp_send_data(req, pos) < 0) {
        tcp_close_conn();
        return EIO;
    }

    rlen = tcp_recv_accumulate(resp, (int)sizeof(resp) - 1, 8000);
    tcp_close_conn();
    if (rlen <= 0)
        return rlen < 0 ? rlen : EIO;
    resp[rlen] = '\0';

    if (resp[0] != 'H' || resp[1] != 'T' || resp[2] != 'T' || resp[3] != 'P')
        return EIO;
    {
        const char *p = resp;
        while (*p && *p != ' ')
            p++;
        if (*p == ' ')
            p++;
        while (*p >= '0' && *p <= '9') {
            status = status * 10 + (*p - '0');
            p++;
        }
    }
    if (status >= 400)
        return EIO;

    for (i = 0; i + 3 < rlen; i++) {
        if (resp[i] == '\r' && resp[i + 1] == '\n' && resp[i + 2] == '\r' &&
            resp[i + 3] == '\n') {
            int blen = rlen - (i + 4);
            if (blen >= body_len)
                blen = body_len - 1;
            for (int j = 0; j < blen; j++)
                body[j] = resp[i + 4 + j];
            body[blen] = '\0';
            kprintf("[net] http_get status=%d body_len=%d\n", status, blen);
            return blen;
        }
    }
    return EIO;
}

static char http_post_req[4096];

int net_http_post(const char *host, uint16_t port, const char *path, const char *headers,
                  const char *body, char *resp, int resp_len, int timeout_ms) {
    char resp_buf[8192];
    uint32_t ip;
    int pos = 0;
    int rlen;
    int i;
    int status = 0;

    if (!host || !path || !resp || resp_len <= 0 || port == 0)
        return EINVAL;

    if (!parse_ipv4(host, &ip)) {
        if (net_dns_resolve(host, &ip, timeout_ms) != 0)
            return ETIMEDOUT;
    }

    if (tcp_connect(ip, port, timeout_ms) != 0)
        return EIO;

    {
        const char *pfx = "POST ";
        for (i = 0; pfx[i] && pos < (int)sizeof(http_post_req) - 256; i++)
            http_post_req[pos++] = pfx[i];
    }
    for (i = 0; path[i] && pos < (int)sizeof(http_post_req) - 256; i++)
        http_post_req[pos++] = path[i];
    {
        const char *mid = " HTTP/1.1\r\nHost: ";
        for (i = 0; mid[i] && pos < (int)sizeof(http_post_req) - 256; i++)
            http_post_req[pos++] = mid[i];
    }
    for (i = 0; host[i] && pos < (int)sizeof(http_post_req) - 128; i++)
        http_post_req[pos++] = host[i];
    if (headers) {
        http_post_req[pos++] = '\r';
        http_post_req[pos++] = '\n';
        for (i = 0; headers[i] && pos < (int)sizeof(http_post_req) - 128; i++)
            http_post_req[pos++] = headers[i];
        http_post_req[pos++] = '\r';
        http_post_req[pos++] = '\n';
    }
    if (body) {
        int bl = 0;
        const char *hdr = "Content-Type: application/json\r\nContent-Length: ";
        for (i = 0; hdr[i] && pos < (int)sizeof(http_post_req) - 64; i++)
            http_post_req[pos++] = hdr[i];
        while (body[bl])
            bl++;
        {
            char clen[12];
            int ci = 0;
            int n = bl;
            char tmp[12];
            int ti = 0;
            if (n == 0)
                clen[ci++] = '0';
            while (n > 0) {
                tmp[ti++] = '0' + (n % 10);
                n /= 10;
            }
            while (ti > 0)
                clen[ci++] = tmp[--ti];
            clen[ci] = '\0';
            for (int j = 0; clen[j] && pos < (int)sizeof(http_post_req) - 32; j++)
                http_post_req[pos++] = clen[j];
        }
        http_post_req[pos++] = '\r';
        http_post_req[pos++] = '\n';
        http_post_req[pos++] = '\r';
        http_post_req[pos++] = '\n';
        for (i = 0; body[i] && pos < (int)sizeof(http_post_req) - 2; i++)
            http_post_req[pos++] = body[i];
    } else {
        const char *sfx = "\r\nConnection: close\r\n\r\n";
        for (i = 0; sfx[i] && pos < (int)sizeof(http_post_req) - 2; i++)
            http_post_req[pos++] = sfx[i];
    }
    http_post_req[pos] = '\0';

    if (tcp_send_data(http_post_req, pos) < 0) {
        tcp_close_conn();
        return EIO;
    }

    rlen = tcp_recv_accumulate(resp_buf, (int)sizeof(resp_buf) - 1, timeout_ms);
    tcp_close_conn();
    if (rlen <= 0)
        return rlen < 0 ? rlen : EIO;
    resp_buf[rlen] = '\0';

    if (resp_buf[0] != 'H')
        return EIO;
    {
        const char *p = resp_buf;
        while (*p && *p != ' ')
            p++;
        if (*p == ' ')
            p++;
        while (*p >= '0' && *p <= '9') {
            status = status * 10 + (*p - '0');
            p++;
        }
    }
    if (status >= 400)
        return EIO;

    for (i = 0; i + 3 < rlen; i++) {
        if (resp_buf[i] == '\r' && resp_buf[i + 1] == '\n' && resp_buf[i + 2] == '\r' &&
            resp_buf[i + 3] == '\n') {
            int blen = rlen - (i + 4);
            if (blen >= resp_len)
                blen = resp_len - 1;
            for (int j = 0; j < blen; j++)
                resp[j] = resp_buf[i + 4 + j];
            resp[blen] = '\0';
            kprintf("[net] http_post status=%d body_len=%d\n", status, blen);
            return blen;
        }
    }
    return EIO;
}

static int host_eq(const char *a, const char *b) {
    int i = 0;
    if (!a || !b)
        return 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i])
            return 0;
        i++;
    }
    return a[i] == b[i];
}

static void dns_cache_store(const char *host, uint32_t ip) {
    int i = 0;
    while (host[i] && i < (int)sizeof(dns_cache_host) - 1) {
        dns_cache_host[i] = host[i];
        i++;
    }
    dns_cache_host[i] = '\0';
    dns_cache_ip = ip;
    dns_cache_valid = 1;
}

int net_dns_resolve(const char *host, uint32_t *out_ip, int timeout_ms) {
    static const uint32_t udp_dns[] = {NET_DNS_HOST, 0x01010101UL, 0x09090909UL};
    int udp_ms = timeout_ms / 2;

    if (!host || !out_ip || !host[0])
        return EINVAL;
    if (parse_ipv4(host, out_ip))
        return 0;
    if (dns_cache_valid && host_eq(host, dns_cache_host)) {
        *out_ip = dns_cache_ip;
        return 0;
    }
    if (!netstack_ready())
        return ENODEV;
    if (udp_ms < 2000)
        udp_ms = 2000;

    for (int s = 0; s < (int)(sizeof(udp_dns) / sizeof(udp_dns[0])); s++) {
        dns_server = udp_dns[s];
        dns_result_ok = 0;
        dns_result_ip = 0;
        dns_saw_bogus = 0;
        if (arp_resolve(dns_server, udp_ms) != 0)
            continue;
        if (dns_send_query(host) < 0)
            continue;

        for (int t = 0; t < udp_ms * 20; t++) {
            netstack_poll();
            if (dns_result_ok) {
                *out_ip = dns_result_ip;
                kprintf("[net] dns %s -> %u.%u.%u.%u\n", host, (dns_result_ip >> 24) & 0xff,
                        (dns_result_ip >> 16) & 0xff, (dns_result_ip >> 8) & 0xff,
                        dns_result_ip & 0xff);
                dns_cache_store(host, dns_result_ip);
                return 0;
            }
            if (dns_saw_bogus)
                break;
            if (t > 0 && (t % 200) == 0)
                dns_send_query(host);
            for (volatile int j = 0; j < 400; j++)
                ;
        }
        if (dns_saw_bogus)
            break;
    }

    kprintf("[net] dns udp failed host=%s, trying DoH\n", host);
    if (dns_doh_resolve_a && dns_doh_resolve_a(host, out_ip, timeout_ms) == 0) {
        kprintf("[net] dns doh %s -> %u.%u.%u.%u\n", host, (*out_ip >> 24) & 0xff,
                (*out_ip >> 16) & 0xff, (*out_ip >> 8) & 0xff, *out_ip & 0xff);
        dns_cache_store(host, *out_ip);
        return 0;
    }
    kprintf("[net] dns timeout host=%s\n", host);
    return ETIMEDOUT;
}

int net_dns_resolve_slirp(const char *host, uint32_t *out_ip, int timeout_ms) {
    int udp_ms = timeout_ms;

    if (!host || !out_ip || !host[0])
        return EINVAL;
    if (parse_ipv4(host, out_ip))
        return 0;
    if (!netstack_ready())
        return ENODEV;
    if (udp_ms < 2000)
        udp_ms = 2000;

    dns_server = NET_DNS_HOST;
    dns_result_ok = 0;
    dns_result_ip = 0;
    dns_saw_bogus = 0;
    dns_accept_bogus = 1;
    if (arp_resolve(dns_server, udp_ms) == 0 && dns_send_query(host) >= 0) {
        for (int t = 0; t < udp_ms * 20; t++) {
            netstack_poll();
            if (dns_result_ok) {
                *out_ip = dns_result_ip;
                dns_accept_bogus = 0;
                return 0;
            }
            for (volatile int j = 0; j < 400; j++)
                ;
        }
    }
    dns_accept_bogus = 0;
    return ETIMEDOUT;
}

int net_tcp_connect_ip(uint32_t ip, uint16_t port, int timeout_ms) {
    if (port == 0)
        return EINVAL;
    return tcp_connect(ip, port, timeout_ms);
}

int net_tcp_connect_host(const char *host, uint16_t port, int timeout_ms) {
    uint32_t ip;

    if (!host || port == 0)
        return EINVAL;
    if (!parse_ipv4(host, &ip)) {
        if (net_dns_resolve(host, &ip, timeout_ms) != 0) {
            kprintf("[net] dns failed host=%s\n", host);
            return ETIMEDOUT;
        }
    }
    return tcp_connect(ip, port, timeout_ms);
}

int net_tcp_write(const void *data, int len) {
    if (!tcp.active)
        return EINVAL;
    return tcp_send_data(data, len);
}

int net_tcp_read(void *buf, int buflen, int timeout_ms) {
    if (!tcp.active)
        return EINVAL;
    return tcp_recv_accumulate(buf, buflen, timeout_ms);
}

void net_tcp_close(void) {
    tcp_close_conn();
}
