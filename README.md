# verhoviyaTEAM

Проект **WEB-AGENT** — это кроссплатформенный агент на языке **C++**, который взаимодействует с сервером через Web API.

Агент автоматически:

* получает задачи от сервера
* выполняет команды
* отправляет результаты выполнения
* передает файлы при необходимости

---

# Структура проекта

```id="40004"
verhoviyaTEAM
│
└── WEB-AGENT
    │
    ├── src
    │   ├── main.cpp
    │   └── httplib.h
    │
    ├── config
    │   └── agent_config.example.json
    │
    ├── docs
    │   └── technical_spec.md
    │
    ├── tests
    │   └── test_environment.cpp
    │
    ├── CMakeLists.txt
    └── README.md
```

---

# Требования

Для сборки проекта необходимо установить:

* CMake
* компилятор C++ (GCC или Clang)
* OpenSSL

Установка зависимостей:

```id="40005"
sudo apt install libssl-dev
```

---

# Сборка проекта

```id="40006"
mkdir build
cd build
cmake ..
make
```

---

# Запуск программы

```id="40007"
./web_agent
```

---

# Пример работы программы

```id="40008"
Checking task...

SERVER RESPONSE:
{"status":"RUN","session_id":"XXXXX"}

SESSION: XXXXX

RESULT RESPONSE:
{"code_responce":"0","msg":"ok"}
```

---

# Используемые технологии

* C++
* CMake
* OpenSSL
* cpp-httplib
* REST API

---

# Документация

Техническое описание проекта находится в:

```
WEB-AGENT/docs/technical_spec.md
```
