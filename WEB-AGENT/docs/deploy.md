# Деплой и скачивание сборок

Проект собирается одним CMake-файлом на Windows, Linux и macOS. Имя бинарника везде `web-agent`; на Windows CMake создаст `web-agent.exe`.

## CMake-опции

* `WEB_AGENT_BUILD_TESTS` - собирать тесты, по умолчанию `ON`.
* `WEB_AGENT_ENABLE_INSTALL` - включить `cmake --install` и CPack, по умолчанию `ON`.
* `WEB_AGENT_STRICT_WARNINGS` - строгие предупреждения компилятора, по умолчанию `OFF`.
* `WEB_AGENT_DEFAULT_CONFIG` - путь к JSON-конфигу по умолчанию; это CMake FILEPATH-опция.
* `WEB_AGENT_PACKAGE_PLATFORM` - суффикс платформы в имени архива, по умолчанию `${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}`.
* `WEB_AGENT_CONFIG_INSTALL_DIR` - каталог установки примера конфига.

## Локальная сборка

```bash
cmake -S . -B build -DWEB_AGENT_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

## Архив для скачивания

```bash
cmake -S . -B build -DWEB_AGENT_PACKAGE_PLATFORM=Windows-x64
cmake --build build --config Release
cpack --config build/CPackConfig.cmake
```

CPack создаст архив вида:

* Windows: `web-agent-1.0.0-Windows-x64.zip`, внутри `bin/web-agent.exe`.
* Linux: `web-agent-1.0.0-Linux-x86_64.tar.gz`, внутри `bin/web-agent`.
* macOS: `web-agent-1.0.0-Darwin-arm64.tar.gz` или аналогичный суффикс архитектуры.

Эти архивы можно публиковать как файлы для скачивания в релизах или на сервере раздачи.

## Установка

```bash
cmake --install build --prefix /usr/local
```

Для Windows используйте `--prefix C:/web-agent` или распакуйте ZIP вручную.

## Запуск с конфигом

```bash
web-agent --config /path/to/agent_config.json
```

Старый позиционный формат также работает:

```bash
web-agent /path/to/agent_config.json
```
## Автоматическая сборка на GitHub

В репозитории есть workflow [.github/workflows/build-packages.yml](../../.github/workflows/build-packages.yml). Он собирает готовые архивы для трех платформ:

* `web-agent-Windows-x64` - ZIP с `bin/web-agent.exe`.
* `web-agent-Linux-x86_64` - tar.gz с `bin/web-agent`.
* `web-agent-macOS` - tar.gz с `bin/web-agent` для runner macOS.

Где скачать после push:

1. Открой GitHub-репозиторий.
2. Перейди во вкладку `Actions`.
3. Открой последний workflow `Build WEB-AGENT packages`.
4. Внизу страницы скачай нужный artifact.

Workflow также можно запустить вручную через `Actions` -> `Build WEB-AGENT packages` -> `Run workflow`.
