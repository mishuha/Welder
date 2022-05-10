# Welder
Arduino transformer welding controller with 2 impulses mode.  
  
Program configurable variables during work:  
1. preheat cycle time  
2. pause  
3. main cycle time  

Power is justified manually by dimmer (20-40 Amps simistor is required)

All cycles start on feedback impulse. SSR is supposed to commutate load after crossing zero level of sin wave. If no feedback is received in any of the cycles an error will occur and welding will be aborted.  

Electric schema:
![Schematic_welder_2022-05-10](https://github.com/mishuha/Welder/blob/main/Schematic_welder_2022-05-10.png)
