// Fill out your copyright notice in the Description page of Project Settings.


#include "VRCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MotionControllerComponent.h"

// Sets default values
AVRCharacter::AVRCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	VRRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VRRoot"));
	VRRoot->SetupAttachment(GetRootComponent());

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VRRoot);

	LeftController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("LeftController"));
	LeftController->SetupAttachment(VRRoot);
	LeftController->SetTrackingSource(EControllerHand::Left);
	//��Ʈ�ѷ� ����
	//LeftController->bDisplayDeviceModel = true;

	RightController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightController"));
	RightController->SetupAttachment(VRRoot);
	RightController->SetTrackingSource(EControllerHand::Right);
	//��Ʈ�ѷ� ����
	//RightController->bDisplayDeviceModel = true;

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

bool AVRCharacter::FindTeleportDestination(FVector& OutLocation)
{
	FVector Look = RightController->GetForwardVector();
	Look = Look.RotateAngleAxis(30, RightController->GetRightVector());
	FVector Start = RightController->GetComponentLocation() + Look * 5.0f;
	FVector End = Start + Look * MaxTeleportDistance;

	FHitResult HitResult;
	bool bhit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility);

	if (!bhit) return false;

	//�׺���̼� �޽� �ȿ� ĳ���� �ƴ��� üũ
	FNavLocation NavLocation;
	bool bOnNavMesh = UNavigationSystemV1::GetCurrent(GetWorld())->ProjectPointToNavigation(HitResult.Location, NavLocation, TeleportProjectExtent);

	if (!bOnNavMesh) return false;

	OutLocation = NavLocation.Location;

	return bOnNavMesh && bhit;
}

//�ڷ���Ʈ ����Ʈ���̽� ��ġ ��ŷ
void AVRCharacter::UpdateDestinationMarker()
{
	FVector Location;
	bool bHasDestination = FindTeleportDestination(Location);

	if (bHasDestination)		//�ڷ���Ʈ ������ ��
	{
		//�ڷ���Ʈ �Ǹ��� �ѱ�
		DestinationMarker->SetVisibility(true);

		DestinationMarker->SetWorldLocation(Location);
	}
	else			//�ڷ���Ʈ �Ұ����� ��
	{
		//�ڷ���Ʈ �Ǹ��� ����
		DestinationMarker->SetVisibility(false);
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