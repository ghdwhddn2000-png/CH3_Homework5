#include "SpartaGameState.h"
#include "Kismet/GameplayStatics.h"
#include "SpawnVolume.h"
#include "CoinItem.h"
#include "SpartaGameInstance.h"
#include "SpartaPlayerController.h"
#include "Components/TextBlock.h"
#include "Blueprint/UserWidget.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/AudioComponent.h"
#include "UObject/UObjectIterator.h"

ASpartaGameState::ASpartaGameState()
{
	Score = 0;
	SpawnedCoinCount = 0;
	CollectedCoinCount = 0;
	LevelDuration = 30.0f;
	CurrentLevelIndex = 0;
	MaxLevels = 3;
	CurrentWaveIndex = 0;
	MaxWaves = 3;
}

void ASpartaGameState::BeginPlay()
{
	Super::BeginPlay();

	StartLevel();

	GetWorldTimerManager().SetTimer(
		HUDUpdateTimerHandle,
		this,
		&ASpartaGameState::UpdateHUD,
		0.1f,
		true
	);
}

int32 ASpartaGameState::GetScore() const
{
	return Score;
}

void ASpartaGameState::AddScore(int32 Amount)
{		
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		USpartaGameInstance* SpartaGameInstance = Cast<USpartaGameInstance>(GameInstance);
		if (SpartaGameInstance)
		{
			SpartaGameInstance->AddToScore(Amount);
		}
	}

}

void ASpartaGameState::StartLevel()
{
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		if (ASpartaPlayerController* SpartaPlayerController = Cast<ASpartaPlayerController>(PlayerController))
		{
			SpartaPlayerController->ShowGameHUD();
		}
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		USpartaGameInstance* SpartaGameInstance = Cast<USpartaGameInstance>(GameInstance);
		if (SpartaGameInstance)
		{
			CurrentLevelIndex = SpartaGameInstance->CurrentLevelIndex;
		}
	}

	StartWave();
	
}

void ASpartaGameState::StartWave()
{
	SpawnedCoinCount = 0;
	CollectedCoinCount = 0;

	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		if (ASpartaPlayerController* SpartaPlayerController = Cast<ASpartaPlayerController>(PlayerController))
		{
			SpartaPlayerController->ShowGameHUD();
		}
	}
	
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		USpartaGameInstance* SpartaGameInstance = Cast<USpartaGameInstance>(GameInstance);
		if (SpartaGameInstance)
		{
			CurrentWaveIndex = SpartaGameInstance->CurrentWaveIndex;
			Score = SpartaGameInstance->TotalScore;
		}
	}

	TArray<AActor*> FoundVolumes;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASpawnVolume::StaticClass(), FoundVolumes);

	const int32 ItemToSpawn = 40 + (CurrentWaveIndex * 10);

	for (int32 i = 0; i < ItemToSpawn; i++)
	{
		if (FoundVolumes.Num() > 0)
		{
			ASpawnVolume* SpawnVolume = Cast<ASpawnVolume>(FoundVolumes[0]);
			if (SpawnVolume)
			{
				AActor* SpawnedActor = SpawnVolume->SpawnRandomItem();
				if (SpawnedActor && SpawnedActor->IsA(ACoinItem::StaticClass()))
				{
					SpawnedCoinCount++;
				}
			}
		}
	}

	GetWorldTimerManager().SetTimer(
		LevelTimerHandle,
		this,
		&ASpartaGameState::OnLevelTimeUp,
		LevelDuration,
		false
	);
}

void ASpartaGameState::OnLevelTimeUp()
{
	EndWave();
}

void ASpartaGameState::OnCoinCollected()
{
	CollectedCoinCount++;

	UE_LOG(LogTemp, Warning, TEXT("Coin Collected: %d / %d"), CollectedCoinCount, SpawnedCoinCount);

	if (SpawnedCoinCount > 0 && CollectedCoinCount >= SpawnedCoinCount)
	{
		EndWave();
	}
}


void ASpartaGameState::EndWave()
{
	GetWorldTimerManager().ClearTimer(LevelTimerHandle);

	if (CurrentWaveIndex >= MaxWaves - 1)
	{
		EndLevel();
	}

	else
	{
		CurrentWaveIndex++;

		if (UGameInstance* GameInstance = GetGameInstance())
		{
			USpartaGameInstance* SpartaGameInstance = Cast<USpartaGameInstance>(GameInstance);
			if (SpartaGameInstance)
			{
				SpartaGameInstance->CurrentWaveIndex = CurrentWaveIndex;
			}
		}

		// 아이템들 삭제
		TArray<AActor*> RemainingItems;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABaseItem::StaticClass(), RemainingItems);
		for (AActor* item : RemainingItems)
		{
			if (item)
			{
				item->Destroy();
			}
		}

		// Particle들 삭제
		TArray<UParticleSystemComponent*> RemainingParticles;
		for (TObjectIterator<UParticleSystemComponent> It; It; ++It)
		{			
			if (It->GetWorld() == GetWorld())
			{
				RemainingParticles.Add(*It);
			}
		}
		for (UParticleSystemComponent* Particle : RemainingParticles)
		{
			if (Particle)
			{
				Particle->DestroyComponent(); 
			}
		}

		// Sound들 삭제
		TArray<UAudioComponent*> RemainingSounds;
		for (TObjectIterator<UAudioComponent> It; It; ++It)
		{			
			if (It->GetWorld() == GetWorld())
			{
				RemainingSounds.Add(*It);
			}
		}
		for (UAudioComponent* Sound : RemainingSounds)
		{
			if (Sound)
			{
				Sound->Stop();             // 소리를 먼저 끄고 (안 끄면 잔상남을 수도 있음)
				Sound->DestroyComponent(); 
			}
		}
				
		StartWave();
	}
}

void ASpartaGameState::EndLevel()
{			
	if (CurrentLevelIndex >= MaxLevels - 1)
	{
		OnGameOver();
		return;
	}

	else
	{
		CurrentLevelIndex++;

		if (UGameInstance* GameInstance = GetGameInstance())
		{
			USpartaGameInstance* SpartaGameInstance = Cast<USpartaGameInstance>(GameInstance);
			if (SpartaGameInstance)
			{
				SpartaGameInstance->CurrentLevelIndex = CurrentLevelIndex;
				SpartaGameInstance->CurrentWaveIndex = 0;
			}
		}

		//GameState에 인덱스를 저장할 수 있고, 다음 인덱스를 불러오는 과정 / 있으면 불러오고 없으면 게임오버
		if (LevelMapNames.IsValidIndex(CurrentLevelIndex))
		{
			UGameplayStatics::OpenLevel(GetWorld(), LevelMapNames[CurrentLevelIndex]);
		}

		else
		{
			OnGameOver();
		}
	}
}

void ASpartaGameState::OnGameOver()
{
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		if (ASpartaPlayerController* SpartaPlayerController = Cast<ASpartaPlayerController>(PlayerController))
		{		
			SpartaPlayerController->ShowMainMenu(true);
		}
	}
}

void ASpartaGameState::UpdateHUD()
{
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		if (ASpartaPlayerController* SpartaPlayerController = Cast<ASpartaPlayerController>(PlayerController))
		{
			if (UUserWidget* HUDWidget = SpartaPlayerController->GetHUDWidget())
			{
				if (UTextBlock* TimeText = Cast<UTextBlock>(HUDWidget->GetWidgetFromName(TEXT("Time"))))
				{
					float RemainingTime = GetWorldTimerManager().GetTimerRemaining(LevelTimerHandle);
					
					if (RemainingTime < 0.0f)
					{
						RemainingTime = 0.0f;
					}
					TimeText->SetText(FText::FromString(FString::Printf(TEXT("Time: %.1f"), RemainingTime)));
				}

				if (UTextBlock* ScoreText = Cast<UTextBlock>(HUDWidget->GetWidgetFromName(TEXT("Score"))))
				{
					if (UGameInstance* GameInstance = GetGameInstance())
					{
						USpartaGameInstance* SpartaGameInstance = Cast<USpartaGameInstance>(GameInstance);
						if (SpartaGameInstance)
						{
							ScoreText->SetText(FText::FromString(FString::Printf(TEXT("Score: %d"), SpartaGameInstance->TotalScore)));
						}
					}					
				}

				if (UTextBlock* LevelIndexText = Cast<UTextBlock>(HUDWidget->GetWidgetFromName(TEXT("Level"))))
				{
					LevelIndexText->SetText(FText::FromString(FString::Printf(TEXT("Level %d"), CurrentLevelIndex + 1)));
				}

				if (UTextBlock* WaveIndexText = Cast<UTextBlock>(HUDWidget->GetWidgetFromName(TEXT("Wave"))))
				{
					WaveIndexText->SetText(FText::FromString(FString::Printf(TEXT("Wave %d"), CurrentWaveIndex + 1)));
				}
			}
		}
	}
}