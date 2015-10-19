﻿// /***************************************************************************
// The Disc Image Chef
// ----------------------------------------------------------------------------
//
// Filename       : ATIP.cs
// Version        : 1.0
// Author(s)      : Natalia Portillo
//
// Component      : Component
//
// Revision       : $Revision$
// Last change by : $Author$
// Date           : $Date$
//
// --[ Description ] ----------------------------------------------------------
//
// Description
//
// --[ License ] --------------------------------------------------------------
//
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as
//     published by the Free Software Foundation, either version 3 of the
//     License, or (at your option) any later version.
//
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the GNU General Public License
//     along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// ----------------------------------------------------------------------------
// Copyright (C) 2011-2015 Claunia.com
// ****************************************************************************/
// //$Id$
using System;
using DiscImageChef.Console;
using System.Text;

namespace DiscImageChef.Decoders.CD
{
    /// <summary>
    /// Information from the following standards:
    /// ANSI X3.304-1997
    /// T10/1048-D revision 9.0
    /// T10/1048-D revision 10a
    /// T10/1228-D revision 7.0c
    /// T10/1228-D revision 11a
    /// T10/1363-D revision 10g
    /// T10/1545-D revision 1d
    /// T10/1545-D revision 5
    /// T10/1545-D revision 5a
    /// T10/1675-D revision 2c
    /// T10/1675-D revision 4
    /// T10/1836-D revision 2g
    /// </summary>
    public static class ATIP
    {
        public struct CDATIP
        {
            /// <summary>
            /// Bytes 1 to 0
            /// Total size of returned session information minus this field
            /// </summary>
            public UInt16 DataLength;
            /// <summary>
            /// Byte 2
            /// Reserved
            /// </summary>
            public byte Reserved1;
            /// <summary>
            /// Byte 3
            /// Reserved
            /// </summary>
            public byte Reserved2;
            /// <summary>
            /// Byte 4, bits 7 to 4
            /// Indicative target writing power
            /// </summary>
            public byte ITWP;
            /// <summary>
            /// Byte 4, bit 3
            /// Set if DDCD
            /// </summary>
            public bool DDCD;
            /// <summary>
            /// Byte 4, bits 2 to 0
            /// Reference speed
            /// </summary>
            public byte ReferenceSpeed;
            /// <summary>
            /// Byte 5, bit 7
            /// Always unset
            /// </summary>
            public bool AlwaysZero;
            /// <summary>
            /// Byte 5, bit 6
            /// Unrestricted media
            /// </summary>
            public bool URU;
            /// <summary>
            /// Byte 5, bits 5 to 0
            /// Reserved
            /// </summary>
            public byte Reserved3;
            /// <summary>
            /// Byte 6, bit 7
            /// Always set
            /// </summary>
            public bool AlwaysOne;
            /// <summary>
            /// Byte 6, bit 6
            /// Set if rewritable (CD-RW or DDCD-RW)
            /// </summary>
            public bool DiscType;
            /// <summary>
            /// Byte 6, bits 5 to 3
            /// Disc subtype
            /// </summary>
            public byte DiscSubType;
            /// <summary>
            /// Byte 6, bit 2
            /// A1 values are valid
            /// </summary>
            public bool A1Valid;
            /// <summary>
            /// Byte 6, bit 1
            /// A2 values are valid
            /// </summary>
            public bool A2Valid;
            /// <summary>
            /// Byte 6, bit 0
            /// A3 values are valid
            /// </summary>
            public bool A3Valid;
            /// <summary>
            /// Byte 7
            /// Reserved
            /// </summary>
            public byte Reserved4;
            /// <summary>
            /// Byte 8
            /// ATIP Start time of Lead-In (Minute)
            /// </summary>
            public byte LeadInStartMin;
            /// <summary>
            /// Byte 9
            /// ATIP Start time of Lead-In (Second)
            /// </summary>
            public byte LeadInStartSec;
            /// <summary>
            /// Byte 10
            /// ATIP Start time of Lead-In (Frame)
            /// </summary>
            public byte LeadInStartFrame;
            /// <summary>
            /// Byte 11
            /// Reserved
            /// </summary>
            public byte Reserved5;
            /// <summary>
            /// Byte 12
            /// ATIP Last possible start time of Lead-Out (Minute)
            /// </summary>
            public byte LeadOutStartMin;
            /// <summary>
            /// Byte 13
            /// ATIP Last possible start time of Lead-Out (Second)
            /// </summary>
            public byte LeadOutStartSec;
            /// <summary>
            /// Byte 14
            /// ATIP Last possible start time of Lead-Out (Frame)
            /// </summary>
            public byte LeadOutStartFrame;
            /// <summary>
            /// Byte 15
            /// Reserved
            /// </summary>
            public byte Reserved6;
            /// <summary>
            /// Bytes 16 to 18
            /// A1 values
            /// </summary>
            public byte[] A1Values;
            /// <summary>
            /// Byte 19
            /// Reserved
            /// </summary>
            public byte Reserved7;
            /// <summary>
            /// Bytes 20 to 22
            /// A2 values
            /// </summary>
            public byte[] A2Values;
            /// <summary>
            /// Byte 23
            /// Reserved
            /// </summary>
            public byte Reserved8;
            /// <summary>
            /// Bytes 24 to 26
            /// A3 values
            /// </summary>
            public byte[] A3Values;
            /// <summary>
            /// Byte 27
            /// Reserved
            /// </summary>
            public byte Reserved9;
            /// <summary>
            /// Bytes 28 to 30
            /// S4 values
            /// </summary>
            public byte[] S4Values;
            /// <summary>
            /// Byte 31
            /// Reserved
            /// </summary>
            public byte Reserved10;
        }

        public static CDATIP? Decode(byte[] CDATIPResponse)
        {
            if (CDATIPResponse == null)
                return null;

            CDATIP decoded = new CDATIP();

            BigEndianBitConverter.IsLittleEndian = BitConverter.IsLittleEndian;

            if (CDATIPResponse.Length != 32)
            {
                DicConsole.DebugWriteLine("CD ATIP decoder", "Expected CD ATIP size (32 bytes) is not received size ({0} bytes), not decoding", CDATIPResponse.Length);
                return null;
            }

            decoded.DataLength = BigEndianBitConverter.ToUInt16(CDATIPResponse, 0);
            decoded.Reserved1 = CDATIPResponse[2];
            decoded.Reserved2 = CDATIPResponse[3];
            decoded.ITWP = (byte)((CDATIPResponse[4] & 0xF0) >> 4);
            decoded.DDCD = Convert.ToBoolean(CDATIPResponse[4] & 0x08);
            decoded.ReferenceSpeed = (byte)(CDATIPResponse[4] & 0x07);
            decoded.AlwaysZero = Convert.ToBoolean(CDATIPResponse[5] & 0x80);
            decoded.URU = Convert.ToBoolean(CDATIPResponse[5] & 0x40);
            decoded.Reserved3 = (byte)(CDATIPResponse[5] & 0x3F);

            decoded.AlwaysOne = Convert.ToBoolean(CDATIPResponse[6] & 0x80);
            decoded.DiscType = Convert.ToBoolean(CDATIPResponse[6] & 0x40);
            decoded.DiscSubType = (byte)((CDATIPResponse[6] & 0x38) >> 3);
            decoded.A1Valid = Convert.ToBoolean(CDATIPResponse[6] & 0x04);
            decoded.A2Valid = Convert.ToBoolean(CDATIPResponse[6] & 0x02);
            decoded.A3Valid = Convert.ToBoolean(CDATIPResponse[6] & 0x01);

            decoded.Reserved4 = CDATIPResponse[7];
            decoded.LeadInStartMin = CDATIPResponse[8];
            decoded.LeadInStartSec = CDATIPResponse[9];
            decoded.LeadInStartFrame = CDATIPResponse[10];
            decoded.Reserved5 = CDATIPResponse[11];
            decoded.LeadOutStartMin = CDATIPResponse[12];
            decoded.LeadOutStartSec = CDATIPResponse[13];
            decoded.LeadOutStartFrame = CDATIPResponse[14];
            decoded.Reserved6 = CDATIPResponse[15];

            decoded.A1Values = new byte[3];
            decoded.A2Values = new byte[3];
            decoded.A3Values = new byte[3];
            decoded.S4Values = new byte[3];

            Array.Copy(CDATIPResponse, 16, decoded.A1Values, 0, 3);
            Array.Copy(CDATIPResponse, 20, decoded.A1Values, 0, 3);
            Array.Copy(CDATIPResponse, 24, decoded.A1Values, 0, 3);
            Array.Copy(CDATIPResponse, 28, decoded.A1Values, 0, 3);

            decoded.Reserved7 = CDATIPResponse[19];
            decoded.Reserved8 = CDATIPResponse[23];
            decoded.Reserved9 = CDATIPResponse[27];
            decoded.Reserved10 = CDATIPResponse[31];

            return decoded;
        }

        public static string Prettify(CDATIP? CDATIPResponse)
        {
            if (CDATIPResponse == null)
                return null;

            CDATIP response = CDATIPResponse.Value;

            StringBuilder sb = new StringBuilder();

            if (response.DDCD)
            {
                sb.AppendFormat("Indicative Target Writing Power: 0x{0:X2}", response.ITWP).AppendLine();
                if (response.DiscType)
                    sb.AppendLine("Disc is DDCD-RW");
                else
                    sb.AppendLine("Disc is DDCD-R");
                switch (response.ReferenceSpeed)
                {
                    case 2:
                        sb.AppendLine("Reference speed is 4x");
                        break;
                    case 3:
                        sb.AppendLine("Reference speed is 8x");
                        break;
                    default:
                        sb.AppendFormat("Reference speed set is unknown: {0}", response.ReferenceSpeed).AppendLine();
                        break;
                }
                sb.AppendFormat("ATIP Start time of Lead-in: 0x{0:X6}", (response.LeadInStartMin << 16) + (response.LeadInStartSec << 8) + response.LeadInStartFrame).AppendLine();
                sb.AppendFormat("ATIP Last possible start time of Lead-out: 0x{0:X6}", (response.LeadOutStartMin << 16) + (response.LeadOutStartSec << 8) + response.LeadOutStartFrame).AppendLine();
                sb.AppendFormat("S4 value: 0x{0:X6}", (response.S4Values[0] << 16) + (response.S4Values[1] << 8) + response.S4Values[2]).AppendLine();
            }
            else
            {
                sb.AppendFormat("Indicative Target Writing Power: 0x{0:X2}", response.ITWP & 0x07).AppendLine();
                if (response.DiscType)
                {
                    switch (response.DiscSubType)
                    {
                        case 0:
                            sb.AppendLine("Disc is CD-RW");
                            break;
                        case 1:
                            sb.AppendLine("Disc is High-Speed CD-RW");
                            break;
                        default:
                            sb.AppendFormat("Unknown CD-RW disc subtype: {0}", response.DiscSubType).AppendLine();
                            break;
                    }
                    switch (response.ReferenceSpeed)
                    {
                        case 1:
                            sb.AppendLine("Reference speed is 2x");
                            break;
                        default:
                            sb.AppendFormat("Reference speed set is unknown: {0}", response.ReferenceSpeed).AppendLine();
                            break;
                    }
                }
                else
                {
                    sb.AppendLine("Disc is CD-R");
                    switch (response.DiscSubType)
                    {
                        default:
                            sb.AppendFormat("Unknown CD-R disc subtype: {0}", response.DiscSubType).AppendLine();
                            break;
                    }
                }

                if (response.URU)
                    sb.AppendLine("Disc use is unrestricted");
                else
                    sb.AppendLine("Disc use is restricted");

                sb.AppendFormat("ATIP Start time of Lead-in: {0:X2}:{1:X2}:{2:X2}", response.LeadInStartMin, response.LeadInStartSec, response.LeadInStartFrame).AppendLine();
                sb.AppendFormat("ATIP Last possible start time of Lead-out: {0:X2}:{1:X2}:{2:X2}", response.LeadOutStartMin, response.LeadOutStartSec, response.LeadOutStartFrame).AppendLine();
                if(response.A1Valid)
                    sb.AppendFormat("A1 value: 0x{0:X6}", (response.A1Values[0] << 16) + (response.A1Values[1] << 8) + response.A1Values[2]).AppendLine();
                if(response.A2Valid)
                    sb.AppendFormat("A2 value: 0x{0:X6}", (response.A2Values[0] << 16) + (response.A2Values[1] << 8) + response.A2Values[2]).AppendLine();
                if(response.A3Valid)
                    sb.AppendFormat("A3 value: 0x{0:X6}", (response.A3Values[0] << 16) + (response.A3Values[1] << 8) + response.A3Values[2]).AppendLine();
                sb.AppendFormat("S4 value: 0x{0:X6}", (response.S4Values[0] << 16) + (response.S4Values[1] << 8) + response.S4Values[2]).AppendLine();
            }

            return sb.ToString();
        }

        public static string Prettify(byte[] CDATIPResponse)
        {
            CDATIP? decoded = Decode(CDATIPResponse);
            return Prettify(decoded);
        }
    }
}
