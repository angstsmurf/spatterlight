/* Projectile-combat verification harness.
 * For the given game: find the first enemy (Attitude==2) and a projectile
 * weapon (Method 3 shoot or 5 throw); give the weapon to the player, wield it,
 * co-locate the enemy, then resolve a full fight via the public combat path,
 * reporting per-round enemy stamina so we can see real damage land. */
#include <stdio.h>
#include <string.h>
#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"

static const char *methods[] = {"chop","cut","hit","shoot","stab","throw"};

static const char* onm(scr_gameref_t g,scr_int o){
  scr_prop_setref_t b=gs_get_bundle(g); scr_vartype_t k[3],r;
  k[0].string="Objects";k[1].integer=o;k[2].string="Short";
  return prop_get(b,"S<-sis",&r,k)?r.string:"?";
}
static const char* nname(scr_gameref_t g,scr_int n){
  scr_prop_setref_t b=gs_get_bundle(g); scr_vartype_t k[3],r;
  k[0].string="NPCs";k[1].integer=n;k[2].string="Name";
  return prop_get(b,"S<-sis",&r,k)?r.string:"?";
}
static scr_int ni(scr_gameref_t g,scr_int n,const char*f){
  scr_prop_setref_t b=gs_get_bundle(g); scr_vartype_t k[4],r;
  k[0].string="NPCs";k[1].integer=n;k[2].string="Battle";k[3].string=f;
  return prop_get(b,"I<-siss",&r,k)?r.integer:0;
}
static scr_int oi(scr_gameref_t g,scr_int o,const char*f){
  scr_prop_setref_t b=gs_get_bundle(g); scr_vartype_t k[4],r;
  k[0].string="Objects";k[1].integer=o;k[2].string="Battle";k[3].string=f;
  return prop_get(b,"I<-siss",&r,k)?r.integer:0;
}
static void flush(scr_gameref_t g){
  const char*s=pf_get_buffer(gs_get_filter(g));
  if(s&&*s)printf("    | %s",s); pf_empty(gs_get_filter(g));
}

int main(int c,char**v){
  scr_game gg=scr_game_from_filename(v[1]); if(!gg){printf("load fail\n");return 1;}
  scr_gameref_t g=(scr_gameref_t)gg; g->is_running=1; battle_start(g);

  const char *base=strrchr(v[1],'/'); base=base?base+1:v[1];
  printf("==== %s ====\n",base);

  /* player battle profile */
  scr_prop_setref_t b=gs_get_bundle(g); scr_vartype_t k[4],r;
  k[0].string="Globals";k[1].string="Battle";
  k[2].string="StrengthHi"; scr_int pstr=prop_get(b,"I<-sss",&r,k)?r.integer:0;
  k[2].string="AccuracyHi"; scr_int pacc=prop_get(b,"I<-sss",&r,k)?r.integer:0;
  k[2].string="DefenseHi";  scr_int pdef=prop_get(b,"I<-sss",&r,k)?r.integer:0;
  k[2].string="AgilityHi";  scr_int pagi=prop_get(b,"I<-sss",&r,k)?r.integer:0;
  printf("player: Str=%ld Acc=%ld Def=%ld Agi=%ld  playerStamina=%ld\n",
         (long)pstr,(long)pacc,(long)pdef,(long)pagi,(long)gs_playerstamina(g));

  /* find a projectile weapon */
  scr_int w=-1;
  for(scr_int o=0;o<gs_object_count(g);o++){
    if(!battle_is_weapon(g,o)) continue;
    scr_int m=battle_weapon_method(g,o);
    if(m==3||m==5){w=o;break;}
  }
  if(w<0){printf("(no projectile weapon)\n\n");return 0;}
  scr_int wm=battle_weapon_method(g,w);
  printf("weapon: '%s' method=%s HitValue=%ld Accuracy=%ld\n",
         onm(g,w),methods[wm],(long)oi(g,w,"HitValue"),(long)oi(g,w,"Accuracy"));

  /* find first live enemy */
  scr_int en=-1;
  for(scr_int n=0;n<gs_npc_count(g);n++)
    if(ni(g,n,"Attitude")==2 && gs_npc_stamina(g,n)>0){en=n;break;}
  if(en<0){printf("(no live enemy)\n\n");return 0;}
  printf("enemy : '%s' Stamina=%ld Str=%ld Def=%ld Acc=%ld Agi=%ld\n",
         nname(g,en),(long)gs_npc_stamina(g,en),(long)ni(g,en,"StrengthHi"),
         (long)ni(g,en,"DefenseHi"),(long)ni(g,en,"AccuracyHi"),(long)ni(g,en,"AgilityHi"));

  /* arm the player with the projectile weapon, co-locate the enemy */
  gs_object_player_get(g,w);
  gs_set_playerwield(g,w);
  gs_set_npc_location(g,en,gs_playerroom(g)+1);
  printf("default_weapon after wield = %ld (weapon obj = %ld)\n",
         (long)battle_player_default_weapon(g),(long)w);

  /* resolve a fight: keep player alive, attack enemy with the wielded weapon */
  printf("--- fight (player attacks; stamina kept up so we can watch enemy fall) ---\n");
  scr_int prev=gs_npc_stamina(g,en), round=0;
  for(round=1; round<=40 && gs_npc_stamina(g,en)>0 && gs_npc_location(g,en)>0; round++){
    gs_set_playerstamina(g,1000);
    battle_player_attack(g,en,w);
    scr_int now=gs_npc_stamina(g,en);
    printf("round %2ld: enemy stamina %4ld -> %4ld  (dmg %ld)%s\n",
           (long)round,(long)prev,(long)now,(long)(prev-now),
           gs_npc_location(g,en)<=0?"  [ENEMY DEAD/REMOVED]":"");
    flush(g);
    prev=now;
  }
  printf("RESULT: enemy stamina=%ld location=%ld after %ld rounds\n\n",
         (long)gs_npc_stamina(g,en),(long)gs_npc_location(g,en),(long)round-1);
  return 0;
}
