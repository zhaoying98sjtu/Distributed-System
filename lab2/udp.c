/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_udp.h>
#include <rte_ip.h>
#include <rte_net.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

struct ether_addr addr;
struct rte_mempool *mbuf_pool;

static const struct rte_eth_conf port_conf_default = {
	.rxmode = {
		.max_rx_pkt_len = ETHER_MAX_LEN,
		.ignore_offload_bitfield = 1,
	},
};

/* basicfwd.c: Basic DPDK skeleton forwarding example. */

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;
	int retval;
	uint16_t q;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf txconf;

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	rte_eth_dev_info_get(port, &dev_info);
	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (retval != 0)
		return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
				rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	txconf = dev_info.default_txconf;
	txconf.txq_flags = ETH_TXQ_FLAGS_IGNORE;
	txconf.offloads = port_conf.txmode.offloads;
	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, nb_txd,
				rte_eth_dev_socket_id(port), &txconf);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	/* Display the port MAC address. */
	rte_eth_macaddr_get(port, &addr);
	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	/* Enable RX in promiscuous mode for the Ethernet device. */
	rte_eth_promiscuous_enable(port);

	return 0;
}

/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and writing to an output port.
 */
static __attribute__((noreturn)) void
lcore_main(void)
{
	uint16_t port;
	/*
	 * Check that the port is on the same NUMA node as the polling thread
	 * for best performance.
	 */
	RTE_ETH_FOREACH_DEV(port)
		if (rte_eth_dev_socket_id(port) > 0 &&
				rte_eth_dev_socket_id(port) !=
						(int)rte_socket_id())
			printf("WARNING, port %u is on remote NUMA node to "
					"polling thread.\n\tPerformance will "
					"not be optimal.\n", port);

	printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
			rte_lcore_id());
	/* Run until the application is quit or killed. */
	for (;;) {
		struct rte_mbuf *mbuf;
		mbuf = rte_pktmbuf_alloc(mbuf_pool);
		char data[12] = "DPDKprogram\0";
			
		struct ether_hdr *eth_hdr = rte_pktmbuf_mtod(mbuf, struct ether_hdr *);
		struct ipv4_hdr *ip_hdr = rte_pktmbuf_mtod_offset(mbuf, struct ipv4_hdr *, sizeof(struct ether_hdr));
		struct udp_hdr *udp_hdr = rte_pktmbuf_mtod_offset(mbuf, struct udp_hdr *,
												 sizeof(struct ether_hdr)+sizeof(struct ipv4_hdr));
		struct ether_addr dst_addr;		//Physical MAC
		dst_addr.addr_bytes[0] = 0x00;
		dst_addr.addr_bytes[1] = 0x50;
		dst_addr.addr_bytes[2] = 0x56;
		dst_addr.addr_bytes[3] = 0xc0;
		dst_addr.addr_bytes[4] = 0x00;
		dst_addr.addr_bytes[5] = 0x02;
		
		eth_hdr->s_addr = addr;	
		eth_hdr->d_addr = dst_addr;
		eth_hdr->ether_type = htons(ETHER_TYPE_IPv4);

		ip_hdr->version_ihl = 0x45;
		ip_hdr->type_of_service = 0x00;
		ip_hdr->total_length = htons(12 + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
		ip_hdr->packet_id = 0;
		ip_hdr->fragment_offset = 0;
		ip_hdr->time_to_live = 0x80;
		ip_hdr->next_proto_id = IPPROTO_UDP;
		ip_hdr->hdr_checksum = 0;
		ip_hdr->src_addr = IPv4(1,255,168,192);
		ip_hdr->dst_addr = IPv4(1,0,0,127);
		ip_hdr->hdr_checksum = rte_ipv4_cksum(ip_hdr);

		udp_hdr->src_port = htons(1000);	
		udp_hdr->dst_port = htons(1001);
		udp_hdr->dgram_len = htons(12 + sizeof(struct udp_hdr));
		udp_hdr->dgram_cksum = 0x7271;
		uint32_t hdr_size = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr);
		char *payload = rte_pktmbuf_mtod_offset(mbuf, char *, hdr_size);
		memcpy(payload, data, 12);

		mbuf->pkt_len = hdr_size + 12;
		mbuf->data_len = mbuf->pkt_len;
		mbuf->next = NULL;
		
		rte_eth_tx_burst(0, 0, &mbuf, 1);
		printf("Success: Send a packet.\n");
		// rte_eth_stats_get(port, );
		/* Free */
		rte_pktmbuf_free(mbuf);
		sleep(1);
	}
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int
main(int argc, char *argv[])
{
	unsigned nb_ports;
	uint16_t portid;

	/* Initialize the Environment Abstraction Layer (EAL). */
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	argc -= ret;
	argv += ret;

	/* Check that there is an even number of ports to send/receive on. */
	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports < 2 || (nb_ports & 1)){
		printf("%d\n",nb_ports);
		// rte_exit(EXIT_FAILURE, "Error: number of ports must be even\n");
	}

	/* Creates a new mempool in memory to hold the mbufs. */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
		MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* Initialize all ports. */
	RTE_ETH_FOREACH_DEV(portid)
		if (port_init(portid, mbuf_pool) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu16 "\n",
					portid);

	if (rte_lcore_count() > 1)
		printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");

	/* Call lcore_main on the master core only. */
	lcore_main();

	return 0;
}
