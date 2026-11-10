# Nawat Kernel

إضافة Unreal Engine 5.7 بلغة C++. المعمارية الكاملة في [`docs/ARCHITECTURE.md`](../../docs/ARCHITECTURE.md).

> **لم تُترجم هذه الإضافة داخل المحرك بعد.** كُتبت دون توفر Unreal Engine؛ أول بناء عندك هو اختبارها الأول. إن ظهرت أخطاء مترجم فأرسلها كما هي.

## التثبيت

1. انسخ مجلد `Plugins/Nawat` إلى مجلد `Plugins/` في مشروعك (مشروع C++، أو أضف صنف C++ فارغاً لمشروع Blueprint).
2. أعد توليد ملفات المشروع وابنِ المشروع.
3. فعّل **Nawat Kernel** من Edit → Plugins إن لم تكن مفعّلة.

## الوحدات في هذه النسخة (0.2.0)

| الوحدة | المحتوى | الدليل |
|---|---|---|
| `NawatCore` | حقائق، أحداث، شروط وأفعال، توقف، حفظ | هذا الملف |
| `NawatNarrative` | فصول ومراحل وأهداف، حوار، تفاعل، Triggers، Checkpoints، استيراد JSON | [`Docs/Narrative.md`](Docs/Narrative.md) |
| `NawatAI` | عدو على أي `ACharacter` (دورية، اشتباه، غطاء، إطلاق، تعبئة)، فرقة الأعداء، مرافقو اللاعب الثلاثة وأوامرهم | [`Docs/AI.md`](Docs/AI.md) |

**غير موجود بعد:** وحدة `NawatPresentation` (الواجهة، المشاهد السينمائية، الموسيقى، ربط الإدخال). مكتوبة جزئياً وغير مشمولة في هذه النسخة.

## وحدة `NawatCore`

| الصنف | الوظيفة |
|---|---|
| `UNawatWorldState` | حقائق القصة (وسم → عدد) وناقل الأحداث. نقطة الاتصال الوحيدة بين الأنظمة |
| `UNawatCondition` / `UNawatAction` | شروط وأفعال تُحرَّر داخل الأصول وتُورَّث من C++ أو Blueprint. جاهز منها: Fact، Group، Set Fact، Broadcast Event |
| `UNawatPauseSubsystem` | التوقف كمكدّس أسباب، لكل سبب سياسته (زمن، إدخال، مؤشر، HUD، موسيقى) |
| `UNawatSaveSubsystem` + `INawatSaveable` | حفظ الحقائق ونقطة التفتيش وكل كائن مسجَّل (خصائص `SaveGame`) |

الأنظمة الثلاثة Game Instance Subsystems، فتظهر في Blueprint مباشرة: ابحث عن `Nawat World State` أو `Nawat Pause Subsystem` أو `Nawat Save Subsystem`.

### أمثلة

```cpp
// حقيقة وحدث
UNawatWorldState* State = UNawatWorldState::Get(this);
State->SetFlag(TAG_Story_Ch2_MapsStolen, true);
State->BroadcastEvent(TAG_Story_AlarmRaised, FNawatEventPayload());

// الاستماع لوسم أب يستقبل كل أبنائه
State->OnEventNative(TAG_Story).AddUObject(this, &AMyActor::HandleStoryEvent);

// مشهد سينمائي: لا يوقف الزمن، يحجب الإدخال ويخفي HUD ويمنع قائمة التوقف
FNawatPausePolicy Policy;
Policy.bPauseTime = false;
Policy.bHideHUD = true;
Policy.bBlockPauseMenu = true;
Pause->PushReason(NawatTags::Pause_Cinematic, Policy);
// ...
Pause->PopReason(NawatTags::Pause_Cinematic);
```

وسوم اللعبة (`Story.*`) تعرّفها في مشروعك من Project Settings → Gameplay Tags.

### ملاحظة عن زر التوقف

`TogglePauseMenu` يوقف زمن اللعبة، والإدخال لا يصل أثناء التوقف افتراضياً. اجعل فعل الإدخال المربوط به يعمل أثناء التوقف (في Enhanced Input: خيار **Trigger When Paused** على الـ Input Action)، وإلا لن يُغلق القائمة.

## اختبار القبول اليدوي للمرحلتين 0 و1

1. **البناء**: المشروع يُبنى والإضافة تظهر في قائمة الإضافات.
2. **الحقائق**: من Blueprint، `Set Fact` ثم `Get Fact` يعيد القيمة نفسها، و`On Fact Changed` يُستدعى مرة واحدة.
3. **التوقف**: `Toggle Pause Menu` يوقف اللعبة ويظهر المؤشر، والاستدعاء الثاني يعيد كل شيء كما كان.
4. **التداخل**: ادفع `Nawat.Pause.Cinematic` (دون إيقاف الزمن) ثم افتح قائمة التوقف وأغلقها. يجب أن يبقى الإدخال محجوباً حتى تسحب سبب المشهد.
5. **الحفظ**: ضع حقيقة، `Save To Slot`، أوقف اللعب وشغّله، `Load From Slot`، ثم تحقق أن `Get Fact` يعيد القيمة.

## التالي

إكمال `NawatPresentation`: الواجهة، المشاهد السينمائية، الموسيقى الخلفية، ومكوّن اللاعب الذي يربط الإدخال.
