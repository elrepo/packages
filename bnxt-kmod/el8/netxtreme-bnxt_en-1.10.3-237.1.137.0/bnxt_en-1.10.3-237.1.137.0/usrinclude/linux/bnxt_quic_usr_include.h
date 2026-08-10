/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2025 Broadcom
 * All rights reserved.
 */

#ifndef _UAPI_LINUX_BNXT_QUIC_USR_INCLUDE_H
#define _UAPI_LINUX_BNXT_QUIC_USR_INCLUDE_H

#include <linux/sockios.h>
#include <linux/types.h>

#define BNXT_MAX_KEY_SIZE	32 /* Max size of crypto key in bytes.
				    * Accommodates 256-bit keys for AES-256-GCM
				    * and ChaCha20-Poly1305.
				    */
#define BNXT_IV_SIZE		12 /* Size of Initialization Vector in bytes.
				    * Matches 96-bit IVs for AES-GCM in QUIC/TLS 1.3.
				    */

#define SIOCDEVQUICFLOWADD SIOCDEVPRIVATE	/* Add QUIC offload flow */
#define SIOCDEVQUICFLOWDEL (SIOCDEVPRIVATE + 1)	/* Delete QUIC offload flow */

struct bnxt_quic_connection_info {
	__u16 cipher;			/* Negotiated IETF QUIC cipher suite */
	__u8 offload_dir;		/* Offload direction (RX=0, TX=1) */

	__u64 tx_conn_id;		/* Client (source) 1-RTT Connection ID */
	__u64 rx_conn_id;		/* Server (destination) 1-RTT Connection ID */

	__u8 tx_data_key[BNXT_MAX_KEY_SIZE];	/* Derived 1-RTT traffic key for transmit data encryption */
	__u8 tx_hdr_key[BNXT_MAX_KEY_SIZE];	/* Derived 1-RTT header protection key for transmit */
	__u8 tx_iv[BNXT_IV_SIZE];		/* Initialization Vector (IV) for transmit */

	__u8 rx_data_key[BNXT_MAX_KEY_SIZE];	/* Derived 1-RTT traffic key for receive data decryption */
	__u8 rx_hdr_key[BNXT_MAX_KEY_SIZE];	/* Derived 1-RTT header protection key for receive */
	__u8 rx_iv[BNXT_IV_SIZE];		/* Initialization Vector (IV) for receive */

	struct sockaddr_storage daddr;	/* Destination IP address storage */
	struct sockaddr_storage saddr;	/* Source IP address storage */

	__u64 pkt_number;		/* Initial packet number for 1-RTT */
	__u32 dst_conn_id_width;	/* Destination Connection ID length */
};

#endif /* _UAPI_LINUX_BNXT_QUIC_USR_INCLUDE_H */
