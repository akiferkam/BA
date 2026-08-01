#!/usr/bin/env python3
"""
UART uzerinden 'od -x' ile alinan hex dump ciktisini binary dosyaya
(ornegin .jpg) geri cevirir.

Kullanim:
    python3 hex_to_jpg.py girdi_hex.txt cikti.jpg
"""
import sys


def parse_line(line):
    parts = line.split()
    addr = int(parts[0], 8)
    b = bytearray()
    for word in parts[1:]:
        if len(word) == 4:
            b.append(int(word[2:4], 16))
            b.append(int(word[0:2], 16))
        elif len(word) == 2:
            b.append(int(word, 16))
    return addr, b


def convert(input_path, output_path):
    lines = [l.rstrip("\n") for l in open(input_path, encoding="utf-8") if l.strip()]

    out = bytearray()
    last_addr = None
    last_bytes = None

    i = 0
    while i < len(lines):
        line = lines[i]
        if line.strip() == "*":
            i += 1
            next_addr, next_bytes = parse_line(lines[i])
            gap = next_addr - last_addr - len(last_bytes)
            reps = gap // len(last_bytes)
            for _ in range(reps):
                out.extend(last_bytes)
            out.extend(next_bytes)
            last_addr, last_bytes = next_addr, next_bytes
            i += 1
            continue
        addr, b = parse_line(line)
        out.extend(b)
        last_addr, last_bytes = addr, b
        i += 1

    with open(output_path, "wb") as f:
        f.write(out)

    print(f"total bytes written: {len(out)}")
    print(f"first bytes: {out[:8].hex()}")
    print(f"last bytes:  {out[-8:].hex()}")

    try:
        from PIL import Image
        img = Image.open(output_path)
        img.verify()
        img2 = Image.open(output_path)
        print(f"VALID {img2.format} image, size={img2.size}")
    except ImportError:
        print("(PIL kurulu degil, gecerlilik kontrolu atlandi)")
    except Exception as e:
        print(f"UYARI: gecerlilik kontrolu basarisiz: {e}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)
    convert(sys.argv[1], sys.argv[2])
