# Base project for Kendtyte K210 with addons

Это темплейтный проект, в котором буду собирать разнообразные исходники для работы с Kendryte K210.
Одна из основных идей разбиение проекта на 2 части. В одной будут драйвера и работа с железом, в другой - пользовательская логика.
Драйвера для работы с железом должны уметь загружать пользовательскую логику разнообразными способами. Через UART и по WiFi.

Предполагаемый вариант импользования этого кода такой. Он копируется полностью в свой проект, потом изменяется под себя.


Компиляция base проекта
```
задать переменную окружения KENDRYTE_TOOLCHAIN 
cd base
mkdir build
cd build
cmake .. -DPROJ=base
```

## Сторонние проекты
Оригинальные пути сторонних проектов, исходники которых есть в этом репозитории.

* https://github.com/kendryte/kendryte-standalone-sdk
* https://github.com/loboris/ktool
