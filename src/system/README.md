# Módulo `system`

**Estado: implementado (Fases 4 e 5).**

- `console.hpp` — o console emulado: possui barramento, controlador de interrupções, CPU, scheduler,
  temporização de vídeo e timers; liga os dispositivos ao barramento e oferece `power_on()`,
  `reset()`, `load_bios()` e `run()` (despacha eventos antes de cada instrução).
- `scheduler.hpp` — eventos por ciclo da CPU, um por componente.
- `clock.hpp` — base de tempo (33,8688 MHz) e conversão exata para os cristais de vídeo.
