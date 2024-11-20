######################################################################
#                                                                    #
#             Temperature Monitoring and Data Insertion System       #
#                                                                    #
#                        ** Powered by Python **                     #
#                                                                    #
#   Features:                                                        #
#   - Fetch temperature data from ESP8266 devices                    #
#   - Automatically insert valid data into MySQL database            #
#   Custom logging with color-coded levels                           #
#   - Device discovery and management via UDP                        #
#   - Real-time temperature monitoring and timestamp management      #
#                                                                    #
#   Configurable:                                                    #
#   - Measurement intervals                                          #
#   - Max temperature difference settings                            #
#   - UDP communication settings                                     #
#                                                                    #
######################################################################

from mysql.connector.connection import MySQLConnection
from mysql.connector.cursor import MySQLCursor
from collections import defaultdict
from mysql.connector import Error
from dotenv import load_dotenv
from datetime import datetime
from icecream import ic
import mysql.connector
from os import getenv
import functools
import threading
import requests
import socket
import time
import json
import sys
import re
import os


interval: int = 0
measurement_interval: int = 0
max_temp_difference: float = 0.0
max_time_difference: float = 0.0
max_temp_difference_esp: float = 0.0
UDP_IP: str = "192.168.0.255"
UDP_PORT: int = 4210
lst_request_time: float = 0.0
LAN_host: str = ""
reset_board: bool = False
load_dotenv()


def load_config() -> None:
    """
    Load configuration settings from 'config.json'.
    """
    global interval, measurement_interval, max_temp_difference, max_time_difference, max_temp_difference_esp, UDP_IP, UDP_PORT, LAN_host, reset_board

    with open("data/config.json") as f:
        config = json.load(f)["dev"]
        if not config["DEBUG_mode"]:
            ic.disable()
        reset_board = config["RST_board_after_fail"]

    with open('data/config.json') as f:
        config = json.load(f)['config']
        interval = config['server-time-get']
        measurement_interval = config['device-time-measurement']
        max_temp_difference = config['max-difference']
        max_time_difference = config['max-time-difference']
        max_temp_difference_esp = config['max_temp_difference_esp']
        UDP_IP = config['UDP_host']
        UDP_PORT = config['UDP_port']
        LAN_host = config['LAN_host']

    ic(UDP_IP, UDP_PORT, LAN_host, interval, measurement_interval, max_temp_difference, max_time_difference, max_temp_difference_esp)
    ic("Develop variables", reset_board)


def dot_or_dash(char: str) -> None:
    print(char, end='', flush=True)


class Logger:
    """
    Custom logger to log program info and exceptions
    """
    LOG_COLORS = {
        'DEBUG': '\033[92m',
        'INFO': '\033[94m',
        'WARNING': '\033[93m',
        'ERROR': '\033[91m',
        'CRITICAL': '\033[41m'
    }
    RESET_COLOR = '\033[0m'

    def __init__(self, log_file: str = 'data/app.log', log_level: str = 'INFO') -> None:
        self.log_file = log_file
        self.log_level = log_level
        self.colored = sys.stdout.isatty() or 'PYCHARM_HOSTED' in os.environ
        self.log_levels = {'DEBUG': 10, 'INFO': 20, 'WARNING': 30, 'ERROR': 40, 'CRITICAL': 50}
        self.console_log_levels = {'DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL'}
        self.file_log_levels = {'WARNING', 'ERROR', 'CRITICAL'}
        self.current_log_level = self.log_levels.get(log_level, 50)

        os.makedirs(os.path.dirname(self.log_file), exist_ok=True)

    def _log(self, level: str, message: str) -> None:
        if level == 'IGNORE':
            print(message)
            return

        if self.log_levels[level] >= self.current_log_level:
            timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
            color = self.LOG_COLORS.get(level, self.RESET_COLOR) if self.colored else ""
            reset_color = self.RESET_COLOR if self.colored else ""
            log_message = f"{timestamp} - {level} - {message}"

            if level in self.console_log_levels:
                colorized_message = f"{timestamp} - {color}{level}{reset_color} - {message}"
                print(colorized_message)

            if level in self.file_log_levels:
                with open(self.log_file, 'a') as f:
                    f.write(log_message + '\n')

    def name(self, message: str) -> None:
        self._log('IGNORE', message)

    def debug(self, message: str) -> None:
        self._log('DEBUG', message)

    def info(self, message: str) -> None:
        self._log('INFO', message)

    def warning(self, message: str) -> None:
        self._log('WARNING', message)

    def error(self, message: str) -> None:
        self._log('ERROR', message)

    def critical(self, message: str) -> None:
        self._log('CRITICAL', message)


logger = Logger()

# Additional global variables
interval_between_json_load: int = 5000
lst_measure: float = time.time()
esp_devices: list = []
lst_check: float = 0.0
entry_time: float = 0.0
lst_device_upload: float = time.time()
lst_request_times: dict = {}
DATABASE_HOST: str = getenv('DATABASE_HOST')
DATABASE_PORT: int = int(getenv('DATABASE_PORT'))
DATABASE_USER: str = getenv('DATABASE_USER')
DATABASE_PASSWORD: str = getenv('DATABASE_PASSWORD')
DATABASE: str = getenv('DATABASE')
exc: list = []
disconnect: bool = False
esp_db: MySQLConnection
esp_cursor: MySQLCursor
sock: socket
last_mtime = 0


def get_database() -> None:
    """
    Connect or reconnect to the MySQL database.
    """
    global esp_db, esp_cursor
    try:
        esp_db = mysql.connector.connect(
            host=DATABASE_HOST,
            port=DATABASE_PORT,
            user=DATABASE_USER,
            password=DATABASE_PASSWORD,
            database=DATABASE,
            connection_timeout=60
        )
        esp_cursor = esp_db.cursor(buffered=True)
    except Error as e:
        logger.error(f"Error connecting to MySQL database: {e}")
        exit()


def insert_data(temp, timestamp, sensor_id) -> bool:
    """
    Inserts temperature data into a database if no duplicate entry exists for the same sensor and timestamp.
    """
    try:
        global esp_db, esp_cursor

        query_check = """
        SELECT id 
        FROM temp_data 
        WHERE sensor_id = %s 
        AND temp = %s 
        AND (time = %s OR time BETWEEN DATE_SUB(%s, INTERVAL 5 SECOND) AND %s)
        """
        esp_cursor.execute(query_check, (sensor_id, temp, timestamp, timestamp, timestamp))
        duplicate = esp_cursor.fetchone()

        if duplicate:
            return False

        query_insert = f"INSERT INTO temp_data (temp, time, sensor_id) VALUES ({temp}, '{timestamp}', {sensor_id})"
        esp_cursor.execute(query_insert)
        esp_db.commit()

        logger.info(f"Data inserted: sensor {sensor_id}, temp {temp}, timestamp {timestamp}")
        return True

    except Error as e:
        logger.error(f"Error inserting data: {e}")
        return False


def update_timestamp(sensor_id, new_time) -> None:
    """
    Updates the timestamp for the most recent entry of a given sensor in the database.
    """
    logger.info(f"Sensor_id : {sensor_id} new time: {new_time}")
    try:
        global esp_db, esp_cursor
        esp_cursor.execute(
            f"UPDATE temp_data SET time = '{new_time}' WHERE sensor_id = {sensor_id} ORDER BY id DESC LIMIT 1"
        )
        esp_db.commit()
    except Error as e:
        logger.error(f"Error updating timestamp: {e}")


def check_and_insert_data(temp, timestamp, sensor_id) -> None:
    """
    Checks the last two temperature records for a given sensor and decides whether to insert a new record or update
    the timestamp based on temperature and time differences. If conditions are met, it either inserts a new record or
    updates the timestamp accordingly.
    """
    global esp_db, esp_cursor

    esp_cursor.execute(
        "SELECT temp, time FROM temp_data WHERE sensor_id = %s ORDER BY id DESC LIMIT 2",
        (sensor_id,)
    )
    last_records = esp_cursor.fetchall()

    timestamp = datetime.fromtimestamp(timestamp)

    if len(last_records) >= 2:
        last_temp, last_time = last_records[0]
        last_last_temp, last_last_time = last_records[1]

        time_diff = abs(datetime.timestamp(timestamp) - datetime.timestamp(last_last_time))
        last_temp_diff = abs(float(last_temp) - float(last_last_temp))

        if last_temp_diff <= max_temp_difference and abs(float(last_last_temp) - float(temp)) <= max_temp_difference:
            if time_diff > max_time_difference:
                logger.info(
                    f"Inserting new record for sensor {sensor_id} due to time difference > {max_time_difference} seconds.")
                insert_data(temp, timestamp, sensor_id)
            else:
                logger.info(f"Moving timestamp due to no temperature change.")
                update_timestamp(sensor_id, timestamp)
        else:
            logger.info(f"Inserting new record for sensor {sensor_id} due to larger temperature change.")
            insert_data(temp, timestamp, sensor_id)
    else:
        logger.info(f"Inserting first record for sensor {sensor_id}")
        insert_data(temp, timestamp, sensor_id)

    if len(last_records) == 1:
        last_temp, last_time = last_records[0]
        if abs(datetime.timestamp(timestamp) - datetime.timestamp(last_time)) > max_time_difference:
            logger.info(f"Inserting new record for sensor {sensor_id} due to timestamp change.")
            insert_data(temp, timestamp, sensor_id)

    if len(last_records) == 0:
        logger.info(f"Inserting new record for sensor {sensor_id} due to no last temperatures.")
        insert_data(temp, timestamp, sensor_id)


def get_esp8266_data(device_ip: str, start_time: dict, max_retries=3, is_first_request=False, entry_time_ms=None, remain=0) -> bool:
    """
    Fetches temperature data from an ESP8266 device, processes it, and inserts valid records into the database. It
    retries up to a specified number of times if the request fails, and handles remaining data by recursively calling
    the function.
    """
    global esp_devices, exc, lst_request_times

    retry_count = 0
    response_data = None

    if is_first_request:
        get_database()
        try:
            while retry_count < max_retries:
                url = f"http://{device_ip}/temp?limit=100"
                response = requests.get(url, timeout=5)
                response.raise_for_status()
                if response.status_code != 204:
                    response_data = response.json()
                    break
        except:
            retry_count += 1
            if retry_count == max_retries:
                logger.error(f"Max retries reached for {device_ip}. Disconnecting device")
                _disconnect_device(device_ip)
                return False

        try:
            remain = response_data["remain"]
            temperatures = response_data["temperature_data"]
        except:
            logger.warning("Response data was empty")
            return False
        curr_time = time.time()

        rec = []
        for record in temperatures:
            sensor = record['id']
            temperature = record['t']
            entry_time_ms = record['ti']

            if sensor not in lst_request_times:
                lst_request_times[sensor] = 0

            if entry_time_ms > lst_request_times[sensor]:
                lst_request_times[sensor] = curr_time - entry_time_ms / 1000

            if int(temperature) != -127:
                check_and_insert_data(temperature, curr_time - entry_time_ms / 1000, sensor)
                if not any(sensor_id[0] == sensor for sensor_id in rec):
                    rec.append((sensor, 1))
                else:
                    for i in range(len(rec)):
                        if rec[i][0] == sensor:
                            rec[i] = (sensor, rec[i][1] + 1)

        rec = sorted(rec, key=lambda x: x[0])
        message = ', '.join(f'sensor {sensor_id}: {count} rec' for sensor_id, count in rec)
        if message != '':
            logger.info(f"Read {message}, remaining {remain} records from sensors")

        if remain > 0:
            get_esp8266_data(device_ip, start_time, is_first_request=False, entry_time_ms=entry_time_ms, remain=remain)
        return True

    retry_count = 0

    while retry_count < max_retries:
        try:
            if remain > 0:
                url = f"http://{device_ip}/temp?time={entry_time_ms}&limit={100}"
            else:
                url = f"http://{device_ip}/temp?time={int(round(time.time() - min(start_time.values()))) + 10}&limit=100"

            response = requests.get(url, timeout=5)
            response.raise_for_status()
            if response.status_code != 204:
                response_data = response.json()
                break

        except:
            retry_count += 1
            if retry_count == max_retries:
                logger.error(f"Max retries reached for {device_ip}. Disconnecting device")
                _disconnect_device(device_ip)
                return False

    try:
        remain = response_data["remain"]
        temperatures = response_data["temperature_data"]
    except:
        logger.warning("Response data was empty")
        return False
    curr_time = time.time()

    rec = []
    get_database()
    for record in temperatures:
        temperature = record['t']
        entry_time_ms = record['ti']
        sensor_id = record['id']

        if curr_time - entry_time_ms / 1000 > start_time.get(sensor_id, 0):
            lst_request_times[sensor_id] = curr_time - entry_time_ms / 1000

        check_and_insert_data(temperature, curr_time - entry_time_ms / 1000, sensor_id)

        if not any(sensor[0] == sensor_id for sensor in rec):
            rec.append((sensor_id, 1))
        else:
            for i in range(len(rec)):
                if rec[i][0] == sensor_id:
                    rec[i] = (sensor_id, rec[i][1] + 1)

    rec = sorted(rec, key=lambda x: x[0])

    message = ', '.join(f'sensor {sensor_id}: {count} rec' for sensor_id, count in rec)
    if message != '':
        logger.info(f"Read {message}, remaining {remain} records from sensors")

    if remain > 0:
        get_esp8266_data(device_ip, start_time, is_first_request=False, entry_time_ms=entry_time_ms, remain=remain)

    return True


def _disconnect_device(device_ip: str) -> bool or None:
    global esp_devices, exc, disconnect

    if not reset_board:
        logger.warning(f"Device {device_ip} was disconnected")
        for i in range(0, len(esp_devices)):
            if esp_devices[i][0] == device_ip:
                del lst_request_times[esp_devices[i][1]]
        esp_devices = [item for item in esp_devices if item[0] != device_ip]
        disconnect = True
        return False
    else:
        try:
            response = requests.get(f"http://{device_ip}/exit", timeout=5)
            if response.status_code == 200:
                logger.info(f"Device {device_ip} was reset")
                return True
            else:
                logger.error("Device not responding. Try to restart device.")
                logger.warning(f"Device {device_ip} was disconnected")
                for i in range(0, len(esp_devices)):
                    if esp_devices[i][0] == device_ip:
                        del lst_request_times[esp_devices[i][1]]

                esp_devices = [item for item in esp_devices if item[0] != device_ip]
                disconnect = True
                return False

        except Exception:
            logger.error("Device not responding. Try to restart device.")
            logger.warning(f"Device {device_ip} was disconnected")
            for i in range(0, len(esp_devices)):
                if esp_devices[i][0] == device_ip:
                    del lst_request_times[esp_devices[i][1]]

            esp_devices = [item for item in esp_devices if item[0] != device_ip]
            disconnect = True
            return False


def get_devices() -> None:
    """
    Disconnects a device by either resetting it via an HTTP request or simply removing it from the list of devices.
    It logs the disconnection and removes associated data from esp_devices and lst_request_times.
    """
    try:
        global sock, exc
        message = f"DISCOVER"

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        sock.bind(("0.0.0.0", UDP_PORT))
        sock.sendto(message.encode(), (UDP_IP, UDP_PORT))
        sock.settimeout(2)

        sock.sendto(message.encode(), (UDP_IP, UDP_PORT))

        while True:
            try:
                data, addr = sock.recvfrom(1024)
            except TimeoutError:
                break

            if data:
                data = data.decode()
                if "DEVICE" in data:
                    match = re.search(r'\[([0-9, ]+)\]', data)
                    list_string = match.group(0)
                    device = eval(list_string)

                    for i in range(0, len(device)):
                        found = any(device[i] in sublist for sublist in esp_devices)
                        if not found:
                            esp_devices.append([addr[0], device[i]])
                            logger.info(f"Added meter device {addr[0]} with id: {device[i]}")
                            if device[i] not in lst_request_times:
                                lst_request_times[device[i]] = 0
                            exc.append([addr[0], device[i]])
            else:
                break
    except OSError:
        check_internet_connection()


def set_params() -> None:
    """
    Discovers devices on the network by sending a broadcast message and listening for responses.
    """
    for i in range(0, len(esp_devices)):
        try:
            while True:
                url = f"http://{esp_devices[i][0]}/setinterval?interval={measurement_interval}"
                response = requests.get(url, timeout=5)
                if response.status_code == 200:
                    break
            logger.info(
                f"Time for measurement ({measurement_interval} milliseconds) was set for {esp_devices[i][0]}")
        except Exception:
            logger.error(f"Cant set time for {esp_devices[i][0]}")

        try:
            while True:
                url = f"http://{esp_devices[i][0]}/setTempDiff?difference={max_temp_difference_esp}"
                response = requests.get(url)
                if response.status_code == 200:
                    break
            logger.info(
                f"Max temp difference ({max_temp_difference_esp}C) was set for {esp_devices[i][0]}")
        except Exception:
            logger.error(f"Cant set max temp difference for {esp_devices[i][0]}")


def internet_on() -> bool:
    """
    Checks internet connection
    """
    try:
        s = socket.create_connection(
            (LAN_host, 80))
        if s is not None:
            s.close
        return True
    except OSError:
        return False


def check_internet_connection():
    dots = 0
    lines = 0
    if not internet_on():
        logger.warning("No internet connection.")
        while not internet_on():
            if dots < 5:
                lines = 0
                dot_or_dash(".")
                dots += 1
            elif lines < 5:
                dot_or_dash("_")
                lines += 1
                if lines == 5:
                    dots = 0
            time.sleep(1)
        dot_or_dash("\n")
    return True


def setup():
    load_config()
    get_database()
    logger.info('Connecting devices...')

    check_internet_connection()

    for _ in range(5):
        get_devices()

    for device in esp_devices:
        if check_internet_connection():
            get_esp8266_data(device_ip=device[0], start_time=lst_request_times, is_first_request=True)
        else:
            time.sleep(1)


logger.name(r'    ___________ ____  ______                     __  ___            _ __            ')
logger.name(r'   / ____/ ___// __ \/_  __/__  ____ ___  ____  /  |/  /___  ____  (_) /_____  _____')
logger.name(r'  / __/  \__ \/ /_/ / / / / _ \/ __ `__ \/ __ \/ /|_/ / __ \/ __ \/ / __/ __ \/ ___/')
logger.name(r' / /___ ___/ / ____/ / / /  __/ / / / / / /_/ / /  / / /_/ / / / / / /_/ /_/ / /    ')
logger.name(r'/_____//____/_/     /_/  \___/_/ /_/ /_/ .___/_/  /_/\____/_/ /_/_/\__/\____/_/     ')
logger.name('              by yar2011t             /_/                             v1.4.0       \u00A9')

if __name__ == '__main__':
    """
    Main loop that continuously checks for an internet connection and performs the following tasks:

     1. Fetches data from ESP8266 devices if the connection is available, using get_esp8266_data().
     2. Loads and updates configuration parameters periodically.
     3. Discovers new devices and processes them if no devices are connected or if it's time to check for new ones.
    """

    setup()

    while True:

        if check_internet_connection():
            if time.time() - lst_check > interval / 1000:
                disconnect = False

                grouped_dict = defaultdict(list)
                for key, value in esp_devices:
                    grouped_dict[key].append(value)

                devices = [(key, value) for key, value in grouped_dict.items()]

                for ip, _ in devices:
                    if not disconnect:
                        get_esp8266_data(device_ip=ip, start_time=lst_request_times)
                    else:
                        break

                lst_check = time.time()

        if time.time() - lst_measure > interval_between_json_load / 1000:
            current_mtime = os.path.getmtime("data\\config.json")

            if current_mtime != last_mtime:
                last_mtime = current_mtime
                load_config()
                set_params()
                logger.info("Configuration params were successfully updated")
                lst_measure = time.time()

        if time.time() - lst_device_upload > 60:  #or len(esp_devices) == 0:
            logger.info('Pending new devices...')
            for _ in range(10):
                get_devices()

            for device in exc:
                if check_internet_connection():
                    get_esp8266_data(device_ip=device[0], start_time=lst_request_times, is_first_request=True)
                    exc.remove(device)

            lst_device_upload = time.time()
        else:
            time.sleep(1)
