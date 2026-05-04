# JSON-опции задания

Поле `options` из `wa_task` поддерживается в двух форматах: строка команды или JSON-объект. Если строка начинается с `{`, агент сначала пытается разобрать ее как JSON-объект.

## Команда строкой

```json
{
  "task_code": "CONF",
  "options": "echo ok>generated.txt",
  "result_files": ["generated.txt"],
  "session_id": "session-1",
  "status": "RUN"
}
```

## Команда объектом

```json
{
  "task_code": "TASK",
  "options": {
    "command": "echo hello",
    "result_wait_timeout_sec": 0
  },
  "session_id": "session-2",
  "status": "RUN"
}
```

## Опция, получающая файл

Для запуска конкретного файла используется `options.file`. Аргументы можно передать строкой или массивом. Массив безопаснее: агент сам экранирует элементы под текущую ОС.

```json
{
  "task_code": "TASK",
  "options": {
    "file": "tools/report-generator.exe",
    "args": ["--input", "input data.json", "--out", "report.txt"],
    "working_directory": "job-42",
    "result_files": ["job-42/report.txt"],
    "result_wait_timeout_sec": 10
  },
  "session_id": "session-3",
  "status": "RUN"
}
```

Алиасы для файла: `program`, `executable`, `path`. Алиасы для аргументов: `arguments`, `args`, `params`.

## Передача готового файла

Если `task_code` равен `FILE` и команда не передана, агент не запускает процесс, а только ждет и отправляет файлы из `result_directory`.

```json
{
  "task_code": "FILE",
  "options": {
    "result_files": ["ready.txt"],
    "result_wait_timeout_sec": 5
  },
  "session_id": "session-4",
  "status": "RUN"
}
```

## Ограничения путей

`working_directory` должен оставаться внутри `result_directory`. Файлы результата принимаются только относительными путями без `..`; абсолютные пути и symlink-выходы за каталог результатов игнорируются.

Формальная схема находится в [options.schema.json](options.schema.json).
