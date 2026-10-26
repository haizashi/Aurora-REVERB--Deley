# AURORA - Reverb + Delay (VST3)

JUCE 8 / C++17. Formats: VST3 + Standalone.

## Сборка
Нужны: CMake >= 3.22, git, компилятор (Visual Studio 2022 / Xcode / gcc + системные библиотеки JUCE на Linux).

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

JUCE скачивается автоматически (FetchContent). После сборки VST3 копируется в системную папку плагинов
(COPY_PLUGIN_AFTER_BUILD). Standalone-версия: build/Aurora_artefacts/Release/Standalone.

## Новое в 1.1
**Синхронизация delay с темпом хоста** (кнопка TEMPO SYNC в панели DELAY).
Длительности: 1/1, 1/2, 1/4, 1/8, 1/16, 1/32 - прямые, пунктирные (dotted) и триольные (triplet).
Темп берётся из транспорта DAW; смена темпа плавно «тянет» эхо, как лента. Если хост темп не сообщает
(Standalone), используется 120 BPM. Если нота длиннее 2 с при медленном темпе, время делится пополам.

**Шиммер в реверберации** (ручка SHIMMER + выбор интервала): питч-сдвинутая копия хвоста возвращается
в петлю обратной связи, и с каждым кругом хвост «взлетает» вверх.
Интервалы: октава вверх, квинта, октава+квинта, две октавы, октава вниз.
Количество шиммера автоматически ограничивается при очень длинном decay (петля иначе уходит в самовозбуждение);
при Freeze шиммер отключается.

## Архитектура
- Source/DSP/FDNReverb.h     - 8-линейная FDN-реверберация + шиммер-петля
- Source/DSP/PitchShifter.h  - гранулярный питч-шифтер (два окна sin^2)
- Source/DSP/StereoDelay.h   - ping-pong/стерео delay, low/high cut и сатурация в петле, wobble, offset
- Source/PluginProcessor.*   - параметры (APVTS), темп хоста, роутинг Parallel/Serial, пресеты, уровни для GUI
- Source/PluginEditor.*      - GUI 1000x556
- Source/UI/                 - LookAndFeel (неоновые ручки) и анимированная «туманность»
