# open-bfme

Локальный набор retail-файлов BFME2 1.06 и RotWK 2.01 для тестирования
OpenBFME. Игровые данные не должны попадать в обычные Git-коммиты.

## Передача настоящих BIG-файлов в Arena

OpenBFME проверяет каждый retail-архив по размеру и MD5 и не монтирует
распакованную папку вместо архивов. Поэтому `.big` нельзя просто распаковать:
для запуска нужны исходные архивы байт-в-байт.

`tools/retail_big_transfer.py` делит все BIG-файлы BFME2 и RotWK на тома по
95 MiB. Это меньше лимита Google Drive-коннектора Arena (100 MiB). При
восстановлении проверяются SHA-256 каждого тома, а также MD5 и SHA-256 каждого
BIG-файла.

На компьютере с настоящими установками или после `git lfs pull`:

```bash
python tools/retail_big_transfer.py pack \
  --bfme2 "/path/to/BFME2" \
  --rotwk "/path/to/RotWK" \
  --out "/path/to/openbfme-retail-transfer"
```

На Windows PowerShell команда выглядит так:

```powershell
python tools/retail_big_transfer.py pack `
  --bfme2 "D:\Games\BFME2" `
  --rotwk "D:\Games\RotWK" `
  --out "D:\openbfme-retail-transfer"
```

В Google Drive нужно загрузить из получившейся папки:

- `manifest.json`;
- все `retail-bigs.part-*.bin`;
- `SHA256SUMS.txt` необязателен.

Для текущего набора объёмом около 5,25 GiB получится примерно **57 томов**.
После загрузки достаточно сообщить агенту имя папки Google Drive: он скачает
тома, восстановит архивы в игнорируемую рабочую папку `.retail/` и проверит их
по политикам актуальной версии OpenBFME.

Ручное восстановление:

```bash
python tools/retail_big_transfer.py unpack \
  --manifest /path/to/transfer/manifest.json \
  --out .retail \
  --delete-parts
```

Результат:

```text
.retail/BFME2
.retail/RotWK
```

Их можно передать движку через `BFME2_INSTALL` и `ROTWK_INSTALL`.
