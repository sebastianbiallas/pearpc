/*
 *	PearPC
 *	hfsstruct.h
 *
 *	Copyright (C) 2004 Stefan Weyergraf
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __HFSSTRUCT_H__
#define __HFSSTRUCT_H__

#define HFSSigWord		MAGIC16("BD")

struct HFSExtentDescriptor {
	uint16 startBlock PACKED;
	uint16 blockCount PACKED;
};

struct HFSMDB {
	uint16 drSigWord PACKED;
	uint32 drCrDate PACKED;
	uint32 drLsMod PACKED;
	uint16 drAtrb PACKED;
	uint16 drNmFls PACKED;
	uint16 drVBMSt PACKED;
	uint16 drAllocPtr PACKED;
	uint16 drNmAlBlks PACKED;
	uint32 drAlblkSz PACKED;
	uint32 drClpSiz PACKED;
	uint16 drAlBlSt PACKED;
	uint32 drNxtCNID PACKED;
	uint16 drFreeBks PACKED;
	char drVN[28] PACKED;
	uint32 drVolBkUp PACKED;
	uint16 drVSeqNum PACKED;
	uint32 drWrCnt PACKED;
	uint32 drXTClpSiz PACKED;
	uint32 drCTClpSiz PACKED;
	uint16 drNmRtDirs PACKED;
	uint32 drFilCnt PACKED;
	uint32 drDirCnt PACKED;
	uint32 drFndrInfo[8] PACKED;
	uint16 drEmbedSigWord PACKED;
	HFSExtentDescriptor drEmbedExtent PACKED;
	uint32 drXtFlSize PACKED;
	HFSExtentDescriptor drXTExtRec[3] PACKED;
	uint32 drCTFlSize PACKED;
	HFSExtentDescriptor drCTExtRec[3] PACKED;
};

#if 0
struct HFSNodeDescriptor {
	uint32	ndFLink PACKED;		// forward link
	uint32	ndBLink PACKED;		// backward link
	char	ndType PACKED;		// node type
	char	ndNHeight PACKED;	// node level
	uint16	ndNRecs PACKED;		// number of records in node
	uint16	ndResv2 PACKED;		// reserved
};

enum HFSNodeType {
	ndIndxNode	= 0,
	ndHdrNode	= 1,
	ndMapNode	= 2,
	ndLeafNode	= -1
};

union HFSNode {
	HFSNodeDescriptor	desc PACKED;
	byte			raw[512] PACKED;
};

struct HFSBTHdrRec {
        uint16	bthDepth PACKED;
	uint32	bthRoot PACKED;
        uint32	bthNRecs PACKED;
	uint32	bthFNode PACKED;
        uint32	bthLNode PACKED;
	uint16	bthNodeSize PACKED;
        uint16	bthKeyLen PACKED;
	uint32	bthNNodes PACKED;
        uint32	bthFree PACKED;
	char	bthResv[76] PACKED;
};

struct HFSCatKeyRec {
	char	ckrKeyLen PACKED;
	char	ckrResrv1 PACKED;
	uint32	ckrParID PACKED;
	char	ckrCName[31] PACKED;
};

enum HFSCatDataType {
	cdrDirRec	= 1,
	cdrFilRec	= 2,
	cdrThdRec	= 3,
	cdrFThdRec	= 4
};

struct HFSFXInfo {
	uint16	fdIconID PACKED;	//icon ID
	uint16	fdUnused[3] PACKED;	//unused but reserved 6 bytes
	char	fdScript PACKED;	//script flag and code
	char	fdXFlags PACKED;	//reserved
	uint16	fdComment PACKED;	//comment ID
	uint32	fdPutAway PACKED;	//home directory ID
};      

struct HFSFInfo {
	char	fdType[4] PACKED;	//file type
	char	fdCreator[4] PACKED;	//file creator
	uint16	fdFlags PACKED;		//Finder flags
	uint16	fdLocation_v PACKED;	//file's location in window
	uint16	fdLocation_h PACKED;
	uint16	fdFldr PACKED;		//directory that contains file
};

struct HFSCatDirRec {
	uint16	dirFlags PACKED;	//directory flags
	uint16	dirVal PACKED;		//directory valence
	uint32	dirDirID PACKED;	//directory ID
	uint32	dirCrDat PACKED;	//date and time of creation
	uint32	dirMdDat PACKED;	//date and time of last modification
	uint32	dirBkDat PACKED;	//date and time of last backup
	// dont care
//	dirUsrInfo:DInfo;//Finder information
//	dirFndrInfo:DXInfo;//additional Finder information
//	uint32	dirResrv[4];
};

struct HFSCatFileRec {
	char		filFlags PACKED;	//file flags
	char		filTyp PACKED;		//file type
	HFSFInfo	filUsrWds PACKED;	//Finder information
	uint32		filFlNum PACKED;	//file ID
	uint16		filStBlk PACKED;	//first alloc. blk. of data fork
	uint32		filLgLen PACKED;	//logical EOF of data fork
	uint32		filPyLen PACKED;	//physical EOF of data fork
	uint16		filRStBlk PACKED;	//first alloc.blk.of resource fork
	uint32		filRLgLen PACKED;	//logical EOF of resource fork
	uint32		filRPyLen PACKED;	//physical EOF of resource fork
	uint32		filCrDat PACKED;	//date and time of creation
	uint32		filMdDat PACKED;	//date and time of last modification
	uint32		filBkDat PACKED;	//date and time of last backup
	HFSFXInfo	filFndrInfo PACKED;	//additional Finder information
	uint16		filClpSize PACKED;	//file clump size
	HFSExtentDescriptor filExtRec[3] PACKED;//first data fork extent record
	HFSExtentDescriptor filRExtRec[3] PACKED;//first resource fork extent record
	uint32		filResrv PACKED;	//reserved
};

struct HFSCatDataRec {
	char	cdrType PACKED;
	char	cdrResrv2 PACKED;
	// layout depends on cdrType
	union {
		HFSCatDirRec	dirRec;
		HFSCatFileRec	fileRec;
		//directory thread record
		struct {
			uint32	thdResrv[2] PACKED;
			uint32	thdParID PACKED;	//parent ID for this directory
			char	thdCName[31] PACKED;	//name of this directory
		} dirThreadRec;
		//file thread record
		struct {
		        uint32	fthdResrv[2] PACKED;
			uint32	fthdParID PACKED;	//parent ID for this file
			char	fthdCName[31] PACKED;	//name of this file
		} fileThreadRec;
	};
};
#endif

#endif
