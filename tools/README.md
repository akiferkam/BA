# tools/

Host-side yardımcı script'ler (kart üzerinde değil, geliştirme bilgisayarında çalıştırılır).

## hex_to_jpg.py

Ethernet bağlantısı olmadığında, kart üzerinde yakalanan bir dosyayı (ör. `/tmp/frame.jpg`) UART üzerinden host'a taşımak için kullanılır.

**Karttaki adım:**
```bash
echo BEGIN_HEX; od -x /tmp/frame.jpg; echo END_HEX
```
Çıktıyı kopyala, host'ta bir `.txt` dosyasına yapıştır (`BEGIN_HEX`/`END_HEX` satırları olmadan).

**Host'taki adım:**
```bash
python3 tools/hex_to_jpg.py girdi_hex.txt cikti.jpg
```

`od`'nin tekrarlayan satırları `*` ile kısaltma davranışını (`-v` bayrağı verilmediğinde) doğru şekilde genişleterek orijinal binary'yi birebir geri üretir. Sonunda PIL kuruluysa dosyanın geçerli bir görüntü olup olmadığını da doğrular.
