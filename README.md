# verhoviyaTEAM

В репозитории реализован `WEB-AGENT` на C++17 для работы с Web API: агент опрашивает сервер, получает инструкции, выполняет локальные задания, собирает текстовый результат и файлы, затем отправляет итог обратно.

Основная точка входа находится в [WEB-AGENT](WEB-AGENT). Инструкции по конфигу, сборке и запуску описаны в [WEB-AGENT/README.md](WEB-AGENT/README.md).

Что закрыто по замечаниям:

* CMake-опции для тестов, install/CPack, strict warnings, FILEPATH-конфига и имени платформенного пакета.
* Формальная JSON Schema для `options`: [WEB-AGENT/docs/options.schema.json](WEB-AGENT/docs/options.schema.json).
* Разные сборки для Windows/Linux/macOS через CPack-архивы; на Windows получается `web-agent.exe`.
* Опция `options.file`, принимающая исполняемый файл/скрипт и аргументы.
* Документация по деплою и скачиваемым архивам: [WEB-AGENT/docs/deploy.md](WEB-AGENT/docs/deploy.md).
## Запуск на разных платформах

Подробные инструкции для Windows `.exe`, Linux и macOS находятся в [WEB-AGENT/README.md](WEB-AGENT/README.md). Готовые архивы собираются через GitHub Actions во вкладке `Actions`.
