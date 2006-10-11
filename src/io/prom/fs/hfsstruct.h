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
	uint16 startBlock;
	uint16 blockCount;
} PACKED;

struct HFSMDB {
	uint16 drSigWord;
	uint32 drCrDate;
	uint32 drLsMod;
	uint16 drAtrb;
	uint16 drNmFls;
	uint16 drVBMSt;
	uint16 drAllocPtr;
	uint16 drNmAlBlks;
	uint32 drAlblkSz;
	uint32 drClpSiz;
	uint16 drAlBlSt;
	uint32 drNxtCNID;
	uint16 drFreeBks;
	char drVN[28];
	uint32 drVolBkUp;
	uint16 drVSeqNum;
	uint32 drWrCnt;
	uint32 drXTClpSiz;
	uint32 drCTClpSiz;
	uint16 drNmRtDirs;
	uint32 drFilCnt;
	uint32 drDirCnt;
	uint32 drFndrInfo[8];
	uint16 drEmbedSigWord;
	HFSExtentDescriptor drEmbedExtent;
	uint32 drXtFlSize;
	HFSExtentDescriptor drXTExtRec[3];
	uint32 drCTFlSize;
	HFSExtentDescriptor drCTExtRec[3];
} PACKED;

#if 0
struct HFSNodeDescriptor {
	uint32	ndFLink;		// forward link
	uint32	ndBLink;		// backward link
	char	ndType;		// node type
	char	ndNHeight;	// node level
	uint16	ndNRecs;		// number of records in node
	uint16	ndResv2;		// reserved
} PACKED;

enum HFSNodeType {
	ndIndxNode	= 0,
	ndHdrNode	= 1,
	ndMapNode	= 2,
	ndLeafNode	= -1
};

union HFSNode {
	HFSNodeDescriptor	desc;
	byte			raw[512];
} PACKED;

struct HFSBTHdrRec {
        uint16	bthDepth;
	uint32	bthRoot;
        uint32	bthNRecs;
	uint32	bthFNode;
        uint32	bthLNode;
	uint16	bthNodeSize;
        uint16	bthKeyLen;
	uint32	bthNNodes;
        uint32	bthFree;
	char	bthResv[76];
} PACKED;

struct HFSCatKeyRec {
	char	ckrKeyLen;
	char	ckrResrv1;
	uint32	ckrParID;
	char	ckrCName[31];
} PACKED;

enum HFSCatDataType {
	cdrDirRec	= 1,
	cdrFilRec	= 2,
	cdrThdRec	= 3,
	cdrFThdRec	= 4
};

struct HFSFXInfo {
	uint16	fdIconID;	//icon ID
	uint16	fdUnused[3];	//unused but reserved 6 bytes
	char	fdScript;	//script flag and code
	char	fdXFlags;	//reserved
	uint16	fdComment;	//comment ID
	uint32	fdPutAway;	//home directory ID
} PACKED;

struct HFSFInfo {
	char	fdType[4];	//file type
	char	fdCreator[4];	//file creator
	uint16	fdFlags;		//Finder flags
	uint16	fdLocation_v;	//file's location in window
	uint16	fdLocation_h;
	uint16	fdFldr;		//directory that contains file
} PACKED;

struct HFSCatDirRec {
	uint16	dirFlags;	//directory flags
	uint16	dirVal;		//directory valence
	uint32	dirDirID;	//directory ID
	uint32	dirCrDat;	//date and time of creation
	uint32	dirMdDat;	//date and time of last modification
	uint32	dirBkDat;	//date and time of last backup
	// dont care
//	dirUsrInfo:DInfo;//Finder information
//	dirFndrInfo:DXInfo;//additional Finder information
//	uint32	dirResrv[4];
} PACKED;

struct HFSCatFileRec {
	char		filFlags;	//file flags
	char		filTyp;		//file type
	HFSFInfo	filUsrWds;	//Finder information
	uint32		filFlNum;	//file ID
	uint16		filStBlk;	//first alloc. blk. of data fork
	uint32		filLgLen;	//logical EOF of data fork
	uint32		filPyLen;	//physical EOF of data fork
	uint16		filRStBlk;	//first alloc.blk.of resource fork
	uint32		filRLgLen;	//logical EOF of resource fork
	uint32		filRPyLen;	//physical EOF of resource fork
	uint32		filCrDat;	//date and time of creation
	uint32		filMdDat;	//date and time of last modification
	uint32		filBkDat;	//date and time of last backup
	HFSFXInfo	filFndrInfo;	//additional Finder information
	uint16		filClpSize;	//file clump size
	HFSExtentDescriptor filExtRec[3];//first data fork extent record
	HFSExtentDescriptor filRExtRec[3];//first resource fork extent record
	uint32		filResrv;	//reserved
} PACKED;

struct HFSCatDataRec {
	char	cdrType;
	char	cdrResrv2;
	// layout depends on cdrType
	union {
		HFSCatDirRec	dirRec;
		HFSCatFileRec	fileRec;
		//directory thread record
		struct {
			uint32	thdResrv[2];
			uint32	thdParID;	//parent ID for this directory
			char	thdCName[31];	//name of this directory
		} dirThreadRec;
		//file thread record
		struct {
		        uint32	fthdResrv[2];
			uint32	fthdParID;	//parent ID for this file
			char	fthdCName[31];	//name of this file
		} fileThreadRec;
	};
} PACKED;
#endif

#endif
