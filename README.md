# Orbit 2.0

Sıfırdan yazılmış, 64-bit (x86_64), grafik arayüzlü (GUI), modüler işletim sistemi.

## Özellikler

- Kendi bootloader'ı (NASM, INT 13h LBA ile çekirdeği yükler, VBE 1024x768x32 video modu, A20, GDT, protected mode → long mode)
- 64-bit C çekirdeği (freestanding, flat binary, `0x10000`, 2 MB sayfalarla 4 GB identity map)
- GDT / IDT / ISR / IRQ / PIC / PIT kesme altyapısı (64-bit)
- VBE lineer framebuffer üzerinde çift tamponlu kompozitör
- Masaüstü ortamı: duvar kağıdı, görev çubuğu, başlat menüsü, saat
- Pencere yöneticisi: sürüklenebilir pencereler, odak, gölge, kapatma düğmesi
- Terminus bitmap fontu ile metin çizimi
- PS/2 fare ve klavye, seri giriş (başsız/headless da sürülebilir)
- Uygulamalar: Terminal (kabuk), Files, System Monitor, About
- RAM dosya sistemi, kullanıcılar, süreçler, ağ yığını (RTL8139, ARP, IP, ICMP, UDP, DHCP)
- Heap (kmalloc/kfree)

## Bellek Haritası

| Adres | İçerik |
|-------|--------|
| `0x01000`–`0x07000` | Sayfa tabloları (PML4/PDPT/PD) |
| `0x07C00` | Bootloader |
| `0x08000` | VBE mod bilgisi |
| `0x10000` | Çekirdek (text/data/bss) |
| `0x400000`–`0x4400000` | Heap (64 MB) |
| VBE LFB | Framebuffer (genelde `0xFD000000`) |

## Derleme ve Çalıştırma

WSL içinde:

```bash
./build.sh
./run.sh            # QEMU GUI
./run.sh headless   # seri konsol
```
