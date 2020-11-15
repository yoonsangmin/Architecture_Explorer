// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VRCharacter.generated.h"

UCLASS()
class ARCHITECTUREEXPLORER_API AVRCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AVRCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:

	bool FindTeleportDestination(TArray<FVector> &OutPath, FVector &OutLocation);
	void UpdateDestinationMarker();
	void UpdateBlinkers();
	void DrawTeleportPath(const TArray<FVector>& Path);
	void UpdateSpline(const TArray<FVector> &Path);
	FVector2D GetBlinkerCentre();

	void MoveForward(float throttle);
	void MoveRight(float throttle);

	void BeginTeleport();
	void FinishTeleport();

	void StartFade(float FromAlpha, float ToAlpha);

	void TurnPlayer(float throttle);
	void ExecuteTurn(int direction);
	void ExecuteTurn_2(int direction);
private:

	UPROPERTY()
	class UCameraComponent* Camera;

	UPROPERTY()
	class AHandController* LeftController;

	UPROPERTY()
	class AHandController* RightController;

	UPROPERTY()
	class USceneComponent* VRRoot;

	UPROPERTY(VisibleAnywhere)
	class USplineComponent* TeleportPath;

	UPROPERTY(VisibleAnywhere)
	class UStaticMeshComponent* DestinationMarker;

	UPROPERTY()
	class UPostProcessComponent* PostProcessComponent;
	
	UPROPERTY()
	class UMaterialInstanceDynamic* BlinkerMaterialInstance;

	UPROPERTY(VisibleAnywhere)
	TArray<class USplineMeshComponent*> TeleportPathMeshPool;

private:
	//Configuration Parameters
	UPROPERTY(EditAnywhere)
	float MoveSpeed = 1.0f;

	UPROPERTY(EditAnywhere)
	float TeleportProjectileRadius = 10;

	UPROPERTY(EditAnywhere)
	float TeleportProjectileSpeed = 600;

	UPROPERTY(EditAnywhere)
	float TeleportSimulationTime = 1;

	UPROPERTY(EditAnywhere)
	float TeleportFadeTime = 0.5;

	UPROPERTY(EditAnywhere)
	FVector TeleportProjectExtent = FVector(100, 100, 100);

	UPROPERTY(EditAnywhere)
	class UMaterialInterface* BlinkerMaterialBase;

	UPROPERTY(EditAnywhere)
	class UCurveFloat* RadiusVsVelocity;

	UPROPERTY(EditAnywhere)
	float TurningDegree = 30.0f;

	UPROPERTY(EditDefaultsOnly)
	class UStaticMesh* TeleportArchMesh;

	UPROPERTY(EditDefaultsOnly)
	class UMaterialInterface* TeleportArchMaterial;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AHandController> HandControllerClass;

	private:
	//State 변수
	UPROPERTY(EditAnywhere)	//컨트롤러 턴 할 때 한번만 실행시키는 시키는 키값
	bool IsTurned = false;

};
