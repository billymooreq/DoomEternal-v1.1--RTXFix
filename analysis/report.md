# DOOM Eternal 1.1 / Vulkan / RTX 4070: чёрное оружие от первого лица — анализ захвата RenderDoc

Захват: `capture1.rdc` (RenderDoc 1.46, драйвер NVIDIA 617.42, 1920×1080). Захват открывался только через replay API и не сохранялся: дата изменения и размер файла после всех прогонов те же (`2026-10-14 15:08:28`, 1 557 919 744 байт). Файлы игры не трогались.

## Кратко

1. Пиксель оружия черный уже в HDR-буфере сцены `53374`. Его рисует **draw EID 15976** (`vkCmdDrawIndexed`, пайплайн 50985, VS 15508 / FS 15507), и фрагментный шейдер выводит ровно `(0,0,0)`. Постобработка (TAA, bloom, тонмаппинг) здесь ни при чём: она лишь переносит этот ноль дальше.
2. Чёрное **всё** оружие: все 8 draw'ов вида от первого лица (EID 15960–15996, около 240 тыс. пикселей) дают ровно 0. Освещённого пикселя оружия в кадре нет вообще.
3. В шейдере оружия обнулены **все** источники света, кроме солнца:
   - **окружающий / непрямой диффузный свет** (SH, вычисляется в *вершинном* шейдере из `texture_ambientshatlas`) = 0;
   - **отражения / IBL от проб** = 0: список проб пуст во **всех** 3000 кластерах;
   - **локальные источники** отсутствуют: во всех 3000 кластерах только солнце (light 0), хотя активных источников 44;
   - солнце для этого пикселя затенено (тень = 0).

   В сумме получается 0. NaN или Inf нигде нет.
4. **Корневая причина пустых списков источников и проб — гонка в синхронизации, а не данные шейдера.** Кластерный биннинг (compute, EID 15761–15883) берёт размер работы из буферов, которые сам же заполняет compute-шейдером, и запускает `vkCmdDispatchIndirect`. Во всех трёх барьерах перед этим (EID 15778, 15818, 15828) `dstStageMask` содержит `DRAW_INDIRECT`, но в `dstAccessMask` **нет `VK_ACCESS_INDIRECT_COMMAND_READ_BIT`**. По спецификации запись аргументов не обязана быть видна при их чтении для indirect-вызова. На replay с этим драйвером NVIDIA каждый такой вызов при обычном выполнении видит **нулевые** аргументы и ничего не делает, хотя в памяти лежат правильные значения. На AMD это обычно «работает»: indirect-аргументы там читаются через тот же когерентный L2.
5. Предупреждения «X bytes needed, Y provided» — **ложный след**. Все 130 таких случаев в кадре, включая `_instanceUniforms` 160/128 на EID 29190, приходятся только на хвостовые массивы `_paddingN`. Шейдеры их нигде не читают (см. [ubo_cases.md](ubo_cases.md)).
6. Отдельно и пока без объяснения: ячейка атласа SH, куда указывает октодерево для позиции оружия (тексель (204,112,78) в `3D Image 201675`), содержит нули. У других динамических объектов SH ненулевой. После исправления пункта 4 это стоит проверить заново (см. «Открытые вопросы»).

**Как проверить исправление:** в слое `doomfix` перехватить `vkCmdPipelineBarrier` (и `vkCmdPipelineBarrier2`) и при `dstStageMask & DRAW_INDIRECT` добавлять `VK_ACCESS_INDIRECT_COMMAND_READ_BIT` в `dstAccessMask`. Набросок кода приведён в конце отчёта.

---

## 1. Как запускались скрипты

> В репозиторий не входят дампы шейдеров, трассы Debug Pixel/Vertex (в них дизассемблированный код шейдеров игры), дампы состояния пайплайна и буферов и кадры игры: это материал id Software. Ссылки на них в отчёте указывают на локальные файлы, которые скрипты ниже воспроизводят на захватах.

Отдельного `renderdoc.pyd` в установке RenderDoc 1.46 нет: модуль встроен в `qrenderdoc.exe` (Python 3.8). Все скрипты запускаются так:

```
"C:\Program Files\RenderDoc\qrenderdoc.exe" --python analysis\scripts\<script>.py
```

Каждый скрипт сам открывает захват (`OpenCaptureFile` → `OpenCapture`, около 15 с), пишет лог в `analysis/` и завершает процесс (`os._exit`). Ваш открытый экземпляр RenderDoc не затрагивается. Общие функции лежат в [scripts/rdcommon.py](scripts/rdcommon.py).

| Скрипт | Что делает | Результат |
|---|---|---|
| [s1_explore.py](scripts/s1_explore.py) | список действий и дескрипторы цепочки постобработки | [s1_explore.log](s1_explore.log) |
| [s2_history.py](scripts/s2_history.py) | Pixel History по всем текстурам цепочки | [s2_history.log](s2_history.log) |
| [s3_drawinfo.py](scripts/s3_drawinfo.py), [s3b_weapon_masks.py](scripts/s3b_weapon_masks.py) | маски покрытия draw'ов оружия (depth overlay), значения в HDR | [draw15976_depth_overlay.png](draw15976_depth_overlay.png), [weapon_draws_zero_map.png](weapon_draws_zero_map.png) |
| [s4_debug.py](scripts/s4_debug.py) | полный дамп draw'а, Debug Pixel, экспорт шейдеров (входные пиксели берёт из `s4_cases.txt`) | `*_state.txt`, `*_trace.txt`, `shaders/` |
| [s5_ubo_scan.py](scripts/s5_ubo_scan.py), [s12_ubo_fields.py](scripts/s12_ubo_fields.py) | поиск «needed > provided» по всему кадру и проверка, какие поля за границей | [ubo_scan.csv](ubo_scan.csv), [ubo_cases.md](ubo_cases.md) |
| [s6_replace.py](scripts/s6_replace.py) | подмена FS внутри replay (GLSL → glslang в RenderDoc) с выводом промежуточных величин на GPU | [s6_replace.log](s6_replace.log) |
| [s7_race.py](scripts/s7_race.py) | повторяемость результата, использование ресурсов, Debug Vertex | [s7_race.log](s7_race.log), `weapon_vs_*_trace.txt` |
| [s8_barriers_atlas.py](scripts/s8_barriers_atlas.py) | дамп API-вызовов 15770–15962 и содержимое атласа SH | [api_15770_15962.txt](api_15770_15962.txt), [s8_barriers_atlas.log](s8_barriers_atlas.log) |
| [s9_listrepeat.py](scripts/s9_listrepeat.py), [s10_findseq.py](scripts/s10_findseq.py), [s11_indirect.py](scripts/s11_indirect.py) | содержимое списков кластеров и indirect-аргументов | соответствующие `.log` |
| [s14_phase1.py](scripts/s14_phase1.py), [s14b_war.py](scripts/s14b_war.py), [s14c_target.py](scripts/s14c_target.py) | доказательство, что indirect-dispatch'и при обычном выполнении не работают | [s14b_war.log](s14b_war.log), [s14c_target.log](s14c_target.log) |
| [s13_sh_others.py](scripts/s13_sh_others.py), [s15_binning_desc.py](scripts/s15_binning_desc.py), [s16_barrier_scan.py](scripts/s16_barrier_scan.py) | SH у других объектов, дескрипторы биннинга, скан барьеров по кадру | соответствующие `.log` |
| [trace_clean.py](scripts/trace_clean.py), [trace_summary.py](scripts/trace_summary.py) | обычный Python: сжатие и сводка трасс | `*_trace_clean.txt` |

## 2. Цепочка от swapchain до графического draw'а

Swapchain тоже имеет размер 1920×1080, поэтому координаты во всех текстурах одинаковы. Использован пиксель **(1437, 917)**: в нём ноль на всех этапах. В точке (1290, 805) swapchain не чёрный, там значение 0.027 (тёмно-серый).

| Этап | EID | Тип | Читает → пишет | Значение в (1437,917) |
|---|---|---|---|---|
| вывод в swapchain | 29190 | compute | 53390, 53398 → Swapchain 685990 | 0 |
| тонмаппинг / пост | 29186 | compute | 1783 (+bloom 53434, 53422, LUT…) → 53390 | 0 |
| bloom | 29110–29182 | compute | 1783 → 53422 → … | — |
| 2-й TAA/фильтр | 29106 | compute | 1779 → 1783 | ≈0.0012 (вход в этом пикселе 0; вероятно, вклад истории или соседних пикселей) |
| 1-й проход пост | 29103 | compute | **1771** (R11G11B10, 1920×1080) → 1779 | 0 |
| композит / TAA | 29089 | compute | **53374**, 1779, 53382, 1775… → 1771 | 0 |
| композит тумана/объёмов | 27847 | compute | 53374, 1713/1717, 1629 → 53374, 1779 | 0 |
| **основной forward-проход** | 15948–18974 | render pass | → **53374** (R11G11B10_FLOAT), depth 1615 | **draw 15976 → 0** |

Pixel History для `53374` в (1437,917):

- clear на 15946;
- **15976 прошёл тест** (prim 3081, depth 0.43658, `shaderOut = (0,0,0)`);
- все последующие draw'ы проваливают depth test (EQUAL, предварительный depth pass уже выполнен).

Полноразмерная HDR-текстура сцены — это **`2D Color Attachment 53374`**. Первый compute-проход постобработки читает её вместе с `1779` (EID 29089 → `1771`), а EID 29103 читает уже `1771`.

## 3. Draw 15976

Полный дамп: [weapon_dark_draw15976_state.txt](weapon_dark_draw15976_state.txt).

- `vkCmdDrawIndexed`, 27420 индексов, 1 инстанс. Graphics Pipeline 50985, Render Pass 71294, Framebuffer 695216.
- Цели: colour[0] = `53374` R11G11B10_FLOAT (без blend), depth = `1615` D32S8: test EQUAL, запись выключена, stencil включён, cull back.
- **VS** Shader Module 15508: скиннинг по пред-скиннированным позициям (`skinningMode` = 21439 → смещение в `worldskinnedpositions`). Здесь же считаются **SH окружающего света** (`vofi_TexCoord8/9/10` = `floatSH0/1/2`) через обход октодерева `texture_ambientatlas` (3D 256×256×24 RGBA8) с последующей выборкой из `texture_ambientshatlas` (3D 256×256×102 RGBA16F).
- **FS** Shader Module 15507, `main`, EarlyFragmentTests, без отладочной информации (имена из OpName). Свет считается по кластерной схеме: emissive, затем SH-ambient, пробы (`probelistbuffer` + `texture_envprobesmaparray`), затем цикл по источникам (`lightlistbuffer` + `lightparms1/2` + `texture_shadowatlasdepthcomparemap`, PCF 3×3), затем умножение на AO.

Дескрипторы FS (используемые динамически):

| set/binding | имя | ресурс |
|---|---|---|
| 0/0 | `_viewUniforms` | Buffer 259 @5120, range 768 (шейдеру нужно 800, разница только `_padding0[1]`) |
| 0/16 | texture_directionalocclusionmap | 1673, 960×540 RGBA8 (SSDO выключен: `ssdoparms.x = 0`) |
| 0/18 | texture_watercausticsmap | 1133, 16×16 RGBA8 |
| 0/20 | probelistbuffer | Buffer 1753 @1152000+768000 |
| 0/21 | texture_viewdepthdownres4xreverse16fmap | 1418, 480×270 R16F |
| 0/23 | texture_viewcolorlastframemap | 1775, 1920×1080 R11G11B10 |
| 0/24, 0/25 | lightparms1/2 | Buffer 1062/1063, 64 КБ (VK_WHOLE_SIZE) |
| 0/26 | texture_envprobesmaparray | 1259, CubeArray 128², 2046 слоёв, BC6H |
| 0/27 | lightlistbuffer | Buffer 1753 @384000+768000 |
| 0/28, 0/29 | shadowsparms(squasheddepth)ubo | Buffer 1061/1060 |
| 0/30 | texture_shadowatlasdepthcomparemap | 59303, 4096×2048 D16 |
| 1/0 | `_instanceUniforms` | Buffer 345131, range 112 (нужно 128, разница только `_padding1`) |
| 2/0 | `_surfaceUniforms` | Buffer 345270, range 96 (нужно 144, разница только `_padding2[3]`) |
| 2/1 | texture_transmap | 16382 BC3 |
| 3/0 | `_renderLayerUniforms` | Buffer 1993 @45440+320 |
| 3/1–3/8 | renderLayer_albedo/specular/normal/smoothness/cover/bloommask/heightmap | 4096×2048 BC1_SRGB / BC1_SRGB / BC5 / BC4, заглушки 16×16 |
| 4/0 [1119] | globalTextures | 1024² BC4 |

Важные значения в UBO:

- `ambientviewparms = (1,0,0,0)`: ambient из атласа включён, принудительного цвета нет;
- `envirradiancescale = 1`, `instanceirradiancescale = 1`, `ambientinstancescale = 1`, `modellightingflags = 0`;
- `ssrparms.x = 0`, `powerup.w = 0`, `gunglow = 0`;
- в `_renderLayerUniforms.surfaceemissivecolor[1].x` лежит NaN, но шейдер берёт индекс 0 (`physicalMtlIndex = 0`), так что это мусор в неиспользуемом слоте.

### «X bytes needed, Y provided»

Найдено 130 уникальных пар «шейдер + блок» во всём кадре ([ubo_scan.csv](ubo_scan.csv), [ubo_cases.md](ubo_cases.md)). **Во всех без исключения** за границей привязанного диапазона лежит только последний член `_padding0/1/2`. Ни один шейдер к нему не обращается: проверено по декомпиляции spirv-cross.

Разница возникает из-за того, что в SPIR-V `float _padding[N]` объявлен с ArrayStride 16 (std140), а CPU-структура упакована плотно. Это относится и к EID 29190 (`_instanceUniforms`: 160 needed, 128 provided → `_padding1@128`). Чтение за границей на NVIDIA с robust access вернуло бы 0, но эти поля не читаются, так что **на результат это не влияет**.

## 4. Debug Pixel: пиксель оружия (1437,917)

Полная трасса: [weapon_dark_draw15976_px1437_917_trace.txt](weapon_dark_draw15976_px1437_917_trace.txt). Сжатая: [weapon_dark_trace_clean.txt](weapon_dark_trace_clean.txt). Код шейдера: [shaders/frag_15507.glsl](shaders/frag_15507.glsl).

Где считается освещение (строки GLSL относятся к `frag_15507.glsl`):

| Шаг трассы | Что считается (GLSL) | Значение |
|---|---|---|
| вход | `vofi_TexCoord8/9/10` → `inputs.ambientSH0..2` (стр. 630) | **(0,0,0,0) × 3** |
| 478 | albedo (`renderLayer_albedo`) | (0.0347, 0.0374, 0.0347): тёмный металл |
| — | specular / smoothness | (0.089, 0.106, 0.136) / 0.457 |
| 96 | `ambientOcclusion` (SSDO выключен) | 1 |
| — | emissive | 0 |
| — | `indirectDiffuse = shc0 - shc1·N.y + shc2·N.z - shc3·N.x`, `ambient = max(0, indirectDiffuse·irradianceScale)·albedo` (стр. 839–842) | **0**: **непрямой диффузный свет пропадает здесь** |
| 838 | `clusterProbeHash = probelistbuffer[2736*64]` (стр. 845) | **0 проб** → отражений/IBL нет (`envprobes_lighting = 0`) |
| 947 | `clusterLightHash = lightlistbuffer[2736*64]` (стр. 1077) | эмулятор: **4** (id 0, 1, 23, 29) |
| — | light 0 (солнце, тип 2, каскадные тени): PCF 3×3 | все выборки 0 → тень 0 → `continue` |
| — | light 1, 23 | отсечены по объёму / затуханию |
| 3196–3209 | light 29 (локальный, около камеры): `light_color_final = (0.0515, 0.0582, 0.0591)`, `NdotL = 0.585` | spec + diffuse |
| 3221 | `output_lighting *= AO` | **эмулятор: (0.00124, 0.00153, 0.00153)** |

NaN/Inf в трассе: **0**. Чтений неинициализированных переменных (`0xCCCCCCCC`) нет.

**GPU дал другой результат.** Pixel History на том же replay показывает `shaderOut = 0`. Чтобы найти расхождение, я подменял FS только внутри replay ([s6_replace.log](s6_replace.log), [s7_race.log](s7_race.log)): вариант шейдера пишет нужную величину в цвет, а она читается на GPU. Результаты:

- albedo, specular и нормаль на GPU совпадают с эмулятором;
- SH-вход = 0;
- `numLights` на GPU = **1** (только солнце), а не 4;
- в цикле до теста тени доходит 1 источник, тень = 0, освещённых источников 0;
- отрицательных значений и NaN в `output_lighting` нет;
- **результат недетерминирован**: 4 replay одного и того же draw'а с исходным SPIR-V дали `0, 0.001236, 0, 0`. Значение 0.001236 — это вклад light 29 после квантования в R11G11B10. Иногда GPU видит полный список, но обычно нет.

## 5. Сравнение: «освещённый» пиксель

Пикселя оружия, освещённого прямым светом, в этом кадре **нет**: [weapon_draws_zero_map.png](weapon_draws_zero_map.png) (синие = 0). Сравнение я сделал по двум другим направлениям.

- **Тот же пиксель с правильным списком источников** (эмулятор и replay, где гонка «выиграна») даёт 0.00124. Даже в этом случае оружие почти чёрное, потому что SH и пробы равны 0.
- **Пиксели мира**: [world_floor_near_draw27064_px1000_850_trace_clean.txt](world_floor_near_draw27064_px1000_850_trace_clean.txt), [world_lit_right_draw21688_px1650_700_trace_clean.txt](world_lit_right_draw21688_px1650_700_trace_clean.txt), шейдер [frag_14337](shaders/frag_14337.glsl). У них тоже только солнце в кластере (`lightHash = 1`, проб 0). Но непрямой свет статическая геометрия берёт из **лайтмапа**: `ambient = 0.00258…` даёт итог 0.00263, ровно как на GPU. Поэтому мир не чёрный, а «местами слишком тёмный»: в нём пропадают локальные источники, отражения от проб и декали.
- **SH у других динамических объектов** ([s13_sh_others.log](s13_sh_others.log)): у draw'ов 16005–16015 и 16134–16144 SH ненулевой (|SH0| до 0.03–0.24). У обеих групп вида от первого лица (15960–15970 и 15976–15986) он ровно 0.

## 6. Корневая причина: indirect-dispatch'и кластерного биннинга не видят свои аргументы

Последовательность в одном командном буфере (дескрипторы: [s15_binning_desc.log](s15_binning_desc.log), API: [api_15770_15962.txt](api_15770_15962.txt)):

```
15761-15767  coarse binning (decals / visboxes(пробы) / light frustums) -> счётчики 1074/1086/1100
15768        barrier CS->CS
15771-15777  compute: счётчики -> indirect args в Buffer 1079/1091/1105            (запись SHADER_WRITE)
15778        barrier src=VS|FS|CS  dst=TOP|DRAW_INDIRECT|CS  dstAccess=UNIFORM_READ|SHADER_READ|SHADER_WRITE   <-- нет INDIRECT_COMMAND_READ
15781-15817  vkCmdDispatchIndirect(1079/1091/1105) фаза 1 -> tile buckets, lightbitfield
15818        barrier (то же, нет INDIRECT_COMMAND_READ; в srcStage нет DRAW_INDIRECT)
15821-15827  compute: счётчики фазы 1 -> НОВЫЕ args в те же 1079/1091/1105
15828        barrier (то же, нет INDIRECT_COMMAND_READ)
15831-15873  vkCmdDispatchIndirect фаза 2 -> decal/light bitfields, visboxes
15876-15883  сборка decallist / lightlist / probelist (солнце добавляется безусловно)
15948...     forward-проход читает списки
```

[s16_barrier_scan.log](s16_barrier_scan.log): во всём кадре ровно 3 барьера с `DRAW_INDIRECT` в `dstStageMask`, и **ни в одном** нет `VK_ACCESS_INDIRECT_COMMAND_READ_BIT`. Все 39 `vkCmdDispatchIndirect` кадра относятся к этому биннингу.

Доказательства:

1. **Indirect-вызовы при обычном выполнении ничего не делают** ([s14c_target.log](s14c_target.log), [s14b_war.log](s14b_war.log)). Если replay остановить ровно на `DispatchIndirect` (15807, 15813, 15817), этот вызов отрабатывает: в `lightbitfield` появляются биты, в buckets — записи. RenderDoc обрабатывает целевой indirect-вызов отдельно. Если остановиться на **следующем** событии (15808, 15814, 15818 — bind или барьер), то те же вызовы, выполненные как обычно, **не записывают ничего**. Буферы аргументов при этом в памяти правильные: `1105 = [1,1,1,2,1,2]`. До EID 15770 эти буферы нулевые, то есть GPU читает устаревшие нули.
2. Из-за этого 15821–15827 считают для фазы 2 нулевые аргументы (после 15827 все args = 0, [s11_indirect.log](s11_indirect.log)), и фаза 2 тоже ничего не делает.
3. Итоговые списки ([s11_indirect.log](s11_indirect.log)), стабильные между replay ([s9_listrepeat.log](s9_listrepeat.log)):
   - в списке источников во всех 3000 кластерах 1 запись, id 0 (солнце), хотя ненулевой цвет имеют 44 источника (лампы, факелы, точечные/прожекторные источники около камеры, например light 29 в (26.7, −1609.7, 21.3));
   - список проб пуст во всех 3000 кластерах;
   - список декалей пуст во всех 3000 кластерах.
4. Это именно гонка, а не «так задумано». При загрузке захвата RenderDoc сам прочитал аргументы фазы 2 для источников как `<1,1,3,10,27,34,115>` (видно в именах действий 15861–15873). Эмулятор шейдера видел в кластере 4 источника, а часть replay пикселя оружия дала вклад light 29. В текущих replay те же аргументы равны 0.
5. Почему AMD не затронута: у AMD командный процессор читает indirect-аргументы через тот же L2, в который пишут шейдеры, поэтому пропущенный бит доступа обычно не проявляется. NVIDIA для `INDIRECT_COMMAND_READ` требует явной инвалидации, которую драйвер делает только по этому биту. Вероятно, новые драйверы перестали делать её «на всякий случай».

Почему из этого получается чёрное оружие:

- нет локальных источников (включая те, что около камеры);
- нет проб (IBL/отражения);
- солнце затенено;
- SH = 0;
- в итоге 0.

Мир спасают лайтмапы, поэтому он лишь «местами слишком тёмный». Также должны пропадать декали.

## 7. Открытые вопросы

- **SH окружающего света оружия = 0** ([weapon_vs_draw15976_vtx933_trace.txt](weapon_vs_draw15976_vtx933_trace.txt), [s8_barriers_atlas.log](s8_barriers_atlas.log)). Обход октодерева для позиции оружия (≈ позиция камеры) находит лист на 13-й итерации с ключом SH (204,112,78). Тексели атласа по этому адресу (z 78–83) — нули. В целом атлас заполнен (примерно половина байт ненулевые в каждом слое). В этом кадре атлас никто не пишет, данные пришли из предыдущих кадров. Неизвестно, такие же ли данные на AMD: возможно, это следствие той же проблемы синхронизации при потоковой загрузке ambient-данных, а возможно, нормальная ситуация, при которой оружие освещают локальные источники и пробы. Проверить можно двумя способами: (1) после исправления барьеров; (2) захватом на AMD.
- Позиции вершин оружия из post-VS GPU и из эмулятора Debug Vertex заметно различаются (`gl_Position` 0.128/0.172 против 0.160/0.120). Это может быть ещё одна гонка со скиннингом (`worldskinnedpositions`, Buffer 1035), но я её не исследовал.
- Трассы и подмены шейдеров выполнялись при replay на том же GPU и драйвере. Поведение реальной игры может отличаться по таймингу. Однако исходный симптом (полностью чёрное оружие на финальном кадре) с этим согласуется.

## 8. Предлагаемое исправление в слое `doomfix`

Минимальная правка, безопасная для любых кадров:

```cpp
// src/features/indirect_barrier_fix.cpp (набросок)
static constexpr VkPipelineStageFlags kIndirect = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;

void VKAPI_CALL CmdPipelineBarrier(VkCommandBuffer cb, VkPipelineStageFlags src, VkPipelineStageFlags dst,
                                   VkDependencyFlags dep, uint32_t memCount, const VkMemoryBarrier* mem,
                                   uint32_t bufCount, const VkBufferMemoryBarrier* buf,
                                   uint32_t imgCount, const VkImageMemoryBarrier* img) {
    if (!(dst & kIndirect)) return next(cb, src, dst, dep, memCount, mem, bufCount, buf, imgCount, img);
    std::vector<VkMemoryBarrier> m(mem, mem + memCount);
    std::vector<VkBufferMemoryBarrier> b(buf, buf + bufCount);
    for (auto& x : m) x.dstAccessMask |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    for (auto& x : b) x.dstAccessMask |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    if (m.empty() && b.empty())   // execution-only barrier: add a global memory dependency
        m.push_back({VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT,
                     VK_ACCESS_INDIRECT_COMMAND_READ_BIT});
    // WAR: indirect reads of phase 1 must finish before args are overwritten (barrier 15818)
    src |= (src & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) ? kIndirect : 0;
    next(cb, src, dst, dep, (uint32_t)m.size(), m.data(), (uint32_t)b.size(), b.data(), imgCount, img);
}
// аналогично для vkCmdPipelineBarrier2/2KHR (VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT / VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT)
```

Как проверить:

1. Новый захват с включённым слоем: в кластерах должно быть больше одного источника, а `probelist` — непустым ([s11_indirect.py](scripts/s11_indirect.py)).
2. Оружие перестаёт быть чёрным. Если оно остаётся заметно темнее, чем на AMD, разбираться с атласом SH (раздел 7).
3. При наличии Vulkan SDK: включить Synchronization Validation в `VK_LAYER_KHRONOS_validation`. Должен быть RAW-хазард `INDIRECT_COMMAND_READ` на буферах 1079/1091/1105, и после исправления он должен исчезнуть.

## 9. Выгруженные шейдеры

Папка `shaders/`:

| Файл | Содержимое |
|---|---|
| `frag_15507.*` | FS оружия: `.spv` (SPIR-V), `.spvasm` (spirv-dis), `.glsl` (spirv-cross, Vulkan GLSL), `.hlsl` (SM 5.1), `.rd_disasm.txt` (дизассемблер RenderDoc с номерами строк трассы) |
| `vert_15508.*` | VS оружия (скиннинг + SH окружающего света) |
| `frag_14337.*`, `vert_14338.*` | шейдеры мира для сравнения (HLSL для FS не собрался: нужны wave-операции SM 6.0) |
| `ubo_cases/` | SPIR-V и GLSL всех шейдеров с «needed > provided» |

