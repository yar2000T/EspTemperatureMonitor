from mysql.connector import Error
from dotenv import load_dotenv
import mysql.connector
from os import getenv
import time

print(r'    ___________ ____  ______                     __  ___            _ __            ')
print(r'   / ____/ ___// __ \/_  __/__  ____ ___  ____  /  |/  /___  ____  (_) /_____  _____')
print(r'  / __/  \__ \/ /_/ / / / / _ \/ __ `__ \/ __ \/ /|_/ / __ \/ __ \/ / __/ __ \/ ___/')
print(r' / /___ ___/ / ____/ / / /  __/ / / / / / /_/ / /  / / /_/ / / / / / /_/ /_/ / /    ')
print(r'/_____//____/_/     /_/  \___/_/ /_/ /_/ .___/_/  /_/\____/_/ /_/_/\__/\____/_/     ')
print('              by yar2011t             /_/                             v1.4.0       \u00A9')
print("                                     SETUP")

print("Welcome to ESPTempMonitor setup! To use this project you need setup your mysql database.")
database_host = input("Enter your mysql database host (leave blank if localhost):")
if database_host == "":
    database_host = "localhost"

database_port = input("Enter your mysql database port (leave blank if 3306):")
if database_port == "":
    database_port = 3306

database_user = input("Enter your mysql database user:")

database_password = input("Enter your mysql database password (leave blank if no password is set):")
if database_password == "":
    database_password = "no_password"

while True:
    database_name = input("Enter your database name to connect to database:")
    if database_name != "":
        break
    else:
        print("Database name cant be blank")

ENV = f'''DATABASE_HOST={database_host}
DATABASE_PORT={database_port}
DATABASE_USER={database_user}
DATABASE_PASSWORD={database_password}
DATABASE={database_name}
'''

open('.env', 'w').write(ENV)

load_dotenv()

DATABASE_HOST = getenv('DATABASE_HOST')
DATABASE_PORT = int(getenv('DATABASE_PORT'))
DATABASE_USER = getenv('DATABASE_USER')
DATABASE_PASSWORD = getenv('DATABASE_PASSWORD')
DATABASE = getenv('DATABASE')

try:
    esp_db = mysql.connector.connect(
        host=DATABASE_HOST,
        port=DATABASE_PORT,
        user=DATABASE_USER,
        password=DATABASE_PASSWORD,
        database=DATABASE
    )
    esp_cursor = esp_db.cursor()

except Error as e:
    print(f"Error connecting to MySQL database: {e}")
    exit()

create = input("Do you want to automaticly create tables in database (Y/N):")
if create == "Y" or create == "y" or create == "Yes" or create == "yes":
    with open('mysql_database.ddl', 'r') as ddl_file:
        ddl_content = ddl_file.read()

    x = ddl_content.split("\n")

    y = []
    y1 = []
    bo = False
    for i in range(0, len(x)):
        if x[i] == '':
            bo = True
        if bo == False:
            y.append(x[i])
        else:
            y1.append(x[i])

    query_string = ''.join(y)
    query_string2 = ''.join(y1)

    try:
        esp_cursor.execute(query_string)
        esp_db.commit()
    except mysql.connector.errors.ProgrammingError:
        print("Maybe you already created table (check your database)")

    try:
        esp_cursor.execute(query_string2)
        esp_db.commit()
    except mysql.connector.errors.ProgrammingError:
        print("Maybe you already created table (check your database)")

    esp_cursor.close()
    esp_db.close()
else:
    create = False

print("Congratulations! Setup is finished. Exiting...")
time.sleep(2)
exit()