# وحدة NawatNarrative — القصة والحوار والتفاعل

> **تنبيه: هذا الكود لم يُبنَ (compile) بعد داخل Unreal Engine.** كُتب دون محرك متاح في بيئة التطوير، لذلك أول خطوة هي بناؤه على جهازك وإرسال أي أخطاء تظهر. لا تعتبر أي سلوك موصوف هنا مؤكداً قبل اجتياز قائمة القبول في آخر الملف.

الوحدة تعتمد على `NawatCore` فقط. كل الشروط (`UNawatCondition`) والأفعال (`UNawatAction`) المستعملة هنا هي نفسها أصناف النواة، وتُكتب داخل الأصول مباشرة (Instanced).

---

## 1. ما الذي تحتويه الوحدة

| الصنف | الملف | الغرض |
|---|---|---|
| `UNawatChapter` | `NawatChapter.h` | أصل بيانات (Data Asset) يصف فصلاً: عنوان + قائمة مراحل `FNawatStage`. |
| `UNawatStoryDirector` | `NawatStoryDirector.h` | نظام فرعي على Game Instance يشغّل فصلاً واحداً: يتتبع المرحلة الحالية، يُكمل الأهداف، يتفرّع، ويُحفظ مع اللعبة. |
| `UNawatDialogue` | `NawatDialogue.h` | أصل بيانات يصف حواراً: قائمة أسطر `FNawatDialogueNode` مع اختيارات. |
| `UNawatDialogueSubsystem` | `NawatDialogueSubsystem.h` | نظام فرعي يشغّل حواراً واحداً: مؤقت للأسطر، صوت، اختيارات. |
| `INawatInteractable` | `NawatInteractable.h` | واجهة لأي Actor يمكن للاعب استعماله. |
| `UNawatInteractorComponent` | `NawatInteractorComponent.h` | مكوّن يُضاف إلى Pawn اللاعب ليجد ما ينظر إليه ويستعمله. |
| `ANawatInteractableActor` | `NawatInteractableActor.h` | Actor جاهز قابل للاستعمال (رافعة، ورقة، شخصية تتحدث). |
| `ANawatStoryTrigger` | `NawatStoryTrigger.h` | حجم (Box) ينفّذ أفعالاً عند دخول اللاعب. |
| `ANawatCheckpoint` | `NawatCheckpoint.h` | حجم يحفظ اللعبة ونقطة العودة عند دخول اللاعب. |
| أفعال وشروط جاهزة | `NawatNarrativeActions.h`، `NawatNarrativeConditions.h` | انظر القسم 5. |
| `UNawatStoryImportLibrary` | `NawatStoryImportLibrary.h` | استيراد فصل أو حوار من JSON. |

> ملاحظة تسمية: الـ Actor الجاهز اسمه `ANawatInteractableActor` وليس `ANawatInteractable`، لأن Unreal لا يسمح بصنفين يتطابق اسماهما بعد حذف البادئة (`UNawatInteractable` هو صنف الواجهة).

---

## 2. الفصول والمراحل والأهداف

### المفاهيم

- **الفصل** `UNawatChapter`: `Title`، `Subtitle`، `Stages`.
- **المرحلة** `FNawatStage`:
  - `Id`: اسم المرحلة (يُستعمل في `GoToStage` والتفرعات والحفظ). اجعله فريداً داخل الفصل.
  - `Title`: عنوان يظهر فوق قائمة الأهداف.
  - `Objectives`: قائمة الأهداف.
  - `EnterActions` / `ExitActions`: أفعال تُنفَّذ عند دخول المرحلة والخروج منها.
  - `CompleteWhen`: شروط إضافية لاكتمال المرحلة.
  - `Branches`: تفرعات تُفحص بالترتيب عند اكتمال المرحلة؛ أول تفرع تنجح شروطه يُؤخذ. `TargetStageId` فارغ (None) = إنهاء الفصل.
  - `NextStageId`: المرحلة التالية إن لم يُؤخذ أي تفرع. فارغ = المرحلة التالية بترتيب القائمة؛ بعد آخر مرحلة يكتمل الفصل.
- **الهدف** `FNawatObjective`: `Id`، `Text`، `bOptional`، `CompleteWhen`.

### القواعد

- الهدف «يُقفل» مكتملاً أول مرة تنجح شروطه، ولا يعود غير مكتمل حتى لو تغيّرت الحقائق.
- هدف بلا شروط لا يكتمل تلقائياً أبداً؛ أكمله يدوياً بـ `CompleteObjective` أو بالفعل `Complete Objective`.
- المرحلة تكتمل عندما تكتمل **كل الأهداف غير الاختيارية** وتنجح `CompleteWhen`.
- مرحلة بلا أهداف إلزامية وبلا `CompleteWhen` لا تتقدم إلا يدوياً (`AdvanceStage`).
- التقييم يحدث عند تغيّر أي حقيقة، وبعد أي حدث على الناقل، وكذلك أربع مرات في الثانية تقريباً (للشروط التي لا تعتمد على الحقائق).
- الكائن الممرَّر كسياق (Context) للشروط والأفعال هو الـ Director نفسه.
- أفعال الدخول/الخروج التي تطلب تغيير المرحلة لا تُنفَّذ بشكل متداخل: الطلب يُؤجَّل حتى ينتهي التغيير الجاري.

### واجهة `UNawatStoryDirector` (كلها متاحة في Blueprint)

```cpp
bool StartChapter(UNawatChapter* InChapter, FName StartStageId = NAME_None);
void StopChapter();                      // يتخلى عن الفصل دون أفعال خروج ودون حدث اكتمال
void AdvanceStage();                     // يُكمل المرحلة الحالية فوراً ثم يتبع التفرعات
bool GoToStage(FName StageId);           // قفزة مباشرة تتجاهل التفرعات
bool CompleteObjective(FName ObjectiveId);

bool IsChapterActive() const;
UNawatChapter* GetCurrentChapter() const;
int32 GetCurrentStageIndex() const;      // -1 إن لم يكن هناك فصل
FName GetCurrentStageId() const;
bool GetCurrentStage(FNawatStage& OutStage) const;
bool IsObjectiveComplete(FName ObjectiveId) const;
TArray<FNawatObjectiveState> GetObjectiveStates() const;   // {Id, Text, bOptional, bComplete} للواجهة
```

الأحداث (Delegates):

| الحدث | التوقيع | الحدث المقابل على الناقل |
|---|---|---|
| `OnChapterStarted` | `(UNawatChapter* Chapter)` | `NawatTags::Event_Story_ChapterStarted` — Subject = الفصل |
| `OnStageChanged` | `(UNawatChapter* Chapter, int32 StageIndex)` | `Event_Story_StageChanged` — Subject = الفصل، Value = رقم المرحلة |
| `OnObjectiveCompleted` | `(UNawatChapter* Chapter, int32 StageIndex, FName ObjectiveId)` | `Event_Story_ObjectiveCompleted` — Subject = الفصل، Value = رقم الهدف داخل المرحلة |
| `OnChapterCompleted` | `(UNawatChapter* Chapter)` | `Event_Story_ChapterCompleted` — Subject = الفصل |

بعد اكتمال الفصل يصبح `IsChapterActive` خاطئاً و`GetCurrentChapter` فارغاً.

### الحفظ

الـ Director يسجّل نفسه في `UNawatSaveSubsystem` بالمفتاح `NawatStoryDirector` ويحفظ: مسار أصل الفصل، اسم المرحلة الحالية، وأسماء الأهداف المكتملة. عند التحميل تُستعاد المرحلة **دون** إعادة تنفيذ `EnterActions`، ثم يُبث `OnStageChanged` لتحديث الواجهة (قد يكون `Chapter` فارغاً إن لم يكن في الحفظ فصل).

قيود يجب معرفتها:

- المرحلة أو الهدف بلا `Id` يُحفظ باسم موضعي (`Stage_3`، `Objective_0`)؛ إعادة ترتيب القائمة تُفسد الحفظات القديمة. أعطِ كل شيء `Id`.
- الفصل المستورد من JSON كائن مؤقت لا مسار له على القرص، فلا يُستعاد بعد إعادة تشغيل اللعبة. استورده من جديد ثم استدعِ `StartChapter` بالمرحلة المحفوظة إن احتجت ذلك.
- حالة «استُعمل» في `ANawatInteractableActor` و«انطلق» في `ANawatStoryTrigger` **لا تُحفظ**. إن كان يجب أن تبقى بعد التحميل، اجعل الفعل يرفع حقيقة واجعل الشرط يفحصها (الحقائق تُحفظ).

---

## 3. خطوات المصمم: فصل كامل

1. **الوسوم:** أضف وسوم حقائق القصة في Project Settings → Gameplay Tags (مثلاً `Story.Aures.FoundRadio`).
2. **الأصل:** Content Browser → Miscellaneous → Data Asset → اختر `NawatChapter`. سمِّه مثلاً `DA_Chapter_Aures`.
3. **المراحل:** أضف عناصر إلى `Stages`. لكل مرحلة ضع `Id` و`Title`.
4. **الأهداف:** داخل المرحلة أضف `Objectives`. لكل هدف `Id` و`Text`، وفي `CompleteWhen` أضف شرط `Fact` واختر الوسم.
5. **الأفعال:** في `EnterActions` أضف مثلاً `Start Dialogue` أو `Broadcast Event` (مثل `Nawat.Request.Squad.Dismiss` لمهمة منفردة).
6. **التفرعات (اختياري):** أضف إلى `Branches` عنصراً بشرط و`TargetStageId`.
7. **التشغيل:** من Blueprint الـ GameMode أو Level Blueprint عند BeginPlay: `Get Game Instance` → `Get Subsystem (NawatStoryDirector)` → `Start Chapter`. أو ضع `ANawatStoryTrigger` عند بداية المستوى وأضف له الفعل `Start Chapter`.
8. **في المستوى:**
   - `ANawatStoryTrigger`: اسحبه إلى المستوى، كبّر الـ Box، أضف في `Actions` فعل `Set Fact` (يرفع الحقيقة التي ينتظرها الهدف). `bTriggerOnce` مفعّل افتراضياً. إن فشلت شروطه عند الدخول يعيد المحاولة كلما تغيّرت حقيقة واللاعب ما زال داخله.
   - `ANawatInteractableActor`: اسحبه، اختر Mesh (اختياري)، اكتب `Prompt`، أضف `Actions` و/أو `Dialogue`. فعّل `bSingleUse` إن كان يُستعمل مرة واحدة. الصندوق `InteractionVolume` هو ما يصيبه فحص النظر؛ كبّره ليغطي الشيء.
   - `ANawatCheckpoint`: اسحبه، حرّك السهم `RespawnPoint` إلى مكان واتجاه عودة اللاعب. يحفظ في الخانة `SlotName` (افتراضياً `NawatAuto`) أول مرة يدخله اللاعب ويبث `Nawat.Event.Checkpoint.Reached`.
9. **اللاعب:** أضف `NawatInteractorComponent` إلى Pawn اللاعب، واربط زر التفاعل بـ `TryInteract`. (وحدة `NawatPresentation` ستعرض النص من `OnFocusChanged` لاحقاً.)

### التفاعل بالتفصيل

- `UNawatInteractorComponent` يفحص عشر مرات في الثانية تقريباً (`UpdateInterval`) خطاً من نقطة نظر اللاعب (كاميرا الـ Controller إن وُجد، وإلا عيون الـ Pawn) بطول `Range` على القناة `TraceChannel` (افتراضياً Visibility). `TraceRadius` أكبر من صفر يجعل الفحص كرة بدل خط.
- في منظور الشخص الثالث المسافة تُقاس من الكاميرا: زد `Range` بطول ذراع الكاميرا.
- لا يوجد تركيز ولا تفاعل ما دام سبب توقف يحجب إدخال اللعب (حوار حاجب، مشهد، قائمة).
- `GetFocusedActor()`، `GetFocusedPrompt()`، `TryInteract()`، والحدث `OnFocusChanged(AActor* NewFocus, FText Prompt)`.
- لجعل أي Actor خاص بك قابلاً للاستعمال: نفّذ الواجهة `NawatInteractable` (`CanInteract`، `GetInteractionPrompt`، `Interact`) في C++ أو Blueprint، وتأكد أن له تصادماً يحجب قناة Visibility.

---

## 4. الحوار

### المفاهيم

`UNawatDialogue` يحتوي `Nodes`. كل `FNawatDialogueNode`:

- `Id`، `SpeakerName`، `Text`، `Portrait` (Texture2D)، `Voice` (Sound).
- `Duration`: مدة السطر بالثواني. صفر = تلقائي: `max(2, 1 + 0.06 × عدد الحروف)`، وإن وُجد صوت فلا تقل عن طول الصوت + 0.3 ثانية.
- `ShowWhen`: إن فشلت يُتخطى السطر ويُجرَّب السطر التالي **بترتيب القائمة**.
- `OnShown`: أفعال تُنفَّذ عند ظهور السطر (قبل حساب الاختيارات المتاحة).
- `Choices`: كل اختيار له `Text`، `Conditions` (يظهر فقط إن نجحت)، `Actions`، `NextNodeId`.
- `NextNodeId`: فارغ = السطر التالي بالترتيب؛ تجاوز النهاية ينهي الحوار. اسم غير موجود ينهي الحوار مع تحذير في السجل.

سطر له اختيارات متاحة ينتظر اللاعب ولا يتقدم بالمؤقت. إن لم ينجح شرط أي اختيار يُعامل السطر كسطر عادي.

### واجهة `UNawatDialogueSubsystem`

```cpp
bool StartDialogue(UNawatDialogue* Dialogue, bool bBlockPlayerInput = true);
void Advance();                 // تخطي إلى السطر التالي؛ يُتجاهل أثناء انتظار اختيار
bool Choose(int32 ChoiceIndex); // الفهرس داخل GetAvailableChoices
void Stop();
bool IsActive() const;
UNawatDialogue* GetCurrentDialogue() const;
FNawatDialogueLine GetCurrentLine() const;   // {SpeakerName, Text, Portrait, bHasChoices, Duration}
TArray<FText> GetAvailableChoices() const;
```

| الحدث | التوقيع |
|---|---|
| `OnDialogueStarted` / `OnDialogueFinished` | `(UNawatDialogue* Dialogue)` |
| `OnLineShown` | `(const FNawatDialogueLine& Line)` |
| `OnChoicesShown` | `(const TArray<FText>& Choices)` — مباشرة بعد `OnLineShown` |

أثناء الحوار يُدفع السبب `Nawat.Pause.Dialogue` إلى `UNawatPauseSubsystem` بسياسة **لا توقف الزمن**، وتحجب الحركة والنظر فقط إن كان `bBlockPlayerInput` صحيحاً، ويُسحب عند الانتهاء. يُبث `Nawat.Event.Dialogue.Started` و`Finished` (Subject = أصل الحوار).

### خطوات المصمم

1. Data Asset → `NawatDialogue`.
2. أضف الأسطر بالترتيب. اترك `NextNodeId` فارغاً للتسلسل العادي.
3. للاختيارات: أضف `Choices` للسطر، ولكل اختيار `NextNodeId` يشير إلى `Id` السطر المقصود.
4. شغّله بالفعل `Start Dialogue` (من مرحلة، Trigger، أو Interactable) أو بوضعه في خانة `Dialogue` على `ANawatInteractableActor`.

---

## 5. الأفعال والشروط الجاهزة

| الاسم في المحرر | الصنف | الخصائص |
|---|---|---|
| Start Chapter | `UNawatAction_StartChapter` | `Chapter`، `StartStageId` |
| Advance Stage | `UNawatAction_AdvanceStage` | — |
| Go To Stage | `UNawatAction_GoToStage` | `StageId` |
| Complete Objective | `UNawatAction_CompleteObjective` | `ObjectiveId` |
| Start Dialogue | `UNawatAction_StartDialogue` | `Dialogue`، `bBlockPlayerInput` |
| Save Game | `UNawatAction_SaveGame` | `SlotName` (لا يغيّر نقطة العودة) |
| Stage Is | `UNawatCondition_StageIs` | `Chapter` (اختياري)، `StageId` |
| Objective Complete | `UNawatCondition_ObjectiveComplete` | `ObjectiveId` (في المرحلة الحالية) |
| Dialogue Active | `UNawatCondition_DialogueActive` | `Dialogue` (اختياري: فارغ = أي حوار) |

ومن النواة: `Set Fact`، `Broadcast Event`، `Fact`، `Group`.

---

## 6. الاستيراد من JSON

```cpp
UNawatDialogue* ImportDialogueFromJson(UObject* Outer, const FString& Json, FString& OutError);
UNawatChapter*  ImportChapterFromJson (UObject* Outer, const FString& Json, FString& OutError);
UNawatDialogue* ImportDialogueFromFile(UObject* Outer, const FString& FilePath, FString& OutError);
UNawatChapter*  ImportChapterFromFile (UObject* Outer, const FString& FilePath, FString& OutError);
```

- الاستيراد **كل شيء أو لا شيء**: عند أي مشكلة تكون النتيجة فارغة و`OutError` يسرد كل المشاكل، سطر لكل مشكلة مع موضعها (مثل `stages[1].objectives[0].completeWhen[0]: unknown gameplay tag 'Story.X'`).
- الوسوم يجب أن تكون موجودة مسبقاً في المشروع؛ الوسم المجهول خطأ استيراد وليس انهياراً.
- الناتج كائن مؤقت (Transient): احتفظ بمرجع له في متغير (مثلاً في Game Instance) وإلا يحذفه جامع النفايات. الاستدعاء `StartChapter` / `StartDialogue` يحتفظ به ما دام يعمل.
- `Outer` قد يكون فارغاً.
- الشروط والأفعال في JSON تقتصر على الحقائق والأحداث.

### الشرط

```json
{ "fact": "Story.Aures.FoundRadio", "op": ">=", "value": 1, "not": false }
```

- `fact` (إلزامي): اسم الوسم.
- `op` (اختياري، الافتراضي `>=`): واحد من `==` `!=` `>` `>=` `<` `<=`.
- `value` (اختياري، الافتراضي `1`): عدد صحيح.
- `not` (اختياري): يعكس النتيجة.

تجميع: `{ "all": [ شرط، ... ] }` أو `{ "any": [ شرط، ... ] }` (ويقبل `not` أيضاً). أي قائمة شروط تعني AND بين عناصرها.

### الفعل

```json
{ "setFact": "Story.Aures.FoundRadio", "value": 1 }
{ "addFact": "Story.Aures.EnemiesKilled", "value": 1 }
{ "event": "Nawat.Request.Squad.Dismiss", "value": 0 }
```

`value` اختياري (الافتراضي `1` للحقائق و`0` للأحداث).

### مخطط الحوار

| الحقل | النوع | ملاحظات |
|---|---|---|
| `nodes` | مصفوفة، إلزامية وغير فارغة | |
| `nodes[].id` | نص | مطلوب فقط إن أُشير إليه |
| `nodes[].speaker` | نص | |
| `nodes[].text` | نص | |
| `nodes[].duration` | عدد | ثوانٍ؛ 0 أو غائب = تلقائي |
| `nodes[].portrait` | نص | مسار أصل، مثل `/Game/UI/T_Omar.T_Omar` |
| `nodes[].voice` | نص | مسار أصل صوت |
| `nodes[].next` | نص | `id` سطر؛ غائب = التالي بالترتيب |
| `nodes[].showWhen` | مصفوفة شروط | |
| `nodes[].onShown` | مصفوفة أفعال | |
| `nodes[].choices[]` | `text`، `next`، `when` (شروط)، `actions` (أفعال) | |

مثال كامل:

```json
{
  "nodes": [
    {
      "id": "greet",
      "speaker": "عمر",
      "text": "وصلتَ أخيراً. الدورية تمرّ من هنا كل ساعة.",
      "voice": "/Game/Audio/VO/Omar_Greet.Omar_Greet",
      "onShown": [ { "setFact": "Story.Aures.MetOmar" } ]
    },
    {
      "id": "ask",
      "speaker": "عمر",
      "text": "ماذا نفعل؟",
      "choices": [
        {
          "text": "ننتظر الليل.",
          "actions": [ { "setFact": "Story.Aures.Plan", "value": 1 } ],
          "next": "wait"
        },
        {
          "text": "نهاجم الآن.",
          "when": [ { "fact": "Story.Aures.HasWeapons" } ],
          "actions": [ { "setFact": "Story.Aures.Plan", "value": 2 } ],
          "next": "attack"
        }
      ]
    },
    {
      "id": "wait",
      "speaker": "عمر",
      "text": "حكيم. سنتحرك بعد الغروب.",
      "next": "end"
    },
    {
      "id": "attack",
      "speaker": "عمر",
      "text": "إذن جهّز سلاحك.",
      "onShown": [ { "event": "Nawat.Request.AI.RaiseAlarm" } ]
    },
    {
      "id": "end",
      "speaker": "عمر",
      "text": "الله معنا.",
      "duration": 2.5
    }
  ]
}
```

### مخطط الفصل

| الحقل | النوع | ملاحظات |
|---|---|---|
| `title`، `subtitle` | نص | |
| `stages` | مصفوفة، إلزامية وغير فارغة | |
| `stages[].id` | نص | |
| `stages[].title` | نص | |
| `stages[].next` | نص | `id` مرحلة؛ غائب = التالية بالترتيب |
| `stages[].enterActions`، `exitActions` | مصفوفة أفعال | |
| `stages[].completeWhen` | مصفوفة شروط | |
| `stages[].objectives[]` | `id`، `text`، `optional` (منطقي)، `completeWhen` (شروط) | |
| `stages[].branches[]` | `when` (شروط)، `target` (`id` مرحلة؛ غائب = إنهاء الفصل) | |

مثال كامل:

```json
{
  "title": "الأوراس",
  "subtitle": "نوفمبر 1954",
  "stages": [
    {
      "id": "reach_village",
      "title": "الوصول إلى القرية",
      "enterActions": [ { "event": "Nawat.Request.Squad.Dismiss" } ],
      "objectives": [
        {
          "id": "enter_village",
          "text": "ادخل القرية دون أن تُكشف",
          "completeWhen": [ { "fact": "Story.Aures.InVillage" } ]
        },
        {
          "id": "find_radio",
          "text": "اعثر على جهاز اللاسلكي",
          "optional": true,
          "completeWhen": [ { "fact": "Story.Aures.FoundRadio" } ]
        }
      ]
    },
    {
      "id": "meet_omar",
      "title": "لقاء عمر",
      "objectives": [
        {
          "id": "talk",
          "text": "تحدث مع عمر",
          "completeWhen": [ { "fact": "Story.Aures.Plan", "op": ">=", "value": 1 } ]
        }
      ],
      "branches": [
        { "when": [ { "fact": "Story.Aures.Plan", "op": "==", "value": 2 } ], "target": "assault" }
      ],
      "next": "night"
    },
    {
      "id": "night",
      "title": "تحت جنح الظلام",
      "objectives": [
        {
          "id": "sabotage",
          "text": "عطّل الشاحنات",
          "completeWhen": [ { "fact": "Story.Aures.TrucksDisabled", "op": ">=", "value": 3 } ]
        }
      ],
      "next": "escape"
    },
    {
      "id": "assault",
      "title": "الهجوم",
      "enterActions": [
        { "event": "Nawat.Request.Squad.Summon" },
        { "event": "Nawat.Request.AI.RaiseAlarm" }
      ],
      "completeWhen": [ { "fact": "Story.Aures.CampCleared" } ]
    },
    {
      "id": "escape",
      "title": "الانسحاب",
      "objectives": [
        {
          "id": "reach_hills",
          "text": "انسحب إلى التلال",
          "completeWhen": [ { "fact": "Story.Aures.ReachedHills" } ]
        }
      ],
      "exitActions": [ { "setFact": "Story.Aures.Done" } ]
    }
  ]
}
```

استعمال من Blueprint: `Import Chapter From File` (المسار مثلاً `<ProjectDir>/Story/aures.json` عبر `Get Project Directory`) → خزّن الناتج في متغير → `Start Chapter`. إن كان `OutError` غير فارغ اطبعه.

---

## 7. قائمة القبول اليدوية (داخل المحرر)

نفّذها بالترتيب بعد أن يُبنى المشروع دون أخطاء.

**البناء**

- [ ] المشروع يُبنى والوحدة `NawatNarrative` تظهر محمّلة (Tools → Debug → Modules).
- [ ] `NawatChapter` و`NawatDialogue` يظهران في قائمة Data Asset، والشروط/الأفعال تظهر في القوائم المنسدلة بأسمائها (Start Chapter، Stage Is، ...).

**الفصل**

- [ ] أنشئ وسمين `Story.Test.A` و`Story.Test.B` وفصلاً بمرحلتين: الأولى هدفها ينتظر `Story.Test.A`، والثانية هدفها ينتظر `Story.Test.B`.
- [ ] `Start Chapter` عند BeginPlay: يُطبع `OnChapterStarted` ثم `OnStageChanged` بالفهرس 0.
- [ ] ضع `NawatStoryTrigger` بفعل `Set Fact (Story.Test.A)`: عند الدخول يُطبع `OnObjectiveCompleted` ثم `OnStageChanged` بالفهرس 1.
- [ ] ارفع `Story.Test.B`: يُطبع `OnChapterCompleted` و`IsChapterActive` يصبح خاطئاً.
- [ ] مرحلة بلا أهداف ولا شروط تبقى حتى يُستدعى `AdvanceStage`.
- [ ] تفرع: مرحلة لها `Branches` بشرط ناجح تقفز إلى `TargetStageId` وليس إلى التالية.
- [ ] `EnterActions` فيها `Advance Stage`: الانتقال يتم مرة واحدة دون تعليق أو انهيار، و`OnStageChanged` يُبث للمرحلتين بالترتيب.
- [ ] `GetObjectiveStates` يُرجع الأهداف مع `bComplete` الصحيح.

**الحفظ**

- [ ] ادخل `NawatCheckpoint` في منتصف المرحلة الثانية (بعد إكمال هدف اختياري فيها): يُبث `Nawat.Event.Checkpoint.Reached` وتوجد الخانة `NawatAuto`.
- [ ] أوقف اللعب، شغّل من جديد، استدعِ `LoadFromSlot`: الـ Director على المرحلة الثانية، الهدف المكتمل ما زال مكتملاً، `EnterActions` لم تُنفَّذ مجدداً، و`OnStageChanged` بُث.
- [ ] `GetCheckpointTransform` يُرجع موضع السهم واتجاهه.

**الحوار**

- [ ] حوار من ثلاثة أسطر دون مدة: الأسطر تتوالى تلقائياً، `OnLineShown` لكل سطر، ثم `OnDialogueFinished`.
- [ ] أثناء الحوار مع `bBlockPlayerInput`: الحركة والنظر محجوبان والزمن يجري (الأعداء يتحركون). بعده يعود الإدخال.
- [ ] `Advance` يتخطى السطر فوراً ويوقف صوته.
- [ ] سطر باختيارين أحدهما شرطه فاشل: `OnChoicesShown` يحمل اختياراً واحداً، `Advance` لا يفعل شيئاً، `Choose(0)` ينفّذ أفعاله وينتقل إلى `next`.
- [ ] سطر له `Voice`: الصوت يُسمع، والسطر لا ينتهي قبل الصوت.
- [ ] فتح قائمة التوقف أثناء الحوار يجمّد مؤقت السطر، وإغلاقها يكمله.

**التفاعل**

- [ ] أضف `NawatInteractorComponent` للاعب وضع `NawatInteractableActor` بـ `Prompt`: `OnFocusChanged` يُبث عند النظر إليه من مسافة أقل من `Range` وعند الابتعاد.
- [ ] `TryInteract` ينفّذ `Actions` ويبدأ `Dialogue`. مع `bSingleUse` لا يُركَّز عليه مرة ثانية.
- [ ] Interactable بشرط فاشل لا يُركَّز عليه؛ يصبح قابلاً للتركيز بعد رفع الحقيقة.
- [ ] `NawatStoryTrigger` بشرط فاشل: لا ينطلق عند الدخول، وينطلق فور رفع الحقيقة واللاعب داخله.
- [ ] عدو (ليس اللاعب) يدخل الـ Trigger أو الـ Checkpoint: لا شيء يحدث.

**JSON**

- [ ] استورد مثالي هذا الملف بعد إضافة وسومهما: الناتج غير فارغ و`OutError` فارغ، والفصل والحوار يعملان.
- [ ] غيّر وسماً إلى اسم غير موجود: الناتج فارغ و`OutError` يذكر الوسم وموضعه، دون انهيار.
- [ ] JSON معطوب (قوس ناقص): الناتج فارغ و`OutError` يشرح.
