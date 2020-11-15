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

	//텔레포트 실린더 끄기
	DestinationMarker->SetVisibility(false);

	//블링커 메테리얼(속도 빠르면 시야제한) 만들어주기
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

bool AVRCharacter::FindTeleportDestination(TArray<FVector> &OutPath, FVector &OutLocation)
{
	FVector Look = RightController->GetActorForwardVector();
	FVector Start = RightController->GetActorLocation() + Look * 3.0f; //오큘러스 컨트롤러 때문에 앞으로 조금 빼 뒀음, 나중에 무시할 물체는 FPredictProjectilePathParams구조체 설정에서 ignore 설정하는게 좋을 듯

	FPredictProjectilePathParams Params(
		TeleportProjectileRadius, 
		Start,
		Look * TeleportProjectileSpeed, 
		TeleportSimulationTime,
		ECC_Visibility,
		this
	);
	Params.bTraceComplex = true;		//메쉬가 제대로된 콜리전이 있어야함, 이건 콜리전이 없을때도 가능하게 편법을 사용한 것 - 완벽하진 않음 여전히 메시가 콜리전이 있어야함
	FPredictProjectilePathResult Result;
	bool bHit = UGameplayStatics::PredictProjectilePath(this, Params, Result);
	
	if (!bHit) return false;

	for (FPredictProjectilePathPointData PointData : Result.PathData)
	{
		OutPath.Add(PointData.Location);
	}

	//네비게이션 메쉬 안에 캐스팅 됐는지 체크
	FNavLocation NavLocation;
	bool bOnNavMesh = UNavigationSystemV1::GetCurrent(GetWorld())->ProjectPointToNavigation(Result.HitResult.Location, NavLocation, TeleportProjectExtent);

	if (!bOnNavMesh) return false;

	OutLocation = NavLocation.Location;

	return bOnNavMesh && bHit;
}

//텔레포트 레이트레이싱 위치 마킹
void AVRCharacter::UpdateDestinationMarker()
{
	TArray<FVector> Path;
	FVector Location;
	bool bHasDestination = FindTeleportDestination(Path, Location);

	if (bHasDestination)		//텔레포트 가능할 때
	{
		//텔레포트 실린더 켜기
		DestinationMarker->SetVisibility(true);

		DestinationMarker->SetWorldLocation(Location);

		DrawTeleportPath(Path);
	}
	else			//텔레포트 불가능할 때
	{
		//텔레포트 실린더 끄기
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

	PlayerInputComponent->BindAxis(TEXT("Forward"), this, &AVRCharacter::MoveForward);		//왼쪽 스틱 앞뒤로 이동
	PlayerInputComponent->BindAxis(TEXT("RIght"), this, &AVRCharacter::MoveRight);			//왼쪽 스틱 좌우로 이동
	PlayerInputComponent->BindAction(TEXT("Teleport"), IE_Released, this, &AVRCharacter::BeginTeleport);	//버튼 텔레포트
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AVRCharacter::TurnPlayer);			//오른쪽 스틱으로 회전
	//PlayerInputComponent->BindAction(TEXT("GripLeft"), IE_Pressed, this, &AVRCharacter::BeginTeleport);	//버튼 텔레포트
	//PlayerInputComponent->BindAction(TEXT("ReleaseLeft"), IE_Released, this, &AVRCharacter::BeginTeleport);	//버튼 텔레포트
	//PlayerInputComponent->BindAction(TEXT("GripRight"), IE_Pressed, this, &AVRCharacter:://);
	//PlayerInputComponent->BindAction(TEXT("ReleaseRight"), IE_Released, this, &AVRCharacter:://);
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