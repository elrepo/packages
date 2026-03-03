/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2025 Broadcom
 * All rights reserved.
 */

#ifndef BNXT_QUIC_USR_INCLUDE_H
#define BNXT_QUIC_USR_INCLUDE_H

#define BNXT_CONN_ID_MAX_LEN		20
#define BNXT_MAX_KEY_SIZE		32
#define BNXT_IV_SIZE			12

struct bnxt_quic_connection_info {
	u16 cipher;
	u16 version;
	u8  offload_dir;
	u64 tx_conn_id;
	u64 rx_conn_id;
	uint8_t tx_data_key[BNXT_MAX_KEY_SIZE];
	uint8_t tx_hdr_key[BNXT_MAX_KEY_SIZE];
	uint8_t tx_iv[BNXT_IV_SIZE];
	uint8_t rx_data_key[BNXT_MAX_KEY_SIZE];
	uint8_t rx_hdr_key[BNXT_MAX_KEY_SIZE];
	uint8_t rx_iv[BNXT_IV_SIZE];
	uint16_t family;
	__be16 dport;
	struct sockaddr_storage daddr;
	__be16 sport;
	struct sockaddr_storage saddr;
	u64 pkt_number;
	u32 dst_conn_id_width;
};

#endif /* ifndef BNXT_QUIC_USR_INCLUDE_H */
