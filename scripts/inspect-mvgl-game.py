#!/usr/bin/env python3
"""Read-only DSTS executable inspection; static signatures never attest launch."""
import argparse, hashlib, json, mmap, struct
parser = argparse.ArgumentParser()
parser.add_argument("executable")
args = parser.parse_args()
patterns = {
    "read": "41 54 41 56 41 57 48 81 EC B0 01 00 00",
    "size": "48 89 5C 24 ?? 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 ?? ?? ?? ?? 48 81 EC C0 02 00 00",
}
with open(args.executable, "rb") as stream:
    with mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as image:
        if image[:2] != b"MZ": raise SystemExit("Not a Windows executable")
        pe = struct.unpack_from("<I", image, 60)[0]
        if image[pe:pe+4] != b"PE\0\0": raise SystemExit("Invalid PE signature")
        machine, sections = struct.unpack_from("<HH", image, pe+4)
        if machine != 0x8664: raise SystemExit("Expected Windows x64")
        optional = struct.unpack_from("<H", image, pe+20)[0]
        section_table = pe+24+optional
        matches = {name: [] for name in patterns}
        for index in range(sections):
            header = section_table+index*40
            virtual_size, rva, size, offset = struct.unpack_from("<IIII", image, header+8)
            flags = struct.unpack_from("<I", image, header+36)[0]
            if not flags & 0x20000000: continue
            if offset+size > len(image): raise SystemExit("Truncated executable section")
            data = image[offset:offset+min(size,virtual_size)]
            for name, pattern in patterns.items():
                tokens = pattern.split()
                # Search a fixed leading run first to avoid scanning a large game in Python.
                prefix = bytes(int(x,16) for x in tokens[:next((i for i,x in enumerate(tokens) if x=="??"),len(tokens))])
                start = 0
                while True:
                    found = data.find(prefix,start)
                    if found < 0: break
                    start = found+1
                    if found+len(tokens) <= len(data) and all(x=="??" or data[found+i]==int(x,16) for i,x in enumerate(tokens)):
                        matches[name].append(hex(rva+found))
        print(json.dumps({"executableSha256": hashlib.sha256(image).hexdigest(), "payloadRid":"win-x64",
                          "matches":matches, "uniqueHookSites":all(len(x)==1 for x in matches.values()),
                          "canLaunchModded":False, "evidence":"static inspection only"},indent=2))
