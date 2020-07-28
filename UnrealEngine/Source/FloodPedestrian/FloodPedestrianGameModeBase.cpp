// Copyright Epic Games, Inc. All Rights Reserved.


#include "FloodPedestrianGameModeBase.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "glm.hpp"
#include "Runtime/Engine/Classes/Kismet/KismetMathLibrary.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include <winbase.h>
#include "Blueprint/UserWidget.h"

#undef UpdateResource

AFloodPedestrianGameModeBase::AFloodPedestrianGameModeBase()
{
	PrimaryActorTick.bCanEverTick = true;

	simulationStarted = false;
	firstFrame = true;
}

void AFloodPedestrianGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	//display hud
	if (menuWidgetClass != nullptr)
	{
		menuWidget = CreateWidget<UUserWidget>(GetWorld(), menuWidgetClass);
		if (menuWidget != nullptr)
		{
			menuWidget->AddToViewport();
			menuWidget->SetVisibility(ESlateVisibility::Visible);
		}
	}

	//alloc memory for communication structs
	pedestrians = (pedestrian_data*)malloc(sizeof(pedestrian_data));
	floodCells = (flood_data*)malloc(sizeof(flood_data));
	options = (options_data*)malloc(sizeof(options_data));

	//fill materials array LOD 0
	materials.Add(pedestrianMaterial00);
	materials.Add(pedestrianMaterial10);

	//fill materials LOD 1 array
	materialsLOD1.Add(pedestrianMaterialLOD100);
	materialsLOD1.Add(pedestrianMaterialLOD110);

	//fill materials LOD 2 array
	materialsLOD2.Add(pedestrianMaterialLOD200);
	materialsLOD2.Add(pedestrianMaterialLOD210);

	//fill wigs array LOD0
	wigs.Add(wigMaterial10);

	//fill wigs array LOD1
	wigsLOD1.Add(wigMaterialLOD110);

	//fill wigs array LOD2
	wigsLOD2.Add(wigMaterialLOD210);

	// spawn pedestrian actor for instances
	FActorSpawnParameters spawnParams;
	spawnParams.Owner = this;
	FRotator spawnRotation = { 0.0f, 0.0f, 0.0f };
	FVector spawnLocation = { 0.0f, 0.0f, 0.0f };

	for (int i = 0; i < NUM_MODELS; i++)
	{
		APedestrian* pedestrian;
		pedestrian = GetWorld()->SpawnActor<APedestrian>(APedestrian::StaticClass(), spawnLocation, spawnRotation, spawnParams);
		pedestrian->ISMComp[0].SetMobility(EComponentMobility::Movable);

		//models
		if (i == 0)
		{
			pedestrian->ISMComp->SetStaticMesh(SMPedestrian0);
			pedestrian->ISMComp->SetMaterial(0, materials[i]);
			pedestrian->ISMComp->SetMaterial(1, materialsLOD1[i]);
			pedestrian->ISMComp->SetMaterial(2, materialsLOD2[i]);
		}
		else
		{
			pedestrian->ISMComp->SetStaticMesh(SMPedestrian1);
			pedestrian->ISMComp->SetMaterial(0, materials[i]);
			pedestrian->ISMComp->SetMaterial(1, wigs[0]);
			pedestrian->ISMComp->SetMaterial(2, materialsLOD1[i]);
			pedestrian->ISMComp->SetMaterial(3, wigsLOD1[0]);
			pedestrian->ISMComp->SetMaterial(4, materialsLOD2[i]);
			pedestrian->ISMComp->SetMaterial(5, wigsLOD2[0]);
		}
		agents.Add(pedestrian);
	}


	//spawn water plane
	spawnLocation.X = 250.0f;
	spawnLocation.Y = 800.0f; //change this to 250.0f to be on top of the map
	spawnLocation.Z = 0.0f;
	FTransform tempTransform;
	tempTransform.SetLocation(spawnLocation);
	tempTransform.SetScale3D({ 1.0f, 1.0f, 1.0f });
	map = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), tempTransform, spawnParams);
	map->GetStaticMeshComponent()->SetStaticMesh(SMMap);
	map->GetStaticMeshComponent()->SetCastShadow(false);
	//map->GetStaticMeshComponent()->SetMaterial(0, waterMaterial); //use this for material
	previousDisplacement.Init(0.0f, xmachine_memory_agent_MAX);
}

void AFloodPedestrianGameModeBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (simulationStarted)
	{
		//communication with FLAME
		Communication();
	}
}

void AFloodPedestrianGameModeBase::StartSimulation()
{
	//get simulation path
	FString directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FString name = "PedestrianNavigation.bat";
	FString path = FPaths::Combine(directory, TEXT("../FLAMEGPU-development/bin/x64/"));

	//set simulation started
	simulationStarted = true;

	// initialise memory segments
	TCHAR pedestrianName[] = TEXT("GraphSizeFileMapping");
	TCHAR floodName[] = TEXT("PositionFileMapping");
	TCHAR optionsName[] = TEXT("OptionsFileMapping");

	// create handles
	hPedestrianMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(pedestrian_data), pedestrianName);
	hFloodMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(flood_data), floodName);
	hOptionsMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(options_data), optionsName);

	if (hPedestrianMapFile == NULL || hFloodMapFile == NULL || hOptionsMapFile == NULL)
	{
		exit(0);
	}

	// map buffers
	pedestrianBuf = (LPTSTR)MapViewOfFile(hPedestrianMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(pedestrian_data));
	floodBuf = (LPTSTR)MapViewOfFile(hFloodMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(flood_data));
	optionsBuf = (LPTSTR)MapViewOfFile(hOptionsMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(options_data));

	if (pedestrianBuf == NULL)
	{
		CloseHandle(hPedestrianMapFile);
		exit(0);
	}
	if (floodBuf == NULL)
	{
		CloseHandle(hFloodMapFile);
		exit(0);
	}
	if (optionsBuf == NULL)
	{
		CloseHandle(hOptionsMapFile);
		exit(0);
	}

	//start process
	FProcHandle Handle = FPlatformProcess::CreateProc(*(path + name), NULL, false, true, true, NULL, 0, *path, NULL, NULL);
}

void AFloodPedestrianGameModeBase::StopSimulation()
{
	//send 'quit' to FLAME to quit the simulation
	options->quit = 1;
	CopyMemory((PVOID)optionsBuf, options, sizeof(options_data));

	//reset data
	simulationStarted = false;
	options->quit = 0;
	memset((PVOID)pedestrianBuf, 0, sizeof(pedestrian_data));
	memset((PVOID)floodBuf, 0, sizeof(flood_data));
	previousDisplacement.Empty();
	previousDisplacement.Init(0.0f, xmachine_memory_agent_MAX);
	ClearPedestrians();


	//restore vertex positions
	map->GetStaticMeshComponent()->GetStaticMesh()->RenderData->LODResources[0].VertexBuffers.PositionVertexBuffer.Init(initialVertexPositions);
	map->GetStaticMeshComponent()->GetStaticMesh()->RenderData->LODResources[0].VertexBuffers.PositionVertexBuffer.InitRHI();
	map->GetStaticMeshComponent()->MarkRenderStateDirty();
	map->GetStaticMeshComponent()->MarkRenderDynamicDataDirty();
	map->GetStaticMeshComponent()->MarkRenderTransformDirty();
}

void AFloodPedestrianGameModeBase::Quit()
{
	options->quit = 1;
	CopyMemory((PVOID)optionsBuf, options, sizeof(options_data));

	//exit Unreal application
	FGenericPlatformMisc::RequestExit(false);
}

void AFloodPedestrianGameModeBase::Communication()
{
	//get flame data
	CopyMemory(options, optionsBuf, sizeof(int));

	if (options->lock == 1)
	{
		CopyMemory(pedestrians, pedestrianBuf, sizeof(pedestrian_data));
		CopyMemory(floodCells, floodBuf, sizeof(flood_data));
		options->lock = 0;
		CopyMemory((PVOID)optionsBuf, options, sizeof(int));
		SpawnPedestrians();
		UpdateWaterLevel();
	}
}

void AFloodPedestrianGameModeBase::SpawnPedestrians()
{
	//each frame pedestrians are deleted and created again with the new data
	ClearPedestrians();

	for (int i = 0; i < pedestrians->total; i++)
	{
		//ignore corrupt data
		if (isnan(pedestrians->x[i]) || isnan(pedestrians->y[i]) || isnan(pedestrians->z[i]))
			continue;
		//position
		glm::vec3 loc = glm::vec3(pedestrians->x[i], pedestrians->y[i], pedestrians->z[i]);
		loc = (loc + glm::vec3(1.0f)) * SCALE;
		FVector spawnLocation = { loc.y, loc.x, 0.0f};
		//orientation
		float angle = FMath::Atan2(pedestrians->velX[i], pedestrians->velY[i]);
		FRotator rotation = { 0.0f, FMath::RadiansToDegrees(angle) - 90.0f, 0.0f };
		FQuat spawnRotation = rotation.Quaternion();
		//model
		int model = pedestrians->gender[i] - 1;
		//temp vector
		FTransform tempVector;
		tempVector.SetLocation(spawnLocation);
		tempVector.SetRotation(spawnRotation);
		FVector modelScale = { 0.1, 0.1, 0.1 };
		//it is possible to change the walking animation speed of the material.  I'm using the scale X variable to do it.  scaleX = 1.01 (full speed), scaleX = 1.00 (idle)
		//I'm setting the speed to the maximum.  If you want to change it, make sure that scaleX does not exceeds 1.01.
		float speed = 1.0; //speed = pedestrians->speed[i]
		modelScale.X += speed / 100.0f;
		tempVector.SetScale3D(modelScale);
		//spawn instance
		agents[model]->ISMComp->AddInstance(tempVector);
	}
}

void AFloodPedestrianGameModeBase::ClearPedestrians()
{
	UWorld* const world = GetWorld();
	//clear instances
	if (world && pedestrians->total >= 0)
	{
		for (int i = 0; i < NUM_MODELS; i++)
			agents[i]->ISMComp->ClearInstances();
	}
}

void AFloodPedestrianGameModeBase::UpdateWaterLevel()
{
	//move the vertices of the water plane
	TArray<FVector> vec;
	if (!map->GetStaticMeshComponent()->GetStaticMesh()->RenderData)
		return;
	if (map->GetStaticMeshComponent()->GetStaticMesh()->RenderData->LODResources.Num() > 0)
	{
		FPositionVertexBuffer* vBuffer = &map->GetStaticMeshComponent()->GetStaticMesh()->RenderData->LODResources[0].VertexBuffers.PositionVertexBuffer; //get vertex buffer
		
		if (vBuffer)
		{
			const int32 nbVertices = vBuffer->GetNumVertices();
			for (int32 index = 0; index < nbVertices; index++)
			{
				//vertex order of the mesh does not match the grid order read from FLAME
				//get corresponding index depending on vertex position

				float cellSize = SCALE * 2.0f / (GRID_SIZE - 1.0f);
				float newX = SCALE - vBuffer->VertexPosition(index).X;
				newX /= cellSize;
				float newY = SCALE - vBuffer->VertexPosition(index).Y;
				newY /= cellSize;
				newX = GRID_SIZE - 1 - newX;
				newY = GRID_SIZE - 1 - newY;
				int newIndex = newY * GRID_SIZE + newX;
				if (newIndex >= xmachine_memory_agent_MAX)
				{
					FVector displacement = { 0.0f, 0.0f, 0.0f };
					const FVector position = displacement + map->GetTransform().TransformVector(vBuffer->VertexPosition(index));
					vec.Add(position);
				}
				else
				{
					FVector displacement = { 0.0f, 0.0f, (floodCells->z[newIndex]) - previousDisplacement[newIndex]  };
					previousDisplacement[newIndex] = floodCells->z[newIndex];
					const FVector position = displacement + map->GetTransform().TransformVector(vBuffer->VertexPosition(index));
					vec.Add(position);
				}
			}
		}

		//same intiial vertex positions
		if (firstFrame)
		{
			initialVertexPositions = vec;
			firstFrame = false;
		}

		map->GetStaticMeshComponent()->GetStaticMesh()->RenderData->LODResources[0].VertexBuffers.PositionVertexBuffer.Init(vec);
		map->GetStaticMeshComponent()->GetStaticMesh()->RenderData->LODResources[0].VertexBuffers.PositionVertexBuffer.InitRHI();

		map->GetStaticMeshComponent()->MarkRenderStateDirty();
		map->GetStaticMeshComponent()->MarkRenderDynamicDataDirty();
		map->GetStaticMeshComponent()->MarkRenderTransformDirty();
	}

	// Code to generate a texture each frame. Dynamic material is not created
	// Texture Information
	/*FString FileName = FString("MyTexture");
	int width = 128;
	int height = 128;
	uint8* pixels = (uint8*)malloc(height * width * 4); // x4 because it's RGBA. 4 integers, one for Red, one for Green, one for Blue, one for Alpha

	// filling the pixels with dummy data (4 boxes: red, green, blue and white)
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			int index = y * GRID_SIZE + x;
			pixels[y * 4 * width + x * 4 + 0] = floodCells->z[index] * 100; // R
			pixels[y * 4 * width + x * 4 + 1] = floodCells->z[index] * 100;   // G
			pixels[y * 4 * width + x * 4 + 2] = floodCells->z[index] * 100;   // B
			pixels[y * 4 * width + x * 4 + 3] = 255; // A
		}
	}


	// Create Package
	FString pathPackage = FString("/Game/Textures/");
	FString absolutePathPackage = FPaths::ProjectContentDir() + "/Textures/";

	FPackageName::RegisterMountPoint(*pathPackage, *absolutePathPackage);

	UPackage* Package = CreatePackage(nullptr, *pathPackage);

	// Create the Texture
	FName TextureName = MakeUniqueObjectName(Package, UTexture2D::StaticClass(), FName(*FileName));
	//TextureName = FName(TEXT("MyTexture"));
	UTexture2D* Texture = NewObject<UTexture2D>(Package, TextureName, RF_Public | RF_Standalone);

	// Texture Settings
	Texture->PlatformData = new FTexturePlatformData();
	Texture->PlatformData->SizeX = width;
	Texture->PlatformData->SizeY = height;
	Texture->PlatformData->PixelFormat = PF_R8G8B8A8;

	// Passing the pixels information to the texture
	FTexture2DMipMap* Mip = new(Texture->PlatformData->Mips) FTexture2DMipMap();
	Mip->SizeX = width;
	Mip->SizeY = height;
	Mip->BulkData.Lock(LOCK_READ_WRITE);
	uint8* TextureData = (uint8*)Mip->BulkData.Realloc(height * width * sizeof(uint8) * 4);
	FMemory::Memcpy(TextureData, pixels, sizeof(uint8) * height * width * 4);
	Mip->BulkData.Unlock();

	// Updating Texture & mark it as unsaved
	Texture->AddToRoot();
	Texture->UpdateResource();
	Package->MarkPackageDirty();

	//UE_LOG(LogTemp, Log, TEXT("Texture created: %s"), &FileName);

	free(pixels);
	pixels = NULL;*/

}