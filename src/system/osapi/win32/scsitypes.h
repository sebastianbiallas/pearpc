//
//
// This file was copied from bochs IA-32 emulator
// Original comment:
//
// > This file was copied from ... ?
//

//***************************************************************************
//                %%% PERIPHERAL DEVICE TYPE DEFINITIONS %%%
//***************************************************************************
#define DTYPE_DASD      0x00	// Disk Device
#define DTYPE_SEQD      0x01	// Tape Device
#define DTYPE_PRNT      0x02	// Printer
#define DTYPE_PROC      0x03	// Processor
#define DTYPE_WORM      0x04	// Write-once read-multiple
#define DTYPE_CROM      0x05	// CD-ROM device
#define DTYPE_CDROM     0x05	// CD-ROM device
#define DTYPE_SCAN      0x06	// Scanner device
#define DTYPE_OPTI      0x07	// Optical memory device
#define DTYPE_JUKE      0x08	// Medium Changer device
#define DTYPE_COMM      0x09	// Communications device
#define DTYPE_RESL      0x0A	// Reserved (low)
#define DTYPE_RESH      0x1E	// Reserved (high)
#define DTYPE_UNKNOWN   0x1F	// Unknown or no device type
