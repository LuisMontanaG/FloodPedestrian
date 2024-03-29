// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Windows/MinWindows.h"
#include "Pedestrian.h"
#include "Engine/StaticMeshActor.h"
#include "FloodPedestrianGameModeBase.generated.h"



#define xmachine_memory_agent_MAX 16384 //same number of agents as FLAME
#define GRID_SIZE 128
#define NUM_MODELS 2 //corresponds to gender variable (agent model)
#define SCALE 250.0f

//agent data read from FLAME
struct pedestrian_data
{
	int total;
	int gender[xmachine_memory_agent_MAX];
	float x[xmachine_memory_agent_MAX];
	float y[xmachine_memory_agent_MAX];
	float z[xmachine_memory_agent_MAX];
	float velX[xmachine_memory_agent_MAX];
	float velY[xmachine_memory_agent_MAX];
	float speed[xmachine_memory_agent_MAX];
};

//water data read from FLAME
struct flood_data
{
	int total;
	float x[xmachine_memory_agent_MAX];
	float y[xmachine_memory_agent_MAX];
	float z[xmachine_memory_agent_MAX];
};

//simulation options sent to FLAME
struct options_data
{
	int lock;
	int quit;
};

/**
 * 
 */
UCLASS()
class FLOODPEDESTRIAN_API AFloodPedestrianGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFloodPedestrianGameModeBase();

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;

protected:

	//HUD functions
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void StartSimulation();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void StopSimulation();

	UFUNCTION(BlueprintCallable, Category = "Menu")
		void Quit();

	//HUD
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD", Meta = (BlueprintProtected = "true"))
		TSubclassOf<class UUserWidget> menuWidgetClass;
	UPROPERTY()
		class UUserWidget* menuWidget;

private:

	bool simulationStarted;
	bool firstFrame; //used to store the intial vertex positions of the water plane

	TArray<FVector> initialVertexPositions;

	//communication
	HANDLE hPedestrianMapFile, hFloodMapFile, hOptionsMapFile;
	LPCTSTR pedestrianBuf, floodBuf, optionsBuf;
	pedestrian_data* pedestrians;
	flood_data* floodCells;
	options_data* options;

	//map, this is the plane to be modified by the water level
	UPROPERTY(EditAnyWhere, Category = "Maps")
		UStaticMesh* SMMap;
	AStaticMeshActor* map;
	//use this if you want to add material to map
	/*UPROPERTY(EditAnyWhere, Category = "Maps")
		UMaterial* waterMaterial;*/

	//store previous displacement to only add the difference each frame
	TArray<float> previousDisplacement;

	//pedestrians
	TArray<APedestrian*> agents;

	//Character 0
	UPROPERTY(EditAnyWhere, Category = "Characters")
		UStaticMesh* SMPedestrian0;
	//LOD0
	UPROPERTY(EditAnyWhere, Category = "Characters")
		UMaterial* pedestrianMaterial00;
	//LOD1
	UPROPERTY(EditAnyWhere, Category = "Characters")
		UMaterial* pedestrianMaterialLOD100;
	//LOD2
	UPROPERTY(EditAnyWhere, Category = "Characters")
		UMaterial* pedestrianMaterialLOD200;


	//Character 1
	UPROPERTY(EditAnyWhere, Category = "Characters")
		UStaticMesh* SMPedestrian1;
	//LOD0
	UPROPERTY(EditAnyWhere, Category = "Characters")
		UMaterial* pedestrianMaterial10;
	//LOD1
	UPROPERTY(EditAnyWhere, Category = "Characters")
		UMaterial* pedestrianMaterialLOD110;
	//LOD2
	UPROPERTY(EditAnyWhere, Category = "Characters")
		UMaterial* pedestrianMaterialLOD210;

	//wigs
	//Character 1
	UPROPERTY(EditAnyWhere, Category = "Characters")
		UMaterial* wigMaterial10;
	//LOD1
	UPROPERTY(EditAnyWhere, Category = "Characters")
		UMaterial* wigMaterialLOD110;
	//LOD2
	UPROPERTY(EditAnyWhere, Category = "Characters")
		UMaterial* wigMaterialLOD210;

	//lists of materials
	TArray<UMaterial*> materials;
	TArray<UMaterial*> materialsLOD1;
	TArray<UMaterial*> materialsLOD2;
	TArray<UMaterial*> wigs;
	TArray<UMaterial*> wigsLOD1;
	TArray<UMaterial*> wigsLOD2;


	void Communication();
	void SpawnPedestrians();
	void ClearPedestrians();
	void UpdateWaterLevel();
	
};
