# Orbit 1.0

Sıfırdan yazılmış, tamamen komut satırı (CLI) tabanlı, modüler, 32-bit x86 işletim sistemi.

## Özellikler

- Kendi bootloader'ı (NASM, INT 13h LBA ile çekirdeği yükler, A20, GDT, protected mode)
- 32-bit C çekirdeği (freestanding, flat binary, `0x10000`)
- GDT / IDT / ISR / IRQ / PIC / PIT kesme altyapısı
- VGA metin terminali + COM1 seri konsol (her ikisi aynı anda)
- PS/2 klavye **ve** seri giriş (başsız/headless de sürülebilir)
- Heap (kmalloc/kfree)

## Bellek Haritası

| Adres | İçerik |
|-------|--------|
| `0x07C00` | Bootloader |
| `0x10000` | Çekirdek (text/data/bss) |
| `0xB8000` | VGA metin belleği |
| `0x200000`–`0x1200000` | Heap (16 MB) |