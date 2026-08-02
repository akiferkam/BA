F2 RAPORU - KANIT PAKETİ (USB 2.0 HS Camera Pipeline)
========================================================
Oluşturulma tarihi: 2026-08-01 (kartın kendisinden canlı olarak toplandı)
Kaynak: ayrıntılı teknik doküman -> ../F2_Report_Detayli_Teknik_Dokuman.md

F3'e ait kanıtlar (eMMC, bandwidth/optimize, uygulama iskeleti) ayrı
bir klasörde: ../F3_Rapor_Kanitlari/

Bu klasördeki tüm dosyalar, kart üzerinde CANLI olarak (JTAG/UART
üzerinden) komutlar çalıştırılarak toplandı - önceden hazırlanmış/
simüle edilmiş değildir.

DOSYA LİSTESİ
-------------
01_usb_kamera_dmesg.txt           - USB host + UVC kamera algılama kanıtı
02_video_formatlari.txt           - v4l2-ctl --list-formats-ext tam çıktısı
03_cma_lowmem_yerlesim_kaniti.txt - CMA'nın highmem yerine lowmem'e
                                     yerleştiğinin kanıtı (asıl fix'in
                                     çalıştığının kanıtı)
04_tekli_kare_yakalama.txt        - Tek kare v4l2-ctl streaming çıktısı
05_surekli_akis_1280x800.txt      - 30 karelik akış testi + ÖNEMLİ NOT
                                     (fps tutarsızlığının gerçek kök
                                     nedeni F3'te bulundu, buraya not
                                     düşüldü - detay F3 raporunda)
06_ornek_kare.jpg                 - Tek kare yakalama sonucu (04 ile
                                     eşleşir), 1280x800 JPEG, doğrulanmış
07-09_surekli_akis_kareN.jpg      - Sürekli akıştan ardışık 3 kare
                                     (05 ile eşleşir), JPEG SOI/EOI
                                     imzalarından ayrıştırıldı

Ayrıntılı anlatım/context için: F2_Report_Detayli_Teknik_Dokuman.md
