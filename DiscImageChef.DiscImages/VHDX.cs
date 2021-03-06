// /***************************************************************************
// The Disc Image Chef
// ----------------------------------------------------------------------------
//
// Filename       : VHDX.cs
// Author(s)      : Natalia Portillo <claunia@claunia.com>
//
// Component      : Disk image plugins.
//
// --[ Description ] ----------------------------------------------------------
//
//     Manages Microsoft Hyper-V disk images.
//
// --[ License ] --------------------------------------------------------------
//
//     This library is free software; you can redistribute it and/or modify
//     it under the terms of the GNU Lesser General Public License as
//     published by the Free Software Foundation; either version 2.1 of the
//     License, or (at your option) any later version.
//
//     This library is distributed in the hope that it will be useful, but
//     WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
//     Lesser General Public License for more details.
//
//     You should have received a copy of the GNU Lesser General Public
//     License along with this library; if not, see <http://www.gnu.org/licenses/>.
//
// ----------------------------------------------------------------------------
// Copyright © 2011-2018 Natalia Portillo
// ****************************************************************************/

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using DiscImageChef.CommonTypes;
using DiscImageChef.Console;
using DiscImageChef.Filters;
using Schemas;

namespace DiscImageChef.DiscImages
{
    public class Vhdx : IMediaImage
    {
        const ulong VHDX_SIGNATURE    = 0x656C696678646876;
        const uint  VHDX_HEADER_SIG   = 0x64616568;
        const uint  VHDX_REGION_SIG   = 0x69676572;
        const ulong VHDX_METADATA_SIG = 0x617461646174656D;

        const string PARENT_LINKAGE_KEY      = "parent_linkage";
        const string PARENT_LINKAGE2_KEY     = "parent_linkage2";
        const string RELATIVE_PATH_KEY       = "relative_path";
        const string VOLUME_PATH_KEY         = "volume_path";
        const string ABSOLUTE_WIN32_PATH_KEY = "absolute_win32_path";

        const uint REGION_FLAGS_REQUIRED = 0x01;

        const uint METADATA_FLAGS_USER     = 0x01;
        const uint METADATA_FLAGS_VIRTUAL  = 0x02;
        const uint METADATA_FLAGS_REQUIRED = 0x04;

        const uint FILE_FLAGS_LEAVE_ALLOCATED = 0x01;
        const uint FILE_FLAGS_HAS_PARENT      = 0x02;

        /// <summary>Block has never been stored on this image, check parent</summary>
        const ulong PAYLOAD_BLOCK_NOT_PRESENT = 0x00;
        /// <summary>Block was stored on this image and is removed, return whatever data you wish</summary>
        const ulong PAYLOAD_BLOCK_UNDEFINED = 0x01;
        /// <summary>Block is filled with zeroes</summary>
        const ulong PAYLOAD_BLOCK_ZERO = 0x02;
        /// <summary>All sectors in this block were UNMAPed/TRIMed, return zeroes</summary>
        const ulong PAYLOAD_BLOCK_UNMAPPER = 0x03;
        /// <summary>Block is present on this image</summary>
        const ulong PAYLOAD_BLOCK_FULLY_PRESENT = 0x06;
        /// <summary>Block is present on image but there may be sectors present on parent image</summary>
        const ulong PAYLOAD_BLOCK_PARTIALLY_PRESENT = 0x07;

        const ulong SECTOR_BITMAP_NOT_PRESENT = 0x00;
        const ulong SECTOR_BITMAP_PRESENT     = 0x06;

        const ulong BAT_FILE_OFFSET_MASK = 0xFFFFFFFFFFFC0000;
        const ulong BAT_FLAGS_MASK       = 0x7;
        const ulong BAT_RESERVED_MASK    = 0x3FFF8;

        const    int  MAX_CACHE_SIZE         = 16777216;
        readonly Guid batGuid                = new Guid("2DC27766-F623-4200-9D64-115E9BFD4A08");
        readonly Guid fileParametersGuid     = new Guid("CAA16737-FA36-4D43-B3B6-33F0AA44E76B");
        readonly Guid logicalSectorSizeGuid  = new Guid("8141BF1D-A96F-4709-BA47-F233A8FAAB5F");
        readonly Guid metadataGuid           = new Guid("8B7CA206-4790-4B9A-B8FE-575F050F886E");
        readonly Guid page83DataGuid         = new Guid("BECA12AB-B2E6-4523-93EF-C309E000C746");
        readonly Guid parentLocatorGuid      = new Guid("A8D35F2D-B30B-454D-ABF7-D3D84834AB0C");
        readonly Guid parentTypeVhdxGuid     = new Guid("B04AEFB7-D19E-4A81-B789-25B8E9445913");
        readonly Guid physicalSectorSizeGuid = new Guid("CDA348C7-445D-4471-9CC9-E9885251C556");
        readonly Guid virtualDiskSizeGuid    = new Guid("2FA54224-CD1B-4876-B211-5DBED83BF4B8");

        long batOffset;

        ulong[]                   blockAllocationTable;
        Dictionary<ulong, byte[]> blockCache;

        long        chunkRatio;
        ulong       dataBlocks;
        bool        hasParent;
        ImageInfo   imageInfo;
        Stream      imageStream;
        uint        logicalSectorSize;
        int         maxBlockCache;
        int         maxSectorCache;
        long        metadataOffset;
        Guid        page83Data;
        IMediaImage parentImage;
        uint        physicalSectorSize;
        byte[]      sectorBitmap;
        ulong[]     sectorBitmapPointers;

        Dictionary<ulong, byte[]> sectorCache;
        VhdxFileParameters        vFileParms;
        VhdxHeader                vHdr;

        VhdxIdentifier vhdxId;

        ulong                    virtualDiskSize;
        VhdxMetadataTableHeader  vMetHdr;
        VhdxMetadataTableEntry[] vMets;
        VhdxParentLocatorHeader  vParHdr;
        VhdxParentLocatorEntry[] vPars;
        VhdxRegionTableHeader    vRegHdr;
        VhdxRegionTableEntry[]   vRegs;

        public Vhdx()
        {
            imageInfo = new ImageInfo
            {
                ReadableSectorTags    = new List<SectorTagType>(),
                ReadableMediaTags     = new List<MediaTagType>(),
                HasPartitions         = false,
                HasSessions           = false,
                Version               = null,
                Application           = null,
                ApplicationVersion    = null,
                Creator               = null,
                Comments              = null,
                MediaManufacturer     = null,
                MediaModel            = null,
                MediaSerialNumber     = null,
                MediaBarcode          = null,
                MediaPartNumber       = null,
                MediaSequence         = 0,
                LastMediaSequence     = 0,
                DriveManufacturer     = null,
                DriveModel            = null,
                DriveSerialNumber     = null,
                DriveFirmwareRevision = null
            };
        }

        public ImageInfo Info => imageInfo;

        public string Name => "Microsoft VHDX";
        public Guid   Id   => new Guid("536B141B-D09C-4799-AB70-34631286EB9D");

        public string Format => "VHDX";

        public List<Partition> Partitions =>
            throw new FeatureUnsupportedImageException("Feature not supported by image format");

        public List<Track> Tracks =>
            throw new FeatureUnsupportedImageException("Feature not supported by image format");

        public List<Session> Sessions =>
            throw new FeatureUnsupportedImageException("Feature not supported by image format");

        public bool Identify(IFilter imageFilter)
        {
            Stream stream = imageFilter.GetDataForkStream();
            stream.Seek(0, SeekOrigin.Begin);

            if(stream.Length < 512) return false;

            byte[] vhdxIdB = new byte[Marshal.SizeOf(vhdxId)];
            stream.Read(vhdxIdB, 0, Marshal.SizeOf(vhdxId));
            vhdxId       = new VhdxIdentifier();
            IntPtr idPtr = Marshal.AllocHGlobal(Marshal.SizeOf(vhdxId));
            Marshal.Copy(vhdxIdB, 0, idPtr, Marshal.SizeOf(vhdxId));
            vhdxId = (VhdxIdentifier)Marshal.PtrToStructure(idPtr, typeof(VhdxIdentifier));
            Marshal.FreeHGlobal(idPtr);

            return vhdxId.signature == VHDX_SIGNATURE;
        }

        public bool Open(IFilter imageFilter)
        {
            Stream stream = imageFilter.GetDataForkStream();
            stream.Seek(0, SeekOrigin.Begin);

            if(stream.Length < 512) return false;

            byte[] vhdxIdB = new byte[Marshal.SizeOf(vhdxId)];
            stream.Read(vhdxIdB, 0, Marshal.SizeOf(vhdxId));
            vhdxId       = new VhdxIdentifier();
            IntPtr idPtr = Marshal.AllocHGlobal(Marshal.SizeOf(vhdxId));
            Marshal.Copy(vhdxIdB, 0, idPtr, Marshal.SizeOf(vhdxId));
            vhdxId = (VhdxIdentifier)Marshal.PtrToStructure(idPtr, typeof(VhdxIdentifier));
            Marshal.FreeHGlobal(idPtr);

            if(vhdxId.signature != VHDX_SIGNATURE) return false;

            imageInfo.Application = Encoding.Unicode.GetString(vhdxId.creator);

            stream.Seek(64 * 1024, SeekOrigin.Begin);
            byte[] vHdrB = new byte[Marshal.SizeOf(vHdr)];
            stream.Read(vHdrB, 0, Marshal.SizeOf(vHdr));
            vHdr             = new VhdxHeader();
            IntPtr headerPtr = Marshal.AllocHGlobal(Marshal.SizeOf(vHdr));
            Marshal.Copy(vHdrB, 0, headerPtr, Marshal.SizeOf(vHdr));
            vHdr = (VhdxHeader)Marshal.PtrToStructure(headerPtr, typeof(VhdxHeader));
            Marshal.FreeHGlobal(headerPtr);

            if(vHdr.Signature != VHDX_HEADER_SIG)
            {
                stream.Seek(128 * 1024, SeekOrigin.Begin);
                vHdrB = new byte[Marshal.SizeOf(vHdr)];
                stream.Read(vHdrB, 0, Marshal.SizeOf(vHdr));
                vHdr      = new VhdxHeader();
                headerPtr = Marshal.AllocHGlobal(Marshal.SizeOf(vHdr));
                Marshal.Copy(vHdrB, 0, headerPtr, Marshal.SizeOf(vHdr));
                vHdr = (VhdxHeader)Marshal.PtrToStructure(headerPtr, typeof(VhdxHeader));
                Marshal.FreeHGlobal(headerPtr);

                if(vHdr.Signature != VHDX_HEADER_SIG) throw new ImageNotSupportedException("VHDX header not found");
            }

            stream.Seek(192 * 1024, SeekOrigin.Begin);
            byte[] vRegTableB = new byte[Marshal.SizeOf(vRegHdr)];
            stream.Read(vRegTableB, 0, Marshal.SizeOf(vRegHdr));
            vRegHdr           = new VhdxRegionTableHeader();
            IntPtr vRegTabPtr = Marshal.AllocHGlobal(Marshal.SizeOf(vRegHdr));
            Marshal.Copy(vRegTableB, 0, vRegTabPtr, Marshal.SizeOf(vRegHdr));
            vRegHdr = (VhdxRegionTableHeader)Marshal.PtrToStructure(vRegTabPtr, typeof(VhdxRegionTableHeader));
            Marshal.FreeHGlobal(vRegTabPtr);

            if(vRegHdr.signature != VHDX_REGION_SIG)
            {
                stream.Seek(256 * 1024, SeekOrigin.Begin);
                vRegTableB = new byte[Marshal.SizeOf(vRegHdr)];
                stream.Read(vRegTableB, 0, Marshal.SizeOf(vRegHdr));
                vRegHdr    = new VhdxRegionTableHeader();
                vRegTabPtr = Marshal.AllocHGlobal(Marshal.SizeOf(vRegHdr));
                Marshal.Copy(vRegTableB, 0, vRegTabPtr, Marshal.SizeOf(vRegHdr));
                vRegHdr = (VhdxRegionTableHeader)Marshal.PtrToStructure(vRegTabPtr, typeof(VhdxRegionTableHeader));
                Marshal.FreeHGlobal(vRegTabPtr);

                if(vRegHdr.signature != VHDX_REGION_SIG)
                    throw new ImageNotSupportedException("VHDX region table not found");
            }

            vRegs = new VhdxRegionTableEntry[vRegHdr.entries];
            for(int i = 0; i < vRegs.Length; i++)
            {
                byte[] vRegB = new byte[Marshal.SizeOf(vRegs[i])];
                stream.Read(vRegB, 0, Marshal.SizeOf(vRegs[i]));
                vRegs[i]       = new VhdxRegionTableEntry();
                IntPtr vRegPtr = Marshal.AllocHGlobal(Marshal.SizeOf(vRegs[i]));
                Marshal.Copy(vRegB, 0, vRegPtr, Marshal.SizeOf(vRegs[i]));
                vRegs[i] = (VhdxRegionTableEntry)Marshal.PtrToStructure(vRegPtr, typeof(VhdxRegionTableEntry));
                Marshal.FreeHGlobal(vRegPtr);

                if(vRegs[i].guid      == batGuid) batOffset = (long)vRegs[i].offset;
                else if(vRegs[i].guid == metadataGuid)
                    metadataOffset = (long)vRegs[i].offset;
                else if((vRegs[i].flags & REGION_FLAGS_REQUIRED) == REGION_FLAGS_REQUIRED)
                    throw new
                        ImageNotSupportedException($"Found unsupported and required region Guid {vRegs[i].guid}, not proceeding with image.");
            }

            if(batOffset == 0) throw new Exception("BAT not found, cannot continue.");

            if(metadataOffset == 0) throw new Exception("Metadata not found, cannot continue.");

            uint fileParamsOff = 0, vdSizeOff = 0, p83Off = 0, logOff = 0, physOff = 0, parentOff = 0;

            stream.Seek(metadataOffset, SeekOrigin.Begin);
            byte[] metTableB = new byte[Marshal.SizeOf(vMetHdr)];
            stream.Read(metTableB, 0, Marshal.SizeOf(vMetHdr));
            vMetHdr            = new VhdxMetadataTableHeader();
            IntPtr metTablePtr = Marshal.AllocHGlobal(Marshal.SizeOf(vMetHdr));
            Marshal.Copy(metTableB, 0, metTablePtr, Marshal.SizeOf(vMetHdr));
            vMetHdr = (VhdxMetadataTableHeader)Marshal.PtrToStructure(metTablePtr, typeof(VhdxMetadataTableHeader));
            Marshal.FreeHGlobal(metTablePtr);

            vMets = new VhdxMetadataTableEntry[vMetHdr.entries];
            for(int i = 0; i < vMets.Length; i++)
            {
                byte[] vMetB = new byte[Marshal.SizeOf(vMets[i])];
                stream.Read(vMetB, 0, Marshal.SizeOf(vMets[i]));
                vMets[i]       = new VhdxMetadataTableEntry();
                IntPtr vMetPtr = Marshal.AllocHGlobal(Marshal.SizeOf(vMets[i]));
                Marshal.Copy(vMetB, 0, vMetPtr, Marshal.SizeOf(vMets[i]));
                vMets[i] = (VhdxMetadataTableEntry)Marshal.PtrToStructure(vMetPtr, typeof(VhdxMetadataTableEntry));
                Marshal.FreeHGlobal(vMetPtr);

                if(vMets[i].itemId      == fileParametersGuid) fileParamsOff = vMets[i].offset;
                else if(vMets[i].itemId == virtualDiskSizeGuid)
                    vdSizeOff = vMets[i].offset;
                else if(vMets[i].itemId == page83DataGuid)
                    p83Off = vMets[i].offset;
                else if(vMets[i].itemId == logicalSectorSizeGuid)
                    logOff = vMets[i].offset;
                else if(vMets[i].itemId == physicalSectorSizeGuid)
                    physOff = vMets[i].offset;
                else if(vMets[i].itemId == parentLocatorGuid)
                    parentOff = vMets[i].offset;
                else if((vMets[i].flags & METADATA_FLAGS_REQUIRED) == METADATA_FLAGS_REQUIRED)
                    throw new
                        ImageNotSupportedException($"Found unsupported and required metadata Guid {vMets[i].itemId}, not proceeding with image.");
            }

            byte[] tmp;

            if(fileParamsOff != 0)
            {
                stream.Seek(fileParamsOff + metadataOffset, SeekOrigin.Begin);
                tmp = new byte[8];
                stream.Read(tmp, 0, 8);
                vFileParms = new VhdxFileParameters
                {
                    blockSize = BitConverter.ToUInt32(tmp, 0),
                    flags     = BitConverter.ToUInt32(tmp, 4)
                };
            }
            else throw new Exception("File parameters not found.");

            if(vdSizeOff != 0)
            {
                stream.Seek(vdSizeOff + metadataOffset, SeekOrigin.Begin);
                tmp = new byte[8];
                stream.Read(tmp, 0, 8);
                virtualDiskSize = BitConverter.ToUInt64(tmp, 0);
            }
            else throw new Exception("Virtual disk size not found.");

            if(p83Off != 0)
            {
                stream.Seek(p83Off + metadataOffset, SeekOrigin.Begin);
                tmp = new byte[16];
                stream.Read(tmp, 0, 16);
                page83Data = new Guid(tmp);
            }

            if(logOff != 0)
            {
                stream.Seek(logOff + metadataOffset, SeekOrigin.Begin);
                tmp = new byte[4];
                stream.Read(tmp, 0, 4);
                logicalSectorSize = BitConverter.ToUInt32(tmp, 0);
            }
            else throw new Exception("Logical sector size not found.");

            if(physOff != 0)
            {
                stream.Seek(physOff + metadataOffset, SeekOrigin.Begin);
                tmp = new byte[4];
                stream.Read(tmp, 0, 4);
                physicalSectorSize = BitConverter.ToUInt32(tmp, 0);
            }
            else throw new Exception("Physical sector size not found.");

            if(parentOff != 0 && (vFileParms.flags & FILE_FLAGS_HAS_PARENT) == FILE_FLAGS_HAS_PARENT)
            {
                stream.Seek(parentOff + metadataOffset, SeekOrigin.Begin);
                byte[] vParHdrB = new byte[Marshal.SizeOf(vMetHdr)];
                stream.Read(vParHdrB, 0, Marshal.SizeOf(vMetHdr));
                vParHdr           = new VhdxParentLocatorHeader();
                IntPtr vParHdrPtr = Marshal.AllocHGlobal(Marshal.SizeOf(vMetHdr));
                Marshal.Copy(vParHdrB, 0, vParHdrPtr, Marshal.SizeOf(vMetHdr));
                vParHdr = (VhdxParentLocatorHeader)Marshal.PtrToStructure(vParHdrPtr, typeof(VhdxParentLocatorHeader));
                Marshal.FreeHGlobal(vParHdrPtr);

                if(vParHdr.locatorType != parentTypeVhdxGuid)
                    throw new
                        ImageNotSupportedException($"Found unsupported and required parent locator type {vParHdr.locatorType}, not proceeding with image.");

                vPars = new VhdxParentLocatorEntry[vParHdr.keyValueCount];
                for(int i = 0; i < vPars.Length; i++)
                {
                    byte[] vParB = new byte[Marshal.SizeOf(vPars[i])];
                    stream.Read(vParB, 0, Marshal.SizeOf(vPars[i]));
                    vPars[i]       = new VhdxParentLocatorEntry();
                    IntPtr vParPtr = Marshal.AllocHGlobal(Marshal.SizeOf(vPars[i]));
                    Marshal.Copy(vParB, 0, vParPtr, Marshal.SizeOf(vPars[i]));
                    vPars[i] = (VhdxParentLocatorEntry)Marshal.PtrToStructure(vParPtr, typeof(VhdxParentLocatorEntry));
                    Marshal.FreeHGlobal(vParPtr);
                }
            }
            else if((vFileParms.flags & FILE_FLAGS_HAS_PARENT) == FILE_FLAGS_HAS_PARENT)
                throw new Exception("Parent locator not found.");

            if((vFileParms.flags & FILE_FLAGS_HAS_PARENT) == FILE_FLAGS_HAS_PARENT &&
               vParHdr.locatorType                        == parentTypeVhdxGuid)
            {
                parentImage      = new Vhdx();
                bool parentWorks = false;

                foreach(VhdxParentLocatorEntry parentEntry in vPars)
                {
                    stream.Seek(parentEntry.keyOffset + metadataOffset, SeekOrigin.Begin);
                    byte[] tmpKey = new byte[parentEntry.keyLength];
                    stream.Read(tmpKey, 0, tmpKey.Length);
                    string entryType = Encoding.Unicode.GetString(tmpKey);

                    IFilter parentFilter;
                    if(string.Compare(entryType, RELATIVE_PATH_KEY, StringComparison.OrdinalIgnoreCase) == 0)
                    {
                        stream.Seek(parentEntry.valueOffset + metadataOffset, SeekOrigin.Begin);
                        byte[] tmpVal = new byte[parentEntry.valueLength];
                        stream.Read(tmpVal, 0, tmpVal.Length);
                        string entryValue = Encoding.Unicode.GetString(tmpVal);

                        try
                        {
                            parentFilter =
                                new FiltersList().GetFilter(Path.Combine(imageFilter.GetParentFolder(), entryValue));
                            if(parentFilter != null && parentImage.Open(parentFilter))
                            {
                                parentWorks = true;
                                break;
                            }
                        }
                        catch { parentWorks = false; }

                        string relEntry = Path.Combine(Path.GetDirectoryName(imageFilter.GetPath()), entryValue);

                        try
                        {
                            parentFilter =
                                new FiltersList().GetFilter(Path.Combine(imageFilter.GetParentFolder(), relEntry));
                            if(parentFilter == null || !parentImage.Open(parentFilter)) continue;

                            parentWorks = true;
                            break;
                        }
                        catch
                        {
                            // ignored
                        }
                    }
                    else if(string.Compare(entryType, VOLUME_PATH_KEY, StringComparison.OrdinalIgnoreCase) ==
                            0 ||
                            string.Compare(entryType, ABSOLUTE_WIN32_PATH_KEY, StringComparison.OrdinalIgnoreCase) == 0)
                    {
                        stream.Seek(parentEntry.valueOffset + metadataOffset, SeekOrigin.Begin);
                        byte[] tmpVal = new byte[parentEntry.valueLength];
                        stream.Read(tmpVal, 0, tmpVal.Length);
                        string entryValue = Encoding.Unicode.GetString(tmpVal);

                        try
                        {
                            parentFilter =
                                new FiltersList().GetFilter(Path.Combine(imageFilter.GetParentFolder(), entryValue));
                            if(parentFilter == null || !parentImage.Open(parentFilter)) continue;

                            parentWorks = true;
                            break;
                        }
                        catch
                        {
                            // ignored
                        }
                    }
                }

                if(!parentWorks) throw new Exception("Image is differential but parent cannot be opened.");

                hasParent = true;
            }

            chunkRatio = (long)(Math.Pow(2, 23) * logicalSectorSize / vFileParms.blockSize);
            dataBlocks = virtualDiskSize        / vFileParms.blockSize;
            if(virtualDiskSize                  % vFileParms.blockSize > 0) dataBlocks++;

            long batEntries;
            if(hasParent)
            {
                long sectorBitmapBlocks = (long)dataBlocks / chunkRatio;
                if(dataBlocks                              % (ulong)chunkRatio > 0) sectorBitmapBlocks++;
                sectorBitmapPointers = new ulong[sectorBitmapBlocks];

                batEntries = sectorBitmapBlocks * (chunkRatio - 1);
            }
            else batEntries = (long)(dataBlocks + (dataBlocks - 1) / (ulong)chunkRatio);

            DicConsole.DebugWriteLine("VHDX plugin", "Reading BAT");

            long readChunks      = 0;
            blockAllocationTable = new ulong[dataBlocks];
            byte[] batB          = new byte[batEntries * 8];
            stream.Seek(batOffset, SeekOrigin.Begin);
            stream.Read(batB, 0, batB.Length);

            ulong skipSize = 0;
            for(ulong i = 0; i < dataBlocks; i++)
                if(readChunks  == chunkRatio)
                {
                    if(hasParent)
                        sectorBitmapPointers[skipSize / 8] = BitConverter.ToUInt64(batB, (int)(i * 8 + skipSize));

                    readChunks =  0;
                    skipSize   += 8;
                }
                else
                {
                    blockAllocationTable[i] = BitConverter.ToUInt64(batB, (int)(i * 8 + skipSize));
                    readChunks++;
                }

            if(hasParent)
            {
                DicConsole.DebugWriteLine("VHDX plugin", "Reading Sector Bitmap");

                MemoryStream sectorBmpMs = new MemoryStream();
                foreach(ulong pt in sectorBitmapPointers)
                    switch(pt & BAT_FLAGS_MASK)
                    {
                        case SECTOR_BITMAP_NOT_PRESENT:
                            sectorBmpMs.Write(new byte[1048576], 0, 1048576);
                            break;
                        case SECTOR_BITMAP_PRESENT:
                            stream.Seek((long)((pt & BAT_FILE_OFFSET_MASK) * 1048576), SeekOrigin.Begin);
                            byte[] bmp = new byte[1048576];
                            stream.Read(bmp, 0, bmp.Length);
                            sectorBmpMs.Write(bmp, 0, bmp.Length);
                            break;
                        default:
                            if((pt & BAT_FLAGS_MASK) != 0)
                                throw new
                                    ImageNotSupportedException($"Unsupported sector bitmap block flags (0x{pt & BAT_FLAGS_MASK:X16}) found, not proceeding.");

                            break;
                    }

                sectorBitmap = sectorBmpMs.ToArray();
                sectorBmpMs.Close();
            }

            maxBlockCache  = (int)(MAX_CACHE_SIZE / vFileParms.blockSize);
            maxSectorCache = (int)(MAX_CACHE_SIZE / logicalSectorSize);

            imageStream = stream;

            sectorCache = new Dictionary<ulong, byte[]>();
            blockCache  = new Dictionary<ulong, byte[]>();

            imageInfo.CreationTime         = imageFilter.GetCreationTime();
            imageInfo.LastModificationTime = imageFilter.GetLastWriteTime();
            imageInfo.MediaTitle           = Path.GetFileNameWithoutExtension(imageFilter.GetFilename());
            imageInfo.SectorSize           = logicalSectorSize;
            imageInfo.XmlMediaType         = XmlMediaType.BlockMedia;
            imageInfo.MediaType            = MediaType.GENERIC_HDD;
            imageInfo.ImageSize            = virtualDiskSize;
            imageInfo.Sectors              = imageInfo.ImageSize / imageInfo.SectorSize;
            imageInfo.DriveSerialNumber    = page83Data.ToString();

            // TODO: Separate image application from version, need several samples.

            imageInfo.Cylinders       = (uint)(imageInfo.Sectors / 16 / 63);
            imageInfo.Heads           = 16;
            imageInfo.SectorsPerTrack = 63;

            return true;
        }

        public byte[] ReadSector(ulong sectorAddress)
        {
            if(sectorAddress > imageInfo.Sectors - 1)
                throw new ArgumentOutOfRangeException(nameof(sectorAddress),
                                                      $"Sector address {sectorAddress} not found");

            if(sectorCache.TryGetValue(sectorAddress, out byte[] sector)) return sector;

            ulong index  = sectorAddress * logicalSectorSize / vFileParms.blockSize;
            ulong secOff = sectorAddress * logicalSectorSize % vFileParms.blockSize;

            ulong blkPtr   = blockAllocationTable[index];
            ulong blkFlags = blkPtr & BAT_FLAGS_MASK;

            if((blkPtr & BAT_RESERVED_MASK) != 0)
                throw new
                    ImageNotSupportedException($"Unknown flags (0x{blkPtr & BAT_RESERVED_MASK:X16}) set in block pointer");

            switch(blkFlags & BAT_FLAGS_MASK)
            {
                case PAYLOAD_BLOCK_NOT_PRESENT:
                    return hasParent ? parentImage.ReadSector(sectorAddress) : new byte[logicalSectorSize];
                case PAYLOAD_BLOCK_UNDEFINED:
                case PAYLOAD_BLOCK_ZERO:
                case PAYLOAD_BLOCK_UNMAPPER: return new byte[logicalSectorSize];
            }

            bool partialBlock;
            partialBlock = (blkFlags & BAT_FLAGS_MASK) == PAYLOAD_BLOCK_PARTIALLY_PRESENT;

            if(partialBlock && hasParent && !CheckBitmap(sectorAddress)) return parentImage.ReadSector(sectorAddress);

            if(!blockCache.TryGetValue(blkPtr & BAT_FILE_OFFSET_MASK, out byte[] block))
            {
                block = new byte[vFileParms.blockSize];
                imageStream.Seek((long)(blkPtr & BAT_FILE_OFFSET_MASK), SeekOrigin.Begin);
                imageStream.Read(block, 0, block.Length);

                if(blockCache.Count >= maxBlockCache) blockCache.Clear();

                blockCache.Add(blkPtr & BAT_FILE_OFFSET_MASK, block);
            }

            sector = new byte[logicalSectorSize];
            Array.Copy(block, (int)secOff, sector, 0, sector.Length);

            if(sectorCache.Count >= maxSectorCache) sectorCache.Clear();

            sectorCache.Add(sectorAddress, sector);

            return sector;
        }

        public byte[] ReadSectors(ulong sectorAddress, uint length)
        {
            if(sectorAddress > imageInfo.Sectors - 1)
                throw new ArgumentOutOfRangeException(nameof(sectorAddress),
                                                      $"Sector address {sectorAddress} not found");

            if(sectorAddress + length > imageInfo.Sectors)
                throw new ArgumentOutOfRangeException(nameof(length),
                                                      $"Requested more sectors ({sectorAddress + length}) than available ({imageInfo.Sectors})");

            MemoryStream ms = new MemoryStream();

            for(uint i = 0; i < length; i++)
            {
                byte[] sector = ReadSector(sectorAddress + i);
                ms.Write(sector, 0, sector.Length);
            }

            return ms.ToArray();
        }

        public byte[] ReadDiskTag(MediaTagType tag)
        {
            throw new FeatureUnsupportedImageException("Feature not supported by image format");
        }

        public byte[] ReadSectorTag(ulong sectorAddress, SectorTagType tag)
        {
            throw new FeatureUnsupportedImageException("Feature not supported by image format");
        }

        public byte[] ReadSector(ulong sectorAddress, uint track)
        {
            throw new FeatureUnsupportedImageException("Feature not supported by image format");
        }

        public byte[] ReadSectorTag(ulong sectorAddress, uint track, SectorTagType tag)
        {
            throw new FeatureUnsupportedImageException("Feature not supported by image format");
        }

        public byte[] ReadSectorsTag(ulong sectorAddress, uint length, SectorTagType tag)
        {
            throw new FeatureUnsupportedImageException("Feature not supported by image format");
        }

        public byte[] ReadSectors(ulong sectorAddress, uint length, uint track)
        {
            throw new FeatureUnsupportedImageException("Feature not supported by image format");
        }

        public byte[] ReadSectorsTag(ulong sectorAddress, uint length, uint track, SectorTagType tag)
        {
            throw new FeatureUnsupportedImageException("Feature not supported by image format");
        }

        public byte[] ReadSectorLong(ulong sectorAddress)
        {
            throw new FeatureUnsupportedImageException("Feature not supported by image format");
        }

        public byte[] ReadSectorLong(ulong sectorAddress, uint track)
        {
            throw new FeatureUnsupportedImageException("Feature not supported by image format");
        }

        public byte[] ReadSectorsLong(ulong sectorAddress, uint length)
        {
            throw new FeatureUnsupportedImageException("Feature not supported by image format");
        }

        public byte[] ReadSectorsLong(ulong sectorAddress, uint length, uint track)
        {
            throw new FeatureUnsupportedImageException("Feature not supported by image format");
        }

        public List<Track> GetSessionTracks(Session session)
        {
            throw new FeatureUnsupportedImageException("Feature not supported by image format");
        }

        public List<Track> GetSessionTracks(ushort session)
        {
            throw new FeatureUnsupportedImageException("Feature not supported by image format");
        }

        public bool? VerifySector(ulong sectorAddress)
        {
            return null;
        }

        public bool? VerifySector(ulong sectorAddress, uint track)
        {
            throw new FeatureUnsupportedImageException("Feature not supported by image format");
        }

        public bool? VerifySectors(ulong sectorAddress, uint length, out List<ulong> failingLbas,
                                   out                                   List<ulong> unknownLbas)
        {
            failingLbas = new List<ulong>();
            unknownLbas = new List<ulong>();
            for(ulong i = 0; i < imageInfo.Sectors; i++) unknownLbas.Add(i);

            return null;
        }

        public bool? VerifySectors(ulong sectorAddress, uint length, uint track, out List<ulong> failingLbas,
                                   out                                               List<ulong> unknownLbas)
        {
            throw new FeatureUnsupportedImageException("Feature not supported by image format");
        }

        public bool? VerifyMediaImage()
        {
            return null;
        }

        public List<DumpHardwareType> DumpHardware => null;
        public CICMMetadataType       CicmMetadata => null;

        bool CheckBitmap(ulong sectorAddress)
        {
            long index = (long)(sectorAddress / 8);
            int  shift = (int)(sectorAddress  % 8);
            byte val   = (byte)(1 << shift);

            if(index > sectorBitmap.LongLength) return false;

            return (sectorBitmap[index] & val) == val;
        }

        static uint VhdxChecksum(IEnumerable<byte> data)
        {
            uint checksum = data.Aggregate<byte, uint>(0, (current, b) => current + b);

            return ~checksum;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        struct VhdxIdentifier
        {
            /// <summary>
            ///     Signature, <see cref="Vhdx.VHDX_SIGNATURE" />
            /// </summary>
            public ulong signature;
            /// <summary>
            ///     UTF-16 string containing creator
            /// </summary>
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 512)]
            public byte[] creator;
        }

        #pragma warning disable 649
        #pragma warning disable 169
        struct VhdxHeader
        {
            /// <summary>
            ///     Signature, <see cref="Vhdx.VHDX_HEADER_SIG" />
            /// </summary>
            public uint Signature;
            /// <summary>
            ///     CRC-32C of whole 4096 bytes header with this field set to 0
            /// </summary>
            public uint Checksum;
            /// <summary>
            ///     Sequence number
            /// </summary>
            public ulong Sequence;
            /// <summary>
            ///     Unique identifier for file contents, must be changed on first write to metadata
            /// </summary>
            public Guid FileWriteGuid;
            /// <summary>
            ///     Unique identifier for disk contents, must be changed on first write to metadata or data
            /// </summary>
            public Guid DataWriteGuid;
            /// <summary>
            ///     Unique identifier for log entries
            /// </summary>
            public Guid LogGuid;
            /// <summary>
            ///     Version of log format
            /// </summary>
            public ushort LogVersion;
            /// <summary>
            ///     Version of VHDX format
            /// </summary>
            public ushort Version;
            /// <summary>
            ///     Length in bytes of the log
            /// </summary>
            public uint LogLength;
            /// <summary>
            ///     Offset from image start to the log
            /// </summary>
            public ulong LogOffset;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4016)]
            public byte[] Reserved;
        }
        #pragma warning restore 649
        #pragma warning restore 169

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        struct VhdxRegionTableHeader
        {
            /// <summary>
            ///     Signature, <see cref="Vhdx.VHDX_REGION_SIG" />
            /// </summary>
            public uint signature;
            /// <summary>
            ///     CRC-32C of whole 64Kb table with this field set to 0
            /// </summary>
            public uint checksum;
            /// <summary>
            ///     How many entries follow this table
            /// </summary>
            public uint entries;
            /// <summary>
            ///     Reserved
            /// </summary>
            public uint reserved;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        struct VhdxRegionTableEntry
        {
            /// <summary>
            ///     Object identifier
            /// </summary>
            public Guid guid;
            /// <summary>
            ///     Offset in image of the object
            /// </summary>
            public ulong offset;
            /// <summary>
            ///     Length in bytes of the object
            /// </summary>
            public uint length;
            /// <summary>
            ///     Flags
            /// </summary>
            public uint flags;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        struct VhdxMetadataTableHeader
        {
            /// <summary>
            ///     Signature
            /// </summary>
            public ulong signature;
            /// <summary>
            ///     Reserved
            /// </summary>
            public ushort reserved;
            /// <summary>
            ///     How many entries are in the table
            /// </summary>
            public ushort entries;
            /// <summary>
            ///     Reserved
            /// </summary>
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 5)]
            public uint[] reserved2;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        struct VhdxMetadataTableEntry
        {
            /// <summary>
            ///     Metadata ID
            /// </summary>
            public Guid itemId;
            /// <summary>
            ///     Offset relative to start of metadata region
            /// </summary>
            public uint offset;
            /// <summary>
            ///     Length in bytes
            /// </summary>
            public uint length;
            /// <summary>
            ///     Flags
            /// </summary>
            public uint flags;
            /// <summary>
            ///     Reserved
            /// </summary>
            public uint reserved;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        struct VhdxFileParameters
        {
            /// <summary>
            ///     Block size in bytes
            /// </summary>
            public uint blockSize;
            /// <summary>
            ///     Flags
            /// </summary>
            public uint flags;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        struct VhdxParentLocatorHeader
        {
            /// <summary>
            ///     Type of parent virtual disk
            /// </summary>
            public Guid   locatorType;
            public ushort reserved;
            /// <summary>
            ///     How many KVPs are in this parent locator
            /// </summary>
            public ushort keyValueCount;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        struct VhdxParentLocatorEntry
        {
            /// <summary>
            ///     Offset from metadata to key
            /// </summary>
            public uint keyOffset;
            /// <summary>
            ///     Offset from metadata to value
            /// </summary>
            public uint valueOffset;
            /// <summary>
            ///     Size of key
            /// </summary>
            public ushort keyLength;
            /// <summary>
            ///     Size of value
            /// </summary>
            public ushort valueLength;
        }
    }
}