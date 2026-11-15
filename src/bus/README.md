# Módulo `bus`

**Estado: implementado (Fase 2).**

- `address.hpp` — segmentos KUSEG/KSEG0/KSEG1/KSEG2, tradução para endereço físico,
  larguras de acesso e alinhamento.
- `bus.hpp` — barramento: despacha acessos de 8/16/32 bits para RAM (janela do DRAM_CTRL),
  scratchpad, BIOS ROM, Expansões 1/2/3 (janelas dos registradores Delay/Size), Memory Control,
  periféricos de E/S e controle de cache (KSEG2). Devolve `BusFault::AddressError`
  (desalinhamento → AdEL/AdES) ou `BusFault::BusError` (→ IBE/DBE) para a CPU.
- `io_device.hpp` — interface dos periféricos e `UnimplementedDevice`, o marcador explícito
  usado enquanto cada periférico não é implementado.

Ainda não modelado: temporização de acesso (Fase 5), fila de escrita (write queue),
isolamento de cache (Fase 4), realocação da Expansão 1.
