# WEB-AGENT

WEB-AGENT — фоновый C++-агент, который опрашивает Web API, выполняет локальные команды, собирает текстовый результат и файлы, после чего отправляет их обратно на сервер.

## Что реализовано

* загрузка JSON-конфига
* проверка доступности сервера
* регистрация агента с fallback на `access_code` из конфига
* периодический опрос `wa_task`
* выполнение заданий `TASK` и `FILE`
* отправка текстового результата и файлов в `wa_result`
* логирование действий и ошибок
* параллельное выполнение нескольких задач
* backoff при сетевых ошибках

## Конфиг

Пример находится в [config/agent_config.example.json](/home/cry/CLionProjects/verhoviyaTEAM/WEB-AGENT/config/agent_config.example.json).

Ключевые поля:

* `uid` — уникальный идентификатор агента
* `server_url` — базовый URL сервера, например `https://xdev.arkcom.ru:9999`
* `access_code` — код доступа, если регистрация на сервере недоступна
* `poll_interval_sec` — базовый интервал опроса
* `max_parallel_tasks` — максимум одновременно исполняемых задач
* `task_directory` и `result_directory` — рабочие каталоги агента

## Сборка

```bash
mkdir build
cd build
cmake ..
make
```

Нужны `CMake`, C++17-компилятор и OpenSSL development headers.

## Запуск

```bash
./web_agent config/agent_config.json
```

Если путь не передан, агент пытается открыть `config/agent_config.json`.
