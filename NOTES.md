# Flashing

OpenOCD leaves the debug port active after flashing.
This leads to constant current draw of around 1.1 mA.
Use this command to disable it:
```
openocd -f board/nordic_nrf52_dk.cfg -c 'init; nrf52.dap dpreg 4 0x04000000; shutdown'
```
