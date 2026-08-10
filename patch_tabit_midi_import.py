from pathlib import Path
import struct, hashlib

SRC = Path('/mnt/data/TabIt(1).exe')
OUT = Path('/mnt/data/TabIt_MIDI_Import_Patched.exe')
STUB = Path('/mnt/data/tabit_stub.bin').read_bytes()

b = bytearray(SRC.read_bytes())

# Sanity check exact build.
expected_sha = '8e7af9014ff94c7fa2e17345e8dc69439f381c00839de469ae33ad3523dfeda9'
actual_sha = hashlib.sha256(b).hexdigest()
if actual_sha != expected_sha:
    raise SystemExit(f'Unexpected TabIt.exe build: {actual_sha}')

# PE section info for this exact TabIt build.
pe = struct.unpack_from('<I', b, 0x3c)[0]
coff = pe + 4
nsec = struct.unpack_from('<H', b, coff + 2)[0]
opt_size = struct.unpack_from('<H', b, coff + 16)[0]
sec_table = coff + 20 + opt_size
sections = {}
for i in range(nsec):
    o = sec_table + i*40
    name = bytes(b[o:o+8]).rstrip(b'\0').decode('ascii')
    vs, va, rs, rp = struct.unpack_from('<IIII', b, o+8)
    sections[name] = (o,vs,va,rs,rp)

t_o,t_vs,t_va,t_rs,t_rp = sections['.text']
image_base = struct.unpack_from('<I', b, pe + 4 + 20 + 28)[0]

# Repurpose obsolete Tools > Check for Updates entry as Import MIDI.
old_caption = b'Check for &Updates'
new_caption = b'&Import MIDI Tool.'
assert len(old_caption) == len(new_caption) == 18
count = b.count(old_caption)
if count != 1:
    raise SystemExit(f'Expected one update caption, found {count}')
pos = b.find(old_caption)
b[pos:pos+len(old_caption)] = new_caption

# Hook TMainForm.TCheckForUpdatesClick at VA 0x004B8E8C.
handler_va = 0x004B8E8C
handler_rva = handler_va - image_base
handler_off = t_rp + (handler_rva - t_va)
orig = bytes(b[handler_off:handler_off+6])
if orig != bytes.fromhex('e84f4cffffc3'):
    raise SystemExit(f'Unexpected handler bytes at {handler_off:#x}: {orig.hex()}')

# Install trampoline in the .text raw padding and extend VirtualSize to cover it.
stub_va = 0x004CC220
stub_rva = stub_va - image_base
stub_off = t_rp + (stub_rva - t_va)
if stub_off + len(STUB) > t_rp + t_rs:
    raise SystemExit('Stub exceeds .text raw section')
if any(b[stub_off:stub_off+len(STUB)]):
    raise SystemExit('Chosen .text padding is not empty')
b[stub_off:stub_off+len(STUB)] = STUB

# JMP rel32 from handler to stub; leave the old RET byte as unreachable padding.
rel = stub_va - (handler_va + 5)
b[handler_off:handler_off+5] = b'\xE9' + struct.pack('<i', rel)

# Increase .text VirtualSize from 0xCB210 to the raw size 0xCB400.
if t_vs != 0xCB210 or t_rs != 0xCB400:
    raise SystemExit(f'Unexpected .text sizes VS={t_vs:#x} raw={t_rs:#x}')
struct.pack_into('<I', b, t_o+8, t_rs)

OUT.write_bytes(b)
print('Created', OUT)
print('Original SHA256', actual_sha)
print('Patched  SHA256', hashlib.sha256(b).hexdigest())
print('Caption file offset', hex(pos))
print('Hook file offset', hex(handler_off), 'stub file offset', hex(stub_off), 'stub bytes', len(STUB))
