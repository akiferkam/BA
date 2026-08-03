# F2 — USB 2.0 HS Camera Pipeline: Ayrıntılı Teknik Dokümantasyon

**Proje:** Zyrenith Linux (custom Zynq-7000 kart, PetaLinux 2022.2)
**Kapsam:** F2 — USB 2.0 HS Camera Pipeline (Bachelorarbeit planı)
**Süre:** 2026-07-31 → 2026-08-01 (2 gece, ~14+ saat aktif çalışma)
**Durum:** F2 Development Milestone TAMAMLANDI (kamera algılama + tekli/sürekli kare yakalama uçtan uca çalışıyor). F3'e ait devam çalışmaları için bkz. `F3_Report_Detayli_Teknik_Dokuman.md`.

---

## 1. Genel Bakış / Hedef

Bachelorarbeit planındaki F2 hedefi: kartın USB 2.0 High-Speed portuna bağlı bir UVC (USB Video Class) kameradan, PetaLinux/Yocto tabanlı özel Linux dağıtımı üzerinde, V4L2 (Video4Linux2) API'si ile gerçek zamanlı görüntü yakalama ("Real Time Capture & Stream") sağlamak.

Başlangıç durumu: Kart JTAG üzerinden boot ediyordu ama **USB host controller tamamen çalışmıyordu** — `/sys/bus/usb/devices/` boştu, kamera hiç görünmüyordu.

Bitiş durumu: Kamera algılanıyor, tekli ve sürekli kare yakalama V4L2 üzerinden uçtan uca çalışıyor.

---

## 2. Zaman Çizelgesi (Özet)

| Gün | Ana Başlıklar |
|---|---|
| 2026-07-31 (gece 1) | JTAG/UART sorunları çözüldü, USB host kök nedeni bulundu ve düzeltildi, kamera ilk kez algılandı, v4l-utils rootfs'e eklendi, format enumeration doğrulandı, streaming denemesi highmem/CMA hatasıyla başarısız oldu |
| 2026-08-01 (gece 2) | CMA/highmem sorunu için 4 farklı yaklaşım denendi (2 başarısız + geri alındı, 1 kısmi, 1 başarılı), gerçek kare yakalama başarıyla doğrulandı, sürekli akış test edildi — F2 burada tamamlandı, devamı (eMMC, bandwidth/optimize, uygulama iskeleti) F3 raporunda |

---

## 3. GÜN 1 (2026-07-31): PetaLinux Boot, USB Host Kök Neden Analizi, Kamera Algılama

### 3.1 JTAG Boot / UART Sorunu ve Çözümü

**Sorun:** `petalinux-boot --jtag --kernel` ile boot ediliyor ama UART'ta (`/dev/ttyUSB1`) hiçbir çıktı görünmüyordu.

**Kök neden:** `hw_server` (Xilinx JTAG debug server) boot işleminden sonra arka planda çalışmaya devam ediyor ve UART portunu bloke ediyordu.

**Çözüm (kalıcı prosedür):**
```bash
# 1) UART dinlemeyi ÖNCE başlat
cat /dev/ttyUSB1 > uart_capture.log &

# 2) JTAG boot komutunu çalıştır
petalinux-boot --jtag --kernel

# 3) Komut biter bitmez HEMEN hw_server'ı öldür
pkill -9 hw_server
```

**Ek bulgu — tekrarlayan JTAG bağlantı hatası:**
JTAG boot'ların önemli bir kısmında (bazı gecelerde %50'ye varan oranda) şu hata alındı:
```
Memory read error at 0xF8007080. MMU section translation fault
    ... ps_version ... ps7_post_config ...
```
Bu hata, FSBL/kernel yüklenmeden ÖNCE, salt JTAG/donanım seviyesinde (SLCR register okuma) oluşuyor — yazılım/device-tree içeriğiyle ilgisi yok. **Tek güvenilir çözüm:** kartın gücünü fiziksel olarak kesip tekrar takmak (bazen 1, bazen 5-6 kez tekrar gerekebiliyor). Bu davranış "beklenen" kabul edildi, kesin kök nedeni bulunamadı.

### 3.2 USB Host Controller Çalışmıyor — Kök Neden Analizi

**Semptom:** `lsusb` kurulu değildi, `/dev/video*` yoktu, `/sys/bus/usb/devices/` **tamamen boştu** (root hub bile yoktu).

**Debug adımları (sırayla):**

1. Kaynak DTS dosyalarında (`pcw.dtsi`) `usb0` node'unun `status = "okay"` olduğu doğrulandı — sorun değil.
2. Derlenmiş `system.dtb` decompile edildi (`dtc -I dtb -O dts`), `usb@e0002000` node'unun doğru şekilde `status = "okay"` ile var olduğu, platform device olarak da (`/sys/bus/platform/devices/e0002000.usb`) kayıtlı olduğu görüldü.
3. **Ama hiçbir sürücü bu cihaza bağlanmamıştı** (driver symlink yoktu).
4. Manuel bind denemesi:
   ```bash
   sudo sh -c "echo e0002000.usb > /sys/bus/platform/drivers/chipidea-usb2/bind"
   ```
   → **`write error: No such device` (ENODEV)**. Bu, sürücünün `probe()` fonksiyonunun çağrıldığını ama başarısız olduğunu kanıtlayan kritik bulgu.
5. Kernel `.config` kontrol edildi: `CONFIG_USB_CHIPIDEA`, `CONFIG_USB_CHIPIDEA_GENERIC`, `CONFIG_USB_PHY`, `CONFIG_NOP_USB_XCEIV` hepsi `=y` — sürücü kodu derlenmişti, `System.map`'te de mevcuttu (69 sembol eşleşmesi). **Eksik olan kernel config değildi.**
6. Vivado PS7 init dosyası (`ps7_init.c`) incelendi — "USB RESET" bölümü boştu (referans bilgi, kesin kanıt değil).

**GERÇEK KÖK NEDEN:** Device tree'de `usb0` node'unda sadece `phy_type = "ulpi";` vardı ama gerçek bir PHY node'una referans (`usb-phy` / `phys` property) **YOKTU**. Modern Linux kernellerinde (5.15) `chipidea` sürücüsü, bir PHY handle bulamayınca `ENODEV` ile başarısız oluyor. Bu, Xilinx PetaLinux'un otomatik ürettiği `pcw.dtsi`'de bilinen bir eksiklik/gotcha.

### 3.3 Kalıcı Düzeltme

**Dosya:** `project-spec/meta-user/recipes-bsp/device-tree/files/system-user.dtsi`

```dts
/include/ "system-conf.dtsi"
/ {
	usb_phy0: usb_phy0 {
		compatible = "usb-nop-xceiv";
		#phy-cells = <0>;
	};
};

&usb0 {
	usb-phy = <&usb_phy0>;
	dr_mode = "host";
};
```

Bu, boş bir "NOP" (no-operation) USB PHY device'ı tanımlayıp `usb0` controller'ına bağlıyor — fiziksel PHY donanımı zaten var ve çalışıyor, sadece Linux kernel'in "bir PHY handle'ı var" diye tatmin olması gerekiyordu.

**Derleme:** `petalinux-build -c device-tree` (sadece device-tree bileşeni, hızlı — tüm kernel/rootfs'e dokunmadan).

**Doğrulama:** JTAG boot sonrası UART logunda:
```
ci_hdrc ci_hdrc.0: EHCI Host Controller
ci_hdrc ci_hdrc.0: new USB bus registered, assigned bus number 1
hub 1-0:1.0: USB hub found
usb 1-1: new high-speed USB device number 2 using ci_hdrc
usb 1-1: Found UVC 1.00 device USB 2.0 Camera (0c45:636d)
input: USB 2.0 Camera: USB Camera as .../input0
```

Kamera bilgileri: **VID:PID = 0c45:636d**, UVC 1.00, `/dev/video0` ve `/dev/video1` olarak tanınıyor.

### 3.4 v4l-utils Kurulumu

**Dosya:** `project-spec/configs/rootfs_config`

```diff
- # CONFIG_v4l-utils is not set
- # CONFIG_libv4l is not set
- # CONFIG_packagegroup-petalinux-v4lutils is not set
+ CONFIG_v4l-utils=y
+ CONFIG_libv4l=y
+ CONFIG_packagegroup-petalinux-v4lutils=y
```

**Derleme:** `petalinux-build -c rootfs`.

**Doğrulama:** `v4l2-ctl --device=/dev/video0 --list-formats-ext` tam çalıştı — MJPG ve YUYV formatları, 320x200'den 1280x800'e kadar çözünürlükler, 120fps'e varan hız seçenekleri doğru okundu. Bu, V4L2 zincirinin (USB host + UVC sürücü + V4L2 framework + userspace araç) uçtan uca çalıştığını kanıtladı — **ama henüz gerçek streaming değil**, sadece format sorgulama.

### 3.5 Gün 1 Sonu Durumu

- ✅ USB host + kamera algılama çalışıyor
- ✅ Format enumeration çalışıyor
- ❌ Gerçek streaming (`VIDIOC_STREAMON`) başarısız:
  ```
  chipidea-usb2 e0002000.usb: Rejecting highmem page from CMA.
  VIDIOC_STREAMON returned -1 (Cannot allocate memory)
  ```

---

## 4. GÜN 2 (2026-08-01): CMA / Highmem Streaming Sorunu ve Çözümü

### 4.1 Sorunun Analizi

32-bit ARM kernel + 1 GiB DRAM kombinasyonunda, kernel belleği "lowmem" (doğrudan erişilebilir) ve "highmem" (sadece geçici eşlemeyle erişilebilir) olarak ikiye ayrılır. Bu karttaki sınır **~768 MB** (`0x30000000`) civarında. ChipIdea USB sürücüsü, DMA transferleri için **highmem sayfalarını kabul etmiyor** (bilinen bir sürücü kısıtlaması) — CMA (Contiguous Memory Allocator) havuzu highmem'de yer alırsa, streaming için buffer tahsisi başarısız oluyor.

### 4.2 Denenen Yaklaşımlar (kronolojik)

#### Deneme 1: `mem=752M` kernel bootarg — BAŞARISIZ, GERİ ALINDI

**Fikir:** Toplam RAM'i 752MB ile sınırlayarak highmem bölgesini tamamen ortadan kaldırmak.

```dts
chosen {
    bootargs = "console=ttyPS0,115200 earlycon root=/dev/ram0 rw mem=752M";
};
```

**Sonuç:** Boot logunda "0K highmem" doğrulandı (fikir teorik olarak doğruydu) AMA:
```
Kernel panic - not syncing: VFS: Unable to mount root fs on unknown-block(1,0)
```
initramfs (`root=/dev/ram0`) artık mount edilemiyordu. Kesin sebep tam anlaşılamadı. **Geri alındı.**

#### Deneme 2: `reserved-memory` + sabit adres — BAŞARISIZ, GERİ ALINDI

**Fikir:** Sadece CMA havuzunu (16MB) düşük bellekte sabit bir adrese rezerve etmek.

```dts
reserved-memory {
    #address-cells = <1>; #size-cells = <1>; ranges;
    cma: linux,cma {
        compatible = "shared-dma-pool";
        reusable;
        reg = <0x1e000000 0x2000000>;
        linux,cma-default;
    };
};
```

**Sonuç:** Kernel paniği YOKTU ama kart U-Boot **"Hit any key to stop autoboot"** noktasında tamamen takılı kaldı (hiçbir ilerleme, Enter'a bile tepki yok). **Geri alındı.**

#### Deneme 2b: `reserved-memory` + `alloc-ranges` (sabit adres yerine) — YİNE BAŞARISIZ, GERİ ALINDI

**Fikir:** Sabit adres yerine kernelin adresi kendi seçmesine izin vermek, belki `0x1e000000` bir çakışmaya sebep oluyordu.

```dts
reserved-memory {
    #address-cells = <1>; #size-cells = <1>; ranges;
    cma: linux,cma {
        compatible = "shared-dma-pool";
        reusable;
        alloc-ranges = <0x10000000 0x1e000000>;
        size = <0x2000000>;
        linux,cma-default;
    };
};
```

**Sonuç:** **BİREBİR AYNI** semptom — U-Boot yine "Hit any key to stop autoboot" noktasında tamamen kilitlendi. **Bu, sorunun adresin kendisiyle değil, `reserved-memory` device-tree node'unun VARLIĞIYLA ilgili olduğunu kanıtladı** (muhtemelen bu board'un U-Boot'u kendi kontrol FDT'si olarak `system.dtb`'yi kullanıyor ve bu node'u işlerken takılıyor). **Geri alındı.**

#### Deneme 3: `CONFIG_CMA_SIZE_MBYTES=4` (kernel config) — KISMİ BAŞARI

**Fikir:** CMA havuzunu küçültmek (16MB → 4MB), belki highmem'e taşma ihtimalini azaltır.

**Dosya:** `project-spec/meta-user/recipes-kernel/linux/linux-xlnx/bsp.cfg`
```diff
+ CONFIG_CMA_SIZE_MBYTES=4
```

**Derleme:** `petalinux-build -c kernel` (tam kernel rebuild, device-tree'den çok daha uzun sürer).

**Sonuç:** Boot BAŞARILI, panic/hang YOK, AMA boot logunda:
```
cma: Reserved 4 MiB at 0x3fc00000
HighMem  [mem 0x0000000030000000-0x000000003fffffff]
```
`0x3fc00000` HÂLÂ highmem bölgesinde! **Kernelin varsayılan CMA yerleştirme davranışı (memblock, RAM'in EN ÜSTÜNDEN aşağıya doğru ayırıyor) boyuttan bağımsız olarak CMA'yı hep en üst adrese koyuyor** — 1GB RAM'de bu her zaman highmem'e denk geliyor. Streaming testi aynı "Rejecting highmem page" hatasını verdi.

#### Deneme 4 (BAŞARILI ÇÖZÜM): `cma=` kernel bootarg parametresi

**Fikir:** `reserved-memory` device-tree node'u U-Boot'u kilitliyordu (Deneme 2/2b), ama `cma=<boyut>@<başlangıç>-<bitiş>` **saf bir kernel command-line parametresi** — device-tree'ye HİÇBİR node eklemiyor, sadece kernelin kendi `early_param("cma", ...)` ayrıştırıcısı tarafından okunuyor. U-Boot'un FDT işleme mantığını hiç etkilemiyor.

**Dosya:** `project-spec/meta-user/recipes-bsp/device-tree/files/system-user.dtsi` (final hâl)
```dts
/include/ "system-conf.dtsi"
/ {
	chosen {
		bootargs = "console=ttyPS0,115200 earlycon root=/dev/ram0 rw cma=4M@0x10000000-0x30000000";
	};

	usb_phy0: usb_phy0 {
		compatible = "usb-nop-xceiv";
		#phy-cells = <0>;
	};
};

&usb0 {
	usb-phy = <&usb_phy0>;
	dr_mode = "host";
};
```

`cma=4M@0x10000000-0x30000000` → CMA havuzunu 256MB-768MB aralığında (kesinlikle lowmem sınırının altında) 4MB olarak zorluyor.

**Derleme:** `petalinux-build -c device-tree` (hızlı, sadece DT).

**Sonuç:** Boot BAŞARILI, panic/hang YOK. Boot logunda:
```
cma: Reserved 4 MiB at 0x2ec00000
```
`0x2ec00000` (≈748MB) **highmem sınırının (768MB) KESİNLİKLE ALTINDA** — CMA artık doğru şekilde lowmem'de!

**Streaming testi:**
```bash
v4l2-ctl --device=/dev/video0 --stream-mmap --stream-count=1 --stream-to=/tmp/frame.jpg
```
"Rejecting highmem page from CMA" hatası **TAMAMEN KAYBOLDU**. Bunun yerine görülen `cma_alloc: reserved: alloc failed, req-size: 19 pages, ret: -12/-16` mesajları FATAL değildi — sürücü otomatik olarak normal (CMA-dışı ama yine lowmem) sayfalara fallback yapıyor.

**Sonuç dosyası:** `/tmp/frame.jpg`, 20976 byte, **GEÇERLİ JPEG** (FF D8 FF C0 ... FF D9 imzaları doğrulandı, PIL ile 1280x800 RGB olarak açıldı).

### 4.3 Dosya Host'a Nasıl Aktarıldı (Ethernet yoktu)

Ethernet bağlı/DHCP yoktu, dolayısıyla dosya **UART üzerinden hex-dump** yöntemiyle aktarıldı:
```bash
# Board tarafında:
od -x /tmp/frame.jpg

# Host tarafında: od çıktısı Python ile parse edilip binary'ye geri çevrildi
# (od'nin tekrarlayan satırları "*" ile kısaltma davranışına dikkat edilerek)
```

### 4.4 Sürekli Video Akışı Testi

```bash
v4l2-ctl --device=/dev/video0 --stream-mmap --stream-count=10 --stream-to=/tmp/video.raw
```
**Sonuç: 7.66-7.89 fps, 10 kareden sadece 1 buffer drop.** *(Bu ölçüm `--set-parm` düzeltmesinden ÖNCEdir — frame rate explicit set edilmediği için düşük çıkmıştır; aynı ayarlarla düzeltme sonrası ölçülen 26.79 fps için bkz. F3 raporu §3.4.)* Ayrı bir 3 karelik doğrulama akışı çekilip host'a aktarıldı, 3 kare de FF D8/FF D9 imzalarından ayrıştırılıp PIL ile doğrulandı — hepsi geçerli 1280x800 JPEG.

---

## 5. Değiştirilen Dosyalar (F2 Kapsamı)

| Dosya | Değişiklik | Kalıcı mı? |
|---|---|---|
| `project-spec/meta-user/recipes-bsp/device-tree/files/system-user.dtsi` | `usb_phy0` (USB PHY fix) + `cma=4M@0x10000000-0x30000000` bootarg | ✅ Evet (not: `cma=` değeri daha sonra F3'ün "Optimize" adımında 16M'a güncellendi — bkz. F3 raporu) |
| `project-spec/configs/rootfs_config` | `v4l-utils`, `libv4l`, `packagegroup-petalinux-v4lutils` eklendi | ✅ Evet |
| `project-spec/meta-user/recipes-kernel/linux/linux-xlnx/bsp.cfg` | `CONFIG_CMA_SIZE_MBYTES=4` (artık gereksiz sayılabilir, `cma=` bootarg zaten boyutu override ediyor) | ✅ Evet (zararsız) |

**Geri alınan (kalıcı OLMAYAN) denemeler:** `mem=752M` bootarg, `reserved-memory` DT node'u (2 varyant) — hiçbiri final dosyada yok.

---

## 6. Tez İçin Önemli Teknik Bulgular (F2 Özel)

1. **PetaLinux'un otomatik ürettiği `pcw.dtsi`, modern chipidea sürücüsü için eksik bir PHY tanımına sahip** — bu, Xilinx/PetaLinux ekosisteminde bilinen ama dokümante edilmemiş bir "gotcha". Çözüm: manuel `usb-nop-xceiv` PHY node'u eklemek.
2. **32-bit ARM + >768MB DRAM sistemlerde, varsayılan CMA reservation davranışı (top-down memblock allocation) her zaman highmem'e düşer** — DMA-kısıtlı sürücüler (chipidea gibi) için bu kritik bir sorun.
3. **Device-tree `reserved-memory` node'u, bazı U-Boot yapılandırmalarında (özellikle system.dtb'yi kontrol FDT'si olarak kullananlarda) boot'u kilitleyebilir** — adresten bağımsız olarak. Bunun yerine **kernel `cma=` bootarg parametresi** kullanmak, U-Boot'u hiç etkilemeden aynı sonucu (CMA'yı lowmem'e zorlamak) güvenle sağlar.

---

*Bu doküman F2 (USB 2.0 HS Camera Pipeline) kapsamını özetler — kaynak: 2026-07-31 ve 2026-08-01 tarihli çalışma oturumları. F3'e ait içerik (eMMC entegrasyonu, Bandwidth/Latency Characterization + Optimize, Skeleton + Component Test uygulaması) için bkz. `F3_Report_Detayli_Teknik_Dokuman.md`. Ham UART logları ve ara adım çıktıları proje dizinindeki scratchpad klasöründe mevcuttur (gerekirse).*
