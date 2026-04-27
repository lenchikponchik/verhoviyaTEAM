# WEB-AGENT

WEB-AGENT — фоновый C++-агент, который опрашивает Web API, выполняет локальные команды, собирает текстовый результат и файлы, после чего отправляет их обратно на сервер.

## Что реализовано

* загрузка JSON-конфига
* проверка доступности сервера
* регистрация агента с fallback на `access_code` из конфига
* периодический опрос `wa_task`
* выполнение заданий `TASK`, `FILE` и API-формата `task_code` + `options`
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
* `access_code_env` — имя переменной окружения, которая переопределяет `access_code`
* `allow_insecure_http` — разрешить небезопасный `http://` (по умолчанию `false`, рекомендуется `https://`)
* `ca_cert_file` и `ca_cert_dir` — путь к кастомному CA bundle/директории сертификатов
* `poll_interval_sec` — базовый интервал опроса
* `max_parallel_tasks` — максимум одновременно исполняемых задач
* `task_directory` и `result_directory` — рабочие каталоги агента

По умолчанию агент использует API из задания: `wa_reg`, `wa_task`, `wa_result`.
Контракт API вынесен в [docs/api.md](/home/cry/CLionProjects/verhoviyaTEAM/WEB-AGENT/docs/api.md).

Для production не храните `access_code` в файле конфига: оставьте поле пустым и задавайте секрет через переменную окружения `WEB_AGENT_ACCESS_CODE`.

## Сборка

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

Нужны `CMake`, C++17-компилятор и OpenSSL development headers.

## Запуск

```bash
./web_agent config/agent_config.json
```

Если путь не передан, агент пытается открыть `config/agent_config.json`.

Для фонового запуска на Linux подготовлен systemd unit-шаблон:
[deploy/systemd/web-agent.service](/home/cry/CLionProjects/verhoviyaTEAM/WEB-AGENT/deploy/systemd/web-agent.service).
