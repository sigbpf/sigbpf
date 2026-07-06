#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h> // bpf_ntohs() e bpf_htonl()
#include <limits.h>

#define KBUILD_MODNAME "xdp_http"

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, USHRT_MAX);
    __type(key, __u32);
    __type(value, int); 
} mapa_sinal SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, USHRT_MAX);
    __type(key, __u32);
    __type(value, int); 
} pid_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, USHRT_MAX);
    __type(key, __u32);
    __type(value, int); 
} service_chain SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_XSKMAP);
    __uint(max_entries, 4); 
    __type(key, __u32);
    __type(value, __u32);
} xsk_map SEC(".maps");

/*****************************************************************************/

#define MAX_METHOD_LEN 7  /* longest method we care about (e.g. "DELETE"=6) */

/* Representação de 10.10.1.2 em formato Big Endian (Network Byte Order) 
   (10 << 24) | (10 << 16) | (1 << 8) | 2 -> convertido via bpf_htonl */
#define TARGET_SRC_IP 0x0A0A0102 

SEC("xdp")
int xdp_prog(struct xdp_md *ctx){

    void *data     = (void *)(long)ctx->data;
    void *data_end = (void *)(long)ctx->data_end;

    /* Ethernet header */
    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end)
        return XDP_PASS;

    /* Only IPv4 */
    if (eth->h_proto != bpf_htons(0x0800))
        return XDP_PASS;

    /* IP header */
    struct iphdr *iph = (void *)(eth + 1);
    if ((void *)iph + sizeof(*iph) > data_end)
        return XDP_PASS;

    /* FILTRO DE IP ADICIONADO AQUI:
       Verifica se o IP de origem é diferente de 10.10.1.2.
       Nota: O campo saddr já está em Network Byte Order. */
    if (iph->saddr != bpf_htonl(TARGET_SRC_IP))
        return XDP_DROP;

    if (iph->protocol != IPPROTO_TCP)
        return XDP_PASS;

    /* ip header length (bytes) and check */
    unsigned int ihl = iph->ihl * 4;
    if (ihl < sizeof(*iph))
        return XDP_PASS;
    if ((void *)iph + ihl > data_end)
        return XDP_PASS;

    /* TCP header */
    struct tcphdr *tcph = (void *)iph + ihl;
    if ((void *)tcph + sizeof(*tcph) > data_end)
        return XDP_PASS;

    /* tcp header length and check */
    unsigned int tcph_len = tcph->doff * 4;
    if (tcph_len < sizeof(*tcph))
        return XDP_PASS;
    if ((void *)tcph + tcph_len > data_end)
        return XDP_PASS;

    /* Optional: only inspect common HTTP ports to reduce noise. */
        __u16 dport = bpf_ntohs(tcph->dest);
        __u16 sport = bpf_ntohs(tcph->source);
        /* if not client->server to port 80/8080, skip */
        if (dport != 80 && dport != 8080 && sport != 80 && sport != 8080)
            return XDP_PASS;

    /* Payload pointer and length */
    unsigned char *payload = (unsigned char *)tcph + tcph_len;
    if (payload >= (unsigned char *)data_end)
        return XDP_PASS;

    long payload_len = (long)((unsigned char *)data_end - payload);
    if (payload_len <= 0)
        return XDP_PASS;

    /* Limit how many bytes we will inspect for the method */
    int max_inspect = payload_len < MAX_METHOD_LEN ? (int)payload_len : MAX_METHOD_LEN;

    /* Buffer for method + NUL */
    char method[MAX_METHOD_LEN + 1];
    #pragma clang loop unroll(full)
    for (int i = 0; i < MAX_METHOD_LEN + 1; i++)
	    method[i] = 0;

    int method_len = 0;

    /* Unrolled loop with per-iteration bound checks to satisfy verifier */
    #pragma clang loop unroll(full)
    for (int i = 0; i < MAX_METHOD_LEN; i++) {
        /* Ensure we won't read past data_end for payload[i] */
        if ((void *)(payload + i + 1) > data_end)
            break;

        if (i >= max_inspect)
            break;

        char c = payload[i];
        /* HTTP method ends at first space or CR/LF */
        if (c == ' ' || c == '\r' || c == '\n')
            break;

        /* store character */
        method[i] = c;
        method_len++;
    }

    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";

