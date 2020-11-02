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
	//컨트롤러 더미
	//LeftController->bDisplayDeviceModel = true;

	RightController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightController"));
	RightController->SetupAttachment(VRRoot);
	RightController->SetTrackingSource(EControllerHand::Right);
	//컨트롤러 더미
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

	//텔레포트 실린더 끄기
	DestinationMarker->SetVisibility(false);

	//블링커 메테리얼(속도 빠르면 시야제한) 만들어주기
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

	//충돌체를 카메라 위치로 옮기는 작업
	//1.액터 전체를 카메라의 위치로 옮김, 높이는 신경쓰지 않음
	FVector NewCameraOffset = Camera->GetComponentLocation() - GetActorLocation();
	NewCameraOffset.Z = 0.0f;
	AddActorWorldOffset(NewCameraOffset);
	//2.카메라를 포함한 VRRoot를 액터를 옮긴만큼 다시 돌려놓음(카메라도 같이 움직였기 때문에 돌려놔야 함)
	VRRoot->AddWorldOffset(-NewCameraOffset);
	
	//텔레포트 레이트레이싱
	UpdateDestinationMarker();

	//속도 계산해서 블링커 크기 변경
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

	//네비게이션 메쉬 안에 캐스팅 됐는지 체크
	FNavLocation NavLocation;
	bool bOnNavMesh = UNavigationSystemV1::GetCurrent(GetWorld())->ProjectPointToNavigation(HitResult.Location, NavLocation, TeleportProjectExtent);

	if (!bOnNavMesh) return false;

	OutLocation = NavLocation.Location;

	return bOnNavMesh && bhit;
}

//텔레포트 레이트레이싱 위치 마킹
void AVRCharacter::UpdateDestinationMarker()
{
	FVector Location;
	bool bHasDestination = FindTeleportDestination(Location);

	if (bHasDestination)		//텔레포트 가능할 때
	{
		//텔레포트 실린더 켜기
		DestinationMarker->SetVisibility(true);

		DestinationMarker->SetWorldLocation(Location);
	}
	else			//텔레포트 불가능할 때
	{
		//텔레포트 실린더 끄기
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

	PlayerInputComponent->BindAxis(TEXT("Forward"), this, &AVRCharacter::MoveForward);		//왼쪽 스틱 앞뒤로 이동
	PlayerInputComponent->BindAxis(TEXT("RIght"), this, &AVRCharacter::MoveRight);			//왼쪽 스틱 좌우로 이동
	PlayerInputComponent->BindAction(TEXT("Teleport"), IE_Released, this, &AVRCharacter::BeginTeleport);	//버튼 텔레포트
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AVRCharacter::TurnPlayer);			//오른쪽 스틱으로 회전
}

//전후 이동
void AVRCharacter::MoveForward(float throttle)
{
	AddMovementInput(throttle * MoveSpeed * Camera->GetForwardVector());
}

//좌우 이동
void AVRCharacter::MoveRight(float throttle)
{
	AddMovementInput(throttle * MoveSpeed * Camera->GetRightVector());
}

//텔레포트 타이머, 페이드 아웃
void AVRCharacter::BeginTeleport()
{
	StartFade(0, 1);

	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, this, &AVRCharacter::FinishTeleport, TeleportFadeTime);
}

//텔레포트 실행, 페이드 인
void AVRCharacter::FinishTeleport()
{
	FVector Destination = DestinationMarker->GetComponentLocation();

	Destination += GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * GetActorUpVector();

	SetActorLocation(Destination);

	StartFade(1, 0);
}

//카메라 페이드 코드
void AVRCharacter::StartFade(float FromAlpha, float ToAlpha)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC != nullptr)
	{
		PC->PlayerCameraManager->StartCameraFade(FromAlpha, ToAlpha, TeleportFadeTime, FLinearColor::Black);
	}
}

// 오른쪽 스틱으로 회전하는 코드
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

//회전 실행하는 함수
void AVRCharacter::ExecuteTurn(int direction)
{
	FVector PrevLocation = Camera->GetComponentLocation();

	VRRoot->SetWorldRotation(VRRoot->GetComponentRotation() + FRotator(0.0f, direction * TurningDegree, 0.0f));

	PrevLocation = PrevLocation - Camera->GetComponentLocation();
	PrevLocation.Z = 0.0f;

	VRRoot->AddWorldOffset(PrevLocation);

	IsTurned = true;
}

//회전 실행하는 함수 2 다른 방식으로 구현해 봄 버그가 있을지 모르니
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


//해야하는 것

//텔포 관련 - 버튼 눌렀을 때 위치 계산 시작하게 하기
//버튼 땠을 때 텔포 실행
//버튼 눌러졌을 때 다른 위치로 못 가게 하기 - 트레이스를 꺼버려서 오브젝트가 그 자리에 있게해도 되고
//텔포 불가능할 때 실행되지 않게 만들기
//왼손 오른손 모두 텔포 가능하게 만들기
//텔레포트 트레이스 채널 바꾸기 - visible 이면 혹시나 파티클이나 아이템에 막히니까