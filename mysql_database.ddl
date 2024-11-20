CREATE TABLE `sensorId_list` (
  `id` int NOT NULL,
  `location` varchar(100) DEFAULT NULL,
  UNIQUE KEY `sensorId_list_unique` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE `temp_data` (
  `id` int NOT NULL AUTO_INCREMENT,
  `temp` float NOT NULL,
  `time` timestamp NOT NULL,
  `sensor_id` int NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=0 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
