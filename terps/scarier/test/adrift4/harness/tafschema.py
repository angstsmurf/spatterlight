"""ADRIFT TAF schema tables, extracted from sctafpar.cpp."""
from __future__ import annotations

# Multiline separators (as a whole TAF line).
SEP_400 = "\xbd\xd0"
SEP_PRE400 = "**"

ROOMLIST_NO_ROOMS = 0
ROOMLIST_ONE_ROOM = 1
ROOMLIST_SOME_ROOMS = 2
ROOMLIST_ALL_ROOMS = 3
ROOMLIST_NPC_PART = 4

SCHEMA_400 = {
    '_GAME_': '<HEADER>Header <GLOBAL>Globals V<ROOM>Rooms V<OBJECT>Objects V<TASK>Tasks V<EVENT>Events V<NPC>NPCs V<ROOM_GROUP>RoomGroups V<SYNONYM>Synonyms V<VARIABLE>Variables V<ALR>ALRs BCustomFont ?BCustomFont:$FontNameSize $CompileDate',
    'HEADER': 'MStartupText #StartRoom MWinText',
    'GLOBAL': '$GameName $GameAuthor $DontUnderstand #Perspective BShowExits #WaitTurns BDispFirstRoom BBattleSystem #MaxScore $PlayerName BPromptName $PlayerDesc #Task ?!#Task=0:$AltDesc #Position #ParentObject #PlayerGender #MaxSize #MaxWt ?GBattleSystem:<BATTLE>Battle BEightPointCompass bNoDebug BNoScoreNotify BNoMap bNoAutoComplete bNoControlPanel bNoMouse BSound BGraphics <RESOURCE>IntroRes <RESOURCE>WinRes BStatusBox $StatusBoxText #SizeMultiple #WeightMultiple BEmbedded',
    'BATTLE': '#StaminaLo #StaminaHi #StrengthLo #StrengthHi #AccuracyLo #AccuracyHi #DefenseLo #DefenseHi #AgilityLo #AgilityHi #Recovery',
    'ROOM': '$Short $Long ?GEightPointCompass:[12]<ROOM_EXIT>Exits ?!GEightPointCompass:[8]<ROOM_EXIT>Exits <RESOURCE>Res V<ROOM_ALT>Alts ?!GNoMap:BHideOnMap',
    'ROOM_EXIT': '{V400_ROOM_EXIT:#Dest_#Var1_#Var2_#Var3}',
    'ROOM_ALT': '$M1 #Type <RESOURCE>Res1 $M2 #Var2 <RESOURCE>Res2 #HideObjects $Changed #Var3 #DisplayRoom',
    'RESOURCE': '?GSound:$SoundFile,#SoundLen,ZSoundOffset ?GGraphics:$GraphicFile,#GraphicLen,ZGraphicOffset {V400_RESOURCE}',
    'OBJECT': '$Prefix $Short V$Alias BStatic $Description #InitialPosition #Task BTaskNotDone $AltDesc ?BStatic:<ROOM_LIST1>Where BContainer BSurface #Capacity ?!BStatic:BWearable,#SizeWeight,#Parent ?BStatic:{OBJECT:#Parent} #Openable ?#Openable=5:#Key ?#Openable=6:#Key ?#Openable=7:#Key #SitLie ?!BStatic:BEdible BReadable ?BReadable:$ReadText ?!BStatic:BWeapon #CurrentState ?!#CurrentState=0:$States,BStateListed BListFlag <RESOURCE>Res1 <RESOURCE>Res2 ?GBattleSystem:<OBJ_BATTLE>Battle $InRoomDesc #OnlyWhenNotMoved',
    'OBJ_BATTLE': '#ProtectionValue #HitValue #Method #Accuracy',
    'ROOM_LIST1': '#Type {ROOM_LIST1}',
    'TASK': 'V$Command $CompleteText $ReverseMessage $RepeatText $AdditionalMessage #ShowRoomDesc BRepeatable BReversible V$ReverseCommand <ROOM_LIST0>Where $Question ?$Question:$Hint1,$Hint2 V<TASK_RESTR>Restrictions V<TASK_ACTION>Actions $RestrMask <RESOURCE>Res',
    'TASK_RESTR': '#Type ?#Type=0:#Var1,#Var2,#Var3 ?#Type=1:#Var1,#Var2 ?#Type=2:#Var1,#Var2 ?#Type=3:#Var1,#Var2,#Var3 ?#Type=4:#Var1,#Var2,#Var3,$Var4 |V400_TASK_RESTR:Type>4?#Var1,#Var2,#Var3| $FailMessage',
    'TASK_ACTION': '#Type ?#Type=0:#Var1,#Var2,#Var3 ?#Type=1:#Var1,#Var2,#Var3 ?#Type=2:#Var1,#Var2 ?#Type=3:#Var1,#Var2,#Var3,$Expr,#Var5 ?#Type=4:#Var1 ?#Type=5:#Var1,#Var2 ?#Type=6:#Var1,#Var2,#Var3 ?#Type=7:#Var1,#Var2,#Var3',
    'ROOM_LIST0': '#Type {ROOM_LIST0}',
    'EVENT': '$Short #StarterType ?#StarterType=2:#StartTime,#EndTime ?#StarterType=3:#TaskNum #RestartType BTaskFinished #Time1 #Time2 $StartText $LookText $FinishText <ROOM_LIST0>Where #PauseTask BPauserCompleted #PrefTime1 $PrefText1 #ResumeTask BResumerCompleted #PrefTime2 $PrefText2 #Obj2 #Obj2Dest #Obj3 #Obj3Dest #Obj1 #Obj1Dest #TaskAffected [5]<RESOURCE>Res',
    'NPC': '$Name $Prefix V$Alias $Descr #StartRoom $AltText #Task V<TOPIC>Topics V<WALK>Walks BShowEnterExit ?BShowEnterExit:$EnterText,$ExitText $InRoomText #Gender [4]<RESOURCE>Res ?GBattleSystem:<NPC_BATTLE>Battle',
    'NPC_BATTLE': '#Attitude #StaminaLo #StaminaHi #StrengthLo #StrengthHi #AccuracyLo #AccuracyHi #DefenseLo #DefenseHi #AgilityLo #AgilityHi #Speed #KilledTask #Recovery #StaminaTask',
    'TOPIC': '$Subject $Reply #Task $AltReply',
    'WALK': '#NumStops BLoop #StartTask #CharTask #MeetObject #ObjectTask #StoppingTask #MeetChar $ChangedDesc {WALK:#Rooms_#Times}',
    'ROOM_GROUP': '$Name {ROOM_GROUP:[]BList}',
    'SYNONYM': '$Replacement $Original',
    'VARIABLE': '$Name #Type $Value',
    'ALR': '$Original $Replacement',
}

SCHEMA_390 = {
    '_GAME_': '<HEADER>Header <GLOBAL>Globals V<ROOM>Rooms V<OBJECT>Objects V<TASK>Tasks V<EVENT>Events V<NPC>NPCs V<ROOM_GROUP>RoomGroups V<SYNONYM>Synonyms V<VARIABLE>Variables V<ALR>ALRs BCustomFont ?BCustomFont:$FontNameSize $CompileDate sPassword',
    'HEADER': 'MStartupText #StartRoom MWinText',
    'GLOBAL': '$GameName $GameAuthor $DontUnderstand #Perspective BShowExits #WaitTurns BDispFirstRoom BBattleSystem #MaxScore $PlayerName BPromptName $PlayerDesc #Task ?!#Task=0:$AltDesc #Position #ParentObject #PlayerGender #MaxSize #MaxWt ?GBattleSystem:<BATTLE>Battle BEightPointCompass bNoDebug BNoScoreNotify BNoMap bNoAutoComplete bNoControlPanel bNoMouse BSound BGraphics <RESOURCE>IntroRes <RESOURCE>WinRes FStatusBox EStatusBoxText #SizeMultiple #WeightMultiple FEmbedded',
    'BATTLE': '#Stamina #Strength #Defense',
    'ROOM': '$Short $Long $LastDesc ?GEightPointCompass:[12]<ROOM_EXIT>Exits ?!GEightPointCompass:[8]<ROOM_EXIT>Exits $AddDesc1 #Task1 $AddDesc2 #Task2 #Obj $AltDesc #TypeHideObjects <RESOURCE>Res <RESOURCE>LastRes <RESOURCE>Task1Res <RESOURCE>Task2Res <RESOURCE>AltRes ?!GNoMap:BHideOnMap |V390_ROOM:_Alts_|',
    'ROOM_EXIT': '{V390_V380_ROOM_EXIT:#Dest_#Var1_#Var2_ZVar3}',
    'RESOURCE': '?GSound:$SoundFile,ZSoundLen,ZSoundOffset ?GGraphics:$GraphicFile,ZGraphicLen,ZGraphicOffset',
    'OBJECT': '$Prefix $Short [1]$Alias BStatic $Description #InitialPosition #Task BTaskNotDone $AltDesc ?BStatic:<ROOM_LIST1>Where BContainer BSurface #Capacity ?!BStatic:BWearable,#SizeWeight,#Parent ?BStatic:{OBJECT:#Parent} #Openable |V390_OBJECT:_Openable_,Key| #SitLie ?!BStatic:BEdible BReadable ?BReadable:$ReadText ?!BStatic:BWeapon ZCurrentState FListFlag <RESOURCE>Res1 <RESOURCE>Res2 ?GBattleSystem:<OBJ_BATTLE>Battle EInRoomDesc ZOnlyWhenNotMoved',
    'OBJ_BATTLE': '#ProtectionValue #HitValue #Method',
    'ROOM_LIST1': '#Type {ROOM_LIST1}',
    'TASK': 'W$Command $CompleteText $ReverseMessage $RepeatText $AdditionalMessage #ShowRoomDesc BRepeatable BReversible W$ReverseCommand <ROOM_LIST0>Where $Question ?$Question:$Hint1,$Hint2 V<TASK_RESTR>Restrictions V<TASK_ACTION>Actions |V390_TASK:$RestrMask| <RESOURCE>Res',
    'TASK_RESTR': '#Type ?#Type=0:#Var1,#Var2,#Var3 ?#Type=1:#Var1,#Var2 ?#Type=2:#Var1,#Var2 ?#Type=3:#Var1,#Var2,#Var3 ?#Type=4:#Var1,#Var2,#Var3,EVar4,|V390_TASK_RESTR:Var1>0?#Var1++| $FailMessage',
    'TASK_ACTION': '#Type |V390_TASK_ACTION:Type>4?#Type++| ?#Type=0:#Var1,#Var2,#Var3 ?#Type=1:#Var1,#Var2,#Var3 ?#Type=2:#Var1,#Var2 ?#Type=3:#Var1,#Var2,#Var3,|V390_TASK_ACTION:$Expr_#Var5| ?#Type=4:#Var1 ?#Type=6:#Var1,ZVar2,ZVar3 ?#Type=7:#Var1,#Var2,#Var3,|V390_TASK_ACTION:_BattleAttr_|',
    'ROOM_LIST0': '#Type {ROOM_LIST0}',
    'EVENT': '$Short #StarterType ?#StarterType=2:#StartTime,#EndTime ?#StarterType=3:#TaskNum #RestartType BTaskFinished #Time1 #Time2 $StartText $LookText $FinishText <ROOM_LIST0>Where #PauseTask BPauserCompleted #PrefTime1 $PrefText1 #ResumeTask BResumerCompleted #PrefTime2 $PrefText2 #Obj2 #Obj2Dest #Obj3 #Obj3Dest #Obj1 #Obj1Dest #TaskAffected [5]<RESOURCE>Res',
    'NPC': '$Name $Prefix [1]$Alias $Descr #StartRoom $AltText #Task V<TOPIC>Topics V<WALK>Walks BShowEnterExit ?BShowEnterExit:$EnterText,$ExitText $InRoomText #Gender [4]<RESOURCE>Res ?GBattleSystem:<NPC_BATTLE>Battle',
    'NPC_BATTLE': '#Attitude #Stamina #Strength #Defense #Speed #KilledTask',
    'TOPIC': '$Subject $Reply #Task $AltReply',
    'WALK': '#NumStops BLoop #StartTask #CharTask #MeetObject #ObjectTask #StoppingTask ZMeetChar $ChangedDesc {WALK:#Rooms_#Times}',
    'ROOM_GROUP': '$Name {ROOM_GROUP:[]BList}',
    'SYNONYM': '$Replacement $Original',
    'VARIABLE': '$Name ZType $Value',
    'ALR': '$Original $Replacement',
}

SCHEMA_380 = {
    '_GAME_': '<HEADER>Header <GLOBAL>Globals V<ROOM>Rooms V<OBJECT>Objects V<TASK>Tasks V<EVENT>Events V<NPC>NPCs V<ROOM_GROUP>RoomGroups V<SYNONYM>Synonyms FCustomFont $CompileDate sPassword |V380_GLOBAL:_MaxScore_| |V380_OBJECT:_InitialPositions_|',
    'HEADER': 'MStartupText #StartRoom MWinText',
    'GLOBAL': '$GameName $GameAuthor #MaxCarried |V380_MaxSize_MaxWt_| $DontUnderstand #Perspective BShowExits #WaitTurns FDispFirstRoom FBattleSystem EPlayerName FPromptName EPlayerDesc ZTask ZPosition ZParentObject ZPlayerGender FEightPointCompass TNoScoreNotify FSound FGraphics FStatusBox EStatusBoxText FEmbedded',
    'ROOM': '$Short $Long $LastDesc [8]<ROOM_EXIT>Exits $AddDesc1 #Task1 $AddDesc2 #Task2 #Obj $AltDesc #TypeHideObjects |V380_ROOM:_Alts_|',
    'ROOM_EXIT': '{V390_V380_ROOM_EXIT:#Dest_#Var1_#Var2_ZVar3}',
    'OBJECT': '$Prefix $Short [1]$Alias BStatic $Description #InitialPosition #Task BTaskNotDone $AltDesc ?BStatic:<ROOM_LIST1>Where #SurfaceContainer FSurface ?#SurfaceContainer=2:TSurface FContainer ?#SurfaceContainer=1:TContainer #Capacity |V380_OBJECT:#Capacity*10+2| ?!BStatic:BWearable,#SizeWeight,|V380_OBJECT:_SizeWeight_|,#Parent ?BStatic:{OBJECT:#Parent} #Openable |V380_OBJECT:_Openable_,Key| #SitLie ?!BStatic:BEdible BReadable ?BReadable:$ReadText ?!BStatic:BWeapon ZCurrentState FListFlag EInRoomDesc ZOnlyWhenNotMoved',
    'ROOM_LIST1': '#Type {ROOM_LIST1}',
    'TASK': 'W$Command $CompleteText $ReverseMessage $RepeatText $AdditionalMessage #ShowRoomDesc BRepeatable #Score BSingleScore [6]<TASK_MOVE>Movements BReversible W$ReverseCommand #WearObj1 #WearObj2 #HoldObj1 #HoldObj2 #HoldObj3 #Obj1 #Task BTaskNotDone $TaskMsg $HoldMsg $WearMsg $CompanyMsg BNotInSameRoom #NPC $Obj1Msg #Obj1Room <ROOM_LIST0>Where BKillsPlayer BHoldingSameRoom $Question ?$Question:$Hint1,$Hint2 #Obj2 ?!#Obj2=0:#Obj2Var1,#Obj2Var2,$Obj2Msg BWinGame |V380_TASK:_Actions_| |V380_TASK:_Restrictions_|',
    'TASK_MOVE': '#Var1 #Var2 #Var3',
    'ROOM_LIST0': '#Type {ROOM_LIST0}',
    'EVENT': '$Short #StarterType ?#StarterType=2:#StartTime,#EndTime ?#StarterType=3:#TaskNum #RestartType BTaskFinished #Time1 #Time2 $StartText $LookText $FinishText <ROOM_LIST0>Where #PauseTask BPauserCompleted #PrefTime1 $PrefText1 #ResumeTask BResumerCompleted #PrefTime2 $PrefText2 #Obj2 #Obj2Dest #Obj3 #Obj3Dest #Obj1 #Obj1Dest #TaskAffected',
    'NPC': '$Name $Prefix [1]$Alias $Descr #StartRoom $AltText #Task V<TOPIC>Topics V<WALK>Walks BShowEnterExit ?BShowEnterExit:$EnterText,$ExitText $InRoomText ZGender',
    'TOPIC': '$Subject $Reply #Task $AltReply',
    'WALK': '#NumStops BLoop #StartTask #CharTask #MeetObject ?!#MeetObject=0:|V380_WALK:_MeetObject_| #ObjectTask ZMeetChar {WALK:#Rooms_#Times} ZStoppingTask EChangedDesc',
    'ROOM_GROUP': '$Name {ROOM_GROUP:[]BList}',
    'SYNONYM': '$Replacement $Original',
}

SCHEMA_370 = {
    '_GAME_': '<HEADER>Header <GLOBAL>Globals V<ROOM>Rooms V<OBJECT>Objects V<TASK>Tasks V<EVENT>Events V<NPC>NPCs V<ROOM_GROUP>RoomGroups [17]<COMMAND>Commands FCustomFont $CompileDate sPassword |V380_GLOBAL:_MaxScore_| |V370_OBJECT:_InitialPositions_| |V370_GLOBAL:_Synonyms_| |V370_TASK:_WinTask_|',
    'HEADER': 'MStartupText #StartRoom MWinText #WinTask',
    'GLOBAL': '$GameName $GameAuthor #MaxCarried |V380_MaxSize_MaxWt_| $DontUnderstand #Perspective BShowExits #WaitTurns FDispFirstRoom FBattleSystem EPlayerName FPromptName EPlayerDesc ZTask ZPosition ZParentObject ZPlayerGender FEightPointCompass TNoScoreNotify FSound FGraphics FStatusBox EStatusBoxText FEmbedded',
    'ROOM': '$Short $Long $LastDesc [8]<ROOM_EXIT>Exits $AddDesc1 #Task1 $AddDesc2 #Task2 #Obj $AltDesc #TypeHideObjects |V380_ROOM:_Alts_|',
    'ROOM_EXIT': '{V390_V380_ROOM_EXIT:#Dest_#Var1_#Var2_ZVar3}',
    'OBJECT': '$Prefix $Short [1]$Alias BStatic $Description #InitialPosition #Task BTaskNotDone $AltDesc ?BStatic:<ROOM_LIST1>Where #SurfaceContainer FSurface ?#SurfaceContainer=2:TSurface FContainer ?#SurfaceContainer=1:TContainer #Capacity |V380_OBJECT:#Capacity*10+2| ?!BStatic:BWearable,#SizeWeight,|V380_OBJECT:_SizeWeight_|,#Parent ?BStatic:{OBJECT:#Parent} #Openable |V380_OBJECT:_Openable_,Key| #SitLie ?!BStatic:BEdible BReadable ?BReadable:$ReadText ?!BStatic:BWeapon ZCurrentState FListFlag EInRoomDesc ZOnlyWhenNotMoved',
    'ROOM_LIST1': '#Type {ROOM_LIST1}',
    'TASK': 'W$Command $CompleteText $ReverseMessage $RepeatText $AdditionalMessage #ShowRoomDesc BRepeatable #Score BSingleScore [6]<TASK_MOVE>Movements BReversible W$ReverseCommand #WearObj1 #WearObj2 #HoldObj1 #HoldObj2 #HoldObj3 #Obj1 #Task BTaskNotDone $TaskMsg $HoldMsg $WearMsg $CompanyMsg BNotInSameRoom #NPC $Obj1Msg #Obj1Room <ROOM_LIST0>Where BKillsPlayer BHoldingSameRoom $Question ?$Question:$Hint1,$Hint2 #Obj2 ?!#Obj2=0:#Obj2Var1,#Obj2Var2,$Obj2Msg |V370_TASK:_Actions_| |V380_TASK:_Restrictions_|',
    'TASK_MOVE': '#Var1 #Var2',
    'ROOM_LIST0': '#Type {ROOM_LIST0}',
    'EVENT': '$Short #StarterType ?#StarterType=2:#StartTime,#EndTime ?#StarterType=3:#TaskNum #RestartType BTaskFinished #Time1 #Time2 $StartText $LookText $FinishText <ROOM_LIST0>Where #PauseTask BPauserCompleted #PrefTime1 $PrefText1 #ResumeTask BResumerCompleted #PrefTime2 $PrefText2 #Obj2 #Obj2Dest #Obj3 #Obj3Dest #Obj1 #Obj1Dest #TaskAffected',
    'NPC': '$Name $Prefix [1]$Alias $Descr #StartRoom $AltText #Task V<TOPIC>Topics V<WALK>Walks BShowEnterExit ?BShowEnterExit:$EnterText,$ExitText $InRoomText ZGender',
    'TOPIC': '$Subject $Reply #Task $AltReply',
    'WALK': '#NumStops BLoop #StartTask #CharTask #MeetObject ?!#MeetObject=0:|V380_WALK:_MeetObject_| #ObjectTask ZMeetChar {WALK:#Rooms_#Times} ZStoppingTask EChangedDesc',
    'ROOM_GROUP': '$Name {ROOM_GROUP:[]BList}',
    'COMMAND': '$Word',
}

SCHEMAS = {
    "4.00": (SCHEMA_400, SEP_400),
    "3.90": (SCHEMA_390, SEP_PRE400),
    "3.80": (SCHEMA_380, SEP_PRE400),
    "3.70": (SCHEMA_370, SEP_PRE400),
}

