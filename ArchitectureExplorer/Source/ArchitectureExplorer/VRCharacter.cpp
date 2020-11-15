// Fill out your copyright notice in the Description page of Project Settings.


#include "VRCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MotionControllerComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "HandController.h"

// Sets default values
AVRCharacter::AVRCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	VRRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VRRoot"));
	VRRoot->SetupAttachment(GetRootComponent());

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VRRoot);

	TeleportPath = CreateDefaultSubobject<USplineComponent>(TEXT("TeleportPath"));
	TeleportPath->SetupAttachment(VRRoot);

	DestinationMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DestinationMarker"));
	DestinationMarker->SetupAttachment(GetRootComponent());
	
	PostProcessComponent = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcessComponent"));
	PostProcessComponent->SetupAttachment(GetRootComponent());

}

// Called when the game starts or when spawned
void AVRCharacter::BeginPlay()
{
	Super::BeginPlay();

	//�ڷ���Ʈ �Ǹ��� ����
	DestinationMarker->SetVisibility(false);

	//��Ŀ ���׸���(�ӵ� ������ �þ�����) ������ֱ�
	if (BlinkerMaterialBase != nullptr)
	{
		BlinkerMaterialInstance = UMaterialInstanceDynamic::Create(BlinkerMaterialBase, this);
		PostProcessComponent->AddOrUpdateBlendable(BlinkerMaterialInstance);
	}

	LeftController = GetWorld()->SpawnActor<AHandController>(HandControllerClass);
	if (LeftController != nullptr)
	{
		LeftController->AttachToComponent(VRRoot, FAttachmentTransformRules::KeepRelativeTransform);
		LeftController->SetHand(EControllerHand::Left);
		LeftController->SetOwner(this);
	}
	
	RightController = GetWorld()->SpawnActor<AHandController>(HandControllerClass);
	if (RightController != nullptr)
	{
		RightController->AttachToComponent(VRRoot, FAttachmentTransformRules::KeepRelativeTransform);
		RightController->SetHand(EControllerHand::Right);
		RightController->SetRight();
		RightController->SetOwner(this);
	}

}

// Called every frame
void AVRCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//�浹ü�� ī�޶� ��ġ�� �ű�� �۾�
	//1.���� ��ü�� ī�޶��� ��ġ�� �ű�, ���̴� �Ű澲�� ����
	FVector NewCameraOffset = Camera->GetComponentLocation() - GetActorLocation();
	NewCameraOffset.Z = 0.0f;
	AddActorWorldOffset(NewCameraOffset);
	//2.ī�޶� ������ VRRoot�� ���͸� �ű丸ŭ �ٽ� ��������(ī�޶� ���� �������� ������ �������� ��)
	VRRoot->AddWorldOffset(-NewCameraOffset);
	
	//�ڷ���Ʈ ����Ʈ���̽�
	UpdateDestinationMarker();

	//�ӵ� ����ؼ� ��Ŀ ũ�� ����
	UpdateBlinkers();
}

bool AVRCharacter::FindTeleportDestination(TArray<FVector> &OutPath, FVector &OutLocation)
{
	FVector Look = RightController->GetActorForwardVector();
	FVector Start = RightController->GetActorLocation() + Look * 3.0f; //��ŧ���� ��Ʈ�ѷ� ������ ������ ���� �� ����, ���߿� ������ ��ü�� FPredictProjectilePathParams����ü �������� ignore �����ϴ°� ���� ��

	FPredictProjectilePathParams Params(
		TeleportProjectileRadius, 
		Start,
		Look * TeleportProjectileSpeed, 
		TeleportSimulationTime,
		ECC_Visibility,
		this
	);
	Params.bTraceComplex = true;		//�޽��� ����ε� �ݸ����� �־����, �̰� �ݸ����� �������� �����ϰ� ����� ����� �� - �Ϻ����� ���� ������ �޽ð� �ݸ����� �־����
	FPredictProjectilePathResult Result;
	bool bHit = UGameplayStatics::PredictProjectilePath(this, Params, Result);
	
	if (!bHit) return false;

	for (FPredictProjectilePathPointData PointData : Result.PathData)
	{
		OutPath.Add(PointData.Location);
	}

	//�׺���̼� �޽� �ȿ� ĳ���� �ƴ��� üũ
	FNavLocation NavLocation;
	bool bOnNavMesh = UNavigationSystemV1::GetCurrent(GetWorld())->ProjectPointToNavigation(Result.HitResult.Location, NavLocation, TeleportProjectExtent);

	if (!bOnNavMesh) return false;

	OutLocation = NavLocation.Location;

	return bOnNavMesh && bHit;
}

//�ڷ���Ʈ ����Ʈ���̽� ��ġ ��ŷ
void AVRCharacter::UpdateDestinationMarker()
{
	TArray<FVector> Path;
	FVector Location;
	bool bHasDestination = FindTeleportDestination(Path, Location);

	if (bHasDestination)		//�ڷ���Ʈ ������ ��
	{
		//�ڷ���Ʈ �Ǹ��� �ѱ�
		DestinationMarker->SetVisibility(true);

		DestinationMarker->SetWorldLocation(Location);

		DrawTeleportPath(Path);
	}
	else			//�ڷ���Ʈ �Ұ����� ��
	{
		//�ڷ���Ʈ �Ǹ��� ����
		DestinationMarker->SetVisibility(false);

		TArray<FVector> EmptyPath;
		DrawTeleportPath(EmptyPath);
	}
}

void AVRCharacter::UpdateBlinkers()
{
	if (RadiusVsVelocity == nullptr) return;

	float Speed = GetVelocity().Size();
	float Radius = RadiusVsVelocity->GetFloatValue(Speed);

	BlinkerMaterialInstance->SetScalarParameterValue(TEXT("Radius"), Radius);

	FVector2D Centre = GetBlinkerCentre();

	BlinkerMaterialInstance->SetVectorParameterValue(TEXT("Centre"), FLinearColor(Centre.X, Centre.Y, 0.0f));
}

void AVRCharacter::DrawTeleportPath(const TArray<FVector>& Path)
{
	UpdateSpline(Path);

	for (USplineMeshComponent* SplineMesh : TeleportPathMeshPool)
	{
		SplineMesh->SetVisibility(false);
	}

	int32 SegmentNum = Path.Num() - 1;
	for (int32 i = 0; i < SegmentNum; ++i)
	{
		if (TeleportPathMeshPool.Num() <= i)
		{
			USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(this);
			SplineMesh->SetMobility(EComponentMobility::Movable);
			SplineMesh->AttachToComponent(TeleportPath, FAttachmentTransformRules::KeepRelativeTransform);
			SplineMesh->SetStaticMesh(TeleportArchMesh);
			SplineMesh->SetMaterial(0, TeleportArchMaterial);
			SplineMesh->RegisterComponent();

			TeleportPathMeshPool.Add(SplineMesh);
		}
	
		USplineMeshComponent* SplineMesh = TeleportPathMeshPool[i];
		SplineMesh->SetVisibility(true);

		FVector StartPos, StartTangent, EndPos, EndTangent;
		TeleportPath->GetLocalLocationAndTangentAtSplinePoint(i, StartPos, StartTangent);
		TeleportPath->GetLocalLocationAndTangentAtSplinePoint(i + 1, EndPos, EndTangent);
		SplineMesh->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent);
	}

}

void AVRCharacter::UpdateSpline(const TArray<FVector> &Path)
{
	TeleportPath->ClearSplinePoints(false);

	for (int32 i = 0; i < Path.Num(); ++i)
	{
		FVector LocalPosition = TeleportPath->GetComponentTransform().InverseTransformPosition(Path[i]);
		FSplinePoint Point(i, LocalPosition, ESplinePointType::Curve);
		TeleportPath->AddPoint(Point, false);
	}

	TeleportPath->UpdateSpline();
}

FVector2D AVRCharacter::GetBlinkerCentre()
{
	FVector MovementDirection = GetVelocity().GetSafeNormal();
	if (MovementDirection.IsNearlyZero())
	{
		return FVector2D(0.5f, 0.5f);
	}

	FVector WorldStationaryLocation;
	if (FVector::DotProduct(Camera->GetForwardVector(), MovementDirection) > 0)
	{
		WorldStationaryLocation = Camera->GetComponentLocation() + MovementDirection * 1000;
	}
	else
	{
		WorldStationaryLocation = Camera->GetComponentLocation() - MovementDirection * 1000;
	}


	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC == nullptr)
	{
		return FVector2D(0.5f, 0.5f);
	}

	FVector2D ScreenStationaryLocation;

	PC->ProjectWorldLocationToScreen(WorldStationaryLocation, ScreenStationaryLocation);

	int32 SizeX, SizeY;
	PC->GetViewportSize(SizeX, SizeY);
	ScreenStationaryLocation.X /= SizeX;
	ScreenStationaryLocation.Y /= SizeY;

	return ScreenStationaryLocation;
}

// Called to bind functionality to input
void AVRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("Forward"), this, &AVRCharacter::MoveForward);		//���� ��ƽ �յڷ� �̵�
	PlayerInputComponent->BindAxis(TEXT("RIght"), this, &AVRCharacter::MoveRight);			//���� ��ƽ �¿�� �̵�
	PlayerInputComponent->BindAction(TEXT("Teleport"), IE_Released, this, &AVRCharacter::BeginTeleport);	//��ư �ڷ���Ʈ
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AVRCharacter::TurnPlayer);			//������ ��ƽ���� ȸ��
	//PlayerInputComponent->BindAction(TEXT("GripLeft"), IE_Pressed, this, &AVRCharacter::BeginTeleport);	//��ư �ڷ���Ʈ
	//PlayerInputComponent->BindAction(TEXT("ReleaseLeft"), IE_Released, this, &AVRCharacter::BeginTeleport);	//��ư �ڷ���Ʈ
	//PlayerInputComponent->BindAction(TEXT("GripRight"), IE_Pressed, this, &AVRCharacter:://);
	//PlayerInputComponent->BindAction(TEXT("ReleaseRight"), IE_Released, this, &AVRCharacter:://);
}

//���� �̵�
void AVRCharacter::MoveForward(float throttle)
{
	AddMovementInput(throttle * MoveSpeed * Camera->GetForwardVector());
}

//�¿� �̵�
void AVRCharacter::MoveRight(float throttle)
{
	AddMovementInput(throttle * MoveSpeed * Camera->GetRightVector());
}

//�ڷ���Ʈ Ÿ�̸�, ���̵� �ƿ�
void AVRCharacter::BeginTeleport()
{
	StartFade(0, 1);

	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, this, &AVRCharacter::FinishTeleport, TeleportFadeTime);
}

//�ڷ���Ʈ ����, ���̵� ��
void AVRCharacter::FinishTeleport()
{
	FVector Destination = DestinationMarker->GetComponentLocation();
	Destination += GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * GetActorUpVector();
	SetActorLocation(Destination);

	StartFade(1, 0);
}

//ī�޶� ���̵� �ڵ�
void AVRCharacter::StartFade(float FromAlpha, float ToAlpha)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC != nullptr)
	{
		PC->PlayerCameraManager->StartCameraFade(FromAlpha, ToAlpha, TeleportFadeTime, FLinearColor::Black);
	}
}

// ������ ��ƽ���� ȸ���ϴ� �ڵ�
void AVRCharacter::TurnPlayer(float throttle)
{
	if (throttle > 0.7f && !IsTurned)
	{
		ExecuteTurn(1);
	}

	else if (throttle < -0.7f && !IsTurned)
	{
		ExecuteTurn(-1);
	}

	else if (throttle > -0.3f && throttle < 0.3f)
	{
		IsTurned = false;
	}
}

//ȸ�� �����ϴ� �Լ�
void AVRCharacter::ExecuteTurn(int direction)
{
	FVector PrevLocation = Camera->GetComponentLocation();

	VRRoot->SetWorldRotation(VRRoot->GetComponentRotation() + FRotator(0.0f, direction * TurningDegree, 0.0f));

	PrevLocation = PrevLocation - Camera->GetComponentLocation();
	PrevLocation.Z = 0.0f;

	VRRoot->AddWorldOffset(PrevLocation);

	IsTurned = true;
}

//ȸ�� �����ϴ� �Լ� 2 �ٸ� ������� ������ �� ���װ� ������ �𸣴�
void AVRCharacter::ExecuteTurn_2(int direction)
{
	FVector RelativeLocation = Camera->GetComponentLocation() - VRRoot->GetComponentLocation();
	float TurningDegree_Radius = direction * TurningDegree * (PI / 180);

	FVector Diff = FVector(RelativeLocation.X - (RelativeLocation.X * cos(TurningDegree_Radius) - RelativeLocation.Y * sin(TurningDegree_Radius)),
		RelativeLocation.Y - (RelativeLocation.X * sin(TurningDegree_Radius) + RelativeLocation.Y * cos(TurningDegree_Radius)), 0.0f);

	VRRoot->SetWorldRotation(VRRoot->GetComponentRotation() + FRotator(0.0f, direction * TurningDegree, 0.0f));
	VRRoot->AddWorldOffset(Diff);

	IsTurned = true;
}


//�ؾ��ϴ� ��

//���� ���� - ��ư ������ �� ��ġ ��� �����ϰ� �ϱ�
//��ư ���� �� ���� ����
//��ư �������� �� �ٸ� ��ġ�� �� ���� �ϱ� - Ʈ���̽��� �������� ������Ʈ�� �� �ڸ��� �ְ��ص� �ǰ�
//���� �Ұ����� �� ������� �ʰ� �����
//�޼� ������ ��� ���� �����ϰ� �����
//�ڷ���Ʈ Ʈ���̽� ä�� �ٲٱ� - visible �̸� Ȥ�ó� ��ƼŬ�̳� �����ۿ� �����ϱ�