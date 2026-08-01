F3 RAPORU - KANIT PAKETİ (Application, Testing & Thesis)
============================================================
Oluşturulma tarihi: 2026-08-01 (kartın kendisinden canlı olarak toplandı)
Kaynak: ayrıntılı teknik doküman -> ../F3_Report_Detayli_Teknik_Dokuman.md
Bağımlılık: F2'ye bağımlı - bkz. ../F2_Rapor_Kanitlari/

Bu klasördeki tüm dosyalar, kart üzerinde CANLI olarak (JTAG/UART
üzerinden) komutlar çalıştırılarak toplandı - önceden hazırlanmış/
simüle edilmiş değildir.

DOSYA LİSTESİ
-------------
01_eMMC_kesif.txt                  - eMMC donanımının dmesg ile tespiti
02_capture_stream_emmc_save.txt    - Demo hedefi: kamera akışının
                                      doğrudan eMMC'ye yazılması +
                                      kalıcılık doğrulaması (umount/remount)
03_eMMC_bandwidth.txt              - eMMC yazma hızı ölçümü (~10 MB/s)
04_coklu_olcum_istatistik.txt      - Optimize denemesi sonrası çoklu
                                      ölçüm turu (fps + eMMC bandwidth,
                                      varyans=0) - "CMA fragmentation"
                                      hipotezinin çürütülüp gerçek kök
                                      nedenin (eksik --set-parm)
                                      bulunduğu deney burada
05_uygulama_iskeleti_ornek_kare.jpg - zyrenith-capture uygulamasıyla
                                      (gerçek C programı, CLI değil)
                                      eMMC'ye yakalanan kare
06_uygulama_iskeleti_calisma.txt   - "Skeleton + Component Test"
                                      maddesinin gerçek bir uygulama
                                      (video_capture + storage modülleri)
                                      ile tamamlandığının kanıtı

ÖNEMLİ METODOLOJİK NOT:
04 numaralı dosyada belgelenen deney (CMA=16MB'a çıkarınca hataların
sıfırlandığı ama fps'in DEĞİŞMEDİĞİ gözlemi), F2'deki ilk karakterizasyon
turunda görülen "CMA fragmentation'a bağlı değişken fps" hipotezini
çürütüyor. Gerçek sebep: `v4l2-ctl --set-parm` ile frame rate'in
explicit set edilmemesiydi. Bu, raporun en değerli metodolojik
bulgularından biri (yanlış hipotez → kontrollü test → doğru kök neden).

Ayrıntılı anlatım/context için: F3_Report_Detayli_Teknik_Dokuman.md
