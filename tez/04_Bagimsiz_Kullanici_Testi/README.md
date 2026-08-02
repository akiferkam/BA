# Bağımsız Kullanıcı Testi

Bu klasördeki ekran görüntüleri ve görseller, projeyi yürüten öğrencinin (Akif Erkam) — asistan tarafından verilen adım adım rehberi takip ederek — tüm iş akışını **kendi başına, bağımsız olarak** tekrarladığının kanıtıdır:

1. JTAG üzerinden boot (`petalinux-boot --jtag --kernel`)
2. picocom ile UART bağlantısı ve login
3. Kamera algılama doğrulaması
4. Tekli kare yakalama (`v4l2-ctl --stream-mmap`)
5. Ethernet/scp ile ve UART hex-dump yöntemiyle (`hex_to_jpg.py`) görüntü transferi
6. Sürekli akış (fps) testi
7. eMMC mount ve kalıcı kayıt testi
8. `zyrenith-capture` uygulamasının çalıştırılması

**Dosyalar:**
- `1.png` – `9.png`: sıralı ekran görüntüleri (boot, login, test adımları)
- `frame.jpg`, `frame_from_uart.jpg`: kullanıcının kendi yakaladığı ve UART üzerinden aktardığı kare örnekleri

Bu, sürecin sadece tek bir kişi (asistan) tarafından değil, projenin sahibi tarafından da bağımsız olarak tekrarlanabilir ve doğrulanabilir olduğunu gösterir — tezin tekrarlanabilirlik/reproducibility açısından güçlü bir kanıtı.
