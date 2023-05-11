#!/usr/bin/env python3
import sys

val = int(sys.argv[1])

regs = {
    "SYS_STATUS_IRQS":          0x00000001,
    "SYS_STATUS_CPLOCK":        0x00000002,
    "SYS_STATUS_ESYNCR":        0x00000004,
    "SYS_STATUS_AAT":           0x00000008,
    "SYS_STATUS_TXFRB":         0x00000010,
    "SYS_STATUS_TXPRS":         0x00000020,
    "SYS_STATUS_TXPHS":         0x00000040,
    "SYS_STATUS_TXFRS":         0x00000080,
    "SYS_STATUS_RXPRD":         0x00000100,
    "SYS_STATUS_RXSFDD":        0x00000200,
    "SYS_STATUS_LDEDONE":       0x00000400,
    "SYS_STATUS_RXPHD":         0x00000800,
    "SYS_STATUS_RXPHE":         0x00001000,
    "SYS_STATUS_RXDFR":         0x00002000,
    "SYS_STATUS_RXFCG":         0x00004000,
    "SYS_STATUS_RXFCE":         0x00008000,
    "SYS_STATUS_RXRFSL":        0x00010000,
    "SYS_STATUS_RXRFTO":        0x00020000,
    "SYS_STATUS_LDEERR":        0x00040000,
    "SYS_STATUS_reserved":      0x00080000,
    "SYS_STATUS_RXOVRR":        0x00100000,
    "SYS_STATUS_RXPTO":         0x00200000,
    "SYS_STATUS_GPIOIRQ":       0x00400000,
    "SYS_STATUS_SLP2INIT":      0x00800000,
    "SYS_STATUS_RFPLL_LL":      0x01000000,
    "SYS_STATUS_CLKPLL_LL":     0x02000000,
    "SYS_STATUS_RXSFDTO":       0x04000000,
    "SYS_STATUS_HPDWARN":       0x08000000,
    "SYS_STATUS_TXBERR":        0x10000000,
    "SYS_STATUS_AFFREJ":        0x20000000,
    "SYS_STATUS_HSRBP":         0x40000000,
    "SYS_STATUS_ICRBP":         0x80000000,
}

for k, v in regs.items():
    if val & v:
        print(k)
