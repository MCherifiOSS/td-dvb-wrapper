/* structs / ioctl definitions for the TD frontend driver */

typedef struct td_fe_para {
	__u32 frequency;	/* kHz */
	__u16 sync;
	__u16 symbolrate;	/* kSyms, 27500 for 27.5 MSyms */
	__u32 fec;		/* 0 auto, 1 1/2, 2 2/3, 3 3/4, 4 5/6, 5 7/8 */
	__u16 polarity;		/* 0 = H, 1 = V */
	__u16 highband;		/* 0 = 22KHz OFF, 1 = 22KHz ON */
	__u16 agc1_state;
	__s16 agc1;
	__u16 agc2;
	__u16 carrier;
	__s16 derotator;
	__u32 realfrq;		/* the "real" frequency the PLL was set to after search */
} td_fe;

typedef enum {
	DISEQC_MODE_AUTO,
	DISEQC_MODE_CMD_ONLY,
	DISEQC_MODE_BURST_ONLY,
} DISEQC_MODE;


typedef enum {
	ERR_SRC_QPSK_BIT_ERRORS,
	ERR_SRC_VITERBI_BIT_ERRORS,
	ERR_SRC_VITERBI_BYTE_ERRORS,
	ERR_SRC_PACKET_ERRORS,
} QPSK_ERR_SRC;

/* those ioctls are defined in the TD driver headers */
#define TD_FE_HARDRESET			_IO (0xed, 1)
#define TD_FE_INIT			_IO (0xed, 2)
#define TD_FE_LNB_ENABLE		_IOW(0xed, 3, unsigned int)
#define TD_FE_LNB_VERTICAL		_IOW(0xed, 4, unsigned int)
#define TD_FE_22KHZ_ENABLE		_IOW(0xed, 5, unsigned int)
#define TD_FE_LNB_SET_POLARISATION	_IOW(0xed, 6, unsigned int)
#define TD_FE_LNB_SET_HORIZONTAL	_IO (0xed, 7)
#define TD_FE_LNB_SET_VERTICAL		_IO (0xed, 8)
#define TD_FE_LNB_BOOST			_IOW(0xed, 9, unsigned int)
#define TD_FE_GET_STANDBY		_IOR(0xed, 20, unsigned int *)
#define TD_FE_SET_STANDBY		_IOW(0xed, 21, unsigned int)
#define TD_FE_GET_STATUS		_IOR(0xed, 22, unsigned int *)
#define TD_FE_SET_ERROR_SOURCE		_IOW(0xed, 23, QPSK_ERR_SRC)
#define TD_FE_GET_ERRORS		_IOR(0xed, 24, unsigned int *)
#define TD_FE_GET_MODEL			_IOR(0xed, 26, unsigned int *)
#define TD_FE_SET_FRQ			_IOW(0xed, 31, unsigned int)
#define TD_FE_SET_SYMBOL		_IOW(0xed, 32, unsigned int)
#define TD_FE_SET_FEC			_IOW(0xed, 33, unsigned int)
#define TD_FE_SET_FRANGE		_IOW(0xed, 35, unsigned int)
#define TD_FE_GET_FRANGE		_IOR(0xed, 36, unsigned int *)
#define TD_FE_SETUP_NB			_IOW(0xed, 38, struct td_fe_para)
#define TD_FE_SETUP_NBF			_IOW(0xed, 39, struct td_fe_para)
#define TD_FE_SETUP_GET			_IOR(0xed, 45, struct td_fe_para)
#define TD_FE_STOPTHAT			_IO (0xed, 49)
#define TD_FE_DISEQC_MODE		_IOW(0xed, 50, DISEQC_MODE)
#define TD_FE_DISEQC_SEND		_IOW(0xed, 51, unsigned char *)
#define TD_FE_DISEQC_SEND_BURST		_IOW(0xed, 52, unsigned int)
#define TD_FE_0_12V			_IOW(0xed, 60, unsigned int)
#define TD_FE_LOCKLED_ENABLE		_IOW(0xed, 151, unsigned int)
#define TD_FE_GET_SIGNAL		_IOR(0xed, 153, unsigned int *)
#define TD_FE_GET_SIG			_IOR(0xed, 154, unsigned int *)
#define TD_FE_READ_STATUS_C		_IOR(0xed, 169, fe_status_t)
#define TD_FE_READ_SIGNAL_STRENGTH_C	_IOR(0xed, 171, unsigned short *)
#define TD_FE_READ_SNR_C		_IOR(0xed, 172, unsigned short *)
#define TD_FE_INIT_C			_IO (0xed, 180)
#define TD_FE_LNBLOOPTHROUGH		_IOW(0xed, 190, unsigned int)

