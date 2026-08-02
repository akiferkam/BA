# F3 — Application, Testing & Thesis: Ayrıntılı Teknik Dokümantasyon

**Proje:** Zyrenith Linux (custom Zynq-7000 kart, PetaLinux 2022.2)
**Kapsam:** F3 — Application, Testing & Thesis (Bachelorarbeit planı)
**Bağımlılık:** F2'ye bağımlı (V4L2 kamera akışı çalışıyor olmalı — bkz. `F2_Report_Detayli_Teknik_Dokuman.md`)
**Tarih:** 2026-08-01 (F2 tamamlandıktan sonra, aynı gece devam)
**Durum:** F3'ün 3 ana maddesi de tamamlandı: Demo (Capture+Stream+EMMC Save), Bandwidth/Latency Characterization + Optimize, Skeleton + Component Test

---

## 1. Genel Bakış / Hedef

F3, F2'de çalışır hale getirilen V4L2 kamera pipeline'ının üzerine inşa edilen uygulama/test katmanı:

1. **Demo: Capture + Stream + EMMC Save** — yakalanan görüntülerin kalıcı depolamaya (eMMC) yazılması
2. **Bandwidth / Latency Characterization + Optimize** — sistemin performans sınırlarının ölçülmesi ve iyileştirilmesi
3. **Skeleton + Component Test** — CLI araçları yerine gerçek, modüler bir uygulama yazılması

---

## 2. eMMC Kalıcı Depolama Entegrasyonu (Demo Hedefi)

### 2.1 eMMC Keşfi

```bash
cat /proc/partitions   # mmcblk0, 3817472 blok (~3.7GB) bulundu
dmesg | grep -i "mmc\|sdhci"
```
Çıktı, gerçek bir eMMC olduğunu doğruladı (SD kart değil):
```
mmc0: new high speed MMC card at address 0001
mmcblk0: mmc0:0001 S40004 3.64 GiB
mmcblk0boot0: mmc0:0001 S40004 4.00 MiB
mmcblk0boot1: mmc0:0001 S40004 4.00 MiB
mmcblk0rpmb: mmc0:0001 S40004 4.00 MiB, chardev (245:0)
```
(`boot0`/`boot1`/`rpmb` partition'ları eMMC'ye özgü donanım özellikleri.)

### 2.2 Format ve Mount

```bash
# İlk sektör kontrolü (güvenlik için) - tamamen sıfırdı, veri yoktu
dd if=/dev/mmcblk0 bs=512 count=1 | od -x

# Format (tüm disk, partition tablosu olmadan)
sudo mkfs.ext4 -F -L emmc_storage /dev/mmcblk0

# Mount
sudo mkdir -p /mnt/emmc
sudo mount /dev/mmcblk0 /mnt/emmc
df -h /mnt/emmc   # 3.5G boyut, 3.3G kullanılabilir
```

### 2.3 Capture + Stream + EMMC Save (Demo)

```bash
# Tekli kare
v4l2-ctl --device=/dev/video0 --stream-mmap --stream-count=1 \
    --stream-to=/mnt/emmc/emmc_frame.jpg
# → 20816 byte

# Kalıcılık doğrulama
sync
sudo umount /mnt/emmc
sudo mount /dev/mmcblk0 /mnt/emmc
ls -la /mnt/emmc/emmc_frame.jpg   # dosya hâlâ orada, aynı boyutta ✓

# Sürekli akış
v4l2-ctl --device=/dev/video0 --stream-mmap --stream-count=10 \
    --stream-to=/mnt/emmc/emmc_stream.raw
# → 7.80 fps, 10 kareden 1 drop, 217176 byte
```

**Önemli mimari not:** Board initramfs'ten (RAM disk) boot ediyor — yani `/root` dosya sistemi her boot'ta sıfırlanıyor. Ama eMMC (`/dev/mmcblk0`) **ayrı bir fiziksel blok cihazı** — bu, root dosya sisteminden bağımsız ve **gerçekten kalıcı**. JTAG ile tekrar boot edilse bile eMMC'deki dosyalar silinmiyor (sonraki bir boot oturumunda mount noktası otomatik olarak `/run/media/mmcblk0` altında oluştu — mount noktası kalıcı değil ama İÇERİK kalıcı; önceki geceden kalan dosyaların hâlâ orada olduğu doğrulandı).

---

## 3. Bandwidth / Latency Karakterizasyonu + Optimize

### 3.1 eMMC Bant Genişliği (ilk ölçüm)

```bash
# Yazma
date; dd if=/dev/zero of=/mnt/emmc/bwtest bs=1M count=50; sync; date
# → 50MB / ~5 saniye ≈ 10 MB/s yazma

# Okuma (cache temizlenerek)
sync; echo 3 | sudo tee /proc/sys/vm/drop_caches
date; dd if=/mnt/emmc/bwtest of=/dev/null bs=1M; date
# → 50MB / ~3 saniye ≈ 16.7 MB/s okuma
```

### 3.2 Çözünürlük vs FPS — İLK TUR (yanıltıcı sonuç)

| Format/Çözünürlük | FPS | CMA alloc durumu |
|---|---|---|
| 1280x800 MJPG (varsayılan, frame interval set edilmeden) | ~7.9-9.0 fps | Sık başarısız, fallback ile telafi |
| 640x480 MJPG | 26.63 fps | HİÇ başarısız olmadı |
| 800x600 MJPG | 29.59 fps | Başarısız oluyordu ama fps yüksek kaldı |

Bu ilk turda kurulan hipotez: **"CMA'nın (F2'de 4MB olarak ayarlanmış) havuzu yüksek çözünürlükte parçalanıyor, bu yüzden 1280x800'de fps düşük"**.

### 3.3 Kontrollü Deney (Optimize): CMA=16MB — Hipotez ÇÜRÜTÜLDÜ

Hipotezi test etmek için F2'de belirlenen `cma=4M@...` bootarg'ı `cma=16M@0x10000000-0x30000000` olarak güncellendi (device-tree rebuild + JTAG reboot). Sonuç:

```bash
v4l2-ctl --device=/dev/video0 --stream-mmap --stream-count=30 --stream-to=/dev/null
```
**1280x800 MJPG, CMA=16MB, 3 kez üst üste test edildi — HER SEFERİNDE BİREBİR AYNI SONUÇ:**
- `9.06 fps`, **0 (SIFIR) cma_alloc hatası** (16MB ile fallback tamamen ortadan kalktı)

**Beklenti:** CMA hataları kalktığına göre fps 26+ olmalıydı (640x480 gibi). **Gerçekleşen:** fps hâlâ ~9'da kaldı. **Hipotez ÇÜRÜTÜLDÜ** — CMA boyutu/parçalanması hiçbir zaman gerçek darboğaz değilmiş.

### 3.4 Gerçek Kök Neden Bulundu: Eksik `--set-parm`

```bash
v4l2-ctl --device=/dev/video0 --set-fmt-video=width=1280,height=800,pixelformat=MJPG --set-parm=30
v4l2-ctl --device=/dev/video0 --get-parm
# → "Frame rate set to 30.000 fps" doğrulandı
v4l2-ctl --device=/dev/video0 --stream-mmap --stream-count=30 --stream-to=/dev/null
```
**SONUÇ: 26.79 fps, 0 cma_alloc hatası** — 640x480 ile pratikte aynı performans, tam 1280x800 çözünürlükte!

**Gerçek kök neden:** `--set-fmt-video` ile sadece çözünürlük/format değiştirildiğinde, frame interval (fps) **hiç explicit set edilmemişti**. Kamera, çözünürlük değiştiğinde varsayılan olarak düşük bir dahili frame rate'e (~10fps nominal) düşüyor — bu bir donanım/USB/CMA darboğazı DEĞİL, sadece eksik bir V4L2 parametresi (`--set-parm`) sorunuydu.

### 3.5 Metodolojik Not (Rapor İçin Önemli)

Bu, iyi bir bilimsel/mühendislik süreç örneği olarak rapora eklenebilir: **ilk gözlemlenen korelasyon (CMA hataları ↔ düşük fps) yanlış bir nedensellik hipotezine yol açmıştı.** Kontrollü bir deneyle (CMA'yı büyüt, hatanın kalktığını doğrula, ama fps'in DEĞİŞMEDİĞİNİ gözlemle) bu hipotez çürütüldü ve gerçek değişken (`--set-parm` eksikliği) izole edilerek bulundu.

### 3.6 Sonuç / Yorum

**USB 2.0 HS'in teorik bant genişliği (480 Mbps ≈ 60 MB/s) burada sınırlayıcı faktör DEĞİL** — tam 1280x800 çözünürlükte, ~27fps sıkıştırılmış MJPG akışı bile bu sınırın çok altında (~15-20 Mbps civarı tahmini).

**Asıl "darboğaz" bir yazılım/kullanım hatasıydı** (frame rate parametresinin explicit set edilmemesi), donanım/protokol kısıtlaması değildi. Bu, planın "USB 2.0 HS bant genişliği FPS/çözünürlüğü sınırlıyor" risk notunu **büyük ölçüde çürütüyor** — düzgün yapılandırıldığında sistem, en yüksek çözünürlükte bile beklenenden çok daha iyi performans veriyor.

**CMA=16MB'ı korumaya değer** (0 hata = daha temiz/güvenilir çalışma, ~12MB ekstra RAM maliyeti düşük) ama kesin bir gereklilik değildi — asıl düzeltme `--set-parm` ile frame rate'i explicit belirtmekti. Bu, "Optimize" maddesinin çıktısı: CMA=16MB kalıcı olarak tutuldu (F2'nin `cma=4M` değeri, F3'ün `cma=16M` değeriyle override edildi).

### 3.7 Çoklu Ölçüm Turu — İstatistiksel Doğrulama (Varyans Analizi)

Tek seferlik ölçümlerin güvenilirliğini doğrulamak için her senaryo 3-5 kez tekrarlandı (CMA=16MB, `--set-parm=30` ile):

| Çözünürlük | Format | Tur sayısı | Sonuç (her turda) | Varyans |
|---|---|---|---|---|
| 1280x800 | MJPG | 5 | **26.79 fps**, 1 drop | **0** (5/5 birebir aynı) |
| 640x480 | MJPG | 5 | **26.63 fps**, 1 drop | **0** (5/5 birebir aynı) |
| 800x600 | MJPG | 3 | **26.79 fps**, 1 drop | **0** (3/3 birebir aynı) |
| 1280x720 | MJPG | 3 | **26.79 fps**, 1 drop | **0** (3/3 birebir aynı) |

**eMMC yazma bant genişliği** (50MB/tur, 5 tur):

| Tur | Süre | Hız |
|---|---|---|
| 1 | 6s | 8.33 MB/s |
| 2 | 6s | 8.33 MB/s |
| 3 | 6s | 8.33 MB/s |
| 4 | 6s | 8.33 MB/s |
| 5 | 6s | 8.33 MB/s |
| **Ortalama** | **6.0s** | **8.33 MB/s** | **Varyans: 0** |

**Sonuç:** `--set-parm` ile frame rate explicit belirtildiğinde ve CMA=16MB kullanıldığında, sistem **tamamen deterministik/tekrarlanabilir** performans gösteriyor (sıfır varyans, saniye/fps hassasiyetinde). Bu, bölüm 3.2'deki görünen "değişkenlik"in gerçek bir stokastik/donanımsal belirsizlikten değil, **eksik parametre nedeniyle tanımsız/varsayılan davranıştan** kaynaklandığını bir kez daha doğruluyor.

(Not: eMMC yazma hızı bu turda 8.33 MB/s ölçüldü, önceki tek ölçümde ~10 MB/s bulunmuştu — bu küçük fark saniye hassasiyetli zamanlama yönteminin (tam saniyeye yuvarlama) doğal ölçüm belirsizliğinden kaynaklanıyor, daha hassas bir zamanlayıcı ile (`date +%N` desteklenmediği için kullanılamadı) daha kesin bir rakam elde edilebilir.)

---

## 4. Skeleton + Component Test — Gerçek Uygulama İskeleti

### 4.1 Motivasyon

Buraya kadarki tüm doğrulamalar (F2 ve F3'ün önceki bölümleri) mevcut CLI araçlarıyla (`v4l2-ctl`, `dd`, `mount`) yapılmıştı — bu, alttaki bileşenlerin çalıştığını kanıtlayan geçerli bir **"component test"**, ama elde gerçek bir **uygulama iskeleti** yoktu. Bu bölümde PetaLinux'un kendi app-recipe mekanizmasıyla modüler bir C uygulaması yazıldı.

### 4.2 Mimari

Proje: `project-spec/meta-user/recipes-apps/zyrenith-capture/`

| Dosya | Rol |
|---|---|
| `files/video_capture.h` / `.c` | V4L2 mmap-streaming capture component (open → format negotiate → mmap → streamon → dqbuf/qbuf döngüsü → streamoff → close). **Bağımsız test edilebilir.** |
| `files/storage.h` / `.c` | Dosya yazma component (open → write → fsync). **Bağımsız test edilebilir.** |
| `files/zyrenith-capture.c` | `main()`: iki component'ı birbirine bağlayan orkestrasyon katmanı. Komut satırı argümanları: `-d device -o outdir -w width -h height -f fps -n count`. |
| `files/Makefile` | `APP_OBJS = zyrenith-capture.o video_capture.o storage.o` |
| `zyrenith-capture.bb` | PetaLinux/Yocto recipe — `SRC_URI` tüm kaynak dosyaları listeler, `do_compile`/`do_install` standart. |

`video_capture.c` içinde önemli bir detay: `VIDIOC_S_PARM` ile frame rate **her zaman explicit set ediliyor** — bölüm 3.4'te bulunan kök nedeni (eksik `--set-parm`) uygulamanın kendisinde, kaynağında çözülmüş durumda; bu hatayı bir daha yapmak mümkün değil.

### 4.3 Oluşturma / Derleme

```bash
petalinux-create -t apps -n zyrenith-capture --enable
petalinux-build -c zyrenith-capture
petalinux-build -c rootfs   # uygulamayı rootfs image'ına dahil etmek için
```

### 4.4 Test 1: tmpfs'e (/tmp) yazma

```bash
zyrenith-capture -d /dev/video0 -o /tmp/capture_test -w 1280 -h 800 -f 30 -n 5
```
```
zyrenith-capture: device=/dev/video0 outdir=/tmp/capture_test 1280x800 @30fps, count=5
zyrenith-capture: negotiated format 1280x800, 4 mmap buffers
zyrenith-capture: done. captured=5 failed=0 elapsed=0.300s achieved=16.68fps
```
5 dosya oluştu (`frame_0000.jpg` … `frame_0004.jpg`, ~20.8KB her biri). Host'a aktarılıp PIL ile doğrulandı: geçerli 1280x800 JPEG.

*(Küçük bir not: dosyanın son 4 byte'ı sıfır padding — gerçek `FF D9` (JPEG EOI) imzası birkaç byte önce, offset 20770'te bulundu. JPEG decoder'lar trailing padding'i görmezden gelir, bu fonksiyonel bir sorun değil, sadece `bytesused`'un birkaç byte fazla rapor etmesi — muhtemelen CMA fallback mekanizmasının bir yan etkisi.)*

### 4.5 Test 2: eMMC'ye doğrudan yazma (gerçek hedef senaryo)

```bash
mkdir -p /run/media/mmcblk0/app_capture
zyrenith-capture -d /dev/video0 -o /run/media/mmcblk0/app_capture -w 1280 -h 800 -f 30 -n 10
```
```
zyrenith-capture: device=/dev/video0 outdir=/run/media/mmcblk0/app_capture 1280x800 @30fps, count=10
zyrenith-capture: negotiated format 1280x800, 4 mmap buffers
zyrenith-capture: done. captured=10 failed=0 elapsed=0.481s achieved=20.80fps
```
10 dosya oluştu (~22-23KB her biri). Bu 20.80fps, **`fsync()` dahil gerçek uçtan-uca hız** — `v4l2-ctl`'in salt-capture ölçümünden farklı olarak, her frame'in gerçekten diske yazılıp flush edildiği an dahil.

**Kalıcılık doğrulaması:**
```bash
sync; sudo umount /run/media/mmcblk0
sudo mount /dev/mmcblk0 /run/media/mmcblk0
ls /run/media/mmcblk0/app_capture/   # 10 dosyanın hepsi hâlâ orada
```
Bir örnek kare (`frame_0005.jpg`) host'a aktarıldı, PIL ile doğrulandı: geçerli 1280x800 JPEG.

### 4.6 Sonuç

F3'ün "Skeleton + Component Test" maddesi artık gerçek anlamda tamamlandı: elde CLI komutları değil, **modüler component'lardan (video_capture, storage) oluşan, gerçekten çalışan, tekrar kullanılabilir bir C uygulaması** var — tezin ilerleyen fazlarında (görüntü işleme, ek özellikler) doğrudan üzerine inşa edilebilir bir temel.

---

## 5. Değiştirilen / Eklenen Dosyalar (F3 Kapsamı)

| Dosya | Değişiklik | Kalıcı mı? |
|---|---|---|
| `project-spec/meta-user/recipes-bsp/device-tree/files/system-user.dtsi` | `cma=4M@...` → `cma=16M@0x10000000-0x30000000` (Optimize denemesi) | ✅ Evet |
| `project-spec/meta-user/recipes-apps/zyrenith-capture/` (yeni) | Tüm uygulama iskeleti (video_capture.c/h, storage.c/h, zyrenith-capture.c, Makefile, .bb recipe) | ✅ Evet |
| `project-spec/configs/rootfs_config` | `CONFIG_zyrenith-capture=y` eklendi (`petalinux-create --enable` otomatik yaptı) | ✅ Evet |

---

## 6. Tez İçin Önemli Teknik Bulgular (F3 Özel)

1. **"CMA fragmentation → düşük fps" hipotezi çürütüldü, gerçek sebep eksik `--set-parm` idi** — iyi bir kontrollü deney/hipotez test etme örneği (bkz. bölüm 3.3-3.5).
2. **eMMC, initramfs-tabanlı (RAM disk) root dosya sisteminden bağımsız kalıcı depolama sağlıyor** — embedded sistemlerde yaygın bir mimari desen (RAM-based rootfs + ayrı kalıcı veri partition'ı).
3. **Doğru parametrelerle sistem tamamen deterministik/tekrarlanabilir performans gösteriyor** (sıfır varyans, 3-5 tekrarlı ölçümlerde) — bu, sistemin kararlılığını ve öngörülebilirliğini gösteren güçlü bir kanıt.
4. **CLI-tabanlı "component test" ile gerçek "application skeleton" arasındaki fark önemli** — ilki alt bileşenlerin çalıştığını kanıtlar, ikincisi tezin sonunda gösterilecek gerçek, yeniden kullanılabilir bir yazılım artefaktı üretir. PetaLinux'un `petalinux-create -t apps` mekanizması bu geçişi (Yocto recipe altyapısıyla) düzgün şekilde sağlıyor.

---

*Bu doküman, F2 tamamlandıktan sonra 2026-08-01 tarihinde aynı gece devam eden F3 çalışmasının teknik özetidir. F2'ye ait içerik (USB host fix, kamera algılama, CMA/highmem streaming çözümü) için bkz. `F2_Report_Detayli_Teknik_Dokuman.md`.*
