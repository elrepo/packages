/*
 *  nvidia-detect_x86.h - PCI device_ids for NVIDIA graphics cards
 *
 *  Copyright (C) 2013-2018 Philip J Perry <phil@elrepo.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _NVIDIA_DETECT_X86_H
#define _NVIDIA_DETECT_X86_H

typedef unsigned short u_int16_t;

/* PCI device_ids supported by the 96xx legacy driver */
static const u_int16_t nv_96xx_pci_ids[] = {
	0x0110, 0x0111, 0x0112, 0x0113, 0x0170, 0x0171, 0x0172, 0x0173, 0x0174, 0x0175,
	0x0176, 0x0177, 0x0178, 0x0179, 0x017A, 0x017C, 0x017D, 0x0181, 0x0182, 0x0183,
	0x0185, 0x0188, 0x018A, 0x018B, 0x018C, 0x01A0, 0x01F0, 0x0200, 0x0201, 0x0202,
	0x0203, 0x0250, 0x0251, 0x0253, 0x0258, 0x0259, 0x025B, 0x0280, 0x0281, 0x0282,
	0x0286, 0x0288, 0x0289, 0x028C,
};

/* PCI device_ids supported by the 173xx legacy driver */
static const u_int16_t nv_173xx_pci_ids[] = {
	0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x0301, 0x0302, 0x0308, 0x0309, 0x0311,
	0x0312, 0x0314, 0x031A, 0x031B, 0x031C, 0x0320, 0x0321, 0x0322, 0x0323, 0x0324,
	0x0325, 0x0326, 0x0327, 0x0328, 0x032A, 0x032B, 0x032C, 0x032D, 0x0330, 0x0331,
	0x0332, 0x0333, 0x0334, 0x0338, 0x033F, 0x0341, 0x0342, 0x0343, 0x0344, 0x0347,
	0x0348, 0x034C, 0x034E,
};

/* PCI device_ids supported by the 304xx legacy driver */
static const u_int16_t nv_304xx_pci_ids[] = {
	0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x004E,
	0x0090, 0x0091, 0x0092, 0x0093, 0x0095, 0x0098, 0x0099, 0x009D, 0x00C0, 0x00C1,
	0x00C2, 0x00C3, 0x00C8, 0x00C9, 0x00CC, 0x00CD, 0x00CE, 0x00F1, 0x00F2, 0x00F3,
	0x00F4, 0x00F5, 0x00F6, 0x00F8, 0x00F9, 0x0140, 0x0141, 0x0142, 0x0143, 0x0144,
	0x0145, 0x0146, 0x0147, 0x0148, 0x0149, 0x014A, 0x014C, 0x014D, 0x014E, 0x014F,
	0x0160, 0x0161, 0x0162, 0x0163, 0x0164, 0x0165, 0x0166, 0x0167, 0x0168, 0x0169,
	0x016A, 0x01D0, 0x01D1, 0x01D2, 0x01D3, 0x01D6, 0x01D7, 0x01D8, 0x01DA, 0x01DB,
	0x01DC, 0x01DD, 0x01DE, 0x01DF, 0x0211, 0x0212, 0x0215, 0x0218, 0x0221, 0x0222,
	0x0240, 0x0241, 0x0242, 0x0244, 0x0245, 0x0247, 0x0290, 0x0291, 0x0292, 0x0293,
	0x0294, 0x0295, 0x0297, 0x0298, 0x0299, 0x029A, 0x029B, 0x029C, 0x029D, 0x029E,
	0x029F, 0x02E0, 0x02E1, 0x02E2, 0x02E3, 0x02E4, 0x038B, 0x0390, 0x0391, 0x0392,
	0x0393, 0x0394, 0x0395, 0x0397, 0x0398, 0x0399, 0x039C, 0x039E, 0x03D0, 0x03D1,
	0x03D2, 0x03D5, 0x03D6, 0x0531, 0x0533, 0x053A, 0x053B, 0x053E, 0x07E0, 0x07E1,
	0x07E2, 0x07E3, 0x07E5,
};

/* PCI device_ids supported by the 340xx legacy driver */
static const u_int16_t nv_340xx_pci_ids[] = {
	0x0191, 0x0193, 0x0194, 0x0197, 0x019D, 0x019E, 0x0400, 0x0401, 0x0402, 0x0403,
	0x0404, 0x0405, 0x0406, 0x0407, 0x0408, 0x0409, 0x040A, 0x040B, 0x040C, 0x040D,
	0x040E, 0x040F, 0x0410, 0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426,
	0x0427, 0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F, 0x05E0,
	0x05E1, 0x05E2, 0x05E3, 0x05E6, 0x05E7, 0x05EA, 0x05EB, 0x05ED, 0x05F8, 0x05F9,
	0x05FD, 0x05FE, 0x05FF, 0x0600, 0x0601, 0x0602, 0x0603, 0x0604, 0x0605, 0x0606,
	0x0607, 0x0608, 0x0609, 0x060A, 0x060B, 0x060C, 0x060D, 0x060F, 0x0610, 0x0611,
	0x0612, 0x0613, 0x0614, 0x0615, 0x0617, 0x0618, 0x0619, 0x061A, 0x061B, 0x061C,
	0x061D, 0x061E, 0x061F, 0x0621, 0x0622, 0x0623, 0x0625, 0x0626, 0x0627, 0x0628,
	0x062A, 0x062B, 0x062C, 0x062D, 0x062E, 0x0630, 0x0631, 0x0632, 0x0635, 0x0637,
	0x0638, 0x063A, 0x0640, 0x0641, 0x0643, 0x0644, 0x0645, 0x0646, 0x0647, 0x0648,
	0x0649, 0x064A, 0x064B, 0x064C, 0x0651, 0x0652, 0x0653, 0x0654, 0x0655, 0x0656,
	0x0658, 0x0659, 0x065A, 0x065B, 0x065C, 0x06E0, 0x06E1, 0x06E2, 0x06E3, 0x06E4,
	0x06E5, 0x06E6, 0x06E7, 0x06E8, 0x06E9, 0x06EA, 0x06EB, 0x06EC, 0x06EF, 0x06F1,
	0x06F8, 0x06F9, 0x06FA, 0x06FB, 0x06FD, 0x06FF, 0x0840, 0x0844, 0x0845, 0x0846,
	0x0847, 0x0848, 0x0849, 0x084A, 0x084B, 0x084C, 0x084D, 0x084F, 0x0860, 0x0861,
	0x0862, 0x0863, 0x0864, 0x0865, 0x0866, 0x0867, 0x0868, 0x0869, 0x086A, 0x086C,
	0x086D, 0x086E, 0x086F, 0x0870, 0x0871, 0x0872, 0x0873, 0x0874, 0x0876, 0x087A,
	0x087D, 0x087E, 0x087F, 0x08A0, 0x08A2, 0x08A3, 0x08A4, 0x08A5, 0x0A20, 0x0A22,
	0x0A23, 0x0A26, 0x0A27, 0x0A28, 0x0A29, 0x0A2A, 0x0A2B, 0x0A2C, 0x0A2D, 0x0A32,
	0x0A34, 0x0A35, 0x0A38, 0x0A3C, 0x0A60, 0x0A62, 0x0A63, 0x0A64, 0x0A65, 0x0A66,
	0x0A67, 0x0A68, 0x0A69, 0x0A6A, 0x0A6C, 0x0A6E, 0x0A6F, 0x0A70, 0x0A71, 0x0A72,
	0x0A73, 0x0A74, 0x0A75, 0x0A76, 0x0A78, 0x0A7A, 0x0A7C, 0x0CA0, 0x0CA2, 0x0CA3,
	0x0CA4, 0x0CA5, 0x0CA7, 0x0CA8, 0x0CA9, 0x0CAC, 0x0CAF, 0x0CB0, 0x0CB1, 0x0CBC,
	0x10C0, 0x10C3, 0x10C5, 0x10D8,
};

/* PCI device_ids supported by the 367xx legacy driver */
static const u_int16_t nv_367xx_pci_ids[] = {
	0x0FEF, 0x0FF2, 0x11BF,
};

/* PCI device_ids supported by the 390xx legacy driver */
static const u_int16_t nv_390xx_pci_ids[] = {
	0x06C0, 0x06C4, 0x06CA, 0x06CD, 0x0DC0, 0x0DC4, 0x0DC5, 0x0DC6, 0x0DCD, 0x0DCE,
	0x0DD1, 0x0DD2, 0x0DD3, 0x0DD6, 0x0DE0, 0x0DE1, 0x0DE2, 0x0DE3, 0x0DE4, 0x0DE5,
	0x0DE7, 0x0DE8, 0x0DE9, 0x0DEA, 0x0DEB, 0x0DEC, 0x0DED, 0x0DEE, 0x0DF0, 0x0DF1,
	0x0DF2, 0x0DF3, 0x0DF4, 0x0DF5, 0x0DF6, 0x0DF7, 0x0E22, 0x0E23, 0x0E24, 0x0E30,
	0x0E31, 0x0F00, 0x0F01, 0x0F02, 0x0FC0, 0x0FC1, 0x0FC2, 0x0FC6, 0x0FC8, 0x0FCD,
	0x0FCE, 0x0FD1, 0x0FD2, 0x0FD3, 0x0FD4, 0x0FD5, 0x0FD8, 0x0FD9, 0x0FDF, 0x0FE0,
	0x0FE1, 0x0FE2, 0x0FE3, 0x0FE4, 0x0FE9, 0x0FEA, 0x1001, 0x1004, 0x1005, 0x1007,
	0x1008, 0x100A, 0x100C, 0x1040, 0x1042, 0x1048, 0x1049, 0x104A, 0x104B, 0x104C,
	0x1050, 0x1051, 0x1052, 0x1054, 0x1055, 0x1058, 0x1059, 0x105A, 0x105B, 0x1080,
	0x1081, 0x1082, 0x1084, 0x1086, 0x1087, 0x1088, 0x1089, 0x108B, 0x1140, 0x1180,
	0x1183, 0x1184, 0x1185, 0x1187, 0x1188, 0x1189, 0x118E, 0x1193, 0x1195, 0x1198,
	0x1199, 0x119A, 0x119D, 0x119E, 0x119F, 0x11A0, 0x11A1, 0x11A2, 0x11A3, 0x11A7,
	0x11C0, 0x11C2, 0x11C3, 0x11C4, 0x11C6, 0x11C8, 0x11E0, 0x11E1, 0x11E2, 0x11E3,
	0x1200, 0x1201, 0x1203, 0x1205, 0x1206, 0x1207, 0x1208, 0x1210, 0x1211, 0x1212,
	0x1213, 0x1241, 0x1243, 0x1244, 0x1245, 0x1246, 0x1247, 0x1248, 0x1249, 0x124B,
	0x124D, 0x1251, 0x1280, 0x1281, 0x1282, 0x1284, 0x1286, 0x1287, 0x1290, 0x1291,
	0x1292, 0x1293, 0x1296, 0x1298, 0x1340, 0x1341, 0x1380, 0x1381, 0x1382, 0x1390,
	0x1391, 0x1392, 0x13C0, 0x13C2, 0x06D8, 0x06D9, 0x06DA, 0x06DC, 0x06DD, 0x0DD8,
	0x0DDA, 0x0DF8, 0x0DF9, 0x0DFA, 0x0E3A, 0x0E3B, 0x0FF3, 0x0FF6, 0x0FF8, 0x0FF9,
	0x0FFA, 0x0FFB, 0x0FFC, 0x0FFE, 0x0FFF, 0x103A, 0x103C, 0x109A, 0x109B, 0x11B4,
	0x11B6, 0x11B7, 0x11B8, 0x11BA, 0x11BC, 0x11BD, 0x11BE, 0x11FA, 0x11FC, 0x12B9,
	0x12BA, 0x13BA, 0x13BB, 0x0DEF, 0x0DFC, 0x0FFD, 0x1056, 0x1057, 0x107C, 0x107D,
	0x06D1, 0x06D2, 0x06DE, 0x06DF, 0x1021, 0x1022, 0x1023, 0x1024, 0x1026, 0x1027,
	0x1028, 0x1029, 0x1091, 0x1094, 0x1096, 0x118F, 0x118A, 0x1BB6, 0x1BB7, 0x1BB8,
	0x0FEC, 0x1194, 0x11C5, 0x1288, 0x1295, 0x1393, 0x0FC9, 0x13D7, 0x13D8, 0x137A,
	0x13B3, 0x13D9, 0x1401, 0x1289, 0x1299, 0x1346, 0x1347, 0x139A, 0x139B, 0x13BC,
	0x17C2, 0x17F0, 0x102A, 0x11CB, 0x1344, 0x137D, 0x1398, 0x1617, 0x1618, 0x1619,
	0x17C8, 0x102D, 0x129A, 0x139C, 0x13F0, 0x13F1, 0x1402, 0x13DA, 0x13F2, 0x13F3,
	0x1407, 0x161A, 0x0FEE, 0x1399, 0x13B0, 0x13B1, 0x13B2, 0x13B9, 0x13F8, 0x13F9,
	0x13FA, 0x17FD, 0x0F03, 0x128B, 0x1348, 0x1349, 0x134B, 0x134D, 0x1427, 0x1431,
	0x1667, 0x134E, 0x134F, 0x179C, 0x17F1, 0x0FED, 0x139D, 0x13FB, 0x1430, 0x1406,
	0x1B80, 0x1B81, 0x1B00, 0x1BA0, 0x1BA1, 0x1BE0, 0x1BE1, 0x1C02, 0x1C03, 0x1C20,
	0x1C60, 0x13B4, 0x15F7, 0x15F8, 0x15F9, 0x1B30, 0x1B38, 0x1B84, 0x1BB0, 0x1BB3,
	0x1C81, 0x1C82, 0x13B6, 0x1436, 0x15F0, 0x1BB1, 0x1C21, 0x1C22, 0x1C30, 0x1C61,
	0x1C62, 0x1C8C, 0x1C8D, 0x1CB1, 0x1CB2, 0x1CB3, 0x137B, 0x1B02, 0x1B06, 0x1B87,
	0x1C07, 0x1D01, 0x1D10, 0x1D12, 0x1BB4, 0x1BB5, 0x1BC7, 0x1C09, 0x1DB1, 0x1DB4,
	0x174D, 0x174E, 0x1D33, 0x1B82, 0x1C04, 0x1C06, 0x1CB6, 0x1D81, 0x1DB5, 0x1DB6,
	0x1DB7, 0x1DBA, 0x1BB9, 0x1BBB, 0x1CBA, 0x1CBB, 0x1CBC, 0x1DB3, 0x1B83, 0x1C83,
	0x1C8F,
};

#endif	/* _NVIDIA_DETECT_X86_H */