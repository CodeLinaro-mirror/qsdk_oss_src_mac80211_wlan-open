/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef HAL_MON_CMN_H
#define HAL_MON_CMN_H

#include "hw.h"
#include "qcn_extns/ath12k_cmn_extn.h"

#define HAL_RX_MON_MAX_AGGR_SIZE	128
#define HAL_RX_MAX_MPDU				256
#define HAL_RX_NUM_WORDS_PER_PPDU_BITMAP	(HAL_RX_MAX_MPDU >> 5)
#define EHT_MAX_USER_INFO	4
#define HAL_MAX_UL_MU_USERS	37
#define HAL_RX_MON_FCS_LEN	4

#define HAL_RX_MON_MPDU_ERR_FCS			BIT(0)
#define HAL_RX_MON_MPDU_ERR_DECRYPT		BIT(1)
#define HAL_RX_MON_MPDU_ERR_TKIP_MIC		BIT(2)
#define HAL_RX_MON_MPDU_ERR_AMSDU_ERR		BIT(3)
#define HAL_RX_MON_MPDU_ERR_OVERFLOW		BIT(4)
#define HAL_RX_MON_MPDU_ERR_MSDU_LEN		BIT(5)
#define HAL_RX_MON_MPDU_ERR_MPDU_LEN		BIT(6)
#define HAL_RX_MON_MPDU_ERR_UNENCRYPTED_FRAME	BIT(7)

#define HAL_RX_UL_OFDMA_USER_INFO_V0_W0_VALID		BIT(30)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W0_VER		BIT(31)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W1_NSS		GENMASK(2, 0)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W1_MCS		GENMASK(6, 3)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W1_LDPC		BIT(7)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W1_DCM		BIT(8)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W1_RU_START	GENMASK(15, 9)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W1_RU_SIZE		GENMASK(18, 16)

#define HE_GI_0_8 0
#define HE_GI_0_4 1
#define HE_GI_1_6 2
#define HE_GI_3_2 3

#define HE_LTF_1_X 0
#define HE_LTF_2_X 1
#define HE_LTF_4_X 2

#define HE_GI_RADIOTAP_0_8 0
#define HE_GI_RADIOTAP_1_6 1
#define HE_GI_RADIOTAP_3_2 2
#define HE_GI_RADIOTAP_RESERVED 3

#define HE_LTF_RADIOTAP_UNKNOWN 0
#define HE_LTF_RADIOTAP_1_X 1
#define HE_LTF_RADIOTAP_2_X 2
#define HE_LTF_RADIOTAP_4_X 3

#define MAX_RU_INDEX 0x7

#define ATH12K_LE32_DEC_ENC(value, dec_bits, enc_bits)	\
		u32_encode_bits(le32_get_bits(value, dec_bits), enc_bits)

#define ATH12K_LE64_DEC_ENC(value, dec_bits, enc_bits) \
		u32_encode_bits(le64_get_bits(value, dec_bits), enc_bits)
struct ath12k_mon_data;

struct hal_rx_u_sig_info {
	bool ul_dl;
	u8 bw;
	u8 ppdu_type_comp_mode;
	u8 eht_sig_mcs;
	u8 num_eht_sig_sym;
	struct ieee80211_radiotap_eht_usig usig;
};

struct hal_rx_tlv_aggr_info {
	bool in_progress;
	u16 cur_len;
	u16 tlv_tag;
	u8 buf[HAL_RX_MON_MAX_AGGR_SIZE];
};

struct hal_rx_user_status {
	u32 mcs:4,
	nss:3,
	ofdma_info_valid:1,
	ul_ofdma_ru_start_index:7,
	ul_ofdma_ru_width:7,
	ul_ofdma_ru_size:8;
	u32 ul_ofdma_user_v0_word0;
	u32 ul_ofdma_user_v0_word1;
	u16 ast_index; // End User Stat
	u16 sw_peer_id;// mpdu start
	u16 tid;
	u16 tcp_msdu_count;
	u16 tcp_ack_msdu_count;
	u16 udp_msdu_count;
	u16 other_msdu_count;
	u8 frame_control;
	u8 frame_control_info_valid:1,
	data_sequence_control_info_valid:1;
	u16 first_data_seq_ctrl;
	u8 preamble_type;
	u16 duration;
	u16 ht_flags;
	u16 vht_flags;
	u16 he_flags;
	u8  vht_flag_values2;
	u8  vht_flag_values3[4];
	u8  vht_flag_values4;
	u8  vht_flag_values5;
	u16 vht_flag_values6;
	u16 he_flags1;
	u16 he_flags2;
	u16 he_data1;
	u16 he_data2;
	u16 he_data3;
	u16 he_data4;
	u16 he_data5;
	u16 he_data6;
	u32 eht_user_info;
	u8  he_RU[8];
	u8 rs_flags;
	u8 ldpc;
	u16 mpdu_cnt_fcs_ok;
	u16 mpdu_cnt_fcs_err;
	u32 mpdu_ok_byte_count;
	u32 mpdu_err_byte_count;
	bool ampdu_present;
	u16 ampdu_id;
	u32 errmap;
	u32 mpdu_retry;
	u8 filter_category;
	u8 enc_type;
	u16 retried_msdu_count;
	u16 start_seq;
	u16 ba_control;
	u32 ba_bitmap[32];
	u16 ba_bitmap_sz;
	u16 aid;
};

struct hal_rx_eht_info {
	u8 num_user_info;
	struct hal_rx_radiotap_eht eht;
	u32 user_info[EHT_MAX_USER_INFO];
};

struct hal_rx_mon_mpdu_info {
	u8  decap_type:3,
	    mpdu_start_received:1,
	    first_rx_hdr_rcvd:1,
	    rx_hdr_rcvd:1,
	    raw_mpdu:1,
	    truncated:1;
	u32 err_bitmap;
};

struct hal_rx_nrp_info {
	uint32_t fc_valid : 1,
		 frame_control : 16,
		 to_ds_flag : 1,
		 mac_addr2_valid : 1,
		 mcast_bcast : 1;
	uint8_t mac_addr2[ETH_ALEN];
};

struct hal_rx_mon_msdu_info {
	u32 first_buffer:1,
	    last_buffer:1;
};

struct hal_rx_user_ctrl_frm_info {
	 uint8_t bar : 1,
		 ndpa : 1;
};

struct hal_rx_mon_ppdu_info {
	u16 ppdu_id;
	u16 last_ppdu_id;
	u64 ppdu_ts;
	u16 num_mpdu_fcs_ok;
	u16 num_mpdu_fcs_err;
	u32 mpdu_len;
	u16 chan_num;
	u16 freq;
	u16 tcp_msdu_count;
	u16 tcp_ack_msdu_count;
	u16 udp_msdu_count;
	u16 other_msdu_count;
	u16 peer_id;
	u8 rate;
	u8 vht_flag_values1;
	u8 vht_flag_values2;
	u8 vht_flag_values3[4];
	u8 vht_flag_values4;
	u8 vht_flag_values5;
	u16 vht_flag_values6;
	u8 gi;
	u8 rssi_comb;
	u16 tid;
	u8 fc_valid;
	u32 ht_flags : 1,
	    vht_flags : 1,
	    he_flags : 1,
	    he_mu_flags : 1,
	    usig_flags : 1,
	    eht_flags : 1,
	    mcs : 4,
	    nss : 3,
	    bw : 4,
	    is_stbc : 1,
	    sgi : 2,
	    he_re : 1,
	    ldpc : 1,
	    beamformed : 1,
	    dcm : 1,
	    preamble_type : 4,
	    reserved : 4;
	u8 ru_alloc;
	u8 reception_type;
	u64 tsft;
	u64 rx_duration;
	u8 frame_control;
	u16 ast_index;
	u8  rtap_flags;
	u8 rs_fcs_err;
	u8 rs_flags;
	u8 cck_flag;
	u8 ofdm_flag;
	u8 ulofdma_flag;
	u8 frame_control_info_valid;
	u16 he_per_user_1;
	u16 he_per_user_2;
	u8 he_per_user_position;
	u8 he_per_user_known;
	u16 he_flags1;
	u16 he_flags2;
	u8 he_RU[4];
	u16 he_data1;
	u16 he_data2;
	u16 he_data3;
	u16 he_data4;
	u16 he_data5;
	u16 he_data6;
	u32 l_sig_a_info;
	u32 l_sig_b_info;
	u32 usig_common;
	u32 usig_value;
	u32 usig_mask;
	u8  ht_mcs;
	u32 eht_known;
	u32 eht_data[9];
	u8  num_eht_user_info_valid;
	u32 ppdu_len;
	u16 prev_ppdu_id;
	u32 device_id;
	u16 first_data_seq_ctrl;
	u8 monitor_direct_used;
	u8 data_sequence_control_info_valid;
	u8 ltf_size;
	u8 rxpcu_filter_pass;
	s8 rssi_chain[8][8];
	u8 num_users;
	u8 addr1[ETH_ALEN];
	u8 addr2[ETH_ALEN];
	u8 addr3[ETH_ALEN];
	u8 addr4[ETH_ALEN];
	struct hal_rx_user_status userstats[HAL_MAX_UL_MU_USERS];
	u8 userid;
	bool first_msdu_in_mpdu;
	bool is_ampdu;
	u8 medium_prot_type;
	bool ppdu_continuation;
	bool eht_usig;
	u8 usr_nss_sum;
	u16 usr_ru_tones_sum;
	struct hal_rx_u_sig_info u_sig_info;
	bool is_eht;
	struct hal_rx_eht_info eht_info;
	struct hal_rx_tlv_aggr_info tlv_aggr;
	struct hal_rx_nrp_info nrp_info;
	u32 errmap;
	u32 mpdu_retry;
	u8 grp_id;
	u8 decap_format;
	u16 mpdu_retry_cnt;
	struct hal_rx_mon_mpdu_info mpdu_info[HAL_MAX_UL_MU_USERS];
	struct sk_buff_head mpdu_q[HAL_MAX_UL_MU_USERS];
	bool is_drop_tlv;
	struct hal_rx_mon_msdu_info msdu_info[HAL_MAX_UL_MU_USERS];
	u8 user_id;
	u16 retried_msdu_count;
	u8 rssi_region_offset;
	u16 punctured_pattern;
	u16 punc_bw;
	struct hal_rx_user_ctrl_frm_info ctrl_frm_info[HAL_MAX_UL_MU_USERS];
	struct hal_mon_ppdu_info_extn ppdu_info_extn;
};

/* in the bitmap 0 indicates no puncturing and 1 indicate that sub channel is punctured */
#define PUNCTURE_NONE    0x0000
#define PUNCTURE_INVALID 0xFFFF
#define PUNCTURE_80MHZ_MASK 0xF
#define PUNCTURE_160MHZ_MASK 0xFF
#define PUNCTURE_320MHZ_MASK 0xFFFF
#define PUNCTURE_40MHZ_MASK 0x3

/*
 * ieee80211_punc_type:
 * Type of puncturing denoting the number of bits that are punctured.
 * Each bit represents a 20MHz channel and therefore, each enum represents
 * the number of 20MHz channels that are punctured.
 */
enum ieee80211_punc_type {
	IEEE80211_PUNC_NONE        = 0,
	IEEE80211_PUNC_MINUS20MHZ  = 1,
	IEEE80211_PUNC_MINUS40MHZ  = 2,
	IEEE80211_PUNC_MINUS60MHZ  = 3,
	IEEE80211_PUNC_MINUS80MHZ  = 4,
	IEEE80211_PUNC_MINUS100MHZ = 5,
	IEEE80211_PUNC_MINUS120MHZ = 6,
	IEEE80211_PUNC_INVALID,
};

struct hal_rx_mon_status_tlv_hdr {
	u32 hdr;
	u8 value[];
};

enum hal_rx_mon_status {
	HAL_RX_MON_STATUS_PPDU_NOT_DONE,
	HAL_RX_MON_STATUS_PPDU_DONE,
	HAL_RX_MON_STATUS_BUF_DONE,
	HAL_RX_MON_STATUS_BUF_ADDR,
	HAL_RX_MON_STATUS_MPDU_START,
	HAL_RX_MON_STATUS_MPDU_END,
	HAL_RX_MON_STATUS_MSDU_END,
	HAL_RX_MON_STATUS_RX_HDR,
	HAL_RX_MON_STATUS_DROP_TLV,
};

enum hal_tx_mon_status {
	HAL_TX_MON_STATUS_PPDU_NOT_DONE,
	HAL_TX_MON_MPDU_START,
	HAL_TX_MON_MPDU_END,
	HAL_TX_MON_FES_SETUP,
	HAL_TX_MON_FES_STATUS_END,
	HAL_TX_MON_PEER_ENTRY,
	HAL_TX_MON_QUEUE_EXTENSION,
	HAL_RX_MON_RESPONSE_REQUIRED_INFO,
	HAL_TX_MON_FES_STATUS_START,
	HAL_TX_MON_FES_STATUS_PROT,
	HAL_TX_MON_FES_STATUS_START_PPDU,
	HAL_TX_MON_FES_STATUS_START_PROT,
	HAL_TX_MON_FES_STATUS_USER_PPDU,
	HAL_TX_MON_FES_STATUS_ACK_OR_BA,
	HAL_TX_MON_FRAME_BITMAP_ACK,
	HAL_TX_MON_FRAME_BITMAP_BLOCK_ACK_1K,
	HAL_TX_MON_COEX_TX_STATUS,
	HAL_TX_MON_MSDU_START,
	HAL_TX_MON_MSDU_END,
	HAL_TX_MON_RESPONSE_END_STATUS_INFO,
	HAL_TX_MON_PCU_PPDU_SETUP_INIT,
	HAL_TX_MON_MACTX_HE_SIG_A_SU,
	HAL_TX_MON_MACTX_HE_SIG_A_MU_DL,
	HAL_TX_MON_MACTX_HE_SIG_B1_MU,
	HAL_TX_MON_MACTX_HE_SIG_B2_MU,
	HAL_TX_MON_MACTX_HE_SIG_B2_OFDMA,
	HAL_TX_MON_MACTX_VHT_SIG,
	HAL_TX_MON_MACTX_L_SIG_A,
	HAL_TX_MON_MACTX_L_SIG_B,
	HAL_TX_MON_MACTX_HT_SIG,
	HAL_TX_MON_MACTX_PHY_DESC,
	HAL_TX_MON_BUFFER_ADDR,
	HAL_TX_MON_DATA,
	HAL_TX_MON_FW2SW,
};

/**
 * struct hal_tx_mon_packet_info - packet info
 * @sw_cookie: 64-bit SW desc virtual address
 * @dma_length: packet DMA length
 * @msdu_continuation: msdu continulation in next buffer
 * @truncated: packet is truncated
 */
struct hal_tx_mon_packet_info {
	u64 sw_cookie;
	u32 dma_length : 16,
	    msdu_continuation : 1,
	    truncated : 1,
	    reserved : 14;
};

/**
 * struct hal_tx_mon_ppdu_info - tx monitor ppdu information
 * @ppdu_id:  Id of the PLCP protocol data unit
 * @num_users: number of users
 * @cur_usr_idx: Current user index of the PPDU
 * @ack_recvd: boolean flag to indicate if ack is received
 * @cts_recvd: boolean flag to indicate if cts is received
 * @su_or_mu: type of transmission used like su, mu, mu_su transmission
 * @mu_type: mu transmission information
 * @reserved: for future purpose
 * @prot_tlv_status: protection tlv status
 * @ack_rssi: rssi of received ack. Valid only if ack_recvd is set
 * @ba_user_id: block ack user id. keeps track for ba payload build
 * @packet_info: packet information
 * @rx_status: monitor mode rx status information
 */
struct hal_tx_mon_ppdu_info {
	u32 ppdu_id;
	u8  num_users;
	u32 cur_usr_idx : 8,
	    ack_recvd : 1,
	    cts_recvd : 1,
	    su_or_mu :2,
	    mu_type :1,
	    reserved : 19;
	u32 prot_tlv_status;
	u8  ack_rssi;
	u8  ba_user_id;
	struct hal_tx_mon_packet_info packet_info;
	struct hal_rx_mon_ppdu_info rx_status;
};

/**
 * struct hal_txmon_word_mask_config - hal tx monitor word mask filter setting
 * Add more members to this structure, if extended in upcoming h/ws
 * @pcu_ppdu_setup_init: PCU_PPDU_SETUP TLV word mask
 * @tx_peer_entry: TX_PEER_ENTRY TLV word mask
 * @tx_queue_ext: TX_QUEUE_EXTENSION TLV word mask
 * @tx_fes_status_end: TX_FES_STATUS_END TLV word mask
 * @response_end_status: RESPONSE_END_STATUS TLV word mask
 * @tx_fes_status_prot: TX_FES_STATUS_PROT TLV word mask
 * @tx_fes_setup: TX_FES_SETUP TLV word mask
 * @tx_msdu_start: TX_MSDU_START TLV word mask
 * @tx_mpdu_start: TX_MPDU_START TLV word mask
 * @rxpcu_user_setup: RXPCU_USER_SETUP TLV word mask
 * @compaction_enable: flag to enable word mask compaction
 */
struct hal_tx_mon_wmask_config {
	u32 pcu_ppdu_setup_init;
	u16 tx_peer_entry;
	u16 tx_queue_ext;
	u16 tx_fes_status_end;
	u16 response_end_status;
	u16 tx_fes_status_prot;
	u8 tx_fes_setup;
	u8 tx_msdu_start;
	u8 tx_mpdu_start;
	u8 rxpcu_user_setup;
	u8 compaction_enable;
};

/**
 * struct hal_tx_mon_status_info - status info that wasn't populated in rx_status
 * @transmission_type: su or mu transmission type
 * @medium_prot_type: medium protection type
 * @generated_response: Generated frame in response window
 * @band_center_freq1:
 * @band_center_freq2:
 * @freq:
 * @phy_mode:
 * @schedule_id:
 * @no_bitmap_avail: Bitmap available flag
 * @explicit_ack: Explicit Acknowledge flag
 * @explicit_ack_type: Explicit Acknowledge type
 * @response_type: Response type in response window
 * @ndp_frame: NDP frame
 * @reserved: reserved bits
 * @mba_count: MBA count
 * @mba_fake_bitmap_count: MBA fake bitmap count
 * @sw_frame_group_id: software frame group ID
 * @r2r_to_follow: Response to Response follow flag
 * @phy_abort_reason: Reason for PHY abort
 * @phy_abort_user_number: User number for PHY abort
 * @protection_addr: Protection Address flag
 * @buffer: Packet buffer pointer address
 * @offset: Packet buffer offset
 * @length: Packet buffer length
 * @addr1: MAC address 1
 * @addr2: MAC address 2
 * @addr3: MAC address 3
 * @addr4: MAC address 4
 * @dp_tx_pkt_cap_cookie: cookie counter
 */
struct hal_tx_mon_status_info {
	u8  transmission_type;
	u8  medium_prot_type;
	u8  generated_response;
	u16 band_center_freq1;
	u16 band_center_freq2;
	u16 freq;
	u16 phy_mode;
	u32 schedule_id;
	u32 no_bitmap_avail :1,
	    explicit_ack : 1,
	    explicit_ack_type : 4,
	    response_type : 5,
	    ndp_frame : 2,
	    reserved : 19;
	u8  mba_count;
	u8  mba_fake_bitmap_count;
	u8  sw_frame_group_id;
	u32 r2r_to_follow;
	u16 phy_abort_reason;
	u8  phy_abort_user_number;
	u8  protection_addr;
	void *buffer;
	u32 offset;
	u32 length;
	u8  addr1[ETH_ALEN];
	u8  addr2[ETH_ALEN];
	u8  addr3[ETH_ALEN];
	u8  addr4[ETH_ALEN];
	u8  dp_tx_pkt_cap_cookie[8];
};

struct hal_mon_tx_usig_cmn {
	u32 phy_version : 3,
	    bw : 3,
	    ul_dl : 1,
	    bss_color : 6,
	    txop : 7,
	    disregard : 5,
	    validate_0 : 1,
	    reserved : 6;
};

struct hal_mon_tx_usig_tb {
	u32 ppdu_type_comp_mode : 2,
	    validate_1 : 1,
	    spatial_reuse_1 : 4,
	    spatial_reuse_2 : 4,
	    disregard_1 : 5,
	    crc : 4,
	    tail : 6,
	    rx_integrity_check_passed : 1;
};

struct hal_mon_tx_usig_mu {
	u32 ppdu_type_comp_mode : 2,
	    validate_1 : 1,
	    punc_ch_info : 5,
	    validate_2 : 1,
	    eht_sig_mcs : 2,
	    num_eht_sig_sym : 5,
	    crc : 4,
	    tail : 6,
	    rx_integrity_check_passed : 1;
};

/**
 * struct hal_mon_tx_usig_hdr: U-SIG header for EHT (and subsequent) frames
 * @usig_1: USIG common header fields
 * @usig_2: USIG version dependent fields
 * @tb: trigger based frame USIG header
 * @mu: MU frame USIG header
 */
struct hal_mon_tx_usig_hdr {
	struct hal_mon_tx_usig_cmn usig_1;
	union {
		struct hal_mon_tx_usig_tb tb;
		struct hal_mon_tx_usig_mu mu;
	} usig_2;
};

struct hal_mon_tx_eht_sig_mu_mimo_user_info {
	u32 sta_id : 11,
	    mcs : 4,
	    coding : 1,
	    spatial_coding : 6,
	    crc : 4;
};

struct hal_mon_tx_eht_sig_non_mu_mimo_user_info {
	u32 sta_id : 11,
	    mcs : 4,
	    validate : 1,
	    nss : 4,
	    beamformed : 1,
	    coding : 1,
	    crc : 4;
};

/**
 * union hal_mon_tx_eht_sig_user_field - User field in EHTSIG
 * @mu_mimo_usr: MU-MIMO user field information in EHTSIG
 * @non_mu_mimo_usr: Non MU-MIMO user field information in EHTSIG
 */
union hal_mon_tx_eht_sig_user_field {
	struct hal_mon_tx_eht_sig_mu_mimo_user_info mu_mimo_user;
	struct hal_mon_tx_eht_sig_non_mu_mimo_user_info non_mu_mimo_user;
};

/**
 * struct hal_mon_tx_user_desc_per_user - user desc per user information
 * @psdu_length: PSDU length of the user in octet
 * @ru_start_index: RU number to which user is assigned
 * @ru_size: Size of the RU for that user
 * @ofdma_mu_mimo_enabled: mu mimo transmission within the RU
 * @nss: Number of spatial stream occupied by the user
 * @stream_offset: Stream Offset from which the User occupies the Streams
 * @mcs: Modulation Coding Scheme for the User
 * @dcm: Indicates whether dual sub-carrier modulation is applied
 * @fec_type: Indicates whether it is BCC or LDPC
 * @user_bf_type: user beamforming type
 * @drop_user_cbf: frame dropped because of CBF FCS failure
 * @ldpc_extra_symbol: LDPC encoding process
 * @force_extra_symbol: force an extra OFDM symbol
 * @reserved: reserved
 * @sw_peer_id: user sw peer id
 * @per_user_subband_mask: Per user sub band mask
 */

struct hal_mon_tx_user_desc_per_user {
	u32 psdu_length;
	u32 ru_start_index         :8,
	    ru_size                :4,
	    ofdma_mu_mimo_enabled  :1,
	    nss                    :3,
	    stream_offset          :3,
	    mcs                    :4,
	    dcm                    :1,
	    fec_type               :1,
	    user_bf_type           :2,
	    drop_user_cbf          :1,
	    ldpc_extra_symbol      :1,
	    force_extra_symbol     :1,
	    reserved               :2;
	u32 sw_peer_id             :16,
	    per_user_subband_mask  :16;
};

/**
 * struct hal_mon_tx_usr_desc_common - user desc common information
 * @num_users: Number of users
 * @ltf_size: LTF size
 * @pkt_extn_pe: packet extension duration of the trigger-based PPDU
 * @a_factor: packet extension duration of the trigger-based PPDU
 * @center_ru_0: Center RU is occupied in the lower 80 MHz band
 * @center_ru_1: Center RU is occupied in the upper 80 MHz band
 * @num_ltf_symbols: number of LTF symbols
 * @doppler_indication: doppler indication
 * @reserved: reserved
 * @spatial_reuse: spatial reuse
 * @gi: guard interval
 * @ru_channel_0: RU arrangement for band 0
 * @ru_channel_1: RU arrangement for band 1
 */

struct hal_mon_tx_usr_desc_common {
	u32 num_users              :6,
	    ltf_size               :2,
	    pkt_extn_pe            :1,
	    a_factor               :2,
	    center_ru_0            :1,
	    center_ru_1            :1,
	    num_ltf_symbols        :16,
	    doppler_indication     :1,
	    reserved               :2;
	u16 spatial_reuse;
	u8  gi;
	u16 ru_channel_0[8];
	u16 ru_channel_1[8];
};

enum mon_tx_fw2sw_user_id {
	HAL_MON_TX_FW2SW_TYPE_FES_SETUP      = 0,
	HAL_MON_TX_FW2SW_TYPE_FES_SETUP_USER = 1,
	HAL_MON_TX_FW2SW_TYPE_FES_SETUP_EXT  = 2,
	HAL_MON_TX_FW2SW_TYPE_MAX            = 4
};

static inline u64 ath12k_hal_le32hilo_to_u64(__le32 hi, __le32 lo)
{
	u64 hi64 = le32_to_cpu(hi);
	u64 lo64 = le32_to_cpu(lo);

	return (hi64 << 32) | lo64;
}

static const u8
ru_alloc_offset[HAL_MAX_UL_MU_USERS][MAX_RU_INDEX] = {
	{0, 0, 0, 0, 0, 0, 0},
	{1, 0, 0, 0, 0, 0, 0},
	{2, 1, 0, 0, 0, 0, 0},
	{3, 1, 0, 0, 0, 0, 0},
	{4, 0, 0, 0, 0, 0, 0},
	{5, 2, 1, 0, 0, 0, 0},
	{6, 2, 1, 0, 0, 0, 0},
	{7, 3, 1, 0, 0, 0, 0},
	{8, 3, 1, 0, 0, 0, 0},
	{9, 4, 2, 1, 0, 0, 0},
	{10, 4, 2, 1, 0, 0, 0},
	{11, 5, 2, 1, 0, 0, 0},
	{12, 5, 2, 1, 0, 0, 0},
	{13, 0, 0, 1, 0, 0, 0},
	{14, 6, 3, 1, 0, 0, 0},
	{15, 6, 3, 1, 0, 0, 0},
	{16, 7, 3, 1, 0, 0, 0},
	{17, 7, 3, 1, 0, 0, 0},
	{18, 0, 0, 0, 0, 0, 0},
	{19, 8, 4, 2, 1, 0, 0},
	{20, 8, 4, 2, 1, 0, 0},
	{21, 9, 4, 2, 1, 0, 0},
	{22, 9, 4, 2, 1, 0, 0},
	{23, 0, 0, 2, 1, 0, 0},
	{24, 10, 5, 2, 1, 0, 0},
	{25, 10, 5, 2, 1, 0, 0},
	{26, 11, 5, 2, 1, 0, 0},
	{27, 11, 5, 2, 1, 0, 0},
	{28, 12, 6, 3, 1, 0, 0},
	{29, 12, 6, 3, 1, 0, 0},
	{30, 13, 6, 3, 1, 0, 0},
	{31, 13, 6, 3, 1, 0, 0},
	{32, 0, 0, 3, 1, 0, 0},
	{33, 14, 7, 3, 1, 0, 0},
	{34, 14, 7, 3, 1, 0, 0},
	{35, 15, 7, 3, 1, 0, 0},
	{36, 15, 7, 3, 1, 0, 0},
};

struct hal_mon_ops {
	enum hal_tx_mon_status
	(*tx_parse_status_tlv)(struct ath12k_hal *hal,
			       struct hal_tx_mon_ppdu_info *ppdu_info,
			       u16 tlv_tag,
			       const void *tlv_data,
			       u32 userid,
			       u16 tlv_len,
			       struct hal_tx_mon_status_info *status_info,
			       u8 *status_frag);
	enum hal_tx_mon_status
	(*tx_status_get_num_user)(struct ath12k_hal *hal,
				  u16 tlv_tag,
				  const void *tlv,
				  u8 *num_users,
				  u16 tlv_len);
	u32 (*get_mon_mpdu_start_wmask)(void);
	u32 (*get_mon_mpdu_end_wmask)(void);
	u32 (*get_mon_msdu_end_wmask)(void);
	u32 (*get_mon_ppdu_end_usr_stats_wmask)(void);
	void (*rx_mpdu_start_info_get)(const void *tlv_data, u32 userid,
				       struct hal_rx_mon_ppdu_info *info,
				       u32 tlv_len);
	void (*rx_msdu_end_info_get)(const void *tlv_data, u32 userid,
				     struct hal_rx_mon_ppdu_info *info,
				     u32 tlv_len);
	void (*rx_ppdu_eu_stats_info_get)(const void *tlv_data, u32 userid,
					  struct hal_rx_mon_ppdu_info *info,
					  u32 tlv_len);
	u8* (*rx_desc_get_msdu_payload)(void *desc);
	struct dp_mon_tx_ppdu_info *
	(*hal_mon_tx_ppdu_info)(struct ath12k_mon_data *pmon,
				u16 tlv_tag);
	void (*hal_mon_set_mon_buf_desc)(void *desc, u32 addr_lo,
					 u32 addr_hi, u64 cookie);
	void (*get_tx_mon_wmask_config)(struct hal_tx_mon_wmask_config *wmsk);
	void (*tx_fes_setup_info_get)(const void *tlv_data, u32 userid,
				      struct hal_tx_mon_ppdu_info *info,
				      u16 tlv_len);
	void (*tx_peer_entry_info_get)(const void *tlv_data, u32 userid,
				       struct hal_tx_mon_ppdu_info *info,
				       struct hal_tx_mon_status_info *tx_status_info,
				       u16 tlv_len);
	void (*tx_queue_ext_info_get)(const void *tlv_data, u32 userid,
				      struct hal_tx_mon_ppdu_info *info,
				      u16 tlv_len);
	void (*tx_mpdu_start_info_get)(const void *tlv_data, u32 userid,
				       struct hal_tx_mon_ppdu_info *info,
				       u16 tlv_len);
	void (*tx_fes_status_info_get)(const void *tlv_data, u32 userid,
				       struct hal_tx_mon_ppdu_info *info,
				       struct hal_tx_mon_status_info *tx_status_info,
				       u16 tlv_len);
	void (*tx_response_end_status_info_get)(const void *tlv_data, u32 userid,
						struct hal_tx_mon_ppdu_info *info,
						struct hal_tx_mon_status_info *status,
						u16 tlv_len);
	void (*tx_fes_status_prot_info_get)(const void *tlv_data, u32 userid,
					    struct hal_tx_mon_ppdu_info *info,
					    u16 tlv_len);
	void (*tx_pcu_ppdu_setup_init_info_get)(const void *tlv_data,
						struct hal_tx_mon_status_info *status,
						u16 tlv_len);
};

static inline enum hal_tx_mon_status
ath12k_hal_mon_tx_parse_status(struct ath12k_hal *hal,
			       struct hal_tx_mon_ppdu_info *ppdu_info,
			       u16 tlv_tag, const void *tlv_data,
			       u32 userid, u16 tlv_len,
			       struct hal_tx_mon_status_info *status_info,
			       u8 *status_frag)
{
	return hal->hal_mon_ops->tx_parse_status_tlv(hal, ppdu_info,
						     tlv_tag, tlv_data,
						     userid, tlv_len,
						     status_info, status_frag);
}

static inline enum hal_tx_mon_status
ath12k_hal_mon_tx_status_get_num_user(struct ath12k_hal *hal,
				      u16 tlv_tag,
				      const void *tlv,
				      u8 *num_users,
				      u16 tlv_len)
{
	return hal->hal_mon_ops->tx_status_get_num_user(hal,
							tlv_tag,
							tlv,
							num_users,
							tlv_len);
}

static inline u32 ath12k_hal_mon_rx_mpdu_start_wmask(struct ath12k_hal *hal)
{
	if (hal->hal_mon_ops->get_mon_mpdu_start_wmask)
		return hal->hal_mon_ops->get_mon_mpdu_start_wmask();

	return 0;
}

static inline u32 ath12k_hal_mon_rx_mpdu_end_wmask(struct ath12k_hal *hal)
{
	if (hal->hal_mon_ops->get_mon_mpdu_end_wmask)
		return hal->hal_mon_ops->get_mon_mpdu_end_wmask();

	return 0;
}

static inline u32 ath12k_hal_mon_rx_msdu_end_wmask(struct ath12k_hal *hal)
{
	if (hal->hal_mon_ops->get_mon_msdu_end_wmask)
		return hal->hal_mon_ops->get_mon_msdu_end_wmask();

	return 0;
}

static inline u32 ath12k_hal_mon_rx_ppdu_end_usr_stats_wmask(struct ath12k_hal *hal)
{
	if (hal->hal_mon_ops->get_mon_ppdu_end_usr_stats_wmask)
		return hal->hal_mon_ops->get_mon_ppdu_end_usr_stats_wmask();

	return 0;
}

static inline
void ath12k_hal_mon_rx_mpdu_start_info_get(struct ath12k_hal *hal,
					   const void *tlv,
					   u32 userid,
					   struct hal_rx_mon_ppdu_info *info,
					   u32 tlv_len)
{
	if (hal->hal_mon_ops->rx_mpdu_start_info_get)
		hal->hal_mon_ops->rx_mpdu_start_info_get(tlv,
							 userid,
							 info,
							 tlv_len);
}

static inline void
ath12k_hal_mon_rx_msdu_end_info_get(struct ath12k_hal *hal,
				    const void *tlv,
				    u32 userid,
				    struct hal_rx_mon_ppdu_info *info,
				    u32 tlv_len)
{
	if (hal->hal_mon_ops->rx_msdu_end_info_get)
		hal->hal_mon_ops->rx_msdu_end_info_get(tlv,
						       userid,
						       info,
						       tlv_len);
}

static inline void
ath12k_hal_mon_rx_ppdu_end_usr_stats_info_get(struct ath12k_hal *hal,
					      const void *tlv,
					      u32 userid,
					      struct hal_rx_mon_ppdu_info *info,
					      u32 tlv_len)
{
	if (hal->hal_mon_ops->rx_ppdu_eu_stats_info_get)
		hal->hal_mon_ops->rx_ppdu_eu_stats_info_get(tlv,
							    userid,
							    info,
							    tlv_len);
}

static inline u8*
ath12k_hal_mon_rx_desc_get_msdu_payload(struct ath12k_hal *hal,
					void *rx_desc)
{
	if (hal->hal_mon_ops->rx_desc_get_msdu_payload)
		return hal->hal_mon_ops->rx_desc_get_msdu_payload(rx_desc);

	return NULL;
}

static inline struct dp_mon_tx_ppdu_info *
ath12k_hal_mon_tx_ppdu_info(struct ath12k_hal *hal,
			    struct ath12k_mon_data *pmon,
			    u16 tlv_tag)
{
	return hal->hal_mon_ops->hal_mon_tx_ppdu_info(pmon, tlv_tag);
}

static inline void ath12k_hal_mon_set_mon_buf_desc(struct ath12k_hal *hal,
						   void *desc, u32 addr_lo,
						   u32 addr_hi, u64 cookie)
{
	return hal->hal_mon_ops->hal_mon_set_mon_buf_desc(desc, addr_lo,
							  addr_hi, cookie);
}

static inline void
ath12k_hal_mon_tx_fes_setup_info_get(struct ath12k_hal *hal,
				     const void *tlv,
				     u32 userid,
				     struct hal_tx_mon_ppdu_info *info,
				     u16 tlv_len)
{
	if (hal->hal_mon_ops->tx_fes_setup_info_get)
		hal->hal_mon_ops->tx_fes_setup_info_get(tlv, userid,
							info, tlv_len);
}

static inline void
ath12k_hal_mon_tx_peer_entry_info_get(struct ath12k_hal *hal,
				      const void *tlv,
				      u32 userid,
				      struct hal_tx_mon_ppdu_info *info,
				      struct hal_tx_mon_status_info *tx_status_info,
				      u16 tlv_len)
{
	if (hal->hal_mon_ops->tx_peer_entry_info_get)
		hal->hal_mon_ops->tx_peer_entry_info_get(tlv, userid,
							 info, tx_status_info,
							 tlv_len);
}

static inline void
ath12k_hal_mon_tx_queue_ext_info_get(struct ath12k_hal *hal,
				     const void *tlv,
				     u32 userid,
				     struct hal_tx_mon_ppdu_info *info,
				     u16 tlv_len)
{
	if (hal->hal_mon_ops->tx_queue_ext_info_get)
		hal->hal_mon_ops->tx_queue_ext_info_get(tlv, userid,
							info, tlv_len);
}

static inline void
ath12k_hal_mon_tx_mpdu_start_info_get(struct ath12k_hal *hal,
				      const void *tlv,
				      u32 userid,
				      struct hal_tx_mon_ppdu_info *info,
				      u16 tlv_len)
{
	if (hal->hal_mon_ops->tx_mpdu_start_info_get)
		hal->hal_mon_ops->tx_mpdu_start_info_get(tlv, userid,
							 info, tlv_len);
}

static inline void
ath12k_hal_mon_tx_fes_status_end_info_get(struct ath12k_hal *hal,
					  const void *tlv,
					  u32 userid,
					  struct hal_tx_mon_ppdu_info *info,
					  struct hal_tx_mon_status_info *tx_status_info,
					  u16 tlv_len)
{
	if (hal->hal_mon_ops->tx_fes_status_info_get)
		hal->hal_mon_ops->tx_fes_status_info_get(tlv, userid,
							 info, tx_status_info,
							 tlv_len);
}

static inline void
ath12k_hal_mon_tx_response_end_status_info_get(struct ath12k_hal *hal,
					       const void *tlv,
					       u32 userid,
					       struct hal_tx_mon_ppdu_info *info,
					       struct hal_tx_mon_status_info *status_info,
					       u16 tlv_len)
{
	if (hal->hal_mon_ops->tx_response_end_status_info_get)
		hal->hal_mon_ops->tx_response_end_status_info_get(tlv, userid,
								  info, status_info,
								  tlv_len);
}

static inline void
ath12k_hal_mon_tx_fes_status_prot_info_get(struct ath12k_hal *hal,
					   const void *tlv,
					   u32 userid,
					   struct hal_tx_mon_ppdu_info *info,
					   u16 tlv_len)
{
	if (hal->hal_mon_ops->tx_fes_status_prot_info_get)
		hal->hal_mon_ops->tx_fes_status_prot_info_get(tlv, userid,
							      info, tlv_len);
}

static inline void
ath12k_hal_mon_tx_pcu_ppdu_setup_init_info_get(struct ath12k_hal *hal,
					       const void *tlv,
					       struct hal_tx_mon_status_info *status_info,
					       u16 tlv_len)
{
	if (hal->hal_mon_ops->tx_pcu_ppdu_setup_init_info_get)
		hal->hal_mon_ops->tx_pcu_ppdu_setup_init_info_get(tlv,
								  status_info,
								  tlv_len);
}

static __always_inline void
hal_get_radiotap_he_gi_ltf(u16 *he_gi, u16 *he_ltf)
{
	switch (*he_gi) {
	case HE_GI_0_8:
		*he_gi = HE_GI_RADIOTAP_0_8;
		break;
	case HE_GI_1_6:
		*he_gi = HE_GI_RADIOTAP_1_6;
		break;
	case HE_GI_3_2:
		*he_gi = HE_GI_RADIOTAP_3_2;
		break;
	default:
		*he_gi = HE_GI_RADIOTAP_RESERVED;
	}

	switch (*he_ltf) {
	case HE_LTF_1_X:
		*he_ltf = HE_LTF_RADIOTAP_1_X;
		break;
	case HE_LTF_2_X:
		*he_ltf = HE_LTF_RADIOTAP_2_X;
		break;
	case HE_LTF_4_X:
		*he_ltf = HE_LTF_RADIOTAP_4_X;
		break;
	default:
		*he_ltf = HE_LTF_RADIOTAP_UNKNOWN;
	}
}

static inline void
ath12k_hal_mon_tx_get_wmask_config(struct ath12k_hal *hal,
				   struct hal_tx_mon_wmask_config *wmsk)
{
	if (hal->hal_mon_ops->get_tx_mon_wmask_config)
		hal->hal_mon_ops->get_tx_mon_wmask_config(wmsk);
}
#endif
