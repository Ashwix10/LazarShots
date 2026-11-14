// Chain-reaction interface: every interactive prop implements this.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BulletHitInterface.generated.h"

class ARicochetBullet;

UINTERFACE(MinimalAPI, Blueprintable)
class UBulletHitInterface : public UInterface
{
	GENERATED_BODY()
};

class MYPROJECT_API IBulletHitInterface
{
	GENERATED_BODY()

public:

	/** The bullet bounced off (or passed through) this prop. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Ricochet")
	void OnBulletHit(ARicochetBullet* Bullet, const FHitResult& Hit);

	/** The previous prop in the chain fired; this one becomes active. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Ricochet")
	void UnlockProp();
};
