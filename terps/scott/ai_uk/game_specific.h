//
//  game_specific.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-27.
//

#ifndef game_specific_h
#define game_specific_h

void SecretAction(int p);
void AdventurelandAction(int p);
void AdventurelandDarkness(void);
void Spiderman64Sysmess(void);
void Adventureland64Sysmess(void);
void Claymorgue64Sysmess(void);
void Mysterious64Sysmess(void);
void PerseusItalianSysmess(void);
void Supergran64Sysmess(void);
void SecretMission64Sysmess(void);
void UpdateSecretAnimations(void);

void CountShowImageOnExamineUS(int noun);
void VoodooShowImageOnExamineUS(int noun);
void AdventurelandShowImageOnExamineUS(int noun);
void PirateShowImageOnExamineUS(int noun);
void MissionShowImageOnExamineUS(int noun);
void StrangeShowImageOnExamineUS(int noun);

#endif /* game_specific_h */
