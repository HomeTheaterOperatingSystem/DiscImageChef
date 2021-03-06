/****************************************************************************
The Disc Image Chef
-----------------------------------------------------------------------------

Filename       : macos.c
Author(s)      : Natalia Portillo

Component      : fstester.setter

--[ Description ] -----------------------------------------------------------

Contains Mac OS code.

--[ License ] ---------------------------------------------------------------
     This program is free software: you can redistribute it and/or modify
     it under the terms of the GNU General Public License as
     published by the Free Software Foundation, either version 3 of the
     License, or (at your option) any later version.

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warraty of
     MERCHANTIBILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
     GNU General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with this program.  If not, see <http://www.gnu.org/licenses/>.

-----------------------------------------------------------------------------
Copyright (C) 2011-2018 Natalia Portillo
*****************************************************************************/

#if defined(macintosh)

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <Gestalt.h>
#include <Files.h>
#include <TextUtils.h>
#include <MacTypes.h>
#include <Aliases.h>
#include <Resources.h>
#include <FileTypesAndCreators.h>

#include "consts.h"
#include "defs.h"
#include "macos.h"

void GetOsInfo()
{
    int32_t gestaltResponse;
    OSErr   rc;

    printf("OS information:\n");

    rc = Gestalt(gestaltAUXVersion, &gestaltResponse);
    if(!rc)
    {
        printf("Running under A/UX version 0x%08X\n", gestaltResponse);
    }
    else
    {
        rc = Gestalt(gestaltSystemVersion, &gestaltResponse);
        if(rc)
        {
            printf("Could not get Mac OS version.\n");
        }
        else
        {
            printf("Running under Mac OS version %d.%d.%d", (gestaltResponse & 0xF00) >> 8,
                   (gestaltResponse & 0xF0) >> 4, gestaltResponse & 0xF);
            rc = Gestalt(gestaltSysArchitecture, &gestaltResponse);
            if(!rc)
            {
                printf(" for ");
                switch(gestaltResponse)
                {
                    case 1:
                        printf("Motorola 68k architecture.");
                        break;
                    case 2:
                        printf("PowerPC architecture.");
                        break;
                    case 3:
                        printf("x86 architecture.");
                        break;
                    default:
                        printf("unknown architecture code %d.", gestaltResponse);
                        break;
                }
            }
            printf("\n");
        }
        rc = Gestalt(gestaltMacOSCompatibilityBoxAttr, &gestaltResponse);
        if(!rc)
        {
            printf("Running under Classic.\n");
        }
    }
}

void GetVolumeInfo(const char *path, size_t *clusterSize)
{
    OSErr        rc;
    Str255       str255;
    HVolumeParam hpb;
    XVolumeParam xpb;
    int32_t      gestaltResponse;
    int16_t      drvInfo;
    int16_t      refNum;
    uint64_t     totalBlocks;
    uint64_t     freeBlocks;
    uint32_t     crDate;
    uint32_t     lwDate;
    uint32_t     bkDate;
    uint16_t     fsId;
    uint64_t     totalBytes;
    uint64_t     freeBytes;
    int          hfsPlusApis = 0;
    int          bigVol      = 0;
    *clusterSize = 0;

    snprintf((char *)str255, 255, "%s", path);

    rc = Gestalt(gestaltFSAttr, &gestaltResponse);
    if(!rc)
    {
        hfsPlusApis = gestaltResponse & (1 << gestaltHasHFSPlusAPIs);
        bigVol      = gestaltResponse & (1 << gestaltFSSupports2TBVols);
    }

    if(!bigVol)
    {
        hpb.ioNamePtr  = str255;
        hpb.ioVRefNum  = 0;
        hpb.ioVolIndex = -1;
        rc = PBHGetVInfo((HParmBlkPtr) & hpb, 0);
        if(rc)
        {
            printf("Could not get volume information.\n");
            return;
        }
        drvInfo     = hpb.ioVDrvInfo;
        refNum      = hpb.ioVRefNum;
        totalBlocks = hpb.ioVNmAlBlks;
        freeBlocks  = hpb.ioVFrBlk;
        crDate      = hpb.ioVCrDate;
        lwDate      = hpb.ioVLsMod;
        bkDate      = hpb.ioVBkUp;
        fsId        = hpb.ioVSigWord;
        *clusterSize = hpb.ioVAlBlkSiz;
        totalBytes = totalBlocks * *clusterSize;
        freeBytes  = freeBlocks * *clusterSize;
        if(hpb.ioVFSID != 0)
        {
            fsId = hpb.ioVFSID;
        }
    }
    else
    {
        xpb.ioNamePtr  = str255;
        xpb.ioVRefNum  = 0;
        xpb.ioVolIndex = -1;
        rc = PBXGetVolInfo((XVolumeParamPtr) & xpb, 0);
        if(rc)
        {
            printf("Could not get volume information.\n");
            return;
        }
        drvInfo     = xpb.ioVDrvInfo;
        refNum      = xpb.ioVRefNum;
        totalBlocks = xpb.ioVTotalBytes / xpb.ioVAlBlkSiz;
        freeBlocks  = xpb.ioVFreeBytes / xpb.ioVAlBlkSiz;
        crDate      = xpb.ioVCrDate;
        lwDate      = xpb.ioVLsMod;
        bkDate      = xpb.ioVBkUp;
        fsId        = xpb.ioVSigWord;
        *clusterSize = xpb.ioVAlBlkSiz;
        totalBytes = xpb.ioVTotalBytes;
        freeBytes  = xpb.ioVFreeBytes;
        if(xpb.ioVFSID != 0)
        {
            fsId = xpb.ioVFSID;
        }
    }

    printf("Volume information:\n");
    printf("\tPath: %s\n", path);
    if(bigVol)
    {
        printf("\tVolume supports up to 2Tb disks.\n");
    }
    if(hfsPlusApis)
    {
        printf("\tVolume supports HFS Plus APIs.\n");
    }
    printf("\tDrive number: %d\n", drvInfo);
    printf("\tVolume number: %d\n", refNum);
    printf("\tVolume name: %#s\n", str255);
    printf("\t%llu allocation blocks in volume, %llu free\n", totalBlocks, freeBlocks);
    printf("\t%llu bytes in volume, %llu free\n", totalBytes, freeBytes);
    printf("\t%d bytes per allocation block.\n", *clusterSize);
    printf("\tVolume created on 0x%08X\n", crDate);
    printf("\tVolume last written on 0x%08X\n", lwDate);
    printf("\tVolume last backed up on 0x%08X\n", bkDate);
    printf("\tFilesystem type: ");
    switch(fsId)
    {
        case 0xD2D7:
            printf("MFS\n");
            break;
        case 0x4244:
            printf("HFS\n");
            break;
        case 0x482B:
            printf("HFS Plus\n");
            break;
        case 0x4147:
            printf("ISO9660\n");
            break;
        case 0x55DF:
            printf("UDF\n");
            break;
        case 0x4242:
            printf("High Sierra\n");
            break;
        case 0x4A48:
            printf("Audio CD\n");
            break;
        case 0x0100:
            printf("ProDOS\n");
            break;
        case 0x4953:
            printf("FAT\n");
            break;
        default:
            printf("unknown id 0x%04X\n", fsId);
            break;
    }
}

void FileAttributes(const char *path)
{
    OSErr        rc, wRc, cRc;
    Str255       str255;
    HVolumeParam hpb;
    int16_t      refNum;
    int16_t      refFile;
    int32_t      dirId;
    FInfo        finderInfo;
    int32_t      count;
    HFileInfo    *fpb;
    CInfoPBRec   cipbr;

    snprintf((char *)str255, 255, "%s", path);
    hpb.ioNamePtr  = str255;
    hpb.ioVRefNum  = 0;
    hpb.ioVolIndex = -1;
    rc = PBHGetVInfo((HParmBlkPtr) & hpb, 0);
    if(rc)
    {
        printf("Could not get volume information.\n");
        return;
    }
    refNum = hpb.ioVRefNum;

    rc = DirCreate(refNum, fsRtDirID, (unsigned char *)"\pATTRS", &dirId);
    if(rc)
    {
        printf("Error %d creating working directory.\n", rc);
        return;
    }

    printf("Creating attribute files.\n");

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pNONE", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pNONE", 0, &refFile);
        if(!rc)
        {
            count = strlen(noAttributeText);
            wRc   = FSWrite(refFile, &count, noAttributeText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            rc = HSetFInfo(refNum, dirId, "\pNONE", &finderInfo);
        }
    }
    printf("\tFile with no attributes: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "NONE", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pINDESK", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pINDESK", 0, &refFile);
        if(!rc)
        {
            count = strlen(desktopText);
            wRc   = FSWrite(refFile, &count, desktopText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            finderInfo.fdFlags   = kIsOnDesk;
            rc = HSetFInfo(refNum, dirId, "\pINDESK", &finderInfo);
        }
    }
    printf("\tFile is in desktop: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "INDESK", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pBROWN", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pBROWN", 0, &refFile);
        if(!rc)
        {
            count = strlen(color2Text);
            wRc   = FSWrite(refFile, &count, color2Text);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            finderInfo.fdFlags   = 0x02;
            rc = HSetFInfo(refNum, dirId, "\pBROWN", &finderInfo);
        }
    }
    printf("\tFile is colored brown: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "BROWN", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pGREEN", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pGREEN", 0, &refFile);
        if(!rc)
        {
            count = strlen(color4Text);
            wRc   = FSWrite(refFile, &count, color4Text);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            finderInfo.fdFlags   = 0x04;
            rc = HSetFInfo(refNum, dirId, "\pGREEN", &finderInfo);
        }
    }
    printf("\tFile is colored green: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "GREEN", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pLILAC", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pLILAC", 0, &refFile);
        if(!rc)
        {
            count = strlen(color6Text);
            wRc   = FSWrite(refFile, &count, color6Text);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            finderInfo.fdFlags   = 0x06;
            rc = HSetFInfo(refNum, dirId, "\pLILAC", &finderInfo);
        }
    }
    printf("\tFile is colored lilac: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "LILAC", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pBLUE", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pBLUE", 0, &refFile);
        if(!rc)
        {
            count = strlen(color8Text);
            wRc   = FSWrite(refFile, &count, color8Text);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            finderInfo.fdFlags   = 0x08;
            rc = HSetFInfo(refNum, dirId, "\pBLUE", &finderInfo);
        }
    }
    printf("\tFile is colored blue: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "BLUE", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pMAGENTA", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pMAGENTA", 0, &refFile);
        if(!rc)
        {
            count = strlen(colorAText);
            wRc   = FSWrite(refFile, &count, colorAText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            finderInfo.fdFlags   = 0x0A;
            rc = HSetFInfo(refNum, dirId, "\pMAGENTA", &finderInfo);
        }
    }
    printf("\tFile is colored magenta: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "MAGENTA", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pRED", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pRED", 0, &refFile);
        if(!rc)
        {
            count = strlen(colorCText);
            wRc   = FSWrite(refFile, &count, colorCText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            finderInfo.fdFlags   = 0x0C;
            rc = HSetFInfo(refNum, dirId, "\pRED", &finderInfo);
        }
    }
    printf("\tFile is colored red: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "RED", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pORANGE", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pORANGE", 0, &refFile);
        if(!rc)
        {
            count = strlen(colorEText);
            wRc   = FSWrite(refFile, &count, colorEText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            finderInfo.fdFlags   = 0x0E;
            rc = HSetFInfo(refNum, dirId, "\pORANGE", &finderInfo);
        }
    }
    printf("\tFile is colored orange: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "ORANGE", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pSWITCH", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pSWITCH", 0, &refFile);
        if(!rc)
        {
            count = strlen(requireSwitchText);
            wRc   = FSWrite(refFile, &count, requireSwitchText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            finderInfo.fdFlags   = kRequireSwitchLaunch;
            rc = HSetFInfo(refNum, dirId, "\pSWITCH", &finderInfo);
        }
    }
    printf("\tFile require switch launch: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "SWITCH", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pSHARED", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pSHARED", 0, &refFile);
        if(!rc)
        {
            count = strlen(sharedText);
            wRc   = FSWrite(refFile, &count, sharedText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            finderInfo.fdFlags   = kIsShared;
            rc = HSetFInfo(refNum, dirId, "\pSHARED", &finderInfo);
        }
    }
    printf("\tFile is shared: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "SHARED", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pNOINIT", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pNOINIT", 0, &refFile);
        if(!rc)
        {
            count = strlen(noInitText);
            wRc   = FSWrite(refFile, &count, noInitText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            finderInfo.fdFlags   = kHasNoINITs;
            rc = HSetFInfo(refNum, dirId, "\pNOINIT", &finderInfo);
        }
    }
    printf("\tFile has no INITs: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "NOINIT", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pINITED", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pINITED", 0, &refFile);
        if(!rc)
        {
            count = strlen(initedText);
            wRc   = FSWrite(refFile, &count, initedText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            finderInfo.fdFlags   = kHasBeenInited;
            rc = HSetFInfo(refNum, dirId, "\pINITED", &finderInfo);
        }
    }
    printf("\tFile has been INITed: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "INITED", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pAOCE", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pAOCE", 0, &refFile);
        if(!rc)
        {
            count = strlen(aoceText);
            wRc   = FSWrite(refFile, &count, aoceText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            finderInfo.fdFlags   = kAOCE;
            rc = HSetFInfo(refNum, dirId, "\pAOCE", &finderInfo);
        }
    }
    printf("\tFile with AOCE set: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "AOCE", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pICON", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pICON", 0, &refFile);
        if(!rc)
        {
            count = strlen(customIconText);
            wRc   = FSWrite(refFile, &count, customIconText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            finderInfo.fdFlags   = kHasCustomIcon;
            rc = HSetFInfo(refNum, dirId, "\pICON", &finderInfo);
        }
    }
    printf("\tFile has custom icon (not really): name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "ICON", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pSTATIONERY", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pSTATIONERY", 0, &refFile);
        if(!rc)
        {
            count = strlen(stationeryText);
            wRc   = FSWrite(refFile, &count, stationeryText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            finderInfo.fdFlags   = kIsStationery;
            rc = HSetFInfo(refNum, dirId, "\pSTATIONERY", &finderInfo);
        }
    }
    printf("\tFile is stationery: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "STATIONERY", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pLOCKED", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pLOCKED", 0, &refFile);
        if(!rc)
        {
            count = strlen(nameLockText);
            wRc   = FSWrite(refFile, &count, nameLockText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            finderInfo.fdFlags   = kNameLocked;
            rc = HSetFInfo(refNum, dirId, "\pLOCKED", &finderInfo);
        }
    }
    printf("\tFile is locked: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "LOCKED", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pBUNDLE", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pBUNDLE", 0, &refFile);
        if(!rc)
        {
            count = strlen(bundleText);
            wRc   = FSWrite(refFile, &count, bundleText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            finderInfo.fdFlags   = kHasBundle;
            rc = HSetFInfo(refNum, dirId, "\pBUNDLE", &finderInfo);
        }
    }
    printf("\tFile has bundle (not really): name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "BUNDLE", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pINVISIBLE", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pINVISIBLE", 0, &refFile);
        if(!rc)
        {
            count = strlen(invisibleText);
            wRc   = FSWrite(refFile, &count, invisibleText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            finderInfo.fdFlags   = kIsInvisible;
            rc = HSetFInfo(refNum, dirId, "\pINVISIBLE", &finderInfo);
        }
    }
    printf("\tFile is invisible: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "INVISIBLE", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pALIAS", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pALIAS", 0, &refFile);
        if(!rc)
        {
            count = strlen(aliasText);
            wRc   = FSWrite(refFile, &count, aliasText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            finderInfo.fdFlags   = kIsAlias;
            rc = HSetFInfo(refNum, dirId, "\pALIAS", &finderInfo);
        }
    }
    printf("\tFile is alias to nowhere: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "ALIAS", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pSIMPLE", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pSIMPLE", 0, &refFile);
        if(!rc)
        {
            count = strlen(simpletextText);
            wRc   = FSWrite(refFile, &count, simpletextText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostSimpleText;
            rc = HSetFInfo(refNum, dirId, "\pSIMPLE", &finderInfo);
        }
    }
    printf("\tFile with creator SimpleText: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "SIMPLE", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pDIC", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pDIC", 0, &refFile);
        if(!rc)
        {
            count = strlen(dicText);
            wRc   = FSWrite(refFile, &count, dicText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostDiscImageChef;
            rc = HSetFInfo(refNum, dirId, "\pDIC", &finderInfo);
        }
    }
    printf("\tFile with creator 'dic ': name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "DIC", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pPOS_M32_M32", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pPOS_M32_M32", 0, &refFile);
        if(!rc)
        {
            count = strlen(pos_m32_m32);
            wRc   = FSWrite(refFile, &count, pos_m32_m32);
            cRc   = FSClose(refFile);

            finderInfo.fdType       = ftGenericDocumentPC;
            finderInfo.fdCreator    = ostUnknown;
            finderInfo.fdLocation.h = -32768;
            finderInfo.fdLocation.v = -32768;
            rc = HSetFInfo(refNum, dirId, "\pPOS_M32_M32", &finderInfo);
        }
    }
    printf("\tFile with position -32768,-32768: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "POS_M32_M32", rc, wRc,
           cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pPOS_32_32", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pPOS_32_32", 0, &refFile);
        if(!rc)
        {
            count = strlen(pos_32_32);
            wRc   = FSWrite(refFile, &count, pos_32_32);
            cRc   = FSClose(refFile);
            finderInfo.fdType       = ftGenericDocumentPC;
            finderInfo.fdCreator    = ostUnknown;
            finderInfo.fdLocation.h = 32767;
            finderInfo.fdLocation.v = 32767;
            rc = HSetFInfo(refNum, dirId, "\pPOS_32_32", &finderInfo);
        }
    }
    printf("\tFile with position 32767,32767: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "POS_32_32", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pPOS_M1_M1", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pPOS_M1_M1", 0, &refFile);
        if(!rc)
        {
            count = strlen(pos_m1_m1);
            wRc   = FSWrite(refFile, &count, pos_m1_m1);
            cRc   = FSClose(refFile);
            finderInfo.fdType       = ftGenericDocumentPC;
            finderInfo.fdCreator    = ostUnknown;
            finderInfo.fdLocation.h = -1024;
            finderInfo.fdLocation.v = -1024;
            rc = HSetFInfo(refNum, dirId, "\pPOS_M1_M1", &finderInfo);
        }
    }
    printf("\tFile with position -1024,-1024: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "POS_M1_M1", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pPOS_M1_M32", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pPOS_M1_M32", 0, &refFile);
        if(!rc)
        {
            count = strlen(pos_m1_m32);
            wRc   = FSWrite(refFile, &count, pos_m1_m32);
            cRc   = FSClose(refFile);
            finderInfo.fdType       = ftGenericDocumentPC;
            finderInfo.fdCreator    = ostUnknown;
            finderInfo.fdLocation.h = -1024;
            finderInfo.fdLocation.v = -32768;
            rc = HSetFInfo(refNum, dirId, "\pPOS_M1_M32", &finderInfo);
        }
    }
    printf("\tFile with position -1024,-32768: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "POS_M1_M32", rc, wRc,
           cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pPOS_M1_32", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pPOS_M1_32", 0, &refFile);
        if(!rc)
        {
            count = strlen(pos_m1_32);
            wRc   = FSWrite(refFile, &count, pos_m1_32);
            cRc   = FSClose(refFile);
            finderInfo.fdType       = ftGenericDocumentPC;
            finderInfo.fdCreator    = ostUnknown;
            finderInfo.fdLocation.h = -1024;
            finderInfo.fdLocation.v = 32767;
            rc = HSetFInfo(refNum, dirId, "\pPOS_M1_32", &finderInfo);
        }
    }
    printf("\tFile with position -1024,32767: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "POS_M1_32", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pPOS_M1_1", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pPOS_M1_1", 0, &refFile);
        if(!rc)
        {
            count = strlen(pos_m1_1);
            wRc   = FSWrite(refFile, &count, pos_m1_1);
            cRc   = FSClose(refFile);
            finderInfo.fdType       = ftGenericDocumentPC;
            finderInfo.fdCreator    = ostUnknown;
            finderInfo.fdLocation.h = -1024;
            finderInfo.fdLocation.v = 1024;
            rc = HSetFInfo(refNum, dirId, "\pPOS_M1_1", &finderInfo);
        }
    }
    printf("\tFile with position -1024,1024: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "POS_M1_1", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pPOS_1_M1", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pPOS_1_M1", 0, &refFile);
        if(!rc)
        {
            count = strlen(pos_1_m1);
            wRc   = FSWrite(refFile, &count, pos_1_m1);
            cRc   = FSClose(refFile);
            finderInfo.fdType       = ftGenericDocumentPC;
            finderInfo.fdCreator    = ostUnknown;
            finderInfo.fdLocation.h = 1024;
            finderInfo.fdLocation.v = -1024;
            rc = HSetFInfo(refNum, dirId, "\pPOS_1_M1", &finderInfo);
        }
    }
    printf("\tFile with position 1024,-1024: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "POS_1_M1", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pPOS_1_M32", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pPOS_1_M32", 0, &refFile);
        if(!rc)
        {
            count = strlen(pos_1_m32);
            wRc   = FSWrite(refFile, &count, pos_1_m32);
            cRc   = FSClose(refFile);
            finderInfo.fdType       = ftGenericDocumentPC;
            finderInfo.fdCreator    = ostUnknown;
            finderInfo.fdLocation.h = 1024;
            finderInfo.fdLocation.v = -32768;
            rc = HSetFInfo(refNum, dirId, "\pPOS_1_M32", &finderInfo);
        }
    }
    printf("\tFile with position 1024,-32768: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "POS_1_M32", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pPOS_1_32", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pPOS_1_32", 0, &refFile);
        if(!rc)
        {
            count = strlen(pos_1_32);
            wRc   = FSWrite(refFile, &count, pos_1_32);
            cRc   = FSClose(refFile);
            finderInfo.fdType       = ftGenericDocumentPC;
            finderInfo.fdCreator    = ostUnknown;
            finderInfo.fdLocation.h = 1024;
            finderInfo.fdLocation.v = 32767;
            rc = HSetFInfo(refNum, dirId, "\pPOS_1_32", &finderInfo);
        }
    }
    printf("\tFile with position 1024,32767: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "POS_1_32", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pPOS_1_1", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pPOS_1_1", 0, &refFile);
        if(!rc)
        {
            count = strlen(pos_1_1);
            wRc   = FSWrite(refFile, &count, pos_1_1);
            cRc   = FSClose(refFile);
            finderInfo.fdType       = ftGenericDocumentPC;
            finderInfo.fdCreator    = ostUnknown;
            finderInfo.fdLocation.h = 1024;
            finderInfo.fdLocation.v = 1024;
            rc = HSetFInfo(refNum, dirId, "\pPOS_1_1", &finderInfo);
        }
    }
    printf("\tFile with position 1024,1024: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "POS_1_1", rc, wRc, cRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pALL", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pALL", 0, &refFile);
        if(!rc)
        {
            count = strlen(allText);
            wRc   = FSWrite(refFile, &count, allText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostUnknown;
            finderInfo.fdFlags   = 0xFFFF;
            rc = HSetFInfo(refNum, dirId, "\pALL", &finderInfo);
        }
    }
    printf("\tFile has all flags bits set: name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", "ALL", rc, wRc, cRc);
}

void FilePermissions(const char *path)
{
    /* Do nothing, not supported by target operating system */
}

void ExtendedAttributes(const char *path)
{
    /* Do nothing, not supported by target operating system */
}

static OSErr
SaveResourceToNewFile(int16_t vRefNum, int32_t dirID, Str255 filename, ResType type, int16_t resId, Str255 resName,
                      unsigned char *buffer, size_t length)
{
    Handle  h;
    OSErr   rc;
    int16_t resRef;

    h = NewHandle(length);

    if(!h)
        return notEnoughMemoryErr;

    memcpy(*h, buffer, length);

    HCreateResFile(vRefNum, dirID, filename);

    resRef = HOpenResFile(vRefNum, dirID, filename, fsCurPerm);
    rc     = ResError();

    if(resRef == -1 || rc)
        return rc;

    UseResFile(resRef);

    AddResource(h, type, resId, resName);
    rc = ResError();

    CloseResFile(resRef);

    DisposeHandle(h);

    return rc;
}

void ResourceFork(const char *path)
{
    OSErr        rc, wRc, cRc, rRc, rRc2, rRc3;
    Str255       str255;
    HVolumeParam hpb;
    int16_t      refNum;
    int16_t      refFile;
    int32_t      dirId;
    FInfo        finderInfo;
    int32_t      count;
    HFileInfo    *fpb;
    CInfoPBRec   cipbr;

    snprintf((char *)str255, 255, "%s", path);
    hpb.ioNamePtr  = str255;
    hpb.ioVRefNum  = 0;
    hpb.ioVolIndex = -1;
    rc = PBHGetVInfo((HParmBlkPtr) & hpb, 0);
    if(rc)
    {
        printf("Could not get volume information.\n");
        return;
    }
    refNum = hpb.ioVRefNum;

    rc = DirCreate(refNum, fsRtDirID, (unsigned char *)"\pRSRC", &dirId);
    if(rc)
    {
        printf("Error %d creating working directory.\n", rc);
        return;
    }

    printf("Creating resource forks.\n");

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pICON", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rRc = SaveResourceToNewFile(refNum, dirId, "\pICON", rtIcons, -16455, "\pIcon resource",
                                    (unsigned char *)IcnsResource, sizeof(IcnsResource));
        rc  = HOpenDF(refNum, dirId, "\pICON", 0, &refFile);
        if(!rc)
        {
            count = strlen(rsrcText);
            wRc   = FSWrite(refFile, &count, rsrcText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostDiscImageChef;
            finderInfo.fdFlags   = kHasCustomIcon;
            rc = HSetFInfo(refNum, dirId, "\pICON", &finderInfo);
        }
    }
    printf("\tFile with three items in the resource fork: name = \"%s\", rc = %d, wRc = %d, cRc = %d, rRc = %d\n",
           "ICON", rc, wRc, cRc, rRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pPICT", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rRc = SaveResourceToNewFile(refNum, dirId, "\pPICT", ftPICTFile, 29876, "\pPicture resource",
                                    (unsigned char *)PictResource, sizeof(PictResource));
        rc  = HOpenDF(refNum, dirId, "\pPICT", 0, &refFile);
        if(!rc)
        {
            count = strlen(rsrcText);
            wRc   = FSWrite(refFile, &count, rsrcText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftPICTFile;
            finderInfo.fdCreator = ostDiscImageChef;
            rc = HSetFInfo(refNum, dirId, "\pPICT", &finderInfo);
        }
    }
    printf("\tFile with three items in the resource fork: name = \"%s\", rc = %d, wRc = %d, cRc = %d, rRc = %d\n",
           "PICT", rc, wRc, cRc, rRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pVERSION", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rRc = SaveResourceToNewFile(refNum, dirId, "\pVERSION", rtVersion, 1, "\pVersion resource",
                                    (unsigned char *)VersResource, sizeof(VersResource));
        rc  = HOpenDF(refNum, dirId, "\pVERSION", 0, &refFile);
        if(!rc)
        {
            count = strlen(rsrcText);
            wRc   = FSWrite(refFile, &count, rsrcText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftGenericDocumentPC;
            finderInfo.fdCreator = ostDiscImageChef;
            rc = HSetFInfo(refNum, dirId, "\pVERSION", &finderInfo);
        }
    }
    printf("\tFile with three items in the resource fork: name = \"%s\", rc = %d, wRc = %d, cRc = %d, rRc = %d\n",
           "VERSION", rc, wRc, cRc, rRc);

    memset(&finderInfo, 0, sizeof(FInfo));
    rc = HCreate(refNum, dirId, "\pALL", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rRc  = SaveResourceToNewFile(refNum, dirId, "\pALL", rtIcons, -16455, "\pIcon resource",
                                     (unsigned char *)IcnsResource, sizeof(IcnsResource));
        rRc2 = SaveResourceToNewFile(refNum, dirId, "\pALL", ftPICTFile, 29876, "\pPicture resource",
                                     (unsigned char *)PictResource, sizeof(PictResource));
        rRc3 = SaveResourceToNewFile(refNum, dirId, "\pALL", rtVersion, 1, "\pVersion resource",
                                     (unsigned char *)VersResource, sizeof(VersResource));
        rc   = HOpenDF(refNum, dirId, "\pALL", 0, &refFile);
        if(!rc)
        {
            count = strlen(rsrcText);
            wRc   = FSWrite(refFile, &count, rsrcText);
            cRc   = FSClose(refFile);
            finderInfo.fdType    = ftPICTFile;
            finderInfo.fdCreator = ostDiscImageChef;
            finderInfo.fdFlags   = kHasCustomIcon;
            rc = HSetFInfo(refNum, dirId, "\pALL", &finderInfo);
        }
    }
    printf("\tFile with three items in the resource fork: name = \"%s\", rc = %d, wRc = %d, cRc = %d, rRc = %d, rRc2 = %d, rRc3 = %d\n",
           "ALL", rc, wRc, cRc, rRc, rRc2, rRc3);
}

void Filenames(const char *path)
{
    OSErr        rc, wRc, cRc;
    Str255       str255;
    HVolumeParam hpb;
    int16_t      refNum;
    int16_t      refFile;
    int32_t      dirId;
    FInfo        finderInfo;
    int32_t      count;
    char         message[300];
    int          pos = 0;

    snprintf((char *)str255, 255, "%s", path);
    hpb.ioNamePtr  = str255;
    hpb.ioVRefNum  = 0;
    hpb.ioVolIndex = -1;
    rc = PBHGetVInfo((HParmBlkPtr) & hpb, 0);
    if(rc)
    {
        printf("Could not get volume information.\n");
        return;
    }
    refNum = hpb.ioVRefNum;

    rc = DirCreate(refNum, fsRtDirID, (unsigned char *)"\pFILENAME", &dirId);
    if(rc)
    {
        printf("Error %d creating working directory.\n", rc);
        return;
    }

    printf("Creating files with different filenames.\n");

    for(pos = 0; filenames[pos]; pos++)
    {
        count = strlen(filenames[pos]);
        memset(&str255, 0, sizeof(Str255));
        if(count > 255)
        {
            continue;
        }
        str255[0] = (char)count;
        memcpy(str255 + 1, filenames[pos], count);

        rc = HCreate(refNum, dirId, str255, ostUnknown, ftGenericDocumentPC);
        if(!rc)
        {
            rc = HOpenDF(refNum, dirId, str255, 0, &refFile);
            if(!rc)
            {
                memset(&message, 0, 300);
                sprintf((char *)message, FILENAME_FORMAT, filenames[pos]);
                count = strlen(message);
                wRc   = FSWrite(refFile, &count, message);
                cRc   = FSClose(refFile);
            }
        }

        printf("\tFile name = \"%s\", rc = %d, wRc = %d, cRc = %d\n", filenames[pos], rc, wRc, cRc);
    }
}

void Timestamps(const char *path)
{
    OSErr        rc, wRc, cRc, tRc;
    Str255       str255;
    HVolumeParam hpb;
    int16_t      refNum;
    int16_t      refFile;
    int32_t      dirId;
    FInfo        finderInfo;
    int32_t      count;
    HFileInfo    *fpb;
    CInfoPBRec   cipbr;
    char         message[300];

    snprintf((char *)str255, 255, "%s", path);
    hpb.ioNamePtr  = str255;
    hpb.ioVRefNum  = 0;
    hpb.ioVolIndex = -1;
    rc = PBHGetVInfo((HParmBlkPtr) & hpb, 0);
    if(rc)
    {
        printf("Could not get volume information.\n");
        return;
    }
    refNum = hpb.ioVRefNum;

    rc = DirCreate(refNum, fsRtDirID, (unsigned char *)"\pTIMES", &dirId);
    if(rc)
    {
        printf("Error %d creating working directory.\n", rc);
        return;
    }

    printf("Creating timestamped files.\n");

    rc = HCreate(refNum, dirId, "\pMAXCTIME", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pMAXCTIME", 0, &refFile);
        if(!rc)
        {
            memset(&message, 0, 300);
            sprintf((char *)message, DATETIME_FORMAT, MAXDATETIME, "creation");
            count = strlen(message);
            wRc   = FSWrite(refFile, &count, message);
            cRc   = FSClose(refFile);
        }
        memset(&cipbr, 0, sizeof(CInfoPBRec));
        fpb = (HFileInfo * ) & cipbr;
        fpb->ioVRefNum   = refNum;
        fpb->ioNamePtr   = "\pMAXCTIME";
        fpb->ioDirID     = dirId;
        fpb->ioFDirIndex = 0;
        PBGetCatInfoSync(&cipbr);

        fpb->ioFlCrDat = MAXTIMESTAMP;
        fpb->ioFlMdDat = NONTIMESTAMP;
        fpb->ioFlBkDat = NONTIMESTAMP;
        fpb->ioDirID   = dirId;
        tRc = PBSetCatInfoSync(&cipbr);
    }
    printf("\tFile name = \"%s\", rc = %d, wRc = %d, cRc = %d, tRc = %d\n", "MAXCTIME", rc, wRc, cRc, tRc);

    rc = HCreate(refNum, dirId, "\pMAXMTIME", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pMAXMTIME", 0, &refFile);
        if(!rc)
        {
            memset(&message, 0, 300);
            sprintf((char *)message, DATETIME_FORMAT, MAXDATETIME, "modification");
            count = strlen(message);
            wRc   = FSWrite(refFile, &count, message);
            cRc   = FSClose(refFile);
        }
        memset(&cipbr, 0, sizeof(CInfoPBRec));
        fpb = (HFileInfo * ) & cipbr;
        fpb->ioVRefNum   = refNum;
        fpb->ioNamePtr   = "\pMAXMTIME";
        fpb->ioDirID     = dirId;
        fpb->ioFDirIndex = 0;
        PBGetCatInfoSync(&cipbr);

        fpb->ioFlCrDat = NONTIMESTAMP;
        fpb->ioFlMdDat = MAXTIMESTAMP;
        fpb->ioFlBkDat = NONTIMESTAMP;
        fpb->ioDirID   = dirId;
        tRc = PBSetCatInfoSync(&cipbr);
    }
    printf("\tFile name = \"%s\", rc = %d, wRc = %d, cRc = %d, tRc = %d\n", "MAXMTIME", rc, wRc, cRc, tRc);

    rc = HCreate(refNum, dirId, "\pMAXBTIME", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pMAXBTIME", 0, &refFile);
        if(!rc)
        {
            memset(&message, 0, 300);
            sprintf((char *)message, DATETIME_FORMAT, MAXDATETIME, "backup");
            count = strlen(message);
            wRc   = FSWrite(refFile, &count, message);
            cRc   = FSClose(refFile);
        }
        memset(&cipbr, 0, sizeof(CInfoPBRec));
        fpb = (HFileInfo * ) & cipbr;
        fpb->ioVRefNum   = refNum;
        fpb->ioNamePtr   = "\pMAXBTIME";
        fpb->ioDirID     = dirId;
        fpb->ioFDirIndex = 0;
        PBGetCatInfoSync(&cipbr);

        fpb->ioFlCrDat = NONTIMESTAMP;
        fpb->ioFlMdDat = NONTIMESTAMP;
        fpb->ioFlBkDat = MAXTIMESTAMP;
        fpb->ioDirID   = dirId;
        tRc = PBSetCatInfoSync(&cipbr);
    }
    printf("\tFile name = \"%s\", rc = %d, wRc = %d, cRc = %d, tRc = %d\n", "MAXBTIME", rc, wRc, cRc, tRc);

    rc = HCreate(refNum, dirId, "\pMINCTIME", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pMINCTIME", 0, &refFile);
        if(!rc)
        {
            memset(&message, 0, 300);
            sprintf((char *)message, DATETIME_FORMAT, MINDATETIME, "creation");
            count = strlen(message);
            wRc   = FSWrite(refFile, &count, message);
            cRc   = FSClose(refFile);
        }
        memset(&cipbr, 0, sizeof(CInfoPBRec));
        fpb = (HFileInfo * ) & cipbr;
        fpb->ioVRefNum   = refNum;
        fpb->ioNamePtr   = "\pMINCTIME";
        fpb->ioDirID     = dirId;
        fpb->ioFDirIndex = 0;
        PBGetCatInfoSync(&cipbr);

        fpb->ioFlCrDat = MINTIMESTAMP;
        fpb->ioFlMdDat = NONTIMESTAMP;
        fpb->ioFlBkDat = NONTIMESTAMP;
        fpb->ioDirID   = dirId;
        tRc = PBSetCatInfoSync(&cipbr);
    }
    printf("\tFile name = \"%s\", rc = %d, wRc = %d, cRc = %d, tRc = %d\n", "MINCTIME", rc, wRc, cRc, tRc);

    rc = HCreate(refNum, dirId, "\pMINMTIME", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pMINMTIME", 0, &refFile);
        if(!rc)
        {
            memset(&message, 0, 300);
            sprintf((char *)message, DATETIME_FORMAT, MINDATETIME, "modification");
            count = strlen(message);
            wRc   = FSWrite(refFile, &count, message);
            cRc   = FSClose(refFile);
        }
        memset(&cipbr, 0, sizeof(CInfoPBRec));
        fpb = (HFileInfo * ) & cipbr;
        fpb->ioVRefNum   = refNum;
        fpb->ioNamePtr   = "\pMINMTIME";
        fpb->ioDirID     = dirId;
        fpb->ioFDirIndex = 0;
        PBGetCatInfoSync(&cipbr);

        fpb->ioFlCrDat = NONTIMESTAMP;
        fpb->ioFlMdDat = MINTIMESTAMP;
        fpb->ioFlBkDat = NONTIMESTAMP;
        fpb->ioDirID   = dirId;
        tRc = PBSetCatInfoSync(&cipbr);
    }
    printf("\tFile name = \"%s\", rc = %d, wRc = %d, cRc = %d, tRc = %d\n", "MINMTIME", rc, wRc, cRc, tRc);

    rc = HCreate(refNum, dirId, "\pMINBTIME", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pMINBTIME", 0, &refFile);
        if(!rc)
        {
            memset(&message, 0, 300);
            sprintf((char *)message, DATETIME_FORMAT, MINDATETIME, "backup");
            count = strlen(message);
            wRc   = FSWrite(refFile, &count, message);
            cRc   = FSClose(refFile);
        }
        memset(&cipbr, 0, sizeof(CInfoPBRec));
        fpb = (HFileInfo * ) & cipbr;
        fpb->ioVRefNum   = refNum;
        fpb->ioNamePtr   = "\pMINBTIME";
        fpb->ioDirID     = dirId;
        fpb->ioFDirIndex = 0;
        PBGetCatInfoSync(&cipbr);

        fpb->ioFlCrDat = NONTIMESTAMP;
        fpb->ioFlMdDat = NONTIMESTAMP;
        fpb->ioFlBkDat = MINTIMESTAMP;
        fpb->ioDirID   = dirId;
        tRc = PBSetCatInfoSync(&cipbr);
    }
    printf("\tFile name = \"%s\", rc = %d, wRc = %d, cRc = %d, tRc = %d\n", "MINBTIME", rc, wRc, cRc, tRc);

    rc = HCreate(refNum, dirId, "\pY2KCTIME", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pY2KCTIME", 0, &refFile);
        if(!rc)
        {
            memset(&message, 0, 300);
            sprintf((char *)message, DATETIME_FORMAT, Y2KDATETIME, "creation");
            count = strlen(message);
            wRc   = FSWrite(refFile, &count, message);
            cRc   = FSClose(refFile);
        }
        memset(&cipbr, 0, sizeof(CInfoPBRec));
        fpb = (HFileInfo * ) & cipbr;
        fpb->ioVRefNum   = refNum;
        fpb->ioNamePtr   = "\pY2KCTIME";
        fpb->ioDirID     = dirId;
        fpb->ioFDirIndex = 0;
        PBGetCatInfoSync(&cipbr);

        fpb->ioFlCrDat = Y2KTIMESTAMP;
        fpb->ioFlMdDat = NONTIMESTAMP;
        fpb->ioFlBkDat = NONTIMESTAMP;
        fpb->ioDirID   = dirId;
        tRc = PBSetCatInfoSync(&cipbr);
    }
    printf("\tFile name = \"%s\", rc = %d, wRc = %d, cRc = %d, tRc = %d\n", "Y2KCTIME", rc, wRc, cRc, tRc);

    rc = HCreate(refNum, dirId, "\pY2KMTIME", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pY2KMTIME", 0, &refFile);
        if(!rc)
        {
            memset(&message, 0, 300);
            sprintf((char *)message, DATETIME_FORMAT, Y2KDATETIME, "modification");
            count = strlen(message);
            wRc   = FSWrite(refFile, &count, message);
            cRc   = FSClose(refFile);
        }
        memset(&cipbr, 0, sizeof(CInfoPBRec));
        fpb = (HFileInfo * ) & cipbr;
        fpb->ioVRefNum   = refNum;
        fpb->ioNamePtr   = "\pY2KMTIME";
        fpb->ioDirID     = dirId;
        fpb->ioFDirIndex = 0;
        PBGetCatInfoSync(&cipbr);

        fpb->ioFlCrDat = NONTIMESTAMP;
        fpb->ioFlMdDat = Y2KTIMESTAMP;
        fpb->ioFlBkDat = NONTIMESTAMP;
        fpb->ioDirID   = dirId;
        tRc = PBSetCatInfoSync(&cipbr);
    }
    printf("\tFile name = \"%s\", rc = %d, wRc = %d, cRc = %d, tRc = %d\n", "Y2KMTIME", rc, wRc, cRc, tRc);

    rc = HCreate(refNum, dirId, "\pY2KBTIME", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pY2KBTIME", 0, &refFile);
        if(!rc)
        {
            memset(&message, 0, 300);
            sprintf((char *)message, DATETIME_FORMAT, Y2KDATETIME, "backup");
            count = strlen(message);
            wRc   = FSWrite(refFile, &count, message);
            cRc   = FSClose(refFile);
        }
        memset(&cipbr, 0, sizeof(CInfoPBRec));
        fpb = (HFileInfo * ) & cipbr;
        fpb->ioVRefNum   = refNum;
        fpb->ioNamePtr   = "\pY2KBTIME";
        fpb->ioDirID     = dirId;
        fpb->ioFDirIndex = 0;
        PBGetCatInfoSync(&cipbr);

        fpb->ioFlCrDat = NONTIMESTAMP;
        fpb->ioFlMdDat = NONTIMESTAMP;
        fpb->ioFlBkDat = Y2KTIMESTAMP;
        fpb->ioDirID   = dirId;
        tRc = PBSetCatInfoSync(&cipbr);
    }
    printf("\tFile name = \"%s\", rc = %d, wRc = %d, cRc = %d, tRc = %d\n", "Y2KBTIME", rc, wRc, cRc, tRc);

    rc = HCreate(refNum, dirId, "\pY1KCTIME", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pY1KCTIME", 0, &refFile);
        if(!rc)
        {
            memset(&message, 0, 300);
            sprintf((char *)message, DATETIME_FORMAT, Y1KDATETIME, "creation");
            count = strlen(message);
            wRc   = FSWrite(refFile, &count, message);
            cRc   = FSClose(refFile);
        }
        memset(&cipbr, 0, sizeof(CInfoPBRec));
        fpb = (HFileInfo * ) & cipbr;
        fpb->ioVRefNum   = refNum;
        fpb->ioNamePtr   = "\pY1KCTIME";
        fpb->ioDirID     = dirId;
        fpb->ioFDirIndex = 0;
        PBGetCatInfoSync(&cipbr);

        fpb->ioFlCrDat = Y1KTIMESTAMP;
        fpb->ioFlMdDat = NONTIMESTAMP;
        fpb->ioFlBkDat = NONTIMESTAMP;
        fpb->ioDirID   = dirId;
        tRc = PBSetCatInfoSync(&cipbr);
    }
    printf("\tFile name = \"%s\", rc = %d, wRc = %d, cRc = %d, tRc = %d\n", "Y1KCTIME", rc, wRc, cRc, tRc);

    rc = HCreate(refNum, dirId, "\pY1KMTIME", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pY1KMTIME", 0, &refFile);
        if(!rc)
        {
            memset(&message, 0, 300);
            sprintf((char *)message, DATETIME_FORMAT, Y1KDATETIME, "modification");
            count = strlen(message);
            wRc   = FSWrite(refFile, &count, message);
            cRc   = FSClose(refFile);
        }
        memset(&cipbr, 0, sizeof(CInfoPBRec));
        fpb = (HFileInfo * ) & cipbr;
        fpb->ioVRefNum   = refNum;
        fpb->ioNamePtr   = "\pY1KMTIME";
        fpb->ioDirID     = dirId;
        fpb->ioFDirIndex = 0;
        PBGetCatInfoSync(&cipbr);

        fpb->ioFlCrDat = NONTIMESTAMP;
        fpb->ioFlMdDat = Y1KTIMESTAMP;
        fpb->ioFlBkDat = NONTIMESTAMP;
        fpb->ioDirID   = dirId;
        tRc = PBSetCatInfoSync(&cipbr);
    }
    printf("\tFile name = \"%s\", rc = %d, wRc = %d, cRc = %d, tRc = %d\n", "Y1KMTIME", rc, wRc, cRc, tRc);

    rc = HCreate(refNum, dirId, "\pY1KBTIME", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pY1KBTIME", 0, &refFile);
        if(!rc)
        {
            memset(&message, 0, 300);
            sprintf((char *)message, DATETIME_FORMAT, Y1KDATETIME, "backup");
            count = strlen(message);
            wRc   = FSWrite(refFile, &count, message);
            cRc   = FSClose(refFile);
        }
        memset(&cipbr, 0, sizeof(CInfoPBRec));
        fpb = (HFileInfo * ) & cipbr;
        fpb->ioVRefNum   = refNum;
        fpb->ioNamePtr   = "\pY1KBTIME";
        fpb->ioDirID     = dirId;
        fpb->ioFDirIndex = 0;
        PBGetCatInfoSync(&cipbr);

        fpb->ioFlCrDat = NONTIMESTAMP;
        fpb->ioFlMdDat = NONTIMESTAMP;
        fpb->ioFlBkDat = Y1KTIMESTAMP;
        fpb->ioDirID   = dirId;
        tRc = PBSetCatInfoSync(&cipbr);
    }
    printf("\tFile name = \"%s\", rc = %d, wRc = %d, cRc = %d, tRc = %d\n", "Y1KBTIME", rc, wRc, cRc, tRc);

    rc = HCreate(refNum, dirId, "\pMAXTIME", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pMAXTIME", 0, &refFile);
        if(!rc)
        {
            memset(&message, 0, 300);
            sprintf((char *)message, DATETIME_FORMAT, MAXDATETIME, "all");
            count = strlen(message);
            wRc   = FSWrite(refFile, &count, message);
            cRc   = FSClose(refFile);
        }
        memset(&cipbr, 0, sizeof(CInfoPBRec));
        fpb = (HFileInfo * ) & cipbr;
        fpb->ioVRefNum   = refNum;
        fpb->ioNamePtr   = "\pMAXTIME";
        fpb->ioDirID     = dirId;
        fpb->ioFDirIndex = 0;
        PBGetCatInfoSync(&cipbr);

        fpb->ioFlCrDat = MAXTIMESTAMP;
        fpb->ioFlMdDat = MAXTIMESTAMP;
        fpb->ioFlBkDat = MAXTIMESTAMP;
        fpb->ioDirID   = dirId;
        tRc = PBSetCatInfoSync(&cipbr);
    }
    printf("\tFile name = \"%s\", rc = %d, wRc = %d, cRc = %d, tRc = %d\n", "MAXTIME", rc, wRc, cRc, tRc);

    rc = HCreate(refNum, dirId, "\pMINTIME", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pMINTIME", 0, &refFile);
        if(!rc)
        {
            memset(&message, 0, 300);
            sprintf((char *)message, DATETIME_FORMAT, MINDATETIME, "all");
            count = strlen(message);
            wRc   = FSWrite(refFile, &count, message);
            cRc   = FSClose(refFile);
        }
        memset(&cipbr, 0, sizeof(CInfoPBRec));
        fpb = (HFileInfo * ) & cipbr;
        fpb->ioVRefNum   = refNum;
        fpb->ioNamePtr   = "\pMINTIME";
        fpb->ioDirID     = dirId;
        fpb->ioFDirIndex = 0;
        PBGetCatInfoSync(&cipbr);

        fpb->ioFlCrDat = MINTIMESTAMP;
        fpb->ioFlMdDat = MINTIMESTAMP;
        fpb->ioFlBkDat = MINTIMESTAMP;
        fpb->ioDirID   = dirId;
        tRc = PBSetCatInfoSync(&cipbr);
    }
    printf("\tFile name = \"%s\", rc = %d, wRc = %d, cRc = %d, tRc = %d\n", "MINTIME", rc, wRc, cRc, tRc);

    rc = HCreate(refNum, dirId, "\pNOTIME", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pNOTIME", 0, &refFile);
        if(!rc)
        {
            memset(&message, 0, 300);
            sprintf((char *)message, DATETIME_FORMAT, NONDATETIME, "all");
            count = strlen(message);
            wRc   = FSWrite(refFile, &count, message);
            cRc   = FSClose(refFile);
        }
        memset(&cipbr, 0, sizeof(CInfoPBRec));
        fpb = (HFileInfo * ) & cipbr;
        fpb->ioVRefNum   = refNum;
        fpb->ioNamePtr   = "\pNOTIME";
        fpb->ioDirID     = dirId;
        fpb->ioFDirIndex = 0;
        PBGetCatInfoSync(&cipbr);

        fpb->ioFlCrDat = NONTIMESTAMP;
        fpb->ioFlMdDat = NONTIMESTAMP;
        fpb->ioFlBkDat = NONTIMESTAMP;
        fpb->ioDirID   = dirId;
        tRc = PBSetCatInfoSync(&cipbr);
    }
    printf("\tFile name = \"%s\", rc = %d, wRc = %d, cRc = %d, tRc = %d\n", "NOTIME", rc, wRc, cRc, tRc);

    rc = HCreate(refNum, dirId, "\pY2KTIME", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pY2KTIME", 0, &refFile);
        if(!rc)
        {
            memset(&message, 0, 300);
            sprintf((char *)message, DATETIME_FORMAT, Y2KDATETIME, "all");
            count = strlen(message);
            wRc   = FSWrite(refFile, &count, message);
            cRc   = FSClose(refFile);
        }
        memset(&cipbr, 0, sizeof(CInfoPBRec));
        fpb = (HFileInfo * ) & cipbr;
        fpb->ioVRefNum   = refNum;
        fpb->ioNamePtr   = "\pY2KTIME";
        fpb->ioDirID     = dirId;
        fpb->ioFDirIndex = 0;
        PBGetCatInfoSync(&cipbr);

        fpb->ioFlCrDat = Y2KTIMESTAMP;
        fpb->ioFlMdDat = Y2KTIMESTAMP;
        fpb->ioFlBkDat = Y2KTIMESTAMP;
        fpb->ioDirID   = dirId;
        tRc = PBSetCatInfoSync(&cipbr);
    }
    printf("\tFile name = \"%s\", rc = %d, wRc = %d, cRc = %d, tRc = %d\n", "Y2KTIME", rc, wRc, cRc, tRc);

    rc = HCreate(refNum, dirId, "\pY1KTIME", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pY1KTIME", 0, &refFile);
        if(!rc)
        {
            memset(&message, 0, 300);
            sprintf((char *)message, DATETIME_FORMAT, Y1KDATETIME, "all");
            count = strlen(message);
            wRc   = FSWrite(refFile, &count, message);
            cRc   = FSClose(refFile);
        }
        memset(&cipbr, 0, sizeof(CInfoPBRec));
        fpb = (HFileInfo * ) & cipbr;
        fpb->ioVRefNum   = refNum;
        fpb->ioNamePtr   = "\pY1KTIME";
        fpb->ioDirID     = dirId;
        fpb->ioFDirIndex = 0;
        PBGetCatInfoSync(&cipbr);

        fpb->ioFlCrDat = Y1KTIMESTAMP;
        fpb->ioFlMdDat = Y1KTIMESTAMP;
        fpb->ioFlBkDat = Y1KTIMESTAMP;
        fpb->ioDirID   = dirId;
        tRc = PBSetCatInfoSync(&cipbr);
    }
    printf("\tFile name = \"%s\", rc = %d, wRc = %d, cRc = %d, tRc = %d\n", "Y1KTIME", rc, wRc, cRc, tRc);
}

void DirectoryDepth(const char *path)
{
    OSErr        rc, wRc, cRc;
    Str255       str255;
    HVolumeParam hpb;
    int16_t      refNum;
    int16_t      refFile;
    int32_t      dirId;
    FInfo        finderInfo;
    int32_t      count;
    char         filename[9];
    int          pos = 0;

    snprintf((char *)str255, 255, "%s", path);
    hpb.ioNamePtr  = str255;
    hpb.ioVRefNum  = 0;
    hpb.ioVolIndex = -1;
    rc = PBHGetVInfo((HParmBlkPtr) & hpb, 0);
    if(rc)
    {
        printf("Could not get volume information.\n");
        return;
    }
    refNum = hpb.ioVRefNum;

    rc = DirCreate(refNum, fsRtDirID, (unsigned char *)"\pDEPTH", &dirId);
    if(rc)
    {
        printf("Error %d creating working directory.\n", rc);
        return;
    }

    printf("Creating deepest directory tree.\n");

    while(!rc)
    {
        memset(&filename, 0, 9);
        sprintf((char *)filename, "%08d", pos);
        str255[0] = 8;
        memcpy(str255 + 1, filename, 8);

        rc = DirCreate(refNum, dirId, str255, &dirId);

        pos++;
        /* Mac OS has no limit, but it will crash because the catalog is single threaded */
        if(pos == 500)
            break;
    }

    printf("\tCreated %d levels of directory hierarchy\n", pos);
}

void Fragmentation(const char *path, size_t clusterSize)
{
    size_t        halfCluster             = clusterSize / 2;
    size_t        quarterCluster          = clusterSize / 4;
    size_t        twoCluster              = clusterSize * 2;
    size_t        threeQuartersCluster    = halfCluster + quarterCluster;
    size_t        twoAndThreeQuartCluster = threeQuartersCluster + twoCluster;
    unsigned char *buffer;
    OSErr         rc, wRc, cRc;
    Str255        str255;
    HVolumeParam  hpb;
    int16_t       refNum;
    int16_t       refFile;
    int32_t       dirId;
    int32_t       count;
    long          i;

    snprintf((char *)str255, 255, "%s", path);
    hpb.ioNamePtr  = str255;
    hpb.ioVRefNum  = 0;
    hpb.ioVolIndex = -1;
    rc = PBHGetVInfo((HParmBlkPtr) & hpb, 0);
    if(rc)
    {
        printf("Could not get volume information.\n");
        return;
    }
    refNum = hpb.ioVRefNum;

    rc = DirCreate(refNum, fsRtDirID, (unsigned char *)"\pFRAGS", &dirId);
    if(rc)
    {
        printf("Error %d creating working directory.\n", rc);
        return;
    }

    rc = HCreate(refNum, dirId, "\pHALFCLST", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pHALFCLST", 0, &refFile);
        if(!rc)
        {
            buffer = malloc(halfCluster);
            memset(buffer, 0, halfCluster);

            for(i = 0; i < halfCluster; i++)
                buffer[i] = clauniaBytes[i % CLAUNIA_SIZE];

            count = halfCluster;
            wRc   = FSWrite(refFile, &count, buffer);
            cRc   = FSClose(refFile);
            free(buffer);
        }
    }

    printf("\tFile name = \"%s\", size = %d, rc = %d, wRc = %d, cRc = %d\n", "HALFCLST", halfCluster, rc, wRc, cRc);

    rc = HCreate(refNum, dirId, "\pQUARCLST", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pQUARCLST", 0, &refFile);
        if(!rc)
        {
            buffer = malloc(quarterCluster);
            memset(buffer, 0, quarterCluster);

            for(i = 0; i < quarterCluster; i++)
                buffer[i] = clauniaBytes[i % CLAUNIA_SIZE];

            count = quarterCluster;
            wRc   = FSWrite(refFile, &count, buffer);
            cRc   = FSClose(refFile);
            free(buffer);
        }
    }

    printf("\tFile name = \"%s\", size = %d, rc = %d, wRc = %d, cRc = %d\n", "QUARCLST", quarterCluster, rc, wRc, cRc);

    rc = HCreate(refNum, dirId, "\pTWOCLST", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pTWOCLST", 0, &refFile);
        if(!rc)
        {
            buffer = malloc(twoCluster);
            memset(buffer, 0, twoCluster);

            for(i = 0; i < twoCluster; i++)
                buffer[i] = clauniaBytes[i % CLAUNIA_SIZE];

            count = twoCluster;
            wRc   = FSWrite(refFile, &count, buffer);
            cRc   = FSClose(refFile);
            free(buffer);
        }
    }

    printf("\tFile name = \"%s\", size = %d, rc = %d, wRc = %d, cRc = %d\n", "TWOCLST", twoCluster, rc, wRc, cRc);

    rc = HCreate(refNum, dirId, "\pTRQTCLST", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pTRQTCLST", 0, &refFile);
        if(!rc)
        {
            buffer = malloc(threeQuartersCluster);
            memset(buffer, 0, threeQuartersCluster);

            for(i = 0; i < threeQuartersCluster; i++)
                buffer[i] = clauniaBytes[i % CLAUNIA_SIZE];

            count = threeQuartersCluster;
            wRc   = FSWrite(refFile, &count, buffer);
            cRc   = FSClose(refFile);
            free(buffer);
        }
    }

    printf("\tFile name = \"%s\", size = %d, rc = %d, wRc = %d, cRc = %d\n", "TRQTCLST", threeQuartersCluster, rc, wRc,
           cRc);

    rc = HCreate(refNum, dirId, "\pTWOQCLST", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pTWOQCLST", 0, &refFile);
        if(!rc)
        {
            buffer = malloc(twoAndThreeQuartCluster);
            memset(buffer, 0, twoAndThreeQuartCluster);

            for(i = 0; i < twoAndThreeQuartCluster; i++)
                buffer[i] = clauniaBytes[i % CLAUNIA_SIZE];

            count = twoAndThreeQuartCluster;
            wRc   = FSWrite(refFile, &count, buffer);
            cRc   = FSClose(refFile);
            free(buffer);
        }
    }

    printf("\tFile name = \"%s\", size = %d, rc = %d, wRc = %d, cRc = %d\n", "TWTQCLST", twoAndThreeQuartCluster, rc,
           wRc, cRc);

    rc = HCreate(refNum, dirId, "\pTWO1", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pTWO1", 0, &refFile);
        if(!rc)
        {
            buffer = malloc(twoCluster);
            memset(buffer, 0, twoCluster);

            for(i = 0; i < twoCluster; i++)
                buffer[i] = clauniaBytes[i % CLAUNIA_SIZE];

            count = twoCluster;
            wRc   = FSWrite(refFile, &count, buffer);
            cRc   = FSClose(refFile);
            free(buffer);
        }
    }

    printf("\tFile name = \"%s\", size = %d, rc = %d, wRc = %d, cRc = %d\n", "TWO1", twoCluster, rc, wRc, cRc);

    rc = HCreate(refNum, dirId, "\pTWO2", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pTWO2", 0, &refFile);
        if(!rc)
        {
            buffer = malloc(twoCluster);
            memset(buffer, 0, twoCluster);

            for(i = 0; i < twoCluster; i++)
                buffer[i] = clauniaBytes[i % CLAUNIA_SIZE];

            count = twoCluster;
            wRc   = FSWrite(refFile, &count, buffer);
            cRc   = FSClose(refFile);
            free(buffer);
        }
    }

    printf("\tFile name = \"%s\", size = %d, rc = %d, wRc = %d, cRc = %d\n", "TWO2", twoCluster, rc, wRc, cRc);

    rc = HCreate(refNum, dirId, "\pTWO3", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pTWO3", 0, &refFile);
        if(!rc)
        {
            buffer = malloc(twoCluster);
            memset(buffer, 0, twoCluster);

            for(i = 0; i < twoCluster; i++)
                buffer[i] = clauniaBytes[i % CLAUNIA_SIZE];

            count = twoCluster;
            wRc   = FSWrite(refFile, &count, buffer);
            cRc   = FSClose(refFile);
            free(buffer);
        }
    }

    printf("\tDeleting \"TWO2\".\n");
    rc = HDelete(refNum, dirId, "\pTWO2");

    printf("\tFile name = \"%s\", size = %d, rc = %d, wRc = %d, cRc = %d\n", "TWO3", twoCluster, rc, wRc, cRc);

    rc = HCreate(refNum, dirId, "\pFRAGTHRQ", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pFRAGTHRQ", 0, &refFile);
        if(!rc)
        {
            buffer = malloc(threeQuartersCluster);
            memset(buffer, 0, threeQuartersCluster);

            for(i = 0; i < threeQuartersCluster; i++)
                buffer[i] = clauniaBytes[i % CLAUNIA_SIZE];

            count = threeQuartersCluster;
            wRc   = FSWrite(refFile, &count, buffer);
            cRc   = FSClose(refFile);
            free(buffer);
        }
    }

    printf("\tDeleting \"TWO1\".\n");
    rc = HDelete(refNum, dirId, "\pTWO1");
    printf("\tDeleting \"TWO3\".\n");
    rc = HDelete(refNum, dirId, "\pTWO3");

    rc = HCreate(refNum, dirId, "\pFRAGSIXQ", ostUnknown, ftGenericDocumentPC);
    if(!rc)
    {
        rc = HOpenDF(refNum, dirId, "\pFRAGSIXQ", 0, &refFile);
        if(!rc)
        {
            buffer = malloc(twoAndThreeQuartCluster);
            memset(buffer, 0, twoAndThreeQuartCluster);

            for(i = 0; i < twoAndThreeQuartCluster; i++)
                buffer[i] = clauniaBytes[i % CLAUNIA_SIZE];

            count = twoAndThreeQuartCluster;
            wRc   = FSWrite(refFile, &count, buffer);
            cRc   = FSClose(refFile);
            free(buffer);
        }
    }

    printf("\tFile name = \"%s\", size = %d, rc = %d, wRc = %d, cRc = %d\n", "FRAGSIXQ", twoAndThreeQuartCluster, rc,
           wRc, cRc);
}

void Sparse(const char *path)
{
    /* Do nothing, not supported by target operating system */
}

static pascal OSErr

CreateAliasFile(const FSSpec *targetFile, const FSSpec *aliasFile, OSType fileCreator, OSType fileType)
{
    short       rsrcID;
    short       aliasRefnum;
    FInfo       finf;
    OSErr       err;
    AliasHandle alias;

    FSpCreateResFile(aliasFile, fileCreator, fileType, 0);
    err = ResError();
    if(err != noErr)
        return err;

    err = NewAlias(aliasFile, targetFile, &alias);
    if(err != noErr || alias == NULL)
        return err;

    aliasRefnum = FSpOpenResFile(aliasFile, fsRdWrPerm);

    AddResource((Handle)alias, rAliasType, 0, "\pDiscImageChef alias");
    err = ResError();

    CloseResFile(aliasRefnum);
    FSpGetFInfo(aliasFile, &finf);

    finf.fdCreator = fileCreator;
    finf.fdType    = fileType;
    finf.fdFlags |= 0x8000;
    finf.fdFlags &= (~0x0100);

    FSpSetFInfo(aliasFile, &finf);

    return err;
}

void Links(const char *path)
{
    int32_t      gestaltResponse;
    OSErr        rc, wRc, cRc, oRc, aRc;
    Str255       str255;
    HVolumeParam hpb;
    int16_t      refNum;
    int16_t      refFile;
    int32_t      dirId;
    char         filename[9];
    int          pos = 0;
    FSSpec       targetSpec, aliasSpec;
    int32_t      count;

    rc = Gestalt(gestaltAliasMgrAttr, &gestaltResponse);
    if(rc || !(gestaltResponse & (1 << gestaltAliasMgrPresent)))
    {
        printf("Alias Manager not present, cannot create aliases.\n");
        return;
    }

    snprintf((char *)str255, 255, "%s", path);
    hpb.ioNamePtr  = str255;
    hpb.ioVRefNum  = 0;
    hpb.ioVolIndex = -1;
    rc = PBHGetVInfo((HParmBlkPtr) & hpb, 0);
    if(rc)
    {
        printf("Could not get volume information.\n");
        return;
    }
    refNum = hpb.ioVRefNum;

    rc = DirCreate(refNum, fsRtDirID, (unsigned char *)"\pLINKS", &dirId);
    if(rc)
    {
        printf("Error %d creating working directory.\n", rc);
        return;
    }

    printf("Creating aliases.\n");

    for(pos = 0; pos < 64; pos++)
    {
        memset(&filename, 0, 9);
        sprintf((char *)filename, "TARGET%02d", pos);
        str255[0] = 8;
        memcpy(str255 + 1, filename, 8);

        rc = HCreate(refNum, dirId, str255, ostSimpleText, ftGenericDocumentPC);

        if(rc)
            break;

        oRc = HOpenDF(refNum, dirId, str255, 0, &refFile);
        if(oRc)
            break;

        count = strlen(targetText);
        wRc   = FSWrite(refFile, &count, targetText);
        cRc   = FSClose(refFile);

        memset(&targetSpec, 0, sizeof(FSSpec));
        targetSpec.vRefNum = refNum;
        targetSpec.parID   = dirId;
        targetSpec.name[0] = 8;
        memcpy(targetSpec.name + 1, filename, 8);

        memset(&filename, 0, 9);
        sprintf((char *)filename, "ALIAS_%02d", pos);
        memset(&aliasSpec, 0, sizeof(FSSpec));
        aliasSpec.vRefNum = refNum;
        aliasSpec.parID   = dirId;
        aliasSpec.name[0] = 8;
        memcpy(aliasSpec.name + 1, filename, 8);

        aRc = CreateAliasFile(&targetSpec, &aliasSpec, ostSimpleText, ftGenericDocumentPC);

        if(aRc)
            break;
    }

    printf("pos = %d, rc = %d, wRc = %d, cRc = %d, oRc = %d, aRc = %d\n", pos, rc, wRc, cRc, oRc, aRc);
}

void MillionFiles(const char *path)
{
    OSErr        rc, wRc, cRc;
    Str255       str255;
    HVolumeParam hpb;
    int16_t      refNum;
    int16_t      refFile;
    int32_t      dirId;
    FInfo        finderInfo;
    int32_t      count;
    char         filename[9];
    int          pos = 0;

    snprintf((char *)str255, 255, "%s", path);
    hpb.ioNamePtr  = str255;
    hpb.ioVRefNum  = 0;
    hpb.ioVolIndex = -1;
    rc = PBHGetVInfo((HParmBlkPtr) & hpb, 0);
    if(rc)
    {
        printf("Could not get volume information.\n");
        return;
    }
    refNum = hpb.ioVRefNum;

    rc = DirCreate(refNum, fsRtDirID, (unsigned char *)"\pMILLION", &dirId);
    if(rc)
    {
        printf("Error %d creating working directory.\n", rc);
        return;
    }

    printf("Creating lots of files.\n");

    for(pos = 0; pos < 5000; pos++)
    {
        memset(&filename, 0, 9);
        sprintf((char *)filename, "%08d", pos);
        str255[0] = 8;
        memcpy(str255 + 1, filename, 8);

        rc = HCreate(refNum, dirId, str255, ostUnknown, ftGenericDocumentPC);

        if(rc)
            break;
    }

    printf("\tCreated %d files\n", pos);
}

void DeleteFiles(const char *path)
{
    OSErr        rc, wRc, cRc;
    Str255       str255;
    HVolumeParam hpb;
    int16_t      refNum;
    int16_t      refFile;
    int32_t      dirId;
    FInfo        finderInfo;
    int32_t      count;
    char         filename[9];
    int          pos = 0;

    snprintf((char *)str255, 255, "%s", path);
    hpb.ioNamePtr  = str255;
    hpb.ioVRefNum  = 0;
    hpb.ioVolIndex = -1;
    rc = PBHGetVInfo((HParmBlkPtr) & hpb, 0);
    if(rc)
    {
        printf("Could not get volume information.\n");
        return;
    }
    refNum = hpb.ioVRefNum;

    rc = DirCreate(refNum, fsRtDirID, (unsigned char *)"\pDELETED", &dirId);
    if(rc)
    {
        printf("Error %d creating working directory.\n", rc);
        return;
    }

    printf("Creating and deleting files.\n");

    for(pos = 0; pos < 64; pos++)
    {
        memset(&filename, 0, 9);
        sprintf((char *)filename, "%08d", pos);
        str255[0] = 8;
        memcpy(str255 + 1, filename, 8);

        rc = HCreate(refNum, dirId, str255, ostUnknown, ftGenericDocumentPC);

        if(rc)
            break;

        HDelete(refNum, dirId, str255);
    }
}
#endif
