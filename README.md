# kissmapper
An arduino project to turn a E&amp;A KISS LoRa board into a basic TTN mapper

To program the board using platformio:
- turn the power switch to off
- plug in power
- hold the push button
- turn the power switch to on
- run:
  pio run -t upload
  
To interact with the serial command prompt using platformio:
- run:
  pio device monitor

To configure the TTN parameters in the board:
- plug in USB and turn the power switch to on
- to enter the serial command prompt, run:
  pio device monitor
- copy the device address, network session key, app session key from the TTN console into the serial command prompt:
  ttn abp device_address netwerk_session_key app_session_key

To use it as a mapper:
- unplug from USB and turn the power switch to on
- the LED shows yellow while initializing, then turns to green when OK (red when not OK)
- during each transmission, the LED turns blue, then off (red when not OK)
- configure the TTN mapper application on your phone
  - TODO

