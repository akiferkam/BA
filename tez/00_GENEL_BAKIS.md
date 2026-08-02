# Bachelorarbeit — Genel Bakış ve Proje Özeti

**Proje:** Zyrenith Linux — Custom Zynq-7000 Kart Bring-up + USB Kamera Pipeline + Uygulama Geliştirme
**Öğrenci:** Akif Erkam
**Kurum:** Technische Hochschule Ingolstadt
**Kapsam:** Bachelorarbeit planındaki F1, F2, F3 çalışma paketlerinin tamamı
**Bu klasör:** Tez yazımı için toplanmış tüm teknik dokümantasyon, kanıtlar ve kaynak kod

---

## Klasör Yapısı

```
tez/
├── 00_GENEL_BAKIS.md                          (bu dosya — sentez/özet)
├── 01_Feature1_Zynq_Platform_Linux/
│   ├── F1_Report.md                           (orijinal Almanca rapor — Vivado, DDR3, MIO, PetaLinux boot)
│   └── images/                                 (kart fotoğrafı, doğrulama ekran görüntüsü)
├── 02_Feature2_USB_Camera_Pipeline/
│   ├── F2_Report.md                           (USB host fix + kamera streaming — ayrıntılı Türkçe rapor)
│   ├── kanitlar/                               (canlı UART logları + yakalanan kareler)
│   └── kaynak-kod-referans/
│       └── system-user.dtsi                   (USB PHY + CMA device-tree fix'i)
├── 03_Feature3_Application_Testing/
│   ├── F3_Report.md                           (eMMC + bandwidth/optimize + uygulama iskeleti)
│   ├── kanitlar/                               (canlı UART logları + eMMC kanıtları + uygulama testleri)
│   └── zyrenith-capture-kaynak-kod/
│       ├── zyrenith-capture.c                 (main — orkestrasyon)
│       ├── video_capture.c / .h                (V4L2 capture component)
│       ├── storage.c / .h                      (dosya yazma component)
│       └── Makefile
└── 04_Bagimsiz_Kullanici_Testi/
    ├── README.md
    ├── 1.png – 9.png                          (öğrencinin kendi başına tekrarladığı test adımları)
    └── frame.jpg, frame_from_uart.jpg          (öğrencinin kendi yakaladığı/aktardığı kareler)
```

Ayrıca tam proje kaynak kodu (PetaLinux `project-spec/` + bu raporların orijinalleri) GitHub'da: **https://github.com/akiferkam/BA**

---

## Proje Anlatısı (Tez İçin Sentez)

### Bağlam ve Hedef

Bu Bachelorarbeit, özel tasarım bir Zynq-7000 (XC7Z010) kartının sıfırdan Linux çalışır hale getirilmesini, üzerine bir USB kamera pipeline'ının (V4L2 tabanlı gerçek zamanlı görüntü yakalama) entegre edilmesini, ve bunun eMMC kalıcı depolama ile birleştiren gerçek bir uygulamaya dönüştürülmesini kapsıyor. Çalışma üç ardışık işlem paketine (Feature) ayrılmıştır — plan dokümanındaki F1/F2/F3 yapısına birebir karşılık gelir.

### Feature 1 — Platform Bring-up (bkz. `01_Feature1_Zynq_Platform_Linux/F1_Report.md`)

Vivado'da PS (Processing System) konfigürasyonu sıfırdan kuruldu: MIO pin ataması, saat ağacı (33.333 MHz osilatörden 666.7 MHz CPU / 533.3 MHz DDR türetimi), ve en kritik kısım — kartın kullandığı DDR3L belleğin (Micron MT41K256M16) Vivado'nun standart parça listesinde bulunmaması nedeniyle **custom part olarak, datasheet + PCB layout'tan elde edilen timing parametreleriyle** manuel tanımlanması. Üretilen `.xsa` dosyası referans bir konfigürasyonla parametre parametre karşılaştırılıp doğrulandı (3 sapma bulunup düzeltildi: bank voltajı, GPIO, QSPI saat hızı).

Bare-metal doğrulama (Vitis, "Hello World" over JTAG) donanımın temel çalışırlığını (güç, saat, DDR, MIO, UART) kanıtladıktan sonra, PetaLinux 2022.2 ile tam bir Embedded Linux imajı (FSBL → U-Boot → Kernel 5.15.36-xilinx → rootfs) inşa edilip JTAG üzerinden boot edildi, login shell'e ulaşıldı.

**Sonuç:** Çalışan bir Linux platformu — 1GB DDR3L tam tanınıyor, eMMC ve USB host controller (EHCI) başarıyla initialize oluyor.

### Feature 2 — USB Kamera Pipeline (bkz. `02_Feature2_USB_Camera_Pipeline/F2_Report.md`)

F1'in bitişinde USB host controller "initialize" olmuş görünse de, bir UVC kamera bağlandığında **hiçbir şey algılanmıyordu** — `/sys/bus/usb/devices/` tamamen boştu. Sistematik bir kök neden analizi (driver bind denemeleri, kernel config kontrolü, device-tree decompile) sonunda gerçek sorun bulundu: PetaLinux'un otomatik ürettiği device-tree'de USB0 için bir PHY handle (`usb-phy`/`phys` property) eksikti — modern `chipidea` sürücüsü bu olmadan `ENODEV` ile başarısız oluyordu. Bir `usb-nop-xceiv` PHY node'u ekleyerek kalıcı olarak düzeltildi.

Bu düzeltmeden sonra kamera (OV9281, monokrom global-shutter UVC modülü) algılandı, ama gerçek video streaming'i (`VIDIOC_STREAMON`) 32-bit ARM + 1GB DRAM sisteminin CMA (Contiguous Memory Allocator) belleğini varsayılan olarak "highmem" bölgesine yerleştirmesi yüzünden başarısız oluyordu — `chipidea` sürücüsü DMA için highmem sayfalarını kabul etmiyor. İki başarısız deneme (`mem=` sınırlama, `reserved-memory` device-tree node'u — ikincisi bu board'da U-Boot'u kilitliyordu) sonrası, **kernel `cma=` bootarg parametresi** ile CMA havuzunun lowmem'e zorlanması sorunu kalıcı olarak çözdü.

**Sonuç:** Kameradan tekli ve sürekli (multi-frame) görüntü yakalama V4L2 üzerinden uçtan uca çalışıyor, ilk gerçek kareler yakalanıp doğrulandı.

### Feature 3 — Uygulama, Test ve eMMC Entegrasyonu (bkz. `03_Feature3_Application_Testing/F3_Report.md`)

F2'nin üzerine üç ek hedef gerçekleştirildi:

1. **Capture + Stream + EMMC Save demo:** eMMC (3.64 GiB gerçek eMMC, SD kart değil — `mmcblk0boot0/boot1/rpmb` ile doğrulandı) keşfedilip ext4 ile formatlandı, mount edildi; kameradan yakalanan kareler doğrudan eMMC'ye yazıldı ve **unmount/remount ile kalıcılığı kanıtlandı**.

2. **Bandwidth/Latency Characterization + Optimize:** Sistematik performans ölçümleri yapıldı. İlk turda "1280x800'de düşük fps (~9), CMA parçalanmasından kaynaklanıyor" hipotezi kuruldu. **Kontrollü bir deneyle** (CMA havuzunu 4MB'den 16MB'a çıkarıp hataların sıfırlandığı ama fps'in DEĞİŞMEDİĞİ gözlemlenerek) bu hipotez çürütüldü; gerçek kök neden **`v4l2-ctl --set-parm` ile frame rate'in explicit set edilmemesi** olduğu bulundu — set edilince 1280x800'de de ~27fps'e (640x480 ile aynı seviye) ulaşıldı. Bu, "yanlış hipotez → kontrollü test → doğru kök neden" şeklinde iyi bir mühendislik metodolojisi örneği. Sonuçlar 3-5 tekrarlı ölçümlerle istatistiksel olarak doğrulandı (sıfır varyans).

3. **Skeleton + Component Test:** CLI araçları (`v4l2-ctl`) yerine gerçek, modüler bir C uygulaması (`zyrenith-capture`) yazıldı — `video_capture` (V4L2 mmap-streaming) ve `storage` (dosya yazma+fsync) olarak ayrı, bağımsız test edilebilir component'lardan oluşuyor. PetaLinux'un `petalinux-create -t apps` mekanizmasıyla projeye entegre edildi, hem tmpfs'e hem doğrudan eMMC'ye başarıyla test edildi (10/10 kare, ~20-21fps uçtan-uca — fsync dahil).

**Sonuç:** eMMC'ye kalıcı, gerçek zamanlı kamera kaydı yapan, yeniden kullanılabilir bir uygulama; sistemin performans sınırlarının doğru şekilde karakterize edilmiş hali.

---

## Tez İçin Öne Çıkan Teknik Katkılar / Bulgular

Bu liste, tezin "Ergebnisse" veya "Diskussion" bölümü için özellikle değerli olabilecek, kendi başına doğrulanmış bulguları özetler:

1. **Custom DDR3L konfigürasyonu** — datasheet + PCB layout'tan manuel timing parametresi çıkarımı, referansla parametre-parametre doğrulama metodolojisi (F1).
2. **PetaLinux'un otomatik ürettiği device-tree'nin eksik USB PHY tanımı** — Xilinx ekosisteminde bilinen ama dokümante edilmemiş bir "gotcha", ve bunun `usb-nop-xceiv` ile çözümü (F2).
3. **32-bit ARM + >768MB DRAM sistemlerde CMA'nın varsayılan olarak highmem'e düşmesi** ve DMA-kısıtlı sürücülerle (chipidea) çakışması; `reserved-memory` device-tree node'unun bazı U-Boot'ları kilitleyebileceği ama `cma=` kernel bootarg'ının güvenli bir alternatif olduğu (F2).
4. **"CMA fragmentation" hipotezinin kontrollü deneyle çürütülmesi**, gerçek kök nedenin eksik bir V4L2 parametresi (`--set-parm`) olduğunun bulunması — USB 2.0 HS bant genişliğinin plan'da öngörülenin aksine gerçek darboğaz olmadığının gösterilmesi (F3).
5. **eMMC'nin initramfs-tabanlı (RAM disk) root dosya sisteminden bağımsız kalıcı depolama sağladığının** kanıtlanması — embedded sistemlerde yaygın bir mimari desen (F3).
6. **CLI-tabanlı "component test" ile gerçek "application skeleton" arasındaki fark** ve PetaLinux/Yocto recipe mekanizmasıyla özel bir uygulamanın projeye nasıl entegre edildiği (F3).

---

## Notlar

- F1 raporu orijinal haliyle (Almanca) korunmuştur — kullanıcının kendi yazdığı metin.
- F2/F3 raporları bu çalışma oturumlarında (2026-08-01/02) Türkçe olarak hazırlanmıştır; tez metnine geçerken hedef dile (muhtemelen Almanca, F1 ile tutarlılık için) çevrilmesi gerekecektir.
- Tüm kanıt dosyaları (`kanitlar/` klasörleri) kart üzerinde CANLI olarak toplanmıştır, önceden hazırlanmış/simüle edilmiş değildir — her komut ve çıktı gerçek donanımdan alınmıştır.
- Kod deposu (GitHub): proje kaynak dosyaları, uygulama kodu ve bu raporların güncel halleri sürekli senkronize tutulmaktadır.
