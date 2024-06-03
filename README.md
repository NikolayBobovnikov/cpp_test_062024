# Описание проекта

## Описание сервера и клиента

### Сервер

Серверная часть проекта написана на языке C++ и использует следующие библиотеки:

- **Boost Asio** для сетевого взаимодействия.
- **RapidJSON** для работы с JSON форматом.
- **YAML-CPP** для работы с конфигурационными файлами в формате YAML.

#### Конфигурация сервера

Сервер настраивается с помощью YAML файла. Пример конфигурационного файла:

```yaml
server:
  host: "127.0.0.1"
  port: 12345
  stats_timeout: 5
  file_write_timeout: 5
  key_values_file: "key_values.json"
```


##### Для простоты реализации было принято следующее:

- Ожидается наличие конфиг файла ```server.yaml``` для сервера в директории ```conf``` в одной из родительских директорий
- Создается пустой файл с парами ключ-значение, если он не существует в ```conf```
- Когда выполняется команда set с отсутствующим ключом, он создается и добавляется.

### Клиент
Клиентская часть написана на Python в 2х 'запускаемых' файлах:
- client.py
- multi.py

Клиент не конфигурируется, хост-порт-остальные параметры захардкодены. При запуске ```client.py``` запускается клиент выполняющий действия из задания, при запуске ```multi.py``` запускается 100 инстансов клиента.

#### Запуск клиента
Ожидается наличие <client_id> в параметрах
```py
python client.py <client_id>
```
(Используется при запуске нескольких инстансов клиента)

## Сборка проекта
Для сборки проекта используется система CMake. Ниже приведены шаги по сборке проекта на Windows:

- Установите CMake.
- Скачайте и установите Boost библиотеки.
- Клонируйте репозиторий проекта.
- Откройте командную строку и перейдите в директорию проекта.
- Создайте директорию сборки:
```sh
   mkdir build
   cd build
   cmake ..
```  
- RapidJSON и YAML-CPP библиотеки скачиваются при конфигурировании cmake проекта
