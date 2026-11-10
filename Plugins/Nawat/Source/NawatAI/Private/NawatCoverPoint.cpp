#include "NawatCoverPoint.h"

#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "NawatCoverSubsystem.h"

ANawatCoverPoint::ANawatCoverPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

#if WITH_EDITORONLY_DATA
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (SpriteComponent)
	{
		SpriteComponent->SetupAttachment(SceneRoot);
	}

	ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (ArrowComponent)
	{
		ArrowComponent->SetupAttachment(SceneRoot);
	}
#endif
}

void ANawatCoverPoint::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (UNawatCoverSubsystem* Covers = World ? World->GetSubsystem<UNawatCoverSubsystem>() : nullptr)
	{
		Covers->RegisterPoint(this);
	}
}

void ANawatCoverPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UWorld* World = GetWorld();
	if (UNawatCoverSubsystem* Covers = World ? World->GetSubsystem<UNawatCoverSubsystem>() : nullptr)
	{
		Covers->UnregisterPoint(this);
	}

	Super::EndPlay(EndPlayReason);
}
