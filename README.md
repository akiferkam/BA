# BA — Zyrenith Linux

Bachelorarbeit projesi: özel Zynq-7000 kart üzerinde PetaLinux 2022.2 tabanlı Linux bring-up ve USB 2.0 HS UVC kamera pipeline'ı (V4L2 capture/stream), eMMC kalıcı depolama entegrasyonu ve bunları birleştiren bir uygulama iskeleti.

## İçerik

- `project-spec/` — PetaLinux proje kaynak dosyaları (device-tree override'ları, kernel/u-boot config fragment'ları, rootfs paket seçimi, özel uygulama recipe'i). Build çıktıları (`build/`, `images/linux/`, `components/yocto`) `.gitignore` ile hariç tutulmuştur — `petalinux-build` ile yeniden üretilebilir.
- `project-spec/meta-user/recipes-apps/zyrenith-capture/` — kamera görüntüsünü yakalayıp eMMC'ye kaydeden, modüler (video_capture + storage component'ları) bir C uygulaması.
- `docs/` — F2 (USB Camera Pipeline) ve F3 (Application/Testing) çalışma paketlerine ait ayrıntılı teknik raporlar ve kanıt logları/görselleri.

## Öne çıkan teknik çözümler

- **USB host controller fix**: `chipidea` sürücüsü için eksik PHY device-tree tanımı (`usb-nop-xceiv`) eklendi.
- **CMA/highmem streaming fix**: 32-bit ARM + 1GB DRAM'de varsayılan CMA yerleşiminin highmem'e düşmesi sorunu, `cma=` kernel bootarg parametresiyle (device-tree `reserved-memory` node'u yerine — o, bu board'da U-Boot'u kilitliyordu) çözüldü.
- **eMMC kalıcı depolama**: initramfs tabanlı (RAM disk) root dosya sisteminden bağımsız, gerçek kalıcı depolama.
- **Bandwidth/latency karakterizasyonu**: kontrollü deneylerle "CMA fragmentation" hipotezinin çürütülüp gerçek kök nedenin (`v4l2-ctl --set-parm` ile frame rate'in explicit set edilmemesi) bulunması — bkz. `docs/F3_Report_Detayli_Teknik_Dokuman.md`.

Ayrıntılar için `docs/` altındaki raporlara bakın.

## Kurulum / Build

**Gereksinimler:** Vivado 2022.2 (hardware handoff — `.xsa` — üretmek için) ve PetaLinux 2022.2 (Linux tool-chain).

```bash
# 1) Hardware description'ı içe aktar (Vivado'dan export edilen .xsa)
petalinux-config --get-hw-description project-spec/hw-description/

# 2) Tam build (FSBL, U-Boot, kernel, device-tree, rootfs)
petalinux-build

# 3) JTAG üzerinden test boot'u (host bilgisayardan, geliştirme için)
petalinux-boot --jtag --kernel
```

`tools/` altındaki yardımcı script (`hex_to_jpg.py`) hakkında bkz. `tools/README.md`.
