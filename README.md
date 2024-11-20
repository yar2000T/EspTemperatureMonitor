# EspTemperatureMonitor
This project uses esp8266 and python to measure temperatures and write to MySQL database.

# Installation

# Requirements
Python >= 3.12
Pip
MySql database
Esp8266

## pip
Install packages using pip `pip install -r requirements.txt`

## Mysql
You need to create a database named **esp_data**. Then you can use this script to create a table:

``CREATE TABLE 'temp_data' (
  'id' int NOT NULL AUTO_INCREMENT,
  'temp' float NOT NULL,
  'time' timestamp NOT NULL,
  'sensor_id' int NOT NULL,
  PRIMARY KEY ('id')
) ENGINE=InnoDB AUTO_INCREMENT=0 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;``

or you can run ``setup.py`` file.


### Esp8266
To use this project you need an esp8266 with wifi support. You need only
1. port D7, 3v, GND.
2. a temperature sensor like 0750C3
3. installed esp8266 plugin (fast [tutorial]( https://www.youtube.com/watch?v=OC9wYhv6juM&ab_channel=RuiSantos "tutorial"))
4. know your wifi ssid and password

#### Connecting esp8266 board
Here is a photo of the schema that is used:


![](https://github.com/yar2000T/EspTemperature/blob/main/schema.png?raw=true)

> [!TIP]
> You can use any esp8266 board and connect up to 8 sensors to pin D7.

### End
