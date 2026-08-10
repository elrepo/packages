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
#define SIOCDEVQUICFLOWFLUSH (SIOCDEVPRIVATE + 2)	/* Flush all QUIC offload flows */

/**
 * struct bnxt_quic_flow_info - Common flow identification (5-tuple)
 * @daddr: Destination IP address storage (AF_INET or AF_INET6)
 * @saddr: Source IP address storage (AF_INET or AF_INET6)
 *
 * Base structure shared by add and delete operations.
 * Contains only the 5-tuple needed to identify a flow (IP addresses
 * and ports are embedded inside the sockaddr_storage).
 */
struct bnxt_quic_flow_info {
	struct sockaddr_storage daddr;	/* Destination IP address storage */
	struct sockaddr_storage saddr;	/* Source IP address storage */
};

/**
 * Key mask bits for bnxt_quic_connection_info.key_mask and
 * bnxt_quic_flow_del_info.key_mask
 *
 * Each bit corresponds to a specific direction + key_phase combination.
 * These match the kernel's internal key_valid bitmask encoding.
 *
 * For ADD: specifies which direction+phase keys to install.
 *   All bits must refer to the same key_phase (0 or 1). TX keys use
 *   the tx_* crypto fields; RX keys use the rx_* crypto fields.
 *
 * For DELETE: specifies which direction+phase keys to remove.
 *   Bits may span multiple phases and directions.
 */
#define BNXT_QUIC_KEY_RX_PHASE_0	(1U << 0)  /* 0x01 - RX key phase 0 */
#define BNXT_QUIC_KEY_RX_PHASE_1	(1U << 1)  /* 0x02 - RX key phase 1 */
#define BNXT_QUIC_KEY_TX_PHASE_0	(1U << 2)  /* 0x04 - TX key phase 0 */
#define BNXT_QUIC_KEY_TX_PHASE_1	(1U << 3)  /* 0x08 - TX key phase 1 */
#define BNXT_QUIC_KEY_ALL		(BNXT_QUIC_KEY_RX_PHASE_0 | \
					 BNXT_QUIC_KEY_RX_PHASE_1 | \
					 BNXT_QUIC_KEY_TX_PHASE_0 | \
					 BNXT_QUIC_KEY_TX_PHASE_1) /* 0x0f - all keys */
#define BNXT_QUIC_KEY_TX_ALL		(BNXT_QUIC_KEY_TX_PHASE_0 | \
					 BNXT_QUIC_KEY_TX_PHASE_1) /* 0x0c - all TX keys */
#define BNXT_QUIC_KEY_RX_ALL		(BNXT_QUIC_KEY_RX_PHASE_0 | \
					 BNXT_QUIC_KEY_RX_PHASE_1) /* 0x03 - all RX keys */

/* Convenience masks for a single phase (TX + RX at same phase) */
#define BNXT_QUIC_KEY_PHASE_0		(BNXT_QUIC_KEY_RX_PHASE_0 | \
					 BNXT_QUIC_KEY_TX_PHASE_0) /* 0x05 - TX + RX at phase 0 */
#define BNXT_QUIC_KEY_PHASE_1		(BNXT_QUIC_KEY_RX_PHASE_1 | \
					 BNXT_QUIC_KEY_TX_PHASE_1) /* 0x0a - TX + RX at phase 1 */

/**
 * struct bnxt_quic_connection_info - QUIC flow add parameters
 * @flow: Common flow identification (5-tuple)
 * @cipher: Negotiated IETF QUIC cipher suite (TLS_CIPHER_AES_GCM_128/256)
 * @key_mask: Bitmask of keys to install (BNXT_QUIC_KEY_* flags)
 * @dst_conn_id_width: Destination Connection ID length (0 or 8)
 * @tx_conn_id: Client (source) 1-RTT Connection ID
 * @rx_conn_id: Server (destination) 1-RTT Connection ID
 * @pkt_number: Initial packet number for 1-RTT
 * @tx_data_key: Derived 1-RTT traffic key for transmit data encryption
 * @tx_hdr_key: Derived 1-RTT header protection key for transmit
 * @tx_iv: Initialization Vector (IV) for transmit
 * @rx_data_key: Derived 1-RTT traffic key for receive data decryption
 * @rx_hdr_key: Derived 1-RTT header protection key for receive
 * @rx_iv: Initialization Vector (IV) for receive
 *
 * Used with SIOCDEVQUICFLOWADD ioctl. Contains the full crypto parameters
 * needed to program one or more keys into hardware.
 *
 * key_mask selects which direction(s) and phase to install. All bits
 * in key_mask must refer to the same key_phase (0 or 1), since there
 * is one set of TX keys and one set of RX keys per call.
 *
 * TX key bits use the tx_data_key/tx_hdr_key/tx_iv fields.
 * RX key bits use the rx_data_key/rx_hdr_key/rx_iv fields.
 *
 * Note: RX offload is not currently supported. Setting any RX bits
 * in key_mask will return -EOPNOTSUPP.
 *
 * Example - add TX key at phase 0 (current usage):
 *   conn_info.key_mask = BNXT_QUIC_KEY_TX_PHASE_0;
 *
 * Example - add TX key at phase 1 (key update):
 *   conn_info.key_mask = BNXT_QUIC_KEY_TX_PHASE_1;
 *
 * Example - add both TX and RX at phase 0 (future, when RX supported):
 *   conn_info.key_mask = BNXT_QUIC_KEY_TX_PHASE_0 | BNXT_QUIC_KEY_RX_PHASE_0;
 */
struct bnxt_quic_connection_info {
	struct bnxt_quic_flow_info flow;	/* Common 5-tuple identification */

	__u16 cipher;			/* Negotiated IETF QUIC cipher suite */
	__u8 key_mask;			/* Bitmask of keys to add (BNXT_QUIC_KEY_*) */
	__u8 reserved;			/* Padding for alignment */

	__u32 dst_conn_id_width;	/* Destination Connection ID length */

	__u64 tx_conn_id;		/* Client (source) 1-RTT Connection ID */
	__u64 rx_conn_id;		/* Server (destination) 1-RTT Connection ID */

	__u64 pkt_number;		/* Initial packet number for 1-RTT */

	__u8 tx_data_key[BNXT_MAX_KEY_SIZE];	/* TX traffic key */
	__u8 tx_hdr_key[BNXT_MAX_KEY_SIZE];	/* TX header protection key */
	__u8 tx_iv[BNXT_IV_SIZE];		/* TX Initialization Vector */

	__u8 rx_data_key[BNXT_MAX_KEY_SIZE];	/* RX traffic key */
	__u8 rx_hdr_key[BNXT_MAX_KEY_SIZE];	/* RX header protection key */
	__u8 rx_iv[BNXT_IV_SIZE];		/* RX Initialization Vector */
};

/**
 * struct bnxt_quic_flow_del_info - QUIC flow delete parameters
 * @flow: Common flow identification (5-tuple)
 * @key_mask: Bitmask of keys to delete (combination of BNXT_QUIC_KEY_* flags)
 *
 * Used with SIOCDEVQUICFLOWDEL ioctl. Identifies a flow by 5-tuple and
 * specifies which keys to delete via key_mask. Multiple keys can be
 * deleted in a single ioctl call by OR-ing multiple BNXT_QUIC_KEY_* flags.
 *
 * No crypto parameters, cipher, connection IDs, or packet numbers are
 * needed for deletion -- only the 5-tuple to find the flow and the
 * key_mask to specify which keys to remove.
 *
 * If all keys in a flow are deleted, the flow is automatically removed
 * from the hash table and freed.
 *
 * Example - delete all TX keys:
 *   del_info.key_mask = BNXT_QUIC_KEY_TX_ALL;
 *
 * Example - delete a single key:
 *   del_info.key_mask = BNXT_QUIC_KEY_TX_PHASE_0;
 *
 * Example - delete everything:
 *   del_info.key_mask = BNXT_QUIC_KEY_ALL;
 */
struct bnxt_quic_flow_del_info {
	struct bnxt_quic_flow_info flow;	/* Common 5-tuple identification */
	__u8 key_mask;				/* Bitmask of keys to delete */
	__u8 reserved[7];			/* Padding for alignment */
};

#endif /* _UAPI_LINUX_BNXT_QUIC_USR_INCLUDE_H */
