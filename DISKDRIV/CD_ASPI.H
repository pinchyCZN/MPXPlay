#ifndef cd_aspi_h
#define cd_aspi_h

//control flags
#define	SC_GET_DEV_TYPE	   0x01	// Get information about installed SCSI devices
#define	SC_EXEC_SCSI_CMD   0x02	// Execute SCSI I/O
#define	SC_ABORT_SRB	   0x03	// Abort an outstanding I/O request
//status flags
#define SS_PENDING         0x00	// SRB being processed
#define SS_COMP            0x01	// SRB completed without error
#define SS_ABORTED         0x02	// SRB aborted
#define SS_ERR             0x04	// SRB completed with error
//device types
#define DTC_WORM	   0x04		// Write-once device
#define DTC_CDROM	   0x05		// CD-ROM device
//srb command bits
#define	SRB_POSTING		  0x01	// Enable ASPI command completion posting. See section on posting below.
#define	SRB_ENABLE_RESIDUAL_COUNT 0x04	// Enables reporting of residual byte count.This flag is only significant if the host adapter reports support for residual byte count in the SC_HA_INQUIRY command.  When data underrun occurs, the SRB_BufLen field is updated to reflect the remaining bytes to transfer.
#define	SRB_DIR_IN		  0x08	// Data transfer from SCSI target to host.
#define SRB_DIR_OUT		  0x10	// Data transfer from host to SCSI target.
#define	SRB_EVENT_NOTIFY	  0x40	// Enable ASPI command event notification. See section on event notification below.

#define CB_CDDASECTORSIZE	2352
#define CB_CDDAC2SIZE		294
#define CB_QSUBCHANNEL	0
#define CB_CDROMSECTOR	2048
#define CB_AUDIO	(CB_CDDASECTORSIZE-CB_QSUBCHANNEL)

#define CDROMDATAFLAG 0x04
#define AUDIOTRKFLAG  0x10

#define CDT_TRACK_TITLE	 0x80	// Title of Album name(ID2=00h) or Track Titles (ID2=01h...63h)
#define	CDT_PERFORMER	 0x81	// Name(s) of the performer(s) (in ASCII)

#define SENSE_LEN 14			// Maximum sense length

#pragma pack(push,1)			// byte align in aspi strutures

typedef unsigned char Ucbit;
typedef unsigned char u_char;
typedef void (*POSTPROCFUNC) ();

typedef struct srb_header_s {
	BYTE SRB_Cmd;				// ASPI command code
	BYTE SRB_Status;			// ASPI command status byte
	BYTE SRB_HaId;				// ASPI host adapter number
	BYTE SRB_Flags;				// ASPI request flags
	DWORD SRB_Hdr_Rsvd;			// Reserved, MUST = 0
} srb_header_s;

typedef struct srb_gdevblock_s {
	BYTE SRB_Cmd;				// ASPI command code = SC_GET_DEV_TYPE
	BYTE SRB_Status;			// ASPI command status byte
	BYTE SRB_HaId;				// ASPI host adapter number
	BYTE SRB_Flags;				// Reserved
	DWORD SRB_Hdr_Rsvd;			// Reserved
	BYTE SRB_Target;			// Target's SCSI ID
	BYTE SRB_Lun;				// Target's LUN number
	BYTE SRB_DeviceType;		// Target's peripheral device type
	BYTE SRB_Rsvd1;				// Reserved for alignment
} srb_gdevblock_s;

typedef struct srb_exec_scsicmd_s {
	BYTE SRB_Cmd;				// ASPI command code = SC_EXEC_SCSI_CMD
	BYTE SRB_Status;			// ASPI command status byte
	BYTE SRB_HaId;				// ASPI host adapter number
	BYTE SRB_Flags;				// ASPI request flags
	DWORD SRB_Hdr_Rsvd;			// Reserved
	BYTE SRB_Target;			// Target's SCSI ID
	BYTE SRB_Lun;				// Target's LUN number
	WORD SRB_Rsvd1;				// Reserved for Alignment
	DWORD SRB_BufLen;			// Data Allocation Length
	BYTE *SRB_BufPointer;		// Data Buffer Point
	BYTE SRB_SenseLen;			// Sense Allocation Length
	BYTE SRB_CDBLen;			// CDB Length
	BYTE SRB_HaStat;			// Host Adapter Status
	BYTE SRB_TargStat;			// Target Status
	void (*SRB_PostProc) ();	// Post routine
	void *SRB_Rsvd2;			// Reserved
	BYTE SRB_Rsvd3[16];			// Reserved for expansion
	BYTE CDBByte[16];			// SCSI CDB
	BYTE SenseArea[SENSE_LEN + 2];	// Request Sense buffer
} srb_exec_scsicmd_s;

typedef struct {
	BYTE SRB_Cmd;				// 00/000 ASPI cmd code == SC_ABORT_SRB
	BYTE SRB_Status;			// 01/001 ASPI command status byte
	BYTE SRB_HaID;				// 02/002 ASPI host adapter number
	BYTE SRB_Flags;				// 03/003 Reserved, must = 0
	DWORD SRB_Hdr_Rsvd;			// 04/004 Reserved, must = 0
	void *SRB_ToAbort;			// 08/008 Pointer to SRB to abort
} SRB_Abort;

typedef struct CDTEXTPACK_TAG {
	BYTE packType;
	BYTE trackNumber;
	BYTE sequenceNumber;

	BYTE characterPosition:4;	// character position
	BYTE block:3;				// block number 0..7
	BYTE bDBC:1;				// double byte character

	BYTE data[12];
	BYTE crc0;
	BYTE crc1;
} CDTEXTPACK;

#define	MP_P_CODE			\
	Ucbit	p_code		: 6;	\
	Ucbit	p_res		: 1;	\
	Ucbit	parsave		: 1

// CD Cap / mech status
typedef struct SCSICDMODEPAGE2A_TAG {
	MP_P_CODE;					// parsave & pagecode               (0)
	u_char p_len;				// 0x14 = 20 Bytes                  (1)

	Ucbit cd_r_read:1;			// Reads CD-R  media                (2)
	Ucbit cd_rw_read:1;			// Reads CD-RW media
	Ucbit method2:1;			// Reads fixed packet method2 media
	Ucbit dvd_rom_read:1;		// Reads DVD ROM media
	Ucbit dvd_r_read:1;			// Reads DVD-R media
	Ucbit dvd_ram_read:1;		// Reads DVD-RAM media
	Ucbit res_2_67:2;			// Reserved

	Ucbit cd_r_write:1;			// Supports writing CD-R  media     (3)
	Ucbit cd_rw_write:1;		// Supports writing CD-RW media
	Ucbit test_write:1;			// Supports emulation write
	Ucbit res_3_3:1;			// Reserved
	Ucbit dvd_r_write:1;		// Supports writing DVD-R media
	Ucbit dvd_ram_write:1;		// Supports writing DVD-RAM media
	Ucbit res_3_67:2;			// Reserved

	Ucbit audio_play:1;			// Supports Audio play operation    (4)
	Ucbit composite:1;			// Deliveres composite A/V stream
	Ucbit digital_port_2:1;		// Supports digital output on port 2
	Ucbit digital_port_1:1;		// Supports digital output on port 1
	Ucbit mode_2_form_1:1;		// Reads Mode-2 form 1 media (XA)
	Ucbit mode_2_form_2:1;		// Reads Mode-2 form 2 media
	Ucbit multi_session:1;		// Reads multi-session media
	Ucbit res_4:1;				// Reserved

	Ucbit cd_da_supported:1;	// Reads audio data with READ CD cmd (5)
	Ucbit cd_da_accurate:1;		// READ CD data stream is accurate
	Ucbit rw_supported:1;		// Reads R-W sub channel information
	Ucbit rw_deint_cor:1;		// Reads de-interleved R-W sub chan
	Ucbit c2_pointers:1;		// Supports C2 error pointers
	Ucbit ISRC:1;				// Reads ISRC information
	Ucbit UPC:1;				// Reads media catalog number (UPC)
	Ucbit read_bar_code:1;		// Supports reading bar codes

	Ucbit lock:1;				// PREVENT/ALLOW may lock media      (6)
	Ucbit lock_state:1;			// Lock state 0=unlocked 1=locked
	Ucbit prevent_jumper:1;		// State of prev/allow jumper 0=pres
	Ucbit eject:1;				// Ejects disc/cartr with STOP LoEj
	Ucbit res_6_4:1;			// Reserved
	Ucbit loading_type:3;		// Loading mechanism type

	Ucbit sep_chan_vol:1;		// Vol controls each channel separat (7)
	Ucbit sep_chan_mute:1;		// Mute controls each channel separat
	Ucbit disk_present_rep:1;	// Changer supports disk present rep
	Ucbit sw_slot_sel:1;		// Load empty slot in changer
	Ucbit res_7:4;				// Reserved

	BYTE max_read_speed[2];		// Max. read speed in KB/s              (8)

	u_char num_vol_levels[2];	// # of supported volume levels         (10)

	u_char buffer_size[2];		// Buffer size for the data in KB       (12)
	u_char cur_read_speed[2];	// Current read speed in KB/s           (14)
	u_char res_16;				// Reserved                             (16)

	Ucbit res_17_0:1;			// Reserved                             (17)
	Ucbit BCK:1;				// Data valid on falling edge of BCK
	Ucbit RCK:1;				// Set: HIGH high LRCK=left channel
	Ucbit LSBF:1;				// Set: LSB first Clear: MSB first
	Ucbit length:2;				// 0=32BCKs 1=16BCKs 2=24BCKs 3=24I2c
	Ucbit res_17:2;				// Reserved

	u_char max_write_speed[2];	// Max. write speed supported in KB/s   (18)

	u_char cur_write_speed[2];	// Current write speed in KB/s          (20)

} SCSICDMODEPAGE2A;

typedef struct CDSTATUSINFO_TAG {
	BYTE sk;
	BYTE asc;
	BYTE ascq;
	BYTE ha_stat;
	BYTE target_stat;
} CDSTATUSINFO;

enum CDMEDIASTATUS {
	CDASPI_CDMEDIA_PRESENT = 0,
	CDASPI_CDMEDIA_NOT_PRESENT,
	CDASPI_CDMEDIA_NOT_PRESENT_TRAY_OPEN,
	CDASPI_CDMEDIA_NOT_PRESENT_TRAY_CLOSED,
};

enum DRIVETYPE {
	CDASPI_DRIVETYPE_GENERIC = 0,
	CDASPI_DRIVETYPE_TOSHIBA,
	CDASPI_DRIVETYPE_TOSHIBANEW,
	CDASPI_DRIVETYPE_IBM,
	CDASPI_DRIVETYPE_NEC,
	CDASPI_DRIVETYPE_DEC,
	CDASPI_DRIVETYPE_IMS,
	CDASPI_DRIVETYPE_KODAK,
	CDASPI_DRIVETYPE_RICOH,
	CDASPI_DRIVETYPE_HP,
	CDASPI_DRIVETYPE_PHILIPS,
	CDASPI_DRIVETYPE_PLASMON,
	CDASPI_DRIVETYPE_GRUNDIGCDR100IPW,
	CDASPI_DRIVETYPE_MITSUMICDR,
	CDASPI_DRIVETYPE_PLEXTOR,
	CDASPI_DRIVETYPE_SONY,
	CDASPI_DRIVETYPE_YAMAHA,
	CDASPI_DRIVETYPE_NRC,
	CDASPI_DRIVETYPE_IMSCDD5,
	CDASPI_DRIVETYPE_CUSTOMDRIVE,
	CDASPI_NUMDRIVETYPES
};

enum CDASPI_READMODE {
	CDASPI_READMODE_MMC = 0,
	CDASPI_READMODE_10,
	CDASPI_READMODE_NEC,
	CDASPI_READMODE_SONY,
	CDASPI_READMODE_MMC2,
	CDASPI_READMODE_MMC3,
	CDASPI_READMODE_C1,
	CDASPI_READMODE_C2,
	CDASPI_READMODE_C3,
	CDASPI_READMODE_MMC4,
	CDASPI_NUMREADMODES
};

enum CDASPI_SETSPEEDMODE {
	CDASPI_SPEEDMODE_NONE = 0,
	CDASPI_SPEEDMODE_MMC,
	CDASPI_SPEEDMODE_SONY,
	CDASPI_SPEEDMODE_YAMAHA,
	CDASPI_SPEEDMODE_TOSHIBA,
	CDASPI_SPEEDMODE_PHILIPS,
	CDASPI_SPEEDMODE_NEC,
	CDASPI_NUMSPEEDMODES
};

enum CDASPI_ENDIANMODE {
	CDASPI_ENDIANMODE_BIGENDIAN = 0,
	CDASPI_ENDIANMODE_LITTLEENDIAN,
	CDASPI_NUMENDIANMODES
};

enum CDASPI_ENABLEMODE {
	CDASPI_ENABLEMODE_NONE = 0,
	CDASPI_ENABLEMODE_STD,
	CDASPI_NUMENABLEMODES
};

//------------------------------------------------------------------------
//ntscsi
#define  SCSI_IOCTL_DATA_OUT          0
#define  SCSI_IOCTL_DATA_IN           1
#define  SCSI_IOCTL_DATA_UNSPECIFIED  2

#define SCSI_CMD_INQUIRY (0x12)

#define  METHOD_BUFFERED     0L
#define  METHOD_IN_DIRECT    1L
#define  METHOD_OUT_DIRECT   2L
#define  METHOD_NEITHER      3L
#define  FILE_ANY_ACCESS     0x0000L
#define  FILE_READ_ACCESS    0x0001L
#define  FILE_WRITE_ACCESS   0x0002L

#define  IOCTL_SCSI_BASE    0x00000004

#ifndef CTL_CODE
#define CTL_CODE( DevType, Function, Method, Access ) (                 \
    ((DevType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
)
#endif

#define IOCTL_SCSI_PASS_THROUGH         CTL_CODE( IOCTL_SCSI_BASE, 0x0401, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS )
#define IOCTL_SCSI_MINIPORT             CTL_CODE( IOCTL_SCSI_BASE, 0x0402, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS )
#define IOCTL_SCSI_GET_INQUIRY_DATA     CTL_CODE( IOCTL_SCSI_BASE, 0x0403, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SCSI_GET_CAPABILITIES     CTL_CODE( IOCTL_SCSI_BASE, 0x0404, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SCSI_PASS_THROUGH_DIRECT  CTL_CODE( IOCTL_SCSI_BASE, 0x0405, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS )
#define IOCTL_SCSI_GET_ADDRESS          CTL_CODE( IOCTL_SCSI_BASE, 0x0406, METHOD_BUFFERED, FILE_ANY_ACCESS )

#pragma pack(pop)

typedef struct {
	ULONG Length;
	UCHAR PortNumber;
	UCHAR PathId;
	UCHAR TargetId;
	UCHAR Lun;
} SCSI_ADDRESS, *PSCSI_ADDRESS;

typedef struct {
	USHORT Length;
	UCHAR ScsiStatus;
	UCHAR PathId;
	UCHAR TargetId;
	UCHAR Lun;
	UCHAR CdbLength;
	UCHAR SenseInfoLength;
	UCHAR DataIn;
	ULONG DataTransferLength;
	ULONG TimeOutValue;
	PVOID DataBuffer;
	ULONG SenseInfoOffset;
	UCHAR Cdb[16];
} SCSI_PASS_THROUGH_DIRECT, *PSCSI_PASS_THROUGH_DIRECT;

typedef struct {
	SCSI_PASS_THROUGH_DIRECT spt;
	UCHAR ucSenseBuf[32];
} SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, *PSCSI_PASS_THROUGH_DIRECT_WITH_BUFFER;

#endif							// cd_aspi_h
