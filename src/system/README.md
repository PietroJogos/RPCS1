# Módulo `system`

**Estado: implementado (Fase 4).**

`console.hpp` — o console emulado: possui o barramento, o controlador de interrupções e a CPU,
liga o controlador ao barramento (1F801070h) e à linha de interrupção da CPU, e oferece
`power_on()`, `reset()` e `load_bios()`. Cada novo componente será integrado aqui.
