# NightDim — Night Mode Plugin for PSP 3000

Плагин ночного режима с тёплым фильтром экрана для PSP.

## Возможности

- **Тёплый фильтр** — уменьшает синий, добавляет оранжево-красный оттенок
- **Регулировка яркости** — 0-100%, по умолчанию 35%
- **Авто-режим по времени** — автоматически включается в заданный интервал
- **INI-конфиг** — настройки сохраняются в `nightdim.ini`
- **OSD-индикатор** — полоска яркости при изменении настроек
- **Индикатор активности** — маленький оранжевый квадратик в правом верхнем углу

## Управление

| Комбинация | Действие |
|-----------|----------|
| SELECT + L | Вкл / выкл NightDim |
| SELECT + ← | Темнее (-5%) |
| SELECT + → | Светлее (+5%) |
| SELECT + ↑ | Теплее (+10) |
| SELECT + ↓ | Холоднее (-10) |

## Установка

1. Скопируй `nightdim.prx` и `nightdim.ini` в `ms0:/seplugins/`
2. Добавь в конфиг CFW:
   - ARK-4: в `PLUGINS.txt` — `all, ms0:/seplugins/nightdim.prx, on`
   - PRO/ME: в `vsh.txt`, `game.txt`, `pops.txt` — `ms0:/seplugins/nightdim.prx 1`
3. Перезагрузи PSP

## Сборка через GitHub Actions

1. Создай репозиторий на GitHub
2. Залей все файлы (включая `.github/workflows/build.yml`)
3. Перейди в Actions → Build NightDim PSP Plugin → Run workflow
4. Через ~2 минуты скачай артефакт `nightdim-prx.zip`

## Локальная сборка

```bash
git clone https://github.com/pspdev/pspdev.git
cd pspdev && ./build.sh
cd ../nightdim
make
