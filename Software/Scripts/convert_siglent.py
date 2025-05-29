#!/usr/bin/env python3

"""

Converts LibreCAL Touchstone files to a format that a Siglent VNA can use.

Usage:

$ python3 convert_siglent.py /Volumes/LIBRECAL_R /Volumes/LIBRECAL_RW

(or your paths or drive letters, as appropriate)

This will create a "siglent" directory in your LIBRECAL_RW volume.  Then,
plug in your LibreCAL while holding down the FUNCTION button, which will
activate Siglent eCal compatibility mode, and calibrate your VNA as if you
were using a SEM5000A (or other Siglent eCal).

Since I only have a two port VNA, I have only tested calibrating with port 1
and port 2 on my LibreCAL connected to SVA1032X ports 1 and 2.  If you have
a fancier VNA, please let me know if it works.

"""

__author__ = "Joshua Wise"
__copyright__ = "Copyright (c) 2025 Accelerated Tech, Inc."
__license__ = "MIT"

import io
from pathlib import Path
import sys
import numpy as np
from zipfile import ZipFile, ZIP_DEFLATED
import zipfile
import struct
import hashlib
from time import strftime, gmtime

if len(sys.argv) != 3:
    print(f"usage: {sys.argv[0]} /Volumes/LIBRECAL_R /Volumes/LIBRECAL_RW")
    sys.exit(1)

indir = Path(sys.argv[1])
outdir = Path(sys.argv[2])

def say_open(name, *args):
    print(f"reading {name}", file=sys.stderr)
    return open(name, *args)

zipbuf = io.BytesIO()
with ZipFile(zipbuf, "w", compression=ZIP_DEFLATED, compresslevel=9) as zf, \
     zf.open("Factory.csv", "w") as outf_b, \
     io.TextIOWrapper(outf_b) as outf:
    with say_open(indir / "info.txt", "r") as inf:
        for l in inf.readlines():
            outf.write(f"! {l.strip()}\n")
    outf.write("#HZ,A,B,C,D,T_AB,T_AC,T_AD,T_BC,T_BD,T_CD\n")
    
    freqs = [] # will get populated below
    axes = [freqs]
    
    for port in ["1", "2", "3", "4"]:
        for col in "OPEN", "SHORT", "LOAD":
            with say_open(indir / f"P{port}_{col}.s1p", "r") as inf:
                ax_r, ax_i = [], []
                for l in inf.readlines():
                    l = l.strip()
                    if l.startswith("!") or l.startswith("#"):
                        continue
                    freq,r,i = [float(x) for x in l.split(" ")]
                    freqs.append(freq * 1e9)
                    ax_r.append(r)
                    ax_i.append(i)
                axes.append(ax_r)
                axes.append(ax_i)
                freqs = [] # hope they're all the same!
        axes.append([0 for _ in axes[0]])
        axes.append([0 for _ in axes[0]])

    for port in ["12", "13", "14", "23", "24", "34"]:
        with say_open(indir / f"P{port}_THROUGH.s2p", "r") as inf:
            snps = [ [] for _ in range(8) ]
            for l in inf.readlines():
                if l.startswith("!") or l.startswith("#"):
                    continue
                snp_line = [float(x) for x in l.split(" ")][1:]
                for k,v in enumerate([float(x) for x in l.split(" ")][1:]):
                    snps[k].append(v)
            for l in snps:
                axes.append(l)

    ar = np.vstack(axes, dtype=np.float64).transpose()
    print(f"compressing {ar.shape[0]} points with {ar.shape[1]} columns")
    np.savetxt(outf, ar, delimiter=",")

ziphash = hashlib.md5(zipbuf.getvalue()).hexdigest()

caldate = gmtime((indir / "P1_OPEN.s1p").stat().st_ctime)
info = { k: v for k,v in (line.split(": ") for line in open(indir / "info.txt", "r").read().split("\n")) }

VENDOR = "LibreCAL"
PRODUCT = "LibreCAL"
SERIAL = info['Serial']
BYTE_0x4E = 0 # What are these?
BYTE_0x4F = 0
header = struct.pack("30s16s16s16sBB64s", b"", VENDOR.encode(), PRODUCT.encode(), SERIAL.encode(), BYTE_0x4E, BYTE_0x4F, b"")

header += f"""Connector:SMA
Module:Factory
Desc:{ziphash}
Frequency:9000,8500000000,1201
Data:0,{len(zipbuf.getvalue())},{ziphash}
Date:{strftime('%Y-%m-%d', caldate)}
""".encode()

# There can also be a "Extension: a,b,c,d" (what is that?) after Module:,
# and there can also be other Modules; we don't handle these for now. 
# Connector: must always come before Module:.

header += b"\x00" * (1024 - len(header))

(outdir / "siglent").mkdir(exist_ok = True)

zipname = outdir / "siglent/data0.zip"
print(f"writing {zipname}", file=sys.stderr)
with open(zipname, "wb") as f:
    f.write(zipbuf.getvalue())

datname = outdir / "siglent/info.dat"
print(f"writing {datname}", file=sys.stderr)
with open(datname, "wb") as f:
    f.write(header)
