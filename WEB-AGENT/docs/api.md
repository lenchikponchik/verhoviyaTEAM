# WEB-AGENT API

Контракт восстановлен по `WEB_AGENT API.pdf`.

## Регистрация агента

`POST https://xdev.arkcom.ru:9999/app/webagent1/api/wa_reg/`

Запрос JSON:

```json
{
  "UID": "007",
  "descr": "web-agent"
}
```

Успешный ответ:

```json
{
  "code_responce": "0",
  "msg": "Регистрация прошла успешно",
  "access_code": "594807-1ddb-36af-9616-d8ed2b9d"
}
```

Если агент уже зарегистрирован, сервер может вернуть:

```json
{
  "code_responce": "-3",
  "msg": "Такой агент уже зарегистрирован"
}
```

## Запрос задания

`POST https://xdev.arkcom.ru:9999/app/webagent1/api/wa_task/`

Запрос JSON:

```json
{
  "UID": "007",
  "descr": "web-agent",
  "access_code": "12588b-3d8c-718e-55f4-6ed26b57"
}
```

Если задание есть:

```json
{
  "code_responce": "1",
  "task_code": "CONF",
  "options": "",
  "session_id": "bvLeD2gv-gtKH-IhmW-rsfd-Ejn1kyweawwi",
  "status": "RUN"
}
```

Если задания нет:

```json
{
  "code_responce": "0",
  "status": "WAIT"
}
```

Ошибка запроса:

```json
{
  "code_responce": "-2",
  "msg": "неверный код доступа"
}
```

## Отправка результата

`POST https://xdev.arkcom.ru:9999/app/webagent1/api/wa_result/`

Encoding: `multipart/form-data`.

Поля формы:

* `result_code` - `0`, если задача выполнена; отрицательное число, если произошла ошибка.
* `result` - JSON-строка с содержанием результата.
* `file1`, `file2`, `file3`, ... - файлы результата.

Пример поля `result`:

```json
{
  "UID": "007",
  "access_code": "12588b-3d8c-718e-55f4-6ed26b57",
  "message": "задание не выполнено",
  "files": 3,
  "session_id": "ieLOLgzL-nyGP-mfG5-m3nI-eYL1CZzcaXz0"
}
```

Успешный ответ сервера:

```json
{
  "code_responce": "0",
  "msg": "ok"
}
```

Ошибка загрузки файлов:

```json
{
  "code_responce": "-3",
  "msg": "не все файлы не загружены",
  "status": "ERROR"
}
```
