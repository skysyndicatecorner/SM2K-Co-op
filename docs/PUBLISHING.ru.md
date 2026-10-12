# Как выложить проект на GitHub

Пошаговая инструкция для автора. После неё у проекта будет репозиторий, автоматическая сборка, релиз со
скачиванием и сайт на GitHub Pages.

## 1. Подготовка аккаунта

1. Зарегистрируйтесь на github.com (если ещё нет аккаунта).
2. Чтобы личная почта не попала в историю коммитов: **Settings → Emails**, включите
   «Keep my email addresses private». Скопируйте адрес вида `12345+ник@users.noreply.github.com`.
3. Установите Git (git-scm.com) или GitHub Desktop.

## 2. Создайте репозиторий

1. Нажмите **New repository**:
   - имя `sm2k-coop`;
   - **Public**;
   - **не** добавляйте README, лицензию и `.gitignore`: они уже есть в папке.
2. Распакуйте архив с репозиторием, откройте терминал в папке `sm2k-coop` и выполните:

```bash
git init -b main
git config user.name "ваш-ник"
git config user.email "12345+ваш-ник@users.noreply.github.com"
git add .
git commit -m "SM2K Co-op v0.12"
git remote add origin https://github.com/ваш-ник/sm2k-coop.git
git push -u origin main
```

Перед `git add .` проверьте, что в папке нет файлов игры: exe, декомпиляции, логов с чужими данными.
`.gitignore` отсекает типичные случаи, а GitHub Actions ещё раз проверит это скриптом `scripts/check-no-game-files.sh`.

## 3. Проверьте сборку

Откройте вкладку **Actions**. Workflow **CI** соберёт DLL тремя способами:
- MSVC на Windows;
- MinGW gcc;
- clang (воспроизводимая сборка).

Все три должны быть зелёными. Если MSVC-сборка упадёт, пришлите лог, я поправлю: здесь я мог проверить код только
компиляторами clang и gcc.

## 4. Выпустите релиз

```bash
git tag v0.12
git push origin v0.12
```

Workflow **Release** соберёт DLL, упакует `sm2k-coop-v0.12-win32.zip` с хешами, подпишет происхождение сборки и
опубликует релиз. Описание релиза берётся из `docs/releases/v0.12.md`.

Для следующих версий:
1. Создайте файл `docs/releases/v0.13.md`.
2. Допишите `CHANGELOG.md`.
3. Поставьте тег `v0.13`.

Если Actions недоступны, релиз можно сделать вручную:
1. **Releases → Draft a new release**, тег `v0.12`.
2. Прикрепите готовый `sm2k-coop-v0.12-win32.zip` и `sm2k-coop-v0.12-win32.sha256.txt`.
3. Текст вставьте из `docs/releases/v0.12.md`, заменив `{{REPO}}` на `ваш-ник/sm2k-coop`.

## 5. Включите сайт

1. **Settings → Pages → Build and deployment → Source: GitHub Actions.**
2. Запустите workflow **Website** (Actions → Website → Run workflow) или сделайте любой коммит в `docs/`.
3. Сайт откроется по адресу `https://ваш-ник.github.io/sm2k-coop/`.
   Ссылки на скачивание и исходники он подставит сам по этому адресу.

## 6. Оформление репозитория

- **About** (шестерёнка справа на главной странице репозитория):
  - описание — из раздела «Готовые тексты» ниже;
  - Website: адрес сайта;
  - темы (topics): `spider-man`, `mod`, `coop`, `split-screen`, `lan`, `game-modding`, `dll-proxy`, `retro-gaming`, `reverse-engineering`.
- **Settings → Code security → Private vulnerability reporting: Enable** (на него ссылается `SECURITY.md`).
- **Settings → General → Features:** включите Issues и при желании Discussions.

## 7. После релиза

- Загрузите `dinput8.dll` на virustotal.com и добавьте ссылку на результат в описание релиза.
  Если какой-то антивирус ругается, отправьте файл как ложное срабатывание (см. `docs/RISKS.ru.md`).
- Сделайте зеркало на Codeberg или GitFlic, например через их импорт репозитория с GitHub.

## Готовые тексты

### Короткое описание (GitHub About, до 350 символов)

> Fan-made co-op mod for Spider-Man (2000, PC): split-screen on one PC or LAN / Radmin VPN on two. In-game menu,
> own camera and HUD for player 2, downed and revive. Open source (MIT), built with AI assistance.

### Для форумов и сайтов модов (по-русски)

> **SM2K Co-op — кооператив для Spider-Man (2000, PC)**
>
> Фанатский мод, который добавляет в PC-версию Spider-Man 2000 года второго игрока.
>
> **Один компьютер, разделённый экран.** У второго Паука своя камера, свой HUD в стиле игры и своё здоровье с
> паутиной, которые сохраняются между уровнями. Управление с клавиатуры или геймпада. Враги атакуют обоих.
> Если второй игрок упал, у первого есть минута, чтобы подойти и поднять его.
>
> **Два компьютера, LAN или Radmin VPN** (тестовый режим). Каждый видит напарника. Смерти врагов, двери и скрипты
> уровня, переходы между уровнями общие.
>
> Всё настраивается во внутриигровом меню (F6). Установка: положить `dinput8.dll` рядом с `SpideyPC.exe`
> (версия игры 1.3).
>
> Код открыт (MIT), доработать может любой. Мод сделан с помощью ИИ (Claude от Anthropic) и проверен в игре.
> Неофициальный проект, не связан с Marvel и Activision. Файлов игры в моде нет.

### For mod sites (English)

> **SM2K Co-op: two-player co-op for Spider-Man (2000, PC)**
>
> A fan-made mod that adds a second player to the PC version of Spider-Man (2000).
>
> **One PC, split-screen.** Player 2 gets his own camera, the game's own HUD, and his own health and web carried
> between levels. Keyboard or gamepad. Enemies go after both players. When player 2 goes down, player 1 has a
> minute to reach him and revive him.
>
> **Two PCs over LAN or Radmin VPN** (testing). You see your partner, and enemy deaths, doors, level scripts and
> level changes are shared.
>
> Everything is set up in an in-game menu (F6). Install: drop `dinput8.dll` next to `SpideyPC.exe` (game v1.3).
>
> Open source (MIT): anyone can improve it. Built with the help of AI (Claude by Anthropic) and tested in the game.
> Unofficial, not affiliated with Marvel or Activision. Contains no game files.
