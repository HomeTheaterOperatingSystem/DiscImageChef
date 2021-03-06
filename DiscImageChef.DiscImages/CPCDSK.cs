// /***************************************************************************
// The Disc Image Chef
// ----------------------------------------------------------------------------
//
// Filename       : CPCDSK.cs
// Author(s)      : Natalia Portillo <claunia@claunia.com>
//
// Component      : Disk image plugins.
//
// --[ Description ] ----------------------------------------------------------
//
//     Manages CPCEMU disk images.
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
using DiscImageChef.Checksums;
using DiscImageChef.CommonTypes;
using DiscImageChef.Console;
using DiscImageChef.Decoders.Floppy;
using DiscImageChef.Filters;
using Schemas;

namespace DiscImageChef.DiscImages
{
    public class Cpcdsk : IMediaImage
    {
        /// <summary>
        ///     Identifier for CPCEMU disk images, "MV - CPCEMU Disk-File"
        /// </summary>
        readonly byte[] cpcdskId =
        {
            0x4D, 0x56, 0x20, 0x2D, 0x20, 0x43, 0x50, 0x43, 0x45, 0x4D, 0x55, 0x20, 0x44, 0x69, 0x73, 0x6B, 0x2D, 0x46,
            0x69, 0x6C, 0x65
        };
        /// <summary>
        ///     Identifier for DU54 disk images, "MV - CPC format Disk Image (DU54)"
        /// </summary>
        readonly byte[] du54Id =
        {
            0x4D, 0x56, 0x20, 0x2D, 0x20, 0x43, 0x50, 0x43, 0x20, 0x66, 0x6F, 0x72, 0x6D, 0x61, 0x74, 0x20, 0x44, 0x69,
            0x73, 0x6B, 0x20
        };
        /// <summary>
        ///     Identifier for Extended CPCEMU disk images, "EXTENDED CPC DSK File"
        /// </summary>
        readonly byte[] edskId =
        {
            0x45, 0x58, 0x54, 0x45, 0x4E, 0x44, 0x45, 0x44, 0x20, 0x43, 0x50, 0x43, 0x20, 0x44, 0x53, 0x4B, 0x20, 0x46,
            0x69, 0x6C, 0x65
        };
        /// <summary>
        ///     Identifier for track information, "Track-Info\r\n"
        /// </summary>
        readonly byte[]           trackId = {0x54, 0x72, 0x61, 0x63, 0x6B, 0x2D, 0x49, 0x6E, 0x66, 0x6F};
        Dictionary<ulong, byte[]> addressMarks;

        bool                      extended;
        ImageInfo                 imageInfo;
        Dictionary<ulong, byte[]> sectors;

        public Cpcdsk()
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

        public string Name => "CPCEMU Disk-File and Extended CPC Disk-File";
        public Guid   Id   => new Guid("724B16CC-ADB9-492E-BA07-CAEEC1012B16");

        public string Format => extended ? "CPCEMU Extended disk image" : "CPCEMU disk image";

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

            byte[] headerB = new byte[256];
            stream.Read(headerB, 0, 256);
            IntPtr headerPtr = Marshal.AllocHGlobal(256);
            Marshal.Copy(headerB, 0, headerPtr, 256);
            CpcDiskInfo header = (CpcDiskInfo)Marshal.PtrToStructure(headerPtr, typeof(CpcDiskInfo));
            Marshal.FreeHGlobal(headerPtr);

            DicConsole.DebugWriteLine("CPCDSK plugin", "header.magic = \"{0}\"",
                                      StringHandlers.CToString(header.magic));

            return cpcdskId.SequenceEqual(header.magic) || edskId.SequenceEqual(header.magic) ||
                   du54Id.SequenceEqual(header.magic);
        }

        public bool Open(IFilter imageFilter)
        {
            Stream stream = imageFilter.GetDataForkStream();
            stream.Seek(0, SeekOrigin.Begin);

            if(stream.Length < 512) return false;

            byte[] headerB = new byte[256];
            stream.Read(headerB, 0, 256);
            IntPtr headerPtr = Marshal.AllocHGlobal(256);
            Marshal.Copy(headerB, 0, headerPtr, 256);
            CpcDiskInfo header = (CpcDiskInfo)Marshal.PtrToStructure(headerPtr, typeof(CpcDiskInfo));
            Marshal.FreeHGlobal(headerPtr);

            if(!cpcdskId.SequenceEqual(header.magic) && !edskId.SequenceEqual(header.magic) &&
               !du54Id.SequenceEqual(header.magic)) return false;

            extended = edskId.SequenceEqual(header.magic);
            DicConsole.DebugWriteLine("CPCDSK plugin", "Extended = {0}", extended);
            DicConsole.DebugWriteLine("CPCDSK plugin", "header.magic = \"{0}\"",
                                      StringHandlers.CToString(header.magic));
            DicConsole.DebugWriteLine("CPCDSK plugin", "header.magic2 = \"{0}\"",
                                      StringHandlers.CToString(header.magic2));
            DicConsole.DebugWriteLine("CPCDSK plugin", "header.creator = \"{0}\"",
                                      StringHandlers.CToString(header.creator));
            DicConsole.DebugWriteLine("CPCDSK plugin",               "header.tracks = {0}",    header.tracks);
            DicConsole.DebugWriteLine("CPCDSK plugin",               "header.sides = {0}",     header.sides);
            if(!extended) DicConsole.DebugWriteLine("CPCDSK plugin", "header.tracksize = {0}", header.tracksize);
            else
                for(int i = 0; i < header.tracks; i++)
                {
                    for(int j = 0; j < header.sides; j++)
                        DicConsole.DebugWriteLine("CPCDSK plugin", "Track {0} Side {1} size = {2}", i, j,
                                                  header.tracksizeTable[i * header.sides + j] * 256);
                }

            ulong currentSector     = 0;
            sectors                 = new Dictionary<ulong, byte[]>();
            addressMarks            = new Dictionary<ulong, byte[]>();
            ulong readtracks        = 0;
            bool  allTracksSameSize = true;
            ulong sectorsPerTrack   = 0;

            // Seek to first track descriptor
            stream.Seek(256, SeekOrigin.Begin);
            for(int i = 0; i < header.tracks; i++)
            {
                for(int j = 0; j < header.sides; j++)
                {
                    // Track not stored in image
                    if(extended && header.tracksizeTable[i * header.sides + j] == 0) continue;

                    long trackPos = stream.Position;

                    byte[] trackB = new byte[256];
                    stream.Read(trackB, 0, 256);
                    IntPtr trackPtr = Marshal.AllocHGlobal(256);
                    Marshal.Copy(trackB, 0, trackPtr, 256);
                    CpcTrackInfo trackInfo = (CpcTrackInfo)Marshal.PtrToStructure(trackPtr, typeof(CpcTrackInfo));
                    Marshal.FreeHGlobal(trackPtr);

                    if(!trackId.SequenceEqual(trackInfo.magic))
                    {
                        DicConsole.ErrorWriteLine("Not the expected track info.");
                        return false;
                    }

                    DicConsole.DebugWriteLine("CPCDSK plugin", "trackInfo[{1}:{2}].magic = \"{0}\"",
                                              StringHandlers.CToString(trackInfo.magic), i, j);
                    DicConsole.DebugWriteLine("CPCDSK plugin", "trackInfo[{1}:{2}].bps = {0}",
                                              SizeCodeToBytes(trackInfo.bps), i, j);
                    DicConsole.DebugWriteLine("CPCDSK plugin", "trackInfo[{1}:{2}].dataRate = {0}", trackInfo.dataRate,
                                              i, j);
                    DicConsole.DebugWriteLine("CPCDSK plugin", "trackInfo[{1}:{2}].filler = 0x{0:X2}", trackInfo.filler,
                                              i, j);
                    DicConsole.DebugWriteLine("CPCDSK plugin", "trackInfo[{1}:{2}].gap3 = 0x{0:X2}", trackInfo.gap3, i,
                                              j);
                    DicConsole.DebugWriteLine("CPCDSK plugin", "trackInfo[{1}:{2}].padding = {0}", trackInfo.padding, i,
                                              j);
                    DicConsole.DebugWriteLine("CPCDSK plugin", "trackInfo[{1}:{2}].recordingMode = {0}",
                                              trackInfo.recordingMode, i, j);
                    DicConsole.DebugWriteLine("CPCDSK plugin", "trackInfo[{1}:{2}].sectors = {0}", trackInfo.sectors, i,
                                              j);
                    DicConsole.DebugWriteLine("CPCDSK plugin", "trackInfo[{1}:{2}].side = {0}",  trackInfo.side,  i, j);
                    DicConsole.DebugWriteLine("CPCDSK plugin", "trackInfo[{1}:{2}].track = {0}", trackInfo.track, i, j);

                    if(trackInfo.sectors   != sectorsPerTrack)
                        if(sectorsPerTrack == 0)
                            sectorsPerTrack = trackInfo.sectors;
                        else
                            allTracksSameSize = false;

                    byte[][] thisTrackSectors      = new byte[trackInfo.sectors][];
                    byte[][] thisTrackAddressMarks = new byte[trackInfo.sectors][];

                    for(int k = 1; k <= trackInfo.sectors; k++)
                    {
                        DicConsole.DebugWriteLine("CPCDSK plugin", "trackInfo[{1}:{2}].sector[{3}].id = 0x{0:X2}",
                                                  trackInfo.sectorsInfo[k - 1].id, i, j, k);
                        DicConsole.DebugWriteLine("CPCDSK plugin", "trackInfo[{1}:{2}].sector[{3}].len = {0}",
                                                  trackInfo.sectorsInfo[k - 1].len, i, j, k);
                        DicConsole.DebugWriteLine("CPCDSK plugin", "trackInfo[{1}:{2}].sector[{3}].side = {0}",
                                                  trackInfo.sectorsInfo[k - 1].side, i, j, k);
                        DicConsole.DebugWriteLine("CPCDSK plugin", "trackInfo[{1}:{2}].sector[{3}].size = {0}",
                                                  SizeCodeToBytes(trackInfo.sectorsInfo[k - 1].size), i, j, k);
                        DicConsole.DebugWriteLine("CPCDSK plugin", "trackInfo[{1}:{2}].sector[{3}].st1 = 0x{0:X2}",
                                                  trackInfo.sectorsInfo[k - 1].st1, i, j, k);
                        DicConsole.DebugWriteLine("CPCDSK plugin", "trackInfo[{1}:{2}].sector[{3}].st2 = 0x{0:X2}",
                                                  trackInfo.sectorsInfo[k - 1].st2, i, j, k);
                        DicConsole.DebugWriteLine("CPCDSK plugin", "trackInfo[{1}:{2}].sector[{3}].track = {0}",
                                                  trackInfo.sectorsInfo[k - 1].track, i, j, k);

                        int sectLen = extended
                                          ? trackInfo.sectorsInfo[k                 - 1].len
                                          : SizeCodeToBytes(trackInfo.sectorsInfo[k - 1].size);

                        byte[] sector = new byte[sectLen];
                        stream.Read(sector, 0, sectLen);

                        if(sectLen < SizeCodeToBytes(trackInfo.sectorsInfo[k - 1].size))
                        {
                            byte[] temp = new byte[SizeCodeToBytes(trackInfo.sectorsInfo[k - 1].size)];
                            Array.Copy(sector, 0, temp, 0, sector.Length);
                            sector = temp;
                        }
                        else if(sectLen > SizeCodeToBytes(trackInfo.sectorsInfo[k - 1].size))
                        {
                            byte[] temp = new byte[SizeCodeToBytes(trackInfo.sectorsInfo[k - 1].size)];
                            Array.Copy(sector, 0, temp, 0, temp.Length);
                            sector = temp;
                        }

                        thisTrackSectors[(trackInfo.sectorsInfo[k - 1].id & 0x3F) - 1] = sector;

                        byte[] amForCrc = new byte[8];
                        amForCrc[0]     = 0xA1;
                        amForCrc[1]     = 0xA1;
                        amForCrc[2]     = 0xA1;
                        amForCrc[3]     = (byte)IBMIdType.AddressMark;
                        amForCrc[4]     = trackInfo.sectorsInfo[k       - 1].track;
                        amForCrc[5]     = trackInfo.sectorsInfo[k       - 1].side;
                        amForCrc[6]     = trackInfo.sectorsInfo[k       - 1].id;
                        amForCrc[7]     = (byte)trackInfo.sectorsInfo[k - 1].size;

                        Crc16Context.Data(amForCrc, 8, out byte[] amCrc);

                        byte[] addressMark = new byte[22];
                        Array.Copy(amForCrc, 0, addressMark, 12, 8);
                        Array.Copy(amCrc,    0, addressMark, 20, 2);

                        thisTrackAddressMarks[(trackInfo.sectorsInfo[k - 1].id & 0x3F) - 1] = addressMark;
                    }

                    for(int s = 0; s < thisTrackSectors.Length; s++)
                    {
                        sectors.Add(currentSector, thisTrackSectors[s]);
                        addressMarks.Add(currentSector, thisTrackAddressMarks[s]);
                        currentSector++;
                        if(thisTrackSectors[s].Length > imageInfo.SectorSize)
                            imageInfo.SectorSize = (uint)thisTrackSectors[s].Length;
                    }

                    stream.Seek(trackPos, SeekOrigin.Begin);
                    if(extended)
                    {
                        stream.Seek(header.tracksizeTable[i * header.sides + j] * 256,
                                    SeekOrigin.Current);
                        imageInfo.ImageSize += (ulong)(header.tracksizeTable[i * header.sides + j] * 256) - 256;
                    }
                    else
                    {
                        stream.Seek(header.tracksize, SeekOrigin.Current);
                        imageInfo.ImageSize += (ulong)header.tracksize - 256;
                    }

                    readtracks++;
                }
            }

            DicConsole.DebugWriteLine("CPCDSK plugin", "Read {0} sectors",              sectors.Count);
            DicConsole.DebugWriteLine("CPCDSK plugin", "Read {0} tracks",               readtracks);
            DicConsole.DebugWriteLine("CPCDSK plugin", "All tracks are same size? {0}", allTracksSameSize);

            imageInfo.Application          = StringHandlers.CToString(header.creator);
            imageInfo.CreationTime         = imageFilter.GetCreationTime();
            imageInfo.LastModificationTime = imageFilter.GetLastWriteTime();
            imageInfo.MediaTitle           = Path.GetFileNameWithoutExtension(imageFilter.GetFilename());
            imageInfo.Sectors              = (ulong)sectors.Count;
            imageInfo.XmlMediaType         = XmlMediaType.BlockMedia;
            imageInfo.MediaType            = MediaType.CompactFloppy;
            imageInfo.ReadableSectorTags.Add(SectorTagType.FloppyAddressMark);

            // Debug writing full disk as raw
            /*
            FileStream foo = new FileStream(Path.GetFileNameWithoutExtension(imageFilter.GetFilename()) + ".bin", FileMode.Create);
            for(ulong i = 0; i < (ulong)sectors.Count; i++)
            {
                byte[] foob;
                sectors.TryGetValue(i, out foob);
                foo.Write(foob, 0, foob.Length);
            }
            foo.Close();
            */

            imageInfo.Cylinders       = header.tracks;
            imageInfo.Heads           = header.sides;
            imageInfo.SectorsPerTrack = (uint)(imageInfo.Sectors / (imageInfo.Cylinders * imageInfo.Heads));

            return true;
        }

        public byte[] ReadSector(ulong sectorAddress)
        {
            if(sectors.TryGetValue(sectorAddress, out byte[] sector)) return sector;

            throw new ArgumentOutOfRangeException(nameof(sectorAddress), $"Sector address {sectorAddress} not found");
        }

        public byte[] ReadSectors(ulong sectorAddress, uint length)
        {
            if(sectorAddress > imageInfo.Sectors - 1)
                throw new ArgumentOutOfRangeException(nameof(sectorAddress),
                                                      $"Sector address {sectorAddress} not found");

            if(sectorAddress + length > imageInfo.Sectors)
                throw new ArgumentOutOfRangeException(nameof(length), "Requested more sectors than available");

            MemoryStream ms = new MemoryStream();

            for(uint i = 0; i < length; i++)
            {
                byte[] sector = ReadSector(sectorAddress + i);
                ms.Write(sector, 0, sector.Length);
            }

            return ms.ToArray();
        }

        public byte[] ReadSectorTag(ulong sectorAddress, SectorTagType tag)
        {
            if(tag != SectorTagType.FloppyAddressMark)
                throw new FeatureUnsupportedImageException($"Tag {tag} not supported by image format");

            if(addressMarks.TryGetValue(sectorAddress, out byte[] addressMark)) return addressMark;

            throw new ArgumentOutOfRangeException(nameof(sectorAddress), "Sector address not found");
        }

        public byte[] ReadSectorsTag(ulong sectorAddress, uint length, SectorTagType tag)
        {
            if(tag != SectorTagType.FloppyAddressMark)
                throw new FeatureUnsupportedImageException($"Tag {tag} not supported by image format");

            if(sectorAddress > imageInfo.Sectors - 1)
                throw new ArgumentOutOfRangeException(nameof(sectorAddress),
                                                      $"Sector address {sectorAddress} not found");

            if(sectorAddress + length > imageInfo.Sectors)
                throw new ArgumentOutOfRangeException(nameof(length), "Requested more sectors than available");

            MemoryStream ms = new MemoryStream();

            for(uint i = 0; i < length; i++)
            {
                byte[] adddressMark = ReadSector(sectorAddress + i);
                ms.Write(adddressMark, 0, adddressMark.Length);
            }

            return ms.ToArray();
        }

        public byte[] ReadDiskTag(MediaTagType tag)
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

        static int SizeCodeToBytes(IBMSectorSizeCode code)
        {
            switch(code)
            {
                case IBMSectorSizeCode.EighthKilo:       return 128;
                case IBMSectorSizeCode.QuarterKilo:      return 256;
                case IBMSectorSizeCode.HalfKilo:         return 512;
                case IBMSectorSizeCode.Kilo:             return 1024;
                case IBMSectorSizeCode.TwiceKilo:        return 2048;
                case IBMSectorSizeCode.FriceKilo:        return 4096;
                case IBMSectorSizeCode.TwiceFriceKilo:   return 8192;
                case IBMSectorSizeCode.FricelyFriceKilo: return 16384;
                default:                                 return 0;
            }
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        struct CpcDiskInfo
        {
            /// <summary>
            ///     Magic number, "MV - CPCEMU Disk-File" in old files, "EXTENDED CPC DSK File" in extended ones
            /// </summary>
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 21)]
            public byte[] magic;
            /// <summary>
            ///     Second part of magic, should be "\r\nDisk-Info\r\n" in all, but some emulators write spaces instead.
            /// </summary>
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 13)]
            public byte[] magic2;
            /// <summary>
            ///     Creator application (can be null)
            /// </summary>
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 14)]
            public byte[] creator;
            /// <summary>
            ///     Tracks
            /// </summary>
            public byte tracks;
            /// <summary>
            ///     Sides
            /// </summary>
            public byte sides;
            /// <summary>
            ///     Size of a track including the 256 bytes header. Unused by extended format, as this format includes a table in the
            ///     next field
            /// </summary>
            public ushort tracksize;
            /// <summary>
            ///     Size of each track in the extended format. 0 indicates track is not formatted and not present in image.
            /// </summary>
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 204)]
            public byte[] tracksizeTable;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        struct CpcTrackInfo
        {
            /// <summary>
            ///     Magic number, "Track-Info\r\n"
            /// </summary>
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 10)]
            public byte[] magic;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)]
            public byte[] carriageReturn;
            /// <summary>
            ///     Padding
            /// </summary>
            public uint padding;
            /// <summary>
            ///     Track number
            /// </summary>
            public byte track;
            /// <summary>
            ///     Side number
            /// </summary>
            public byte side;
            /// <summary>
            ///     Controller data rate
            /// </summary>
            public byte dataRate;
            /// <summary>
            ///     Recording mode
            /// </summary>
            public byte recordingMode;
            /// <summary>
            ///     Bytes per sector
            /// </summary>
            public IBMSectorSizeCode bps;
            /// <summary>
            ///     How many sectors in this track
            /// </summary>
            public byte sectors;
            /// <summary>
            ///     GAP#3
            /// </summary>
            public byte gap3;
            /// <summary>
            ///     Filler
            /// </summary>
            public byte filler;
            /// <summary>
            ///     Informatino for up to 32 sectors
            /// </summary>
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
            public CpcSectorInfo[] sectorsInfo;
        }

        /// <summary>
        ///     Sector information
        /// </summary>
        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        struct CpcSectorInfo
        {
            /// <summary>
            ///     Track number from address mark
            /// </summary>
            public byte track;
            /// <summary>
            ///     Side number from address mark
            /// </summary>
            public byte side;
            /// <summary>
            ///     Sector ID from address mark
            /// </summary>
            public byte id;
            /// <summary>
            ///     Sector size from address mark
            /// </summary>
            public IBMSectorSizeCode size;
            /// <summary>
            ///     ST1 register from controller
            /// </summary>
            public byte st1;
            /// <summary>
            ///     ST2 register from controller
            /// </summary>
            public byte st2;
            /// <summary>
            ///     Length in bytes of this sector size. If it is bigger than expected sector size, it's a weak sector read several
            ///     times.
            /// </summary>
            public ushort len;
        }
    }
}