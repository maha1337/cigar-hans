# Cigar-Hans: Fully Automatic Cigarbox Humidifier
Arduino project, automatic cigar box humidifier using Arduino Uno with water atomizer, humidity sensors, motor (fan) control and display.  
Dependent on humidity sensors, water atomizer will water inside of humidor and fan will distribute water until a certain level of humidity is reached.  
Around 70% humidity is perfect for storing cigars inside of humidor, so this valus is pre-set.


### Required libs:
* Adafruit-ST7735-Library and ST77789 Library https://github.com/adafruit/Adafruit-ST7735-Library
* Grove Temperature and Humidity Sensor https://github.com/Seeed-Studio/Grove_Temperature_And_Humidity_Sensor
* SparkFun MiniMoto https://github.com/sparkfun/SparkFun_MiniMoto_Arduino_Library
* Adafruit BusIO https://github.com/adafruit/Adafruit_BusIO


### Version History

* TODO
  * Adaptive humidifying time based on history
  * ...Ideas
* 0.0.4
  * Replace string operations by prints to save dynamic memory
  * Print elapsed time between the last humidification process (only possible from "device-on" time)
  * Adjusted humidifying time to hardcoded 10s, 5s was not sufficient. Depending on humidor size.
  * Added RM version history
* 0.0.3
  * First stable version
