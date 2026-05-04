# WEB-AGENT

WEB-AGENT — фоновый C++17-агент, который опрашивает Web API, получает задания, выполняет локальные команды или программы, собирает текстовый результат и файлы, после чего отправляет их обратно на сервер.

## Что реализовано

* загрузка JSON-конфига и переопределение `access_code` через переменную окружения
* CLI-опция `--config <file>` и совместимый позиционный запуск `web-agent <config-file>`
* проверка доступности сервера
* регистрация агента через `wa_reg` с fallback на `access_code` из конфига
* периодический опрос `wa_task`
* выполнение заданий `TASK`, `CONF`, `FILE` и API-формата `task_code` + `options`
* `options` как строка команды или JSON-объект по схеме [docs/options.schema.json](docs/options.schema.json)
* опция `options.file`, получающая путь к исполняемому файлу/скрипту, с аргументами строкой или массивом
* отправка `result_code`, JSON-результата и файлов `file1..fileN` в `wa_result`
* параллельное выполнение нескольких задач
* backoff при сетевых ошибках
* кроссплатформенная сборка Windows/Linux/macOS и CPack-архивы для скачивания

## Конфиг

Пример находится в [config/agent_config.example.json](config/agent_config.example.json).

Ключевые поля:

* `uid` — уникальный идентификатор агента
* `server_url` — базовый URL сервера, например `https://xdev.arkcom.ru:9999`
* `access_code` — код доступа, если регистрация на сервере недоступна или агент уже зарегистрирован
* `access_code_env` — имя переменной окружения, которая переопределяет `access_code`
* `poll_path`, `result_path`, `register_path` — пути API
* `allow_insecure_http` — разрешить `http://`; по умолчанию нужен `https://`
* `ca_cert_file`, `ca_cert_dir` — путь к кастомному CA bundle/каталогу сертификатов
* `poll_interval_sec` — базовый интервал опроса
* `max_parallel_tasks` — максимум одновременно исполняемых задач
* `task_directory`, `result_directory` — рабочие каталоги агента
* `log_file` — журнал действий

Для production не храните секретный `access_code` в файле: оставьте поле пустым и задавайте секрет через `WEB_AGENT_ACCESS_CODE`.

## Сборка

Нужны CMake, C++17-компилятор и OpenSSL development package.

```bash
cmake -S . -B build -DWEB_AGENT_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

На Windows итоговый файл будет `web-agent.exe`, на Linux/macOS — `web-agent`.

## CMake-опции

* `WEB_AGENT_BUILD_TESTS` — собирать тесты, по умолчанию `ON`.
* `WEB_AGENT_ENABLE_INSTALL` — включить `cmake --install` и CPack, по умолчанию `ON`.
* `WEB_AGENT_STRICT_WARNINGS` — строгие предупреждения компилятора, по умолчанию `OFF`.
* `WEB_AGENT_DEFAULT_CONFIG` — FILEPATH-опция с путем к конфигу по умолчанию.
* `WEB_AGENT_PACKAGE_PLATFORM` — суффикс платформы в имени архива.
* `WEB_AGENT_CONFIG_INSTALL_DIR` — каталог установки примера конфига.

Пример сборки Windows-архива:

```bash
cmake -S . -B build -DWEB_AGENT_PACKAGE_PLATFORM=Windows-x64
cmake --build build --config Release
cpack --config build/CPackConfig.cmake
```

Подробности деплоя и скачиваемых архивов описаны в [docs/deploy.md](docs/deploy.md).

## Запуск

```bash
web-agent --config config/agent_config.json
```

Также работает старый формат:

```bash
web-agent config/agent_config.json
```

Полезные команды:

```bash
web-agent --help
web-agent --version
```

## Документация

* [docs/api.md](docs/api.md) — контракт `wa_reg`, `wa_task`, `wa_result` из PDF.
* [docs/options.md](docs/options.md) — формат JSON-поля `options` и примеры.
* [docs/options.schema.json](docs/options.schema.json) — формальная JSON Schema для `options`.
* [docs/deploy.md](docs/deploy.md) — сборка архивов, установка и деплой.
* [docs/technical_spec.md](docs/technical_spec.md) — технические заметки по поведению агента.

Шаблоны деплоя находятся в [deploy](deploy): `systemd` для Linux, `launchd` для macOS и PowerShell-скрипты сервиса для Windows.
