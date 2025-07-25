#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <math.h>
#include <assert.h>
#include <float.h>
#include <random>
#include <string>

//  <time.h> added to add time stamps to recorded data
#include <time.h>


#include "minorGems/util/stringUtils.h"
#include "minorGems/util/SettingsManager.h"
#include "minorGems/util/SimpleVector.h"
#include "minorGems/network/SocketServer.h"
#include "minorGems/network/SocketPoll.h"
#include "minorGems/network/web/WebRequest.h"
#include "minorGems/network/web/URLUtils.h"

#include "minorGems/crypto/hashes/sha1.h"

#include "minorGems/system/Thread.h"
#include "minorGems/system/Time.h"

#include "minorGems/game/doublePair.h"

#include "minorGems/util/log/AppLog.h"
#include "minorGems/util/log/FileLog.h"

#include "minorGems/formats/encodingUtils.h"

#include "minorGems/io/file/File.h"


#include "map.h"
#include "../gameSource/transitionBank.h"
#include "../gameSource/objectBank.h"
#include "../gameSource/objectMetadata.h"
#include "../gameSource/animationBank.h"
#include "../gameSource/categoryBank.h"
#include "../commonSource/sayLimit.h"

#include "lifeLog.h"
#include "foodLog.h"
#include "backup.h"
#include "triggers.h"
#include "playerStats.h"
#include "lineageLog.h"
#include "serverCalls.h"
#include "failureLog.h"
#include "names.h"
#include "curses.h"
#include "lineageLimit.h"
#include "objectSurvey.h"
#include "language.h"
#include "familySkipList.h"
#include "lifeTokens.h"
#include "fitnessScore.h"
#include "arcReport.h"
#include "curseDB.h"
#include "cravings.h"


#include "minorGems/util/random/JenkinsRandomSource.h"


//#define IGNORE_PRINTF

#ifdef IGNORE_PRINTF
#define printf(fmt, ...) (0)
#endif


static FILE *familyDataLogFile = NULL;


static JenkinsRandomSource randSource;


#include "../gameSource/GridPos.h"


#define HEAT_MAP_D 13

float targetHeat = 10;


double secondsPerYear = 60.0;



#define PERSON_OBJ_ID 12


int minPickupBabyAge = 10;

int babyAge = 5;

// age when bare-hand actions become available to a baby (opening doors, etc.)
int defaultActionAge = 3;


double forceDeathAge = 120;
// UncleGus Custom Variables
double adultAge = 20;
double oldAge = 104;
double fertileAge = 15;
// End UncleGus Custom Variables
double minSayGapInSeconds = 1.0;

// each generation is at minimum 14 minutes apart
// so 1024 generations is approximately 10 days
int maxLineageTracked = 1024;

int apocalypsePossible = 0;
char apocalypseTriggered = false;
char apocalypseRemote = false;
GridPos apocalypseLocation = { 0, 0 };
int lastApocalypseNumber = 0;
double apocalypseStartTime = 0;
char apocalypseStarted = false;
char postApocalypseStarted = false;


double remoteApocalypseCheckInterval = 30;
double lastRemoteApocalypseCheckTime = 0;
WebRequest *apocalypseRequest = NULL;



static int babyInheritMonument = 1;
char monumentCallPending = false;
int monumentCallX = 0;
int monumentCallY = 0;
int monumentCallID = 0;




static double minFoodDecrementSeconds = 5.0;
static double maxFoodDecrementSeconds = 20;

static double newPlayerFoodDecrementSecondsBonus = 8;
static int newPlayerFoodEatingBonus = 5;
// first 10 hours of living
static double newPlayerFoodBonusHalfLifeSeconds = 36000;



static int babyBirthFoodDecrement = 10;

// bonus applied to all foods
// makes whole server a bit easier (or harder, if negative)
static int eatBonus = 0;

// static double eatBonusFloor = 0;
// static double eatBonusHalfLife = 50;

static int canYumChainBreak = 0;

static double minAgeForCravings = 10;

static int eatEverythingMode = 0;


// static double posseSizeSpeedMultipliers[4] = { 0.75, 1.25, 1.5, 2.0 };



static int minActivePlayersForLanguages = 15;


// keep a running sequence number to challenge each connecting client
// to produce new login hashes, avoiding replay attacks.
static unsigned int nextSequenceNumber = 1;


static int requireClientPassword = 1;
static int requireTicketServerCheck = 1;
static char *clientPassword = NULL;
static char *ticketServerURL = NULL;
static char *reflectorURL = NULL;

// larger of dataVersionNumber.txt or serverCodeVersionNumber.txt
static int versionNumber = 1;


static double childSameRaceLikelihood = 0.9;
static int familySpan = 2;


// phrases that trigger baby and family naming
static SimpleVector<char*> nameGivingPhrases;
static SimpleVector<char*> familyNameGivingPhrases;
static SimpleVector<char*> cursingPhrases;

char *curseYouPhrase = NULL;
char *curseBabyPhrase = NULL;

static SimpleVector<char*> forgivingPhrases;
static SimpleVector<char*> youForgivingPhrases;


static SimpleVector<char*> youGivingPhrases;
static SimpleVector<char*> namedGivingPhrases;


// password-protected objects
static SimpleVector<char*> passwordProtectingPhrases;

typedef struct passwordRecord {
        int x;
        int y;
        std::string password;
    } passwordRecord;

static SimpleVector<passwordRecord> passwordRecords;

void restorePasswordRecord( int x, int y, unsigned char* passwordChars ) {
    std::string password( reinterpret_cast<char*>( passwordChars ) );
    passwordRecord r = { x, y, password };
    passwordRecords.push_back( r );
    }
    
//2HOL: <fstream>, <iostream> added to handle restoration of in-game passwords on server restart
#include <fstream>
#include <iostream>
    
void temp_passwordRecordTransfer() {
    
    SimpleVector<passwordRecord> temp_passwordRecords;
    
    // look through saved passwords and get ones that belong to the currently processed object kind
    std::ifstream file;
    file.open( "2HOL passwords.txt" );
    if ( !file.is_open() ) return;
    // parsing 2HOL passwords.txt, the expected format is "id:%i|x:%i|y:%i|word:%s"
    for ( std::string line; std::getline(file, line); ) {
        
        if ( line.find("id:") == std::string::npos ) continue;
        
        // int posId = line.find("id:") + 3;
        // int lenId = line.find("|", posId) - posId;
        int posX = line.find("x:") + 2;
        int lenX = line.find("|", posX) - posX;
        int posY = line.find("y:") + 2;
        int lenY = line.find("|", posY) - posY;
        int posPw = line.find("word:") + 5;
        
        // int id = stoi(line.substr(posId, lenId));
        int x = stoi(line.substr(posX, lenX));
        int y = stoi(line.substr(posY, lenY));
        std::string pw = line.substr(posPw, line.length());
        
        
        // remove duplicated saved passwords
        // so only the last row counts
        for( int i=0; i<temp_passwordRecords.size(); i++ ) {
            passwordRecord r = temp_passwordRecords.getElementDirect(i);
            if ( x == r.x && y == r.y ) {
                temp_passwordRecords.deleteElement(i);
                break;
                }
            }
        
        // std::cout << "\nRestoring secret word for object with ID:" << inR->id;
        
        char* pwc = new char[48];
        strcpy (pwc, pw.c_str());
        
        passwordRecord r = { x, y, pw };
        
        temp_passwordRecords.push_back( r );

        }
    file.close();
    
    for( int i=0; i<temp_passwordRecords.size(); i++ ) {
        passwordRecord r = temp_passwordRecords.getElementDirect(i);
        
        int id = getMapObject( r.x, r.y );
        ObjectRecord *o = getObject( id );
        
        int b = 0;
        if( o->passwordProtectable ) b = 1;
        
        printf( " ========================= %d %d %d %d %s\n", b, id, r.x, r.y, r.password.c_str() );
        
        if( o == NULL ) continue;
        if( !o->passwordProtectable ) continue;
        
        passwordRecords.push_back( r );
        
        persistentMapDBPut( r.x, r.y, 1, r.password.c_str() );
        }
    
    }    


static SimpleVector<char*> infertilityDeclaringPhrases;
static SimpleVector<char*> fertilityDeclaringPhrases;




static char *eveName = NULL;

static char *infertilitySuffix = NULL;
static char *fertilitySuffix = NULL;

// maps extended ascii codes to true/false for characters allowed in SAY
// messages
static char allowedSayCharMap[256];

static const char *allowedSayChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ.-,'?! ";


static int killEmotionIndex = 2;
static int victimEmotionIndex = 2;

static int starvingEmotionIndex = 2;
static int satisfiedEmotionIndex = 2;

// if changed also change in discordController.cpp
static int afkEmotionIndex = 2;
static double afkTimeSeconds = 0;

static int drunkEmotionIndex = 2;
static int trippingEmotionIndex = 2;

float getLivingLifeBouncingYOffset( int oid ) {
    // dummy function because this is expected in objectBank and animationBank
    // it is used for yum finder and object finder in the client
    return 0.0;
    }


static double lastBabyPassedThresholdTime = 0;


static double eveWindowStart = 0;


typedef struct PeaceTreaty {
        int lineageAEveID;
        int lineageBEveID;
        
        // they have to say it in both directions
        // before it comes into effect
        char dirAToB;
        char dirBToA;

        // track directions of breaking it later
        char dirAToBBroken;
        char dirBToABroken;
    } PeaceTreaty;

    

static SimpleVector<PeaceTreaty> peaceTreaties;


// may be partial
static PeaceTreaty *getMatchingTreaty( int inLineageAEveID, 
                                       int inLineageBEveID ) {
    
    for( int i=0; i<peaceTreaties.size(); i++ ) {
        PeaceTreaty *p = peaceTreaties.getElement( i );
        

        if( ( p->lineageAEveID == inLineageAEveID &&
              p->lineageBEveID == inLineageBEveID )
            ||
            ( p->lineageAEveID == inLineageBEveID &&
              p->lineageBEveID == inLineageAEveID ) ) {
            // they match a treaty.
            return p;
            }
        }
    return NULL;
    }



// parial treaty returned if it's requested
static char isPeaceTreaty( int inLineageAEveID, int inLineageBEveID,
                           PeaceTreaty **outPartialTreaty = NULL ) {
    
    PeaceTreaty *p = getMatchingTreaty( inLineageAEveID, inLineageBEveID );
        
    if( p != NULL ) {
        
        if( !( p->dirAToB && p->dirBToA ) ) {
            // partial treaty
            if( outPartialTreaty != NULL ) {
                *outPartialTreaty = p;
                }
            return false;
            }
        return true;
        }
    return false;
    }


void sendPeaceWarMessage( const char *inPeaceOrWar,
                          char inWar,
                          int inLineageAEveID, int inLineageBEveID );


static void addPeaceTreaty( int inLineageAEveID, int inLineageBEveID ) {
    PeaceTreaty *p = getMatchingTreaty( inLineageAEveID, inLineageBEveID );
    
    if( p != NULL ) {
        char peaceBefore = p->dirAToB && p->dirBToA;
        
        // maybe it has been sealed in a new direction?
        if( p->lineageAEveID == inLineageAEveID ) {
            p->dirAToB = true;
            p->dirBToABroken = false;
            }
        if( p->lineageBEveID == inLineageAEveID ) {
            p->dirBToA = true;
            p->dirBToABroken = false;
            }
        if( p->dirAToB && p->dirBToA &&
            ! peaceBefore ) {
            // new peace!
            sendPeaceWarMessage( "PEACE", 
                                 false,
                                 p->lineageAEveID, p->lineageBEveID );
            }
        }
    else {
        // else doesn't exist, create new unidirectional
        PeaceTreaty p = { inLineageAEveID, inLineageBEveID,
                          true, false,
                          false, false };
        
        peaceTreaties.push_back( p );
        }
    }



static void removePeaceTreaty( int inLineageAEveID, int inLineageBEveID ) {
    PeaceTreaty *p = getMatchingTreaty( inLineageAEveID, inLineageBEveID );
    
    char remove = false;
    
    if( p != NULL ) {
        if( p->dirAToB && p->dirBToA ) {
            // established
            
            // maybe it has been broken in a new direction?
            if( p->lineageAEveID == inLineageAEveID ) {
                p->dirAToBBroken = true;
                }
            if( p->lineageBEveID == inLineageAEveID ) {
                p->dirBToABroken = true;
                }
            
            if( p->dirAToBBroken && p->dirBToABroken ) {
                // fully broken
                // remove it
                remove = true;

                // new war!
                sendPeaceWarMessage( "WAR",
                                     true,
                                     p->lineageAEveID, p->lineageBEveID );
                }
            }
        else {
            // not fully established
            // remove it 
            
            // this means if one person says PEACE and the other
            // responds with WAR, the first person's PEACE half-way treaty
            // is canceled.  Both need to say PEACE again once WAR has been
            // mentioned
            remove = true;
            }
        }
    
    if( remove ) {
        for( int i=0; i<peaceTreaties.size(); i++ ) {
            PeaceTreaty *otherP = peaceTreaties.getElement( i );
            
            if( otherP->lineageAEveID == p->lineageAEveID &&
                otherP->lineageBEveID == p->lineageBEveID ) {
                
                peaceTreaties.deleteElement( i );
                return;
                }
            }
        }
    }


typedef struct PastLifeStats {
        int lifeCount;
        int lifeTotalSeconds;
        char error;
    } PastLifeStats;

    



// for incoming socket connections that are still in the login process
typedef struct FreshConnection {
        Socket *sock;
        SimpleVector<char> *sockBuffer;

        unsigned int sequenceNumber;
        char *sequenceNumberString;
        
        WebRequest *ticketServerRequest;
        char ticketServerAccepted;
        char lifeTokenSpent;

        float fitnessScore;

        double ticketServerRequestStartTime;
        
        char error;
        const char *errorCauseString;
        
        double rejectedSendTime;

        char shutdownMode;

        // for tracking connections that have failed to LOGIN 
        // in a timely manner
        double connectionStartTimeSeconds;

        char *email;
	char *spawnCode = NULL;
        uint32_t hashedSpawnSeed;
        char *famTarget = NULL;
        
        int tutorialNumber;
        CurseStatus curseStatus;
        PastLifeStats lifeStats;
        
        char *twinCode;
        int twinCount;
        char playerListSent;

        int mMapD;
    } FreshConnection;


SimpleVector<FreshConnection> newConnections;

SimpleVector<FreshConnection> waitingForTwinConnections;



typedef struct LiveObject {
        char *email;
        // for tracking old email after player has been deleted 
        // but is still on list
        char *origEmail;
        
        int id;
        
        // -1 if unknown
        float fitnessScore;
        

        // object ID used to visually represent this player
        int displayID;
        
        char *name;
        char nameHasSuffix;
        char *displayedName;
        
        char *familyName;
        

        char *lastSay;
        
        // password-protected objects
        char *saidPassword;

        CurseStatus curseStatus;
        PastLifeStats lifeStats;
        
        int curseTokenCount;
        char curseTokenUpdate;


        char isEve;        

        char isTutorial;
        
        // used to track incremental tutorial map loading
        TutorialLoadProgress tutorialLoad;


        GridPos birthPos;
        GridPos originalBirthPos;
        

        int parentID;

        // 1 for Eve
        int parentChainLength;

        SimpleVector<int> *lineage;
        
        SimpleVector<int> *ancestorIDs;
        SimpleVector<char*> *ancestorEmails;
        SimpleVector<char*> *ancestorRelNames;
        SimpleVector<double> *ancestorLifeStartTimeSeconds;
        SimpleVector<double> *ancestorLifeEndTimeSeconds;
        

        // id of Eve that started this line
        int lineageEveID;
        


        // time that this life started (for computing age)
        // not actual creation time (can be adjusted to tweak starting age,
        // for example, in case of Eve who starts older).
        double lifeStartTimeSeconds;

        // time when this player actually died
        double deathTimeSeconds;
        
        
        // the wall clock time when this life started
        // used for computing playtime, not age
        double trueStartTimeSeconds;
        

        double lastSayTimeSeconds;

        // held by other player?
        char heldByOther;
        int heldByOtherID;
        char everHeldByParent;

        // player that's responsible for updates that happen to this
        // player during current step
        int responsiblePlayerID;

        // start and dest for a move
        // same if reached destination
        int xs;
        int ys;
        
        int xd;
        int yd;
        
        // next player update should be flagged
        // as a forced position change
        char posForced;
        
        char waitingForForceResponse;
        
        int lastMoveSequenceNumber;


        int facingLeft;
        double lastFlipTime;
        

        int pathLength;
        GridPos *pathToDest;
        
        char pathTruncated;

        char firstMapSent;
        int lastSentMapX;
        int lastSentMapY;
        
        double moveTotalSeconds;
        double moveStartTime;
        
        int facingOverride;
        int actionAttempt;
        GridPos actionTarget;
        
        int holdingID;

        // absolute time in seconds that what we're holding should decay
        // or 0 if it never decays
        timeSec_t holdingEtaDecay;


        // where on map held object was picked up from
        char heldOriginValid;
        int heldOriginX;
        int heldOriginY;
        

        // track origin of held separate to use when placing a grave
        int heldGraveOriginX;
        int heldGraveOriginY;
        int heldGravePlayerID;
        

        // if held object was created by a transition on a target, what is the
        // object ID of the target from the transition?
        int heldTransitionSourceID;
        

        int numContained;
        int *containedIDs;
        timeSec_t *containedEtaDecays;

        // vector of sub-contained for each contained item
        SimpleVector<int> *subContainedIDs;
        SimpleVector<timeSec_t> *subContainedEtaDecays;
        

        // if they've been killed and part of a weapon (bullet?) has hit them
        // this will be included in their grave
        int embeddedWeaponID;
        timeSec_t embeddedWeaponEtaDecay;
        
        // and what original weapon killed them?
        int murderSourceID;
        char holdingWound;

        // who killed them?
        int murderPerpID;
        char *murderPerpEmail;
        
        // or if they were killed by a non-person, what was it?
        int deathSourceID;
        
        // true if this character landed a mortal wound on another player
        char everKilledAnyone;

        // true in case of sudden infant death
        char suicide;
        

        Socket *sock;
        SimpleVector<char> *sockBuffer;
        
        // indicates that some messages were sent to this player this 
        // frame, and they need a FRAME terminator message
        char gotPartOfThisFrame;
        

        char isNew;
        char isNewCursed;
        char firstMessageSent;
        
        char inFlight;
        

        char dying;
        // wall clock time when they will be dead
        double dyingETA;

        // in cases where their held wound produces a forced emot
        char emotFrozen;
        double emotUnfreezeETA;
        int emotFrozenIndex;
        
        char starving;
        

        char connected;
        
        char error;
        const char *errorCauseString;
        
        

        int customGraveID;
        
        char *deathReason;

        char deleteSent;
        // wall clock time when we consider the delete good and sent
        // and can close their connection
        double deleteSentDoneETA;

        char deathLogged;

        char newMove;
        
        // Absolute position used when generating last PU sent out about this
        // player.
        // If they are making a very long chained move, and their status
        // isn't changing, they might not generate a PU message for a very
        // long time.  This becomes a problem when them move out/in range
        // of another player.  If their status (held item, etc) has changed
        // while they are out of range, the other player won't see that
        // status change when they come back in range (because the PU
        // happened when they were out of range) and the long chained move
        // isn't generating any PU messages now that they are back in range.
        // Since modded clients might make very long MOVEs for each part
        // of a MOVE chain (since they are zoomed out), we can't just count
        // MOVE messages sent since the las PU message went out.
        // We use this position to determine how far they've moved away
        // from their last PU position, and send an intermediary PU if
        // they get too far away
        GridPos lastPlayerUpdateAbsolutePos;
        

        // heat map that player carries around with them
        // every time they stop moving, it is updated to compute
        // their local temp
        float heatMap[ HEAT_MAP_D * HEAT_MAP_D ];

        // net heat of environment around player
        // map is tracked in heat units (each object produces an 
        // integer amount of heat)
        // this is in base heat units, range 0 to infinity
        float envHeat;

        // amount of heat currently in player's body, also in
        // base heat units
        float bodyHeat;
        

        // used track current biome heat for biome-change shock effects
        float biomeHeat;
        float lastBiomeHeat;


        // body heat normalized to [0,1], with targetHeat at 0.5
        float heat;
        
        // flags this player as needing to recieve a heat update
        char heatUpdate;
        
        // wall clock time of last time this player was sent
        // a heat update
        double lastHeatUpdate;

        // true if heat map features player surrounded by walls
        char isIndoors;
        


        int foodStore;
        
        double foodCapModifier;

        double drunkenness;
        bool drunkennessEffect;
        double drunkennessEffectETA;
        
        bool tripping;
        bool gonnaBeTripping;
        double trippingEffectStartTime;
        double trippingEffectETA;


        double fever;
        

        // wall clock time when we should decrement the food store
        double foodDecrementETASeconds;
        
        // should we send player a food status message
        char foodUpdate;
        
        // info about the last thing we ate, for FX food messages sent
        // just to player
        int lastAteID;
        int lastAteFillMax;
        
        // this is for PU messages sent to everyone
        char justAte;
        int justAteID;
        
        // chain of non-repeating foods eaten
        SimpleVector<int> yummyFoodChain;
        
        // how many bonus from yummy food is stored
        // these are used first before food is decremented
        int yummyBonusStore;
        
        // last time we told player their capacity in a food update
        // what did we tell them?
        int lastReportedFoodCapacity;
        

        ClothingSet clothing;
        
        timeSec_t clothingEtaDecay[NUM_CLOTHING_PIECES];

        SimpleVector<int> clothingContained[NUM_CLOTHING_PIECES];
        
        SimpleVector<timeSec_t> 
            clothingContainedEtaDecays[NUM_CLOTHING_PIECES];

        char needsUpdate;
        char updateSent;
        char updateGlobal;
        
        // babies born to this player
        SimpleVector<timeSec_t> *babyBirthTimes;
        SimpleVector<int> *babyIDs;

        // for CURSE MY BABY after baby is dead/deleted
        char *lastBabyEmail;
        
        
        // wall clock time after which they can have another baby
        // starts at 0 (start of time epoch) for non-mothers, as
        // they can have their first baby right away.
        timeSec_t birthCoolDown;
        
        bool declaredInfertile;

        timeSec_t lastRegionLookTime;
        
        double playerCrossingCheckTime;
        

        char monumentPosSet;
        GridPos lastMonumentPos;
        int lastMonumentID;
        char monumentPosSent;
        

        char holdingFlightObject;
        
        char vogMode;
        GridPos preVogPos;
        GridPos preVogBirthPos;
        int vogJumpIndex;
        char postVogMode;
        
        char forceSpawn;
        

        // list of positions owned by this player
        SimpleVector<GridPos> ownedPositions;

        // list of owned positions that this player has heard about
        SimpleVector<GridPos> knownOwnedPositions;
        
        // email of last baby that we had that did /DIE
        char *lastSidsBabyEmail;
        
        //2HOL mechanics to read written objects
        //positions already read while in range
        SimpleVector<GridPos> readPositions;
        timeSec_t lastWrittenObjectScanTime;
        GridPos lastWrittenObjectScanPos;
        
        //time when read position is expired and can be read again
        SimpleVector<double> readPositionsETA;

        GridPos forceFlightDest;
        double forceFlightDestSetTime;

        SimpleVector<int> permanentEmots;
                
        //2HOL: last time player does something
        double lastActionTime;
        
        //2HOL: player is either disconnected or inactive
        bool isAFK;

        Craving cravingFood;
        int cravingFoodYumIncrement;
        char cravingKnown;

        // to give new players a boost
        // set these at birth based on how long they have played so far
        int personalEatBonus;
        double personalFoodDecrementSecondsBonus;
        

        // don't send global messages too quickly
        // give player chance to read each one
        double lastGlobalMessageTime;
        
        SimpleVector<char*> globalMessageQueue;

        int mMapD;

    } LiveObject;



SimpleVector<LiveObject> players;
SimpleVector<LiveObject> tutorialLoadingPlayers;



char doesEveLineExist( int inEveID ) {
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( ( ! o->error ) && o->lineageEveID == inEveID ) {
            return true;
            }
        }
    return false;
    }




typedef struct DeadObject {
        int id;
        
        int displayID;
        
        char *name;
        
        SimpleVector<int> *lineage;
        
        // id of Eve that started this line
        int lineageEveID;
        


        // time that this life started (for computing age)
        // not actual creation time (can be adjusted to tweak starting age,
        // for example, in case of Eve who starts older).
        double lifeStartTimeSeconds;
        
        // time this person died
        double deathTimeSeconds;
        

    } DeadObject;



static double lastPastPlayerFlushTime = 0;

SimpleVector<DeadObject> pastPlayers;



static void addPastPlayer( LiveObject *inPlayer ) {
    
    DeadObject o;
    
    o.id = inPlayer->id;
    o.displayID = inPlayer->displayID;
    o.name = NULL;
    if( inPlayer->name != NULL ) {
        o.name = stringDuplicate( inPlayer->name );
        }
    o.lineageEveID = inPlayer->lineageEveID;
    o.lifeStartTimeSeconds = inPlayer->lifeStartTimeSeconds;
    o.deathTimeSeconds = inPlayer->deathTimeSeconds;
    
    o.lineage = new SimpleVector<int>();
    for( int i=0; i< inPlayer->lineage->size(); i++ ) {
        o.lineage->push_back( inPlayer->lineage->getElementDirect( i ) );
        }
    
    pastPlayers.push_back( o );
    }



char isOwned( LiveObject *inPlayer, int inX, int inY ) {
    for( int i=0; i<inPlayer->ownedPositions.size(); i++ ) {
        GridPos *p = inPlayer->ownedPositions.getElement( i );
        
        if( p->x == inX && p->y == inY ) {
            return true;
            }
        }
    return false;
    }



char isOwned( LiveObject *inPlayer, GridPos inPos ) {
    return isOwned( inPlayer, inPos.x, inPos.y );
    }



char isKnownOwned( LiveObject *inPlayer, int inX, int inY ) {
    for( int i=0; i<inPlayer->knownOwnedPositions.size(); i++ ) {
        GridPos *p = inPlayer->knownOwnedPositions.getElement( i );
        
        if( p->x == inX && p->y == inY ) {
            return true;
            }
        }
    return false;
    }



char isKnownOwned( LiveObject *inPlayer, GridPos inPos ) {
    return isKnownOwned( inPlayer, inPos.x, inPos.y );
    }


// messages with no follow-up hang out on client for 10 seconds
// 7 seconds should be long enough to read if there's a follow-up waiting
static double minGlobalMessageSpacingSeconds = 7;


void sendGlobalMessage( char *inMessage,
                        LiveObject *inOnePlayerOnly = NULL );


void sendMessageToPlayer( LiveObject *inPlayer, 
                          char *inMessage, int inLength );

SimpleVector<GridPos> recentlyRemovedOwnerPos;


void removeAllOwnership( LiveObject *inPlayer ) {
    double startTime = Time::getCurrentTime();
    int num = inPlayer->ownedPositions.size();
    
    for( int i=0; i<inPlayer->ownedPositions.size(); i++ ) {
        GridPos *p = inPlayer->ownedPositions.getElement( i );

        recentlyRemovedOwnerPos.push_back( *p );
        
        int oID = getMapObject( p->x, p->y );

        if( oID <= 0 ) {
            continue;
            }

        char noOtherOwners = true;
        
        for( int j=0; j<players.size(); j++ ) {
            LiveObject *otherPlayer = players.getElement( j );
            
            if( otherPlayer != inPlayer ) {
                if( isOwned( otherPlayer, *p ) ) {
                    noOtherOwners = false;
                    break;
                    }
                }
            }
        
        if( noOtherOwners ) {
            // last owner of p just died
            // force end transition
            SimpleVector<int> *deathMarkers = getAllPossibleDeathIDs();
            for( int j=0; j<deathMarkers->size(); j++ ) {
                int deathID = deathMarkers->getElementDirect( j );
                TransRecord *t = getTrans( deathID, oID );
                
                if( t != NULL ) {
                    
                    setMapObject( p->x, p->y, t->newTarget );
                    break;
                    }
                }
            }
        }
    
    inPlayer->ownedPositions.deleteAll();

    AppLog::infoF( "Removing all ownership (%d owned) for "
                   "player %d (%s) took %lf sec",
                   num, inPlayer->id, inPlayer->email, 
                   Time::getCurrentTime() - startTime );
    }



char *getOwnershipString( int inX, int inY ) {    
    SimpleVector<char> messageWorking;
    
    for( int j=0; j<players.size(); j++ ) {
        LiveObject *otherPlayer = players.getElement( j );
        if( ! otherPlayer->error &&
            isOwned( otherPlayer, inX, inY ) ) {
            char *playerIDString = 
                autoSprintf( " %d", otherPlayer->id );
            messageWorking.appendElementString( 
                playerIDString );
            delete [] playerIDString;
            }
        }
    char *message = messageWorking.getElementString();
    return message;
    }


char *getOwnershipString( GridPos inPos ) {
    return getOwnershipString( inPos.x, inPos.y );
    }



static char checkReadOnly() {
    const char *testFileName = "testReadOnly.txt";
    
    FILE *testFile = fopen( testFileName, "w" );
    
    if( testFile != NULL ) {
        
        fclose( testFile );
        remove( testFileName );
        return false;
        }
    return true;
    }




// returns a person to their natural state
static void backToBasics( LiveObject *inPlayer ) {
    LiveObject *p = inPlayer;

    // do not heal dying people
    if( ! p->holdingWound && p->holdingID > 0 ) {
        
        p->holdingID = 0;
        
        p->holdingEtaDecay = 0;
        
        p->heldOriginValid = false;
        p->heldTransitionSourceID = -1;
        
        
        p->numContained = 0;
        if( p->containedIDs != NULL ) {
            delete [] p->containedIDs;
            delete [] p->containedEtaDecays;
            p->containedIDs = NULL;
        p->containedEtaDecays = NULL;
            }
        
        if( p->subContainedIDs != NULL ) {
            delete [] p->subContainedIDs;
            delete [] p->subContainedEtaDecays;
            p->subContainedIDs = NULL;
            p->subContainedEtaDecays = NULL;
            }
        }
        
        
    p->clothing = getEmptyClothingSet();
    
    for( int c=0; c<NUM_CLOTHING_PIECES; c++ ) {
        p->clothingEtaDecay[c] = 0;
        p->clothingContained[c].deleteAll();
        p->clothingContainedEtaDecays[c].deleteAll();
        }

    p->emotFrozen = false;
    p->emotUnfreezeETA = 0;
    }




typedef struct GraveInfo {
        GridPos pos;
        int playerID;
        // eve that started the line of this dead person
        // used for tracking whether grave is part of player's family or not
        int lineageEveID;
    } GraveInfo;


typedef struct GraveMoveInfo {
        GridPos posStart;
        GridPos posEnd;
        int swapDest;
    } GraveMoveInfo;




// tracking spots on map that inflicted a mortal wound
// put them on timeout afterward so that they don't attack
// again immediately
typedef struct DeadlyMapSpot {
        GridPos pos;
        double timeOfAttack;
    } DeadlyMapSpot;


static double deadlyMapSpotTimeoutSec = 10;

static SimpleVector<DeadlyMapSpot> deadlyMapSpots;


static char wasRecentlyDeadly( GridPos inPos ) {
    double curTime = Time::getCurrentTime();
    
    for( int i=0; i<deadlyMapSpots.size(); i++ ) {
        
        DeadlyMapSpot *s = deadlyMapSpots.getElement( i );
        
        if( curTime - s->timeOfAttack >= deadlyMapSpotTimeoutSec ) {
            deadlyMapSpots.deleteElement( i );
            i--;
            }
        else if( s->pos.x == inPos.x && s->pos.y == inPos.y ) {
            // note that this is a lazy method that only walks through
            // the whole list and checks for timeouts when
            // inPos isn't found
            return true;
            }
        }
    return false;
    }



static void addDeadlyMapSpot( GridPos inPos ) {
    // don't check for duplicates
    // we're only called to add a new deadly spot when the spot isn't
    // currently on deadly cooldown anyway
    DeadlyMapSpot s = { inPos, Time::getCurrentTime() };
    deadlyMapSpots.push_back( s );
    }




static LiveObject *getLiveObject( int inID ) {
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( o->id == inID ) {
            return o;
            }
        }
    
    return NULL;
    }


char *getPlayerName( int inID ) {
    LiveObject *o = getLiveObject( inID );
    if( o != NULL ) {
        return o->name;
        }
    return NULL;
    }




static double pickBirthCooldownSeconds() {
    // Kumaraswamy distribution
    // PDF:
    // k(x,a,b) = a * b * x**( a - 1 ) * (1-x**a)**(b-1)
    // CDF:
    // kCDF(x,a,b) = 1 - (1-x**a)**b
    // Invers CDF:
    // kCDFInv(y,a,b) = ( 1 - (1-y)**(1.0/b) )**(1.0/a)

    // For b=1, PDF curve starts at 0 and curves upward, for all a > 2
    // good values seem to be a=1.5, b=1

    // actually, make it more bell-curve like, with a=2, b=3

    double a = 2;
    double b = 3;
    
    // mean is around 2 minutes
    

    // uniform
    double u = randSource.getRandomDouble();
    
    // feed into inverted CDF to sample a value from the distribution
    double v = pow( 1 - pow( 1-u, (1/b) ), 1/a );
    
    // v is in [0..1], the value range of Kumaraswamy

    // put max at 5 minutes
    return v * 5 * 60;
    }




typedef struct FullMapContained{ 
        int numContained;
        int *containedIDs;
        timeSec_t *containedEtaDecays;
        SimpleVector<int> *subContainedIDs;
        SimpleVector<timeSec_t> *subContainedEtaDecays;
    } FullMapContained;



// including contained and sub contained in one call
FullMapContained getFullMapContained( int inX, int inY ) {
    FullMapContained r;
    
    r.containedIDs = getContained( inX, inY, &( r.numContained ) );
    r.containedEtaDecays = 
        getContainedEtaDecay( inX, inY, &( r.numContained ) );
    
    if( r.numContained == 0 ) {
        r.subContainedIDs = NULL;
        r.subContainedEtaDecays = NULL;
        }
    else {
        r.subContainedIDs = new SimpleVector<int>[ r.numContained ];
        r.subContainedEtaDecays = new SimpleVector<timeSec_t>[ r.numContained ];
        }
    
    for( int c=0; c< r.numContained; c++ ) {
        if( r.containedIDs[c] < 0 ) {
            
            int numSub;
            int *subContainedIDs = getContained( inX, inY, &numSub,
                                                 c + 1 );
            
            if( subContainedIDs != NULL ) {
                
                r.subContainedIDs[c].appendArray( subContainedIDs, numSub );
                delete [] subContainedIDs;
                }
            
            timeSec_t *subContainedEtaDecays = 
                getContainedEtaDecay( inX, inY, &numSub,
                                      c + 1 );

            if( subContainedEtaDecays != NULL ) {
                
                r.subContainedEtaDecays[c].appendArray( subContainedEtaDecays, 
                                                        numSub );
                delete [] subContainedEtaDecays;
                }
            }
        }
    
    return r;
    }



void freePlayerContainedArrays( LiveObject *inPlayer ) {
    if( inPlayer->containedIDs != NULL ) {
        delete [] inPlayer->containedIDs;
        }
    if( inPlayer->containedEtaDecays != NULL ) {
        delete [] inPlayer->containedEtaDecays;
        }
    if( inPlayer->subContainedIDs != NULL ) {
        delete [] inPlayer->subContainedIDs;
        }
    if( inPlayer->subContainedEtaDecays != NULL ) {
        delete [] inPlayer->subContainedEtaDecays;
        }

    inPlayer->containedIDs = NULL;
    inPlayer->containedEtaDecays = NULL;
    inPlayer->subContainedIDs = NULL;
    inPlayer->subContainedEtaDecays = NULL;
    }



void setContained( LiveObject *inPlayer, FullMapContained inContained ) {
    
    inPlayer->numContained = inContained.numContained;
     
    freePlayerContainedArrays( inPlayer );
    
    inPlayer->containedIDs = inContained.containedIDs;
    
    inPlayer->containedEtaDecays =
        inContained.containedEtaDecays;
    
    inPlayer->subContainedIDs =
        inContained.subContainedIDs;
    inPlayer->subContainedEtaDecays =
        inContained.subContainedEtaDecays;
    }
    
    
    
    
void clearPlayerHeldContained( LiveObject *inPlayer ) {
    inPlayer->numContained = 0;
    
    delete [] inPlayer->containedIDs;
    delete [] inPlayer->containedEtaDecays;
    delete [] inPlayer->subContainedIDs;
    delete [] inPlayer->subContainedEtaDecays;
    
    inPlayer->containedIDs = NULL;
    inPlayer->containedEtaDecays = NULL;
    inPlayer->subContainedIDs = NULL;
    inPlayer->subContainedEtaDecays = NULL;
    }
    



void transferHeldContainedToMap( LiveObject *inPlayer, int inX, int inY ) {
    if( inPlayer->numContained != 0 ) {
        timeSec_t curTime = Time::getCurrentTime();
        float stretch = 
            getObject( inPlayer->holdingID )->slotTimeStretch;
        
        for( int c=0;
             c < inPlayer->numContained;
             c++ ) {
            
            // undo decay stretch before adding
            // (stretch applied by adding)
            if( stretch != 1.0 &&
                inPlayer->containedEtaDecays[c] != 0 ) {
                
                timeSec_t offset = 
                    inPlayer->containedEtaDecays[c] - curTime;
                
                offset = offset * stretch;
                
                inPlayer->containedEtaDecays[c] =
                    curTime + offset;
                }

            addContained( 
                inX, inY,
                inPlayer->containedIDs[c],
                inPlayer->containedEtaDecays[c] );

            int numSub = inPlayer->subContainedIDs[c].size();
            if( numSub > 0 ) {

                int container = inPlayer->containedIDs[c];
                
                if( container < 0 ) {
                    container *= -1;
                    }
                
                float subStretch = getObject( container )->slotTimeStretch;
                    
                
                int *subIDs = 
                    inPlayer->subContainedIDs[c].getElementArray();
                timeSec_t *subDecays = 
                    inPlayer->subContainedEtaDecays[c].
                    getElementArray();
                
                for( int s=0; s < numSub; s++ ) {
                    
                    // undo decay stretch before adding
                    // (stretch applied by adding)
                    if( subStretch != 1.0 &&
                        subDecays[s] != 0 ) {
                
                        timeSec_t offset = subDecays[s] - curTime;
                        
                        offset = offset * subStretch;
                        
                        subDecays[s] = curTime + offset;
                        }

                    addContained( inX, inY,
                                  subIDs[s], subDecays[s],
                                  c + 1 );
                    }
                delete [] subIDs;
                delete [] subDecays;
                }
            }

        clearPlayerHeldContained( inPlayer );
        }
    }




// diagonal steps are longer
static double measurePathLength( int inXS, int inYS, 
                                 GridPos *inPathPos, int inPathLength ) {
    
    // diags are square root of 2 in length
    double diagLength = 1.41421356237;
    

    double totalLength = 0;
    
    GridPos lastPos = { inXS, inYS };
    
    for( int i=0; i<inPathLength; i++ ) {
        
        GridPos thisPos = inPathPos[i];
        
        if( thisPos.x != lastPos.x &&
            thisPos.y != lastPos.y ) {
            totalLength += diagLength;
            }
        else {
            // not diag
            totalLength += 1;
            }
        lastPos = thisPos;
        }
    
    return totalLength;
    }



bool sameRoadClass( int inFloorA, int inFloorB ) {
    if( inFloorA <= 0 || inFloorB <= 0 ) {
        return false;
        }
    
    if( inFloorA == inFloorB ) {
        return true;
        }
    
    // the 2 floors are in the same class if they are in the same cateogory which name contains +road tag
    ReverseCategoryRecord *floorARecord = getReverseCategory( inFloorA );
    ReverseCategoryRecord *floorBRecord = getReverseCategory( inFloorB );
    
    if( floorARecord != NULL && floorBRecord != NULL ) {
        for( int i=0; i< floorARecord->categoryIDSet.size(); i++ ) {
            int floorACID = floorARecord->categoryIDSet.getElementDirect( i );
            
            for( int j=0; j< floorBRecord->categoryIDSet.size(); j++ ) {
                int floorBCID = floorBRecord->categoryIDSet.getElementDirect( j );
                
                if( floorACID == floorBCID ) {
                    CategoryRecord *floorCategory = getCategory( floorACID );
                    if( floorCategory == NULL ) continue;
                    int categoryID = floorCategory->parentID;
                    ObjectRecord *categoryObj = getObject( categoryID );
                    if( categoryObj == NULL ) continue;
                    if( strstr( categoryObj->description, "+road" ) != NULL ) return true;
                    }
                }
            }
        }

    return false;
    }



static double getPathSpeedModifier( GridPos *inPathPos, int inPathLength ) {
    
    if( inPathLength < 1 ) {
        return 1;
        }
    

    int floor = getMapFloor( inPathPos[0].x, inPathPos[0].y );

    if( floor == 0 ) {
        return 1;
        }

    double speedMult = getObject( floor )->speedMult;
    
    if( speedMult == 1 ) {
        return 1;
        }
    

    // else we have a speed mult for at least first step in path
    // see if we have same floor for rest of path

    for( int i=1; i<inPathLength; i++ ) {
        
        int thisFloor = getMapFloor( inPathPos[i].x, inPathPos[i].y );
        
        if( ! sameRoadClass( thisFloor, floor ) ) {
            // not same floor whole way
            return 1;
            }
        }
    // same floor whole way
    printf( "Speed modifier = %f\n", speedMult );
    return speedMult;
    }



static int getLiveObjectIndex( int inID ) {
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( o->id == inID ) {
            return i;
            }
        }

    return -1;
    }





int nextID = 2;


static void deleteMembers( FreshConnection *inConnection ) {
    delete inConnection->sock;
    delete inConnection->sockBuffer;
    
    if( inConnection->sequenceNumberString != NULL ) {    
        delete [] inConnection->sequenceNumberString;
        }
    
    if( inConnection->ticketServerRequest != NULL ) {
        delete inConnection->ticketServerRequest;
        }
    
    if( inConnection->email != NULL ) {
        delete [] inConnection->email;
        }
    
    if( inConnection->twinCode != NULL ) {
        delete [] inConnection->twinCode;
        }
    if (inConnection->spawnCode != NULL ) {
	    delete [] inConnection->spawnCode;
    }
    }



static SimpleVector<char *> curseWords;

static char *curseSecret = NULL;

static char *playerListSecret = NULL;

void quitCleanup() {
    AppLog::info( "Cleaning up on quit..." );

    // FreshConnections are in two different lists
    // free structures from both
    SimpleVector<FreshConnection> *connectionLists[2] =
        { &newConnections, &waitingForTwinConnections };

    for( int c=0; c<2; c++ ) {
        SimpleVector<FreshConnection> *list = connectionLists[c];
        
        for( int i=0; i<list->size(); i++ ) {
            FreshConnection *nextConnection = list->getElement( i );
            deleteMembers( nextConnection );
            }
        list->deleteAll();
        }
    
    // add these to players to clean them up togeter
    for( int i=0; i<tutorialLoadingPlayers.size(); i++ ) {
        LiveObject nextPlayer = tutorialLoadingPlayers.getElementDirect( i );
        players.push_back( nextPlayer );
        }
    tutorialLoadingPlayers.deleteAll();
    


    for( int i=0; i<players.size(); i++ ) {
        LiveObject *nextPlayer = players.getElement(i);
        
        removeAllOwnership( nextPlayer );

        if( nextPlayer->sock != NULL ) {
            delete nextPlayer->sock;
            nextPlayer->sock = NULL;
            }
        if( nextPlayer->sockBuffer != NULL ) {
            delete nextPlayer->sockBuffer;
            nextPlayer->sockBuffer = NULL;
            }

        delete nextPlayer->lineage;

        delete nextPlayer->ancestorIDs;

        nextPlayer->ancestorEmails->deallocateStringElements();
        delete nextPlayer->ancestorEmails;
        
        nextPlayer->ancestorRelNames->deallocateStringElements();
        delete nextPlayer->ancestorRelNames;
        
        delete nextPlayer->ancestorLifeStartTimeSeconds;
        delete nextPlayer->ancestorLifeEndTimeSeconds;
        

        if( nextPlayer->name != NULL ) {
            delete [] nextPlayer->name;
            }

        if( nextPlayer->displayedName != NULL ) {
            delete [] nextPlayer->displayedName;
            }

        if( nextPlayer->familyName != NULL ) {
            delete [] nextPlayer->familyName;
            }

        if( nextPlayer->lastSay != NULL ) {
            delete [] nextPlayer->lastSay;
            }
        
        if( nextPlayer->email != NULL  ) {
            delete [] nextPlayer->email;
            }
        if( nextPlayer->origEmail != NULL  ) {
            delete [] nextPlayer->origEmail;
            }
        if( nextPlayer->lastBabyEmail != NULL  ) {
            delete [] nextPlayer->lastBabyEmail;
            }
        if( nextPlayer->lastSidsBabyEmail != NULL ) {
            delete [] nextPlayer->lastSidsBabyEmail;
            }

        if( nextPlayer->murderPerpEmail != NULL  ) {
            delete [] nextPlayer->murderPerpEmail;
            }

        nextPlayer->globalMessageQueue.deallocateStringElements();
        
        
        freePlayerContainedArrays( nextPlayer );
        
        
        if( nextPlayer->pathToDest != NULL ) {
            delete [] nextPlayer->pathToDest;
            }
        
        if( nextPlayer->deathReason != NULL ) {
            delete [] nextPlayer->deathReason;
            }


        delete nextPlayer->babyBirthTimes;
        delete nextPlayer->babyIDs;        
        }
    players.deleteAll();


    for( int i=0; i<pastPlayers.size(); i++ ) {
        DeadObject *o = pastPlayers.getElement( i );
        
        delete [] o->name;
        delete o->lineage;
        }
    pastPlayers.deleteAll();
    

    freeLineageLimit();
    
    freePlayerStats();
    freeLineageLog();
    
    freeNames();
    
    freeCurses();
    
    freeCurseDB();
    
    freeLifeTokens();

    freeFitnessScore();

    freeLifeLog();
    
    freeFoodLog();
    freeFailureLog();
    
    freeObjectSurvey();
    
    freeLanguage();
    freeFamilySkipList();

    freeTriggers();

    freeMap();

    freeTransBank();
    freeCategoryBank();
    freeObjectBank();
    freeAnimationBank();
    
    freeArcReport();
    

    if( clientPassword != NULL ) {
        delete [] clientPassword;
        clientPassword = NULL;
        }
    

    if( ticketServerURL != NULL ) {
        delete [] ticketServerURL;
        ticketServerURL = NULL;
        }

    if( reflectorURL != NULL ) {
        delete [] reflectorURL;
        reflectorURL = NULL;
        }

    nameGivingPhrases.deallocateStringElements();
    familyNameGivingPhrases.deallocateStringElements();
    cursingPhrases.deallocateStringElements();
    
    forgivingPhrases.deallocateStringElements();
    youForgivingPhrases.deallocateStringElements();
    
    youGivingPhrases.deallocateStringElements();
    namedGivingPhrases.deallocateStringElements();
    infertilityDeclaringPhrases.deallocateStringElements();
    fertilityDeclaringPhrases.deallocateStringElements();
    
    // password-protected objects
    passwordProtectingPhrases.deallocateStringElements();
    
    if( curseYouPhrase != NULL ) {
        delete [] curseYouPhrase;
        curseYouPhrase = NULL;
        }
    if( curseBabyPhrase != NULL ) {
        delete [] curseBabyPhrase;
        curseBabyPhrase = NULL;
        }
    

    if( eveName != NULL ) {
        delete [] eveName;
        eveName = NULL;
        }
    if( infertilitySuffix != NULL ) {
        delete [] infertilitySuffix;
        infertilitySuffix = NULL;
        }
    if( fertilitySuffix != NULL ) {
        delete [] fertilitySuffix;
        fertilitySuffix = NULL;
        }

    if( apocalypseRequest != NULL ) {
        delete apocalypseRequest;
        apocalypseRequest = NULL;
        }

    if( familyDataLogFile != NULL ) {
        fclose( familyDataLogFile );
        familyDataLogFile = NULL;
        }

    curseWords.deallocateStringElements();
    
    if( curseSecret != NULL ) {
        delete [] curseSecret;
        curseSecret = NULL;
        }
    }






#include "minorGems/util/crc32.h"

JenkinsRandomSource curseSource;


static int cursesUseSenderEmail = 0;

static int useCurseWords = 1;


// result NOT destroyed by caller
static const char *getCurseWord( char *inSenderEmail,
                                 char *inEmail, int inWordIndex ) {
    if( ! useCurseWords || curseWords.size() == 0 ) {
        return "X";
        }

    if( curseSecret == NULL ) {
        curseSecret = 
            SettingsManager::getStringSetting( 
                "statsServerSharedSecret", "sdfmlk3490sadfm3ug9324" );
        }
    
    char *emailPlusSecret;

    if( cursesUseSenderEmail ) {
        emailPlusSecret =
            autoSprintf( "%s_%s_%s", inSenderEmail, inEmail, curseSecret );
        }
    else {
        emailPlusSecret = 
            autoSprintf( "%s_%s", inEmail, curseSecret );
        }
    
    unsigned int c = crc32( (unsigned char*)emailPlusSecret, 
                            strlen( emailPlusSecret ) );
    
    delete [] emailPlusSecret;

    curseSource.reseed( c );
    
    // mix based on index
    for( int i=0; i<inWordIndex; i++ ) {
        curseSource.getRandomDouble();
        }

    int index = curseSource.getRandomBoundedInt( 0, curseWords.size() - 1 );
    
    return curseWords.getElementDirect( index );
    }




volatile char quit = false;

void intHandler( int inUnused ) {
    AppLog::info( "Quit received for unix" );
    
    // since we handled this singal, we will return to normal execution
    quit = true;
    }


#ifdef WIN_32
#include <windows.h>
BOOL WINAPI ctrlHandler( DWORD dwCtrlType ) {
    if( CTRL_C_EVENT == dwCtrlType ) {
        AppLog::info( "Quit received for windows" );
        
        // will auto-quit as soon as we return from this handler
        // so cleanup now
        //quitCleanup();
        
        // seems to handle CTRL-C properly if launched by double-click
        // or batch file
        // (just not if launched directly from command line)
        quit = true;
        }
    return true;
    }
#endif


int numConnections = 0;







// reads all waiting data from socket and stores it in buffer
// returns true if socket still good, false on error
char readSocketFull( Socket *inSock, SimpleVector<char> *inBuffer ) {

    char buffer[512];
    
    int numRead = inSock->receive( (unsigned char*)buffer, 512, 0 );
    
    if( numRead == -1 ) {

        if( ! inSock->isSocketInFDRange() ) {
            // the internal FD of this socket is out of range
            // probably some kind of heap corruption.

            // save a bug report
            int allow = 
                SettingsManager::getIntSetting( "allowBugReports", 0 );

            if( allow ) {
                char *bugName = 
                    autoSprintf( "bug_socket_%f", Time::getCurrentTime() );
                
                char *bugOutName = autoSprintf( "%s_out.txt", bugName );
                
                File outFile( NULL, "serverOut.txt" );
                if( outFile.exists() ) {
                    fflush( stdout );
                    File outCopyFile( NULL, bugOutName );
                    
                    outFile.copy( &outCopyFile );
                    }
                delete [] bugName;
                delete [] bugOutName;
                }
            }
        
            
        return false;
        }
    
    while( numRead > 0 ) {
        inBuffer->appendArray( buffer, numRead );

        numRead = inSock->receive( (unsigned char*)buffer, 512, 0 );
        }

    return true;
    }



// NULL if there's no full message available
char *getNextClientMessage( SimpleVector<char> *inBuffer ) {
    // find first terminal character #

    int index = inBuffer->getElementIndex( '#' );
        
    if( index == -1 ) {

        if( inBuffer->size() > 200 ) {
            // 200 characters with no message terminator?
            // client is sending us nonsense
            // cut it off here to avoid buffer overflow
            
            AppLog::info( "More than 200 characters in client receive buffer "
                          "with no messsage terminator present, "
                          "generating NONSENSE message." );
            
            return stringDuplicate( "NONSENSE 0 0" );
            }

        return NULL;
        }
    
    if( index > 1 && 
        inBuffer->getElementDirect( 0 ) == 'K' &&
        inBuffer->getElementDirect( 1 ) == 'A' ) {
        
        // a KA (keep alive) message
        // short-cicuit the processing here
        
        inBuffer->deleteStartElements( index + 1 );
        return NULL;
        }
    
        

    char *message = new char[ index + 1 ];
    
    // all but terminal character
    for( int i=0; i<index; i++ ) {
        message[i] = inBuffer->getElementDirect( i );
        }
    
    // delete from buffer, including terminal character
    inBuffer->deleteStartElements( index + 1 );
    
    message[ index ] = '\0';
    
    return message;
    }





typedef enum messageType {
    MOVE,
    USE,
    SELF,
    BABY,
    UBABY,
    REMV,
    SREMV,
    DROP,
    KILL,
    SAY,
    EMOT,
    JUMP,
    DIE,
    GRAVE,
    OWNER,
    FORCE,
    MAP,
    TRIGGER,
    BUG,
    PING,
    VOGS,
    VOGN,
    VOGP,
    VOGM,
    VOGI,
    VOGT,
    VOGX,
    PHOTO,
    FLIP,
    UNKNOWN
    } messageType;




typedef struct ClientMessage {
        messageType type;
        int x, y, c, i, id;
        
        int trigger;
        int bug;

        // some messages have extra positions attached
        int numExtraPos;

        // NULL if there are no extra
        GridPos *extraPos;

        // null if type not SAY
        char *saidText;
        
        // null if type not BUG
        char *bugText;

        // for MOVE messages
        int sequenceNumber;

    } ClientMessage;


static int pathDeltaMax = 16;



static int stringToInt( char *inString ) {
    return strtol( inString, NULL, 10 );
    }



// if extraPos present in result, destroyed by caller
// inMessage may be modified by this call
ClientMessage parseMessage( LiveObject *inPlayer, char *inMessage ) {
    
    char nameBuffer[100];
    
    ClientMessage m;
    
    m.i = -1;
    m.c = -1;
    m.id = -1;
    m.trigger = -1;
    m.numExtraPos = 0;
    m.extraPos = NULL;
    m.saidText = NULL;
    m.bugText = NULL;
    m.sequenceNumber = -1;
    
    // don't require # terminator here
    
    
    //int numRead = sscanf( inMessage, 
    //                      "%99s %d %d", nameBuffer, &( m.x ), &( m.y ) );
    

    // profiler finds sscanf as a hotspot
    // try a custom bit of code instead
    
    int numRead = 0;
    
    int parseLen = strlen( inMessage );
    if( parseLen > 99 ) {
        parseLen = 99;
        }
    
    for( int i=0; i<parseLen; i++ ) {
        if( inMessage[i] == ' ' ) {
            switch( numRead ) {
                case 0:
                    if( i != 0 ) {
                        memcpy( nameBuffer, inMessage, i );
                        nameBuffer[i] = '\0';
                        numRead++;
                        // rewind back to read the space again
                        // before the first number
                        i--;
                        }
                    break;
                case 1:
                    m.x = stringToInt( &( inMessage[i] ) );
                    numRead++;
                    break;
                case 2:
                    m.y = stringToInt( &( inMessage[i] ) );
                    numRead++;
                    break;
                }
            if( numRead == 3 ) {
                break;
                }
            }
        }
    

    
    if( numRead >= 2 &&
        strcmp( nameBuffer, "BUG" ) == 0 ) {
        m.type = BUG;
        m.bug = m.x;
        m.bugText = stringDuplicate( inMessage );
        return m;
        }


    if( numRead != 3 ) {
        
        if( numRead == 2 &&
            strcmp( nameBuffer, "TRIGGER" ) == 0 ) {
            m.type = TRIGGER;
            m.trigger = m.x;
            }
        else {
            m.type = UNKNOWN;
            }
        
        return m;
        }
    

    if( strcmp( nameBuffer, "MOVE" ) == 0) {
        m.type = MOVE;
        
        char *atPos = strstr( inMessage, "@" );
        
        int offset = 3;
        
        if( atPos != NULL ) {
            offset = 4;            
            }
        

        // in place, so we don't need to deallocate them
        SimpleVector<char *> *tokens =
            tokenizeStringInPlace( inMessage );
        
        // require an even number of extra coords beyond offset
        if( tokens->size() < offset + 2 || 
            ( tokens->size() - offset ) %2 != 0 ) {
            
            delete tokens;
            
            m.type = UNKNOWN;
            return m;
            }
        
        if( atPos != NULL ) {
            // skip @ symbol in token and parse int
            m.sequenceNumber = 
                stringToInt( &( tokens->getElementDirect( 3 )[1] ) );
            }

        int numTokens = tokens->size();
        
        m.numExtraPos = (numTokens - offset) / 2;
        
        m.extraPos = new GridPos[ m.numExtraPos ];

        for( int e=0; e<m.numExtraPos; e++ ) {
            
            char *xToken = tokens->getElementDirect( offset + e * 2 );
            char *yToken = tokens->getElementDirect( offset + e * 2 + 1 );
            
            // profiler found sscanf is a bottleneck here
            // try atoi (stringToInt) instead
            //sscanf( xToken, "%d", &( m.extraPos[e].x ) );
            //sscanf( yToken, "%d", &( m.extraPos[e].y ) );

            m.extraPos[e].x = stringToInt( xToken );
            m.extraPos[e].y = stringToInt( yToken );
            
            
            if( abs( m.extraPos[e].x ) > pathDeltaMax ||
                abs( m.extraPos[e].y ) > pathDeltaMax ) {
                // path goes too far afield
                
                // terminate it here
                m.numExtraPos = e;
                
                if( e == 0 ) {
                    delete [] m.extraPos;
                    m.extraPos = NULL;
                    m.numExtraPos = 0;
                    m.type = UNKNOWN;
                    delete tokens;
                    return m;
                    }
                break;
                }
                

            // make them absolute
            m.extraPos[e].x += m.x;
            m.extraPos[e].y += m.y;
            }
        
        delete tokens;
        }
    else if( strcmp( nameBuffer, "JUMP" ) == 0 ) {
        m.type = JUMP;
        }
    else if( strcmp( nameBuffer, "DIE" ) == 0 ) {
        m.type = DIE;
        }
    else if( strcmp( nameBuffer, "GRAVE" ) == 0 ) {
        m.type = GRAVE;
        }
    else if( strcmp( nameBuffer, "OWNER" ) == 0 ) {
        m.type = OWNER;
        }
    else if( strcmp( nameBuffer, "FORCE" ) == 0 ) {
        m.type = FORCE;
        }
    else if( strcmp( nameBuffer, "USE" ) == 0 ) {
        m.type = USE;
        // read optional id parameter
        numRead = sscanf( inMessage, 
                          "%99s %d %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.id ), &( m.i ) );
        
        if( numRead < 5 ) {
            m.i = -1;
            }
        if( numRead < 4 ) {
            m.id = -1;
            }
        }
    else if( strcmp( nameBuffer, "SELF" ) == 0 ) {
        m.type = SELF;

        numRead = sscanf( inMessage, 
                          "%99s %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.i ) );
        
        if( numRead != 4 ) {
            m.type = UNKNOWN;
            }
        }
    else if( strcmp( nameBuffer, "UBABY" ) == 0 ) {
        m.type = UBABY;

        // id param optional
        numRead = sscanf( inMessage, 
                          "%99s %d %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.i ), &( m.id ) );
        
        if( numRead != 4 && numRead != 5 ) {
            m.type = UNKNOWN;
            }
        if( numRead != 5 ) {
            m.id = -1;
            }
        }
    else if( strcmp( nameBuffer, "BABY" ) == 0 ) {
        m.type = BABY;
        // read optional id parameter
        numRead = sscanf( inMessage, 
                          "%99s %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.id ) );
        
        if( numRead != 4 ) {
            m.id = -1;
            }
        }
    else if( strcmp( nameBuffer, "PING" ) == 0 ) {
        m.type = PING;
        // read unique id parameter
        numRead = sscanf( inMessage, 
                          "%99s %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.id ) );
        
        if( numRead != 4 ) {
            m.id = 0;
            }
        }
    else if( strcmp( nameBuffer, "SREMV" ) == 0 ) {
        m.type = SREMV;
        
        numRead = sscanf( inMessage, 
                          "%99s %d %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.c ),
                          &( m.i ) );
        
        if( numRead != 5 ) {
            m.type = UNKNOWN;
            }
        }
    else if( strcmp( nameBuffer, "REMV" ) == 0 ) {
        m.type = REMV;
        
        numRead = sscanf( inMessage, 
                          "%99s %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.i ) );
        
        if( numRead != 4 ) {
            m.type = UNKNOWN;
            }
        }
    else if( strcmp( nameBuffer, "DROP" ) == 0 ) {
        m.type = DROP;
        numRead = sscanf( inMessage, 
                          "%99s %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.c ) );
        
        if( numRead != 4 ) {
            m.type = UNKNOWN;
            }
        }
    else if( strcmp( nameBuffer, "KILL" ) == 0 ) {
        m.type = KILL;
        
        // read optional id parameter
        numRead = sscanf( inMessage, 
                          "%99s %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.id ) );
        
        if( numRead != 4 ) {
            m.id = -1;
            }
        }
    else if( strcmp( nameBuffer, "MAP" ) == 0 ) {
        m.type = MAP;
        }
    else if( strcmp( nameBuffer, "SAY" ) == 0 ) {
        m.type = SAY;

        // look after second space
        char *firstSpace = strstr( inMessage, " " );
        
        if( firstSpace != NULL ) {
            
            char *secondSpace = strstr( &( firstSpace[1] ), " " );
            
            if( secondSpace != NULL ) {

                char *thirdSpace = strstr( &( secondSpace[1] ), " " );
                
                if( thirdSpace != NULL ) {
                    m.saidText = stringDuplicate( &( thirdSpace[1] ) );
                    }
                }
            }
        }
    else if( strcmp( nameBuffer, "EMOT" ) == 0 ) {
        m.type = EMOT;

        numRead = sscanf( inMessage, 
                          "%99s %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.i ) );
        
        if( numRead != 4 ) {
            m.type = UNKNOWN;
            }
        }
    else if( strcmp( nameBuffer, "VOGS" ) == 0 ) {
        m.type = VOGS;
        }
    else if( strcmp( nameBuffer, "VOGN" ) == 0 ) {
        m.type = VOGN;
        }
    else if( strcmp( nameBuffer, "VOGP" ) == 0 ) {
        m.type = VOGP;
        }
    else if( strcmp( nameBuffer, "VOGM" ) == 0 ) {
        m.type = VOGM;
        }
    else if( strcmp( nameBuffer, "VOGI" ) == 0 ) {
        m.type = VOGI;
        numRead = sscanf( inMessage, 
                          "%99s %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.id ) );
        
        if( numRead != 4 ) {
            m.id = -1;
            }
        }
    else if( strcmp( nameBuffer, "VOGT" ) == 0 ) {
        m.type = VOGT;

        // look after second space
        char *firstSpace = strstr( inMessage, " " );
        
        if( firstSpace != NULL ) {
            
            char *secondSpace = strstr( &( firstSpace[1] ), " " );
            
            if( secondSpace != NULL ) {

                char *thirdSpace = strstr( &( secondSpace[1] ), " " );
                
                if( thirdSpace != NULL ) {
                    m.saidText = stringDuplicate( &( thirdSpace[1] ) );
                    }
                }
            }
        }
    else if( strcmp( nameBuffer, "VOGX" ) == 0 ) {
        m.type = VOGX;
        }
   else if( strcmp( nameBuffer, "PHOTO" ) == 0 ) {
        m.type = PHOTO;
        numRead = sscanf( inMessage, 
                          "%99s %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.id ) );
        
        if( numRead != 4 ) {
            m.id = 0;
            }
        }
    else if( strcmp( nameBuffer, "FLIP" ) == 0 ) {
        m.type = FLIP;
        }
     else {
        m.type = UNKNOWN;
        }
    
    // incoming client messages are relative to birth pos
    // except NOT map pull messages, which are absolute
    if( m.type != MAP ) {    
        m.x += inPlayer->birthPos.x;
        m.y += inPlayer->birthPos.y;

        for( int i=0; i<m.numExtraPos; i++ ) {
            m.extraPos[i].x += inPlayer->birthPos.x;
            m.extraPos[i].y += inPlayer->birthPos.y;
            }
        }

    return m;
    }



// compute closest starting position part way along
// path
// (-1 if closest spot is starting spot not included in path steps)
int computePartialMovePathStep( LiveObject *inPlayer ) {
    
    double fractionDone = 
        ( Time::getCurrentTime() - 
          inPlayer->moveStartTime )
        / inPlayer->moveTotalSeconds;
    
    if( fractionDone > 1 ) {
        fractionDone = 1;
        }
    
    int c = 
        lrint( ( inPlayer->pathLength ) *
               fractionDone );
    return c - 1;
    }



GridPos computePartialMoveSpot( LiveObject *inPlayer ) {

    int c = computePartialMovePathStep( inPlayer );

    if( c >= 0 ) {
        
        GridPos cPos = inPlayer->pathToDest[c];
        
        return cPos;
        }
    else {
        GridPos cPos = { inPlayer->xs, inPlayer->ys };
        
        return cPos;
        }
    }



GridPos getPlayerPos( LiveObject *inPlayer ) {
    if( inPlayer->xs == inPlayer->xd &&
        inPlayer->ys == inPlayer->yd ) {
        
        GridPos cPos = { inPlayer->xs, inPlayer->ys };
        
        return cPos;
        }
    else {
        return computePartialMoveSpot( inPlayer );
        }
    }



GridPos killPlayer( const char *inEmail ) {
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( strcmp( o->email, inEmail ) == 0 ) {
            o->error = true;
            
            return computePartialMoveSpot( o );
            }
        }
    
    GridPos noPos = { 0, 0 };
    return noPos;
    }



void forcePlayerAge( const char *inEmail, double inAge ) {
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( strcmp( o->email, inEmail ) == 0 ) {
            double ageSec = inAge / getAgeRate();
            
            o->lifeStartTimeSeconds = Time::getCurrentTime() - ageSec;
            o->needsUpdate = true;
            }
        }
    }





double computeFoodDecrementTimeSeconds( LiveObject *inPlayer ) {
    
    float baseHeat = inPlayer->heat;
    
    if( inPlayer->tripping ) {
        
        // Increased food drain when tripping
        if( inPlayer->heat >= 0.5 ) {
            baseHeat =  1.0 - (2/3) * ( 1.0 - baseHeat );
            } 
        else {
            baseHeat = (2/3) * baseHeat;
            }
        
        }
    
    
    double value = maxFoodDecrementSeconds * 2 * baseHeat;
    
    if( value > maxFoodDecrementSeconds ) {
        // also reduce if too hot (above 0.5 heat)
        
        double extra = value - maxFoodDecrementSeconds;

        value = maxFoodDecrementSeconds - extra;
        }
    
    // all player temp effects push us up above min
    value += minFoodDecrementSeconds;

    value += inPlayer->personalFoodDecrementSecondsBonus;
    
    // The more you stack the yum bonus, the faster it drains
    // A nerf against extreme bonus stacking that lasts for a whole life
    
    // bonus above this will start to fall off
    float xStart = 80.0;
    if( inPlayer->yummyBonusStore > xStart ) {
        // controls the rate of fall off 
        float xScaling = 1.5;
        float x = (inPlayer->yummyBonusStore - xStart) / xStart * xScaling;
        // y is a fraction
        float y = 1/(x+1);
        // multiplied to the original value
        value = value * y;
        // still obey the min
        value = std::max(value, minFoodDecrementSeconds);
        }

    return value;
    }


double getAgeRate() {
    return 1.0 / secondsPerYear;
    }


static void setDeathReason( LiveObject *inPlayer, const char *inTag,
                            int inOptionalID = 0 ) {
    
    if( inPlayer->deathReason != NULL ) {
        delete [] inPlayer->deathReason;
        }
    
    // leave space in front so it works at end of PU line
    if( strcmp( inTag, "killed" ) == 0 ||
        strcmp( inTag, "succumbed" ) == 0 ||
        strcmp( inTag, "suicide" ) == 0 ) {
        
        inPlayer->deathReason = autoSprintf( " reason_%s_%d", 
                                             inTag, inOptionalID );
        }
    else {
        // ignore ID
        inPlayer->deathReason = autoSprintf( " reason_%s", inTag );
        }
    }



int longestShutdownLine = -1;

void handleShutdownDeath( LiveObject *inPlayer,
                          int inX, int inY ) {
    if( inPlayer->curseStatus.curseLevel == 0 &&
        inPlayer->parentChainLength > longestShutdownLine ) {
        
        // never count a cursed player as a long line
        
        longestShutdownLine = inPlayer->parentChainLength;
        
        FILE *f = fopen( "shutdownLongLineagePos.txt", "w" );
        if( f != NULL ) {
            fprintf( f, "%d,%d", inX, inY );
            fclose( f );
            }
        }
    }



double computeAge( double inLifeStartTimeSeconds ) {
    
    double deltaSeconds = 
        Time::getCurrentTime() - inLifeStartTimeSeconds;
    
    double age = deltaSeconds * getAgeRate();
    
    return age;
    }



double computeAge( LiveObject *inPlayer ) {
    double age = computeAge( inPlayer->lifeStartTimeSeconds );
    if( age >= forceDeathAge ) {
        setDeathReason( inPlayer, "age" );
        
        inPlayer->error = true;
        
        age = forceDeathAge;
        }
    return age;
    }



int getSayLimit( LiveObject *inPlayer ) {
    return getSayLimit( computeAge( inPlayer ) );
    }




int getSecondsPlayed( LiveObject *inPlayer ) {
    double deltaSeconds = 
        Time::getCurrentTime() - inPlayer->trueStartTimeSeconds;

    return lrint( deltaSeconds );
    }


// false for male, true for female
char getFemale( LiveObject *inPlayer ) {
    ObjectRecord *r = getObject( inPlayer->displayID );
    
    return ! r->male;
    }



char isFertileAge( LiveObject *inPlayer ) {
    double age = computeAge( inPlayer );
                    
    char f = getFemale( inPlayer );
                    
    if( age >= fertileAge && age <= oldAge && f ) {
        return true;
        }
    else {
        return false;
        }
    }



static int countYoungFemalesInLineage( int inLineageEveID ) {
    int count = 0;
    
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( o->error ) {
            continue;
            }
        if( o->isTutorial ) {
            continue;
            }    
        if( o->vogMode ) {
            continue;
            }
        if( o->curseStatus.curseLevel > 0 ) {
            continue;
            }
        
        // while this doesn't match up with what this function is called, the way it's used
        // is for counting how many potentially fertile females a lineage has currently
        // for the force baby girl feature
        if( o->declaredInfertile ) {
            continue;
            }
            
        if( o->lineageEveID == inLineageEveID ) {
            double age = computeAge( o );
            char f = getFemale( o );
            if( age <= oldAge && f ) {
                count ++;
                }
            }
            
        }
    return count;
    }




int computeFoodCapacity( LiveObject *inPlayer ) {
    int ageInYears = lrint( computeAge( inPlayer ) );
    int minFoodCap = 3;
    int maxFoodcap = 10;
    int returnVal = 0;
    
    if( ageInYears < oldAge ) {
        
        returnVal = ageInYears + minFoodCap;
        if( returnVal > maxFoodcap ) returnVal = maxFoodcap;
        
        }
    else {
        // food capacity decreases as we near death
        int cap = maxFoodcap - ( ageInYears - oldAge );
        if( cap < minFoodCap ) cap = minFoodCap;
        
        int lostBars = maxFoodcap - cap;

        if( lostBars > 0 && inPlayer->fitnessScore > 0 ) {
        
            // consider effect of fitness on reducing lost bars

            // for now, let's make it quadratic
            double maxLostBars = 
                (maxFoodcap - minFoodCap) * (1 - pow( inPlayer->fitnessScore / 60.0, 2 ));
            
            if( lostBars > maxLostBars ) {
                lostBars = maxLostBars;
                }
            }
        
        returnVal = maxFoodcap - lostBars;
        }

    return ceil( returnVal * inPlayer->foodCapModifier );
    }



int computeOverflowFoodCapacity( int inBaseCapacity ) {
    // even littlest baby has +2 overflow, to get everyone used to the
    // concept.
    // by adulthood (when base cap is 20), overflow cap is 91.6
    return 2 + pow( inBaseCapacity, 8 ) * 0.0000000035;
    }



char *slurSpeech( int inSpeakerID,
                  char *inTranslatedPhrase, double inDrunkenness ) {
    char *working = stringDuplicate( inTranslatedPhrase );
    
    char *starPos = strstr( working, " *" );

    char *extraData = NULL;
    
    if( starPos != NULL ) {
        extraData = stringDuplicate( starPos );
        starPos[0] = '\0';
        }
    
    SimpleVector<char> slurredChars;
    
    // 1 in 10 letters slurred with 1 drunkenness
    // all characters slurred with 10 drunkenness
    double baseSlurChance = 0.1;
    
    double slurChance = baseSlurChance * inDrunkenness;

    // 2 in 10 words mixed up in order with 6 drunkenness
    // all words mixed up at 10 drunkenness
    double baseWordSwapChance = 0.1;

    // but don't start mixing up words at all until 6 drunkenness
    // thus, the 0 to 100% mix up range is from 6 to 10 drunkenness
    double wordSwapChance = 2 * baseWordSwapChance * ( inDrunkenness - 5 );



    // first, swap word order
    SimpleVector<char *> *words = tokenizeString( working );

    // always slurr exactly the same for a given speaker
    // repeating the same phrase won't keep remapping
    // but map different length phrases differently
    JenkinsRandomSource slurRand( inSpeakerID + 
                                  words->size() + 
                                  inDrunkenness );
    

    for( int i=0; i<words->size(); i++ ) {
        if( slurRand.getRandomBoundedDouble( 0, 1 ) < wordSwapChance ) {
            char *temp = words->getElementDirect( i );
            
            // possible swap distance based on drunkenness
            
            // again, don't start reording words until 6 drunkenness
            int maxDist = inDrunkenness - 5;

            if( maxDist >= words->size() - i ) {
                maxDist = words->size() - i - 1;
                }
            
            if( maxDist > 0 ) {
                int jump = slurRand.getRandomBoundedInt( 0, maxDist );
            
                
                *( words->getElement( i ) ) = 
                    words->getElementDirect( i + jump );
            
                *( words->getElement( i + jump ) ) = temp;
                }
            }
        }
    

    char **allWords = words->getElementArray();
    char *wordsTogether = join( allWords, words->size(), " " );
    
    words->deallocateStringElements();
    delete words;
    
    delete [] allWords;

    delete [] working;
    
    working = wordsTogether;


    int len = strlen( working );
    for( int i=0; i<len; i++ ) {
        char c = working[i];
        
        slurredChars.push_back( c );

        if( c < 'A' || c > 'Z' ) {
            // only A-Z, no slurred punctuation
            continue;
            }

        if( slurRand.getRandomBoundedDouble( 0, 1 ) < slurChance ) {
            slurredChars.push_back( c );
            }
        }

    delete [] working;
    
    if( extraData != NULL ) {
        slurredChars.appendElementString( extraData );
        delete [] extraData;
        }
    

    return slurredChars.getElementString();
    }


char *yellingSpeech( int inSpeakerID,
                  char *inTranslatedPhrase ) {
    char *working = stringDuplicate( inTranslatedPhrase );
    
    char *starPos = strstr( working, " *" );

    char *extraData = NULL;
    
    if( starPos != NULL ) {
        extraData = stringDuplicate( starPos );
        starPos[0] = '\0';
        }
    
    SimpleVector<char> workedChars;

    int len = strlen( working );
    for( int i=0; i<len; i++ ) {
        char c = working[i];
        
        char r;

        if( c == 'A' ) {
            r = c;
            }
        else if( c == 'E' ) {
            r = c;
            }
        else if( c == 'O' ) {
            r = c;
            }
        else if( c == 'Y' ) {
            r = c;
            }
        else if( c == ',' || c == '.' || c == '!' ) {
            r = '!';
            }
        else {
            workedChars.push_back( c );
            continue;
            }
            

        for(int i = 0; i < 5; i++) {
            workedChars.push_back( r );
            }
        }

    delete [] working;
    
    if( len > 0 ) {
        int repeatLen = randSource.getRandomBoundedDouble( 0, 1 ) * 4 + 1;
        for(int i = 0; i < repeatLen; i++) {
            workedChars.push_back( '!' );
            }
        }
    
    if( extraData != NULL ) {
        workedChars.appendElementString( extraData );
        delete [] extraData;
        }
    

    return workedChars.getElementString();
    }



// with 128-wide tiles, character moves at 480 pixels per second
// at 60 fps, this is 8 pixels per frame
// important that it's a whole number of pixels for smooth camera movement
static double baseWalkSpeed = 3.75;

// min speed for takeoff
static double minFlightSpeed = 15;



double computeMoveSpeed( LiveObject *inPlayer ) {
    double age = computeAge( inPlayer );
    

    double speed = baseWalkSpeed;
    
    // baby moves at 360 pixels per second, or 6 pixels per frame
    double babySpeedFactor = 0.75;

    double fullSpeedAge = 10.0;
    

    if( age < fullSpeedAge ) {
        
        double speedFactor = babySpeedFactor + 
            ( 1.0 - babySpeedFactor ) * age / fullSpeedAge;
        
        speed *= speedFactor;
        }


    // for now, try no age-based speed decrease
    /*
    if( age < 20 ) {
        speed *= age / 20;
        }
    if( age > 40 ) {
        // half speed by 60, then keep slowing down after that
        speed -= (age - 40 ) * 2.0 / 20.0;
        
        }
    */
    // no longer slow down with hunger
    /*
    int foodCap = computeFoodCapacity( inPlayer );
    
    
    if( inPlayer->foodStore <= foodCap / 2 ) {
        // jumps instantly to 1/2 speed at half food, then decays after that
        speed *= inPlayer->foodStore / (double) foodCap;
        }
    */



    // apply character's speed mult
    speed *= getObject( inPlayer->displayID )->speedMult;
    

    char riding = false;
    
    if( inPlayer->holdingID > 0 ) {
        ObjectRecord *r = getObject( inPlayer->holdingID );

        if( r->clothing == 'n' ) {
            // clothing only changes your speed when it's worn
            speed *= r->speedMult;
            }
        
        if( r->rideable ) {
            riding = true;
            }
        }
    

    if( !riding ) {
        // clothing can affect speed

        for( int i=0; i<NUM_CLOTHING_PIECES; i++ ) {
            ObjectRecord *c = clothingByIndex( inPlayer->clothing, i );
            
            if( c != NULL ) {
                
                speed *= c->speedMult;
                }
            }
            
        if( inPlayer->tripping ) {
            speed *= 1.2;
            }
        else if( inPlayer->drunkennessEffect ) {
            speed *= 0.9;
            }
        }

    // never move at 0 speed, divide by 0 errors for eta times
    if( speed < 0.01 ) {
        speed = 0.01;
        }

    
    // after all multipliers, make sure it's a whole number of pixels per frame

    double pixelsPerFrame = speed * 128.0 / 60.0;
    
    
    if( pixelsPerFrame > 0.5 ) {
        // can round to at least one pixel per frame
        pixelsPerFrame = lrint( pixelsPerFrame );
        }
    else {
        // fractional pixels per frame
        
        // ensure a whole number of frames per pixel
        double framesPerPixel = 1.0 / pixelsPerFrame;
        
        framesPerPixel = lrint( framesPerPixel );
        
        pixelsPerFrame = 1.0 / framesPerPixel;
        }
    
    speed = pixelsPerFrame * 60 / 128.0;
        
    return speed;
    }







static float sign( float inF ) {
    if (inF > 0) return 1;
    if (inF < 0) return -1;
    return 0;
    }


// how often do we check what a player is standing on top of for attack effects?
static double playerCrossingCheckStepTime = 0.25;


// for steps in main loop that shouldn't happen every loop
// (loop runs faster or slower depending on how many messages are incoming)
static double periodicStepTime = 0.25;
static double lastPeriodicStepTime = 0;




// recompute heat for fixed number of players per timestep
static int numPlayersRecomputeHeatPerStep = 8;
static int lastPlayerIndexHeatRecomputed = -1;
static double lastHeatUpdateTime = 0;
static double heatUpdateTimeStep = 0.1;


// how often the player's personal heat advances toward their environmental
// heat value
static double heatUpdateSeconds = 2;


// air itself offers some insulation
// a vacuum panel has R-value that is 25x greater than air
static float rAir = 0.04;



// blend R-values multiplicatively, for layers
// 1 - R( A + B ) = (1 - R(A)) * (1 - R(B))
//
// or
//
//R( A + B ) =  R(A) + R(B) - R(A) * R(B)
static double rCombine( double inRA, double inRB ) {
    return inRA + inRB - inRA * inRB;
    }




static float computeClothingR( LiveObject *inPlayer ) {
    
    float headWeight = 0.25;
    float chestWeight = 0.35;
    float buttWeight = 0.2;
    float eachFootWeigth = 0.1;
            
    float backWeight = 0.1;


    float clothingR = 0;
            
    if( inPlayer->clothing.hat != NULL ) {
        clothingR += headWeight *  inPlayer->clothing.hat->rValue;
        }
    if( inPlayer->clothing.tunic != NULL ) {
        clothingR += chestWeight * inPlayer->clothing.tunic->rValue;
        }
    if( inPlayer->clothing.frontShoe != NULL ) {
        clothingR += 
            eachFootWeigth * inPlayer->clothing.frontShoe->rValue;
        }
    if( inPlayer->clothing.backShoe != NULL ) {
        clothingR += eachFootWeigth * 
            inPlayer->clothing.backShoe->rValue;
        }
    if( inPlayer->clothing.bottom != NULL ) {
        clothingR += buttWeight * inPlayer->clothing.bottom->rValue;
        }
    if( inPlayer->clothing.backpack != NULL ) {
        clothingR += backWeight * inPlayer->clothing.backpack->rValue;
        }
    
    // even if the player is naked, they are insulated from their
    // environment by rAir
    return rCombine( rAir, clothingR );
    }



static float computeClothingHeat( LiveObject *inPlayer ) {
    // clothing can contribute heat
    // apply this separate from heat grid above
    float clothingHeat = 0;
    for( int c=0; c<NUM_CLOTHING_PIECES; c++ ) {
                
        ObjectRecord *cO = clothingByIndex( inPlayer->clothing, c );
            
        if( cO != NULL ) {
            clothingHeat += cO->heatValue;

            // contained items in clothing can contribute
            // heat, shielded by clothing r-values
            double cRFactor = 1 - cO->rValue;

            for( int s=0; 
                 s < inPlayer->clothingContained[c].size(); s++ ) {
                        
                ObjectRecord *sO = 
                    getObject( inPlayer->clothingContained[c].
                               getElementDirect( s ) );
                        
                clothingHeat += 
                    sO->heatValue * cRFactor;
                }
            }
        }
    return clothingHeat;
    }



static float computeHeldHeat( LiveObject *inPlayer ) {
    float heat = 0;
    
    // what player is holding can contribute heat
    // add this to the grid, since it's "outside" the player's body
    if( inPlayer->holdingID > 0 ) {
        ObjectRecord *heldO = getObject( inPlayer->holdingID );
                
        heat += heldO->heatValue;
                
        double heldRFactor = 1 - heldO->rValue;
                
        // contained can contribute too, but shielded by r-value
        // of container
        for( int c=0; c<inPlayer->numContained; c++ ) {
                    
            int cID = inPlayer->containedIDs[c];
            char hasSub = false;
                    
            if( cID < 0 ) {
                hasSub = true;
                cID = -cID;
                }

            ObjectRecord *contO = getObject( cID );
                    
            heat += 
                contO->heatValue * heldRFactor;
                    

            if( hasSub ) {
                // sub contained too, but shielded by both r-values
                double contRFactor = 1 - contO->rValue;

                for( int s=0; 
                     s<inPlayer->subContainedIDs[c].size(); s++ ) {
                        
                    ObjectRecord *subO =
                        getObject( inPlayer->subContainedIDs[c].
                                   getElementDirect( s ) );
                            
                    heat += 
                        subO->heatValue * 
                        contRFactor * heldRFactor;
                    }
                }
            }
        }
    return heat;
    }




static void recomputeHeatMap( LiveObject *inPlayer ) {
    
    int gridSize = HEAT_MAP_D * HEAT_MAP_D;

    // assume indoors until we find an air boundary of space
    inPlayer->isIndoors = true;
    

    // what if we recompute it from scratch every time?
    for( int i=0; i<gridSize; i++ ) {
        inPlayer->heatMap[i] = 0;
        }

    float heatOutputGrid[ HEAT_MAP_D * HEAT_MAP_D ];
    float rGrid[ HEAT_MAP_D * HEAT_MAP_D ];
    float rFloorGrid[ HEAT_MAP_D * HEAT_MAP_D ];


    GridPos pos = getPlayerPos( inPlayer );


    // held baby's pos matches parent pos
    if( inPlayer->heldByOther ) {
        LiveObject *parentObject = getLiveObject( inPlayer->heldByOtherID );
        
        if( parentObject != NULL ) {
            pos = getPlayerPos( parentObject );
            }
        } 

    
    

    for( int y=0; y<HEAT_MAP_D; y++ ) {
        int mapY = pos.y + y - HEAT_MAP_D / 2;
                
        for( int x=0; x<HEAT_MAP_D; x++ ) {
                    
            int mapX = pos.x + x - HEAT_MAP_D / 2;
                    
            int j = y * HEAT_MAP_D + x;
            heatOutputGrid[j] = 0;
            rGrid[j] = rAir;
            rFloorGrid[j] = rAir;


            // call Raw version for better performance
            // we don't care if object decayed since we last looked at it
            ObjectRecord *o = getObject( getMapObjectRaw( mapX, mapY ) );
                    
                    
                    

            if( o != NULL ) {
                heatOutputGrid[j] += o->heatValue;
                if( o->permanent ) {
                    // loose objects sitting on ground don't
                    // contribute to r-value (like dropped clothing)
                    rGrid[j] = rCombine( rGrid[j], o->rValue );
                    }


                // skip checking for heat-producing contained items
                // for now.  Consumes too many server-side resources
                // can still check for heat produced by stuff in
                // held container (below).
                        
                if( false && o->numSlots > 0 ) {
                    // contained can produce heat shielded by container
                    // r value
                    double oRFactor = 1 - o->rValue;
                            
                    int numCont;
                    int *cont = getContained( mapX, mapY, &numCont );
                            
                    if( cont != NULL ) {
                                
                        for( int c=0; c<numCont; c++ ) {
                                    
                            int cID = cont[c];
                            char hasSub = false;
                            if( cID < 0 ) {
                                hasSub = true;
                                cID = -cID;
                                }

                            ObjectRecord *cO = getObject( cID );
                            heatOutputGrid[j] += 
                                cO->heatValue * oRFactor;
                                    
                            if( hasSub ) {
                                double cRFactor = 1 - cO->rValue;
                                        
                                int numSub;
                                int *sub = getContained( mapX, mapY, 
                                                         &numSub, 
                                                         c + 1 );
                                if( sub != NULL ) {
                                    for( int s=0; s<numSub; s++ ) {
                                        ObjectRecord *sO = 
                                            getObject( sub[s] );
                                                
                                        heatOutputGrid[j] += 
                                            sO->heatValue * 
                                            cRFactor * 
                                            oRFactor;
                                        }
                                    delete [] sub;
                                    }
                                }
                            }
                        delete [] cont;
                        }
                    }
                }
                    

            // floor can insulate or produce heat too
            ObjectRecord *fO = getObject( getMapFloor( mapX, mapY ) );
                    
            if( fO != NULL ) {
                heatOutputGrid[j] += fO->heatValue;
                rFloorGrid[j] = rCombine( rFloorGrid[j], fO->rValue );
                }
            }
        }


    
    int numNeighbors = 8;
    int ndx[8] = { 0, 1,  0, -1,  1,  1, -1, -1 };
    int ndy[8] = { 1, 0, -1,  0,  1, -1,  1, -1 };
    
            
    int playerMapIndex = 
        ( HEAT_MAP_D / 2 ) * HEAT_MAP_D +
        ( HEAT_MAP_D / 2 );
        

    
            
    heatOutputGrid[ playerMapIndex ] += computeHeldHeat( inPlayer );
    

    // grid of flags for points that are in same airspace (surrounded by walls)
    // as player
    // This is the area where heat spreads evenly by convection
    char airSpaceGrid[ HEAT_MAP_D * HEAT_MAP_D ];
    
    memset( airSpaceGrid, false, HEAT_MAP_D * HEAT_MAP_D );
    
    airSpaceGrid[ playerMapIndex ] = true;

    SimpleVector<int> frontierA;
    SimpleVector<int> frontierB;
    frontierA.push_back( playerMapIndex );
    
    SimpleVector<int> *thisFrontier = &frontierA;
    SimpleVector<int> *nextFrontier = &frontierB;

    while( thisFrontier->size() > 0 ) {

        for( int f=0; f<thisFrontier->size(); f++ ) {
            
            int i = thisFrontier->getElementDirect( f );
            
            char negativeYCutoff = false;
            
            if( rGrid[i] > rAir ) {
                // grid cell is insulating, and somehow it's in our
                // frontier.  Player must be standing behind a closed
                // door.  Block neighbors to south
                negativeYCutoff = true;
                }
            

            int x = i % HEAT_MAP_D;
            int y = i / HEAT_MAP_D;
            
            for( int n=0; n<numNeighbors; n++ ) {
                        
                int nx = x + ndx[n];
                int ny = y + ndy[n];
                
                if( negativeYCutoff && ndy[n] < 1 ) {
                    continue;
                    }

                if( nx >= 0 && nx < HEAT_MAP_D &&
                    ny >= 0 && ny < HEAT_MAP_D ) {

                    int nj = ny * HEAT_MAP_D + nx;
                    
                    if( ! airSpaceGrid[ nj ]
                        && rGrid[ nj ] <= rAir ) {

                        airSpaceGrid[ nj ] = true;
                        
                        nextFrontier->push_back( nj );
                        }
                    }
                }
            }
        
        thisFrontier->deleteAll();
        
        SimpleVector<int> *temp = thisFrontier;
        thisFrontier = nextFrontier;
        
        nextFrontier = temp;
        }
    
    if( rGrid[playerMapIndex] > rAir ) {
        // player standing in insulated spot
        // don't count this as part of their air space
        airSpaceGrid[ playerMapIndex ] = false;
        }

    int numInAirspace = 0;
    for( int i=0; i<gridSize; i++ ) {
        if( airSpaceGrid[ i ] ) {
            numInAirspace++;
            }
        }
    
    
    float rBoundarySum = 0;
    int rBoundarySize = 0;
    
    for( int i=0; i<gridSize; i++ ) {
        if( airSpaceGrid[i] ) {
            
            int x = i % HEAT_MAP_D;
            int y = i / HEAT_MAP_D;
            
            for( int n=0; n<numNeighbors; n++ ) {
                        
                int nx = x + ndx[n];
                int ny = y + ndy[n];
                
                if( nx >= 0 && nx < HEAT_MAP_D &&
                    ny >= 0 && ny < HEAT_MAP_D ) {
                    
                    int nj = ny * HEAT_MAP_D + nx;
                    
                    if( ! airSpaceGrid[ nj ] ) {
                        
                        // boundary!
                        rBoundarySum += rGrid[ nj ];
                        rBoundarySize ++;
                        }
                    }
                else {
                    // boundary is off edge
                    // assume air R-value
                    rBoundarySum += rAir;
                    rBoundarySize ++;
                    inPlayer->isIndoors = false;
                    }
                }
            }
        }

    
    // floor counts as boundary too
    // 4x its effect (seems more important than one of 8 walls
    
    // count non-air floor tiles while we're at it
    int numFloorTilesInAirspace = 0;

    if( numInAirspace > 0 ) {
        for( int i=0; i<gridSize; i++ ) {
            if( airSpaceGrid[i] ) {
                rBoundarySum += 4 * rFloorGrid[i];
                rBoundarySize += 4;
                
                if( rFloorGrid[i] > rAir ) {
                    numFloorTilesInAirspace++;
                    }
                else {
                    // gap in floor
                    inPlayer->isIndoors = false;
                    }
                }
            }
        }
    


    float rBoundaryAverage = rAir;
    
    if( rBoundarySize > 0 ) {
        rBoundaryAverage = rBoundarySum / rBoundarySize;
        }

    
    



    float airSpaceHeatSum = 0;
    
    for( int i=0; i<gridSize; i++ ) {
        if( airSpaceGrid[i] ) {
            airSpaceHeatSum += heatOutputGrid[i];
            }
        }


    float airSpaceHeatVal = 0;
    
    if( numInAirspace > 0 ) {
        // spread heat evenly over airspace
        airSpaceHeatVal = airSpaceHeatSum / numInAirspace;
        }

    float containedAirSpaceHeatVal = airSpaceHeatVal * rBoundaryAverage;
    


    float radiantAirSpaceHeatVal = 0;

    GridPos playerHeatMapPos = { playerMapIndex % HEAT_MAP_D, 
                                 playerMapIndex / HEAT_MAP_D };
    

    int numRadiantHeatSources = 0;
    
    for( int i=0; i<gridSize; i++ ) {
        if( airSpaceGrid[i] && heatOutputGrid[i] > 0 ) {
            
            int x = i % HEAT_MAP_D;
            int y = i / HEAT_MAP_D;
            
            GridPos heatPos = { x, y };
            

            double d = distance( playerHeatMapPos, heatPos );
            
            // avoid infinite heat when player standing on source

            radiantAirSpaceHeatVal += heatOutputGrid[ i ] / ( 1.5 * d + 1 );
            numRadiantHeatSources ++;
            }
        }
    

    float biomeHeatWeight = 1;
    float radiantHeatWeight = 1;
    
    float containedHeatWeight = 4;


    // boundary r-value also limits affect of biome heat on player's
    // environment... keeps biome "out"
    float boundaryLeak = 1 - rBoundaryAverage;

    if( numFloorTilesInAirspace != numInAirspace ) {
        // biome heat invades airspace if entire thing isn't covered by
        // a floor (not really indoors)
        boundaryLeak = 1;
        }


    // a hot biome only pulls us up above perfect
    // (hot biome leaking into a building can never make the building
    //  just right).
    // Enclosed walls can make a hot biome not as hot, but never cool
    float biomeHeat = getBiomeHeatValue( getMapBiome( pos.x, pos.y ) );
    
    if( biomeHeat > targetHeat ) {
        biomeHeat = boundaryLeak * (biomeHeat - targetHeat) + targetHeat;
        }
    else if( biomeHeat < 0 ) {
        // a cold biome's coldness is modulated directly by walls, however
        biomeHeat = boundaryLeak * biomeHeat;
        }
    
    // small offset to ensure that naked-on-green biome the same
    // in new heat model as old
    float constHeatValue = 1.1;

    inPlayer->envHeat = 
        radiantHeatWeight * radiantAirSpaceHeatVal + 
        containedHeatWeight * containedAirSpaceHeatVal +
        biomeHeatWeight * biomeHeat +
        constHeatValue;

    inPlayer->biomeHeat = biomeHeat + constHeatValue;
    }




typedef struct MoveRecord {
        int playerID;
        char *formatString;
        int absoluteX, absoluteY;
    } MoveRecord;



// formatString in returned record destroyed by caller
MoveRecord getMoveRecord( LiveObject *inPlayer,
                          char inNewMovesOnly,
                          SimpleVector<ChangePosition> *inChangeVector = 
                          NULL ) {

    MoveRecord r;
    r.playerID = inPlayer->id;
    
    // p_id xs ys xd yd fraction_done eta_sec
    
    double deltaSec = Time::getCurrentTime() - inPlayer->moveStartTime;
    
    double etaSec = inPlayer->moveTotalSeconds - deltaSec;
    
    if( etaSec < 0 ) {
        etaSec = 0;
        }

    
    r.absoluteX = inPlayer->xs;
    r.absoluteY = inPlayer->ys;
            
            
    SimpleVector<char> messageLineBuffer;
    
    // start is absolute
    char *startString = autoSprintf( "%d %%d %%d %.3f %.3f %d", 
                                     inPlayer->id, 
                                     inPlayer->moveTotalSeconds, etaSec,
                                     inPlayer->pathTruncated );
    // mark that this has been sent
    inPlayer->pathTruncated = false;

    if( inNewMovesOnly ) {
        inPlayer->newMove = false;
        }

            
    messageLineBuffer.appendElementString( startString );
    delete [] startString;
            
    for( int p=0; p<inPlayer->pathLength; p++ ) {
                // rest are relative to start
        char *stepString = autoSprintf( " %d %d", 
                                        inPlayer->pathToDest[p].x
                                        - inPlayer->xs,
                                        inPlayer->pathToDest[p].y
                                        - inPlayer->ys );
        
        messageLineBuffer.appendElementString( stepString );
        delete [] stepString;
        }
    
    messageLineBuffer.appendElementString( "\n" );
    
    r.formatString = messageLineBuffer.getElementString();    
    
    if( inChangeVector != NULL ) {
        ChangePosition p = { inPlayer->xd, inPlayer->yd, false };
        inChangeVector->push_back( p );
        }

    return r;
    }



SimpleVector<MoveRecord> getMoveRecords( 
    char inNewMovesOnly,
    SimpleVector<ChangePosition> *inChangeVector = NULL ) {
    
    SimpleVector<MoveRecord> v;
    
    int numPlayers = players.size();
                
    for( int i=0; i<numPlayers; i++ ) {
                
        LiveObject *o = players.getElement( i );                
        
        if( o->error ) {
            continue;
            }

        if( ( o->xd != o->xs || o->yd != o->ys )
            &&
            ( o->newMove || !inNewMovesOnly ) ) {
            
 
            MoveRecord r = getMoveRecord( o, inNewMovesOnly, inChangeVector );
            
            v.push_back( r );
            }
        }

    return v;
    }



char *getMovesMessageFromList( SimpleVector<MoveRecord> *inMoves,
                               GridPos inRelativeToPos ) {

    int numLines = 0;
    
    SimpleVector<char> messageBuffer;

    messageBuffer.appendElementString( "PM\n" );

    for( int i=0; i<inMoves->size(); i++ ) {
        MoveRecord r = inMoves->getElementDirect(i);
        
        char *line = autoSprintf( r.formatString, 
                                  r.absoluteX - inRelativeToPos.x,
                                  r.absoluteY - inRelativeToPos.y );
        
        messageBuffer.appendElementString( line );
        delete [] line;
        
        numLines ++;
        }
    
    if( numLines > 0 ) {
        
        messageBuffer.push_back( '#' );
                
        char *message = messageBuffer.getElementString();
        
        return message;
        }
    
    return NULL;
    }



double intDist( int inXA, int inYA, int inXB, int inYB ) {
    double dx = (double)inXA - (double)inXB;
    double dy = (double)inYA - (double)inYB;

    return sqrt(  dx * dx + dy * dy );
    }
    
    
    
// returns NULL if there are no matching moves
// positions in moves relative to inRelativeToPos
// filters out moves that are taking place further than 32 away from inLocalPos
char *getMovesMessage( char inNewMovesOnly,
                       GridPos inRelativeToPos,
                       GridPos inLocalPos,
                       SimpleVector<ChangePosition> *inChangeVector = NULL ) {
    
    
    SimpleVector<MoveRecord> v = getMoveRecords( inNewMovesOnly, 
                                                 inChangeVector );
    
    SimpleVector<MoveRecord> closeRecords;

    for( int i=0; i<v.size(); i++ ) {
        MoveRecord r = v.getElementDirect( i );
        
        double d = intDist( r.absoluteX, r.absoluteY,
                            inLocalPos.x, inLocalPos.y );
        
        if( d <= 32 ) {
            closeRecords.push_back( r );
            }
        }
    
    

    char *message = getMovesMessageFromList( &closeRecords, inRelativeToPos );
    
    for( int i=0; i<v.size(); i++ ) {
        delete [] v.getElement(i)->formatString;
        }
    
    return message;
    }



static char isGridAdjacent( int inXA, int inYA, int inXB, int inYB ) {
    if( ( abs( inXA - inXB ) == 1 && inYA == inYB ) 
        ||
        ( abs( inYA - inYB ) == 1 && inXA == inXB ) ) {
        
        return true;
        }

    return false;
    }


//static char isGridAdjacent( GridPos inA, GridPos inB ) {
//    return isGridAdjacent( inA.x, inA.y, inB.x, inB.y );
//    }


static char isGridAdjacentDiag( int inXA, int inYA, int inXB, int inYB ) {
    if( isGridAdjacent( inXA, inYA, inXB, inYB ) ) {
        return true;
        }
    
    if( abs( inXA - inXB ) == 1 && abs( inYA - inYB ) == 1 ) {
        return true;
        }
    
    return false;
    }


static char isGridAdjacentDiag( GridPos inA, GridPos inB ) {
    return isGridAdjacentDiag( inA.x, inA.y, inB.x, inB.y );
    }



static char equal( GridPos inA, GridPos inB ) {
    if( inA.x == inB.x && inA.y == inB.y ) {
        return true;
        }
    return false;
    }





// returns (0,0) if no player found
GridPos getClosestPlayerPos( int inX, int inY ) {
    GridPos c = { inX, inY };
    
    double closeDist = DBL_MAX;
    GridPos closeP = { 0, 0 };
    
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        if( o->error ) {
            continue;
            }
        if( o->heldByOther ) {
            continue;
            }
        
        GridPos p;

        if( o->xs == o->xd && o->ys == o->yd ) {
            p.x = o->xd;
            p.y = o->yd;
            }
        else {
            p = computePartialMoveSpot( o );
            }
        
        double d = distance( p, c );
        
        if( d < closeDist ) {
            closeDist = d;
            closeP = p;
            }
        }
    return closeP;
    }



#define MAP_D 64
#define MAX_MAP_D 64
static int maxChunkDimensionX = 32;
static int maxChunkDimensionY = 30;


static int getMaxChunkDimension() {
    return maxChunkDimensionX;
    }


static SocketPoll sockPoll;



static void setPlayerDisconnected( LiveObject *inPlayer, 
                                   const char *inReason, const char* func, int line ) {    
    /*
    setDeathReason( inPlayer, "disconnected" );
    
    inPlayer->error = true;
    inPlayer->errorCauseString = inReason;
    */
    // don't kill them
    
    // just mark them as not connected

    AppLog::infoF( "Player %d (%s) marked as disconnected (%s) in func (%s:%d)",
                   inPlayer->id, inPlayer->email, inReason, func, line );
    inPlayer->connected = false;

    // when player reconnects, they won't get a force PU message
    // so we shouldn't be waiting for them to ack
    inPlayer->waitingForForceResponse = false;


    if( inPlayer->vogMode ) {    
        inPlayer->vogMode = false;
                        
        GridPos p = inPlayer->preVogPos;
        
        inPlayer->xd = p.x;
        inPlayer->yd = p.y;
        
        inPlayer->xs = p.x;
        inPlayer->ys = p.y;

        inPlayer->birthPos = inPlayer->preVogBirthPos;
        }
    
    
    if( inPlayer->sock != NULL ) {
        // also, stop polling their socket, which will trigger constant
        // socket events from here on out, and cause us to busy-loop
        sockPoll.removeSocket( inPlayer->sock );

        delete inPlayer->sock;
        inPlayer->sock = NULL;
        }
    if( inPlayer->sockBuffer != NULL ) {
        delete inPlayer->sockBuffer;
        inPlayer->sockBuffer = NULL;
        }
    }



// if inOnePlayerOnly set, we only send to that player
void sendGlobalMessage( char *inMessage,
                        LiveObject *inOnePlayerOnly ) {
    
    double curTime = Time::getCurrentTime();
    
    char found;
    char *noSpaceMessage = replaceAll( inMessage, " ", "_", &found );

    char *fullMessage = autoSprintf( "MS\n%s\n#", noSpaceMessage );
    
    delete [] noSpaceMessage;

    int len = strlen( fullMessage );
    
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( inOnePlayerOnly != NULL && o != inOnePlayerOnly ) {
            continue;
            }

        if( ! o->error && ! o->isTutorial && o->connected ) {


            if( curTime - o->lastGlobalMessageTime > 
                minGlobalMessageSpacingSeconds ) {
                
                int numSent = 
                    o->sock->send( (unsigned char*)fullMessage, 
                                   len, 
                                   false, false );
                
                o->lastGlobalMessageTime = curTime;
                
                if( numSent != len ) {
                    setPlayerDisconnected( o, "Socket write failed",  __func__  , __LINE__);
                    }
                }
            else {
                // messages are coming too quickly for this player to read
                // them, wait before sending this one
                o->globalMessageQueue.push_back( stringDuplicate( inMessage ) );
                }
            }
        }
    delete [] fullMessage;
    }



typedef struct WarPeaceMessageRecord {
        char war;
        int lineageAEveID;
        int lineageBEveID;
        double t;
    } WarPeaceMessageRecord;

SimpleVector<WarPeaceMessageRecord> warPeaceRecords;



void sendPeaceWarMessage( const char *inPeaceOrWar,
                          char inWar,
                          int inLineageAEveID, int inLineageBEveID ) {
    
    double curTime = Time::getCurrentTime();
    
    for( int i=0; i<warPeaceRecords.size(); i++ ) {
        WarPeaceMessageRecord *r = warPeaceRecords.getElement( i );
        
        if( inWar != r->war ) {
            continue;
            }
        
        if( ( r->lineageAEveID == inLineageAEveID &&
              r->lineageBEveID == inLineageBEveID )
            ||
            ( r->lineageAEveID == inLineageBEveID &&
              r->lineageBEveID == inLineageAEveID ) ) {

            if( r->t > curTime - 3 * 60 ) {
                // stil fresh, last similar message happened
                // less than three minutes ago
                return;
                }
            else {
                // stale
                // remove it
                warPeaceRecords.deleteElement( i );
                break;
                }
            }
        }
    WarPeaceMessageRecord r = { inWar, inLineageAEveID, inLineageBEveID,
                                curTime };
    warPeaceRecords.push_back( r );


    const char *nameA = "NAMELESS";
    const char *nameB = "NAMELESS";
    
    for( int j=0; j<players.size(); j++ ) {
        LiveObject *o = players.getElement( j );
                        
        if( ! o->error && 
            o->lineageEveID == inLineageAEveID &&
            o->familyName != NULL ) {
            nameA = o->familyName;
            break;
            }
        }
    for( int j=0; j<players.size(); j++ ) {
        LiveObject *o = players.getElement( j );
                        
        if( ! o->error && 
            o->lineageEveID == inLineageBEveID &&
            o->familyName != NULL ) {
            nameB = o->familyName;
            break;
            }
        }

    char *message = autoSprintf( "%s BETWEEN %s**AND %s FAMILIES",
                                 inPeaceOrWar,
                                 nameA, nameB );

    sendGlobalMessage( message );
    
    delete [] message;
    }




void checkCustomGlobalMessage() {
    
    if( ! SettingsManager::getIntSetting( "customGlobalMessageOn", 0 ) ) {
        return;
        }


    double spacing = 
        SettingsManager::getDoubleSetting( 
            "customGlobalMessageSecondsSpacing", 10.0 );
    
    double lastTime = 
        SettingsManager::getDoubleSetting( 
            "customGlobalMessageLastSendTime", 0.0 );

    double curTime = Time::getCurrentTime();
    
    if( curTime - lastTime < spacing ) {
        return;
        }
        

    
    // check if there's a new custom message waiting
    char *message = 
        SettingsManager::getSettingContents( "customGlobalMessage", 
                                             "" );
    
    if( strcmp( message, "" ) != 0 ) {
        

        int numLines;
        
        char **lines = split( message, "\n", &numLines );
        
        int nextLine = 
            SettingsManager::getIntSetting( 
                "customGlobalMessageNextLine", 0 );
        
        if( nextLine < numLines ) {
            sendGlobalMessage( lines[nextLine] );
            
            nextLine++;
            SettingsManager::setSetting( 
                "customGlobalMessageNextLine", nextLine );

            SettingsManager::setDoubleSetting( 
                "customGlobalMessageLastSendTime", curTime );
            }
        else {
            // out of lines
            SettingsManager::setSetting( "customGlobalMessageOn", 0 );
            SettingsManager::setSetting( "customGlobalMessageNextLine", 0 );
            }

        for( int i=0; i<numLines; i++ ) {
            delete [] lines[i];
            }
        delete [] lines;
        }
    else {
        // no message, disable
        SettingsManager::setSetting( "customGlobalMessageOn", 0 );
        }
    
    delete [] message;
    }





// sets lastSentMap in inO if chunk goes through
// returns result of send, auto-marks error in inO
int sendMapChunkMessage( LiveObject *inO, 
                         char inDestOverride = false,
                         int inDestOverrideX = 0, 
                         int inDestOverrideY = 0 ) {
    
    if( ! inO->connected ) {
        // act like it was a successful send so we can move on until
        // they reconnect later
        return 1;
        }
    
    int messageLength = 0;

    int xd = inO->xd;
    int yd = inO->yd;
    
    if( inDestOverride ) {
        xd = inDestOverrideX;
        yd = inDestOverrideY;
        }
    
    int chunkDimensionX = inO->mMapD / 2;
    int chunkDimensionY = chunkDimensionX - 2;

    int halfW = chunkDimensionX / 2;
    int halfH = chunkDimensionY / 2;
    
    int fullStartX = xd - halfW;
    int fullStartY = yd - halfH;
    
    int numSent = 0;

    

    if( ! inO->firstMapSent ) {
        // send full rect centered on x,y
        
        inO->firstMapSent = true;
        //printf("startx: %d, starty: %d, cx: %d, cy:%d\n",
        //    fullStartX, fullStartY, chunkDimensionX, chunkDimensionY);
        unsigned char *mapChunkMessage = getChunkMessage( fullStartX,
                                                          fullStartY,
                                                          chunkDimensionX,
                                                          chunkDimensionY,
                                                          inO->birthPos,
                                                          &messageLength );
                
        numSent += 
            inO->sock->send( mapChunkMessage, 
                             messageLength, 
                             false, false );
                
        delete [] mapChunkMessage;
        }
    else {
        
        // our closest previous chunk center
        int lastX = inO->lastSentMapX;
        int lastY = inO->lastSentMapY;

        //printf("lastX: %d, lastY: %d\n", lastX, lastY);
        // split next chunk into two bars by subtracting last chunk
        
        int horBarStartX = fullStartX;
        int horBarStartY = fullStartY;
        int horBarW = chunkDimensionX;
        int horBarH = chunkDimensionY;
        
        if( yd > lastY ) {
            // remove bottom of bar
            horBarStartY = lastY + halfH;
            horBarH = yd - lastY;
            }
        else {
            // remove top of bar
            horBarH = lastY - yd;
            }

        if( horBarH > chunkDimensionY ) {
            // don't allow bar to grow too big if we have a huge jump
            // like from VOG mode
            horBarH = chunkDimensionY;
            }
        

        int vertBarStartX = fullStartX;
        int vertBarStartY = fullStartY;
        int vertBarW = chunkDimensionX;
        int vertBarH = chunkDimensionY;
        
        if( xd > lastX ) {
            // remove left part of bar
            vertBarStartX = lastX + halfW;
            vertBarW = xd - lastX;
            }
        else {
            // remove right part of bar
            vertBarW = lastX - xd;
            }
        
        
        if( vertBarW > chunkDimensionX ) {
            // don't allow bar to grow too big if we have a huge jump
            // like from VOG mode
            vertBarW = chunkDimensionX;
            }
        
        
        // now trim vert bar where it intersects with hor bar
        if( yd > lastY ) {
            // remove top of vert bar
            vertBarH -= horBarH;
            }
        else {
            // remove bottom of vert bar
            vertBarStartY = horBarStartY + horBarH;
            vertBarH -= horBarH;
            }
        
        //printf("horBarW: %d, horBarH: %d\n", horBarW, horBarH);
        // only send if non-zero width and height
        if( horBarW > 0 && horBarH > 0 ) {
            int len;
            printf("horBar_startx: %d, horBar_starty: %d, cx: %d, cy:%d\n",
                horBarStartX, horBarStartY, horBarW, horBarH);

            unsigned char *mapChunkMessage = getChunkMessage( horBarStartX,
                                                              horBarStartY,
                                                              horBarW,
                                                              horBarH,
                                                              inO->birthPos,
                                                              &len );
            messageLength += len;
            
            numSent += 
                inO->sock->send( mapChunkMessage, 
                                 len, 
                                 false, false );
            
            delete [] mapChunkMessage;
            }
        //printf("vertBarW: %d, vertBarH: %d\n", vertBarW, vertBarH);
        if( vertBarW > 0 && vertBarH > 0 ) {
            int len;
            //printf("vertBar_startx: %d, vertBar_starty: %d, cx: %d, cy:%d\n",
            //    vertBarStartX, vertBarStartY, vertBarW, vertBarH);
            unsigned char *mapChunkMessage = getChunkMessage( vertBarStartX,
                                                              vertBarStartY,
                                                              vertBarW,
                                                              vertBarH,
                                                              inO->birthPos,
                                                              &len );
            messageLength += len;
            
            numSent += 
                inO->sock->send( mapChunkMessage, 
                                 len, 
                                 false, false );
            
            delete [] mapChunkMessage;
            }
        }
    
    
    inO->gotPartOfThisFrame = true;
                

    if( numSent == messageLength ) {
        // sent correctly
        inO->lastSentMapX = xd;
        inO->lastSentMapY = yd;
        }
    else {
        setPlayerDisconnected( inO, "Socket write failed",  __func__ , __LINE__);
        }
    return numSent;
    }







char *getHoldingString( LiveObject *inObject ) {
    
    int holdingID = hideIDForClient( inObject->holdingID );    


    if( inObject->numContained == 0 ) {
        return autoSprintf( "%d", holdingID );
        }

    
    SimpleVector<char> buffer;
    

    char *idString = autoSprintf( "%d", holdingID );
    
    buffer.appendElementString( idString );
    
    delete [] idString;
    
    
    if( inObject->numContained > 0 ) {
        for( int i=0; i<inObject->numContained; i++ ) {
            
            char *idString = autoSprintf( 
                ",%d", 
                hideIDForClient( abs( inObject->containedIDs[i] ) ) );
    
            buffer.appendElementString( idString );
    
            delete [] idString;

            if( inObject->subContainedIDs[i].size() > 0 ) {
                for( int s=0; s<inObject->subContainedIDs[i].size(); s++ ) {
                    
                    idString = autoSprintf( 
                        ":%d", 
                        hideIDForClient( 
                            inObject->subContainedIDs[i].
                            getElementDirect( s ) ) );
    
                    buffer.appendElementString( idString );
                
                    delete [] idString;
                    }
                }
            }
        }
    
    return buffer.getElementString();
    }



// only consider living, non-moving players
char isMapSpotEmptyOfPlayers( int inX, int inY ) {

    int numLive = players.size();
    
    for( int i=0; i<numLive; i++ ) {
        LiveObject *nextPlayer = players.getElement( i );
        
        if( // not about to be deleted
            ! nextPlayer->error &&
            // held players aren't on map (their coordinates are stale)
            ! nextPlayer->heldByOther &&
            // stationary
            nextPlayer->xs == nextPlayer->xd &&
            nextPlayer->ys == nextPlayer->yd &&
            // in this spot
            inX == nextPlayer->xd &&
            inY == nextPlayer->yd ) {
            return false;            
            } 
        }
    
    return true;
    }




// checks both grid of objects and live, non-moving player positions
char isMapSpotEmpty( int inX, int inY, char inConsiderPlayers = true ) {
    int target = getMapObject( inX, inY );
    
    if( target != 0 ) {
        return false;
        }
    
    if( !inConsiderPlayers ) {
        return true;
        }
    
    return isMapSpotEmptyOfPlayers( inX, inY );
    }



static void setFreshEtaDecayForHeld( LiveObject *inPlayer ) {
    
    if( inPlayer->holdingID == 0 ) {
        inPlayer->holdingEtaDecay = 0;
        }
    
    // does newly-held object have a decay defined?
    TransRecord *newDecayT = getMetaTrans( -1, inPlayer->holdingID );
                    
    if( newDecayT != NULL ) {
        inPlayer->holdingEtaDecay = 
            Time::getCurrentTime() + newDecayT->autoDecaySeconds;
        }
    else {
        // no further decay
        inPlayer->holdingEtaDecay = 0;
        }
    }



void handleMapChangeToPaths( 
    int inX, int inY, ObjectRecord *inNewObject,
    SimpleVector<int> *inPlayerIndicesToSendUpdatesAbout ) {
    
    if( inNewObject->blocksWalking ) {
    
        GridPos dropSpot = { inX, inY };
          
        int numLive = players.size();
                      
        for( int j=0; j<numLive; j++ ) {
            LiveObject *otherPlayer = 
                players.getElement( j );
            
            if( otherPlayer->error ) {
                continue;
                }

            if( otherPlayer->xd != otherPlayer->xs ||
                otherPlayer->yd != otherPlayer->ys ) {
                
                GridPos cPos = 
                    computePartialMoveSpot( otherPlayer );
                                        
                if( distance( cPos, dropSpot ) 
                    <= 2 * pathDeltaMax ) {
                                            
                    // this is close enough
                    // to this path that it might
                    // block it
                
                    int c = computePartialMovePathStep( otherPlayer );

                    // -1 means starting, pre-path pos is closest
                    // push it up to first path step
                    if( c < 0 ) {
                        c = 0;
                        }

                    char blocked = false;
                    int blockedStep = -1;
                                            
                    for( int p=c; 
                         p<otherPlayer->pathLength;
                         p++ ) {
                                                
                        if( equal( 
                                otherPlayer->
                                pathToDest[p],
                                dropSpot ) ) {
                                                    
                            blocked = true;
                            blockedStep = p;
                            break;
                            }
                        }
                                            
                    if( blocked ) {
                        printf( 
                            "  Blocked by drop\n" );
                        }
                                            

                    if( blocked &&
                        blockedStep > 0 ) {
                                                
                        otherPlayer->pathLength
                            = blockedStep;
                        otherPlayer->pathTruncated
                            = true;

                        // update timing
                        double dist = 
                            measurePathLength( otherPlayer->xs,
                                               otherPlayer->ys,
                                               otherPlayer->pathToDest,
                                               otherPlayer->pathLength );    
                                                
                        double distAlreadyDone =
                            measurePathLength( otherPlayer->xs,
                                               otherPlayer->ys,
                                               otherPlayer->pathToDest,
                                               c );
                            
                        double moveSpeed = computeMoveSpeed( otherPlayer ) *
                            getPathSpeedModifier( otherPlayer->pathToDest,
                                                  otherPlayer->pathLength );

                        otherPlayer->moveTotalSeconds 
                            = 
                            dist / 
                            moveSpeed;
                            
                        double secondsAlreadyDone = 
                            distAlreadyDone / 
                            moveSpeed;
                                
                        otherPlayer->moveStartTime = 
                            Time::getCurrentTime() - 
                            secondsAlreadyDone;
                            
                        otherPlayer->newMove = true;
                                                
                        otherPlayer->xd 
                            = otherPlayer->pathToDest[
                                blockedStep - 1].x;
                        otherPlayer->yd 
                            = otherPlayer->pathToDest[
                                blockedStep - 1].y;
                                                
                        }
                    else if( blocked ) {
                        // cutting off path
                        // right at the beginning
                        // nothing left

                        // end move now
                        otherPlayer->xd = 
                            otherPlayer->xs;
                                                
                        otherPlayer->yd = 
                            otherPlayer->ys;
                             
                        otherPlayer->posForced = true;
                    
                        inPlayerIndicesToSendUpdatesAbout->push_back( j );
                        }
                    } 
                                        
                }                                    
            }
        }
    
    }



// returns true if found
char findDropSpot( int inX, int inY, int inSourceX, int inSourceY, 
                   GridPos *outSpot ) {
    char found = false;
    int foundX = inX;
    int foundY = inY;
    
    // change direction of throw
    // to match direction of 
    // drop action
    int xDir = inX - inSourceX;
    int yDir = inY - inSourceY;
                                    
        
    if( xDir == 0 && yDir == 0 ) {
        xDir = 1;
        }
    
    // cap to magnitude
    // death drops can be non-adjacent
    if( xDir > 1 ) {
        xDir = 1;
        }
    if( xDir < -1 ) {
        xDir = -1;
        }
    
    if( yDir > 1 ) {
        yDir = 1;
        }
    if( yDir < -1 ) {
        yDir = -1;
        }
        

    // check in y dir first at each
    // expanded radius?
    char yFirst = false;
        
    if( yDir != 0 ) {
        yFirst = true;
        }
        
    for( int d=1; d<10 && !found; d++ ) {
            
        char doneY0 = false;
            
        for( int yD = -d; yD<=d && !found; 
             yD++ ) {
                
            if( ! doneY0 ) {
                yD = 0;
                }
                
            if( yDir != 0 ) {
                yD *= yDir;
                }
                
            char doneX0 = false;
                
            for( int xD = -d; 
                 xD<=d && !found; 
                 xD++ ) {
                    
                if( ! doneX0 ) {
                    xD = 0;
                    }
                    
                if( xDir != 0 ) {
                    xD *= xDir;
                    }
                    
                    
                if( yD == 0 && xD == 0 ) {
                    if( ! doneX0 ) {
                        doneX0 = true;
                            
                        // back up in loop
                        xD = -d - 1;
                        }
                    continue;
                    }
                                                
                int x = 
                    inSourceX + xD;
                int y = 
                    inSourceY + yD;
                                                
                if( yFirst ) {
                    // swap them
                    // to reverse order
                    // of expansion
                    x = 
                        inSourceX + yD;
                    y =
                        inSourceY + xD;
                    }
                                                


                if( 
                    isMapSpotEmpty( x, y ) ) {
                                                    
                    found = true;
                    foundX = x;
                    foundY = y;
                    }
                                                    
                if( ! doneX0 ) {
                    doneX0 = true;
                                                        
                    // back up in loop
                    xD = -d - 1;
                    }
                }
                                                
            if( ! doneY0 ) {
                doneY0 = true;
                                                
                // back up in loop
                yD = -d - 1;
                }
            }
        }

    outSpot->x = foundX;
    outSpot->y = foundY;
    return found;
    }



#include "spiral.h"

GridPos findClosestEmptyMapSpot( int inX, int inY, int inMaxPointsToCheck,
                                 char *outFound ) {

    GridPos center = { inX, inY };

    for( int i=0; i<inMaxPointsToCheck; i++ ) {
        GridPos p = getSpriralPoint( center, i );

        if( isMapSpotEmpty( p.x, p.y, false ) ) {    
            *outFound = true;
            return p;
            }
        }
    
    *outFound = false;
    GridPos p = { inX, inY };
    return p;
    }



// returns NULL if not found
static LiveObject *getPlayerByEmail( char *inEmail ) {
    for( int j=0; j<players.size(); j++ ) {
        LiveObject *otherPlayer = players.getElement( j );
        if( ! otherPlayer->error &&
            otherPlayer->email != NULL &&
            strcmp( otherPlayer->email, inEmail ) == 0 ) {
            
            return otherPlayer;
            }
        }
    return NULL;
    }



static int usePersonalCurses = 0;





SimpleVector<ChangePosition> newSpeechPos;

SimpleVector<char*> newSpeechPhrases;
SimpleVector<int> newSpeechPlayerIDs;
SimpleVector<char> newSpeechCurseFlags;



SimpleVector<char*> newLocationSpeech;
SimpleVector<ChangePosition> newLocationSpeechPos;




char *isCurseNamingSay( char *inSaidString );

char *isInfertilityDeclaringSay( char *inSaidString );

char *isFertilityDeclaringSay( char *inSaidString );


// password-protected objects
char *isPasswordProtectingSay( char *inSaidString );

static void makePlayerSay( LiveObject *inPlayer, char *inToSay, bool inPrivate = false ) {    

    // password-protected objects
    char *sayingPassword = isPasswordProtectingSay( inToSay );
    if( sayingPassword != NULL ) {
        int passwordLen = strlen( sayingPassword );
        // maximum length is 50
        // as allowed in persistentMapDB
        if( passwordLen > 50 ) {
            // too long, truncate the rest
            sayingPassword[ 50 ] = '\0';
            }
        inPlayer->saidPassword = stringDuplicate( sayingPassword );
        
        // give feedback to player that a password is said
        sendGlobalMessage( (char*)"A PASSWORD HAS BEEN SAID.", inPlayer );
        
        return; // password is silenced, not visible to other players
        }
        
                        
    if( sayingPassword == NULL ) {
        // password should not be recorded as last words
        if( inPlayer->lastSay != NULL ) {
            delete [] inPlayer->lastSay;
            inPlayer->lastSay = NULL;
            }
        inPlayer->lastSay = stringDuplicate( inToSay );
        }


    if( getFemale( inPlayer ) ) {
        char *infertilityDeclaring = isInfertilityDeclaringSay( inToSay );
        char *fertilityDeclaring = isFertilityDeclaringSay( inToSay );
        if( infertilityDeclaring != NULL || fertilityDeclaring != NULL ) return;
    }


    char isCurse = false;

    char *cursedName = isCurseNamingSay( inToSay );

    char isYouShortcut = false;
    char isBabyShortcut = false;
    if( strcmp( inToSay, curseYouPhrase ) == 0 ) {
        isYouShortcut = true;
        }
    
    if( strcmp( inToSay, curseBabyPhrase ) == 0
        &&
        SettingsManager::getIntSetting( "allowBabyCursing", 0 ) ) {
        
        isBabyShortcut = true;
        }
    

    if( cursedName != NULL ) {
        // it's a pointer into inToSay
        
        // make a copy so we can delete it later
        cursedName = stringDuplicate( cursedName );
        }


            
    int curseDistance = SettingsManager::getIntSetting( "curseDistance", 200 );
    
        
    if( cursedName == NULL &&
        players.size() >= minActivePlayersForLanguages ) {
        
        // consider cursing in other languages

        int speakerAge = computeAge( inPlayer );
        
        GridPos speakerPos = getPlayerPos( inPlayer );
        
        for( int i=0; i<players.size(); i++ ) {
            LiveObject *otherPlayer = players.getElement( i );
            
            if( otherPlayer == inPlayer ||
                otherPlayer->error ||
                otherPlayer->lineageEveID == inPlayer->lineageEveID ) {
                continue;
                }

            if( distance( speakerPos, getPlayerPos( otherPlayer ) ) >
                curseDistance ) {
                // only consider nearby players
                continue;
                }
                
            char *translatedPhrase =
                mapLanguagePhrase( 
                    inToSay,
                    inPlayer->lineageEveID,
                    otherPlayer->lineageEveID,
                    inPlayer->id,
                    otherPlayer->id,
                    speakerAge,
                    computeAge( otherPlayer ),
                    inPlayer->parentID,
                    otherPlayer->parentID );
            
            cursedName = isCurseNamingSay( translatedPhrase );
            
            if( strcmp( translatedPhrase, curseYouPhrase ) == 0 ) {
                // said CURSE YOU in other language
                isYouShortcut = true;
                }

            // make copy so we can delete later an delete the underlying
            // translatedPhrase now
            
            if( cursedName != NULL ) {
                cursedName = stringDuplicate( cursedName );
                }

            delete [] translatedPhrase;

            if( cursedName != NULL ) {
                int namedPersonLineageEveID = 
                    getCurseReceiverLineageEveID( cursedName );
                
                if( namedPersonLineageEveID == otherPlayer->lineageEveID ) {
                    // the named person belonged to the lineage of the 
                    // person who spoke this language!
                    break;
                    }
                // else cursed in this language, for someone outside
                // this language's line
                delete [] cursedName;
                cursedName = NULL;
                }
            }
        }



    LiveObject *youCursePlayer = NULL;
    LiveObject *babyCursePlayer = NULL;

    if( isYouShortcut ) {
        // find closest player
        GridPos speakerPos = getPlayerPos( inPlayer );
        
        LiveObject *closestOther = NULL;
        double closestDist = 9999999;
        
        for( int i=0; i<players.size(); i++ ) {
            LiveObject *otherPlayer = players.getElement( i );
            
            if( otherPlayer == inPlayer ||
                otherPlayer->error ) {
                continue;
                }
            double dist = distance( speakerPos, getPlayerPos( otherPlayer ) );

            if( dist > getMaxChunkDimension() ) {
                // only consider nearby players
                // don't use curseDistance setting here,
                // because we don't want CURSE YOU to apply from too
                // far away (would likely be a random target player)
                continue;
                }
            if( dist < closestDist ) {
                closestDist = dist;
                closestOther = otherPlayer;
                }
            }


        if( closestOther != NULL ) {
            youCursePlayer = closestOther;
            
            if( cursedName != NULL ) {
                delete [] cursedName;
                cursedName = NULL;
                }

            if( youCursePlayer->name != NULL ) {
                // allow name-based curse to go through, if possible
                cursedName = stringDuplicate( youCursePlayer->name );
                }
            }
        }
    else if( isBabyShortcut ) {
        // this case is more robust (below) by simply using the lastBabyEmail
        // in all cases

        // That way there's no confusing about who MY BABY is (always the
        // most-recent baby).
        }


    // make sure, no matter what, we can't curse living 
    // people at a great distance
    // note that, sice we're not tracking dead people here
    // that case will be caught below, since the curses.h tracks death
    // locations
    GridPos speakerPos = getPlayerPos( inPlayer );
    
    if( cursedName != NULL &&
        strcmp( cursedName, "" ) != 0 ) {

        for( int i=0; i<players.size(); i++ ) {
            LiveObject *otherPlayer = players.getElement( i );
            
            if( otherPlayer == inPlayer ||
                otherPlayer->error ) {
                continue;
                }
            if( otherPlayer->name != NULL &&
                strcmp( otherPlayer->name, cursedName ) == 0 ) {
                // matching player
                
                double dist = 
                    distance( speakerPos, getPlayerPos( otherPlayer ) );

                if( dist > curseDistance ) {
                    // too far
                    delete [] cursedName;
                    cursedName = NULL;
                    }
                break;
                }
            }
        }
    
    
    char *dbCurseTargetEmail = NULL;

    
    char canCurse = false;
    
    if( inPlayer->curseTokenCount > 0 ) {
        canCurse = true;
        }
    

    if( canCurse && 
        cursedName != NULL && 
        strcmp( cursedName, "" ) != 0 ) {
        
        isCurse = cursePlayer( inPlayer->id,
                               inPlayer->lineageEveID,
                               inPlayer->email,
                               speakerPos,
                               curseDistance,
                               cursedName );
        
        if( isCurse ) {
            char *targetEmail = getCurseReceiverEmail( cursedName );
            if( targetEmail != NULL ) {
                setDBCurse( inPlayer->id, inPlayer->email, targetEmail );
                dbCurseTargetEmail = targetEmail;
                }
            }
        }
    
    
    if( cursedName != NULL ) {
        delete [] cursedName;
        }
    

    if( canCurse && !isCurse ) {
        // named curse didn't happen above
        // maybe we used a shortcut, and target didn't have name
        
        if( isYouShortcut && youCursePlayer != NULL &&
            spendCurseToken( inPlayer->email ) ) {
            
            isCurse = true;
            setDBCurse( inPlayer->id, inPlayer->email, youCursePlayer->email );
            dbCurseTargetEmail = youCursePlayer->email;
            }
        else if( isBabyShortcut && babyCursePlayer != NULL &&
            spendCurseToken( inPlayer->email ) ) {
            
            isCurse = true;
            char *targetEmail = babyCursePlayer->email;
            
            if( strcmp( targetEmail, "email_cleared" ) == 0 ) {
                // deleted players allowed here
                targetEmail = babyCursePlayer->origEmail;
                }
            if( targetEmail != NULL ) {
                setDBCurse( inPlayer->id, inPlayer->email, targetEmail );
                dbCurseTargetEmail = targetEmail;
                }
            }
        else if( isBabyShortcut && babyCursePlayer == NULL &&
                 inPlayer->lastBabyEmail != NULL &&
                 spendCurseToken( inPlayer->email ) ) {
            
            isCurse = true;
            
            setDBCurse( inPlayer->id, 
                        inPlayer->email, inPlayer->lastBabyEmail );
            dbCurseTargetEmail = inPlayer->lastBabyEmail;
            }
        }

    if( dbCurseTargetEmail != NULL && usePersonalCurses ) {
        LiveObject *targetP = getPlayerByEmail( dbCurseTargetEmail );
        
        if( targetP != NULL ) {
            char *message = autoSprintf( "CU\n%d 1 %s_%s\n#", targetP->id,
                                         getCurseWord( inPlayer->email,
                                                       targetP->email, 0 ),
                                         getCurseWord( inPlayer->email,
                                                       targetP->email, 1 ) );
            sendMessageToPlayer( inPlayer,
                                 message, strlen( message ) );
            delete [] message;
            }
        }
    


    if( isCurse ) {
        if( inPlayer->curseStatus.curseLevel == 0 &&
            hasCurseToken( inPlayer->email ) ) {
            inPlayer->curseTokenCount = 1;
            }
        else {
            inPlayer->curseTokenCount = 0;
            }
        inPlayer->curseTokenUpdate = true;
        }

    

    int curseFlag = 0;
    if( isCurse ) {
        curseFlag = 1;
        }
    

    newSpeechPhrases.push_back( stringDuplicate( inToSay ) );
    newSpeechCurseFlags.push_back( curseFlag );
    newSpeechPlayerIDs.push_back( inPlayer->id );

                        
    ChangePosition p = { inPlayer->xd, inPlayer->yd, false, -1 };
    if( inPrivate ) p.responsiblePlayerID = inPlayer->id;
                        
    // if held, speech happens where held
    if( inPlayer->heldByOther ) {
        LiveObject *holdingPlayer = 
            getLiveObject( inPlayer->heldByOtherID );
                
        if( holdingPlayer != NULL ) {
            p.x = holdingPlayer->xd;
            p.y = holdingPlayer->yd;
            }
        }

    newSpeechPos.push_back( p );
    if( inPrivate ) return;


    SimpleVector<int> pipesIn;
    GridPos playerPos = getPlayerPos( inPlayer );
    
    
    if( inPlayer->heldByOther ) {    
        LiveObject *holdingPlayer = 
            getLiveObject( inPlayer->heldByOtherID );
                
        if( holdingPlayer != NULL ) {
            playerPos = getPlayerPos( holdingPlayer );
            }
        }
    
    getSpeechPipesIn( playerPos.x, playerPos.y, &pipesIn );
    
    if( pipesIn.size() > 0 ) {
        for( int p=0; p<pipesIn.size(); p++ ) {
            int pipeIndex = pipesIn.getElementDirect( p );

            SimpleVector<GridPos> *pipesOut = getSpeechPipesOut( pipeIndex );

            for( int i=0; i<pipesOut->size(); i++ ) {
                GridPos outPos = pipesOut->getElementDirect( i );
                
                char *newSpeech = stringDuplicate( inToSay );
                
                // trim off any metadata so it doesn't go through
                char *starLoc = strstr( newSpeech, " *" );
                
                if( starLoc != NULL ) {
                    starLoc[0] = '\0';
                    }

                newLocationSpeech.push_back( newSpeech );
            
                ChangePosition outChangePos = { outPos.x, outPos.y, false, -1 };
                newLocationSpeechPos.push_back( outChangePos );
                }
            }
        }
    }


static void forcePlayerToRead( LiveObject *inPlayer,
                               int inObjectID ) {
            
    char metaData[ MAP_METADATA_LENGTH ];
    char found = getMetadata( inObjectID, 
                              (unsigned char*)metaData );

    if( found ) {
        // read what they picked up, subject to limit
                
        unsigned int sayLimit = getSayLimit( inPlayer );
        
        if( computeAge( inPlayer ) < 10 &&
            strlen( metaData ) > sayLimit ) {
            // truncate with ...
            metaData[ sayLimit ] = '.';
            metaData[ sayLimit + 1 ] = '.';
            metaData[ sayLimit + 2 ] = '.';
            metaData[ sayLimit + 3 ] = '\0';
            
            // watch for truncated map metadata
            // trim it off (too young to read maps)
            char *starLoc = strstr( metaData, " *" );
            
            if( starLoc != NULL ) {
                starLoc[0] = '\0';
                }
            }
        char *quotedPhrase = autoSprintf( ":%s", metaData );
        makePlayerSay( inPlayer, quotedPhrase );
        delete [] quotedPhrase;
        }
    }
    
//2HOL mechanics to read written objects
static void forceObjectToRead( LiveObject *inPlayer,
                               int inObjectID,
                               GridPos inReadPos,
                               bool passToRead ) {

    //avoid spamming location speech
    //different behavior for clickToRead and/or passToRead objects
    for( int j = 0; j < inPlayer->readPositions.size(); j++ ) {
        GridPos p = inPlayer->readPositions.getElementDirect( j );
        double eta = inPlayer->readPositionsETA.getElementDirect( j );
        
        if( !passToRead )
        if( p.x == inReadPos.x && p.y == inReadPos.y && Time::getCurrentTime() <= eta ){
            return;
        }
        
        if( passToRead )
        if( p.x == inReadPos.x && p.y == inReadPos.y ){
            return;
        }
    }

    char metaData[ MAP_METADATA_LENGTH ];
    char found = getMetadata( inObjectID, 
                              (unsigned char*)metaData );

    if( found ) {
        //speech limit is ignored here
        char *quotedPhrase = autoSprintf( ":%s", metaData );
        
        ChangePosition cp;
        cp.x = inReadPos.x;
        cp.y = inReadPos.y;
        cp.global = false;
        cp.responsiblePlayerID = inPlayer->id;

        newLocationSpeechPos.push_back( cp );
        newLocationSpeech.push_back( 
            stringDuplicate( quotedPhrase ) );
        
        //longer time for longer speech
        //roughly matching but slightly longer than client speech bubbles duration
        double speechETA = Time::getCurrentTime() + 3.25 + strlen( quotedPhrase ) / 5;
        inPlayer->readPositions.push_back( inReadPos );
        inPlayer->readPositionsETA.push_back( speechETA );
        
        delete [] quotedPhrase;
        }
    }

static void holdingSomethingNew( LiveObject *inPlayer, 
                                 int inOldHoldingID = 0 ) {
    if( inPlayer->holdingID > 0 ) {
       
        ObjectRecord *o = getObject( inPlayer->holdingID );
        
        ObjectRecord *oldO = NULL;
        if( inOldHoldingID > 0 ) {
            oldO = getObject( inOldHoldingID );
            }
        
        if( o->written &&
            ( oldO == NULL ||
              ! ( oldO->written || oldO->writable ) ) ) {

            forcePlayerToRead( inPlayer, inPlayer->holdingID );
            }

        if( o->isFlying ) {
            inPlayer->holdingFlightObject = true;
            }
        else {
            inPlayer->holdingFlightObject = false;
            }
        }
    else {
        inPlayer->holdingFlightObject = false;
        }
    }




static SimpleVector<GraveInfo> newGraves;
static SimpleVector<GraveMoveInfo> newGraveMoves;



static int isGraveSwapDest( int inTargetX, int inTargetY,
                            int inDroppingPlayerID ) {
    
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( o->error || o->id == inDroppingPlayerID ) {
            continue;
            }
        
        if( o->holdingID > 0 && strstr( getObject( o->holdingID )->description,
                                        "origGrave" ) != NULL ) {
            
            if( inTargetX == o->heldGraveOriginX &&
                inTargetY == o->heldGraveOriginY ) {
                return true;
                }
            }
        }
    
    return false;
    }



// drops an object held by a player at target x,y location
// doesn't check for adjacency (so works for thrown drops too)
// if target spot blocked, will search for empty spot to throw object into
// if inPlayerIndicesToSendUpdatesAbout is NULL, it is ignored
void handleDrop( int inX, int inY, LiveObject *inDroppingPlayer,
                 SimpleVector<int> *inPlayerIndicesToSendUpdatesAbout ) {
    
    int oldHoldingID = inDroppingPlayer->holdingID;
    

    if( oldHoldingID > 0 &&
        getObject( oldHoldingID )->permanent ) {
        // what they are holding is stuck in their
        // hand

        // see if a use-on-bare-ground drop 
        // action applies (example:  dismounting
        // a horse)
                            
        // note that if use on bare ground
        // also has a new actor, that will be lost
        // in this process.
        // (example:  holding a roped lamb when dying,
        //            lamb is dropped, rope is lost)

        TransRecord *bareTrans =
            getPTrans( oldHoldingID, -1 );
                            

        if( bareTrans == NULL ||
            bareTrans->newTarget == 0 ) {
            // no immediate bare ground trans
            // check if there's a timer transition for this held object
            // (like cast fishing pole)
            // and force-run that transition now
            TransRecord *timeTrans = getPTrans( -1, oldHoldingID );
            
            if( timeTrans != NULL && timeTrans->newTarget != 0 ) {
                oldHoldingID = timeTrans->newTarget;
            
                inDroppingPlayer->holdingID = 
                    timeTrans->newTarget;
                holdingSomethingNew( inDroppingPlayer, oldHoldingID );

                setFreshEtaDecayForHeld( inDroppingPlayer );
                }

            if( getObject( oldHoldingID )->permanent ) {
                // still permanent after timed trans
                
                // check again for a bare ground trans
                bareTrans =
                    getPTrans( oldHoldingID, -1 );
                }
            }
        

        if( bareTrans != NULL &&
            bareTrans->newTarget > 0 ) {
            
            if( bareTrans->newActor > 0 ) {
                // something would be left in hand
                
                // throw it down first
                inDroppingPlayer->holdingID = bareTrans->newActor;
                setFreshEtaDecayForHeld( inDroppingPlayer );
                handleDrop( inX, inY, inDroppingPlayer, 
                            inPlayerIndicesToSendUpdatesAbout );
                }

            oldHoldingID = bareTrans->newTarget;
            
            inDroppingPlayer->holdingID = 
                bareTrans->newTarget;
            holdingSomethingNew( inDroppingPlayer, oldHoldingID );

            setFreshEtaDecayForHeld( inDroppingPlayer );
            }
        }
    else if( oldHoldingID > 0 &&
             ! getObject( oldHoldingID )->permanent ) {
        // what they are holding is NOT stuck in their
        // hand

        // see if a use-on-bare-ground drop 
        // action applies (example:  getting wounded while holding a goose)
                            
        // do not consider doing this if use-on-bare-ground leaves something
        // in the hand

        TransRecord *bareTrans =
            getPTrans( oldHoldingID, -1 );
                            
        if( bareTrans != NULL &&
            bareTrans->newTarget > 0 &&
            bareTrans->newActor == 0 ) {
                            
            oldHoldingID = bareTrans->newTarget;
            
            inDroppingPlayer->holdingID = 
                bareTrans->newTarget;
            holdingSomethingNew( inDroppingPlayer, oldHoldingID );

            setFreshEtaDecayForHeld( inDroppingPlayer );
            }
        }

    int targetX = inX;
    int targetY = inY;

    int mapID = getMapObject( inX, inY );
    char mapSpotBlocking = false;
    if( mapID > 0 ) {
        mapSpotBlocking = getObject( mapID )->blocksWalking;
        }
    

    if( ( inDroppingPlayer->holdingID < 0 && mapSpotBlocking )
        ||
        ( inDroppingPlayer->holdingID > 0 && mapID != 0 ) ) {
        
        // drop spot blocked
        // search for another
        // throw held into nearest empty spot
                                    
        
        GridPos spot;

        GridPos playerPos = getPlayerPos( inDroppingPlayer );
        
        char found = findDropSpot( inX, inY, 
                                   playerPos.x, playerPos.y,
                                   &spot );
        
        int foundX = spot.x;
        int foundY = spot.y;



        if( found && inDroppingPlayer->holdingID > 0 ) {
            targetX = foundX;
            targetY = foundY;
            }
        else {
            // no place to drop it, it disappears

            // OR we're holding a baby,
            // then just put the baby where we are
            // (don't ever throw babies, that's weird and exploitable)
            if( inDroppingPlayer->holdingID < 0 ) {
                int babyID = - inDroppingPlayer->holdingID;
                
                LiveObject *babyO = getLiveObject( babyID );
                
                if( babyO != NULL ) {
                    babyO->xd = inDroppingPlayer->xd;
                    babyO->xs = inDroppingPlayer->xd;
                    
                    babyO->yd = inDroppingPlayer->yd;
                    babyO->ys = inDroppingPlayer->yd;

                    babyO->heldByOther = false;

                    if( isFertileAge( inDroppingPlayer ) ) {    
                        // reset food decrement time
                        babyO->foodDecrementETASeconds =
                            Time::getCurrentTime() +
                            computeFoodDecrementTimeSeconds( babyO );
                        }
                    
                    if( inPlayerIndicesToSendUpdatesAbout != NULL ) {    
                        inPlayerIndicesToSendUpdatesAbout->push_back( 
                            getLiveObjectIndex( babyID ) );
                        }
                    
                    }
                
                }
            
            inDroppingPlayer->holdingID = 0;
            inDroppingPlayer->holdingEtaDecay = 0;
            inDroppingPlayer->heldOriginValid = 0;
            inDroppingPlayer->heldOriginX = 0;
            inDroppingPlayer->heldOriginY = 0;
            inDroppingPlayer->heldTransitionSourceID = -1;
            
            if( inDroppingPlayer->numContained != 0 ) {
                clearPlayerHeldContained( inDroppingPlayer );
                }
            return;
            }            
        }
    
    
    if( inDroppingPlayer->holdingID < 0 ) {
        // dropping a baby
        
        int babyID = - inDroppingPlayer->holdingID;
                
        LiveObject *babyO = getLiveObject( babyID );
        
        if( babyO != NULL ) {
            babyO->xd = targetX;
            babyO->xs = targetX;
                    
            babyO->yd = targetY;
            babyO->ys = targetY;
            
            babyO->heldByOther = false;
            
            // force baby pos
            // baby can wriggle out of arms in same server step that it was
            // picked up.  In that case, the clients will never get the
            // message that the baby was picked up.  The baby client could
            // be in the middle of a client-side move, and we need to force
            // them back to their true position.
            babyO->posForced = true;
            
            if( isFertileAge( inDroppingPlayer ) ) {    
                // reset food decrement time
                babyO->foodDecrementETASeconds =
                    Time::getCurrentTime() +
                    computeFoodDecrementTimeSeconds( babyO );
                }

            if( inPlayerIndicesToSendUpdatesAbout != NULL ) {
                inPlayerIndicesToSendUpdatesAbout->push_back( 
                    getLiveObjectIndex( babyID ) );
                }
            }
        
        inDroppingPlayer->holdingID = 0;
        inDroppingPlayer->holdingEtaDecay = 0;
        inDroppingPlayer->heldOriginValid = 0;
        inDroppingPlayer->heldOriginX = 0;
        inDroppingPlayer->heldOriginY = 0;
        inDroppingPlayer->heldTransitionSourceID = -1;
        
        return;
        }
    
    setResponsiblePlayer( inDroppingPlayer->id );
    
    ObjectRecord *o = getObject( inDroppingPlayer->holdingID );
                                
    if( strstr( o->description, "origGrave" ) 
        != NULL ) {
                                    
        setGravePlayerID( 
            targetX, targetY, inDroppingPlayer->heldGravePlayerID );
        
        int swapDest = isGraveSwapDest( targetX, targetY, 
                                        inDroppingPlayer->id );
        
        // see if another player has target location in air


        GraveMoveInfo g = { 
            { inDroppingPlayer->heldGraveOriginX,
              inDroppingPlayer->heldGraveOriginY },
            { targetX,
              targetY },
            swapDest };
        newGraveMoves.push_back( g );
        }


    setMapObject( targetX, targetY, inDroppingPlayer->holdingID );
    setEtaDecay( targetX, targetY, inDroppingPlayer->holdingEtaDecay );

    transferHeldContainedToMap( inDroppingPlayer, targetX, targetY );
    
                                

    setResponsiblePlayer( -1 );
                                
    inDroppingPlayer->holdingID = 0;
    inDroppingPlayer->holdingEtaDecay = 0;
    inDroppingPlayer->heldOriginValid = 0;
    inDroppingPlayer->heldOriginX = 0;
    inDroppingPlayer->heldOriginY = 0;
    inDroppingPlayer->heldTransitionSourceID = -1;
                                
    // watch out for truncations of in-progress
    // moves of other players
            
    ObjectRecord *droppedObject = getObject( oldHoldingID );
   
    if( inPlayerIndicesToSendUpdatesAbout != NULL ) {    
        handleMapChangeToPaths( targetX, targetY, droppedObject,
                                inPlayerIndicesToSendUpdatesAbout );
        }
    
    
    }



LiveObject *getAdultHolding( LiveObject *inBabyObject ) {
    int numLive = players.size();
    
    for( int j=0; j<numLive; j++ ) {
        LiveObject *adultO = players.getElement( j );

        if( - adultO->holdingID == inBabyObject->id ) {
            return adultO;
            }
        }
    return NULL;
    }



void handleForcedBabyDrop( 
    LiveObject *inBabyObject,
    SimpleVector<int> *inPlayerIndicesToSendUpdatesAbout ) {
    
    int numLive = players.size();
    
    for( int j=0; j<numLive; j++ ) {
        LiveObject *adultO = players.getElement( j );

        if( - adultO->holdingID == inBabyObject->id ) {

            // don't need to send update about this adult's
            // holding status.
            // the update sent about the baby will inform clients
            // that the baby is no longer held by this adult
            //inPlayerIndicesToSendUpdatesAbout->push_back( j );
            
            GridPos dropPos;
            
            if( adultO->xd == 
                adultO->xs &&
                adultO->yd ==
                adultO->ys ) {
                
                dropPos.x = adultO->xd;
                dropPos.y = adultO->yd;
                }
            else {
                dropPos = 
                    computePartialMoveSpot( adultO );
                }
            
            
            handleDrop( 
                dropPos.x, dropPos.y, 
                adultO,
                inPlayerIndicesToSendUpdatesAbout );

            
            break;
            }
        }
    }



static void handleHoldingChange( LiveObject *inPlayer, int inNewHeldID );



static void swapHeldWithGround( 
    LiveObject *inPlayer, int inTargetID, 
    int inMapX, int inMapY,
    SimpleVector<int> *inPlayerIndicesToSendUpdatesAbout) {
    
    
    if( inTargetID == inPlayer->holdingID &&
        inPlayer->numContained == 0 &&
        getNumContained( inMapX, inMapY ) == 0 ) {
        // swap of same non-container object with self
        // ignore this, to prevent weird case of swapping
        // grave basket with self
        return;
        }
    

    timeSec_t newHoldingEtaDecay = getEtaDecay( inMapX, inMapY );
    
    FullMapContained f = getFullMapContained( inMapX, inMapY );


    int gravePlayerID = getGravePlayerID( inMapX, inMapY );
        
    if( gravePlayerID > 0 ) {
            
        // player action actually picked up this grave
        
        // clear it from ground
        setGravePlayerID( inMapX, inMapY, 0 );
        }

    
    clearAllContained( inMapX, inMapY );
    setMapObject( inMapX, inMapY, 0 );
    
    handleDrop( inMapX, inMapY, inPlayer, inPlayerIndicesToSendUpdatesAbout );
    
    
    inPlayer->holdingID = inTargetID;
    inPlayer->holdingEtaDecay = newHoldingEtaDecay;
    
    setContained( inPlayer, f );


    // does bare-hand action apply to this newly-held object
    // one that results in something new in the hand and
    // nothing on the ground?
    
    // if so, it is a pick-up action, and it should apply here
    
    TransRecord *pickupTrans = getPTrans( 0, inTargetID );

    char newHandled = false;
                
    if( pickupTrans != NULL && pickupTrans->newActor > 0 &&
        pickupTrans->newTarget == 0 ) {
                    
        int newTargetID = pickupTrans->newActor;
        
        if( newTargetID != inTargetID ) {
            handleHoldingChange( inPlayer, newTargetID );
            newHandled = true;
            }
        }
    
    if( ! newHandled ) {
        holdingSomethingNew( inPlayer );
        }
    
    inPlayer->heldOriginValid = 1;
    inPlayer->heldOriginX = inMapX;
    inPlayer->heldOriginY = inMapY;
    inPlayer->heldTransitionSourceID = -1;


    inPlayer->heldGravePlayerID = 0;

    if( inPlayer->holdingID > 0 &&
        strstr( getObject( inPlayer->holdingID )->description, 
                "origGrave" ) != NULL &&
        gravePlayerID > 0 ) {
    
        inPlayer->heldGraveOriginX = inMapX;
        inPlayer->heldGraveOriginY = inMapY;
        inPlayer->heldGravePlayerID = gravePlayerID;
        }
    }









// returns 0 for NULL
static int objectRecordToID( ObjectRecord *inRecord ) {
    if( inRecord == NULL ) {
        return 0;
        }
    else {
        return inRecord->id;
        }
    }



typedef struct UpdateRecord{
        char *formatString;
        char posUsed;
        int absolutePosX, absolutePosY;
        GridPos absoluteActionTarget;
        int absoluteHeldOriginX, absoluteHeldOriginY;
    } UpdateRecord;



static char *getUpdateLineFromRecord( 
    UpdateRecord *inRecord, GridPos inRelativeToPos, GridPos inObserverPos ) {
    
    if( inRecord->posUsed ) {
        
        GridPos updatePos = { inRecord->absolutePosX, inRecord->absolutePosY };
        
        if( distance( updatePos, inObserverPos ) > 
            getMaxChunkDimension() * 2 ) {
            
            // this update is for a far-away player
            
            // put dummy positions in to hide their coordinates
            // so that people sniffing the protocol can't get relative
            // location information
            
            return autoSprintf( inRecord->formatString,
                                1977, 1977,
                                1977, 1977,
                                1977, 1977 );
            }


        return autoSprintf( inRecord->formatString,
                            inRecord->absoluteActionTarget.x 
                            - inRelativeToPos.x,
                            inRecord->absoluteActionTarget.y 
                            - inRelativeToPos.y,
                            inRecord->absoluteHeldOriginX - inRelativeToPos.x, 
                            inRecord->absoluteHeldOriginY - inRelativeToPos.y,
                            inRecord->absolutePosX - inRelativeToPos.x, 
                            inRecord->absolutePosY - inRelativeToPos.y );
        }
    else {
        // posUsed false only if thise is a DELETE PU message
        // set all positions to 0 in that case
        return autoSprintf( inRecord->formatString,
                            0, 0,
                            0, 0 );
        }
    }



static SimpleVector<int> newEmotPlayerIDs;
static SimpleVector<int> newEmotIndices;
// 0 if no ttl specified
static SimpleVector<int> newEmotTTLs;



static char isYummy( LiveObject *inPlayer, int inObjectID ) {
    ObjectRecord *o = getObject( inObjectID );
    
    if( o->isUseDummy ) {
        inObjectID = o->useDummyParent;
        o = getObject( inObjectID );
        }

    if( o->foodValue == 0 && ! eatEverythingMode ) {
        return false;
        }

    if( inObjectID == inPlayer->cravingFood.foodID &&
        computeAge( inPlayer ) >= minAgeForCravings ) {
        return true;
        }

    for( int i=0; i<inPlayer->yummyFoodChain.size(); i++ ) {
        if( inObjectID == inPlayer->yummyFoodChain.getElementDirect(i) ) {
            return false;
            }
        }
    return true;
    }
    
static char isReallyYummy( LiveObject *inPlayer, int inObjectID ) {
    
    // whether the food is actually not in the yum chain
    // return false for meh food that the player is craving
    // which is displayed "yum" client-side
    
    ObjectRecord *o = getObject( inObjectID );
    
    if( o->isUseDummy ) {
        inObjectID = o->useDummyParent;
        o = getObject( inObjectID );
        }

    if( o->foodValue == 0 && ! eatEverythingMode ) {
        return false;
        }

    for( int i=0; i<inPlayer->yummyFoodChain.size(); i++ ) {
        if( inObjectID == inPlayer->yummyFoodChain.getElementDirect(i) ) {
            return false;
            }
        }
    return true;
    }


static void setRefuseFoodEmote( LiveObject *hitPlayer );

static void updateYum( LiveObject *inPlayer, int inFoodEatenID,
                       char inFedSelf = true ) {

    char wasYummy = true;
    
    if( ! isYummy( inPlayer, inFoodEatenID ) ) {
        wasYummy = false;
        
        // chain broken
        
        // only feeding self can break chain
        if( inFedSelf && canYumChainBreak ) {
            inPlayer->yummyFoodChain.deleteAll();
            }
            
        setRefuseFoodEmote( inPlayer );
            
        }
    
    
    ObjectRecord *o = getObject( inFoodEatenID );
    
    if( o->isUseDummy ) {
        inFoodEatenID = o->useDummyParent;
        }
    
    
    // add to chain
    // might be starting a new chain
    // (do this if fed yummy food by other player too)
    if( wasYummy ||
        inPlayer->yummyFoodChain.size() == 0 ) {
        
        int eatenID = inFoodEatenID;
        
        if( isReallyYummy( inPlayer, eatenID ) ) {
            inPlayer->yummyFoodChain.push_back( eatenID );
            }
        
        // now it is possible to "grief" the craving pool
        // by eating high tech food without craving them
        // but this also means that it requires more effort to
        // cheese the craving system by deliberately eating
        // easy food first in an advanced town
        logFoodDepth( inPlayer->lineageEveID, eatenID );
        
        if( eatenID == inPlayer->cravingFood.foodID &&
            computeAge( inPlayer ) >= minAgeForCravings ) {
            
            for( int i=0; i< inPlayer->cravingFood.bonus; i++ ) {
                // add extra copies to YUM chain as a bonus
                inPlayer->yummyFoodChain.push_back( eatenID );
                }
            
            // craving satisfied, go on to next thing in list
            inPlayer->cravingFood = 
                getCravedFood( inPlayer->lineageEveID,
                               inPlayer->parentChainLength,
                               inPlayer->cravingFood );
            // reset generational bonus counter
            inPlayer->cravingFoodYumIncrement = 1;
            
            // flag them for getting a new craving message
            inPlayer->cravingKnown = false;
            
            // satisfied emot
            
            if( satisfiedEmotionIndex != -1 ) {
                inPlayer->emotFrozen = false;
                inPlayer->emotUnfreezeETA = 0;
        
                newEmotPlayerIDs.push_back( inPlayer->id );
                
                newEmotIndices.push_back( satisfiedEmotionIndex );
                // 3 sec
                newEmotTTLs.push_back( 1 );
                
                // don't leave starving status, or else non-starving
                // change might override our satisfied emote
                inPlayer->starving = false;
                }
            }
        }
    

    int currentBonus = inPlayer->yummyFoodChain.size() - 1;

    if( currentBonus < 0 ) {
        currentBonus = 0;
        }    

    if( wasYummy ) {
        // only get bonus if actually was yummy (whether fed self or not)
        // chain not broken if fed non-yummy by other, but don't get bonus
        inPlayer->yummyBonusStore += currentBonus;
        }
        
    if( wasYummy ||
        strstr( o->description, "modTool" )
    ) {
        // bonus part of foodValue goes into the yum bonus if yummy (or is craved)
        // or if food is permanent, special case for testing/mod objects
        inPlayer->yummyBonusStore += o->bonusValue;
        }
    else {
        // otherwise it goes into the base food bar, without overflow
        inPlayer->foodStore += o->bonusValue;
        int cap = computeFoodCapacity( inPlayer );
        if( inPlayer->foodStore > cap ) inPlayer->foodStore = cap;
        }
    
    }


static int getEatBonus( LiveObject *inPlayer ) {
    
    // in OHOL, this function returns the food value based on generational food decay
    // now we just use it for newbie food buff

    int b = 
        // newbie food buff
        inPlayer->personalEatBonus +
        // plain server-wide food bonus for adjusting difficulty
        // berry was worth 3 pips, plus 2 pips eatBonus.
        // In OHOL, this bonus was then repurposed to be the base of generational food decay
        eatBonus;
    
    return b;
    }




static UpdateRecord getUpdateRecord( 
    LiveObject *inPlayer,
    char inDelete,
    char inPartial = false ) {

    char *holdingString = getHoldingString( inPlayer );
    
    // this is 0 if still in motion (mid-move update)
    int doneMoving = 0;
    
    if( inPlayer->xs == inPlayer->xd &&
        inPlayer->ys == inPlayer->yd &&
        ! inPlayer->heldByOther ) {
        // not moving
        doneMoving = inPlayer->lastMoveSequenceNumber;
        }
    
    char midMove = false;
    
    if( inPartial || 
        inPlayer->xs != inPlayer->xd ||
        inPlayer->ys != inPlayer->yd ) {
        
        midMove = true;
        }
    

    UpdateRecord r;
        

    char *posString;
    if( inDelete ) {
        posString = stringDuplicate( "0 0 X X" );
        r.posUsed = false;
        }
    else {
        int x, y;

        r.posUsed = true;

        if( doneMoving > 0 || ! midMove ) {
            x = inPlayer->xs;
            y = inPlayer->ys;
            }
        else {
            // mid-move, and partial position requested
            GridPos p = computePartialMoveSpot( inPlayer );
            
            x = p.x;
            y = p.y;
            }
        
        posString = autoSprintf( "%d %d %%d %%d",          
                                 doneMoving,
                                 inPlayer->posForced );
        r.absolutePosX = x;
        r.absolutePosY = y;

        inPlayer->lastPlayerUpdateAbsolutePos.x = x;
        inPlayer->lastPlayerUpdateAbsolutePos.y = y;
        }
    
    SimpleVector<char> clothingListBuffer;
    
    for( int c=0; c<NUM_CLOTHING_PIECES; c++ ) {
        ObjectRecord *cObj = clothingByIndex( inPlayer->clothing, c );
        int id = 0;
        
        if( cObj != NULL ) {
            id = objectRecordToID( cObj );
            }
        
        char *idString = autoSprintf( "%d", hideIDForClient( id ) );
        
        clothingListBuffer.appendElementString( idString );
        delete [] idString;
        
        if( cObj != NULL && cObj->numSlots > 0 ) {
            
            for( int cc=0; cc<inPlayer->clothingContained[c].size(); cc++ ) {
                char *contString = 
                    autoSprintf( 
                        ",%d", 
                        hideIDForClient( 
                            inPlayer->
                            clothingContained[c].getElementDirect( cc ) ) );
                
                clothingListBuffer.appendElementString( contString );
                delete [] contString;
                }
            }

        if( c < NUM_CLOTHING_PIECES - 1 ) {
            clothingListBuffer.push_back( ';' );
            }
        }
    
    char *clothingList = clothingListBuffer.getElementString();


    char *deathReason;
    
    if( inDelete && inPlayer->deathReason != NULL ) {
        deathReason = stringDuplicate( inPlayer->deathReason );
        }
    else {
        deathReason = stringDuplicate( "" );
        }
    
    
    int heldYum = 0;
    
    if( inPlayer->holdingID > 0 &&
        isYummy( inPlayer, inPlayer->holdingID ) ) {
        heldYum = 1;
        }


    r.formatString = autoSprintf( 
        "%d %d %d %d %%d %%d %s %d %%d %%d %d "
        "%.2f %s %.2f %.2f %.2f %s %d %d %d %d%s\n",
        inPlayer->id,
        inPlayer->displayID,
        inPlayer->facingOverride,
        inPlayer->actionAttempt,
        //inPlayer->actionTarget.x - inRelativeToPos.x,
        //inPlayer->actionTarget.y - inRelativeToPos.y,
        holdingString,
        inPlayer->heldOriginValid,
        //inPlayer->heldOriginX - inRelativeToPos.x,
        //inPlayer->heldOriginY - inRelativeToPos.y,
        hideIDForClient( inPlayer->heldTransitionSourceID ),
        inPlayer->heat,
        posString,
        computeAge( inPlayer ),
        1.0 / getAgeRate(),
        computeMoveSpeed( inPlayer ),
        clothingList,
        inPlayer->justAte,
        hideIDForClient( inPlayer->justAteID ),
        inPlayer->responsiblePlayerID,
        heldYum,
        deathReason );
    
    delete [] deathReason;
    

    r.absoluteActionTarget = inPlayer->actionTarget;
    
    if( inPlayer->heldOriginValid ) {
        r.absoluteHeldOriginX = inPlayer->heldOriginX;
        r.absoluteHeldOriginY = inPlayer->heldOriginY;
        }
    else {
        // we set 0,0 to clear held origins in many places in the code
        // if we leave that as an absolute pos, our birth pos leaks through
        // when we make it birth-pos relative
        
        // instead, substitute our birth pos for all invalid held pos coords
        // to prevent this
        r.absoluteHeldOriginX = inPlayer->birthPos.x;
        r.absoluteHeldOriginY = inPlayer->birthPos.y;
        }
    
        

    inPlayer->justAte = false;
    inPlayer->justAteID = 0;
    
    // held origin only valid once
    inPlayer->heldOriginValid = 0;
    
    inPlayer->facingOverride = 0;
    inPlayer->actionAttempt = 0;

    delete [] holdingString;
    delete [] posString;
    delete [] clothingList;
    
    return r;
    }



// inDelete true to send X X for position
// inPartial gets update line for player's current possition mid-path
// positions in update line will be relative to inRelativeToPos
static char *getUpdateLine( LiveObject *inPlayer, GridPos inRelativeToPos,
                            GridPos inObserverPos,
                            char inDelete,
                            char inPartial = false ) {
    
    UpdateRecord r = getUpdateRecord( inPlayer, inDelete, inPartial );
    
    char *line = getUpdateLineFromRecord( &r, inRelativeToPos, inObserverPos );

    delete [] r.formatString;
    
    return line;
    }




// if inTargetID set, we only detect whether inTargetID is close enough to
// be hit
// otherwise, we find the lowest-id player that is hit and return that
static LiveObject *getHitPlayer( int inX, int inY,
                                 int inTargetID = -1,
                                 char inCountMidPath = false,
                                 int inMaxAge = -1,
                                 int inMinAge = -1,
                                 int *outHitIndex = NULL ) {
    GridPos targetPos = { inX, inY };

    int numLive = players.size();
                                    
    LiveObject *hitPlayer = NULL;
                                    
    for( int j=0; j<numLive; j++ ) {
        LiveObject *otherPlayer = 
            players.getElement( j );
        
        if( otherPlayer->error ) {
            continue;
            }
        
        if( otherPlayer->heldByOther ) {
            // ghost position of a held baby
            continue;
            }
        
        if( inMaxAge != -1 &&
            computeAge( otherPlayer ) > inMaxAge ) {
            continue;
            }

        if( inMinAge != -1 &&
            computeAge( otherPlayer ) < inMinAge ) {
            continue;
            }
        
        if( inTargetID != -1 &&
            otherPlayer->id != inTargetID ) {
            continue;
            }

        if( otherPlayer->xd == 
            otherPlayer->xs &&
            otherPlayer->yd ==
            otherPlayer->ys ) {
            // other player standing still
                                            
            if( otherPlayer->xd ==
                inX &&
                otherPlayer->yd ==
                inY ) {
                                                
                // hit
                hitPlayer = otherPlayer;
                if( outHitIndex != NULL ) {
                    *outHitIndex = j;
                    }
                break;
                }
            }
        else {
            // other player moving
                
            GridPos cPos = 
                computePartialMoveSpot( 
                    otherPlayer );
                                        
            if( equal( cPos, targetPos ) ) {
                // hit
                hitPlayer = otherPlayer;
                if( outHitIndex != NULL ) {
                    *outHitIndex = j;
                    }
                break;
                }
            else if( inCountMidPath ) {
                
                int c = computePartialMovePathStep( otherPlayer );

                // consider path step before and after current location
                for( int i=-1; i<=1; i++ ) {
                    int testC = c + i;
                    
                    if( testC >= 0 && testC < otherPlayer->pathLength ) {
                        cPos = otherPlayer->pathToDest[testC];
                 
                        if( equal( cPos, targetPos ) ) {
                            // hit
                            hitPlayer = otherPlayer;
                            if( outHitIndex != NULL ) {
                                *outHitIndex = j;
                                }
                            break;
                            }
                        }
                    }
                if( hitPlayer != NULL ) {
                    break;
                    }
                }
            }
        }

    return hitPlayer;
    }




static int countFertileMothers() {
    
    int barrierRadius = 
        SettingsManager::getIntSetting( 
            "barrierRadius", 250 );
    int barrierOn = SettingsManager::getIntSetting( 
        "barrierOn", 1 );
    
    int c = 0;
    
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *p = players.getElement( i );
        
        if( p->error ) {
            continue;
            }
        
        if( isFertileAge( p ) ) {
            if( barrierOn ) {
                // only fertile mothers inside the barrier
                GridPos pos = getPlayerPos( p );
                
                if( abs( pos.x ) < barrierRadius &&
                    abs( pos.y ) < barrierRadius ) {
                    c++;
                    }
                }
            else {
                c++;
                }
            }
        }
    
    return c;
    }



static int countHelplessBabies() {
    
    int barrierRadius = 
        SettingsManager::getIntSetting( 
            "barrierRadius", 250 );
    int barrierOn = SettingsManager::getIntSetting( 
        "barrierOn", 1 );
    
    int c = 0;
    
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *p = players.getElement( i );
        
        if( p->error ) {
            continue;
            }

        if( computeAge( p ) < defaultActionAge ) {
            if( barrierOn ) {
                // only babies inside the barrier
                GridPos pos = getPlayerPos( p );
                
                if( abs( pos.x ) < barrierRadius &&
                    abs( pos.y ) < barrierRadius ) {
                    c++;
                    }
                }
            else {
                c++;
                }
            }
        }
    
    return c;
    }




static int countFamilies() {
    
    int barrierRadius = 
        SettingsManager::getIntSetting( 
            "barrierRadius", 250 );
    int barrierOn = SettingsManager::getIntSetting( 
        "barrierOn", 1 );
    
    SimpleVector<int> uniqueLines;

    
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *p = players.getElement( i );
        
        if( p->error ) {
            continue;
            }
        if( p->isTutorial ) {
            continue;
            }    
        if( p->vogMode ) {
            continue;
            }
        if( p->curseStatus.curseLevel > 0 ) {
            continue;
            }

        int lineageEveID = p->lineageEveID;
            
        if( uniqueLines.getElementIndex( lineageEveID ) == -1 ) {
            
            if( barrierOn ) {
                // only those inside the barrier
                GridPos pos = getPlayerPos( p );
                
                if( abs( pos.x ) < barrierRadius &&
                    abs( pos.y ) < barrierRadius ) {
                    uniqueLines.push_back( lineageEveID );
                    }
                }
            else {
                uniqueLines.push_back( lineageEveID );
                }
            }
        }
    
    return uniqueLines.size();
    }



static char isEveWindow() {
    
    if( players.size() <=
        SettingsManager::getIntSetting( "minActivePlayersForEveWindow", 15 ) ) {
        // not enough players
        // always Eve window
        
        // new window starts if we ever get enough players again
        eveWindowStart = 0;
        
        return true;
        }

    if( eveWindowStart == 0 ) {
        // start window now
        eveWindowStart = Time::getCurrentTime();
        return true;
        }
    else {
        double secSinceStart = Time::getCurrentTime() - eveWindowStart;
        
        if( secSinceStart >
            SettingsManager::getIntSetting( "eveWindowSeconds", 3600 ) ) {
            return false;
            }
        return true;
        }
    }



static void triggerApocalypseNow() {
    apocalypseTriggered = true;
    
    // restart Eve window, and let this player be the
    // first new Eve
    eveWindowStart = 0;
    
    // reset other apocalypse trigger
    lastBabyPassedThresholdTime = 0;
    }



static int countLivingChildren( int inMotherID ) {
    int count = 0;
    
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( o->parentID == inMotherID && ! o->error ) {
            count ++;
            }
        }
    return count;
    }


typedef struct ForceSpawnRecord {
        GridPos pos;
        double age;
        char *firstName;
        char *lastName;
        int displayID;
        int hatID;
        int tunicID;
        int bottomID;
        int frontShoeID;
        int backShoeID;
        int backpackID;
        int holdingID;
    } ForceSpawnRecord;



// strings in outRecordToFill destroyed by caller
char getForceSpawn( char *inEmail, ForceSpawnRecord *outRecordToFill ) {
    char *cont = SettingsManager::getSettingContents( "forceSpawnAccounts" );
    
    if( cont == NULL ) {
        return false;
        }
    int numParts;
    char **lines = split( cont, "\n", &numParts );

    delete [] cont;
    
    char found = false;

    for( int i=0; i<numParts; i++ ) {
        
        if( strstr( lines[i], inEmail ) == lines[i] ) {
            // matches email

            char emailBuff[100];
            
            int on = 0;
            
            sscanf( lines[i],
                    "%99s %d", emailBuff, &on );

            if( on == 1 ) {
                
                outRecordToFill->firstName = new char[20];
                outRecordToFill->lastName = new char[20];
                

                int numRead = sscanf( 
                    lines[i],
                    "%99s %d %d,%d %lf %19s %19s %d %d %d %d %d %d %d %d", 
                    emailBuff, &on,
                    &outRecordToFill->pos.x,
                    &outRecordToFill->pos.y,
                    &outRecordToFill->age,
                    outRecordToFill->firstName,
                    outRecordToFill->lastName,
                    &outRecordToFill->displayID,
                    &outRecordToFill->hatID,
                    &outRecordToFill->tunicID,
                    &outRecordToFill->bottomID,
                    &outRecordToFill->frontShoeID,
                    &outRecordToFill->backShoeID,
                    &outRecordToFill->backpackID,
                    &outRecordToFill->holdingID );
                
                if( numRead == 15 ) {
                    found = true;
                    }
                else {
                    delete [] outRecordToFill->firstName;
                    delete [] outRecordToFill->lastName;
                    }
                }
            break;
            }
        }


    for( int i=0; i<numParts; i++ ) {
        delete [] lines[i];
        }
    delete [] lines;
    
    return found;
    }





// for placement of tutorials out of the way 
static int maxPlacementX = 5000000;

// tutorial is alwasy placed 400,000 to East of furthest birth/Eve
// location
static int tutorialOffsetX = 400000;


// each subsequent tutorial gets put in a diferent place
static int tutorialCount = 0;



// fill this with emails that should also affect lineage ban
// if any twin in group is banned, all should be
static SimpleVector<char*> tempTwinEmails;

static char nextLogInTwin = false;

static int firstTwinID = -1;

// FNV-1a Hashing algorithm
uint64_t fnv1aHash(std::string &s, const uint64_t FNV_init = 0xcbf29ce484222325u) {
    const uint64_t FNV_prime = 0x00000100000001b3u;

    uint64_t hash = FNV_init;
    for (auto c : s) {
        hash ^= c;
        hash *= FNV_prime;
    }

    return hash;
};

// returns ID of new player,
// or -1 if this player reconnected to an existing ID
int processLoggedInPlayer( char inAllowReconnect,
                           Socket *inSock,
                           SimpleVector<char> *inSockBuffer,
                           char *inEmail,
                           //passing the whole thing for the seed and famTarget
                           FreshConnection *connection,
                           int inTutorialNumber,
                           CurseStatus inCurseStatus,
                           PastLifeStats inLifeStats,
                           float inFitnessScore,
                           // set to -2 to force Eve
                           int inForceParentID = -1,
                           int inForceDisplayID = -1,
                           GridPos *inForcePlayerPos = NULL) {
    

    usePersonalCurses = SettingsManager::getIntSetting( "usePersonalCurses",
                                                        0 );
    
    if( usePersonalCurses ) {
        // ignore what old curse system said
        inCurseStatus.curseLevel = 0;
        inCurseStatus.excessPoints = 0;
        
        initPersonalCurseTest( inEmail );
        
        for( int p=0; p<players.size(); p++ ) {
            LiveObject *o = players.getElement( p );
        
            if( ! o->error && 
                ! o->isTutorial &&
                o->curseStatus.curseLevel == 0 &&
                strcmp( o->email, inEmail ) != 0 ) {

                // non-tutorial, non-cursed, non-us player
                addPersonToPersonalCurseTest( o->email, inEmail,
                                              getPlayerPos( o ) );
                }
            }
        }
    


    // new behavior:
    // allow this new connection from same
    // email (most likely a re-connect
    // by same person, when the old connection
    // hasn't broken on our end yet)
    
    // to make it work, force-mark
    // the old connection as broken
    for( int p=0; p<players.size(); p++ ) {
        LiveObject *o = players.getElement( p );
        
        if( ! o->error && 
            o->connected && 
            strcmp( o->email, inEmail ) == 0 ) {
            
            setPlayerDisconnected( o, "Authentic reconnect received", __func__, __LINE__ );
            
            break;
            }
        }



    // see if player was previously disconnected
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( ! o->error && ! o->connected &&
            strcmp( o->email, inEmail ) == 0 ) {

            if( ! inAllowReconnect ) {
                // trigger an error for them, so they die and are removed
                o->error = true;
                o->errorCauseString = "Reconnected as twin";
                break;
                }
            
            // else allow them to reconnect to existing life

            // give them this new socket and buffer
            if( o->sock != NULL ) {
                delete o->sock;
                o->sock = NULL;
                }
            if( o->sockBuffer != NULL ) {
                delete o->sockBuffer;
                o->sockBuffer = NULL;
                }
            
            o->sock = inSock;
            o->sockBuffer = inSockBuffer;
            
            // they are connecting again, need to send them everything again
            o->firstMapSent = false;
            o->firstMessageSent = false;
            o->inFlight = false;

            o->foodUpdate = true;
            
            o->connected = true;
            o->cravingKnown = false;
            
            o->curseTokenUpdate = true;
            
            if( o->heldByOther ) {
                // they're held, so they may have moved far away from their
                // original location
                
                // their first PU on reconnect should give an estimate of this
                // new location
                
                LiveObject *holdingPlayer = 
                    getLiveObject( o->heldByOtherID );
                
                if( holdingPlayer != NULL ) {
                    o->xd = holdingPlayer->xd;
                    o->yd = holdingPlayer->yd;
                    
                    o->xs = holdingPlayer->xs;
                    o->ys = holdingPlayer->ys;
                    }
                }
            
            AppLog::infoF( "Player %d (%s) has reconnected.",
                           o->id, o->email );

            delete [] inEmail;
            
            return -1;
            }
        }
    


    // a baby needs to be born

    char eveWindow = isEveWindow();
    char forceGirl = false;
    
    int familyLimitAfterEveWindow = SettingsManager::getIntSetting( 
            "familyLimitAfterEveWindow", 15 );

    int cM = countFertileMothers();
    int cB = countHelplessBabies();
    int cFam = countFamilies();

    if( ! eveWindow ) {
        
        float ratio = SettingsManager::getFloatSetting( 
            "babyMotherApocalypseRatio", 6.0 );
        
        if( cM == 0 || (float)cB / (float)cM >= ratio ) {
            // too many babies per mother inside barrier

            triggerApocalypseNow();
            }
        else {
            int minFertile = players.size() / 15;
            if( minFertile < 2 ) {
                minFertile = 2;
                }
            if( cM < minFertile ) {
                // less than 1/15 of the players are fertile mothers
                forceGirl = true;
                }
            }

        if( !apocalypseTriggered && familyLimitAfterEveWindow > 0 ) {
            
            // there's a family limit
            // see if we passed it
            
            if( cFam > familyLimitAfterEveWindow ) {
                // too many families
                
                // that means we've reach a state where no one is surviving
                // and there are lots of eves scrounging around
                triggerApocalypseNow();
                }
            }
            
        }

    
    int barrierRadius = SettingsManager::getIntSetting( "barrierRadius", 250 );
    int barrierOn = SettingsManager::getIntSetting( "barrierOn", 1 );
    

    // reload these settings every time someone new connects
    // thus, they can be changed without restarting the server
    minFoodDecrementSeconds = 
        SettingsManager::getFloatSetting( "minFoodDecrementSeconds", 5.0f );
    
    maxFoodDecrementSeconds = 
        SettingsManager::getFloatSetting( "maxFoodDecrementSeconds", 20 );

    newPlayerFoodEatingBonus = 
        SettingsManager::getIntSetting( "newPlayerFoodEatingBonus", 5 );
    newPlayerFoodDecrementSecondsBonus =
        SettingsManager::getFloatSetting( "newPlayerFoodDecrementSecondsBonus",
                                          8 );
    newPlayerFoodBonusHalfLifeSeconds =
        SettingsManager::getFloatSetting( "newPlayerFoodBonusHalfLifeSeconds",
                                          36000 );

    babyBirthFoodDecrement = 
        SettingsManager::getIntSetting( "babyBirthFoodDecrement", 10 );


    eatBonus = 
        SettingsManager::getIntSetting( "eatBonus", 0 );
        
    useCurseWords = 
        SettingsManager::getIntSetting( "useCurseWords", 1 );

    minActivePlayersForLanguages =
        SettingsManager::getIntSetting( "minActivePlayersForLanguages", 15 );

    canYumChainBreak = SettingsManager::getIntSetting( "canYumChainBreak", 0 );
    

    
    minAgeForCravings = SettingsManager::getDoubleSetting( "minAgeForCravings",
                                                           10 );
    
    eatEverythingMode = SettingsManager::getIntSetting( "eatEverythingMode", 0 );
    
    // change the setting from cravings
    eatEverythingModeEnabled = eatEverythingMode;
    

    numConnections ++;
                
    LiveObject newObject;

    // record player's custom map size
    newObject.mMapD = connection->mMapD; 

    newObject.email = inEmail;
    newObject.origEmail = NULL;
    
    newObject.lastSidsBabyEmail = NULL;

    newObject.lastBabyEmail = NULL;

    newObject.cravingFood = noCraving;
    newObject.cravingFoodYumIncrement = 0;
    newObject.cravingKnown = false;
    
    newObject.id = nextID;
    nextID++;




    if( familyDataLogFile != NULL ) {
        int eveCount = 0;
        int inCount = 0;
        
        double ageSum = 0;
        int ageSumCount = 0;
        
        for( int i=0; i<players.size(); i++ ) {
            LiveObject *o = players.getElement( i );
        
            if( ! o->error && o->connected ) {
                if( o->parentID == -1 ) {
                    eveCount++;
                    }
                if( barrierOn ) {
                    // only those inside the barrier
                    GridPos pos = getPlayerPos( o );
                
                    if( abs( pos.x ) < barrierRadius &&
                        abs( pos.y ) < barrierRadius ) {
                        inCount++;
                        
                        ageSum += computeAge( o );
                        ageSumCount++;
                        }
                    }
                else {
                    ageSum += computeAge( o );
                    ageSumCount++;
                    }
                }
            }
        
        double averageAge = 0;
        if( ageSumCount > 0 ) {
            averageAge = ageSum / ageSumCount;
            }
        
        fprintf( familyDataLogFile,
                 "%.2f nid:%d fam:%d mom:%d bb:%d plr:%d eve:%d rft:%d "
                 "avAge:%.2f\n",
                 Time::getCurrentTime(), newObject.id, 
                 cFam, cM, cB,
                 players.size(),
                 eveCount,
                 inCount,
                 averageAge );
        }


    
    newObject.fitnessScore = inFitnessScore;
    

    SettingsManager::setSetting( "nextPlayerID",
                                 (int)nextID );


    newObject.responsiblePlayerID = -1;
    
    newObject.displayID = getRandomPersonObject();
    
    newObject.isEve = false;
    
    newObject.isTutorial = false;
    
    if( inTutorialNumber > 0 ) {
        newObject.isTutorial = true;
        }

    newObject.trueStartTimeSeconds = Time::getCurrentTime();
    newObject.lifeStartTimeSeconds = newObject.trueStartTimeSeconds;
                            

    newObject.lastSayTimeSeconds = Time::getCurrentTime();
    

    newObject.heldByOther = false;
    newObject.everHeldByParent = false;
    

    int numPlayers = players.size();

    SimpleVector<LiveObject*> parentChoices;
    
    int numBirthLocationsCurseBlocked = 0;

    int numOfAge = 0;
    

    // first, find all mothers that could possibly have us

    // three passes, once with birth cooldown limit and lineage limits on, 
    // then more passes with them off (if needed)
    char checkCooldown = true;
    

    for( int p=0; p<2; p++ ) {
    
        for( int i=0; i<numPlayers; i++ ) {
            LiveObject *player = players.getElement( i );
            
            if( player->error ) {
                continue;
                }
            
            if( player->isTutorial ) {
                continue;
                }
            
            if( player->vogMode ) {
                continue;
                }
                
            //skips over solo players who declare themselves infertile
            if( player->declaredInfertile ) {
                continue;
                }
                
            //we specified a family we wanna be born into, skip others
            if( connection->famTarget != NULL ) {
                if( player->familyName != NULL ) {
                    std::string famTarget( connection->famTarget );
                    std::string familyName( player->familyName );
                    if( familyName != famTarget ) continue;
                    }
                else {
                    continue;
                    }
                }
            

            //GridPos motherPos = getPlayerPos( player );
                
            
            if( player->lastSidsBabyEmail != NULL &&
                strcmp( player->lastSidsBabyEmail,
                        newObject.email ) == 0 ) {
                // this baby JUST committed SIDS for this mother
                // skip her
                // (don't ever send SIDS baby to same mother twice in a row)
                continue;
                }

            if( isFertileAge( player ) ) {
                numOfAge ++;
                
                if( checkCooldown &&
                    Time::timeSec() < player->birthCoolDown ) {    
                    continue;
                    }
                
                GridPos motherPos = getPlayerPos( player );

                if( usePersonalCurses &&
                    isBirthLocationCurseBlocked( newObject.email, 
                                                 motherPos ) ) {
                    // this spot forbidden
                    // because someone nearby cursed new player
                    numBirthLocationsCurseBlocked++;
                    continue;
                    }
            
                // test any twins also
                char twinBanned = false;
                for( int s=0; s<tempTwinEmails.size(); s++ ) {
                    if( usePersonalCurses &&
                        // non-cached version for twin emails
                        // (otherwise, we interfere with caching done
                        //  for our email)
                        isBirthLocationCurseBlockedNoCache( 
                            tempTwinEmails.getElementDirect( s ), 
                            motherPos ) ) {
                        twinBanned = true;
                        
                        numBirthLocationsCurseBlocked++;
                        
                        break;
                        }
                    }
                
                if( twinBanned ) {
                    continue;
                    }
                
            
                if( ( inCurseStatus.curseLevel <= 0 && 
                      player->curseStatus.curseLevel <= 0 ) 
                    || 
                    ( inCurseStatus.curseLevel > 0 && 
                      player->curseStatus.curseLevel > 0 ) ) {
                    // cursed babies only born to cursed mothers
                    // non-cursed babies never born to cursed mothers
                    parentChoices.push_back( player );
                    }
                }
            }
        
        

        if( p == 0 ) {
            if( parentChoices.size() > 0 || numOfAge == 0 ) {
                // found some mothers off-cool-down, 
                // or there are none at all
                // skip second pass
                break;
                }
            
            // else found no mothers (but some on cool-down?)
            // start over with cooldowns off
            
            AppLog::infoF( 
                "Trying to place new baby %s, out of %d fertile moms, "
                "all are on cooldown, lineage banned, or curse blocked.  "
                "Trying again ignoring cooldowns.", newObject.email, numOfAge );
            
            checkCooldown = false;
            numBirthLocationsCurseBlocked = 0;
            numOfAge = 0;
            }
        
        }
    
    


    if( parentChoices.size() == 0 && numBirthLocationsCurseBlocked > 0 ) {
        // they are blocked from being born EVERYWHERE by curses

        AppLog::infoF( "No available mothers, and %d are curse blocked, "
                       "sending a new Eve to donkeytown",
                       numBirthLocationsCurseBlocked );

        // d-town
        inCurseStatus.curseLevel = 1;
        inCurseStatus.excessPoints = 1;
        }

    

    if( inTutorialNumber > 0 ) {
        // Tutorial always played full-grown
        parentChoices.deleteAll();
        }

    if( inForceParentID == -2 ) {
        // force eve
        parentChoices.deleteAll();
        }
    else if( inForceParentID > -1 ) {
        // force parent choice
        parentChoices.deleteAll();
        
        LiveObject *forcedParent = getLiveObject( inForceParentID );
        
        if( forcedParent != NULL ) {
            parentChoices.push_back( forcedParent );
            }
        }
    
    
    if( !newObject.isTutorial )
    if( connection->famTarget != NULL && parentChoices.size() == 0 ) {
        // -2 means failure to be born due to famTarget restriction
        return -2;
        }
    
    char forceSpawn = false;
    ForceSpawnRecord forceSpawnInfo;
    
    if( SettingsManager::getIntSetting( "forceAllPlayersEve", 0 ) ) {
        parentChoices.deleteAll();
        }
    else {
        forceSpawn = getForceSpawn( inEmail, &forceSpawnInfo );
    
        if( forceSpawn ) {
            parentChoices.deleteAll();
            }
        }
        
    if( connection->hashedSpawnSeed != 0 && SettingsManager::getIntSetting( "forceEveOnSeededSpawn", 0 ) ) {
        parentChoices.deleteAll();
        }




    newObject.parentChainLength = 1;

    if( parentChoices.size() == 0 ) {
        // new Eve
        // she starts almost full grown

        newObject.isEve = true;
        newObject.lineageEveID = newObject.id;
        
        newObject.lifeStartTimeSeconds -= 14 * ( 1.0 / getAgeRate() );
        
        // she starts off craving a food right away
        newObject.cravingFood = getCravedFood( newObject.lineageEveID,
                                               newObject.parentChainLength );
        // initilize increment
        newObject.cravingFoodYumIncrement = 1;

        int femaleID = getRandomFemalePersonObject();
        
        if( femaleID != -1 ) {
            newObject.displayID = femaleID;
            }

        }
    
                
    // else player starts as newborn
                

    newObject.foodCapModifier = 1.0;

    newObject.fever = 0;

    // start full up to capacity with food
    newObject.foodStore = computeFoodCapacity( &newObject );

    newObject.drunkenness = 0;
    newObject.drunkennessEffectETA = 0;
    newObject.drunkennessEffect = false;
    
    newObject.tripping = false;
    newObject.gonnaBeTripping = false;
    newObject.trippingEffectStartTime = 0;
    newObject.trippingEffectETA = 0;
    

    if( ! newObject.isEve ) {
        // babies start out almost starving
        newObject.foodStore = 2;
        }
    
    if( newObject.isTutorial && newObject.foodStore > 5 ) {
        // so they can practice eating at the beginning of the tutorial
        newObject.foodStore -= 4;
        }
    
    double currentTime = Time::getCurrentTime();
    

    newObject.envHeat = targetHeat;
    newObject.bodyHeat = targetHeat;
    newObject.biomeHeat = targetHeat;
    newObject.lastBiomeHeat = targetHeat;
    newObject.heat = 0.5;
    newObject.heatUpdate = false;
    newObject.lastHeatUpdate = currentTime;
    newObject.isIndoors = false;
    

    
    
                
    newObject.foodUpdate = true;
    newObject.lastAteID = 0;
    newObject.lastAteFillMax = 0;
    newObject.justAte = false;
    newObject.justAteID = 0;
    
    newObject.yummyBonusStore = 0;

    newObject.lastReportedFoodCapacity = 0;

    newObject.clothing = getEmptyClothingSet();

    for( int c=0; c<NUM_CLOTHING_PIECES; c++ ) {
        newObject.clothingEtaDecay[c] = 0;
        }
    
    newObject.xs = 0;
    newObject.ys = 0;
    newObject.xd = 0;
    newObject.yd = 0;
    
    newObject.facingLeft = 0;
    newObject.lastFlipTime = currentTime;
    
    newObject.lastRegionLookTime = 0;
    newObject.playerCrossingCheckTime = 0;
    
    
    LiveObject *parent = NULL;

    char placed = false;
    
    if( parentChoices.size() > 0 ) {
        placed = true;
        
        if( newObject.isEve ) {
            // spawned next to random existing player
            int parentIndex = 
                randSource.getRandomBoundedInt( 0,
                                                parentChoices.size() - 1 );
            
            parent = parentChoices.getElementDirect( parentIndex );
            }
        else {
            // baby


            
            // filter parent choices by this baby's skip list
            SimpleVector<LiveObject *> 
                filteredParentChoices( parentChoices.size() );
            
            for( int i=0; i<parentChoices.size(); i++ ) {
                LiveObject *p = parentChoices.getElementDirect( i );
                
                if( ! isSkipped( inEmail, p->lineageEveID ) ) {
                    filteredParentChoices.push_back( p );
                    }
                }

            if( filteredParentChoices.size() == 0 ) {
                // baby has skipped everyone
                
                // clear their list and let them start over again
                clearSkipList( inEmail );
                
                filteredParentChoices.push_back_other( &parentChoices );
                }
            

            
            // pick random mother from a weighted distribution based on 
            // each mother's temperature
            
            // AND each mother's current YUM multiplier
            
            int maxYumMult = 1;

            for( int i=0; i<filteredParentChoices.size(); i++ ) {
                LiveObject *p = filteredParentChoices.getElementDirect( i );
                
                int yumMult = p->yummyFoodChain.size() - 1;
                
                if( yumMult < 0 ) {
                    yumMult = 0;
                    }
                
                if( yumMult > maxYumMult ) {
                    maxYumMult = yumMult;
                    }
                }
            
            // 0.5 temp is worth .5 weight
            // 1.0 temp and 0 are worth 0 weight
            
            // max YumMult worth same that perfect temp is worth (0.5 weight)

            double totalWeight = 0;
            
            SimpleVector<double> filteredParentChoiceWeights;
            
            for( int i=0; i<filteredParentChoices.size(); i++ ) {
                LiveObject *p = filteredParentChoices.getElementDirect( i );

                // temp part of weight
                double thisMotherWeight = 0.5 - fabs( p->heat - 0.5 );
                

                int yumMult = p->yummyFoodChain.size() - 1;
                                
                if( yumMult < 0 ) {
                    yumMult = 0;
                    }

                // yum mult part of weight
                thisMotherWeight += 0.5 * yumMult / (double) maxYumMult;
                
                filteredParentChoiceWeights.push_back( thisMotherWeight );
                
                totalWeight += thisMotherWeight;
                }

            double choice = 
                randSource.getRandomBoundedDouble( 0, totalWeight );
            
            
            totalWeight = 0;
            
            for( int i=0; i<filteredParentChoices.size(); i++ ) {
                LiveObject *p = filteredParentChoices.getElementDirect( i );
                
                totalWeight += 
                    filteredParentChoiceWeights.getElementDirect( i );

                if( totalWeight >= choice ) {
                    parent = p;
                    break;
                    }                
                }
            }
        

        
        if( ! newObject.isEve ) {
            // mother giving birth to baby
            // take a ton out of her food store

            int min = 4;
            if( parent->foodStore < min ) {
                min = parent->foodStore;
                }
            parent->foodStore -= babyBirthFoodDecrement;
            if( parent->foodStore < min ) {
                parent->foodStore = min;
                }

            parent->foodDecrementETASeconds +=
                computeFoodDecrementTimeSeconds( parent );
            
            parent->foodUpdate = true;
            

            // only set race if the spawn-near player is our mother
            // otherwise, we are a new Eve spawning next to a baby
            
            timeSec_t curTime = Time::timeSec();
            
            parent->babyBirthTimes->push_back( curTime );
            parent->babyIDs->push_back( newObject.id );
            
            if( parent->lastBabyEmail != NULL ) {
                delete [] parent->lastBabyEmail;
                }
            parent->lastBabyEmail = stringDuplicate( newObject.email );
            

            // set cool-down time before this worman can have another baby
            parent->birthCoolDown = pickBirthCooldownSeconds() + curTime;

            ObjectRecord *parentObject = getObject( parent->displayID );

            // pick race of child
            int numRaces;
            int *races = getRaces( &numRaces );
        
            int parentRaceIndex = -1;
            
            for( int i=0; i<numRaces; i++ ) {
                if( parentObject->race == races[i] ) {
                    parentRaceIndex = i;
                    break;
                    }
                }
            

            if( parentRaceIndex != -1 ) {
                
                int childRace = parentObject->race;
                
                char forceDifferentRace = false;

                if( getRaceSize( parentObject->race ) < 3 ) {
                    // no room in race for diverse family members
                    
                    // pick a different race for child to ensure village 
                    // diversity
                    // (otherwise, almost everyone is going to look the same)
                    forceDifferentRace = true;
                    }
                
                // everyone has a small chance of having a neighboring-race
                // baby, even if not forced by parent's small race size
                if( forceDifferentRace ||
                    randSource.getRandomDouble() > 
                    childSameRaceLikelihood ) {
                    
                    // different race than parent
                    
                    int offset = 1;
                    
                    if( randSource.getRandomBoolean() ) {
                        offset = -1;
                        }
                    int childRaceIndex = parentRaceIndex + offset;
                    
                    // don't wrap around
                    // but push in other direction instead
                    if( childRaceIndex >= numRaces ) {
                        childRaceIndex = numRaces - 2;
                        }
                    if( childRaceIndex < 0 ) {
                        childRaceIndex = 1;
                        }
                    
                    // stay in bounds
                    if( childRaceIndex >= numRaces ) {
                        childRaceIndex = numRaces - 1;
                        }
                    

                    childRace = races[ childRaceIndex ];
                    }
                
                if( childRace == parentObject->race ) {
                    
                    if( countYoungFemalesInLineage( parent->lineageEveID ) <
                        SettingsManager::getIntSetting( "minYoungFemalesToForceGirl", 2 ) ) {
                        forceGirl = true;
                        }
                    
                    newObject.displayID = getRandomFamilyMember( 
                        parentObject->race, parent->displayID, familySpan,
                        forceGirl );
                    }
                else {
                    newObject.displayID = 
                        getRandomPersonObjectOfRace( childRace );
                    }
            
                }
        
            delete [] races;
            }
        
        if( parent->xs == parent->xd && 
            parent->ys == parent->yd ) {
                        
            // stationary parent
            newObject.xs = parent->xs;
            newObject.ys = parent->ys;
                        
            newObject.xd = parent->xs;
            newObject.yd = parent->ys;
            }
        else {
            // find where parent is along path
            GridPos cPos = computePartialMoveSpot( parent );
                        
            newObject.xs = cPos.x;
            newObject.ys = cPos.y;
                        
            newObject.xd = cPos.x;
            newObject.yd = cPos.y;
            }
        
        if( newObject.xs > maxPlacementX ) {
            maxPlacementX = newObject.xs;
            }
        }
    else if( inTutorialNumber > 0 ) {
        
        int startX = maxPlacementX + tutorialOffsetX;
        int startY = tutorialCount * 25;

        newObject.xs = startX;
        newObject.ys = startY;
        
        newObject.xd = startX;
        newObject.yd = startY;

        char *mapFileName = autoSprintf( "tutorial%d.txt", inTutorialNumber );
        
        placed = loadTutorialStart( &( newObject.tutorialLoad ),
                                    mapFileName, startX, startY );
        
        delete [] mapFileName;

        tutorialCount ++;

        int maxPlayers = 
            SettingsManager::getIntSetting( "maxPlayers", 200 );

        if( tutorialCount > maxPlayers ) {
            // wrap back to 0 so we don't keep getting farther
            // and farther away on map if server runs for a long time.

            // The earlier-placed tutorials are over by now, because
            // we can't have more than maxPlayers tutorials running at once
            
            tutorialCount = 0;
            }
        }
    
    
    if( !placed ) {
        // tutorial didn't happen if not placed
        newObject.isTutorial = false;
        
        char allowEveRespawn = true;
        
        if( numOfAge >= 4 ) {
            // there are at least 4 fertile females on the server
            // why is this player spawning as Eve?
            // they must be on lineage ban everywhere
            // (and they are NOT a solo player on an empty server)
            // don't allow them to spawn back at their last old-age Eve death
            // location.
            allowEveRespawn = false;
            }

        // else starts at civ outskirts (lone Eve)
        
        SimpleVector<GridPos> otherPeoplePos( numPlayers );


        // consider players to be near Eve location that match
        // Eve's curse status
        char seekingCursed = false;
        
        if( inCurseStatus.curseLevel > 0 ) {
            seekingCursed = true;
            }
        

        for( int i=0; i<numPlayers; i++ ) {
            LiveObject *player = players.getElement( i );
            
            if( player->error || 
                ! player->connected ||
                player->isTutorial ||
                player->vogMode ) {
                continue;
                }

            if( seekingCursed && player->curseStatus.curseLevel <= 0 ) {
                continue;
                }
            else if( ! seekingCursed &&
                     player->curseStatus.curseLevel > 0 ) {
                continue;
                }

            GridPos p = { player->xs, player->ys };
            otherPeoplePos.push_back( p );
            }
        

        int startX, startY;
        getEvePosition( newObject.email, 
                        newObject.id, &startX, &startY, 
                        &otherPeoplePos, allowEveRespawn );

        if( inCurseStatus.curseLevel > 0 ) {
            // keep cursed players away

            // 20K away in X and 20K away in Y, pushing out away from 0
            // in both directions

            if( startX > 0 )
                startX += 20000;
            else
                startX -= 20000;
            
            if( startY > 0 )
                startY += 20000;
            else
                startY -= 20000;
            }
        

        if( SettingsManager::getIntSetting( "forceEveLocation", 0 ) && inCurseStatus.curseLevel == 0 ) {

            startX = 
                SettingsManager::getIntSetting( "forceEveLocationX", 0 );
            startY = 
                SettingsManager::getIntSetting( "forceEveLocationY", 0 );
            }
        
        uint64_t tempHashedSpawnSeed;
        int useSeedList = SettingsManager::getIntSetting( "useSeedList", 0 );
        //pick a random seed from a list to be the default spawn
        if ( useSeedList && connection->hashedSpawnSeed == 0 ) {
            
            //parse the seeds
            SimpleVector<char *> *list = 
                SettingsManager::getSetting( 
                    "defaultSeedList" );
            
            //chose a random seed from the list
            int seedIndex = 
                randSource.getRandomBoundedInt( 0, list->size() - 1 );
            
            char *choseSeed;
            for( int i=0; i<list->size(); i++ ) {
                if( seedIndex == i ) {
                    choseSeed = list->getElementDirect( i );
                    break;
                    }
                }
                
            std::string seed( choseSeed );
            
            //convert and apply seed hash (copy pasted code)
            //make this a separate method in the future to prevent redundancy
            
            // Get the substr from one after the seed delim
            char *sSeed = SettingsManager::getStringSetting("seedPepper", "default pepper");
            std::string seedPepper { sSeed };
            delete [] sSeed;
            
            tempHashedSpawnSeed =
                fnv1aHash(seed, fnv1aHash(seedPepper));
          }
        else {
            //use defalt seed configuration
            tempHashedSpawnSeed = connection->hashedSpawnSeed;
        }

        if( tempHashedSpawnSeed != 0 ) {
            // Get bounding box from setting, default to 10k
            int seedSpawnBoundingBox =
                SettingsManager::getIntSetting( "seedSpawnBoundingBox", 10000 );

            std::seed_seq ssq { tempHashedSpawnSeed };
            std::mt19937_64 mt { ssq };

            std::uniform_int_distribution<int> dist( -seedSpawnBoundingBox/2, seedSpawnBoundingBox/2 );

            startX = dist(mt);
            startY = dist(mt);

            AppLog::infoF( "Player %s seed evaluated to (%d,%d)",
                    newObject.email, startX, startY );
            }
        
        
        newObject.xs = startX;
        newObject.ys = startY;
        
        newObject.xd = startX;
        newObject.yd = startY;

        if( newObject.xs > maxPlacementX ) {
            maxPlacementX = newObject.xs;
            }
        }
    
    if ( SettingsManager::getIntSetting( "randomisePlayersObject", 0 ) ) {
        SimpleVector<int> *objectsPool =
            SettingsManager::getIntSettingMulti( "randomisePlayersObjectPool" );
        ObjectRecord *randomObject;
        int randomObjectIndex;
        int objectPoolSize = objectsPool->size();
        while ( randomObject == NULL ) {
            if( objectPoolSize > 0 ) {
                randomObjectIndex = randSource.getRandomBoundedInt( 0, objectPoolSize - 1 );
                randomObject = getObject( objectsPool->getElementDirect( randomObjectIndex ) );
                }
            else {
                randomObject = getObject( randSource.getRandomBoundedInt( 0, getMaxObjectID() ) );
                }
            }
        inForceDisplayID = randomObject->id;
        }    

    if( inForceDisplayID != -1 ) {
        newObject.displayID = inForceDisplayID;
        }

    if( inForcePlayerPos != NULL ) {
        int startX = inForcePlayerPos->x;
        int startY = inForcePlayerPos->y;
        
        newObject.xs = startX;
        newObject.ys = startY;
        
        newObject.xd = startX;
        newObject.yd = startY;

        if( newObject.xs > maxPlacementX ) {
            maxPlacementX = newObject.xs;
            }
        }
    

    
    if( parent == NULL ) {
        // Eve
        int forceID = SettingsManager::getIntSetting( "forceEveObject", 0 );
    
        if( forceID > 0 ) {
            newObject.displayID = forceID;
            }
        
        
        float forceAge = SettingsManager::getFloatSetting( "forceEveAge", 0.0 );
        
        if( forceAge > 0 ) {
            newObject.lifeStartTimeSeconds = 
                Time::getCurrentTime() - forceAge * ( 1.0 / getAgeRate() );
            }
        }
    

    newObject.holdingID = 0;


    if( areTriggersEnabled() ) {
        int id = getTriggerPlayerDisplayID( inEmail );
        
        if( id != -1 ) {
            newObject.displayID = id;
            
            newObject.lifeStartTimeSeconds = 
                Time::getCurrentTime() - 
                getTriggerPlayerAge( inEmail ) * ( 1.0 / getAgeRate() );
        
            GridPos pos = getTriggerPlayerPos( inEmail );
            
            newObject.xd = pos.x;
            newObject.yd = pos.y;
            newObject.xs = pos.x;
            newObject.ys = pos.y;
            newObject.xd = pos.x;
            
            newObject.holdingID = getTriggerPlayerHolding( inEmail );
            newObject.clothing = getTriggerPlayerClothing( inEmail );
            }
        }
    
    
    newObject.lineage = new SimpleVector<int>();
    
    newObject.name = NULL;
    newObject.displayedName = NULL;
    newObject.familyName = NULL;
    
    newObject.nameHasSuffix = false;
    newObject.lastSay = NULL;
    newObject.curseStatus = inCurseStatus;
    newObject.lifeStats = inLifeStats;
    
    // password-protected objects
    newObject.saidPassword = NULL;
    

    if( newObject.curseStatus.curseLevel == 0 &&
        hasCurseToken( inEmail ) ) {
        newObject.curseTokenCount = 1;
        }
    else {
        newObject.curseTokenCount = 0;
        }

    newObject.curseTokenUpdate = true;

    
    newObject.pathLength = 0;
    newObject.pathToDest = NULL;
    newObject.pathTruncated = 0;
    newObject.firstMapSent = false;
    newObject.lastSentMapX = 0;
    newObject.lastSentMapY = 0;
    newObject.moveStartTime = Time::getCurrentTime();
    newObject.moveTotalSeconds = 0;
    newObject.facingOverride = 0;
    newObject.actionAttempt = 0;
    newObject.actionTarget.x = 0;
    newObject.actionTarget.y = 0;
    newObject.holdingEtaDecay = 0;
    newObject.heldOriginValid = 0;
    newObject.heldOriginX = 0;
    newObject.heldOriginY = 0;

    newObject.heldGraveOriginX = 0;
    newObject.heldGraveOriginY = 0;
    newObject.heldGravePlayerID = 0;
    
    newObject.heldTransitionSourceID = -1;
    newObject.numContained = 0;
    newObject.containedIDs = NULL;
    newObject.containedEtaDecays = NULL;
    newObject.subContainedIDs = NULL;
    newObject.subContainedEtaDecays = NULL;
    newObject.embeddedWeaponID = 0;
    newObject.embeddedWeaponEtaDecay = 0;
    newObject.murderSourceID = 0;
    newObject.holdingWound = false;
    
    newObject.murderPerpID = 0;
    newObject.murderPerpEmail = NULL;
    
    newObject.deathSourceID = 0;
    
    newObject.everKilledAnyone = false;
    newObject.suicide = false;
    

    newObject.sock = inSock;
    newObject.sockBuffer = inSockBuffer;
    
    newObject.gotPartOfThisFrame = false;
    
    newObject.isNew = true;
    newObject.isNewCursed = false;
    newObject.firstMessageSent = false;
    newObject.inFlight = false;
    
    newObject.dying = false;
    newObject.dyingETA = 0;
    
    newObject.emotFrozen = false;
    newObject.emotUnfreezeETA = 0;
    newObject.emotFrozenIndex = 0;
    
    newObject.starving = false;

    newObject.connected = true;
    newObject.error = false;
    newObject.errorCauseString = "";
    
    newObject.lastActionTime = Time::getCurrentTime();
    newObject.isAFK = false;
    
    newObject.lastWrittenObjectScanTime = 0;
    newObject.lastWrittenObjectScanPos.x = 9999;
    newObject.lastWrittenObjectScanPos.y = 9999;
    
    newObject.customGraveID = -1;
    newObject.deathReason = NULL;
    
    newObject.deleteSent = false;
    newObject.deathLogged = false;
    newObject.newMove = false;
    
    newObject.lastPlayerUpdateAbsolutePos.x = 0;
    newObject.lastPlayerUpdateAbsolutePos.y = 0;
    

    newObject.posForced = false;
    newObject.waitingForForceResponse = false;
    
    // first move that player sends will be 2
    newObject.lastMoveSequenceNumber = 1;

    newObject.needsUpdate = false;
    newObject.updateSent = false;
    newObject.updateGlobal = false;
    
    newObject.babyBirthTimes = new SimpleVector<timeSec_t>();
    newObject.babyIDs = new SimpleVector<int>();
    
    newObject.birthCoolDown = 0;
    newObject.declaredInfertile = false;
    
    newObject.monumentPosSet = false;
    newObject.monumentPosSent = true;
    
    newObject.holdingFlightObject = false;

    newObject.vogMode = false;
    newObject.postVogMode = false;
    newObject.vogJumpIndex = 0;
    
    newObject.forceSpawn = false;

    newObject.forceFlightDestSetTime = 0;
                
    for( int i=0; i<HEAT_MAP_D * HEAT_MAP_D; i++ ) {
        newObject.heatMap[i] = 0;
        }

    
    newObject.parentID = -1;
    char *parentEmail = NULL;

    if( parent != NULL && isFertileAge( parent ) ) {
        // do not log babies that new Eve spawns next to as parents
        newObject.parentID = parent->id;
        parentEmail = parent->email;

        if( parent->familyName != NULL ) {
            newObject.familyName = stringDuplicate( parent->familyName );
            }

        newObject.lineageEveID = parent->lineageEveID;

        newObject.parentChainLength = parent->parentChainLength + 1;

        // mother
        newObject.lineage->push_back( newObject.parentID );

        
        // inherit mother's craving at time of birth
        newObject.cravingFood = parent->cravingFood;
        
        // increment for next generation
        newObject.cravingFoodYumIncrement = parent->cravingFoodYumIncrement + 1;
        

        // inherit last heard monument, if any, from parent
        if( babyInheritMonument ) {
            newObject.monumentPosSet = parent->monumentPosSet;
            newObject.lastMonumentPos = parent->lastMonumentPos;
            newObject.lastMonumentID = parent->lastMonumentID;
            if( newObject.monumentPosSet ) {
                newObject.monumentPosSent = false;
                }
            }
        
        for( int i=0; 
             i < parent->lineage->size() && 
                 i < maxLineageTracked - 1;
             i++ ) {
            
            newObject.lineage->push_back( 
                parent->lineage->getElementDirect( i ) );
            }

        if( strstr( newObject.email, "paxkiosk" ) ) {
            // whoa, this baby is a PAX player!
            // let the mother know
            sendGlobalMessage( 
                (char*)"YOUR BABY IS A NEW PLAYER FROM THE PAX EXPO BOOTH.**"
                "PLEASE HELP THEM LEARN THE GAME.  THANKS!  -JASON",
                parent );
            }
        else if( isUsingStatsServer() && 
                 ! newObject.lifeStats.error &&
                 ( newObject.lifeStats.lifeCount < 
                   SettingsManager::getIntSetting( "newPlayerLifeCount", 5 ) ||
                   newObject.lifeStats.lifeTotalSeconds < 
                   SettingsManager::getIntSetting( "newPlayerLifeTotalSeconds",
                                                   7200 ) ) ) {
            // a new player (not at a PAX kiosk)
            // let mother know
            char *motherMessage =  
                SettingsManager::getSettingContents( 
                    "newPlayerMessageForMother", "" );
            
            if( strcmp( motherMessage, "" ) != 0 ) {
                sendGlobalMessage( motherMessage, parent );
                }
            
            delete [] motherMessage;
            }
        }

    newObject.personalEatBonus = 0;
    newObject.personalFoodDecrementSecondsBonus = 0;

    if( 
        ! newObject.isTutorial &&
        isUsingStatsServer() &&
        ! newObject.lifeStats.error ) {
        
        int sec = newObject.lifeStats.lifeTotalSeconds;

        double halfLifeFactor = 
            pow( 0.5, sec / newPlayerFoodBonusHalfLifeSeconds );
        

        newObject.personalEatBonus = 
            lrint( halfLifeFactor * newPlayerFoodEatingBonus );
        
        newObject.personalFoodDecrementSecondsBonus =
            lrint( halfLifeFactor * newPlayerFoodDecrementSecondsBonus );
        }
    
    newObject.foodDecrementETASeconds =
        currentTime + 
        computeFoodDecrementTimeSeconds( &newObject );

        
    if( forceSpawn ) {
        newObject.forceSpawn = true;
        newObject.xs = forceSpawnInfo.pos.x;
        newObject.ys = forceSpawnInfo.pos.y;
        newObject.xd = forceSpawnInfo.pos.x;
        newObject.yd = forceSpawnInfo.pos.y;
        
        newObject.birthPos = forceSpawnInfo.pos;
        
        newObject.lifeStartTimeSeconds = 
            Time::getCurrentTime() -
            forceSpawnInfo.age * ( 1.0 / getAgeRate() );
        
        newObject.displayedName = autoSprintf( "%s %s", 
                                      forceSpawnInfo.firstName,
                                      forceSpawnInfo.lastName );
        newObject.displayID = forceSpawnInfo.displayID;
        
        newObject.clothing.hat = getObject( forceSpawnInfo.hatID, true );
        newObject.clothing.tunic = getObject( forceSpawnInfo.tunicID, true );
        newObject.clothing.bottom = getObject( forceSpawnInfo.bottomID, true );
        newObject.clothing.frontShoe = 
            getObject( forceSpawnInfo.frontShoeID, true );
        newObject.clothing.backShoe = 
            getObject( forceSpawnInfo.backShoeID, true );
        newObject.clothing.backpack = 
            getObject( forceSpawnInfo.backpackID, true );
        
        newObject.yummyBonusStore = 999;
        
        newObject.holdingID = getObject( forceSpawnInfo.holdingID, false )->id;

        delete [] forceSpawnInfo.firstName;
        delete [] forceSpawnInfo.lastName;
        }
    

    newObject.lastGlobalMessageTime = 0;
    

    newObject.birthPos.x = newObject.xd;
    newObject.birthPos.y = newObject.yd;
    
    newObject.originalBirthPos = newObject.birthPos;
    

    newObject.heldOriginX = newObject.xd;
    newObject.heldOriginY = newObject.yd;
    
    newObject.actionTarget = newObject.birthPos;



    newObject.ancestorIDs = new SimpleVector<int>();
    newObject.ancestorEmails = new SimpleVector<char*>();
    newObject.ancestorRelNames = new SimpleVector<char*>();
    newObject.ancestorLifeStartTimeSeconds = new SimpleVector<double>();
    newObject.ancestorLifeEndTimeSeconds = new SimpleVector<double>();
                                                  
    for( int j=0; j<players.size(); j++ ) {
        LiveObject *otherPlayer = players.getElement( j );
        
        if( otherPlayer->error ) {
            continue;
            }
        
        // a living other player
        
        // consider all men here
        // and any childless women (they are counted as aunts
        // for any children born before they themselves have children
        // or after all their own children die)
        if( newObject.parentID != otherPlayer->id 
            &&
            ( ! getFemale( otherPlayer ) ||
              countLivingChildren( otherPlayer->id ) == 0 ) ) {
            
                //Only direct mother-son/daughter parenting is counted

            }
        else {
            // females, look for direct ancestry

            for( int i=0; i<newObject.lineage->size(); i++ ) {
                    
                if( newObject.lineage->getElementDirect( i ) ==
                    otherPlayer->id ) {
                        
                    //Only direct mother-son/daughter parenting is counted
                    if( i != 0 ) continue;
                    
                    newObject.ancestorIDs->push_back( otherPlayer->id );

                    newObject.ancestorEmails->push_back( 
                        stringDuplicate( otherPlayer->email ) );

                    // i tells us how many greats and grands
                    SimpleVector<char> workingName;
                    
                    for( int g=1; g<=i; g++ ) {
                        if( g == i ) {
                            workingName.appendElementString( "Grand" );
                            }
                        else {
                            workingName.appendElementString( "Great_" );
                            }
                        }
                    
                    
                    if( i != 0 ) {
                        if( ! getFemale( &newObject ) ) {
                            workingName.appendElementString( "son" );
                            }
                        else {
                            workingName.appendElementString( "daughter" );
                            }
                        }
                    else {
                        // no "Grand"
                        if( ! getFemale( &newObject ) ) {
                                workingName.appendElementString( "Son" );
                            }
                        else {
                            workingName.appendElementString( "Daughter" );
                            }
                        }
                    
                    
                    newObject.ancestorRelNames->push_back(
                        workingName.getElementString() );
                    
                    newObject.ancestorLifeStartTimeSeconds->push_back(
                            otherPlayer->lifeStartTimeSeconds );
                    newObject.ancestorLifeEndTimeSeconds->push_back(
                            -1.0 );
                    
                    break;
                    }
                }
            }
        

        }
    

    

    
    // parent pointer possibly no longer valid after push_back, which
    // can resize the vector
    parent = NULL;


    if( newObject.isTutorial ) {
        AppLog::infoF( "New player %s pending tutorial load (tutorial=%d)",
                       newObject.email,
                       inTutorialNumber );

        // holding bay for loading tutorial maps incrementally
        tutorialLoadingPlayers.push_back( newObject );
        }
    else {
        players.push_back( newObject );            
        }

    if( newObject.isEve ) {
        addEveLanguage( newObject.id );
        }
    else {
        incrementLanguageCount( newObject.lineageEveID );
        }
    

    // addRecentScore( newObject.email, inFitnessScore );
    
    char *seed = "";
    if (connection->spawnCode) {
	    seed = connection->spawnCode;
    }
    if( ! newObject.isTutorial )     
    logBirth( newObject.id,
              newObject.email,
              newObject.parentID,
              parentEmail,
              ! getFemale( &newObject ),
              getObject( newObject.displayID )->race, 
              newObject.xd,
              newObject.yd,
              players.size(),
              newObject.parentChainLength, seed);
    
    HostAddress* a = connection->sock->getRemoteHostAddress(); 
    AppLog::infoF( "New player %s connected as player %d (tutorial=%d, IP=%s, Seed=%s) (%d,%d)"
                   " (maxPlacementX=%d)",
                   newObject.email, newObject.id,
                   inTutorialNumber, a->mAddressString, seed, newObject.xs, newObject.ys,
                   maxPlacementX );
    
    return newObject.id;
    }




static void processWaitingTwinConnection( FreshConnection inConnection ) {
    AppLog::infoF( "Player %s waiting for twin party of %d", 
                   inConnection.email,
                   inConnection.twinCount );
    waitingForTwinConnections.push_back( inConnection );
    
    CurseStatus anyTwinCurseLevel = inConnection.curseStatus;
    

    // count how many match twin code from inConnection
    // is this the last one to join the party?
    SimpleVector<FreshConnection*> twinConnections;
    

    for( int i=0; i<waitingForTwinConnections.size(); i++ ) {
        FreshConnection *nextConnection = 
            waitingForTwinConnections.getElement( i );
        
        if( nextConnection->error ) {
            continue;
            }
        
        if( nextConnection->twinCode != NULL
            &&
            strcmp( inConnection.twinCode, nextConnection->twinCode ) == 0 
            &&
            inConnection.twinCount == nextConnection->twinCount ) {

            if( nextConnection->curseStatus.curseLevel > 
                anyTwinCurseLevel.curseLevel ) {
                anyTwinCurseLevel = nextConnection->curseStatus;
                }
            
            twinConnections.push_back( nextConnection );
            }
        }

    
    if( twinConnections.size() >= inConnection.twinCount ) {
        // everyone connected and ready in twin party

        AppLog::infoF( "Found %d other people waiting for twin party of %s, "
                       "ready", 
                       twinConnections.size(), inConnection.email );
                       
                       
        // see if player was previously disconnected
        for( int i=0; i<players.size(); i++ ) {
            LiveObject *o = players.getElement( i );
            
            if( ! o->error && ! o->connected &&
                strcmp( o->email, inConnection.email ) == 0 ) {
                       
                // take them out of waiting list too
                for( int i=0; i<waitingForTwinConnections.size(); i++ ) {
                    if( waitingForTwinConnections.getElement( i )->sock ==
                        inConnection.sock ) {
                        // found
                        
                        waitingForTwinConnections.deleteElement( i );
                        break;
                        }
                    }

                if( inConnection.twinCode != NULL ) {
                    delete [] inConnection.twinCode;
                    inConnection.twinCode = NULL;
                    }
                nextLogInTwin = false;
                return;
                }
            }
            
        
        nextLogInTwin = true;
        
        // set up twin emails for lineage ban
        for( int i=0; i<twinConnections.size(); i++ ) {
            FreshConnection *nextConnection = 
                twinConnections.getElementDirect( i );
        
            tempTwinEmails.push_back( nextConnection->email );
            }

        char usePersonalCurses = 
            SettingsManager::getIntSetting( "usePersonalCurses", 0 );


        int newID = -1;
        LiveObject *newPlayer = NULL;
        
        int parent = 0;
        int displayID = 0;
        GridPos playerPos;
        GridPos *forcedEvePos = NULL;
        
        // save these out here, because newPlayer points into 
        // tutorialLoadingPlayers, which may expand during this loop,
        // invalidating that pointer
        char isTutorial = false;
        TutorialLoadProgress sharedTutorialLoad;



        for( int i=0; i<twinConnections.size(); i++ ) {
            FreshConnection *nextConnection = 
                twinConnections.getElementDirect( i );
                
            if( i == 0 ) {
            
                newID = processLoggedInPlayer( false, 
                                               nextConnection->sock,
                                               nextConnection->sockBuffer,
                                               nextConnection->email,
                                               nextConnection,
                                               nextConnection->tutorialNumber,
                                               anyTwinCurseLevel,
                                               nextConnection->lifeStats,
                                               nextConnection->fitnessScore );
                tempTwinEmails.deleteAll();
                                                   
                if( newID == -1 ) {
                    char *emailCopy = stringDuplicate( inConnection.email );
                    
                    AppLog::infoF( "%s reconnected to existing life, not triggering "
                                   "fellow twins to spawn now.",
                                   emailCopy );
                                   
                    delete [] emailCopy;

                    if( inConnection.twinCode != NULL ) {
                        delete [] inConnection.twinCode;
                        inConnection.twinCode = NULL;
                        }
                    nextLogInTwin = false;
                    return;
                    }
                else if( newID == -2 ) {
                    
                    for( int j=0; j<twinConnections.size(); j++ ) {
                        FreshConnection *nextConnectionToReject = 
                            twinConnections.getElementDirect( j );
                    
                        nextConnectionToReject->error = true;
                        nextConnectionToReject->errorCauseString =
                            "Target family is not found or does not have fertiles";
                            
                        if( nextConnectionToReject->twinCode != NULL ) {
                            delete [] nextConnectionToReject->twinCode;
                            nextConnectionToReject->twinCode = NULL;
                            }
                        }
                    
                    nextLogInTwin = false;
                    // Do not remove the connection from waitingForTwinConnections
                    // we need to notify them about the famTarget failure
                    return;
                    }
                    
                firstTwinID = newID;
                    
                if( nextConnection->tutorialNumber == 0 ) {
                    newPlayer = getLiveObject( newID );
                    }
                else {
                    newPlayer = tutorialLoadingPlayers.getElement(
                        tutorialLoadingPlayers.size() - 1 );
                    }
                    
                parent = newPlayer->parentID;
                displayID = newPlayer->displayID;
                playerPos = { newPlayer->xd, newPlayer->yd };
                forcedEvePos = NULL;
                
                if( parent == -1 ) {
                    // first twin placed was Eve
                    // others are identical Eves
                    forcedEvePos = &playerPos;
                    // trigger forced Eve placement
                    parent = -2;
                    }
                    
                isTutorial = newPlayer->isTutorial;
                sharedTutorialLoad = newPlayer->tutorialLoad;
                    
                                                   
                }
            else {
                
                nextConnection->hashedSpawnSeed = 0;
                
                if( nextConnection->famTarget != NULL ) {
                    delete[] nextConnection->famTarget;
                    nextConnection->famTarget = NULL;
                    }
                
                processLoggedInPlayer( false, 
                                       nextConnection->sock,
                                       nextConnection->sockBuffer,
                                       nextConnection->email,
                                       nextConnection,
                                       // ignore tutorial number of all but
                                       // first player
                                       0,
                                       anyTwinCurseLevel,
                                       nextConnection->lifeStats,
                                       nextConnection->fitnessScore,
                                       parent,
                                       displayID,
                                       forcedEvePos );
                                       
                // just added is always last object in list
                
                if( usePersonalCurses ) {
                    // curse level not known until after first twin logs in
                    // their curse level is set based on blockage caused
                    // by any of the other twins in the party
                    // pass it on.
                    LiveObject *newTwinPlayer = 
                        players.getElement( players.size() - 1 );
                    newTwinPlayer->curseStatus = newPlayer->curseStatus;
                    }



                LiveObject newTwinPlayer = 
                    players.getElementDirect( players.size() - 1 );

                if( isTutorial ) {
                    // force this one to wait for same tutorial map load
                    newTwinPlayer.tutorialLoad = sharedTutorialLoad;

                    // flag them as a tutorial player too, so they can't have
                    // babies in the tutorial, and they won't be remembered
                    // as a long-lineage position at shutdown
                    newTwinPlayer.isTutorial = true;

                    players.deleteElement( players.size() - 1 );
                    
                    tutorialLoadingPlayers.push_back( newTwinPlayer );
                    }
                                       
                }
                                       
            }

                
        firstTwinID = -1;

        char *twinCode = stringDuplicate( inConnection.twinCode );
        
        for( int i=0; i<waitingForTwinConnections.size(); i++ ) {
            FreshConnection *nextConnection = 
                waitingForTwinConnections.getElement( i );
            
            if( nextConnection->error ) {
                continue;
                }
            
            if( nextConnection->twinCode != NULL 
                &&
                nextConnection->twinCount == inConnection.twinCount
                &&
                strcmp( nextConnection->twinCode, twinCode ) == 0 ) {
                
                delete [] nextConnection->twinCode;
                waitingForTwinConnections.deleteElement( i );
                i--;
                }
            }
        
        delete [] twinCode;
        
        nextLogInTwin = false;
        }
    }


// doesn't check whether dest itself is blocked
static char directLineBlocked( GridPos inSource, GridPos inDest ) {
    // line algorithm from here
    // https://en.wikipedia.org/wiki/Bresenham's_line_algorithm
    
    double deltaX = inDest.x - inSource.x;
    
    double deltaY = inDest.y - inSource.y;
    

    int xStep = 1;
    if( deltaX < 0 ) {
        xStep = -1;
        }
    
    int yStep = 1;
    if( deltaY < 0 ) {
        yStep = -1;
        }
    

    if( deltaX == 0 ) {
        // vertical line
        
        // just walk through y
        for( int y=inSource.y; y != inDest.y; y += yStep ) {
            if( isMapSpotBlocking( inSource.x, y ) ) {
                return true;
                }
            }
        }
    else {
        double deltaErr = fabs( deltaY / (double)deltaX );
        
        double error = 0;
        
        int y = inSource.y;
        for( int x=inSource.x; x != inDest.x || y != inDest.y; x += xStep ) {
            if( isMapSpotBlocking( x, y ) ) {
                return true;
                }
            error += deltaErr;
            
            if( error >= 0.5 ) {
                y += yStep;
                error -= 1.0;
                }
            
            // we may need to take multiple steps in y
            // if line is vertically oriented
            while( error >= 0.5 ) {
                if( isMapSpotBlocking( x, y ) ) {
                    return true;
                    }

                y += yStep;
                error -= 1.0;
                }
            }
        }

    return false;
    }



char removeFromContainerToHold( LiveObject *inPlayer, 
                                int inContX, int inContY,
                                int inSlotNumber,
                                char inSwap );




// find index of spot on container held item can swap with, or -1 if none found
static int getContainerSwapIndex( LiveObject *inPlayer,
                                  int idToAdd,
                                  int inStillHeld,
                                  int inSearchLimit,
                                  int inContX, int inContY ) {
    // take what's on bottom of container, but only if it's different
    // from what's in our hand
    // AND we are old enough to take it
    double playerAge = computeAge( inPlayer );
    
    // if we find a same object on bottom, keep going up until
    // we find a non-same one to swap
    for( int botInd = 0; botInd < inSearchLimit; botInd ++ ) {
        
        char same = false;
        
        int bottomItem = 
            getContained( inContX, inContY, botInd, 0 );
        
        TransRecord *pickUpTrans = getPTrans( 0, bottomItem );
        bool hasPickUpTrans = 
            pickUpTrans != NULL && 
            pickUpTrans->newTarget == 0;
                
        if( bottomItem > 0 &&
            ( getObject( bottomItem )->minPickupAge > playerAge ||
            ( getObject( bottomItem )->permanent && !hasPickUpTrans ) ) 
            ) {
            // too young to hold!
            // or contained object is permanent
            same = true;
            }
        else if( bottomItem == idToAdd ) {
            if( bottomItem > 0 ) {
                // not sub conts
                same = true;
                }
            else {
                // they must contain same stuff to be same
                int bottomNum = getNumContained( inContX, inContY,
                                                 botInd + 1 );
                int topNum;

                if( inStillHeld ) {
                    topNum = inPlayer->numContained;
                    }
                else {
                    // already in the container
                    topNum =  getNumContained( inContX, inContY,
                                               inSearchLimit + 1 );
                    }
                
                if( bottomNum != topNum ) {
                    same = false;
                    }
                else {
                    same = true;
                    for( int b=0; b<bottomNum; b++ ) {
                        int subB = getContained( inContX, inContY,
                                                 b, botInd + 1 );
                        int subT;

                        if( inStillHeld ) {
                            subT = inPlayer->containedIDs[b];
                            }
                        else {
                            subT = getContained( inContX, inContY,
                                                 b, inSearchLimit + 1 );
                            }
                        
                                
                        if( subB != subT ) {
                            same = false;
                            break;
                            }
                        }
                    }
                }
            }
        if( !same ) {
            return botInd;
            }
        }
    
    return -1;
    }



// checks for granular +cont containment limitations
// assumes that container size limitation and 
// containable property checked elsewhere
static char isContainmentWithMatchedTags( int inContainerID, int inContainedID ) {
    ObjectRecord *containedO = getObject( inContainedID );
    
    char *contLoc = strstr( containedO->description, "+cont" );
    
    if( contLoc == NULL ) {
        // not a limited containable object
        return false;
        }

    char anyWithLimitNameFound = false;
    
    while( contLoc != NULL ) {
        
    
        char *limitNameLoc = &( contLoc[5] );
    
        if( limitNameLoc[0] != ' ' &&
            limitNameLoc[0] != '\0' ) {

            // there's something after +cont
            // scan the whole thing, including +cont
            
            anyWithLimitNameFound = true;

            char tag[100];
            
            int numRead = sscanf( contLoc, "%99s", tag );
            
            if( numRead == 1 ) {
                
                // clean up # character that might delimit end of string
                int tagLen = strlen( tag );
                
                for( int i=0; i<tagLen; i++ ) {
                    if( tag[i] == '#' ) {
                        tag[i] = '\0';
                        tagLen = i;
                        break;
                        }
                    }
                
                char *locInContainerName =
                    strstr( getObject( inContainerID )->description, tag );
                
                if( locInContainerName != NULL ) {
                    // skip to end of tag
                    // and make sure tag isn't a sub-tag of container tag
                    // don't want contained to be +contHot
                    // and contaienr to be +contHotPlates
                    
                    char end = locInContainerName[ tagLen ];
                    
                    if( end == ' ' ||
                        end == '\0'||
                        end == '#' ) {
                        return true;
                        }
                    }
                // no match with this container so far, 
                // but we can keep trying other +cont tags
                // in our contained object
                }
            }
        else {
            // +cont with nothing after it, no limit based on this tag
            }

        // keep looking beyond last limit loc
        contLoc = strstr( limitNameLoc, "+cont" );
        }

    if( anyWithLimitNameFound ) {
        // item is limited to some types of container, and this
        // container didn't match any of the limit names
        return false;
        }
    

    // we get here if we found +cont in the item, but no limit name after it
    return false;
    }


static void changeContained( int inX, int inY, int inSlotNumber, 
                             int inNewObjectID ) {
    
    int numContained = 0;
    int *contained = getContained( inX, inY, &numContained );

    timeSec_t *containedETA = 
        getContainedEtaDecay( inX, inY, &numContained );
    
    timeSec_t curTimeSec = Time::getCurrentTime();
    
    if( contained != NULL && containedETA != NULL &&
        numContained > inSlotNumber ) {
    
        int oldObjectID = contained[ inSlotNumber ];
        timeSec_t oldETA = containedETA[ inSlotNumber ];
        
        if( oldObjectID > 0 ) {
            
            TransRecord *oldDecayTrans = getTrans( -1, oldObjectID );

            TransRecord *newDecayTrans = getTrans( -1, inNewObjectID );
            

            timeSec_t newETA = 0;
            
            if( newDecayTrans != NULL ) {
                newETA = curTimeSec + newDecayTrans->autoDecaySeconds;
                }
            
            if( oldDecayTrans != NULL && newDecayTrans != NULL &&
                oldDecayTrans->autoDecaySeconds == 
                newDecayTrans->autoDecaySeconds ) {
                // preserve remaining seconds from old object
                newETA = oldETA;
                }
            
            contained[ inSlotNumber ] = inNewObjectID;
            containedETA[ inSlotNumber ] = newETA;

            setContained( inX, inY, numContained, contained );
            setContainedEtaDecay( inX, inY, numContained, containedETA );
            }
        }

    if( contained != NULL ) {
        delete [] contained;
        }
    if( containedETA != NULL ) {
        delete [] containedETA;
        }
    }


// check whether container has slots, containability, size and tags
// whether container has empty slot is checked elsewhere
char containmentPermitted( int inContainerID, int inContainedID ) {
    
    // Use the container's and object's dummy parents to judge
    // So use objects also inherit the cont tag
    ObjectRecord *containerObj = getObject( inContainerID );
    ObjectRecord *containedObj = getObject( inContainedID );
    if( containerObj->isUseDummy ) inContainerID = containerObj->useDummyParent;
    if( containedObj->isUseDummy ) inContainedID = containedObj->useDummyParent;
    
    // avoid container-ception
    if( inContainerID == inContainedID ) return false;    
    
    // container does not have slots
    if( getObject( inContainerID )->numSlots == 0 ) return false;
    
    // matching tags for container and object, skip other checks
    if( isContainmentWithMatchedTags( inContainerID, inContainedID ) ) return true;
    
    // either or both container and object have parent categories which have matching tags, skip other checks
    
    ReverseCategoryRecord *containedRecord = getReverseCategory( inContainedID );
    ReverseCategoryRecord *containerRecord = getReverseCategory( inContainerID );
    
    if( containerRecord != NULL && containedRecord != NULL ) {
        for( int i=0; i< containerRecord->categoryIDSet.size(); i++ ) {
            int containerCID = containerRecord->categoryIDSet.getElementDirect( i );
            CategoryRecord *containerCategory = getCategory( containerCID );
            if( containerCategory == NULL ) continue;
            int containerPID = containerCategory->parentID;
            
            for( int j=0; j< containedRecord->categoryIDSet.size(); j++ ) {
                int containedCID = containedRecord->categoryIDSet.getElementDirect( j );
                CategoryRecord *containedCategory = getCategory( containedCID );
                if( containedCategory == NULL ) continue;
                int containedPID = containedCategory->parentID;
                
                if( isContainmentWithMatchedTags( containerPID, containedPID ) ) return true;
            }
        }
    } else if ( containerRecord != NULL ) {
        for( int i=0; i< containerRecord->categoryIDSet.size(); i++ ) {
            int containerCID = containerRecord->categoryIDSet.getElementDirect( i );
            CategoryRecord *containerCategory = getCategory( containerCID );
            if( containerCategory == NULL ) continue;
            int containerPID = containerCategory->parentID;
            if( isContainmentWithMatchedTags( containerPID, inContainedID ) ) return true;
        }
    } else if ( containedRecord != NULL ) {
        for( int j=0; j< containedRecord->categoryIDSet.size(); j++ ) {
            int containedCID = containedRecord->categoryIDSet.getElementDirect( j );
            CategoryRecord *containedCategory = getCategory( containedCID );
            if( containedCategory == NULL ) continue;
            int containedPID = containedCategory->parentID;
            if( isContainmentWithMatchedTags( inContainerID, containedPID ) ) return true;
        }
    }
    
    // object not containable
    if( !isContainable( inContainedID ) ) return false;
    
    float slotSize = getObject( inContainerID )->slotSize;
    float objectSize = getObject( inContainedID )->containSize;
    
    // object is too big for the container
    if( objectSize > slotSize ) return false;
    
    return true;
    }


// swap indicates that we want to put the held item at the bottom
// of the container and take the top one
// returns true if added
static char addHeldToContainer( LiveObject *inPlayer,
                                int inTargetID,
                                int inContX, int inContY,
                                char inSwap = false ) {
    
    int target = inTargetID;
        
    int targetSlots = 
        getNumContainerSlots( target );
                                        
    ObjectRecord *targetObj =
        getObject( target );
    
    if( isGrave( target ) ) {
        return false;
        }
    if( targetObj->slotsLocked ) {
        return false;
        }

    int numIn = 
        getNumContained( inContX, inContY );

    
    int isRoom = false;
    

    if( numIn < targetSlots ) {
        isRoom = true;
        }
    else {
        // container full
        // but check if swap is possible

        if( inSwap ) {
            
            int idToAdd = inPlayer->holdingID;
            TransRecord *r = getPTrans( idToAdd, -1 );

            if( r != NULL && r->newActor == 0 && r->newTarget > 0 ) {
                idToAdd = r->newTarget;
                }
            
            
            if( inPlayer->numContained > 0 ) {
                // negative to indicate sub-container
                idToAdd *= -1;
                }

            int swapInd = getContainerSwapIndex ( inPlayer,
                                                  idToAdd,
                                                  true,
                                                  numIn,
                                                  inContX, inContY );
            if( swapInd != -1 ) {
                isRoom = true;
                }
            }
        }
    


    
    if( isRoom &&
        containmentPermitted( inTargetID, inPlayer->holdingID ) ) {
        
        
        // Check for containment transitions
        
        TransRecord *contTrans = NULL;
        
        if( numIn == 0 ) {
            contTrans = getPTrans( inPlayer->holdingID, target, false, false, 1 );
            if( contTrans == NULL ) contTrans = getPTrans( 0, target, false, false, 1 );
        } else if( targetSlots - 1 == numIn ) {
            contTrans = getPTrans( inPlayer->holdingID, target, false, false, 2 );
            if( contTrans == NULL ) contTrans = getPTrans( 0, target, false, false, 2 );
        }
        
        if( contTrans == NULL ) {
            contTrans = getPTrans( inPlayer->holdingID, target, false, false, 3 );
            if( contTrans == NULL ) contTrans = getPTrans( 0, target, false, false, 3 );
            
            // If object will transition after going into the container
            // We use the new object to consider potential swapping
            // Similar to how Set-down transitions are handled above before swapping is done
            int idToConsiderSwap = inPlayer->holdingID;
            if( contTrans != NULL ) {
                idToConsiderSwap = contTrans->newActor;
                }
            if( inPlayer->numContained > 0 ) {
                // negative to indicate sub-container
                idToConsiderSwap *= -1;
                }
             
            int swapInd = getContainerSwapIndex( inPlayer, 
                                                 idToConsiderSwap,
                                                 true,
                                                 numIn,
                                                 inContX, inContY ); 
             
            if( swapInd != -1 && inSwap ) contTrans = NULL;
        }
        
        if( contTrans == NULL ) {
            contTrans = getPTrans( inPlayer->holdingID, target, false, false, 4 );
            if( contTrans == NULL ) contTrans = getPTrans( 0, target, false, false, 4 );
        }
        
        if( contTrans != NULL ) {
            
            // Check that the new container can contain all the objects
            
            int newNumSlots = getNumContainerSlots( contTrans->newTarget );
            
            if( numIn > newNumSlots || (numIn == newNumSlots && !inSwap) ) {
                return false;
                } 
            else {
                int slotNumber = numIn - 1;
                
                int contID = getContained( 
                    inContX, inContY,
                    slotNumber );
                    
                if( contID < 0 ) contID *= -1;
            
                while( slotNumber >= 0 &&
                       containmentPermitted( contTrans->newTarget, contID ) )  {
            
                    slotNumber--;
                    
                    if( slotNumber < 0 ) break;
                    
                    contID = getContained( 
                        inContX, inContY,
                        slotNumber );
                
                    if( contID < 0 ) {
                        contID *= -1;
                        }
                    }
                    
                if( slotNumber >= 0 ) {
                    return false;
                    }
                }
            }
        
        
        // add to container
        
        setResponsiblePlayer( 
            inPlayer->id );
        

        // adding something to a container acts like a drop
        // but some non-permanent held objects are supposed to go through 
        // a transition when they drop (example:  held wild piglet is
        // non-permanent, so it can be containable, but it supposed to
        // switch to a moving wild piglet when dropped.... we should
        // switch to this other wild piglet when putting it into a container
        // too)

        // "set-down" type bare ground
        // trans exists for what we're 
        // holding?
        TransRecord *r = getPTrans( inPlayer->holdingID, -1 );

        if( r != NULL && r->newActor == 0 && r->newTarget > 0 ) {
                                            
            // only applies if the 
            // bare-ground
            // trans leaves nothing in
            // our hand
            
            // first, change what they
            // are holding to this 
            // newTarget
            

            handleHoldingChange( 
                inPlayer,
                r->newTarget );
            }


        int idToAdd = inPlayer->holdingID;


        float stretch = getObject( idToAdd )->slotTimeStretch;
                    
                    

        if( inPlayer->numContained > 0 ) {
            // negative to indicate sub-container
            idToAdd *= -1;
            }

        
        addContained( 
            inContX, inContY,
            idToAdd,
            inPlayer->holdingEtaDecay );
            
            
        // Execute containment transitions - addHeldToContainer - contained
        
        if( contTrans != NULL && contTrans->newActor > 0 ) {
                                
            changeContained( inContX, inContY, numIn, contTrans->newActor );
            
        }
            

        if( inPlayer->numContained > 0 ) {
            timeSec_t curTime = Time::getCurrentTime();
            
            for( int c=0; c<inPlayer->numContained; c++ ) {
                
                // undo decay stretch before adding
                // (stretch applied by adding)
                if( stretch != 1.0 &&
                    inPlayer->containedEtaDecays[c] != 0 ) {
                
                    timeSec_t offset = 
                        inPlayer->containedEtaDecays[c] - curTime;
                    
                    offset = offset * stretch;
                    
                    inPlayer->containedEtaDecays[c] = curTime + offset;
                    }


                addContained( inContX, inContY, inPlayer->containedIDs[c],
                              inPlayer->containedEtaDecays[c],
                              numIn + 1 );
                }
            
            clearPlayerHeldContained( inPlayer );
            }
        

        
        setResponsiblePlayer( -1 );
        
        inPlayer->holdingID = 0;
        inPlayer->holdingEtaDecay = 0;
        inPlayer->heldOriginValid = 0;
        inPlayer->heldOriginX = 0;
        inPlayer->heldOriginY = 0;
        inPlayer->heldTransitionSourceID = -1;

        int numInNow = getNumContained( inContX, inContY );
        
        if( inSwap &&  numInNow > 1 ) {
            
            int swapInd = getContainerSwapIndex( inPlayer, 
                                                 idToAdd,
                                                 false,
                                                 // don't consider top slot
                                                 // where we just put this
                                                 // new item
                                                 numInNow - 1,
                                                 inContX, inContY );
            if( swapInd != -1 ) {
                // found one to swap
                removeFromContainerToHold( inPlayer, inContX, inContY, 
                                           swapInd, true );
                }
            // if we didn't remove one, it means whole container is full
            // of identical items.
            // the swap action doesn't work, so we just let it
            // behave like an add action instead.
            }
            
        // Execute containment transitions - addHeldToContainer - container
        
        if( contTrans != NULL ) {
            setResponsiblePlayer( -inPlayer->id );
            if( contTrans->newTarget != target ) setMapObject( inContX, inContY, contTrans->newTarget );
        }

        return true;
        }

    return false;
    }



// returns true if succeeded
char removeFromContainerToHold( LiveObject *inPlayer, 
                                int inContX, int inContY,
                                int inSlotNumber,
                                char inSwap = false ) {
    inPlayer->heldOriginValid = 0;
    inPlayer->heldOriginX = 0;
    inPlayer->heldOriginY = 0;                        
    inPlayer->heldTransitionSourceID = -1;
    

    if( isGridAdjacent( inContX, inContY,
                        inPlayer->xd, 
                        inPlayer->yd ) 
        ||
        ( inContX == inPlayer->xd &&
          inContY == inPlayer->yd ) ) {
                            
        inPlayer->actionAttempt = 1;
        inPlayer->actionTarget.x = inContX;
        inPlayer->actionTarget.y = inContY;
                            
        if( inContX > inPlayer->xd ) {
            inPlayer->facingOverride = 1;
            }
        else if( inContX < inPlayer->xd ) {
            inPlayer->facingOverride = -1;
            }

        // can only use on targets next to us for now,
        // no diags
                            
        int target = getMapObject( inContX, inContY );
                            
        if( target != 0 ) {
                            
            if( target > 0 && getObject( target )->slotsLocked ) {
                return false;
                }

            int numIn = 
                getNumContained( inContX, inContY );
                                
            int toRemoveID = 0;
            
            double playerAge = computeAge( inPlayer );

            
            if( inSlotNumber < 0 ) {
                inSlotNumber = numIn - 1;
                
                // no slot specified
                // find top-most object that they can actually pick up

                int toRemoveID = getContained( 
                    inContX, inContY,
                    inSlotNumber );
                
                if( toRemoveID < 0 ) {
                    toRemoveID *= -1;
                    }
                
                while( inSlotNumber > 0 &&
                       (getObject( toRemoveID )->minPickupAge > playerAge ||
                       ( getObject( toRemoveID )->permanent &&
                       ( getPTrans( 0, toRemoveID ) == NULL ||
                       getPTrans( 0, toRemoveID )->newTarget != 0 ) )
                       )
                       )  {
            
                    inSlotNumber--;
                    
                    toRemoveID = getContained( 
                        inContX, inContY,
                        inSlotNumber );
                
                    if( toRemoveID < 0 ) {
                        toRemoveID *= -1;
                        }
                    }
                }
            


                                
            if( numIn > 0 ) {
                toRemoveID = getContained( inContX, inContY, inSlotNumber );
                }
            
            char subContain = false;
            
            if( toRemoveID < 0 ) {
                toRemoveID *= -1;
                subContain = true;
                }

            
            if( toRemoveID == 0 ) {
                // this should never happen, except due to map corruption
                
                // clear container, to be safe
                clearAllContained( inContX, inContY );
                return false;
                }


            TransRecord *pickUpTrans = getPTrans( 0, toRemoveID );
            bool hasPickUpTrans = 
                pickUpTrans != NULL && 
                pickUpTrans->newTarget == 0;
        
            if( inPlayer->holdingID == 0 && 
                numIn > 0 &&
                // old enough to handle it
                getObject( toRemoveID )->minPickupAge <= 
                computeAge( inPlayer ) &&
                // permanent object cannot be removed from container
                ( !getObject( toRemoveID )->permanent ||
                hasPickUpTrans )
                ) {
                // get from container


                // Check for containment transitions
                
                int targetSlots = 
                    getNumContainerSlots( target );
                
                TransRecord *contTrans = NULL;
                
                if( numIn == 1 ) {
                    contTrans = getPTrans( target, toRemoveID, false, false, 2 );
                    if( contTrans == NULL ) contTrans = getPTrans( target, -1, false, false, 2 );
                } else if( targetSlots == numIn ) {
                    contTrans = getPTrans( target, toRemoveID, false, false, 1 );
                    if( contTrans == NULL ) contTrans = getPTrans( target, -1, false, false, 1 );
                }
                
                if( contTrans == NULL && !inSwap ) {
                    contTrans = getPTrans( target, toRemoveID, false, false, 3 );
                    if( contTrans == NULL ) contTrans = getPTrans( target, -1, false, false, 3 );
                }
                
                if( contTrans == NULL ) {
                    contTrans = getPTrans( target, toRemoveID, false, false, 4 );
                    if( contTrans == NULL ) contTrans = getPTrans( target, -1, false, false, 4 );
                }
                
                if( contTrans != NULL ) {
                    
                    // Check that the new container can contain all the objects
                    
                    int slotNumber = numIn - 1;
                    
                    if( inSlotNumber == slotNumber ) slotNumber--;
                    
                    int contID = getContained( 
                        inContX, inContY,
                        slotNumber );
                        
                    if( contID < 0 ) contID *= -1;

                
                    while( slotNumber >= 0 &&
                           containmentPermitted( contTrans->newActor, contID ) )  {
                
                        slotNumber--;
                        
                        if( inSlotNumber == slotNumber ) slotNumber--;
                        
                        if( slotNumber < 0 ) break;
                        
                        contID = getContained( 
                            inContX, inContY,
                            slotNumber );
                    
                        if( contID < 0 ) {
                            contID *= -1;
                            }
                        }
                        
                    if( slotNumber >= 0 ) {
                        contTrans = NULL;
                        }
                        
                    }

                
                // Execute containment transitions - removeFromContainerToHold - container
                
                if( contTrans != NULL && target != contTrans->newActor ) {
                    setMapObject( inContX, inContY, contTrans->newActor );
                }


                if( subContain ) {
                    int subSlotNumber = inSlotNumber;
                    
                    if( subSlotNumber == -1 ) {
                        subSlotNumber = numIn - 1;
                        }

                    inPlayer->containedIDs =
                        getContained( inContX, inContY, 
                                      &( inPlayer->numContained ), 
                                      subSlotNumber + 1 );
                    inPlayer->containedEtaDecays =
                        getContainedEtaDecay( inContX, inContY, 
                                              &( inPlayer->numContained ), 
                                              subSlotNumber + 1 );

                    // these will be cleared when removeContained is called
                    // for this slot below, so just get them now without clearing

                    // empty vectors... there are no sub-sub containers
                    inPlayer->subContainedIDs = 
                        new SimpleVector<int>[ inPlayer->numContained ];
                    inPlayer->subContainedEtaDecays = 
                        new SimpleVector<timeSec_t>[ inPlayer->numContained ];
                
                    }
                
                
                setResponsiblePlayer( - inPlayer->id );
                
                inPlayer->holdingID =
                    removeContained( 
                        inContX, inContY, inSlotNumber,
                        &( inPlayer->holdingEtaDecay ) );
                        
                if( inPlayer->holdingID < 0 ) {
                    // sub-contained
                    
                    inPlayer->holdingID *= -1;    
                    }
                    
                // Execute containment transitions - removeFromContainerToHold - contained
                
                if( contTrans != NULL ) {
                    if( contTrans->newTarget > 0 ) handleHoldingChange( inPlayer, contTrans->newTarget );
                    }

                // does bare-hand action apply to this newly-held object
                // one that results in something new in the hand and
                // nothing on the ground?

                // if so, it is a pick-up action, and it should apply here

                TransRecord *pickupTrans = getPTrans( 0, inPlayer->holdingID );
                
                if( pickupTrans != NULL && pickupTrans->newActor > 0 &&
                    pickupTrans->newTarget == 0 ) {
                    
                    handleHoldingChange( inPlayer, pickupTrans->newActor );
                    }
                else {
                    holdingSomethingNew( inPlayer );
                    }

                setResponsiblePlayer( -1 );
                
                // contained objects aren't animating
                // in a way that needs to be smooth
                // transitioned on client
                inPlayer->heldOriginValid = 0;
                inPlayer->heldOriginX = 0;
                inPlayer->heldOriginY = 0;

                return true;
                }
            }
        }        
    
    return false;
    }



// outCouldHaveGoneIn, if non-NULL, is set to TRUE if clothing
// could potentialy contain what we're holding (even if clothing too full
// to contain it)
static char addHeldToClothingContainer( LiveObject *inPlayer, 
                                        int inC,
                                        // true if we should over-pack
                                        // container in anticipation of a swap
                                        char inWillSwap = false,
                                        char *outCouldHaveGoneIn = NULL,
                                        char skipContainmentCheck = false ) {    
    // drop into own clothing
    ObjectRecord *cObj = 
        clothingByIndex( 
            inPlayer->clothing,
            inC );
                                    
    if( skipContainmentCheck || (cObj != NULL &&
        containmentPermitted( cObj->id, inPlayer->holdingID ) ) ) {
                                        
        int oldNum =
            inPlayer->
            clothingContained[inC].size();
        
        if( cObj->numSlots > 0 &&
            outCouldHaveGoneIn != NULL ) {
            *outCouldHaveGoneIn = true;
            }

        if( oldNum < cObj->numSlots
            || ( oldNum == cObj->numSlots && inWillSwap ) ) {
                
            // Check for containment transitions
            
            int containedID = inPlayer->holdingID;
            int containerID = cObj->id;
            int numSlots = cObj->numSlots;
            
            TransRecord *contTrans = NULL;
            
            if( oldNum == 0 ) {
                contTrans = getPTrans( containedID, containerID, false, false, 1 );
                if( contTrans == NULL ) contTrans = getPTrans( 0, containerID, false, false, 1 );
            } else if( numSlots == oldNum ) {
                contTrans = getPTrans( containedID, containerID, false, false, 2 );
                if( contTrans == NULL ) contTrans = getPTrans( 0, containerID, false, false, 2 );
            }
            
            if( contTrans == NULL && !inWillSwap ) {
                contTrans = getPTrans( containedID, containerID, false, false, 3 );
                if( contTrans == NULL ) contTrans = getPTrans( 0, containerID, false, false, 3 );
            }
            
            if( contTrans == NULL ) {
                contTrans = getPTrans( containedID, containerID, false, false, 4 );
                if( contTrans == NULL ) contTrans = getPTrans( 0, containerID, false, false, 4 );
            }
                
            // int idToAdd = inPlayer->holdingID;
            
            if( contTrans != NULL ) {
                
                // Execute containment transitions - addHeldToClothingContainer - contained and container
                
                if( contTrans->newActor > 0 ) handleHoldingChange( inPlayer, contTrans->newActor );
                
                // idToAdd = contTrans->newActor;
                
                setClothingByIndex( 
                    &( inPlayer->clothing ), 
                    inC,
                    getObject( contTrans->newTarget ) );
                    
            }
                
                
            // room (or will swap, so we can over-pack it)
            inPlayer->clothingContained[inC].
                push_back( 
                    inPlayer->holdingID );

            if( inPlayer->
                holdingEtaDecay != 0 ) {
                                                
                timeSec_t curTime = 
                    Time::getCurrentTime();
                                            
                timeSec_t offset = 
                    inPlayer->
                    holdingEtaDecay - 
                    curTime;
                                            
                offset = 
                    offset / 
                    cObj->
                    slotTimeStretch;
                                                
                inPlayer->holdingEtaDecay =
                    curTime + offset;
                }
                                            
            inPlayer->
                clothingContainedEtaDecays[inC].
                push_back( inPlayer->
                           holdingEtaDecay );
                                        
            inPlayer->holdingID = 0;
            inPlayer->holdingEtaDecay = 0;
            inPlayer->heldOriginValid = 0;
            inPlayer->heldOriginX = 0;
            inPlayer->heldOriginY = 0;
            inPlayer->heldTransitionSourceID =
                -1;

            return true;
            }
        }

    return false;
    }



static void setHeldGraveOrigin( LiveObject *inPlayer, int inX, int inY,
                                int inNewTarget ) {
    // make sure that there is nothing left there
    // for now, transitions that remove graves leave nothing behind
    if( inNewTarget == 0 ) {
        
        // make sure that that there was a grave there before
        int gravePlayerID = getGravePlayerID( inX, inY );
        
        // clear it
        inPlayer->heldGravePlayerID = 0;
            

        if( gravePlayerID > 0 ) {
            
            // player action actually picked up this grave
            
            if( inPlayer->holdingID > 0 &&
                strstr( getObject( inPlayer->holdingID )->description, 
                        "origGrave" ) != NULL ) {
                
                inPlayer->heldGraveOriginX = inX;
                inPlayer->heldGraveOriginY = inY;
                
                inPlayer->heldGravePlayerID = getGravePlayerID( inX, inY );
                }
            
            // clear it from ground
            setGravePlayerID( inX, inY, 0 );
            }
        }
    
    }



static void pickupToHold( LiveObject *inPlayer, int inX, int inY, 
                          int inTargetID ) {
    inPlayer->holdingEtaDecay = 
        getEtaDecay( inX, inY );
    
    FullMapContained f =
        getFullMapContained( inX, inY );
    
    setContained( inPlayer, f );
    
    clearAllContained( inX, inY );
    
    setResponsiblePlayer( - inPlayer->id );
    setMapObject( inX, inY, 0 );
    setResponsiblePlayer( -1 );
    
    inPlayer->holdingID = inTargetID;
    holdingSomethingNew( inPlayer );

    setHeldGraveOrigin( inPlayer, inX, inY, 0 );
    
    inPlayer->heldOriginValid = 1;
    inPlayer->heldOriginX = inX;
    inPlayer->heldOriginY = inY;
    inPlayer->heldTransitionSourceID = -1;
    }


// returns true if it worked
static char removeFromClothingContainerToHold( LiveObject *inPlayer,
                                               int inC,
                                               int inI = -1,
                                               bool inSwap = false ) {    
    
    ObjectRecord *cObj = 
        clothingByIndex( inPlayer->clothing, 
                         inC );
                                
    float stretch = 1.0f;
    
    if( cObj != NULL ) {
        stretch = cObj->slotTimeStretch;
        }
    
    int oldNumContained = 
        inPlayer->clothingContained[inC].size();

    int slotToRemove = inI;

    double playerAge = computeAge( inPlayer );

                                
    if( slotToRemove < 0 ) {
        slotToRemove = oldNumContained - 1;

        // no slot specified
        // find top-most object that they can actually pick up

        while( slotToRemove > 0 &&
               ( getObject( inPlayer->clothingContained[inC].
                          getElementDirect( slotToRemove ) )->minPickupAge >
               playerAge ||
               ( getObject( inPlayer->clothingContained[inC].
                          getElementDirect( slotToRemove ) )->permanent &&
               ( getPTrans( 0, inPlayer->clothingContained[inC].getElementDirect( slotToRemove ) ) == NULL ||
               getPTrans( 0, inPlayer->clothingContained[inC].getElementDirect( slotToRemove ) )->newTarget != 0 ) )
               )
               ) {
            
            slotToRemove --;
            }
        }
                                
    int toRemoveID = -1;
                                
    if( oldNumContained > 0 &&
        oldNumContained > slotToRemove &&
        slotToRemove >= 0 ) {
                                    
        toRemoveID = 
            inPlayer->clothingContained[inC].
            getElementDirect( slotToRemove );
        }

    TransRecord *pickUpTrans = getPTrans( 0, toRemoveID );
    bool hasPickUpTrans = 
        pickUpTrans != NULL && 
        pickUpTrans->newTarget == 0;
                                            
    if( oldNumContained > 0 &&
        oldNumContained > slotToRemove &&
        slotToRemove >= 0 &&
        // old enough to handle it
        getObject( toRemoveID )->minPickupAge <= playerAge &&
        // permanent object that dont have a pick up transition
        // cannot be removed from container
        ( !getObject( toRemoveID )->permanent ||
        hasPickUpTrans )
        ) {
                                    

        inPlayer->holdingID = 
            inPlayer->clothingContained[inC].
            getElementDirect( slotToRemove );
        holdingSomethingNew( inPlayer );

        inPlayer->holdingEtaDecay = 
            inPlayer->
            clothingContainedEtaDecays[inC].
            getElementDirect( slotToRemove );
                                    
        timeSec_t curTime = Time::getCurrentTime();

        if( inPlayer->holdingEtaDecay != 0 ) {
                                        
            timeSec_t offset = 
                inPlayer->holdingEtaDecay
                - curTime;
            offset = offset * stretch;
            inPlayer->holdingEtaDecay =
                curTime + offset;
            }
            
        
        // Check for containment transitions
        
        int containedID = inPlayer->holdingID;
        int containerID = cObj->id;
        int numSlots = cObj->numSlots;
        
        TransRecord *contTrans = NULL;
        
        if( oldNumContained == 1 ) {
            contTrans = getPTrans( containerID, containedID, false, false, 2 );
            if( contTrans == NULL ) contTrans = getPTrans( containerID, -1, false, false, 2 );
        } else if( numSlots == oldNumContained ) {
            contTrans = getPTrans( containerID, containedID, false, false, 1 );
            if( contTrans == NULL ) contTrans = getPTrans( containerID, -1, false, false, 1 );
        }
        
        if( contTrans == NULL && !inSwap ) {
            contTrans = getPTrans( containerID, containedID, false, false, 3 );
            if( contTrans == NULL ) contTrans = getPTrans( containerID, -1, false, false, 3 );
        }
        
        if( contTrans == NULL ) {
            contTrans = getPTrans( containerID, containedID, false, false, 4 );
            if( contTrans == NULL ) contTrans = getPTrans( containerID, -1, false, false, 4 );
        }
        
        if( contTrans != NULL ) {
            
            // Execute containment transitions - removeFromClothingContainerToHold - contained and container
            
            if( contTrans->newTarget > 0 ) handleHoldingChange( inPlayer, contTrans->newTarget );
            
            // idToAdd = contTrans->newActor;
            
            setClothingByIndex( 
                &( inPlayer->clothing ), 
                inC,
                getObject( contTrans->newActor ) );
                
        }
            

        inPlayer->clothingContained[inC].
            deleteElement( slotToRemove );
        inPlayer->clothingContainedEtaDecays[inC].
            deleteElement( slotToRemove );

        inPlayer->heldOriginValid = 0;
        inPlayer->heldOriginX = 0;
        inPlayer->heldOriginY = 0;
        inPlayer->heldTransitionSourceID = -1;
        return true;
        }
    
    return false;
    }



static ObjectRecord **getClothingSlot( LiveObject *targetPlayer, int inIndex ) {
    
    ObjectRecord **clothingSlot = NULL;    


    if( inIndex == 2 &&
        targetPlayer->clothing.frontShoe != NULL ) {
        clothingSlot = 
            &( targetPlayer->clothing.frontShoe );
        }
    else if( inIndex == 3 &&
             targetPlayer->clothing.backShoe 
             != NULL ) {
        clothingSlot = 
            &( targetPlayer->clothing.backShoe );
        }
    else if( inIndex == 0 && 
             targetPlayer->clothing.hat != NULL ) {
        clothingSlot = 
            &( targetPlayer->clothing.hat );
        }
    else if( inIndex == 1 &&
             targetPlayer->clothing.tunic 
             != NULL ) {
        clothingSlot = 
            &( targetPlayer->clothing.tunic );
        }
    else if( inIndex == 4 &&
             targetPlayer->clothing.bottom 
             != NULL ) {
        clothingSlot = 
            &( targetPlayer->clothing.bottom );
        }
    else if( inIndex == 5 &&
             targetPlayer->
             clothing.backpack != NULL ) {
        clothingSlot = 
            &( targetPlayer->clothing.backpack );
        }
    
    return clothingSlot;
    }

    

static void removeClothingToHold( LiveObject *nextPlayer, 
                                  LiveObject *targetPlayer,
                                  ObjectRecord **clothingSlot,
                                  int clothingSlotIndex ) {
    int ind = clothingSlotIndex;
    
    nextPlayer->holdingID =
        ( *clothingSlot )->id;
    holdingSomethingNew( nextPlayer );
    
    *clothingSlot = NULL;
    nextPlayer->holdingEtaDecay =
        targetPlayer->clothingEtaDecay[ind];
    targetPlayer->clothingEtaDecay[ind] = 0;
    
    nextPlayer->numContained =
        targetPlayer->
        clothingContained[ind].size();
    
    freePlayerContainedArrays( nextPlayer );
    
    nextPlayer->containedIDs =
        targetPlayer->
        clothingContained[ind].
        getElementArray();
    
    targetPlayer->clothingContained[ind].
        deleteAll();
    
    nextPlayer->containedEtaDecays =
        targetPlayer->
        clothingContainedEtaDecays[ind].
        getElementArray();
    
    targetPlayer->
        clothingContainedEtaDecays[ind].
        deleteAll();
    
    // empty sub contained in clothing
    nextPlayer->subContainedIDs =
        new SimpleVector<int>[
            nextPlayer->numContained ];
    
    nextPlayer->subContainedEtaDecays =
        new SimpleVector<timeSec_t>[
            nextPlayer->numContained ];
    
    
    nextPlayer->heldOriginValid = 0;
    nextPlayer->heldOriginX = 0;
    nextPlayer->heldOriginY = 0;
    }



static TransRecord *getBareHandClothingTrans( LiveObject *nextPlayer,
                                              ObjectRecord **clothingSlot ) {
    TransRecord *bareHandClothingTrans = NULL;
    
    if( clothingSlot != NULL ) {
        bareHandClothingTrans =
            getPTrans( 0, ( *clothingSlot )->id );
                                    
        if( bareHandClothingTrans != NULL ) {
            int na =
                bareHandClothingTrans->newActor;
            
            if( na > 0 &&
                getObject( na )->minPickupAge >
                computeAge( nextPlayer ) ) {
                // too young for trans
                bareHandClothingTrans = NULL;
                }
            
            if( bareHandClothingTrans != NULL ) {
                int nt = 
                    bareHandClothingTrans->
                    newTarget;
                
                if( nt > 0 &&
                    getObject( nt )->clothing 
                    == 'n' ) {
                    // don't allow transitions
                    // that leave a non-wearable
                    // item on your body
                    bareHandClothingTrans = NULL;
                    }
                }
            }
        }
    
    return bareHandClothingTrans;
    }




// change held as the result of a transition
static void handleHoldingChange( LiveObject *inPlayer, int inNewHeldID ) {
    
    LiveObject *nextPlayer = inPlayer;

    int oldHolding = nextPlayer->holdingID;
    
    int oldContained = 
        nextPlayer->numContained;
    
    
    nextPlayer->heldOriginValid = 0;
    nextPlayer->heldOriginX = 0;
    nextPlayer->heldOriginY = 0;
    
    // can newly changed container hold
    // less than what it could contain
    // before?
    
    int newHeldSlots = getNumContainerSlots( inNewHeldID );
    
    if( newHeldSlots < oldContained ) {
        // new container can hold less
        // truncate
                            
        GridPos dropPos = 
            getPlayerPos( inPlayer );
                            
        // offset to counter-act offsets built into
        // drop code
        dropPos.x += 1;
        dropPos.y += 1;
        
        char found = false;
        GridPos spot;
        
        if( getMapObject( dropPos.x, dropPos.y ) == 0 ) {
            spot = dropPos;
            found = true;
            }
        else {
            found = findDropSpot( 
                dropPos.x, dropPos.y,
                dropPos.x, dropPos.y,
                &spot );
            }
        
        
        if( found ) {
            
            // throw it on map temporarily
            handleDrop( 
                spot.x, spot.y, 
                inPlayer,
                // only temporary, don't worry about blocking players
                // with this drop
                NULL );
                                

            // responsible player for stuff thrown on map by shrink
            setResponsiblePlayer( inPlayer->id );

            // shrink contianer on map
            shrinkContainer( spot.x, spot.y, 
                             newHeldSlots );
            
            setResponsiblePlayer( -1 );
            

            // pick it back up
            nextPlayer->holdingEtaDecay = 
                getEtaDecay( spot.x, spot.y );
                                    
            FullMapContained f =
                getFullMapContained( spot.x, spot.y );

            setContained( inPlayer, f );
            
            clearAllContained( spot.x, spot.y );
            setMapObject( spot.x, spot.y, 0 );
            }
        else {
            // no spot to throw it
            // cannot leverage map's container-shrinking
            // just truncate held container directly
            
            // truncated contained items will be lost
            inPlayer->numContained = newHeldSlots;
            }
        }

    nextPlayer->holdingID = inNewHeldID;
    holdingSomethingNew( inPlayer, oldHolding );

    if( newHeldSlots > 0 && 
        oldHolding != 0 ) {
                                        
        restretchDecays( 
            newHeldSlots,
            nextPlayer->containedEtaDecays,
            oldHolding,
            nextPlayer->holdingID );
        }
    
    
    if( oldHolding != inNewHeldID ) {
            
        char kept = false;

        // keep old decay timeer going...
        // if they both decay to the same thing in the same time
        if( oldHolding > 0 && inNewHeldID > 0 ) {
            
            TransRecord *oldDecayT = getMetaTrans( -1, oldHolding );
            TransRecord *newDecayT = getMetaTrans( -1, inNewHeldID );
            
            if( oldDecayT != NULL && newDecayT != NULL ) {
                if( oldDecayT->autoDecaySeconds == newDecayT->autoDecaySeconds
                    && 
                    oldDecayT->newTarget == newDecayT->newTarget ) {
                    
                    kept = true;
                    }
                }
            }
        if( !kept ) {
            setFreshEtaDecayForHeld( nextPlayer );
            }
        }

    }



static unsigned char *makeCompressedMessage( char *inMessage, int inLength,
                                             int *outLength ) {
    
    int compressedSize;
    unsigned char *compressedData =
        zipCompress( (unsigned char*)inMessage, inLength, &compressedSize );



    char *header = autoSprintf( "CM\n%d %d\n#", 
                                inLength,
                                compressedSize );
    int headerLength = strlen( header );
    int fullLength = headerLength + compressedSize;
    
    unsigned char *fullMessage = new unsigned char[ fullLength ];
    
    memcpy( fullMessage, (unsigned char*)header, headerLength );
    
    memcpy( &( fullMessage[ headerLength ] ), compressedData, compressedSize );

    delete [] compressedData;
    
    *outLength = fullLength;
    
    delete [] header;
    
    return fullMessage;
    }



static int maxUncompressedSize = 256;


void sendMessageToPlayer( LiveObject *inPlayer, 
                                 char *inMessage, int inLength ) {
    if( ! inPlayer->connected ) {
        // stop sending messages to disconnected players
        return;
        }
    
    
    unsigned char *message = (unsigned char*)inMessage;
    int len = inLength;
    
    char deleteMessage = false;

    if( inLength > maxUncompressedSize ) {
        message = makeCompressedMessage( inMessage, inLength, &len );
        deleteMessage = true;
        }

    int numSent = 
        inPlayer->sock->send( message, 
                              len, 
                              false, false );
        
    if( numSent != len ) {
        setPlayerDisconnected( inPlayer, "Socket write failed",  __func__ , __LINE__);
        }

    inPlayer->gotPartOfThisFrame = true;
    
    if( deleteMessage ) {
        delete [] message;
        }
    }
    


void readPhrases( const char *inSettingsName, 
                  SimpleVector<char*> *inList ) {
    char *cont = SettingsManager::getSettingContents( inSettingsName, "" );
    
    if( strcmp( cont, "" ) == 0 ) {
        delete [] cont;
        return;    
        }
    
    int numParts;
    char **parts = split( cont, "\n", &numParts );
    delete [] cont;
    
    for( int i=0; i<numParts; i++ ) {
        if( strcmp( parts[i], "" ) != 0 ) {
            inList->push_back( stringToUpperCase( parts[i] ) );
            }
        delete [] parts[i];
        }
    delete [] parts;
    }



// returns pointer to name in string
char *isNamingSay( char *inSaidString, SimpleVector<char*> *inPhraseList ) {
    char *saidString = inSaidString;
    
    if( saidString[0] == ':' ) {
        // first : indicates reading a written phrase.
        // reading written phrase aloud does not have usual effects
        // (block curse exploit)
        return NULL;
        }
    
    for( int i=0; i<inPhraseList->size(); i++ ) {
        char *testString = inPhraseList->getElementDirect( i );
        
        if( strstr( inSaidString, testString ) == saidString ) {
            // hit
            int phraseLen = strlen( testString );
            // skip spaces after
            while( saidString[ phraseLen ] == ' ' ) {
                phraseLen++;
                }
            return &( saidString[ phraseLen ] );
            }
        }
    return NULL;
    }


// returns newly allocated name, or NULL
// looks for phrases that start with a name
char *isReverseNamingSay( char *inSaidString, 
                          SimpleVector<char*> *inPhraseList ) {
    
    if( inSaidString[0] == ':' ) {
        // first : indicates reading a written phrase.
        // reading written phrase aloud does not have usual effects
        // (block curse exploit)
        return NULL;
        }

    for( int i=0; i<inPhraseList->size(); i++ ) {
        char *testString = inPhraseList->getElementDirect( i );
        
        char *hitLoc = strstr( inSaidString, testString );

        if( hitLoc != NULL ) {

            char *saidDupe = stringDuplicate( inSaidString );

            hitLoc = strstr( saidDupe, testString );

            // back one, to exclude space from name
            if( hitLoc != saidDupe ) {
                hitLoc[-1] = '\0';
                return saidDupe;
                }
            else {
                delete [] saidDupe;
                return NULL;
                }
            }
        }
    return NULL;
    }



char *isBabyNamingSay( char *inSaidString ) {
    return isNamingSay( inSaidString, &nameGivingPhrases );
    }

char *isFamilyNamingSay( char *inSaidString ) {
    return isNamingSay( inSaidString, &familyNameGivingPhrases );
    }

char *isCurseNamingSay( char *inSaidString ) {
    return isNamingSay( inSaidString, &cursingPhrases );
    }

char *isNamedGivingSay( char *inSaidString ) {
    return isReverseNamingSay( inSaidString, &namedGivingPhrases );
    }

char *isInfertilityDeclaringSay( char *inSaidString ) {
    return isNamingSay( inSaidString, &infertilityDeclaringPhrases );
    }

char *isFertilityDeclaringSay( char *inSaidString ) {
    return isNamingSay( inSaidString, &fertilityDeclaringPhrases );
    }



static char isWildcardGivingSay( char *inSaidString,
                                 SimpleVector<char*> *inPhrases ) {
    if( inSaidString[0] == ':' ) {
        // first : indicates reading a written phrase.
        // reading written phrase aloud does not have usual effects
        // (block curse exploit)
        return false;
        }

    for( int i=0; i<inPhrases->size(); i++ ) {
        char *testString = inPhrases->getElementDirect( i );
        
        if( strcmp( inSaidString, testString ) == 0 ) {
            return true;
            }
        }
    return false;
    }


char isYouGivingSay( char *inSaidString ) {
    return isWildcardGivingSay( inSaidString, &youGivingPhrases );
    }


char isYouForgivingSay( char *inSaidString ) {
    return isWildcardGivingSay( inSaidString, &youForgivingPhrases );
    }

// returns pointer into inSaidString
char *isNamedForgivingSay( char *inSaidString ) {
    return isNamingSay( inSaidString, &forgivingPhrases );
    }

// password-protected objects
char *isPasswordProtectingSay( char *inSaidString ) {
    return isNamingSay( inSaidString, &passwordProtectingPhrases );
    }
    

LiveObject *getClosestOtherPlayer( LiveObject *inThisPlayer,
                                   double inMinAge = 0,
                                   char inNameMustBeNULL = false ) {
    GridPos thisPos = getPlayerPos( inThisPlayer );

    // don't consider anyone who is too far away
    double closestDist = 20;
    LiveObject *closestOther = NULL;
    
    for( int j=0; j<players.size(); j++ ) {
        LiveObject *otherPlayer = 
            players.getElement(j);
        
        if( otherPlayer != inThisPlayer &&
            ! otherPlayer->error &&
            computeAge( otherPlayer ) >= inMinAge &&
            ( ! inNameMustBeNULL || otherPlayer->name == NULL ) ) {
                                        
            GridPos otherPos = 
                getPlayerPos( otherPlayer );
            
            double dist =
                distance( thisPos, otherPos );
            
            if( dist < closestDist ) {
                closestDist = dist;
                closestOther = otherPlayer;
                }
            }
        }
    return closestOther;
    }



int readIntFromFile( const char *inFileName, int inDefaultValue ) {
    FILE *f = fopen( inFileName, "r" );
    
    if( f == NULL ) {
        return inDefaultValue;
        }
    
    int val = inDefaultValue;
    
    fscanf( f, "%d", &val );

    fclose( f );

    return val;
    }



typedef struct KillState {
        int killerID;
        int killerWeaponID;
        int targetID;
        double killStartTime;
        double emotStartTime;
        int emotRefreshSeconds;
    } KillState;


SimpleVector<KillState> activeKillStates;




void apocalypseStep() {
    
    double curTime = Time::getCurrentTime();

    if( !apocalypseTriggered ) {
        
        if( apocalypseRequest == NULL &&
            curTime - lastRemoteApocalypseCheckTime > 
            remoteApocalypseCheckInterval ) {

            lastRemoteApocalypseCheckTime = curTime;

            // don't actually send request to reflector if apocalypse
            // not possible locally
            // or if broadcast mode disabled
            if( SettingsManager::getIntSetting( "remoteReport", 0 ) &&
                SettingsManager::getIntSetting( "apocalypsePossible", 0 ) &&
                SettingsManager::getIntSetting( "apocalypseBroadcast", 0 ) ) {

                printf( "Checking for remote apocalypse\n" );
            
                char *url = autoSprintf( "%s?action=check_apocalypse", 
                                         reflectorURL );
        
                apocalypseRequest =
                    new WebRequest( "GET", url, NULL );
            
                delete [] url;
                }
            }
        else if( apocalypseRequest != NULL ) {
            int result = apocalypseRequest->step();

            if( result == -1 ) {
                AppLog::info( 
                    "Apocalypse check:  Request to reflector failed." );
                }
            else if( result == 1 ) {
                // done, have result

                char *webResult = 
                    apocalypseRequest->getResult();
                
                if( strstr( webResult, "OK" ) == NULL ) {
                    AppLog::infoF( 
                        "Apocalypse check:  Bad response from reflector:  %s.",
                        webResult );
                    }
                else {
                    int newApocalypseNumber = lastApocalypseNumber;
                    
                    sscanf( webResult, "%d\n", &newApocalypseNumber );
                
                    if( newApocalypseNumber > lastApocalypseNumber ) {
                        lastApocalypseNumber = newApocalypseNumber;
                        apocalypseTriggered = true;
                        apocalypseRemote = true;
                        AppLog::infoF( 
                            "Apocalypse check:  New remote apocalypse:  %d.",
                            lastApocalypseNumber );
                        SettingsManager::setSetting( "lastApocalypseNumber",
                                                     lastApocalypseNumber );
                        }
                    }
                    
                delete [] webResult;
                }
            
            if( result != 0 ) {
                delete apocalypseRequest;
                apocalypseRequest = NULL;
                }
            }
        }
        


    if( apocalypseTriggered ) {

        if( !apocalypseStarted ) {
            apocalypsePossible = 
                SettingsManager::getIntSetting( "apocalypsePossible", 0 );

            if( !apocalypsePossible ) {
                // settings change since we last looked at it
                apocalypseTriggered = false;
                return;
                }
            
            AppLog::info( "Apocalypse triggerered, starting it" );


            reportArcEnd();
            

            // only broadcast to reflector if apocalypseBroadcast set
            if( !apocalypseRemote &&
                SettingsManager::getIntSetting( "remoteReport", 0 ) &&
                SettingsManager::getIntSetting( "apocalypseBroadcast", 0 ) &&
                apocalypseRequest == NULL && reflectorURL != NULL ) {
                
                AppLog::info( "Apocalypse broadcast set, telling reflector" );

                
                char *reflectorSharedSecret = 
                    SettingsManager::
                    getStringSetting( "reflectorSharedSecret" );
                
                if( reflectorSharedSecret != NULL ) {
                    lastApocalypseNumber++;

                    AppLog::infoF( 
                        "Apocalypse trigger:  New local apocalypse:  %d.",
                        lastApocalypseNumber );

                    SettingsManager::setSetting( "lastApocalypseNumber",
                                                 lastApocalypseNumber );

                    int closestPlayerIndex = -1;
                    double closestDist = 999999999;
                    
                    for( int i=0; i<players.size(); i++ ) {
                        LiveObject *nextPlayer = players.getElement( i );
                        if( !nextPlayer->error ) {
                            
                            double dist = 
                                abs( nextPlayer->xd - apocalypseLocation.x ) +
                                abs( nextPlayer->yd - apocalypseLocation.y );
                            if( dist < closestDist ) {
                                closestPlayerIndex = i;
                                closestDist = dist;
                                }
                            }
                        
                        }
                    char *name = NULL;
                    if( closestPlayerIndex != -1 ) {
                        name = 
                            players.getElement( closestPlayerIndex )->
                            name;
                        }
                    
                    if( name == NULL ) {
                        name = stringDuplicate( "UNKNOWN" );
                        }
                    
                    char *idString = autoSprintf( "%d", lastApocalypseNumber );
                    
                    char *hash = hmac_sha1( reflectorSharedSecret, idString );

                    delete [] idString;

                    char *url = autoSprintf( 
                        "%s?action=trigger_apocalypse"
                        "&id=%d&id_hash=%s&name=%s",
                        reflectorURL, lastApocalypseNumber, hash, name );

                    delete [] hash;
                    delete [] name;
                    
                    printf( "Starting new web request for %s\n", url );
                    
                    apocalypseRequest =
                        new WebRequest( "GET", url, NULL );
                                
                    delete [] url;
                    delete [] reflectorSharedSecret;
                    }
                }


            // send all players the AP message
            const char *message = "AP\n#";
            int messageLength = strlen( message );
            
            for( int i=0; i<players.size(); i++ ) {
                LiveObject *nextPlayer = players.getElement( i );
                if( !nextPlayer->error && nextPlayer->connected ) {
                    
                    int numSent = 
                        nextPlayer->sock->send( 
                            (unsigned char*)message, 
                            messageLength,
                            false, false );
                    
                    nextPlayer->gotPartOfThisFrame = true;
                    
                    if( numSent != messageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed",  __func__ , __LINE__);
                        }
                    }
                }
            
            apocalypseStartTime = Time::getCurrentTime();
            apocalypseStarted = true;
            postApocalypseStarted = false;
            }

        if( apocalypseRequest != NULL ) {
            
            int result = apocalypseRequest->step();
                

            if( result == -1 ) {
                AppLog::info( 
                    "Apocalypse trigger:  Request to reflector failed." );
                }
            else if( result == 1 ) {
                // done, have result

                char *webResult = 
                    apocalypseRequest->getResult();
                printf( "Apocalypse trigger:  "
                        "Got web result:  '%s'\n", webResult );
                
                if( strstr( webResult, "OK" ) == NULL ) {
                    AppLog::infoF( 
                        "Apocalypse trigger:  "
                        "Bad response from reflector:  %s.",
                        webResult );
                    }
                delete [] webResult;
                }
            
            if( result != 0 ) {
                delete apocalypseRequest;
                apocalypseRequest = NULL;
                }
            }

        if( apocalypseRequest == NULL &&
            Time::getCurrentTime() - apocalypseStartTime >= 8 ) {
            
            if( ! postApocalypseStarted  ) {
                AppLog::infoF( "Enough warning time, %d players still alive",
                               players.size() );
                
                
                double startTime = Time::getCurrentTime();
                
                if( familyDataLogFile != NULL ) {
                    fprintf( familyDataLogFile, "%.2f apocalypse triggered\n",
                             startTime );
                    }
    

                // clear map
                freeMap( true );

                AppLog::infoF( "Apocalypse freeMap took %f sec",
                               Time::getCurrentTime() - startTime );
                wipeMapFiles();

                AppLog::infoF( "Apocalypse wipeMapFiles took %f sec",
                               Time::getCurrentTime() - startTime );
                
                initMap();

                reseedMap( true );
                
                AppLog::infoF( "Apocalypse initMap took %f sec",
                               Time::getCurrentTime() - startTime );
                
                peaceTreaties.deleteAll();
                warPeaceRecords.deleteAll();
                activeKillStates.deleteAll();

                lastRemoteApocalypseCheckTime = curTime;
                
                for( int i=0; i<players.size(); i++ ) {
                    LiveObject *nextPlayer = players.getElement( i );
                    backToBasics( nextPlayer );
                    }
                
                // send everyone update about everyone
                for( int i=0; i<players.size(); i++ ) {
                    LiveObject *nextPlayer = players.getElement( i );
                    nextPlayer->firstMessageSent = false;
                    nextPlayer->firstMapSent = false;
                    nextPlayer->inFlight = false;
                    }

                postApocalypseStarted = true;
                }
            else {
                // make sure all players have gotten map and update
                char allMapAndUpdate = true;
                
                for( int i=0; i<players.size(); i++ ) {
                    LiveObject *nextPlayer = players.getElement( i );
                    if( ! nextPlayer->firstMapSent ) {
                        allMapAndUpdate = false;
                        break;
                        }
                    }
                
                if( allMapAndUpdate ) {
                    
                    // send all players the AD message
                    const char *message = "AD\n#";
                    int messageLength = strlen( message );
            
                    for( int i=0; i<players.size(); i++ ) {
                        LiveObject *nextPlayer = players.getElement( i );
                        if( !nextPlayer->error && nextPlayer->connected ) {
                    
                            int numSent = 
                                nextPlayer->sock->send( 
                                    (unsigned char*)message, 
                                    messageLength,
                                    false, false );
                            
                            nextPlayer->gotPartOfThisFrame = true;
                    
                            if( numSent != messageLength ) {
                                setPlayerDisconnected( nextPlayer, 
                                                       "Socket write failed"  ,__func__ , __LINE__);
                                }
                            }
                        }

                    // totally done
                    apocalypseStarted = false;
                    apocalypseTriggered = false;
                    apocalypseRemote = false;
                    postApocalypseStarted = false;
                    }
                }    
            }
        }
    }




void monumentStep() {
    if( monumentCallPending ) {
        
        // send to all players
        for( int i=0; i<players.size(); i++ ) {
            LiveObject *nextPlayer = players.getElement( i );
            // remember it to tell babies about it
            nextPlayer->monumentPosSet = true;
            nextPlayer->lastMonumentPos.x = monumentCallX;
            nextPlayer->lastMonumentPos.y = monumentCallY;
            nextPlayer->lastMonumentID = monumentCallID;
            nextPlayer->monumentPosSent = true;
            
            if( !nextPlayer->error && nextPlayer->connected ) {
                
                char *message = autoSprintf( "MN\n%d %d %d\n#", 
                                             monumentCallX -
                                             nextPlayer->birthPos.x, 
                                             monumentCallY -
                                             nextPlayer->birthPos.y,
                                             hideIDForClient( 
                                                 monumentCallID ) );
                int messageLength = strlen( message );


                int numSent = 
                    nextPlayer->sock->send( 
                        (unsigned char*)message, 
                        messageLength,
                        false, false );
                
                nextPlayer->gotPartOfThisFrame = true;
                
                delete [] message;

                if( numSent != messageLength ) {
                    setPlayerDisconnected( nextPlayer, "Socket write failed",  __func__ , __LINE__);
                    }
                }
            }

        monumentCallPending = false;
        }
    }




// inPlayerName may be destroyed inside this function
// returns a uniquified name, sometimes newly allocated.
// return value destroyed by caller
char *getUniqueCursableName( char *inPlayerName, char *outSuffixAdded,
                             char inIsEve ) {
    
    char dup = isNameDuplicateForCurses( inPlayerName );
    
    if( ! dup ) {
        *outSuffixAdded = false;

        if( inIsEve ) {
            // make sure Eve doesn't have same last name as any living person
            char firstName[99];
            char lastName[99];
            
            sscanf( inPlayerName, "%s %s", firstName, lastName );
            
            for( int i=0; i<players.size(); i++ ) {
                LiveObject *o = players.getElement( i );
                
                if( ! o->error && o->familyName != NULL &&
                    strcmp( o->familyName, lastName ) == 0 ) {
                    
                    dup = true;
                    break;
                    }
                }
            }
        

        return inPlayerName;
        }    
    
    
    if( false ) {
        // old code, add suffix to make unique

        *outSuffixAdded = true;

        int targetPersonNumber = 1;
        
        char *fullName = stringDuplicate( inPlayerName );

        while( dup ) {
            // try next roman numeral
            targetPersonNumber++;
            
            int personNumber = targetPersonNumber;            
        
            SimpleVector<char> romanNumeralList;
        
            while( personNumber >= 100 ) {
                romanNumeralList.push_back( 'C' );
                personNumber -= 100;
                }
            while( personNumber >= 50 ) {
                romanNumeralList.push_back( 'L' );
                personNumber -= 50;
                }
            while( personNumber >= 40 ) {
                romanNumeralList.push_back( 'X' );
                romanNumeralList.push_back( 'L' );
                personNumber -= 40;
                }
            while( personNumber >= 10 ) {
                romanNumeralList.push_back( 'X' );
                personNumber -= 10;
                }
            while( personNumber >= 9 ) {
                romanNumeralList.push_back( 'I' );
                romanNumeralList.push_back( 'X' );
                personNumber -= 9;
                }
            while( personNumber >= 5 ) {
                romanNumeralList.push_back( 'V' );
                personNumber -= 5;
                }
            while( personNumber >= 4 ) {
                romanNumeralList.push_back( 'I' );
                romanNumeralList.push_back( 'V' );
                personNumber -= 4;
                }
            while( personNumber >= 1 ) {
                romanNumeralList.push_back( 'I' );
                personNumber -= 1;
                }
            
            char *romanString = romanNumeralList.getElementString();

            delete [] fullName;
            
            fullName = autoSprintf( "%s %s", inPlayerName, romanString );
            delete [] romanString;
            
            dup = isNameDuplicateForCurses( fullName );
            }
        
        delete [] inPlayerName;
        
        return fullName;
        }
    else {
        // new code:
        // make name unique by finding close matching name that hasn't been
        // used recently
        
        *outSuffixAdded = false;

        char firstName[99];
        char lastName[99];
        
        int numNames = sscanf( inPlayerName, "%s %s", firstName, lastName );
        
        if( numNames == 1 ) {
            // special case, find a totally unique first name for them
            
            int i = getFirstNameIndex( firstName );

            while( dup ) {

                int nextI;
                
                dup = isNameDuplicateForCurses( getFirstName( i, &nextI ) );
            
                if( dup ) {
                    i = nextI;
                    }
                }
            
            if( dup ) {
                // ran out of names, yikes
                return inPlayerName;
                }
            else {
                delete [] inPlayerName;
                int nextI;
                return stringDuplicate( getFirstName( i, &nextI ) );
                }
            }
        else if( numNames == 2 ) {
            if( inIsEve ) {
                // cycle last names until we find one not used by any
                // family
                
                int i = getLastNameIndex( lastName );
            
                const char *tempLastName = "";
                
                while( dup ) {
                    
                    int nextI;
                    tempLastName = getLastName( i, &nextI );
                    
                    dup = false;

                    for( int j=0; j<players.size(); j++ ) {
                        LiveObject *o = players.getElement( j );
                        
                        if( ! o->error && 
                            o->familyName != NULL &&
                            strcmp( o->familyName, tempLastName ) == 0 ) {
                    
                            dup = true;
                            break;
                            }
                        }
                    
                    if( dup ) {
                        i = nextI;
                        }
                    }
            
                if( dup ) {
                    // ran out of names, yikes
                    return inPlayerName;
                    }
                else {
                    delete [] inPlayerName;
                    return autoSprintf( "%s %s", firstName, tempLastName );
                    }
                }
            else {
                // cycle first names until we find one
                int i = getFirstNameIndex( firstName );
            
                char *tempName = NULL;
                
                while( dup ) {                    
                    if( tempName != NULL ) {
                        delete [] tempName;
                        }
                    
                    int nextI;
                    tempName = autoSprintf( "%s %s", getFirstName( i, &nextI ),
                                            lastName );
                    

                    dup = isNameDuplicateForCurses( tempName );
                    if( dup ) {
                        i = nextI;
                        }
                    }
            
                if( dup ) {
                    // ran out of names, yikes
                    if( tempName != NULL ) {
                        delete [] tempName;
                        }
                    return inPlayerName;
                    }
                else {
                    delete [] inPlayerName;
                    return tempName;
                    }
                }
            }
        else {
            // weird case, name doesn't even have two string parts, give up
            return inPlayerName;
            }
        }
    
    }




typedef struct ForcedEffects {
        // -1 if no emot specified
        int emotIndex;
        int ttlSec;
        
        char foodModifierSet;
        double foodCapModifier;
        
        char feverSet;
        float fever;
    } ForcedEffects;
        


ForcedEffects checkForForcedEffects( int inHeldObjectID ) {
    ForcedEffects e = { -1, 0, false, 1.0, false, 0.0f };
    
    ObjectRecord *o = getObject( inHeldObjectID );
    
    if( o != NULL ) {
        char *emotPos = strstr( o->description, "emot_" );
        
        if( emotPos != NULL ) {
            sscanf( emotPos, "emot_%d_%d", 
                    &( e.emotIndex ), &( e.ttlSec ) );
            }

        char *foodPos = strstr( o->description, "food_" );
        
        if( foodPos != NULL ) {
            int numRead = sscanf( foodPos, "food_%lf", 
                                  &( e.foodCapModifier ) );
            if( numRead == 1 ) {
                e.foodModifierSet = true;
                }
            }

        char *feverPos = strstr( o->description, "fever_" );
        
        if( feverPos != NULL ) {
            int numRead = sscanf( feverPos, "fever_%f", 
                                  &( e.fever ) );
            if( numRead == 1 ) {
                e.feverSet = true;
                }
            }
        }
    
    
    return e;
    }




void setNoLongerDying( LiveObject *inPlayer, 
                       SimpleVector<int> *inPlayerIndicesToSendHealingAbout ) {
    inPlayer->dying = false;
    inPlayer->murderSourceID = 0;
    inPlayer->murderPerpID = 0;
    if( inPlayer->murderPerpEmail != 
        NULL ) {
        delete [] 
            inPlayer->murderPerpEmail;
        inPlayer->murderPerpEmail =
            NULL;
        }
    
    inPlayer->deathSourceID = 0;
    inPlayer->holdingWound = false;
    inPlayer->customGraveID = -1;
    
    inPlayer->emotFrozen = false;
    inPlayer->emotUnfreezeETA = 0;
    
    inPlayer->foodCapModifier = 1.0;
    inPlayer->foodUpdate = true;

    inPlayer->fever = 0;

    if( inPlayer->deathReason 
        != NULL ) {
        delete [] inPlayer->deathReason;
        inPlayer->deathReason = NULL;
        }
                                        
    inPlayerIndicesToSendHealingAbout->
        push_back( 
            getLiveObjectIndex( 
                inPlayer->id ) );
    }



static void checkSickStaggerTime( LiveObject *inPlayer ) {
    ObjectRecord *heldObj = NULL;
    
    if( inPlayer->holdingID > 0 ) {
        heldObj = getObject( inPlayer->holdingID );
        }
    else {
        return;
        }

    
    char isSick = false;
    
    if( strstr(
            heldObj->
            description,
            "sick" ) != NULL ) {
        isSick = true;
        
        // sicknesses override basic death-stagger
        // time.  The person can live forever
        // if they are taken care of until
        // the sickness passes
        
        int staggerTime = 
            SettingsManager::getIntSetting(
                "deathStaggerTime", 20 );
        
        double currentTime = 
            Time::getCurrentTime();
        
        // 10x base stagger time should
        // give them enough time to either heal
        // from the disease or die from its
        // side-effects
        inPlayer->dyingETA = 
            currentTime + 10 * staggerTime;
        }
    
    if( isSick ) {
        // what they have will heal on its own 
        // with time.  Sickness, not wound.
        
        // death source is sickness, not
        // source
        inPlayer->deathSourceID = 
            inPlayer->holdingID;
        
        setDeathReason( inPlayer, 
                        "succumbed",
                        inPlayer->holdingID );
        }
    }



typedef struct FlightDest {
        int playerID;
        GridPos destPos;
    } FlightDest;
        



// inEatenID = 0 for nursing
static void checkForFoodEatingEmot( LiveObject *inPlayer,
                                    int inEatenID ) {
    
    char wasStarving = inPlayer->starving;
    inPlayer->starving = false;

    
    if( inEatenID > 0 ) {
        
        ObjectRecord *o = getObject( inEatenID );
        
        if( o != NULL ) {
            char *emotPos = strstr( o->description, "emotEat_" );
            
            if( emotPos != NULL ) {
                int e, t;
                int numRead = sscanf( emotPos, "emotEat_%d_%d", &e, &t );
                
                if( numRead == 2 && !inPlayer->emotFrozen ) {
                    inPlayer->emotFrozen = true;
                    inPlayer->emotFrozenIndex = e;
                    
                    inPlayer->emotUnfreezeETA = Time::getCurrentTime() + t;
                    
                    newEmotPlayerIDs.push_back( inPlayer->id );
                    newEmotIndices.push_back( e );
                    newEmotTTLs.push_back( t );
                    return;
                    }
                }
            }
        }

    // no food emot found
    if( wasStarving && !inPlayer->emotFrozen ) {
        // clear their starving emot
        newEmotPlayerIDs.push_back( inPlayer->id );
        newEmotIndices.push_back( -1 );
        newEmotTTLs.push_back( 0 );
        }
                
    }
    
static void drinkAlcohol( LiveObject *inPlayer, int inAlcoholAmount ) {
    double doneGrowingAge = 16;
    
    double multiplier = 1.0;
    

    double age = computeAge( inPlayer );
    
    // alcohol affects a baby 2x
    // affects an 8-y-o 1.5x
    if( age < doneGrowingAge ) {
        multiplier += 1.0 - age / doneGrowingAge;
        }

    double amount = inAlcoholAmount * multiplier;
    
    inPlayer->drunkenness += amount;
    
    if( inPlayer->drunkenness >= 6 ) {
        
        double drunkennessEffectDuration = 60.0;
        
        inPlayer->drunkennessEffectETA = Time::getCurrentTime() + drunkennessEffectDuration;
        inPlayer->drunkennessEffect = true;
        
        makePlayerSay( inPlayer, (char*)"+DRUNK+", true );
        
        }
    }


static void doDrug( LiveObject *inPlayer ) {
    
    double trippingEffectDelay = 15.0;
    double trippingEffectDuration = 30.0;
    double curTime = Time::getCurrentTime();
    
    if( !inPlayer->tripping && !inPlayer->gonnaBeTripping ) {
        inPlayer->gonnaBeTripping = true;
        inPlayer->trippingEffectStartTime = curTime + trippingEffectDelay;
        inPlayer->trippingEffectETA = curTime + trippingEffectDelay + trippingEffectDuration;
        }
    else if( !inPlayer->tripping && inPlayer->gonnaBeTripping ) {
        // Half the delay if they keep munching drug before effect hits
        float remainingDelay = inPlayer->trippingEffectStartTime - curTime;
        if( remainingDelay > 0 ) {
            inPlayer->trippingEffectStartTime = curTime + 0.5 * remainingDelay;
            }
        }
    else {
        // Refresh duration if they are already tripping
        inPlayer->trippingEffectETA = curTime + trippingEffectDuration;
        }
        
    }
    
    
// returns true if frozen emote cleared successfully
static bool clearFrozenEmote( LiveObject *inPlayer, int inEmoteIndex ) {
    if( !inPlayer->emotFrozen ||
        (inPlayer->emotFrozen &&
        inPlayer->emotFrozenIndex == inEmoteIndex) ) {
            
        inPlayer->emotFrozen = false;
        inPlayer->emotUnfreezeETA = 0;
        
        newEmotPlayerIDs.push_back( inPlayer->id );
        newEmotIndices.push_back( -1 );
        newEmotTTLs.push_back( 0 );
        
        return true;
        }
    
    return false;
    }


// return true if it worked
char addKillState( LiveObject *inKiller, LiveObject *inTarget ) {
    char found = false;
    
    
    if( distance( getPlayerPos( inKiller ), getPlayerPos( inTarget ) )
        > 8 ) {
        // out of range
        return false;
        }
    
    

    for( int i=0; i<activeKillStates.size(); i++ ) {
        KillState *s = activeKillStates.getElement( i );
        
        if( s->killerID == inKiller->id ) {
            found = true;
            s->killerWeaponID = inKiller->holdingID;
            s->targetID = inTarget->id;

            double curTime = Time::getCurrentTime();
            s->emotStartTime = curTime;
            s->killStartTime = curTime;

            s->emotRefreshSeconds = 10;
            break;
            }
        }
    
    if( !found ) {
        // add new
        double curTime = Time::getCurrentTime();
        KillState s = { inKiller->id, 
                        inKiller->holdingID,
                        inTarget->id, 
                        curTime,
                        curTime,
                        10 };
        activeKillStates.push_back( s );

        // force target to gasp
        makePlayerSay( inTarget, (char*)"[GASP]" );
        }
    return true;
    }



static void removeKillState( LiveObject *inKiller, LiveObject *inTarget ) {
    for( int i=0; i<activeKillStates.size(); i++ ) {
        KillState *s = activeKillStates.getElement( i );
    
        if( s->killerID == inKiller->id &&
            s->targetID == inTarget->id ) {
            activeKillStates.deleteElement( i );
            
            break;
            }
        }

    if( inKiller != NULL ) {
        // clear their emot
        inKiller->emotFrozen = false;
        inKiller->emotUnfreezeETA = 0;
        
        newEmotPlayerIDs.push_back( inKiller->id );
        
        newEmotIndices.push_back( -1 );
        newEmotTTLs.push_back( 0 );
        }
    
    if( inTarget != NULL &&
        inTarget->emotFrozen &&
        inTarget->emotFrozenIndex == victimEmotionIndex ) {
        
        // inTarget's emot hasn't been replaced, end it
        inTarget->emotFrozen = false;
        inTarget->emotUnfreezeETA = 0;
        
        newEmotPlayerIDs.push_back( inTarget->id );
        
        newEmotIndices.push_back( -1 );
        newEmotTTLs.push_back( 0 );
        }
    }



static void removeAnyKillState( LiveObject *inKiller ) {
    for( int i=0; i<activeKillStates.size(); i++ ) {
        KillState *s = activeKillStates.getElement( i );
    
        if( s->killerID == inKiller->id ) {
            
            LiveObject *target = getLiveObject( s->targetID );
            
            if( target != NULL ) {
                removeKillState( inKiller, target );
                i--;
                }
            }
        }
    }

            



static void interruptAnyKillEmots( int inPlayerID, 
                                   int inInterruptingTTL ) {
    for( int i=0; i<activeKillStates.size(); i++ ) {
        KillState *s = activeKillStates.getElement( i );
        
        if( s->killerID == inPlayerID ) {
            s->emotStartTime = Time::getCurrentTime();
            s->emotRefreshSeconds = inInterruptingTTL;
            break;
            }
        }
    }    



static void setPerpetratorHoldingAfterKill( LiveObject *nextPlayer,
                                            TransRecord *woundHit,
                                            TransRecord *rHit,
                                            TransRecord *r ) {

    int oldHolding = nextPlayer->holdingID;


    if( rHit != NULL ) {
        // if hit trans exist
        // leave bloody knife or
        // whatever in hand
        nextPlayer->holdingID = rHit->newActor;
        holdingSomethingNew( nextPlayer,
                             oldHolding );
        }
    else if( woundHit != NULL ) {
        // result of hit on held weapon 
        // could also be
        // specified in wound trans
        nextPlayer->holdingID = 
            woundHit->newActor;
        holdingSomethingNew( nextPlayer,
                             oldHolding );
        }
    else if( r != NULL ) {
        nextPlayer->holdingID = r->newActor;
        holdingSomethingNew( nextPlayer,
                             oldHolding );
        }
                        
    if( r != NULL || rHit != NULL || woundHit != NULL ) {
        
        nextPlayer->heldTransitionSourceID = 0;
        
        if( oldHolding != 
            nextPlayer->holdingID ) {
            
            setFreshEtaDecayForHeld( 
                nextPlayer );
            }
        }
    }




void executeKillAction( int inKillerIndex,
                        int inTargetIndex,
                        SimpleVector<int> *playerIndicesToSendUpdatesAbout,
                        SimpleVector<int> *playerIndicesToSendDyingAbout,
                        SimpleVector<int> *newEmotPlayerIDs,
                        SimpleVector<int> *newEmotIndices,
                        SimpleVector<int> *newEmotTTLs ) {
    int i = inKillerIndex;
    LiveObject *nextPlayer = players.getElement( inKillerIndex );    

    LiveObject *hitPlayer = players.getElement( inTargetIndex );

    GridPos targetPos = getPlayerPos( hitPlayer );


    // send update even if action fails (to let them
    // know that action is over)
    playerIndicesToSendUpdatesAbout->push_back( i );
                        
    if( nextPlayer->holdingID > 0 ) {
                            
        nextPlayer->actionAttempt = 1;
        nextPlayer->actionTarget.x = targetPos.x;
        nextPlayer->actionTarget.y = targetPos.y;
                            
        if( nextPlayer->actionTarget.x > nextPlayer->xd ) {
            nextPlayer->facingOverride = 1;
            }
        else if( nextPlayer->actionTarget.x < nextPlayer->xd ) {
            nextPlayer->facingOverride = -1;
            }

        // holding something
        ObjectRecord *heldObj = 
            getObject( nextPlayer->holdingID );
                            
        if( heldObj->deadlyDistance > 0 ) {
            // it's deadly

            GridPos playerPos = getPlayerPos( nextPlayer );
                                
            double d = distance( targetPos,
                                 playerPos );
                                
            if( heldObj->deadlyDistance >= d &&
                ! directLineBlocked( playerPos, 
                                     targetPos ) ) {
                // target is close enough
                // and no blocking objects along the way                

                char someoneHit = false;


                if( hitPlayer != NULL &&
                    strstr( heldObj->description,
                            "otherFamilyOnly" ) ) {
                    // make sure victim is in
                    // different family
                    // and no treaty
                                        
                    if( hitPlayer->lineageEveID ==
                        nextPlayer->lineageEveID
                        || 
                        isPeaceTreaty( hitPlayer->lineageEveID,
                                       nextPlayer->lineageEveID ) ) {
                                            
                        hitPlayer = NULL;
                        }
                    }
                

                // special case:
                // non-lethal no_replace ends up in victim's hand
                // they aren't dying, but they may have an emot
                // effect only
                if( hitPlayer != NULL ) {

                    TransRecord *woundHit = 
                        getPTrans( nextPlayer->holdingID, 
                                   0, true, false );

                    if( woundHit != NULL && woundHit->newTarget > 0 &&
                        strstr( getObject( woundHit->newTarget )->description,
                                "no_replace" ) != NULL ) {
                        
                        
                        TransRecord *rHit = 
                            getPTrans( nextPlayer->holdingID, 0, false, true );
                        
                        TransRecord *r = 
                            getPTrans( nextPlayer->holdingID, 0 );

                        setPerpetratorHoldingAfterKill( nextPlayer,
                                                        woundHit, rHit, r );
                        
                        ForcedEffects e = 
                            checkForForcedEffects( woundHit->newTarget );
                            
                        // emote-effect only for no_replace
                        // no fever or food effect
                        if( e.emotIndex != -1 ) {
                            hitPlayer->emotFrozen = 
                                true;
                            hitPlayer->emotFrozenIndex = e.emotIndex;
                            
                            hitPlayer->emotUnfreezeETA =
                                Time::getCurrentTime() + e.ttlSec;
                            
                            newEmotPlayerIDs->push_back( 
                                hitPlayer->id );
                            newEmotIndices->push_back( 
                                e.emotIndex );
                            newEmotTTLs->push_back( 
                                e.ttlSec );

                            interruptAnyKillEmots( hitPlayer->id,
                                                   e.ttlSec );
                            }
                        return;
                        }
                    }
                

                if( hitPlayer != NULL ) {
                    someoneHit = true;
                    // break the connection with 
                    // them, eventually
                    // let them stagger a bit first

                    hitPlayer->murderSourceID =
                        nextPlayer->holdingID;
                                        
                    hitPlayer->murderPerpID =
                        nextPlayer->id;
                                        
                    // brand this player as a murderer
                    nextPlayer->everKilledAnyone = true;

                    if( hitPlayer->murderPerpEmail 
                        != NULL ) {
                        delete [] 
                            hitPlayer->murderPerpEmail;
                        }
                                        
                    hitPlayer->murderPerpEmail =
                        stringDuplicate( 
                            nextPlayer->email );
                                        

                    setDeathReason( hitPlayer, 
                                    "killed",
                                    nextPlayer->holdingID );

                    // if not already dying
                    if( ! hitPlayer->dying ) {
                        int staggerTime = 
                            SettingsManager::getIntSetting(
                                "deathStaggerTime", 20 );
                                            
                        double currentTime = 
                            Time::getCurrentTime();
                                            
                        hitPlayer->dying = true;
                        hitPlayer->dyingETA = 
                            currentTime + staggerTime;

                        playerIndicesToSendDyingAbout->
                            push_back( 
                                getLiveObjectIndex( 
                                    hitPlayer->id ) );
                                        
                        hitPlayer->errorCauseString =
                            "Player killed by other player";
                        }
                    else {
                        // already dying, 
                        // and getting attacked again
                        
                        // halve their remaining 
                        // stagger time
                        double currentTime = 
                            Time::getCurrentTime();
                                             
                        double staggerTimeLeft = 
                            hitPlayer->dyingETA - 
                            currentTime;
                        
                        if( staggerTimeLeft > 0 ) {
                            staggerTimeLeft /= 2;
                            hitPlayer->dyingETA = 
                                currentTime + 
                                staggerTimeLeft;
                            }
                        }
                    }
                                    
                                    
                // a player either hit or not
                // in either case, weapon was used
                                    
                // check for a transition for weapon

                // 0 is generic "on person" target
                TransRecord *r = 
                    getPTrans( nextPlayer->holdingID, 
                               0 );

                TransRecord *rHit = NULL;
                TransRecord *woundHit = NULL;
                                    
                if( someoneHit ) {
                    // last use on target specifies
                    // grave and weapon change on hit
                    // non-last use (r above) specifies
                    // what projectile ends up in grave
                    // or on ground
                    rHit = 
                        getPTrans( nextPlayer->holdingID, 
                                   0, false, true );
                                        
                    if( rHit != NULL &&
                        rHit->newTarget > 0 ) {
                        hitPlayer->customGraveID = 
                            rHit->newTarget;
                        }
                                        
                    char wasSick = false;
                                        
                    if( hitPlayer->holdingID > 0 &&
                        strstr(
                            getObject( 
                                hitPlayer->holdingID )->
                            description,
                            "sick" ) != NULL ) {
                        wasSick = true;
                        }

                    // last use on actor specifies
                    // what is left in victim's hand
                    woundHit = 
                        getPTrans( nextPlayer->holdingID, 
                                   0, true, false );
                                        
                    if( woundHit != NULL &&
                        woundHit->newTarget > 0 ) {
                                            
                        // don't drop their wound
                        if( hitPlayer->holdingID != 0 &&
                            ! hitPlayer->holdingWound ) {
                            handleDrop( 
                                targetPos.x, targetPos.y, 
                                hitPlayer,
                                playerIndicesToSendUpdatesAbout );
                            }

                        // give them a new wound
                        // if they don't already have
                        // one, but never replace their
                        // original wound.  That allows
                        // a healing exploit where you
                        // intentionally give someone
                        // an easier-to-treat wound
                        // to replace their hard-to-treat
                        // wound

                        // however, do let wounds replace
                        // sickness
                        char woundChange = false;
                                            
                        if( ! hitPlayer->holdingWound ||
                            wasSick ) {
                            woundChange = true;
                                                
                            hitPlayer->holdingID = 
                                woundHit->newTarget;
                            holdingSomethingNew( 
                                hitPlayer );
                            setFreshEtaDecayForHeld( 
                                hitPlayer );
                            }
                                            
                                            
                        hitPlayer->holdingWound = true;
                                            
                        if( woundChange ) {
                                                
                            ForcedEffects e = 
                                checkForForcedEffects( 
                                    hitPlayer->holdingID );
                            
                            if( e.emotIndex != -1 ) {
                                hitPlayer->emotFrozen = 
                                    true;
                                hitPlayer->emotFrozenIndex = e.emotIndex;
                                
                                newEmotPlayerIDs->push_back( 
                                    hitPlayer->id );
                                newEmotIndices->push_back( 
                                    e.emotIndex );
                                newEmotTTLs->push_back( 
                                    e.ttlSec );
                                interruptAnyKillEmots( hitPlayer->id,
                                                       e.ttlSec );
                                }
                                            
                            if( e.foodModifierSet && 
                                e.foodCapModifier != 1 ) {
                                                
                                hitPlayer->
                                    foodCapModifier = 
                                    e.foodCapModifier;
                                hitPlayer->foodUpdate = 
                                    true;
                                }
                                                
                            if( e.feverSet ) {
                                hitPlayer->fever = e.fever;
                                }

                            checkSickStaggerTime( 
                                hitPlayer );
                                                
                            playerIndicesToSendUpdatesAbout->
                                push_back( 
                                    getLiveObjectIndex( 
                                        hitPlayer->id ) );
                            }   
                        }
                    }
                                    

                int oldHolding = nextPlayer->holdingID;

                setPerpetratorHoldingAfterKill( nextPlayer, 
                                                woundHit, rHit, r );

                timeSec_t oldEtaDecay = 
                    nextPlayer->holdingEtaDecay;
                                    

                if( r != NULL ) {
                                    
                    if( hitPlayer != NULL &&
                        r->newTarget != 0 ) {
                                        
                        hitPlayer->embeddedWeaponID = 
                            r->newTarget;
                                        
                        if( oldHolding == r->newTarget ) {
                            // what we are holding
                            // is now embedded in them
                            // keep old decay
                            hitPlayer->
                                embeddedWeaponEtaDecay =
                                oldEtaDecay;
                            }
                        else {
                                            
                            TransRecord *newDecayT = 
                                getMetaTrans( 
                                    -1, 
                                    r->newTarget );
                    
                            if( newDecayT != NULL ) {
                                hitPlayer->
                                    embeddedWeaponEtaDecay = 
                                    Time::getCurrentTime() + 
                                    newDecayT->
                                    autoDecaySeconds;
                                }
                            else {
                                // no further decay
                                hitPlayer->
                                    embeddedWeaponEtaDecay 
                                    = 0;
                                }
                            }
                        }
                    else if( hitPlayer == NULL &&
                             isMapSpotEmpty( targetPos.x, 
                                             targetPos.y ) ) {
                        // this is old code, and probably never gets executed
                        
                        // no player hit, and target ground
                        // spot is empty
                        setMapObject( targetPos.x, targetPos.y, 
                                      r->newTarget );
                                        
                        // if we're thowing a weapon
                        // target is same as what we
                        // were holding
                        if( oldHolding == r->newTarget ) {
                            // preserve old decay time 
                            // of what we were holding
                            setEtaDecay( targetPos.x, targetPos.y,
                                         oldEtaDecay );
                            }
                        }
                    // else new target, post-kill-attempt
                    // is lost
                    }
                }
            }
        }
    }




void nameBaby( LiveObject *inNamer, LiveObject *inBaby, char *inName,
               SimpleVector<int> *playerIndicesToSendNamesAbout ) {    

    LiveObject *nextPlayer = inNamer;
    LiveObject *babyO = inBaby;
    
    char *name = inName;
    
    
    const char *lastName = "";
    if( nextPlayer->name != NULL ) {
        lastName = strstr( nextPlayer->name, 
                           " " );
                                        
        if( lastName != NULL ) {
            // skip space
            lastName = &( lastName[1] );
            }

        if( lastName == NULL ) {
            lastName = "";

            if( nextPlayer->familyName != 
                NULL ) {
                lastName = 
                    nextPlayer->familyName;
                }    
            }
        else if( nextPlayer->nameHasSuffix ) {
            // only keep last name
            // if it contains another
            // space (the suffix is after
            // the last name).  Otherwise
            // we are probably confused,
            // and what we think
            // is the last name IS the suffix.
                                            
            char *suffixPos =
                strstr( (char*)lastName, " " );
                                            
            if( suffixPos == NULL ) {
                // last name is suffix, actually
                // don't pass suffix on to baby
                lastName = "";
                }
            else {
                // last name plus suffix
                // okay to pass to baby
                // because we strip off
                // third part of name
                // (suffix) below.
                }
            }
        }
    else if( nextPlayer->familyName != NULL ) {
        lastName = nextPlayer->familyName;
        }
    else if( babyO->familyName != NULL ) {
        lastName = babyO->familyName;
        }
                                    


    const char *close = 
        findCloseFirstName( name );

    if( strcmp( lastName, "" ) != 0 ) {    
        babyO->name = autoSprintf( "%s %s",
                                   close, 
                                   lastName );
        }
    else {
        babyO->name = stringDuplicate( close );
        }
    
    
    if( babyO->familyName == NULL &&
        nextPlayer->familyName != NULL ) {
        // mother didn't have a family 
        // name set when baby was born
        // now she does
        // or whatever player named 
        // this orphaned baby does
        babyO->familyName = 
            stringDuplicate( 
                nextPlayer->familyName );
        }
                                    

    int spaceCount = 0;
    int lastSpaceIndex = -1;

    int nameLen = strlen( babyO->name );
    for( int s=0; s<nameLen; s++ ) {
        if( babyO->name[s] == ' ' ) {
            lastSpaceIndex = s;
            spaceCount++;
            }
        }
                                    
    if( spaceCount > 1 ) {
        // remove suffix from end
        babyO->name[ lastSpaceIndex ] = '\0';
        }
                                    
    babyO->name = getUniqueCursableName( 
        babyO->name, 
        &( babyO->nameHasSuffix ), false );
                                    
    logName( babyO->id,
             babyO->email,
             babyO->name,
             babyO->lineageEveID );
                                    
    playerIndicesToSendNamesAbout->push_back( 
        getLiveObjectIndex( babyO->id ) );
    }




void getLineageLineForPlayer( LiveObject *inPlayer,
                              SimpleVector<char> *inVector ) {
    
    char *pID = autoSprintf( "%d", inPlayer->id );
    inVector->appendElementString( pID );
    delete [] pID;
    
    for( int j=0; j<inPlayer->lineage->size(); j++ ) {
        char *mID = 
            autoSprintf( 
                " %d",
                inPlayer->lineage->getElementDirect( j ) );
        inVector->appendElementString( mID );
        delete [] mID;
        }        
    // include eve tag at end
    char *eveTag = autoSprintf( " eve=%d",
                                inPlayer->lineageEveID );
    inVector->appendElementString( eveTag );
    delete [] eveTag;
    
    inVector->push_back( '\n' );            
    }





void logFitnessDeath( LiveObject *nextPlayer ) {
    
    double curTime = Time::getCurrentTime();
    for( int i=0; i<players.size(); i++ ) {
            
        LiveObject *o = players.getElement( i );
        
        if( o->error ||
            o->isTutorial ||
            o->id == nextPlayer->id ) {
            continue;
        }
        
        SimpleVector<double> *newAncestorLifeEndTimeSeconds = new SimpleVector<double>();

        for( int e=0; e< o->ancestorIDs->size(); e++ ) {
            
            if( o->ancestorIDs->getElementDirect( e ) == nextPlayer->id ) {
                newAncestorLifeEndTimeSeconds->push_back( curTime );
            } else {
                newAncestorLifeEndTimeSeconds->push_back( o->ancestorLifeEndTimeSeconds->getElementDirect( e ) );
            }
        }
            
        for( int e=0; e< o->ancestorIDs->size(); e++ ) {
            o->ancestorLifeEndTimeSeconds->deleteElement( e );
        }
        delete o->ancestorLifeEndTimeSeconds;
        o->ancestorLifeEndTimeSeconds = newAncestorLifeEndTimeSeconds;
        
    }
    
    // log this death for fitness purposes,
    // for both tutorial and non    


    // if this person themselves died before their mom, gma, etc.
    // remove them from the "ancestor" list of everyone who is older than they
    // are and still alive

    // You only get genetic points for ma, gma, and other older ancestors
    // if you are alive when they die.

    // This ends an exploit where people suicide as a baby (or young person)
    // yet reap genetic benefit from their mother living a long life
    // (your mother, gma, etc count for your genetic score if you yourself
    //  live beyond 3, so it is in your interest to protect them)
    double deadPersonAge = computeAge( nextPlayer );
    if( deadPersonAge < forceDeathAge ) {
        for( int i=0; i<players.size(); i++ ) {
                
            LiveObject *o = players.getElement( i );
            
            if( o->error ||
                o->isTutorial ||
                o->id == nextPlayer->id ) {
                continue;
                }
            
            if( computeAge( o ) < deadPersonAge ) {
                // this person was born after the dead person
                // thus, there's no way they are their ma, gma, etc.
                continue;
                }

            for( int e=0; e< o->ancestorIDs->size(); e++ ) {
                if( o->ancestorIDs->getElementDirect( e ) == nextPlayer->id ) {
                    o->ancestorIDs->deleteElement( e );
                    
                    delete [] o->ancestorEmails->getElementDirect( e );
                    o->ancestorEmails->deleteElement( e );
                
                    delete [] o->ancestorRelNames->getElementDirect( e );
                    o->ancestorRelNames->deleteElement( e );
                    
                    o->ancestorLifeStartTimeSeconds->deleteElement( e );
                    o->ancestorLifeEndTimeSeconds->deleteElement( e );

                    break;
                    }
                }
            }
        }


    SimpleVector<int> emptyAncestorIDs;
    SimpleVector<char*> emptyAncestorEmails;
    SimpleVector<char*> emptyAncestorRelNames;
    SimpleVector<double> emptyAncestorLifeStartTimeSeconds;
    SimpleVector<double> emptyAncestorLifeEndTimeSeconds;
    

    //SimpleVector<int> *ancestorIDs = nextPlayer->ancestorIDs;
    SimpleVector<char*> *ancestorEmails = nextPlayer->ancestorEmails;
    SimpleVector<char*> *ancestorRelNames = nextPlayer->ancestorRelNames;
    //SimpleVector<double> *ancestorLifeStartTimeSeconds = 
    //    nextPlayer->ancestorLifeStartTimeSeconds;
    SimpleVector<double> *ancestorLifeEndTimeSeconds = 
        nextPlayer->ancestorLifeEndTimeSeconds;   

    SimpleVector<char*> ancestorData;
    double deadPersonLifeStartTime = nextPlayer->trueStartTimeSeconds;
    double ageRate = getAgeRate();
    
    for( int i=0; i<ancestorEmails->size(); i++ ) {
        
        double endTime = ancestorLifeEndTimeSeconds->getElementDirect( i );
        double parentingTime = 0.0;
        
        if( endTime > 0 ) {
            parentingTime = ageRate * (endTime - deadPersonLifeStartTime);
        } else {
            parentingTime = ageRate * (curTime - deadPersonLifeStartTime);
        }
        
        char buffer[16];
        snprintf(buffer, sizeof buffer, "%.6f", parentingTime);
        
        ancestorData.push_back( buffer );
        
    } 

    logFitnessDeath( players.size(),
                     nextPlayer->email, 
                     nextPlayer->name, nextPlayer->displayID,
                     computeAge( nextPlayer ),
                     ancestorEmails, 
                     ancestorRelNames,
                     &ancestorData
                     );
    }

    
    
// access blocked b/c of access direction or ownership?
static char isAccessBlocked( LiveObject *inPlayer, 
                             int inTargetX, int inTargetY,
                             int inTargetID ) {
    int target = inTargetID;
    
    int x = inTargetX;
    int y = inTargetY;
    

    char wrongSide = false;
    char ownershipBlocked = false;
    // password-protected objects
    char blockedByPassword = false;
    char notStandingOnSameTile = false;
    
    if( target > 0 ) {
        ObjectRecord *targetObj = getObject( target );

        if( isGridAdjacent( x, y,
                            inPlayer->xd, 
                            inPlayer->yd ) ) {
            
            if( targetObj->useDistance == 0 &&
                ( inTargetY != inPlayer->yd ||
                  inTargetX != inPlayer->xd ) ) {
                notStandingOnSameTile = true;
                }
            
            if( targetObj->sideAccess ) {
                
                if( y > inPlayer->yd ||
                    y < inPlayer->yd ) {
                    // access from N or S
                    wrongSide = true;
                    }
                }
            else if( targetObj->noBackAccess ) {
                if( y < inPlayer->yd ) {
                    // access from N
                    wrongSide = true;
                    }
                }
            }
        if( targetObj->isOwned ) {
            // make sure player owns this pos
            ownershipBlocked = 
                ! isOwned( inPlayer, x, y );
            }
            
        // password-protected objects
        if( targetObj->passwordProtectable ) {
            
            for( int i=passwordRecords.size()-1; i>=0; i-- ) {
                passwordRecord r = passwordRecords.getElementDirect(i);
                if( x == r.x && y == r.y ) {
                    if ( inPlayer->saidPassword == NULL ) {
                        // player hasn't said any password
                        blockedByPassword = true;
                        
                        sendGlobalMessage( (char*)"SAY A PASSWORD FIRST.**"
                            "SAY   PASSWORD IS XXX   TO SET YOUR PASSWORD."
                            , inPlayer );
                        }
                    else {                    
                        std::string tryPw( inPlayer->saidPassword );
                        if( tryPw.compare( r.password ) != 0 ) {
                            // passwords do not match
                            blockedByPassword = true;
                            
                            sendGlobalMessage( (char*)"WRONG PASSWORD.", inPlayer );
                            }
                        }
                    break;
                    }
                }
            }
        }
            
    return wrongSide || ownershipBlocked || blockedByPassword || notStandingOnSameTile;
    }



// returns NULL if not found
static LiveObject *getPlayerByName( char *inName, 
                                    LiveObject *inPlayerSayingName ) {
    for( int j=0; j<players.size(); j++ ) {
        LiveObject *otherPlayer = players.getElement( j );
        if( ! otherPlayer->error &&
            otherPlayer != inPlayerSayingName &&
            otherPlayer->name != NULL &&
            strcmp( otherPlayer->name, inName ) == 0 ) {
            
            return otherPlayer;
            }
        }
    
    // no exact match.

    // does name contain no space?
    char *spacePos = strstr( inName, " " );

    if( spacePos == NULL ) {
        // try again, using just the first name for each potential target

        // stick a space at the end to forbid matching prefix of someone's name
        char *firstName = autoSprintf( "%s ", inName );

        LiveObject *matchingPlayer = NULL;
        double matchingDistance = DBL_MAX;
        
        GridPos playerPos = getPlayerPos( inPlayerSayingName );
        

        for( int j=0; j<players.size(); j++ ) {
            LiveObject *otherPlayer = players.getElement( j );
            if( ! otherPlayer->error &&
                otherPlayer != inPlayerSayingName &&
                otherPlayer->name != NULL &&
                // does their name start with firstName
                strstr( otherPlayer->name, firstName ) == otherPlayer->name ) {
                
                GridPos pos = getPlayerPos( otherPlayer );            
                double d = distance( pos, playerPos );
                
                if( d < matchingDistance ) {
                    matchingDistance = d;
                    matchingPlayer = otherPlayer;
                    }
                }
            }
        
        delete [] firstName;

        return matchingPlayer;
        }
        

    return NULL;
    }
    
    


static void sendCraving( LiveObject *inPlayer ) {
    // they earn the normal YUM multiplier increase (+1) if food is actually yum PLUS the bonus
    // increase, so send them the total.
    
    int totalBonus = inPlayer->cravingFood.bonus;
    if( isReallyYummy( inPlayer, inPlayer->cravingFood.foodID ) ) totalBonus = totalBonus + 1;
    
    char *message = autoSprintf( "CR\n%d %d\n#", 
                                 inPlayer->cravingFood.foodID,
                                 totalBonus );
    sendMessageToPlayer( inPlayer, message, strlen( message ) );
    delete [] message;

    inPlayer->cravingKnown = true;
    }




static void handleHeldDecay( 
    LiveObject *nextPlayer, int i,
    SimpleVector<int> *playerIndicesToSendUpdatesAbout,
    SimpleVector<int> *playerIndicesToSendHealingAbout ) {
    
    int oldID = nextPlayer->holdingID;
    
    TransRecord *t = getPTrans( -1, oldID );
    
    if( t != NULL ) {
        
        int newID = t->newTarget;
        
        handleHoldingChange( nextPlayer, newID );
        
        if( newID == 0 &&
            nextPlayer->holdingWound &&
            nextPlayer->dying ) {
            
            // wound decayed naturally, count as healed
            setNoLongerDying( 
                nextPlayer,
                playerIndicesToSendHealingAbout );            
            }
        
        
        nextPlayer->heldTransitionSourceID = -1;
        
        ObjectRecord *newObj = getObject( newID );
        ObjectRecord *oldObj = getObject( oldID );
        
        
        if( newObj != NULL && newObj->permanent &&
            oldObj != NULL && ! oldObj->permanent &&
            ! nextPlayer->holdingWound ) {
            // object decayed into a permanent
            // force drop
            GridPos dropPos = 
                getPlayerPos( nextPlayer );
            
            handleDrop( 
                dropPos.x, dropPos.y, 
                nextPlayer,
                playerIndicesToSendUpdatesAbout );
            }
        
        
        playerIndicesToSendUpdatesAbout->push_back( i );
        }
    else {
        // no decay transition exists
        // clear it
        setFreshEtaDecayForHeld( nextPlayer );
        }
    }



// check if target has a 1-second
// decay specified
// if so, make it happen NOW and set in map
// return new target id
static int checkTargetInstantDecay( int inTarget, int inX, int inY ) {
    int newTarget = inTarget;
    
    TransRecord *targetDecay = getPTrans( -1, inTarget );
    
    // do NOT auto-apply movement transitions here
    // don't want result of move to repace this object in place, because
    // moving object might leave something behind, or require something to
    // land on in its destination (like a moving cart leaving one track
    // and landing on another)

    if( targetDecay != NULL &&
        targetDecay->autoDecaySeconds == 1  &&
        targetDecay->newTarget > 0 &&
        targetDecay->move == 0 ) {
                                            
        newTarget = targetDecay->newTarget;
                                            
        setMapObject( inX, inY, newTarget );
        }
    
    return newTarget;
    }


static void setRefuseFoodEmote( LiveObject *hitPlayer ) {
    if( hitPlayer->emotFrozen ) {
        return;
        }
    
    int newEmotIndex =
        SettingsManager::
        getIntSetting( 
            "refuseFoodEmotionIndex",
            -1 );
    if( newEmotIndex != -1 ) {
        newEmotPlayerIDs.push_back( 
            hitPlayer->id );
        
        newEmotIndices.push_back( 
            newEmotIndex );
        // was 5 sec for OHOL's babies' refuseFoodEmote
        // changed to 3 for 2HOL's mehEmote
        newEmotTTLs.push_back( 3 );
        }
    }
/**
 * use to check if two passwords are equal, to prevent timing attacks.
 * the function will not early return in the for loop unlike strcmp.
*/
int constant_time_strcmp(const char *s1, const char *s2) {
    size_t i = 0;
    int areDifferent = 0;
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    // return 0 if equal, return 1 if not equal
    if(len1 != len2)
        return 1;
    size_t max_len = (len1 > len2) ? len1 : len2;

    for (i = 0; i < max_len; i++) {
        areDifferent |= s1[i % len1] ^ s2[i % len2];
        }
    
    if(areDifferent)
        return 1;
    return 0;
    }


int main() {

    if( checkReadOnly() ) {
        printf( "File system read-only.  Server exiting.\n" );
        return 1;
        }
    
    familyDataLogFile = fopen( "familyDataLog.txt", "a" );

    if( familyDataLogFile != NULL ) {
        fprintf( familyDataLogFile, "%.2f server starting up\n",
                 Time::getCurrentTime() );
        }


    memset( allowedSayCharMap, false, 256 );
    
    int numAllowed = strlen( allowedSayChars );
    for( int i=0; i<numAllowed; i++ ) {
        allowedSayCharMap[ (int)( allowedSayChars[i] ) ] = true;
        }
    

    nextID = 
        SettingsManager::getIntSetting( "nextPlayerID", 2 );


    // make backup and delete old backup every day
    AppLog::setLog( new FileLog( "log.txt", 86400 ) );

    // Log::INFO_LEVEL = 4
    // Log::DETAIL_LEVEL = 5
    // Log::TRACE_LEVEL = 6
    int logLevel = SettingsManager::getIntSetting( "logLevel", 4 );
    
    switch(logLevel) {
        case 4:
            logLevel = Log::INFO_LEVEL;
            break;
        case 5:
            logLevel = Log::DETAIL_LEVEL;
            break;
        case 6:
            logLevel = Log::TRACE_LEVEL;
            break;
        default:
            logLevel = Log::INFO_LEVEL;
        }

    AppLog::setLoggingLevel( logLevel );
    AppLog::printAllMessages( true );

    printf( "\n" );
    AppLog::info( "Server starting up" );

    printf( "\n" );
    
    
    

    nextSequenceNumber = 
        SettingsManager::getIntSetting( "sequenceNumber", 1 );

    requireClientPassword =
        SettingsManager::getIntSetting( "requireClientPassword", 1 );
    
    requireTicketServerCheck =
        SettingsManager::getIntSetting( "requireTicketServerCheck", 1 );
    
    clientPassword = 
        SettingsManager::getStringSetting( "clientPassword" );


    int dataVer = readIntFromFile( "dataVersionNumber.txt", 1 );
    int codVer = readIntFromFile( "serverCodeVersionNumber.txt", 1 );
    
    versionNumber = dataVer;
    if( codVer > versionNumber ) {
        versionNumber = codVer;
        }
    
    printf( "\n" );
    AppLog::infoF( "Server using version number %d", versionNumber );

    printf( "\n" );
    


    babyInheritMonument = 
        SettingsManager::getIntSetting( "babyInheritMonument", 1 );

    minFoodDecrementSeconds = 
        SettingsManager::getFloatSetting( "minFoodDecrementSeconds", 5.0f );

    maxFoodDecrementSeconds = 
        SettingsManager::getFloatSetting( "maxFoodDecrementSeconds", 20 );

    babyBirthFoodDecrement = 
        SettingsManager::getIntSetting( "babyBirthFoodDecrement", 10 );


    eatBonus = 
        SettingsManager::getIntSetting( "eatBonus", 0 );


    secondsPerYear = 
        SettingsManager::getFloatSetting( "secondsPerYear", 60.0f );
    

    if( clientPassword == NULL ) {
        requireClientPassword = 0;
        }


    ticketServerURL = 
        SettingsManager::getStringSetting( "ticketServerURL" );
    

    if( ticketServerURL == NULL ) {
        requireTicketServerCheck = 0;
        }

    
    reflectorURL = SettingsManager::getStringSetting( "reflectorURL" );

    apocalypsePossible = 
        SettingsManager::getIntSetting( "apocalypsePossible", 0 );

    lastApocalypseNumber = 
        SettingsManager::getIntSetting( "lastApocalypseNumber", 0 );


    childSameRaceLikelihood =
        (double)SettingsManager::getFloatSetting( "childSameRaceLikelihood",
                                                  0.90 );
    
    familySpan =
        SettingsManager::getIntSetting( "familySpan", 2 );
    
    
    readPhrases( "babyNamingPhrases", &nameGivingPhrases );
    readPhrases( "familyNamingPhrases", &familyNameGivingPhrases );

    readPhrases( "cursingPhrases", &cursingPhrases );

    readPhrases( "forgivingPhrases", &forgivingPhrases );
    readPhrases( "forgiveYouPhrases", &youForgivingPhrases );

    
    readPhrases( "youGivingPhrases", &youGivingPhrases );
    readPhrases( "namedGivingPhrases", &namedGivingPhrases );
    
    // password-protected objects
    readPhrases( "passwordProtectingPhrases", &passwordProtectingPhrases );
    
    readPhrases( "infertilityDeclaringPhrases", &infertilityDeclaringPhrases );
    readPhrases( "fertilityDeclaringPhrases", &fertilityDeclaringPhrases );

    eveName = 
        SettingsManager::getStringSetting( "eveName", "EVE" );
    infertilitySuffix = 
        SettingsManager::getStringSetting( "infertilitySuffix", "+INFERTILE+" );
    fertilitySuffix = 
        SettingsManager::getStringSetting( "fertilitySuffix", "+FERTILE+" );
    
    curseYouPhrase = 
        SettingsManager::getSettingContents( "curseYouPhrase", 
                                             "CURSE YOU" );
    
    curseBabyPhrase = 
        SettingsManager::getSettingContents( "curseBabyPhrase", 
                                             "CURSE MY BABY" );



    
    killEmotionIndex =
        SettingsManager::getIntSetting( "killEmotionIndex", 2 );

    victimEmotionIndex =
        SettingsManager::getIntSetting( "victimEmotionIndex", 2 );

    starvingEmotionIndex =
        SettingsManager::getIntSetting( "starvingEmotionIndex", 2 );

    // if changed also change in discordController.cpp
    afkEmotionIndex =
        SettingsManager::getIntSetting( "afkEmotionIndex", 2 );

    drunkEmotionIndex =
        SettingsManager::getIntSetting( "drunkEmotionIndex", 2 );

    trippingEmotionIndex =
        SettingsManager::getIntSetting( "trippingEmotionIndex", 2 );

    afkTimeSeconds =
        SettingsManager::getDoubleSetting( "afkTimeSeconds", 120.0 );

    satisfiedEmotionIndex =
        SettingsManager::getIntSetting( "satisfiedEmotionIndex", 2 );


    FILE *f = fopen( "curseWordList.txt", "r" );
    
    if( f != NULL ) {
    
        int numRead = 1;
        
        char buff[100];
        
        while( numRead == 1 ) {
            numRead = fscanf( f, "%99s", buff );
            
            if( numRead == 1 ) {
                if( strlen( buff ) < 6 ) {
                    // short words only, 3, 4, 5 letters
                    curseWords.push_back( stringToUpperCase( buff ) );
                    }
                }
            }
        fclose( f );
        }
    printf( "Curse word list has %d words\n", curseWords.size() );
    

#ifdef WIN_32
    printf( "\n\nPress CTRL-C to shut down server gracefully\n\n" );

    SetConsoleCtrlHandler( ctrlHandler, TRUE );
#else
    printf( "\n\nPress CTRL-Z to shut down server gracefully\n\n" );

    signal( SIGTSTP, intHandler );
#endif

    initNames();

    initCurses();
    
    initLifeTokens();
    
    initFitnessScore();
    

    initLifeLog();
    //initBackup();
    
    initPlayerStats();
    initLineageLog();
    
    initLineageLimit();
    
    initCurseDB();
    

    char rebuilding;

    initAnimationBankStart( &rebuilding );
    while( initAnimationBankStep() < 1.0 );
    initAnimationBankFinish();


    initObjectBankStart( &rebuilding, true, true );
    while( initObjectBankStep() < 1.0 );
    initObjectBankFinish();

    
    initCategoryBankStart( &rebuilding );
    while( initCategoryBankStep() < 1.0 );
    initCategoryBankFinish();


    // auto-generate category-based transitions
    initTransBankStart( &rebuilding, true, true, true, true );
    while( initTransBankStep() < 1.0 );
    initTransBankFinish();
    

    // defaults to one hour
    int epochSeconds = 
        SettingsManager::getIntSetting( "epochSeconds", 3600 );
    
    setTransitionEpoch( epochSeconds );


    initFoodLog();
    initFailureLog();

    initObjectSurvey();
    
    initLanguage();
    initFamilySkipList();
    
    
    initTriggers();


    if( initMap() != true ) {
        // cannot continue after map init fails
        return 1;
        }
    


    if( false ) {
        
        printf( "Running map sampling\n" );
    
        int idA = 290;
        int idB = 942;
        
        int totalCountA = 0;
        int totalCountB = 0;
        int numRuns = 2;

        for( int i=0; i<numRuns; i++ ) {
        
        
            int countA = 0;
            int countB = 0;
        
            int x = randSource.getRandomBoundedInt( 10000, 300000 );
            int y = randSource.getRandomBoundedInt( 10000, 300000 );
        
            printf( "Sampling at %d,%d\n", x, y );


            for( int yd=y; yd<y + 2400; yd++ ) {
                for( int xd=x; xd<x + 2400; xd++ ) {
                    int oID = getMapObject( xd, yd );
                
                    if( oID == idA ) {
                        countA ++;
                        }
                    else if( oID == idB ) {
                        countB ++;
                        }
                    }
                }
            printf( "   Count at %d,%d is %d = %d, %d = %d\n",
                    x, y, idA, countA, idB, countB );

            totalCountA += countA;
            totalCountB += countB;
            }
        printf( "Average count %d (%s) = %f,  %d (%s) = %f  over %d runs\n",
                idA, getObject( idA )->description, 
                totalCountA / (double)numRuns,
                idB, getObject( idB )->description, 
                totalCountB / (double)numRuns,
                numRuns );
        printf( "Press ENTER to continue:\n" );
    
        int readInt;
        scanf( "%d", &readInt );
        }
    


    
    int port = 
        SettingsManager::getIntSetting( "port", 5077 );
    
    char *toTrim = SettingsManager::getStringSetting("playerListSecret", "");
    char *trimmed = trimWhitespace(toTrim);
    delete [] toTrim;
    if(strlen(trimmed) > 0) {
        playerListSecret = trimmed;
        }
    else {
        playerListSecret = NULL;
        delete[] trimmed;
        }
    SocketServer *server = new SocketServer(port, 256);

    sockPoll.addSocketServer( server );
    
    AppLog::infoF( "Listening for connection on port %d", port );

    // if we received one the last time we looped, don't sleep when
    // polling for socket being ready, because there could be more data
    // waiting in the buffer for a given socket
    char someClientMessageReceived = false;
    
    
    int shutdownMode = SettingsManager::getIntSetting( "shutdownMode", 0 );
    int forceShutdownMode = 
            SettingsManager::getIntSetting( "forceShutdownMode", 0 );
        
    
    // test code for printing sample eve locations
    // direct output from server to out.txt
    // then run:
    // grep "Eve location" out.txt | sed -e "s/Eve location //" | 
    //      sed -e "s/,/ /" > eveTest.txt
    // Then in gnuplot, do:
    //  plot "eveTest.txt" using 1:2 with linespoints;

    /*
    for( int i=0; i<1000; i++ ) {
        int x, y;
        
        SimpleVector<GridPos> temp;
        
        getEvePosition( "test@blah", 1, &x, &y, &temp, false );
        
        printf( "Eve location %d,%d\n", x, y );
        }
    */


    while( !quit ) {

        double curStepTime = Time::getCurrentTime();
        
        // flush past players hourly
        if( curStepTime - lastPastPlayerFlushTime > 3600 ) {
            
            // default one week
            int pastPlayerFlushTime = 
                SettingsManager::getIntSetting( "pastPlayerFlushTime", 604000 );
            
            for( int i=0; i<pastPlayers.size(); i++ ) {
                DeadObject *o = pastPlayers.getElement( i );
                
                if( curStepTime - o->lifeStartTimeSeconds > 
                    pastPlayerFlushTime ) {
                    // stale
                    delete [] o->name;
                    delete o->lineage;
                    pastPlayers.deleteElement( i );
                    i--;
                    } 
                }
            
            lastPastPlayerFlushTime = curStepTime;
            }
        
        
        char periodicStepThisStep = false;
        
        if( curStepTime - lastPeriodicStepTime > periodicStepTime ) {
            periodicStepThisStep = true;
            lastPeriodicStepTime = curStepTime;
            }
        
        
        if( periodicStepThisStep ) {
            shutdownMode = SettingsManager::getIntSetting( "shutdownMode", 0 );
            forceShutdownMode = 
                SettingsManager::getIntSetting( "forceShutdownMode", 0 );

            int logLevel = SettingsManager::getIntSetting( "logLevel", 4 );
            
            switch(logLevel) {
                case 4:
                    logLevel = Log::INFO_LEVEL;
                    break;
                case 5:
                    logLevel = Log::DETAIL_LEVEL;
                    break;
                case 6:
                    logLevel = Log::TRACE_LEVEL;
                    break;
                default:
                    logLevel = Log::INFO_LEVEL;
                }

            AppLog::setLoggingLevel( logLevel );
            
            if( checkReadOnly() ) {
                // read-only file system causes all kinds of weird 
                // behavior
                // shut this server down NOW
                printf( "File system read only, forcing server shutdown.\n" );

                // force-run cron script one time here
                // this will send warning email to admin
                // (cron jobs stop running if filesystem read-only)
                system( "../scripts/checkServerRunningCron.sh" );

                shutdownMode = 1;
                forceShutdownMode = 1;
                }
            char *toTrim = SettingsManager::getStringSetting("playerListSecret", "");
            char *trimmed = trimWhitespace(toTrim);
            delete [] toTrim;
            if(strlen(trimmed) > 0) {
                playerListSecret = trimmed;
                }
            else {
                playerListSecret = NULL;
                delete[] trimmed;
                }
            }
        
        
        if( forceShutdownMode ) {
            shutdownMode = 1;
        
            const char *shutdownMessage = "SD\n#";
            int messageLength = strlen( shutdownMessage );
            
            // send everyone who's still alive a shutdown message
            for( int i=0; i<players.size(); i++ ) {
                LiveObject *nextPlayer = players.getElement( i );
                
                if( nextPlayer->error ) {
                    continue;
                    }

                if( nextPlayer->connected ) {    
                    nextPlayer->sock->send( 
                        (unsigned char*)shutdownMessage, 
                        messageLength,
                        false, false );
                
                    nextPlayer->gotPartOfThisFrame = true;
                    }
                
                // don't worry about num sent
                // it's the last message to this client anyway
                setDeathReason( nextPlayer, 
                                "forced_shutdown" );
                nextPlayer->error = true;
                nextPlayer->errorCauseString =
                    "Forced server shutdown";
                }
            }
        else if( shutdownMode ) {
            // any disconnected players should be killed now
            for( int i=0; i<players.size(); i++ ) {
                LiveObject *nextPlayer = players.getElement( i );
                if( ! nextPlayer->error && ! nextPlayer->connected ) {
                    
                    setDeathReason( nextPlayer, 
                                    "disconnect_shutdown" );
                    
                    nextPlayer->error = true;
                    nextPlayer->errorCauseString =
                        "Disconnected during shutdown";
                    }
                }
            }
        

        if( periodicStepThisStep ) {
            
            apocalypseStep();
            monumentStep();
            
            //checkBackup();

            stepFoodLog();
            stepFailureLog();
            
            stepPlayerStats();
            stepLineageLog();
            stepCurseServerRequests();
            
            stepLifeTokens();
            stepFitnessScore();
            
            stepMapLongTermCulling( players.size() );
            
            stepArcReport();
            
            int arcMilestone = getArcYearsToReport( secondsPerYear, 100 );

            int enableArcReport = 
                SettingsManager::getIntSetting( "enableArcReport", 1 );
            
            if( arcMilestone != -1 && enableArcReport ) {
                int familyLimitAfterEveWindow = 
                    SettingsManager::getIntSetting( 
                        "familyLimitAfterEveWindow", 15 );
                
                char *familyLine;
                
                if( familyLimitAfterEveWindow > 0 &&
                    ! isEveWindow() ) {
                    familyLine = autoSprintf( "of %d",
                                              familyLimitAfterEveWindow );
                    }
                else {
                    familyLine = stringDuplicate( "" );
                    }

                const char *familyWord = "FAMILIES ARE";
                
                int numFams = countFamilies();
                
                if( numFams == 1 ) {
                    familyWord = "FAMILY IS";
                    }

                char *message = autoSprintf( ":%s: ARC HAS LASTED %d YEARS**"
                                             "%d %s %s ALIVE",
                                             getArcName(),
                                             arcMilestone,
                                             numFams,
                                             familyLine,
                                             familyWord);
                delete [] familyLine;
                
                sendGlobalMessage( message );
                
                delete [] message;           
                }

            
            checkCustomGlobalMessage();
            

            int lowestCravingID = INT_MAX;
            
            for( int i=0; i< players.size(); i++ ) {
                LiveObject *nextPlayer = players.getElement( i );
                
                if( nextPlayer->cravingFood.uniqueID > -1 && 
                    nextPlayer->cravingFood.uniqueID < lowestCravingID ) {
                    
                    lowestCravingID = nextPlayer->cravingFood.uniqueID;
                    }

                // also send queued global messages
                if( nextPlayer->globalMessageQueue.size() > 0 &&
                    curStepTime - nextPlayer->lastGlobalMessageTime > 
                    minGlobalMessageSpacingSeconds ) {
                    
                    // send next one
                    char *message = 
                        nextPlayer->globalMessageQueue.getElementDirect( 0 );
                    nextPlayer->globalMessageQueue.deleteElement( 0 );
                    
                    sendGlobalMessage( message, nextPlayer );
                    
                    delete [] message;
                    }
                }
            purgeStaleCravings( lowestCravingID );
            }
        
        
        int numLive = players.size();



        if( shouldRunObjectSurvey() ) {
            SimpleVector<GridPos> livePlayerPos;
            
            for( int i=0; i<numLive; i++ ) {
                LiveObject *nextPlayer = players.getElement( i );
            
                if( nextPlayer->error ) {
                    continue;
                    }
                
                livePlayerPos.push_back( getPlayerPos( nextPlayer ) );
                }

            startObjectSurvey( &livePlayerPos );
            }
        
        stepObjectSurvey();
        
        stepLanguage();

        
        double secPerYear = 1.0 / getAgeRate();
        

        // check for timeout for shortest player move or food decrement
        // so that we wake up from listening to socket to handle it
        double minMoveTime = 999999;
        
        double curTime = Time::getCurrentTime();

        for( int i=0; i<numLive; i++ ) {
            LiveObject *nextPlayer = players.getElement( i );
            
            // clear at the start of each step
            nextPlayer->responsiblePlayerID = -1;

            if( nextPlayer->error ) {
                continue;
                }

            if( nextPlayer->xd != nextPlayer->xs ||
                nextPlayer->yd != nextPlayer->ys ) {
                
                double moveTimeLeft =
                    nextPlayer->moveTotalSeconds -
                    ( curTime - nextPlayer->moveStartTime );
                
                if( moveTimeLeft < 0 ) {
                    moveTimeLeft = 0;
                    }
                
                if( moveTimeLeft < minMoveTime ) {
                    minMoveTime = moveTimeLeft;
                    }
                }
            

            double timeLeft = minMoveTime;
            
            if( ! nextPlayer->vogMode ) {
                // look at food decrement time too
                
                timeLeft =
                    nextPlayer->foodDecrementETASeconds - curTime;
                
                if( timeLeft < 0 ) {
                    timeLeft = 0;
                    }
                if( timeLeft < minMoveTime ) {
                    minMoveTime = timeLeft;
                    }           
                }
            
            // look at held decay too
            if( nextPlayer->holdingEtaDecay != 0 ) {
                
                timeLeft = nextPlayer->holdingEtaDecay - curTime;
                
                if( timeLeft < 0 ) {
                    timeLeft = 0;
                    }
                if( timeLeft < minMoveTime ) {
                    minMoveTime = timeLeft;
                    }
                }
            
            for( int c=0; c<NUM_CLOTHING_PIECES; c++ ) {
                if( nextPlayer->clothingEtaDecay[c] != 0 ) {
                    timeLeft = nextPlayer->clothingEtaDecay[c] - curTime;
                    
                    if( timeLeft < 0 ) {
                        timeLeft = 0;
                        }
                    if( timeLeft < minMoveTime ) {
                        minMoveTime = timeLeft;
                        }
                    }
                for( int cc=0; cc<nextPlayer->clothingContained[c].size();
                     cc++ ) {
                    timeSec_t decay =
                        nextPlayer->clothingContainedEtaDecays[c].
                        getElementDirect( cc );
                    
                    if( decay != 0 ) {
                        timeLeft = decay - curTime;
                        
                        if( timeLeft < 0 ) {
                            timeLeft = 0;
                            }
                        if( timeLeft < minMoveTime ) {
                            minMoveTime = timeLeft;
                            }
                        }
                    }
                }
            
            // look at old age death to
            double ageLeft = forceDeathAge - computeAge( nextPlayer );
            
            double ageSecondsLeft = ageLeft * secPerYear;
            
            if( ageSecondsLeft < minMoveTime ) {
                minMoveTime = ageSecondsLeft;

                if( minMoveTime < 0 ) {
                    minMoveTime = 0;
                    }
                }
            

            // as low as it can get, no need to check other players
            if( minMoveTime == 0 ) {
                break;
                }
            }
        
        
        SocketOrServer *readySock =  NULL;

        // at bare minimum, run our periodic steps at a fixed
        // frequency
        double pollTimeout = periodicStepTime;
        
        if( minMoveTime < pollTimeout ) {
            // shorter timeout if we have to wake up for a move
            
            // HOWEVER, always keep max timout at 2 sec
            // so we always wake up periodically to catch quit signals, etc

            pollTimeout = minMoveTime;
            }
        
        if( pollTimeout > 0 ) {
            double shortestDecay = getNextDecayDelta();
            
            if( shortestDecay != -1 ) {
                
                if( shortestDecay < pollTimeout ) {
                    pollTimeout = shortestDecay;
                    }
                }
            }

        
        char anyTicketServerRequestsOut = false;

        for( int i=0; i<newConnections.size(); i++ ) {
            
            FreshConnection *nextConnection = newConnections.getElement( i );

            if( nextConnection->ticketServerRequest != NULL ) {
                anyTicketServerRequestsOut = true;
                break;
                }
            }
        
        if( anyTicketServerRequestsOut ) {
            // need to step outstanding ticket server web requests
            // sleep a tiny amount of time to avoid cpu spin
            pollTimeout = 0.01;
            }


        if( areTriggersEnabled() ) {
            // need to handle trigger timing
            pollTimeout = 0.01;
            }

        if( someClientMessageReceived ) {
            // don't wait at all
            // we need to check for next message right away
            pollTimeout = 0;
            }

        if( tutorialLoadingPlayers.size() > 0 ) {
            // don't wait at all if there are tutorial maps to load
            pollTimeout = 0;
            }
        

        if( pollTimeout > 0.1 && activeKillStates.size() > 0 ) {
            // we have active kill requests pending
            // want a short timeout so that we can catch kills 
            // when player's paths cross
            pollTimeout = 0.1;
            }
        

        // we thus use zero CPU as long as no messages or new connections
        // come in, and only wake up when some timed action needs to be
        // handled
        
        readySock = sockPoll.wait( (int)( pollTimeout * 1000 ) );
        
        
        
        
        if( readySock != NULL && !readySock->isSocket ) {
            // server ready
            Socket *sock = server->acceptConnection( 0 );

            if( sock != NULL ) {
                HostAddress *a = sock->getRemoteHostAddress();
                
                if( a == NULL ) {    
                    AppLog::info( "Got connection from unknown address" );
                    }
                else {
                    AppLog::infoF( "Got connection from %s:%d",
                                  a->mAddressString, a->mPort );
                    delete a;
                    }
            

                FreshConnection newConnection;
                
                newConnection.connectionStartTimeSeconds = 
                    Time::getCurrentTime();

                newConnection.email = NULL;

                newConnection.sock = sock;

                newConnection.sequenceNumber = nextSequenceNumber;

                

                char *secretString = 
                    SettingsManager::getStringSetting( 
                        "statsServerSharedSecret", "sdfmlk3490sadfm3ug9324" );

                char *numberString = 
                    autoSprintf( "%lu", newConnection.sequenceNumber );
                
                char *nonce = hmac_sha1( secretString, numberString );

                delete [] secretString;
                delete [] numberString;

                newConnection.sequenceNumberString = 
                    autoSprintf( "%s%lu", nonce, 
                                 newConnection.sequenceNumber );
                
                delete [] nonce;
                    

                newConnection.tutorialNumber = 0;
                newConnection.curseStatus.curseLevel = 0;
                newConnection.curseStatus.excessPoints = 0;

                newConnection.twinCode = NULL;
                newConnection.twinCount = 0;
                
                
                nextSequenceNumber ++;
                
                SettingsManager::setSetting( "sequenceNumber",
                                             (int)nextSequenceNumber );
                
                char *message;
                
                int maxPlayers = 
                    SettingsManager::getIntSetting( "maxPlayers", 200 );
                
                int currentPlayers = players.size() + newConnections.size();
                    

                if( apocalypseTriggered || shutdownMode ) {
                        
                    AppLog::info( "We are in shutdown mode, "
                                  "deflecting new connection" );         
                    
                    AppLog::infoF( "%d player(s) still alive on server.",
                                   players.size() );

                    message = autoSprintf( "SHUTDOWN\n"
                                           "%d/%d\n"
                                           "#",
                                           currentPlayers, maxPlayers );
                    newConnection.shutdownMode = true;
                    }
                else if( currentPlayers >= maxPlayers ) {
                    AppLog::infoF( "%d of %d permitted players connected, "
                                   "deflecting new connection",
                                   currentPlayers, maxPlayers );
                    
                    message = autoSprintf( "SERVER_FULL\n"
                                           "%d/%d\n"
                                           "#",
                                           currentPlayers, maxPlayers );
                    
                    newConnection.shutdownMode = true;
                    }         
                else {
                    message = autoSprintf( "SN\n"
                                           "%d/%d\n"
                                           "%s\n"
                                           "%lu\n#",
                                           currentPlayers, maxPlayers,
                                           newConnection.sequenceNumberString,
                                           versionNumber );
                    newConnection.shutdownMode = false;
                    }


                // wait for email and hashes to come from client
                // (and maybe ticket server check isn't required by settings)
                newConnection.ticketServerRequest = NULL;
                newConnection.ticketServerAccepted = false;
                newConnection.lifeTokenSpent = false;
                
                newConnection.error = false;
                newConnection.errorCauseString = "";
                newConnection.rejectedSendTime = 0;
                newConnection.playerListSent = false;
                int messageLength = strlen( message );
                
                int numSent = 
                    sock->send( (unsigned char*)message, 
                                messageLength, 
                                false, false );
                    
                delete [] message;
                    

                if( numSent != messageLength ) {
                    // failed or blocked on our first send attempt

                    // reject it right away

                    delete sock;
                    sock = NULL;
                    newConnection.error = true;
                    AppLog::infoF("socket write error. msg_len: %d, write_len:%d", messageLength, numSent);
                    newConnection.errorCauseString = "socket write error";
                    
                    }
                else {
                    // first message sent okay
                    newConnection.sockBuffer = new SimpleVector<char>();
                    

                    sockPoll.addSocket( sock );

                    newConnections.push_back( newConnection );
                    }

                AppLog::infoF( "Listening for another connection on port %d", 
                               port );
    
                }
            }
        

        stepTriggers();
        
        
        // listen for messages from new connections
        double currentTime = Time::getCurrentTime();
        std::string seed; 
        for( int i=0; i<newConnections.size(); i++ ) {
            
            FreshConnection *nextConnection = newConnections.getElement( i );
            
            if( nextConnection->error ) {
                continue;
                }
            
            if( nextConnection->email != NULL &&
                nextConnection->curseStatus.curseLevel == -1 ) {
                // keep checking if curse level has arrived from
                // curse server
                nextConnection->curseStatus =
                    getCurseLevel( nextConnection->email );
                if( nextConnection->curseStatus.curseLevel != -1 ) {
                    AppLog::infoF( 
                        "Got curse level for %s from curse server: "
                        "%d (excess %d)",
                        nextConnection->email,
                        nextConnection->curseStatus.curseLevel,
                        nextConnection->curseStatus.excessPoints );
                    }
                }
            else if( nextConnection->email != NULL &&
                nextConnection->lifeStats.lifeCount == -1 ) {
                // keep checking if life stats have arrived from
                // stats server
                int statsResult = getPlayerLifeStats( nextConnection->email,
                    &( nextConnection->lifeStats.lifeCount ),
                    &( nextConnection->lifeStats.lifeTotalSeconds ) );
                
                if( statsResult == -1 ) {
                    // error
                    // it's done now!
                    nextConnection->lifeStats.lifeCount = 0;
                    nextConnection->lifeStats.lifeTotalSeconds = 0;
                    nextConnection->lifeStats.error = true;
                    }
                else if( statsResult == 1 ) {
                    AppLog::infoF( 
                        "Got life stats for %s from stats server: "
                        "%d lives, %d total seconds (%.2lf hours)",
                        nextConnection->email,
                        nextConnection->lifeStats.lifeCount,
                        nextConnection->lifeStats.lifeTotalSeconds,
                        nextConnection->lifeStats.lifeTotalSeconds / 3600.0 );
                    }
                }
            else if( nextConnection->ticketServerRequest != NULL &&
                     ! nextConnection->ticketServerAccepted ) {
                
                int result;

                if( currentTime - nextConnection->ticketServerRequestStartTime
                    < 8 ) {
                    // 8-second timeout on ticket server requests
                    result = nextConnection->ticketServerRequest->step();
                    }
                else {
                    result = -1;
                    }

                if( result == -1 ) {
                    AppLog::info( "Request to ticket server failed, "
                                  "client rejected." );
                    nextConnection->error = true;
                    nextConnection->errorCauseString =
                        "Ticket server failed";
                    }
                else if( result == 1 ) {
                    // done, have result

                    char *webResult = 
                        nextConnection->ticketServerRequest->getResult();
                    
                    if( strstr( webResult, "INVALID" ) != NULL ) {
                        AppLog::info( 
                            "Client key hmac rejected by ticket server, "
                            "client rejected." );
                        nextConnection->error = true;
                        nextConnection->errorCauseString =
                            "Client key check failed";
                        }
                    else if( strstr( webResult, "VALID" ) != NULL ) {
                        // correct!
                        nextConnection->ticketServerAccepted = true;
                        }
                    else {
                        AppLog::errorF( 
                            "Unexpected result from ticket server, "
                            "client rejected:  %s", webResult );
                        nextConnection->error = true;
                        nextConnection->errorCauseString =
                            "Client key check failed "
                            "(bad ticketServer response)";
                        }
                    delete [] webResult;
                    }
                }
            else if( nextConnection->ticketServerRequest != NULL &&
                     nextConnection->ticketServerAccepted &&
                     ! nextConnection->lifeTokenSpent ) {

                // this "butDisconnected" state applies even if
                // we see them as connected, becasue they are clearly
                // reconnecting now
                char liveButDisconnected = false;
                
                for( int p=0; p<players.size(); p++ ) {
                    LiveObject *o = players.getElement( p );
                    if( ! o->error && 
                        strcmp( o->email, 
                                nextConnection->email ) == 0 ) {
                        liveButDisconnected = true;
                        break;
                        }
                    }

                if( liveButDisconnected ) {
                    // spent when they first connected, don't respend now
                    nextConnection->lifeTokenSpent = true;
                    }
                else {
                    int spendResult = 
                        spendLifeToken( nextConnection->email );
                    if( spendResult == -1 ) {
                        AppLog::info( 
                            "Failed to spend life token for client, "
                            "client rejected." );

                        const char *message = "NO_LIFE_TOKENS\n#";
                        nextConnection->sock->send( (unsigned char*)message,
                                                    strlen( message ), 
                                                    false, false );

                        nextConnection->error = true;
                        nextConnection->errorCauseString =
                            "Client life token spend failed";
                        }
                    else if( spendResult == 1 ) {
                        nextConnection->lifeTokenSpent = true;
                        }
                    }
                }
            else if( nextConnection->ticketServerRequest != NULL &&
                     nextConnection->ticketServerAccepted &&
                     nextConnection->lifeTokenSpent ) {
                // token spent successfully (or token server not used)

                const char *message = "ACCEPTED\n#";
                int messageLength = strlen( message );
                
                int numSent = 
                    nextConnection->sock->send( 
                        (unsigned char*)message, 
                        messageLength, 
                        false, false );
                        

                if( numSent != messageLength ) {
                    AppLog::info( "Failed to write to client socket, "
                                  "client rejected." );
                    nextConnection->error = true;
                    nextConnection->errorCauseString =
                        "Socket write failed";

                    }
                else {
                    // ready to start normal message exchange
                    // with client
                            
                    AppLog::info( "Got new player logged in" );
                            
                    delete nextConnection->ticketServerRequest;
                    nextConnection->ticketServerRequest = NULL;

                    delete [] nextConnection->sequenceNumberString;
                    nextConnection->sequenceNumberString = NULL;
                            
                    bool removeConnectionFromList = true;
                    
                    if( nextConnection->twinCode != NULL
                        && 
                        nextConnection->twinCount > 0 ) {
                        // Failed connection due to famTarget will be notified elsewhere
                        // we can remove their connection from the list here
                        processWaitingTwinConnection( *nextConnection );
                        }
                    else {
                        if( nextConnection->twinCode != NULL ) {
                            delete [] nextConnection->twinCode;
                            nextConnection->twinCode = NULL;
                            }
                                
                        int newID = processLoggedInPlayer( 
                            true,
                            nextConnection->sock,
                            nextConnection->sockBuffer,
                            nextConnection->email,
                            nextConnection,
                            nextConnection->tutorialNumber,
                            nextConnection->curseStatus,
                            nextConnection->lifeStats,
                            nextConnection->fitnessScore );
                            
                        if( newID == -2 ) {
                            nextConnection->error = true;
                            nextConnection->errorCauseString =
                                "Target family is not found or does not have fertiles";
                            // Do not remove this connection
                            // we need to notify them about the famTarget failure
                            removeConnectionFromList = false;
                            }
                        }
                                                        
                    if( removeConnectionFromList ) {
                        newConnections.deleteElement( i );
                        i--;
                        }
                    }
                }
            else if( nextConnection->ticketServerRequest == NULL ) {

                double timeDelta = Time::getCurrentTime() -
                    nextConnection->connectionStartTimeSeconds;
                

                

                char result = 
                    readSocketFull( nextConnection->sock,
                                    nextConnection->sockBuffer );
                
                if( ! result ) {
                    AppLog::info( "Failed to read from client socket, "
                                  "client rejected." );
                    nextConnection->error = true;
                    
                    // force connection close right away
                    // don't send REJECTED message and wait
                    nextConnection->rejectedSendTime = 1;
                    
                    nextConnection->errorCauseString =
                        "Socket read failed";
                    }
                
                char *message = NULL;
                int timeLimit = 10;
                
                if( ! nextConnection->shutdownMode ) {
                    message = 
                        getNextClientMessage( nextConnection->sockBuffer );
                    }
                else {
                    timeLimit = 5;
                    }
                
                if( message != NULL ) {
                    char passedSecret = false;
                    if(!nextConnection->playerListSent) {
                        if( (playerListSecret == NULL) && 0 == strcmp( message, "PLAYER_LIST" ) ) {
                            passedSecret = true;
                            } 
                        else if( playerListSecret != NULL ) {
                            char *requestWithSecret = autoSprintf("PLAYER_LIST %s", playerListSecret);
                            if(0 == constant_time_strcmp( message, requestWithSecret )) { // TODO: using plain password is not good, it would be better to expect the client to hash his password and we check it with by hashing our copy of the password. if the client can use a hashing library i will leave it to the reviewer to decide.
                                passedSecret = true;
                                }
                            delete[] requestWithSecret;
                            }
                        }
                    if(passedSecret || nextConnection->playerListSent) {
                        // request for player list https://github.com/twohoursonelife/OneLife/issues/202
                        if( !nextConnection->playerListSent ) {
                            HostAddress *a = nextConnection->sock->getRemoteHostAddress();
                            char address[100];
                            if( a == NULL ) {    
                                sprintf(address, "%s", "unknown");
                                }
                            else {
                                snprintf(address, 99, "%s:%d", a->mAddressString, a->mPort );
                                delete a;
                                }
                            AppLog::infoF( "Got PLAYER_LIST request from address: %s", address );
                            int numLive = 0;
                            for( int i=0; i<players.size(); i++ ) {
                                LiveObject *player = players.getElement( i );
                                if( ! player->error ) {
                                    numLive += 1;
                                    }
                                }
                            int buffSize = 32 * 1024; // NOTE: this can fit 800 players in the worst case, message will be truncated if more than that(and # wont be sent).
                            char messageBuff[buffSize];
                            messageBuff[0] = '\0';
                            sprintf(messageBuff, "%d\n", numLive);
                            // -2 here is for \0 and #
                            int remainingLen = buffSize - 2 - strlen(messageBuff);
                            float age;
                            char gender, *name, *familyName;
                            char finished = true;
                            char *playerLine;
                            for( int i = 0; i < players.size(); i++ ) {
                                LiveObject *player = players.getElement( i );
                                if( player->error ) {
                                    continue;
                                }
                                gender = getFemale( player ) ? 'F' : 'M';
                                age = (float) computeAge( player->lifeStartTimeSeconds );
                                if(player->name == NULL) {
                                    // on linux NULL is printed as "(null)" but i belive on windows it is treated as NULL character (empty), here we standaradize it to empty string.
                                    name = (char*)"";
                                    }
                                else {
                                    name = player->name;
                                    }
                                if(player->familyName == NULL) {
                                    familyName = (char*)"";
                                    }
                                else {
                                    familyName = player->familyName;
                                    }
                                playerLine = autoSprintf("%d,%d,%d,%c,%.1f,%d,%d,%s,%s\n",
                                                        player->id, player->lineageEveID, player->parentID,
                                                        gender, age, player->declaredInfertile, player->isTutorial,
                                                        name, familyName);
                                int playerLineLen = strlen(playerLine);
                                if(playerLineLen + 2 > remainingLen) {
                                    delete[] playerLine;
                                    finished = false;
                                    break;
                                    }
                                strncat(messageBuff, playerLine, playerLineLen);
                                remainingLen -= playerLineLen;
                                delete[] playerLine;
                                }
                            if(finished) {
                                strncat(messageBuff, "#", 2);
                                }
                            nextConnection->sock->send( (unsigned char*)messageBuff, strlen( messageBuff ), false, false);
                            nextConnection->playerListSent = true;
                            AppLog::infoF("PLAYER_LIST response-message sent to: %s", address);
                            }
                        }
                    else if( !passedSecret && stringStartsWith( message, "PLAYER_LIST" ) ) {
                        HostAddress *a = nextConnection->sock->getRemoteHostAddress();
                        char address[100];
                        if( a == NULL ) {    
                            sprintf(address, "%s", "unknown");
                            }
                        else {
                            snprintf(address, 99, "%s:%d", a->mAddressString, a->mPort );
                            delete a;
                            }
                        AppLog::infoF( "Invalid secret for request PLAYER_LIST from address: %s", address );
                        nextConnection->error = true;
                        nextConnection->errorCauseString = "Bad secret for PLAYER_LIST message";
                    }
                    else if( strstr( message, "LOGIN" ) != NULL ) {
                        
                        SimpleVector<char *> *tokens =
                            tokenizeString( message );
                        
                        int playerMapD = MAP_D;
                        if( tokens->size() == 4 || tokens->size() == 5 ||
                            tokens->size() == 7 ) {
                            
                            // login format: [mMapD@]email[:familyCode|\|spawnCode]
                            nextConnection->email = 
                                stringDuplicate( 
                                    tokens->getElementDirect( 1 ) );
                            nextConnection->mMapD = playerMapD;

                            std::string mMapDAndEmail {tokens->getElementDirect( 1 )};
                            //printf("email: %s, ", tokens->getElementDirect( 1 ));
                            //printf("nextconnection->email: %s\n", nextConnection->email);

                            const char mMapDelim = '@';
                            const size_t mMapDelimPos = mMapDAndEmail.find( mMapDelim );
                            //printf("mMapDelimPos: %d\n", mMapDelimPos);
                            if ( mMapDelimPos != std::string::npos && mMapDelimPos <=3 ){
                                playerMapD = std::stoi( mMapDAndEmail.substr(0, mMapDelimPos) );
                                //printf("playerMapD: %d\n", playerMapD);
                                if (playerMapD > MAX_MAP_D) playerMapD = MAP_D;

                                std::string onlyEmail { mMapDAndEmail.substr( mMapDelimPos + 1 ) };
                                delete[] nextConnection->email;
                                nextConnection->email = stringDuplicate( onlyEmail.c_str() );
                                nextConnection->mMapD = playerMapD;
                                //printf("nMapD: %d\n", nextConnection->mMapD);
                            }
                            //printf("mMapD: %d\n", nextConnection->mMapD);
                            // If email contains string delimiter
                            // Set nextConnection's hashedSpawnSeed to hash of seed
                            // then cut off seed and set email to onlyEmail
                            const size_t minSeedLen = 1;
                            const char seedDelim = '|'; 

                            std::string emailAndSeed { nextConnection->email };
                            const size_t seedDelimPos = emailAndSeed.find( seedDelim );
                            if( seedDelimPos != std::string::npos ) {
                                

                                const size_t seedLen = emailAndSeed.length() - seedDelimPos;

                                if( seedLen > minSeedLen ) {
                                    // Get the substr from one after the seed delim
				    // std::string seed { emailAndSeed.substr( seedDelimPos + 1 ) };
				    seed = emailAndSeed.substr( seedDelimPos + 1 ) ;
                                    char *sSeed = SettingsManager::getStringSetting("seedPepper", "default pepper");
                                    std::string seedPepper { sSeed };
                                    nextConnection->spawnCode = stringDuplicate(seed.c_str()); 
                                    nextConnection->hashedSpawnSeed =
                                        fnv1aHash(seed, fnv1aHash(seedPepper));
                                    
                                    delete [] sSeed;
                                }

                                // Remove seed from email
                                if( seedDelimPos == 0) {
                                    // There was only a seed not email
                                    nextConnection->email = stringDuplicate( "blank_email" );
                                } else {
                                    std::string onlyEmail { emailAndSeed.substr( 0, seedDelimPos ) };

                                    delete[] nextConnection->email;
                                    nextConnection->email = stringDuplicate( onlyEmail.c_str() );
                                }
                            } else {
                                nextConnection->hashedSpawnSeed = 0;
                                
                                // Check for famTarget as well only if seed isn't present in email
                                const char famTargetDelim = ':';


                                std::string emailAndFamTarget { nextConnection->email };

                                const size_t famTargetDelimPos = emailAndFamTarget.find( famTargetDelim );

                                if( famTargetDelimPos != std::string::npos ) {

                                    // Get the substr from one after the famTarget delim
                                    std::string famTarget { emailAndFamTarget.substr( famTargetDelimPos + 1 ) };

                                    nextConnection->famTarget =
                                        stringDuplicate( famTarget.c_str() );

                                    // Remove famTarget from email
                                    if( famTargetDelimPos == 0 ) {
                                        // There was only a famTarget not email
                                        nextConnection->email = stringDuplicate( "blank_email" );
                                    } else {
                                        std::string onlyEmail { emailAndFamTarget.substr( 0, famTargetDelimPos ) };

                                        delete[] nextConnection->email;
                                        nextConnection->email = stringDuplicate( onlyEmail.c_str() );
                                    }
                                } else {
                                    nextConnection->famTarget = NULL;
                                }
                            }

                            char *pwHash = tokens->getElementDirect( 2 );
                            char *keyHash = tokens->getElementDirect( 3 );
                            
                            if( tokens->size() >= 5 ) {
                                sscanf( tokens->getElementDirect( 4 ),
                                        "%d", 
                                        &( nextConnection->tutorialNumber ) );
                                }
                            
                            if( tokens->size() == 7 ) {
                                nextConnection->twinCode =
                                    stringDuplicate( 
                                        tokens->getElementDirect( 5 ) );
                                
                                sscanf( tokens->getElementDirect( 6 ),
                                        "%d", 
                                        &( nextConnection->twinCount ) );

                                int maxCount = 
                                    SettingsManager::getIntSetting( 
                                        "maxTwinPartySize", 4 );
                                
                                if( nextConnection->twinCount > maxCount ) {
                                    nextConnection->twinCount = maxCount;
                                    }
                                }
                            

                            // this may return -1 if curse server
                            // request is pending
                            // we'll catch that case later above
                            nextConnection->curseStatus =
                                getCurseLevel( nextConnection->email );


                            nextConnection->lifeStats.lifeCount = -1;
                            nextConnection->lifeStats.lifeTotalSeconds = -1;
                            nextConnection->lifeStats.error = false;
                            
                            // this will leave them as -1 if request pending
                            // we'll catch that case later above
                            int statsResult = getPlayerLifeStats(
                                nextConnection->email,
                                &( nextConnection->
                                   lifeStats.lifeCount ),
                                &( nextConnection->
                                   lifeStats.lifeTotalSeconds ) );

                            if( statsResult == -1 ) {
                                // error
                                // it's done now!
                                nextConnection->lifeStats.lifeCount = 0;
                                nextConnection->lifeStats.lifeTotalSeconds = 0;
                                nextConnection->lifeStats.error = true;
                                }
                                


                            if( requireClientPassword &&
                                ! nextConnection->error  ) {

                                char *trueHash = 
                                    hmac_sha1( 
                                        clientPassword, 
                                        nextConnection->sequenceNumberString );
                                

                                if( strcmp( trueHash, pwHash ) != 0 ) {
                                    AppLog::info( "Client password hmac bad, "
                                                  "client rejected." );
                                    nextConnection->error = true;
                                    nextConnection->errorCauseString =
                                        "Password check failed";
                                    }

                                delete [] trueHash;
                                }
                            
                            if( requireTicketServerCheck &&
                                ! nextConnection->error ) {
                                
                                char *encodedEmail =
                                    URLUtils::urlEncode( 
                                        nextConnection->email );

                                char *url = autoSprintf( 
                                    "%s?action=check_ticket_hash"
                                    "&email=%s"
                                    "&hash_value=%s"
                                    "&string_to_hash=%s",
                                    ticketServerURL,
                                    encodedEmail,
                                    keyHash,
                                    nextConnection->sequenceNumberString );

                                delete [] encodedEmail;

                                nextConnection->ticketServerRequest =
                                    new WebRequest( "GET", url, NULL );
                                nextConnection->ticketServerAccepted = false;

                                nextConnection->ticketServerRequestStartTime
                                    = currentTime;

                                delete [] url;
                                }
                            else if( !requireTicketServerCheck &&
                                     !nextConnection->error ) {
                                
                                // let them in without checking
                                
                                const char *message = "ACCEPTED\n#";
                                int messageLength = strlen( message );
                
                                int numSent = 
                                    nextConnection->sock->send( 
                                        (unsigned char*)message, 
                                        messageLength, 
                                        false, false );
                        

                                if( numSent != messageLength ) {
                                    AppLog::info( 
                                        "Failed to send on client socket, "
                                        "client rejected." );
                                    nextConnection->error = true;
                                    nextConnection->errorCauseString =
                                        "Socket write failed";
                                    }
                                else {
                                    // ready to start normal message exchange
                                    // with client
                            
				    if (nextConnection->spawnCode == NULL){
					    seed = "";
				    }
				    else {
					    seed = nextConnection->spawnCode;
				    }
				    HostAddress* a = nextConnection->sock->getRemoteHostAddress();
                                    AppLog::info( "Got new player %s (IP:%s, Seed:%s) logged in",
					       nextConnection->email, a->mAddressString, seed);
                                    
                                    delete nextConnection->ticketServerRequest;
                                    nextConnection->ticketServerRequest = NULL;
                                    
                                    delete [] 
                                        nextConnection->sequenceNumberString;
                                    nextConnection->sequenceNumberString = NULL;


                                    bool removeConnectionFromList = true;
                                    
                                    if( nextConnection->twinCode != NULL
                                        && 
                                        nextConnection->twinCount > 0 ) {
                                        // Failed connection due to famTarget will be notified elsewhere
                                        // we can remove their connection from the list here                        
                                        processWaitingTwinConnection(
                                            *nextConnection );
                                        }
                                    else {
                                        if( nextConnection->twinCode != NULL ) {
                                            delete [] nextConnection->twinCode;
                                            nextConnection->twinCode = NULL;
                                            }
                                        int newID = processLoggedInPlayer( 
                                            true,
                                            nextConnection->sock,
                                            nextConnection->sockBuffer,
                                            nextConnection->email,
                                            nextConnection,
                                            nextConnection->tutorialNumber,
                                            nextConnection->curseStatus,
                                            nextConnection->lifeStats,
                                            nextConnection->fitnessScore );
                                            
                                        if( newID == -2 ) {
                                            nextConnection->error = true;
                                            nextConnection->errorCauseString =
                                                "Target family is not found or does not have fertiles";
                                            // Do not remove this connection
                                            // we need to notify them about the famTarget failure
                                            removeConnectionFromList = false;
                                            }
                                        }
                                                                        
                                    if( removeConnectionFromList ) {
                                        newConnections.deleteElement( i );
                                        i--;
                                        }
                                    }
                                }
                            }
                        else {
                            AppLog::info( "LOGIN message has wrong format, "
                                          "client rejected." );
                            nextConnection->error = true;
                            nextConnection->errorCauseString =
                                "Bad login message";
                            }


                        tokens->deallocateStringElements();
                        delete tokens;
                        }
                    else {
                        AppLog::info( "Client's first message not LOGIN, "
                                      "client rejected." );
                        nextConnection->error = true;
                        nextConnection->errorCauseString =
                            "Unexpected first message";
                        }
                    
                    delete [] message;
                    }
                else if(nextConnection->playerListSent) {
                    int timeToClose = playerListSecret != NULL ? 10 : 4; // give more time if it is private.
                    if(currentTime - nextConnection->connectionStartTimeSeconds > timeToClose) {
                        HostAddress *a = nextConnection->sock->getRemoteHostAddress();
                        char address[100];
                        if( a == NULL ) {    
                            sprintf(address, "%s", "unknown");
                            }
                        else {
                            snprintf(address, 99, "%s:%d", a->mAddressString, a->mPort );
                            delete a;
                            }
                        AppLog::infoF("Closing socket of %s for PLAYER_LIST request after %d seconds", address, timeToClose);
                        deleteMembers( nextConnection );
                        newConnections.deleteElement(i);
                        i--;
                        }
                    }
                else if( timeDelta > timeLimit ) {
                    if( nextConnection->shutdownMode ) {
                        AppLog::info( "5 second grace period for new "
                                      "connection in shutdown mode, closing." );
                        }
                    else {
                        AppLog::info( 
                            "Client failed to LOGIN after 10 seconds, "
                            "client rejected." );
                        }
                    nextConnection->error = true;
                    nextConnection->errorCauseString =
                        "Login timeout";
                    }
                }
            }
            


        // make sure all twin-waiting sockets are still connected
        for( int i=0; i<waitingForTwinConnections.size(); i++ ) {
            FreshConnection *nextConnection = 
                waitingForTwinConnections.getElement( i );
            
            char result = 
                readSocketFull( nextConnection->sock,
                                nextConnection->sockBuffer );
            
            if( ! result ) {
                AppLog::info( "Failed to read from twin-waiting client socket, "
                              "client rejected." );

                refundLifeToken( nextConnection->email );
                
                nextConnection->error = true;

                // force connection close right away
                // don't send REJECTED message and wait
                nextConnection->rejectedSendTime = 1;
                    
                nextConnection->errorCauseString =
                    "Socket read failed";
                }
            }
            
        

        // now clean up any new connections that have errors
        
        // FreshConnections are in two different lists
        // clean up errors in both
        currentTime = Time::getCurrentTime();
        
        SimpleVector<FreshConnection> *connectionLists[2] =
            { &newConnections, &waitingForTwinConnections };
        for( int c=0; c<2; c++ ) {
            SimpleVector<FreshConnection> *list = connectionLists[c];
        
            for( int i=0; i<list->size(); i++ ) {
            
                FreshConnection *nextConnection = list->getElement( i );
            
                if( nextConnection->error ) {
                
                    if( nextConnection->rejectedSendTime == 0 ) {
                        
                        // try sending REJECTED message at end
                        // give them 5 seconds to receive it before closing
                        // the connection
                        const char *message = "REJECTED\n#";
                        nextConnection->sock->send( (unsigned char*)message,
                                                    strlen( message ), 
                                                false, false );
                        nextConnection->rejectedSendTime = currentTime;
                        }
                    else if( currentTime - nextConnection->rejectedSendTime >
                             5 ) {
                        // 5 sec passed since REJECTED sent
                        
                        AppLog::infoF( "Closing new connection on error "
                                       "(cause: %s)",
                                       nextConnection->errorCauseString );
                        
                        if( nextConnection->sock != NULL ) {
                            sockPoll.removeSocket( nextConnection->sock );
                            }
                        
                        deleteMembers( nextConnection );
                        
                        list->deleteElement( i );
                        i--;
                        }
                    }
                }
            }
    

        // step tutorial map load for player at front of line
        
        // 5 ms
        double timeLimit = 0.005;
        
        for( int i=0; i<tutorialLoadingPlayers.size(); i++ ) {
            LiveObject *nextPlayer = tutorialLoadingPlayers.getElement( i );
            
            char moreLeft = loadTutorialStep( &( nextPlayer->tutorialLoad ),
                                              timeLimit );
            
            if( moreLeft ) {
                // only load one step from first in line
                break;
                }
            
            // first in line is done
            
            AppLog::infoF( "New player %s tutorial loaded after %u steps, "
                           "%f total sec (loadID = %u )",
                           nextPlayer->email,
                           nextPlayer->tutorialLoad.stepCount,
                           Time::getCurrentTime() - 
                           nextPlayer->tutorialLoad.startTime,
                           nextPlayer->tutorialLoad.uniqueLoadID );

            // remove it and any twins
            unsigned int uniqueID = nextPlayer->tutorialLoad.uniqueLoadID;
            

            players.push_back( *nextPlayer );

            tutorialLoadingPlayers.deleteElement( i );
            
            LiveObject *twinPlayer = NULL;
            
            if( i < tutorialLoadingPlayers.size() ) {
                twinPlayer = tutorialLoadingPlayers.getElement( i );
                }
            
            while( twinPlayer != NULL && 
                   twinPlayer->tutorialLoad.uniqueLoadID == uniqueID ) {
                
                AppLog::infoF( "Twin %s tutorial loaded too (loadID = %u )",
                               twinPlayer->email,
                               uniqueID );
            
                players.push_back( *twinPlayer );

                tutorialLoadingPlayers.deleteElement( i );
                
                twinPlayer = NULL;
                
                if( i < tutorialLoadingPlayers.size() ) {
                    twinPlayer = tutorialLoadingPlayers.getElement( i );
                    }
                }
            break;
            
            }
        


        
    
        someClientMessageReceived = false;

        numLive = players.size();
        

        // listen for any messages from clients 

        // track index of each player that needs an update sent about it
        // we compose the full update message below
        SimpleVector<int> playerIndicesToSendUpdatesAbout;
        
        SimpleVector<int> playerIndicesToSendLineageAbout;

        SimpleVector<int> playerIndicesToSendCursesAbout;

        SimpleVector<int> playerIndicesToSendNamesAbout;

        SimpleVector<int> playerIndicesToSendDyingAbout;

        SimpleVector<int> playerIndicesToSendHealingAbout;


        SimpleVector<GridPos> newOwnerPos;

        newOwnerPos.push_back_other( &recentlyRemovedOwnerPos );
        recentlyRemovedOwnerPos.deleteAll();


        SimpleVector<UpdateRecord> newUpdates;
        SimpleVector<ChangePosition> newUpdatesPos;
        SimpleVector<int> newUpdatePlayerIDs;

        SimpleVector<int> newFlipPlayerIDs;
        SimpleVector<int> newFlipFacingLeft;
        SimpleVector<GridPos> newFlipPositions;


        // these are global, so they're not tagged with positions for
        // spatial filtering
        SimpleVector<UpdateRecord> newDeleteUpdates;
        

        SimpleVector<MapChangeRecord> mapChanges;
        SimpleVector<ChangePosition> mapChangesPos;
        

        SimpleVector<FlightDest> newFlightDest;
        


        
        timeSec_t curLookTime = Time::getCurrentTime();
        
        for( int i=0; i<numLive; i++ ) {
            LiveObject *nextPlayer = players.getElement( i );
            
            nextPlayer->updateSent = false;

            if( nextPlayer->error ) {
                continue;
                }            

            
            if( nextPlayer->fitnessScore == -1 ) {
                // see if result ready yet    
                int fitResult = 
                    getFitnessScore( nextPlayer->email, 
                                     &nextPlayer->fitnessScore );

                if( fitResult == -1 ) {
                    // failed
                    // stop asking now
                    nextPlayer->fitnessScore = 0;
                    }
                }
            

            double curCrossTime = Time::getCurrentTime();

            char checkCrossing = true;
            
            if( curCrossTime < nextPlayer->playerCrossingCheckTime +
                playerCrossingCheckStepTime ) {
                // player not due for another check yet
                checkCrossing = false;
                }
            else {
                // time for next check for this player
                nextPlayer->playerCrossingCheckTime = curCrossTime;
                checkCrossing = true;
                }
            
            
            if( checkCrossing ) {
                GridPos curPos = { nextPlayer->xd, nextPlayer->yd };
            
                if( nextPlayer->xd != nextPlayer->xs ||
                    nextPlayer->yd != nextPlayer->ys ) {
                
                    curPos = computePartialMoveSpot( nextPlayer );
                    }
            
                int curOverID = getMapObject( curPos.x, curPos.y );
                
                char riding = false;
                
                if( nextPlayer->holdingID > 0 && 
                    getObject( nextPlayer->holdingID )->rideable ) {
                    riding = true;
                    }


                GridPos deadlyDestPos = curPos;

                if( ! riding ) {
                    // check if player is standing on
                    // a non-deadly object OR on nothing
                    // if so, moving deadly objects might still be able
                    // to get them
                    ObjectRecord *curOverObj = NULL;
                    
                    if( curOverID > 0 ) {
                        curOverObj = getObject( curOverID );
                        }
                    
                    if( curOverObj == NULL ||
                        ! curOverObj->permanent ||
                        curOverObj->deadlyDistance == 0 ) {
                        
                        int movingDestX, movingDestY;
                        
                        int curMovingID =
                            getDeadlyMovingMapObject( 
                                curPos.x, curPos.y,
                                &movingDestX, &movingDestY );
                        
                        if( curMovingID != 0 ) {
                            
                            // make sure that dest object hasn't changed
                            // since moving record was created
                            // (if a bear is shot mid-move, for example,
                            //  the movement record will still show the unshot
                            //  bear)
                            curMovingID = getMapObject( movingDestX,
                                                        movingDestY );
                            
                            ObjectRecord *movingObj = getObject( curMovingID );
                            if( movingObj->permanent &&
                                movingObj->deadlyDistance > 0 ) {
                                curOverID = curMovingID;
                                
                                deadlyDestPos.x = movingDestX;
                                deadlyDestPos.y = movingDestY;
                                }
                            }
                        }
                    }
                    


                if( ! nextPlayer->heldByOther &&
                    ! nextPlayer->vogMode &&
                    curOverID != 0 && 
                    ! isMapObjectInTransit( curPos.x, curPos.y ) &&
                    ! wasRecentlyDeadly( curPos ) ) {
                
                    ObjectRecord *curOverObj = getObject( curOverID );
                

                    if( !riding &&
                        curOverObj->permanent && 
                        curOverObj->deadlyDistance > 0 ) {
                    
                        char wasSick = false;
                                        
                        if( nextPlayer->holdingID > 0 &&
                            strstr(
                                getObject( nextPlayer->holdingID )->
                                description,
                                "sick" ) != NULL ) {
                            wasSick = true;
                            }


                        addDeadlyMapSpot( curPos );
                    
                        setDeathReason( nextPlayer, 
                                        "killed",
                                        curOverID );
                    
                        nextPlayer->deathSourceID = curOverID;
                    
                        if( curOverObj->isUseDummy ) {
                            nextPlayer->deathSourceID = 
                                curOverObj->useDummyParent;
                            }

                        nextPlayer->errorCauseString =
                            "Player killed by permanent object";
                    
                        if( ! nextPlayer->dying || wasSick ) {
                            // if was sick, they had a long stagger
                            // time set, so cutting it in half makes no sense
                        
                            int staggerTime = 
                                SettingsManager::getIntSetting(
                                    "deathStaggerTime", 20 );
                        
                            double currentTime = 
                                Time::getCurrentTime();
                        
                            nextPlayer->dying = true;
                            nextPlayer->dyingETA = 
                                currentTime + staggerTime;

                            playerIndicesToSendDyingAbout.
                                push_back( 
                                    getLiveObjectIndex( 
                                        nextPlayer->id ) );
                            }
                        else {
                            // already dying, and getting attacked again
                        
                            // halve their remaining stagger time
                            double currentTime = 
                                Time::getCurrentTime();
                        
                            double staggerTimeLeft = 
                                nextPlayer->dyingETA - currentTime;
                        
                            if( staggerTimeLeft > 0 ) {
                                staggerTimeLeft /= 2;
                                nextPlayer->dyingETA = 
                                    currentTime + staggerTimeLeft;
                                }
                            }
                
                    
                        // generic on-person
                        TransRecord *r = 
                            getPTrans( curOverID, 0 );

                        if( r != NULL ) {
                            setMapObject( deadlyDestPos.x, deadlyDestPos.y, 
                                          r->newActor );

                            // new target specifies wound
                            // but never replace an existing wound
                            // death time is shortened above
                            // however, wounds can replace sickness 
                            if( r->newTarget > 0 &&
                                ( ! nextPlayer->holdingWound || wasSick ) ) {
                                // don't drop their wound
                                if( nextPlayer->holdingID != 0 &&
                                    ! nextPlayer->holdingWound ) {
                                    handleDrop( 
                                        curPos.x, curPos.y, 
                                        nextPlayer,
                                        &playerIndicesToSendUpdatesAbout );
                                    }
                                nextPlayer->holdingID = 
                                    r->newTarget;
                                holdingSomethingNew( nextPlayer );
                            
                                setFreshEtaDecayForHeld( nextPlayer );
                            
                                checkSickStaggerTime( nextPlayer );
                                
                                
                                nextPlayer->holdingWound = true;
                            
                                ForcedEffects e = 
                                    checkForForcedEffects( 
                                        nextPlayer->holdingID );
                            
                                if( e.emotIndex != -1 ) {
                                    nextPlayer->emotFrozen = true;
                                    nextPlayer->emotFrozenIndex = e.emotIndex;
                                    
                                    newEmotPlayerIDs.push_back( 
                                        nextPlayer->id );
                                    newEmotIndices.push_back( e.emotIndex );
                                    newEmotTTLs.push_back( e.ttlSec );
                                    interruptAnyKillEmots( nextPlayer->id,
                                                           e.ttlSec );
                                    }
                                if( e.foodModifierSet && 
                                    e.foodCapModifier != 1 ) {
                                
                                    nextPlayer->foodCapModifier = 
                                        e.foodCapModifier;
                                    nextPlayer->foodUpdate = true;
                                    }
                                if( e.feverSet ) {
                                    nextPlayer->fever = e.fever;
                                    }
                            

                                playerIndicesToSendUpdatesAbout.
                                    push_back( 
                                        getLiveObjectIndex( 
                                            nextPlayer->id ) );
                                }
                            }
                        }
                    else if( riding && 
                             curOverObj->permanent && 
                             curOverObj->deadlyDistance > 0 ) {
                        // rode over something deadly
                        // see if it affects what we're riding

                        TransRecord *r = 
                            getPTrans( nextPlayer->holdingID, curOverID );
                    
                        if( r != NULL ) {
                            handleHoldingChange( nextPlayer,
                                                 r->newActor );
                            nextPlayer->heldTransitionSourceID = curOverID;
                            playerIndicesToSendUpdatesAbout.push_back( i );

                            setMapObject( curPos.x, curPos.y, r->newTarget );

                            // it attacked their vehicle 
                            // put it on cooldown so it won't immediately
                            // attack them
                            addDeadlyMapSpot( curPos );
                            }
                        }                
                    }
                }
            
            
            if( curLookTime - nextPlayer->lastRegionLookTime > 5 ) {
                lookAtRegion( nextPlayer->xd - 8, nextPlayer->yd - 7,
                              nextPlayer->xd + 8, nextPlayer->yd + 7 );
                nextPlayer->lastRegionLookTime = curLookTime;
                }
                
            if( curLookTime - nextPlayer->lastWrittenObjectScanTime > 1 ) {
                
                //2HOL mechanics to read written objects
                GridPos playerPos;
                if( nextPlayer->xs == nextPlayer->xd && nextPlayer->ys == nextPlayer->yd ) {
                    playerPos.x = nextPlayer->xd;
                    playerPos.y = nextPlayer->yd;
                } else {
                    playerPos = computePartialMoveSpot( nextPlayer );
                }
                
                if( !equal( playerPos, nextPlayer->lastWrittenObjectScanPos ) ) {
                    
                    nextPlayer->lastWrittenObjectScanPos = playerPos;
                    nextPlayer->lastWrittenObjectScanTime = curLookTime;
                
                    float readRange = 3.0;
                    
                    //Remove positions already read when players get out of range and speech bubbles are expired 
                    for( int j = nextPlayer->readPositions.size() - 1; j >= 0; j-- ) {
                        GridPos p = nextPlayer->readPositions.getElementDirect( j );
                        double eta = nextPlayer->readPositionsETA.getElementDirect( j );
                        if( 
                            distance( p, playerPos ) > readRange && 
                            Time::getCurrentTime() > eta
                            ) {
                            nextPlayer->readPositions.deleteElement( j );
                            nextPlayer->readPositionsETA.deleteElement( j );
                        }
                    }
                    
                    //Scan area around players for pass-to-read objects
                    for( int dx = -3; dx <= 3; dx++ ) {
                        for( int dy = -3; dy <= 3; dy++ ) {
                            float dist = sqrt(dx * dx + dy * dy);
                            if( dist > readRange ) continue;
                            int objId = getMapObjectRaw( playerPos.x + dx, playerPos.y + dy );
                            if( objId <= 0 ) continue;
                            ObjectRecord *obj = getObject( objId );
                            if( obj != NULL && obj->written && obj->passToRead ) {
                                GridPos readPos = { playerPos.x + dx, playerPos.y + dy };
                                forceObjectToRead( nextPlayer, objId, readPos, true );
                            }
                        }
                    }
                }
            }

            char *message = NULL;
            
            if( nextPlayer->connected ) {    
                char result = 
                    readSocketFull( nextPlayer->sock, nextPlayer->sockBuffer );
            
                if( ! result ) {
                    setPlayerDisconnected( nextPlayer, "Socket read failed",__func__, __LINE__ );
                    }
                else {
                    // don't even bother parsing message buffer for players
                    // that are not currently connected
                    message = getNextClientMessage( nextPlayer->sockBuffer );
                    }
                }
            
            
            if( message != NULL ) {
                someClientMessageReceived = true;
                
                AppLog::infoF( "Got client message from %d: %s",
                              nextPlayer->id, message );
                
                ClientMessage m = parseMessage( nextPlayer, message );
                
                delete [] message;
                
                
                //2HOL: Player not AFK
                //Skipping EMOT because modded player sends EMOT automatically
                if( m.type != EMOT ) {
                    //Clear afk emote if they were afk
                    if( nextPlayer->isAFK ) {
                        if( clearFrozenEmote( nextPlayer, afkEmotionIndex ) ) {
                            //Only change state when afk emote is successfully cleared
                            nextPlayer->isAFK = false;
                            nextPlayer->lastActionTime = Time::getCurrentTime();
                            }
                        }
                    else {                    
                        nextPlayer->isAFK = false;
                        nextPlayer->lastActionTime = Time::getCurrentTime();
                        }
                    }
                

                //Thread::staticSleep( 
                //    testRandSource.getRandomBoundedInt( 0, 450 ) );
                
                // GOTO below jumps here if we need to reparse the message
                // as a different type
                RESTART_MESSAGE_ACTION:
                if( m.type == UNKNOWN ) {
                    AppLog::info( "Client error, unknown message type." );
                    //setPlayerDisconnected( nextPlayer,
                    //                       "Unknown message type" );
                    // do not disconnect client here
                    // keep server flexible, so client can be updated
                    // with a protocol change before the server gets updated
                    }
                else if( m.type == BUG ) {
                    int allow = 
                        SettingsManager::getIntSetting( "allowBugReports", 0 );

                    if( allow ) {
                        char *bugName = 
                            autoSprintf( "bug_%d_%d_%f",
                                         m.bug,
                                         nextPlayer->id,
                                         Time::getCurrentTime() );
                        char *bugInfoName = autoSprintf( "%s_info.txt",
                                                         bugName );
                        char *bugOutName = autoSprintf( "%s_out.txt",
                                                        bugName );
                        FILE *bugInfo = fopen( bugInfoName, "w" );
                        if( bugInfo != NULL ) {
                            fprintf( bugInfo, 
                                     "Bug report from player %d\n"
                                     "Bug text:  %s\n", 
                                     nextPlayer->id,
                                     m.bugText );
                            fclose( bugInfo );
                            
                            File outFile( NULL, "serverOut.txt" );
                            if( outFile.exists() ) {
                                fflush( stdout );
                                File outCopyFile( NULL, bugOutName );
                                
                                outFile.copy( &outCopyFile );
                                }
                            }
                        delete [] bugName;
                        delete [] bugInfoName;
                        delete [] bugOutName;
                        }
                    }
                else if( m.type == MAP ) {
                    
                    int allow = 
                        SettingsManager::getIntSetting( "allowMapRequests", 0 );
                    

                    if( allow ) {
                        
                        SimpleVector<char *> *list = 
                            SettingsManager::getSetting( 
                                "mapRequestAllowAccounts" );
                        
                        allow = false;
                        
                        for( int i=0; i<list->size(); i++ ) {
                            if( strcmp( nextPlayer->email,
                                        list->getElementDirect( i ) ) == 0 ) {
                                
                                allow = true;
                                break;
                                }
                            }
                        
                        list->deallocateStringElements();
                        delete list;
                        }
                    

                    if( allow && nextPlayer->connected ) {
                        
                        // keep them full of food so they don't 
                        // die of hunger during the pull
                        nextPlayer->foodStore = 
                            computeFoodCapacity( nextPlayer );
                        

                        int length;

                        // map chunks sent back to client absolute
                        // relative to center instead of birth pos
                        GridPos centerPos = { 0, 0 };
                        int chunkDimensionX = nextPlayer->mMapD / 2;
                        int chunkDimensionY =chunkDimensionX - 2;
                        //printf("mx_startx: %d, mx_starty: %d, cx: %d, cy:%d\n",
                            //m.x - chunkDimensionX / 2, m.y - chunkDimensionY / 2, chunkDimensionX, chunkDimensionY);
                        unsigned char *mapChunkMessage = 
                            getChunkMessage( m.x - chunkDimensionX / 2, 
                                             m.y - chunkDimensionY / 2,
                                             chunkDimensionX,
                                             chunkDimensionY,
                                             centerPos,
                                             &length );
                        
                        int numSent = 
                            nextPlayer->sock->send( mapChunkMessage, 
                                                    length, 
                                                    false, false );
                        
                        nextPlayer->gotPartOfThisFrame = true;
                        
                        delete [] mapChunkMessage;

                        if( numSent != length ) {
                            setPlayerDisconnected( nextPlayer, 
                                                   "Socket write failed" , __func__ , __LINE__);
                            }
                        }
                    else {
                        AppLog::infoF( "Map pull request rejected for %s", 
                                       nextPlayer->email );
                        }
                    }
                else if( m.type == TRIGGER ) {
                    if( areTriggersEnabled() ) {
                        trigger( m.trigger );
                        }
                    }
                else if( m.type == VOGS ) {
                    int allow = 
                        SettingsManager::getIntSetting( "allowVOGMode", 0 );

                    if( allow ) {
                        
                        SimpleVector<char *> *list = 
                            SettingsManager::getSetting( 
                                "vogAllowAccounts" );
                        
                        char *lowerEmail = stringToLowerCase(nextPlayer->email);

                        allow = false;
                        
                        for( int i=0; i<list->size(); i++ ) {
                            char *lowerAllowed = stringToLowerCase(list->getElementDirect( i ));
                            if( strcmp( lowerEmail,
                                        lowerAllowed ) == 0 ) {

                                allow = true;
                                delete [] lowerAllowed;
                                break;
                                }
                            else if( strcmp(
                                         "*",
                                         list->getElementDirect( i ) ) == 0 ) {
                                // wildcard present in settings file
                                allow = true;
                                delete [] lowerAllowed;
                                break;
                                }
                            delete [] lowerAllowed;
                            }
                        
                        list->deallocateStringElements();
                        delete list;
                        delete [] lowerEmail;
                        }
                    

                    if( allow && nextPlayer->connected ) {
                        nextPlayer->vogMode = true;
                        nextPlayer->preVogPos = getPlayerPos( nextPlayer );
                        nextPlayer->preVogBirthPos = nextPlayer->birthPos;
                        nextPlayer->vogJumpIndex = 0;
                        }
                    }
                else if( m.type == VOGN ) {
                    if( nextPlayer->vogMode &&
                        players.size() > 1 ) {
                        
                        nextPlayer->vogJumpIndex++;
                        if( nextPlayer->vogJumpIndex == i ) {
                            nextPlayer->vogJumpIndex++;
                            }
                        if( nextPlayer->vogJumpIndex >= players.size() ) {
                            nextPlayer->vogJumpIndex = 0;
                            }
                        if( nextPlayer->vogJumpIndex == i ) {
                            nextPlayer->vogJumpIndex++;
                            }
                        
                        LiveObject *otherPlayer = 
                            players.getElement( 
                                nextPlayer->vogJumpIndex );
                        
                        GridPos o = getPlayerPos( otherPlayer );
                        
                        GridPos oldPos = getPlayerPos( nextPlayer );
                        

                        nextPlayer->xd = o.x;
                        nextPlayer->yd = o.y;

                        nextPlayer->xs = o.x;
                        nextPlayer->ys = o.y;

                        if( distance( oldPos, o ) > 10000 ) {
                            nextPlayer->birthPos = o;
                            }

                        char *message = autoSprintf( "VU\n%d %d\n#",
                                                     nextPlayer->xs - 
                                                     nextPlayer->birthPos.x,
                                                     nextPlayer->ys -
                                                     nextPlayer->birthPos.y );
                        sendMessageToPlayer( nextPlayer, message,
                                             strlen( message ) );
                        
                        delete [] message;

                        nextPlayer->firstMessageSent = false;
                        nextPlayer->firstMapSent = false;
                        }
                    }
                else if( m.type == VOGP ) {
                    if( nextPlayer->vogMode &&
                        players.size() > 1 ) {

                        nextPlayer->vogJumpIndex--;

                        // if several people have died since last VOGP
                        // sent by this player, their vogJumpIndex can
                        // be out of bounds
                        if( nextPlayer->vogJumpIndex >= players.size() ) {
                            nextPlayer->vogJumpIndex = players.size() - 1;
                            }

                        if( nextPlayer->vogJumpIndex == i ) {
                            nextPlayer->vogJumpIndex--;
                            }
                        if( nextPlayer->vogJumpIndex < 0 ) {
                            nextPlayer->vogJumpIndex = players.size() - 1;
                            }
                        if( nextPlayer->vogJumpIndex == i ) {
                            nextPlayer->vogJumpIndex--;
                            }

                        LiveObject *otherPlayer = 
                            players.getElement( 
                                nextPlayer->vogJumpIndex );
                        
                        GridPos o = getPlayerPos( otherPlayer );
                        
                        GridPos oldPos = getPlayerPos( nextPlayer );
                        

                        nextPlayer->xd = o.x;
                        nextPlayer->yd = o.y;

                        nextPlayer->xs = o.x;
                        nextPlayer->ys = o.y;
                        
                        if( distance( oldPos, o ) > 10000 ) {
                            nextPlayer->birthPos = o;
                            }
                        
                        char *message = autoSprintf( "VU\n%d %d\n#",
                                                     nextPlayer->xs - 
                                                     nextPlayer->birthPos.x,
                                                     nextPlayer->ys -
                                                     nextPlayer->birthPos.y );
                        sendMessageToPlayer( nextPlayer, message,
                                             strlen( message ) );
                        
                        delete [] message;

                        nextPlayer->firstMessageSent = false;
                        nextPlayer->firstMapSent = false;
                        }
                    }
                else if( m.type == VOGM ) {
                    if( nextPlayer->vogMode ) {
                        nextPlayer->xd = m.x;
                        nextPlayer->yd = m.y;
                        
                        nextPlayer->xs = m.x;
                        nextPlayer->ys = m.y;
                        
                        char *message = autoSprintf( "VU\n%d %d\n#",
                                                     nextPlayer->xs - 
                                                     nextPlayer->birthPos.x,
                                                     nextPlayer->ys -
                                                     nextPlayer->birthPos.y );
                        sendMessageToPlayer( nextPlayer, message,
                                             strlen( message ) );
                        
                        delete [] message;
                        }
                    }
                else if( m.type == VOGI ) {
                    if( nextPlayer->vogMode ) {
                        if( m.id > 0 &&
                            getObject( m.id ) != NULL ) {
                            
                            if( getObject( m.id )->floor ) {
                                setMapFloor( m.x, m.y, m.id );
                                }
                            else {
                                setMapObject( m.x, m.y, m.id );
                                }
                            }
                        }
                    }
                else if( m.type == VOGT && m.saidText != NULL ) {
                    if( nextPlayer->vogMode ) {
                        
                        newLocationSpeech.push_back( 
                            stringDuplicate( m.saidText ) );
                        GridPos p = getPlayerPos( nextPlayer );
                        
                        ChangePosition cp;
                        cp.x = p.x;
                        cp.y = p.y;
                        cp.global = false;
                        cp.responsiblePlayerID = -1;

                        newLocationSpeechPos.push_back( cp );
                        }
                    }
                else if( m.type == VOGX ) {
                    if( nextPlayer->vogMode ) {
                        nextPlayer->vogMode = false;
                        
                        // If they send VOGX with coords other than (0, 0), teleport them
                        if( m.x - nextPlayer->birthPos.x == 0 && m.y - nextPlayer->birthPos.y == 0 ) {
                            GridPos p = nextPlayer->preVogPos;
                            
                            nextPlayer->xd = p.x;
                            nextPlayer->yd = p.y;
                            
                            nextPlayer->xs = p.x;
                            nextPlayer->ys = p.y;
                            
                            nextPlayer->birthPos = nextPlayer->preVogBirthPos;
                            }

                        // send them one last VU message to move them 
                        // back instantly
                        char *message = autoSprintf( "VU\n%d %d\n#",
                                                     nextPlayer->xs - 
                                                     nextPlayer->birthPos.x,
                                                     nextPlayer->ys -
                                                     nextPlayer->birthPos.y );
                        sendMessageToPlayer( nextPlayer, message,
                                             strlen( message ) );
                        
                        delete [] message;
                        
                        nextPlayer->postVogMode = true;
                        nextPlayer->firstMessageSent = false;
                        nextPlayer->firstMapSent = false;
                        }
                    }
                else if( nextPlayer->vogMode ) {
                    // ignore non-VOG messages from them
                    }
                else if( m.type == FORCE ) {
                    if( m.x == nextPlayer->xd &&
                        m.y == nextPlayer->yd ) {
                        
                        // player has ack'ed their forced pos correctly
                        
                        // stop ignoring their messages now
                        nextPlayer->waitingForForceResponse = false;
                        }
                    else {
                        AppLog::infoF( 
                            "FORCE message has unexpected "
                            "absolute pos (%d,%d), expecting (%d,%d)",
                            m.x, m.y,
                            nextPlayer->xd, nextPlayer->yd );
                        }
                    }
                else if( m.type == PING ) {
                    // immediately send pong
                    char *message = autoSprintf( "PONG\n%d#", m.id );

                    sendMessageToPlayer( nextPlayer, message, 
                                         strlen( message ) );
                    delete [] message;
                    }
                else if( m.type == DIE ) {
                    if( computeAge( nextPlayer ) < 2 ) {
                        
                        // killed self
                        // SID triggers a lineage ban
                        nextPlayer->suicide = true;

                        setDeathReason( nextPlayer, "SID" );

                        nextPlayer->error = true;
                        nextPlayer->errorCauseString = "Baby suicide";
                        int parentID = nextPlayer->parentID;
                        
                        LiveObject *parentO = 
                            getLiveObject( parentID );
                        
                        if( parentO != NULL && nextPlayer->everHeldByParent ) {
                            // mother picked up this SID baby at least
                            // one time
                            // mother can have another baby right away
                            parentO->birthCoolDown = 0;
                            }

                        if( parentO != NULL &&
                            parentO->lastSidsBabyEmail != NULL ) {
                            delete [] parentO->lastSidsBabyEmail;
                            parentO->lastSidsBabyEmail = NULL;
                            }
                        
                        // walk through all other players and clear THIS
                        // player from their SIDS mememory
                        // we only track the most recent parent who had this
                        // baby SIDS
                        for( int p=0; p<players.size(); p++ ) {
                            LiveObject *parent = players.getElement( p );
                            
                            if( parent->lastSidsBabyEmail != NULL &&
                                strcmp( parent->lastSidsBabyEmail,
                                        nextPlayer->email ) == 0 ) {
                                delete [] parent->lastSidsBabyEmail;
                                parent->lastSidsBabyEmail = NULL;
                                }
                            }
                        
                        if( parentO != NULL ) {
                            parentO->lastSidsBabyEmail = 
                                stringDuplicate( nextPlayer->email );
                            }
                        
                        int holdingAdultID = nextPlayer->heldByOtherID;

                        LiveObject *adult = NULL;
                        if( nextPlayer->heldByOther ) {
                            adult = getLiveObject( holdingAdultID );
                            }

                        int babyBonesID = 
                            SettingsManager::getIntSetting( 
                                "babyBones", -1 );
                        
                        if( adult != NULL ) {
                            
                            if( babyBonesID != -1 ) {
                                ObjectRecord *babyBonesO = 
                                    getObject( babyBonesID );
                                
                                if( babyBonesO != NULL ) {
                                    
                                    // don't leave grave on ground just yet
                                    nextPlayer->customGraveID = 0;
                            
                                    GridPos adultPos = 
                                        getPlayerPos( adult );

                                    // put invisible grave there for now
                                    // find an empty spot for this grave
                                    // where there's no grave already
                                    GridPos gravePos = adultPos;
                                    
                                    // give up after 100 steps
                                    // huge graveyard around?
                                    int stepCount = 0;
                                    while( getGravePlayerID( 
                                               gravePos.x, 
                                               gravePos.y ) > 0 &&
                                           stepCount < 100 ) {
                                        gravePos.x ++;
                                        stepCount ++;
                                        }
                                    
                                    GraveInfo graveInfo = 
                                        { gravePos, 
                                          nextPlayer->id,
                                          nextPlayer->lineageEveID };
                                    newGraves.push_back( graveInfo );
                                    
                                    setGravePlayerID(
                                        gravePos.x, gravePos.y,
                                        nextPlayer->id );
                                    
                                    
                                    playerIndicesToSendUpdatesAbout.push_back(
                                        getLiveObjectIndex( holdingAdultID ) );
                                    
                                    // what if baby wearing clothes?
                                    for( int c=0; 
                                         c < NUM_CLOTHING_PIECES; 
                                         c++ ) {
                                             
                                        ObjectRecord *cObj = clothingByIndex(
                                            nextPlayer->clothing, c );
                                        
                                        if( cObj != NULL ) {
                                            // put clothing in adult's hand
                                            // and then drop
                                            adult->holdingID = cObj->id;
                                            if( nextPlayer->
                                                clothingContained[c].
                                                size() > 0 ) {
                                                
                                                adult->numContained =
                                                    nextPlayer->
                                                    clothingContained[c].
                                                    size();
                                                
                                                adult->containedIDs =
                                                    nextPlayer->
                                                    clothingContained[c].
                                                    getElementArray();
                                                adult->containedEtaDecays =
                                                    nextPlayer->
                                                    clothingContainedEtaDecays
                                                    [c].
                                                    getElementArray();
                                                
                                                adult->subContainedIDs
                                                    = new 
                                                    SimpleVector<int>[
                                                    adult->numContained ];
                                                adult->subContainedEtaDecays
                                                    = new 
                                                    SimpleVector<timeSec_t>[
                                                    adult->numContained ];
                                                }
                                            
                                            handleDrop( 
                                                adultPos.x, adultPos.y, 
                                                adult,
                                                NULL );
                                            }
                                        }
                                    
                                    // finally leave baby bones
                                    // in their hands
                                    adult->holdingID = babyBonesID;
                                    
                                    setHeldGraveOrigin( adult, 
                                                        gravePos.x,
                                                        gravePos.y,
                                                        0 );


                                    // this works to force client to play
                                    // creation sound for baby bones.
                                    adult->heldTransitionSourceID = 
                                        nextPlayer->displayID;
                                    
                                    nextPlayer->heldByOther = false;
                                    }
                                }
                            }
                        else {
                            
                            int babyBonesGroundID = 
                                SettingsManager::getIntSetting( 
                                    "babyBonesGround", -1 );
                            
                            if( babyBonesGroundID != -1 ) {
                                nextPlayer->customGraveID = babyBonesGroundID;
                                }
                            else if( babyBonesID != -1 ) {
                                // else figure out what the held baby bones
                                // become when dropped on ground
                                TransRecord *groundTrans =
                                    getPTrans( babyBonesID, -1 );
                                
                                if( groundTrans != NULL &&
                                    groundTrans->newTarget > 0 ) {
                                    
                                    nextPlayer->customGraveID = 
                                        groundTrans->newTarget;
                                    }
                                }
                            // else just use standard grave
                            }
                        }
                    else {
                        // adult /DIE
                        int holdingID = nextPlayer->holdingID;
                        if( holdingID < 0 ) holdingID = 0; // negative ID means a baby
                        // if player was wounded or sick before commiting suicide
                        // that should be the reason of death instead
                        if( !nextPlayer->deathSourceID ) {
                            setDeathReason( nextPlayer, "suicide", holdingID );
                            }

                        nextPlayer->error = true;
                        nextPlayer->errorCauseString = "Suicide";
                        }
                    }
                else if( m.type == GRAVE ) {
                    // immediately send GO response
                    
                    int id = getGravePlayerID( m.x, m.y );
                    
                    DeadObject *o = NULL;
                    for( int i=0; i<pastPlayers.size(); i++ ) {
                        DeadObject *oThis = pastPlayers.getElement( i );
                        
                        if( oThis->id == id ) {
                            o = oThis;
                            break;
                            }
                        }
                    
                    SimpleVector<int> *defaultLineage = 
                        new SimpleVector<int>();
                    
                    defaultLineage->push_back( 0 );
                    DeadObject defaultO = 
                        { 0,
                          0,
                          stringDuplicate( "~" ),
                          defaultLineage,
                          0,
                          0 };
                    
                    if( o == NULL ) {
                        // check for living player too 
                        for( int i=0; i<players.size(); i++ ) {
                            LiveObject *oThis = players.getElement( i );
                            
                            if( oThis->id == id ) {
                                defaultO.id = oThis->id;
                                defaultO.displayID = oThis->displayID;
                            
                                if( oThis->name != NULL ) {
                                    delete [] defaultO.name;
                                    defaultO.name = 
                                        stringDuplicate( oThis->name );
                                    }
                            
                                defaultO.lineage->push_back_other( 
                                    oThis->lineage );
                            
                                defaultO.lineageEveID = oThis->lineageEveID;
                                defaultO.lifeStartTimeSeconds =
                                    oThis->lifeStartTimeSeconds;
                                defaultO.deathTimeSeconds =
                                    oThis->deathTimeSeconds;
                                }
                            }
                        }
                    

                    if( o == NULL ) {
                        o = &defaultO;
                        }

                    if( o != NULL ) {
                        char *formattedName;
                        
                        if( o->name != NULL ) {
                            char found;
                            formattedName =
                                replaceAll( o->name, " ", "_", &found );
                            }
                        else {
                            formattedName = stringDuplicate( "~" );
                            }

                        SimpleVector<char> linWorking;
                        
                        for( int j=0; j<o->lineage->size(); j++ ) {
                            char *mID = 
                                autoSprintf( 
                                    " %d",
                                    o->lineage->getElementDirect( j ) );
                            linWorking.appendElementString( mID );
                            delete [] mID;
                            }
                        char *linString = linWorking.getElementString();
                        
                        double age;
                        
                        if( o->deathTimeSeconds > 0 ) {
                            // "age" in years since they died 
                            age = computeAge( o->deathTimeSeconds );
                            }
                        else {
                            // grave of unknown person
                            // let client know that age is bogus
                            age = -1;
                            }
                        
                        char *message = autoSprintf(
                            "GO\n%d %d %d %d %lf %s%s eve=%d\n#",
                            m.x - nextPlayer->birthPos.x,
                            m.y - nextPlayer->birthPos.y,
                            o->id, o->displayID, 
                            age,
                            formattedName, linString,
                            o->lineageEveID );
                        printf( "Processing %d,%d from birth pos %d,%d\n",
                                m.x, m.y, nextPlayer->birthPos.x,
                                nextPlayer->birthPos.y );
                        
                        delete [] formattedName;
                        delete [] linString;

                        sendMessageToPlayer( nextPlayer, message, 
                                             strlen( message ) );
                        delete [] message;
                        }
                    
                    delete [] defaultO.name;
                    delete defaultO.lineage;
                    }
                else if( m.type == OWNER ) {
                    // immediately send OW response
                    SimpleVector<char> messageWorking;
                    messageWorking.appendElementString( "OW\n" );
                    
                    char *leadString = 
                        autoSprintf( "%d %d", 
                                     m.x - nextPlayer->birthPos.x, 
                                     m.y - nextPlayer->birthPos.y );
                    messageWorking.appendElementString( leadString );
                    delete [] leadString;
                    
                    char *ownerString = getOwnershipString( m.x, m.y );
                    messageWorking.appendElementString( ownerString );
                    delete [] ownerString;

                    messageWorking.appendElementString( "\n#" );
                    char *message = messageWorking.getElementString();
                    
                    sendMessageToPlayer( nextPlayer, message, 
                                         strlen( message ) );
                    delete [] message;

                    GridPos p = { m.x, m.y };
                    
                    if( ! isKnownOwned( nextPlayer, p ) ) {
                        // remember that we know about it
                        nextPlayer->knownOwnedPositions.push_back( p );
                        }
                    }
                else if( m.type == PHOTO ) {
                    // immediately send photo response

                    char *photoServerSharedSecret = 
                        SettingsManager::
                        getStringSetting( "photoServerSharedSecret",
                                          "secret_phrase" );
                    
                    char *idString = autoSprintf( "%d", m.id );
                    
                    char *hash;
                    
                    // is a photo device present at x and y?
                    char photo = false;
                    
                    int oID = getMapObject( m.x, m.y );
                    
                    if( oID > 0 ) {
                        if( strstr( getObject( oID )->description,
                                    "+photo" ) != NULL ) {
                            photo = true;
                            }
                        }
                    
                    if( ! photo ) {
                        hash = hmac_sha1( "dummy", idString );
                        }
                    else {
                        hash = hmac_sha1( photoServerSharedSecret, idString );
                        }
                    
                    delete [] photoServerSharedSecret;
                    delete [] idString;
                    
                    char *message = autoSprintf( "PH\n%d %d %s#", 
                                                 m.x, m.y, hash );
                    
                    delete [] hash;

                    sendMessageToPlayer( nextPlayer, message, 
                                         strlen( message ) );
                    delete [] message;
                    }
                else if( m.type == FLIP ) {
                    
                    if( currentTime - nextPlayer->lastFlipTime > 1.75 ) {
                        // client should send at most one flip ever 2 seconds
                        // allow some wiggle room
                        GridPos p = getPlayerPos( nextPlayer );
                        
                        int oldFacingLeft = nextPlayer->facingLeft;
                        
                        if( m.x > p.x ) {
                            nextPlayer-> facingLeft = 0;
                            }
                        else if( m.x < p.x ) {
                            nextPlayer->facingLeft = 1;
                            }
                        
                        if( oldFacingLeft != nextPlayer->facingLeft ) {
                            nextPlayer->lastFlipTime = currentTime;
                            newFlipPlayerIDs.push_back( nextPlayer->id );
                            newFlipFacingLeft.push_back( 
                                nextPlayer->facingLeft );
                            newFlipPositions.push_back( p );
                            }
                        }
                    }
                else if( m.type != SAY && m.type != EMOT &&
                         nextPlayer->waitingForForceResponse ) {
                    // if we're waiting for a FORCE response, ignore
                    // all messages from player except SAY and EMOT
                    
                    AppLog::infoF( "Ignoring client message because we're "
                                   "waiting for FORCE ack message after a "
                                   "forced-pos PU at (%d, %d), "
                                   "relative=(%d, %d)",
                                   nextPlayer->xd, nextPlayer->yd,
                                   nextPlayer->xd - nextPlayer->birthPos.x,
                                   nextPlayer->yd - nextPlayer->birthPos.y );
                    }
                // if player is still moving (or held by an adult), 
                // ignore all actions
                // except for move interrupts
                else if( ( nextPlayer->xs == nextPlayer->xd &&
                           nextPlayer->ys == nextPlayer->yd &&
                           ! nextPlayer->heldByOther )
                         ||
                         m.type == MOVE ||
                         m.type == JUMP || 
                         m.type == SAY ||
                         m.type == EMOT ) {

                    if( m.type == MOVE &&
                        m.sequenceNumber != -1 ) {
                        nextPlayer->lastMoveSequenceNumber = m.sequenceNumber;
                        }

                    if( ( m.type == MOVE || m.type == JUMP ) && 
                        nextPlayer->heldByOther ) {
                        
                        // only JUMP actually makes them jump out
                        if( m.type == JUMP ) {
                            // baby wiggling out of parent's arms
                            
                            // block them from wiggling from their own 
                            // mother's arms if they are under 1
                            
                            if( computeAge( nextPlayer ) >= 1  ||
                                nextPlayer->heldByOtherID != 
                                nextPlayer->parentID ) {
                                
                                handleForcedBabyDrop( 
                                    nextPlayer,
                                    &playerIndicesToSendUpdatesAbout );
                                }
                            }
                        
                        // ignore their move requests while
                        // in-arms, until they JUMP out
                        }
                    else if( m.type == MOVE && nextPlayer->holdingID > 0 &&
                             getObject( nextPlayer->holdingID )->
                             speedMult == 0 ) {
                        // next player holding something that prevents
                        // movement entirely
                        printf( "  Processing move, "
                                "but player holding a speed-0 object, "
                                "ending now\n" );
                        nextPlayer->xd = nextPlayer->xs;
                        nextPlayer->yd = nextPlayer->ys;
                        
                        nextPlayer->posForced = true;
                        
                        // send update about them to end the move
                        // right now
                        playerIndicesToSendUpdatesAbout.push_back( i );
                        }
                    else if( m.type == MOVE ) {
                        //Thread::staticSleep( 1000 );

                        /*
                        printf( "  Processing move, "
                                "we think player at old start pos %d,%d\n",
                                nextPlayer->xs,
                                nextPlayer->ys );
                        printf( "  Player's last path = " );
                        for( int p=0; p<nextPlayer->pathLength; p++ ) {
                            printf( "(%d, %d) ",
                                    nextPlayer->pathToDest[p].x, 
                                    nextPlayer->pathToDest[p].y );
                            }
                        printf( "\n" );
                        */
                        
                        char interrupt = false;
                        char pathPrefixAdded = false;
                        
                        // first, construct a path from any existing
                        // path PLUS path that player is suggesting
                        SimpleVector<GridPos> unfilteredPath;

                        if( nextPlayer->xs != m.x ||
                            nextPlayer->ys != m.y ) {
                            
                            // start pos of their submitted path
                            // donesn't match where we think they are

                            // it could be an interrupt to past move
                            // OR, if our server sees move as done but client 
                            // doesn't yet, they may be sending a move
                            // from the middle of their last path.

                            // treat this like an interrupt to last move
                            // in both cases.

                            // a new move interrupting a non-stationary object
                            interrupt = true;

                            // where we think they are along last move path
                            GridPos cPos;
                            
                            if( nextPlayer->xs != nextPlayer->xd 
                                ||
                                nextPlayer->ys != nextPlayer->yd ) {
                                
                                // a real interrupt to a move that is
                                // still in-progress on server
                                cPos = computePartialMoveSpot( nextPlayer );
                                }
                            else {
                                // we think their last path is done
                                cPos.x = nextPlayer->xs;
                                cPos.y = nextPlayer->ys;
                                }
                            
                            /*
                            printf( "   we think player in motion or "
                                    "done moving at %d,%d\n",
                                    cPos.x,
                                    cPos.y );
                            */
                            nextPlayer->xs = cPos.x;
                            nextPlayer->ys = cPos.y;
                            
                            
                            char cOnTheirNewPath = false;
                            

                            for( int p=0; p<m.numExtraPos; p++ ) {
                                if( equal( cPos, m.extraPos[p] ) ) {
                                    cOnTheirNewPath = true;
                                    break;
                                    }
                                }
                            
                            if( cPos.x == m.x && cPos.y == m.y ) {
                                // also if equal to their start pos
                                cOnTheirNewPath = true;
                                }
                            


                            if( !cOnTheirNewPath &&
                                nextPlayer->pathLength > 0 ) {

                                // add prefix to their path from
                                // c to the start of their path
                                
                                // index where they think they are

                                // could be ahead or behind where we think
                                // they are
                                
                                int theirPathIndex = -1;
                            
                                for( int p=0; p<nextPlayer->pathLength; p++ ) {
                                    GridPos pos = nextPlayer->pathToDest[p];
                                    
                                    if( m.x == pos.x && m.y == pos.y ) {
                                        // reached point along old path
                                        // where player thinks they 
                                        // actually are
                                        theirPathIndex = p;
                                        break;
                                        }
                                    }
                                
                                char theirIndexNotFound = false;
                                
                                if( theirPathIndex == -1 ) {
                                    // if not found, assume they think they
                                    // are at start of their old path
                                    
                                    theirIndexNotFound = true;
                                    theirPathIndex = 0;
                                    }
                                
                                /*
                                printf( "They are on our path at index %d\n",
                                        theirPathIndex );
                                */

                                // okay, they think they are on last path
                                // that we had for them

                                // step through path from where WE
                                // think they should be to where they
                                // think they are and add this as a prefix
                                // to the path they submitted
                                // (we may walk backward along the old
                                //  path to do this)
                                
                                int c = computePartialMovePathStep( 
                                    nextPlayer );
                                    
                                // -1 means starting, pre-path 
                                // pos is closest
                                // but okay to leave c at -1, because
                                // we will add pathStep=1 to it

                                int pathStep = 0;
                                    
                                if( theirPathIndex < c ) {
                                    pathStep = -1;
                                    }
                                else if( theirPathIndex > c ) {
                                    pathStep = 1;
                                    }
                                    
                                if( pathStep != 0 ) {

                                    if( c == -1 ) {
                                        // fix weird case where our start
                                        // pos is on our path
                                        // not sure what causes this
                                        // but it causes the valid path
                                        // check to fail below
                                        int firstStep = c + pathStep;
                                        GridPos firstPos =
                                            nextPlayer->pathToDest[ firstStep ];
                                        
                                        if( firstPos.x == nextPlayer->xs &&
                                            firstPos.y == nextPlayer->ys ) {
                                            c = 0;
                                            }
                                        }
                                    
                                    for( int p = c + pathStep; 
                                         p != theirPathIndex + pathStep; 
                                         p += pathStep ) {
                                        GridPos pos = 
                                            nextPlayer->pathToDest[p];
                                            
                                        unfilteredPath.push_back( pos );
                                        }
                                    }

                                if( theirIndexNotFound ) {
                                    // add their path's starting pos
                                    // at the end of the prefix
                                    GridPos pos = { m.x, m.y };
                                    
                                    unfilteredPath.push_back( pos );
                                    }
                                
                                // otherwise, they are where we think
                                // they are, and we don't need to prefix
                                // their path

                                /*
                                printf( "Prefixing their path "
                                        "with %d steps\n",
                                        unfilteredPath.size() );
                                */
                                }
                            }
                        
                        if( unfilteredPath.size() > 0 ) {
                            pathPrefixAdded = true;
                            }

                        // now add path player says they want to go down

                        for( int p=0; p < m.numExtraPos; p++ ) {
                            unfilteredPath.push_back( m.extraPos[p] );
                            }
                        
                        /*
                        printf( "Unfiltered path = " );
                        for( int p=0; p<unfilteredPath.size(); p++ ) {
                            printf( "(%d, %d) ",
                                    unfilteredPath.getElementDirect(p).x, 
                                    unfilteredPath.getElementDirect(p).y );
                            }
                        printf( "\n" );
                        */

                        // remove any duplicate spots due to doubling back

                        for( int p=1; p<unfilteredPath.size(); p++ ) {
                            
                            if( equal( unfilteredPath.getElementDirect(p-1),
                                       unfilteredPath.getElementDirect(p) ) ) {
                                unfilteredPath.deleteElement( p );
                                p--;
                                //printf( "FOUND duplicate path element\n" );
                                }
                            }
                            
                                
                                       
                        
                        nextPlayer->xd = m.extraPos[ m.numExtraPos - 1].x;
                        nextPlayer->yd = m.extraPos[ m.numExtraPos - 1].y;
                        

                        if( distance( nextPlayer->lastPlayerUpdateAbsolutePos,
                                      m.extraPos[ m.numExtraPos - 1] ) 
                            > 
                            getMaxChunkDimension() / 2 ) {
                            // they have moved a long way since their
                            // last PU was sent
                            // Send one now, mid-move
                            
                            playerIndicesToSendUpdatesAbout.push_back( i );
                            }
                        

                        
                        if( nextPlayer->xd == nextPlayer->xs &&
                            nextPlayer->yd == nextPlayer->ys ) {
                            // this move request truncates to where
                            // we think player actually is

                            // send update to terminate move right now
                            playerIndicesToSendUpdatesAbout.push_back( i );
                            /*
                            printf( "A move that takes player "
                                    "where they already are, "
                                    "ending move now\n" );
                            */
                            }
                        else {
                            // an actual move away from current xs,ys

                            if( interrupt ) {
                                //printf( "Got valid move interrupt\n" );
                                }
                                

                            // check path for obstacles
                            // and make sure it contains the location
                            // where we think they are
                            
                            char truncated = 0;
                            
                            SimpleVector<GridPos> validPath;

                            char startFound = false;
                            
                            
                            int startIndex = 0;
                            // search from end first to find last occurrence
                            // of start pos
                            for( int p=unfilteredPath.size() - 1; p>=0; p-- ) {
                                
                                if( unfilteredPath.getElementDirect(p).x 
                                      == nextPlayer->xs
                                    &&
                                    unfilteredPath.getElementDirect(p).y 
                                      == nextPlayer->ys ) {
                                    
                                    startFound = true;
                                    startIndex = p;
                                    break;
                                    }
                                }
                            /*
                            printf( "Start index = %d (startFound = %d)\n", 
                                    startIndex, startFound );
                            */

                            if( ! startFound &&
                                ! isGridAdjacentDiag( 
                                    unfilteredPath.
                                      getElementDirect(startIndex).x,
                                    unfilteredPath.
                                      getElementDirect(startIndex).y,
                                    nextPlayer->xs,
                                    nextPlayer->ys ) ) {
                                // path start jumps away from current player 
                                // start
                                // ignore it
                                }
                            else {
                                
                                GridPos lastValidPathStep =
                                    { m.x, m.y };
                                
                                if( pathPrefixAdded ) {
                                    lastValidPathStep.x = nextPlayer->xs;
                                    lastValidPathStep.y = nextPlayer->ys;
                                    }
                                
                                // we know where we think start
                                // of this path should be,
                                // but player may be behind this point
                                // on path (if we get their message late)
                                // So, it's not safe to pre-truncate
                                // the path

                                // However, we will adjust timing, below,
                                // to match where we think they should be
                                
                                // enforce client behavior of not walking
                                // down through objects in our cell that are
                                // blocking us
                                char currentBlocked = false;
                                
                                if( isMapSpotBlocking( lastValidPathStep.x,
                                                       lastValidPathStep.y ) ) {
                                    currentBlocked = true;
                                    }
                                

                                for( int p=0; 
                                     p<unfilteredPath.size(); p++ ) {
                                
                                    GridPos pos = 
                                        unfilteredPath.getElementDirect(p);

                                    if( isMapSpotBlocking( pos.x, pos.y ) ) {
                                        // blockage in middle of path
                                        // terminate path here
                                        truncated = 1;
                                        break;
                                        }
                                    
                                    if( currentBlocked && p == 0 &&
                                        pos.y == lastValidPathStep.y - 1 ) {
                                        // attempt to walk down through
                                        // blocking object at starting location
                                        truncated = 1;
                                        break;
                                        }
                                    

                                    // make sure it's not more
                                    // than one step beyond
                                    // last step

                                    if( ! isGridAdjacentDiag( 
                                            pos, lastValidPathStep ) ) {
                                        // a path with a break in it
                                        // terminate it here
                                        truncated = 1;
                                        break;
                                        }
                                    
                                    // no blockage, no gaps, add this step
                                    validPath.push_back( pos );
                                    lastValidPathStep = pos;
                                    }
                                }
                            
                            if( validPath.size() == 0 ) {
                                // path not permitted
                                AppLog::info( "Path submitted by player "
                                              "not valid, "
                                              "ending move now" );
                                //assert( false );
                                nextPlayer->xd = nextPlayer->xs;
                                nextPlayer->yd = nextPlayer->ys;
                                
                                nextPlayer->posForced = true;

                                // send update about them to end the move
                                // right now
                                playerIndicesToSendUpdatesAbout.push_back( i );
                                }
                            else {
                                // a good path
                                
                                if( nextPlayer->pathToDest != NULL ) {
                                    delete [] nextPlayer->pathToDest;
                                    nextPlayer->pathToDest = NULL;
                                    }

                                nextPlayer->pathTruncated = truncated;
                                
                                nextPlayer->pathLength = validPath.size();
                                
                                nextPlayer->pathToDest = 
                                    validPath.getElementArray();
                                    
                                // path may be truncated from what was 
                                // requested, so set new d
                                nextPlayer->xd = 
                                    nextPlayer->pathToDest[ 
                                        nextPlayer->pathLength - 1 ].x;
                                nextPlayer->yd = 
                                    nextPlayer->pathToDest[ 
                                        nextPlayer->pathLength - 1 ].y;

                                // distance is number of orthogonal steps
                            
                                double dist = 
                                    measurePathLength( nextPlayer->xs,
                                                       nextPlayer->ys,
                                                       nextPlayer->pathToDest,
                                                       nextPlayer->pathLength );
 
                                double distAlreadyDone =
                                    measurePathLength( nextPlayer->xs,
                                                       nextPlayer->ys,
                                                       nextPlayer->pathToDest,
                                                       startIndex );
                             
                                double moveSpeed = computeMoveSpeed( 
                                    nextPlayer ) *
                                    getPathSpeedModifier( 
                                        nextPlayer->pathToDest,
                                        nextPlayer->pathLength );
                                
                                nextPlayer->moveTotalSeconds = dist / 
                                    moveSpeed;
                           
                                double secondsAlreadyDone = distAlreadyDone / 
                                    moveSpeed;
                                /*
                                printf( "Skipping %f seconds along new %f-"
                                        "second path\n",
                                        secondsAlreadyDone, 
                                        nextPlayer->moveTotalSeconds );
                                */
                                nextPlayer->moveStartTime = 
                                    Time::getCurrentTime() - 
                                    secondsAlreadyDone;
                            
                                nextPlayer->newMove = true;
                                
                                
                                // check if path passes over
                                // an object with autoDefaultTrans
                                for( int p=0; p< nextPlayer->pathLength; p++ ) {
                                    int x = nextPlayer->pathToDest[p].x;
                                    int y = nextPlayer->pathToDest[p].y;
                                    
                                    int oID = getMapObject( x, y );
                                    
                                    if( oID > 0 &&
                                        getObject( oID )->autoDefaultTrans ) {
                                        TransRecord *t = getPTrans( -2, oID );
                                        
                                        if( t == NULL ) {
                                            // also consider applying bare-hand
                                            // action, if defined and if
                                            // it produces nothing in the hand
                                            t = getPTrans( 0, oID );
                                            
                                            if( t != NULL &&
                                                t->newActor > 0 ) {
                                                t = NULL;
                                                }
                                            }

                                        if( t != NULL && t->newTarget > 0 ) {
                                            int newTarg = t->newTarget;
                                            setMapObject( x, y, newTarg );

                                            TransRecord *timeT =
                                                getPTrans( -1, newTarg );
                                            
                                            if( timeT != NULL &&
                                                timeT->autoDecaySeconds < 20 ) {
                                                // target will decay to
                                                // something else in a short
                                                // time
                                                // Likely meant to reset
                                                // after person passes through
                                                
                                                // fix the time based on our
                                                // pass-through time
                                                double timeLeft =
                                                    nextPlayer->moveTotalSeconds
                                                    - secondsAlreadyDone;
                                                
                                                double plannedETADecay =
                                                    Time::getCurrentTime()
                                                    + timeLeft 
                                                    // pad with extra second
                                                    + 1;
                                                
                                                timeSec_t actual =
                                                    getEtaDecay( x, y );
                                                
                                                // don't ever shorten
                                                // we could be interrupting
                                                // another player who
                                                // is on a longer path
                                                // through the same object
                                                if( plannedETADecay >
                                                    actual ) {
                                                    setEtaDecay( 
                                                        x, y, plannedETADecay );
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    else if( m.type == SAY && m.saidText != NULL &&
                             Time::getCurrentTime() - 
                             nextPlayer->lastSayTimeSeconds > 
                             minSayGapInSeconds ) {
                        
                        nextPlayer->lastSayTimeSeconds = 
                            Time::getCurrentTime();

                        unsigned int sayLimit = getSayLimit( nextPlayer );
                        
                        if( strlen( m.saidText ) > sayLimit ) {
                            // truncate
                            m.saidText[ sayLimit ] = '\0';
                            }

                        int len = strlen( m.saidText );
                        
                        // replace not-allowed characters with spaces
			/*
                        for( int c=0; c<len; c++ ) {
                            if( ! allowedSayCharMap[ 
                                    (int)( m.saidText[c] ) ] ) {
                                
                                m.saidText[c] = ' ';
                                }
                            }
                        */ 
                        // now clean up gratuitous runs of spaces left behind
                        // by removed characters (or submitted by a wayward
                        // client)
                        SimpleVector<char *> *tokens = 
                            tokenizeString( m.saidText );

                        char *cleanedString;
                        if( tokens->size() > 0 ) {
                        
                            char **tokensArray = 
                                tokens->getElementArray();
                        
                            // join words with single spaces
                            cleanedString = join( tokensArray,
                                                  tokens->size(),
                                                  " " );
                        
                            tokens->deallocateStringElements();
                            delete [] tokensArray;
                            }
                        else {
                            cleanedString = stringDuplicate( "" );
                            }

                        delete tokens;
                        
                        delete [] m.saidText;
                        m.saidText = cleanedString;
                        
                        
                        if( nextPlayer->ownedPositions.size() > 0 ) {
                            // consider phrases that assign ownership
                            LiveObject *newOwnerPlayer = NULL;

                            char *namedOwner = isNamedGivingSay( m.saidText );
                            
                            if( namedOwner != NULL ) {
                                
                                for( int j=0; j<players.size(); j++ ) {
                                    LiveObject *otherPlayer = 
                                        players.getElement( j );
                                    if( ! otherPlayer->error &&
                                        otherPlayer != nextPlayer &&
                                        otherPlayer->name != NULL &&
                                        strcmp( otherPlayer->name, 
                                                namedOwner ) == 0 ) {
                                        
                                        newOwnerPlayer = otherPlayer;
                                        break;
                                        }
                                    }
                                delete [] namedOwner;
                                }
                            else if( isYouGivingSay( m.saidText ) ) {
                                // find closest other player
                                newOwnerPlayer = 
                                    getClosestOtherPlayer( nextPlayer );
                                }
                            
                            if( newOwnerPlayer != NULL ) {
                                // find closest spot that this player owns
                                GridPos thisPos = getPlayerPos( nextPlayer );

                                double minDist = DBL_MAX;
                            
                                GridPos closePos;
                            
                                for( int j=0; 
                                     j< nextPlayer->ownedPositions.size();
                                     j++ ) {
                                    GridPos nextPos = 
                                        nextPlayer->
                                        ownedPositions.getElementDirect( j );
                                    double d = distance( nextPos, thisPos );
                                
                                    if( d < minDist ) {
                                        minDist = d;
                                        closePos = nextPos;
                                        }
                                    }

                                if( minDist < DBL_MAX ) {
                                    // found one
                                    if( ! isOwned( newOwnerPlayer, 
                                                   closePos ) ) {
                                        newOwnerPlayer->
                                            ownedPositions.push_back( 
                                                closePos );
                                        newOwnerPos.push_back( closePos );
                                        }
                                    }
                                }
                            }


                        
                        if( nextPlayer->isEve && nextPlayer->name == NULL ) {
                            char *name = isFamilyNamingSay( m.saidText );
                            
                            if( name != NULL && strcmp( name, "" ) != 0 ) {
                                const char *close = findCloseLastName( name );
                                nextPlayer->name = autoSprintf( "%s %s",
                                                                eveName, 
                                                                close );

                                
                                nextPlayer->name = getUniqueCursableName( 
                                    nextPlayer->name, 
                                    &( nextPlayer->nameHasSuffix ),
                                    true );
                                
                                char firstName[99];
                                char lastName[99];
                                char suffix[99];

                                if( nextPlayer->nameHasSuffix ) {
                                    
                                    sscanf( nextPlayer->name, 
                                            "%s %s %s", 
                                            firstName, lastName, suffix );
                                    }
                                else {
                                    sscanf( nextPlayer->name, 
                                            "%s %s", 
                                            firstName, lastName );
                                    }
                                
                                nextPlayer->familyName = 
                                        stringDuplicate( lastName );


                                if( ! nextPlayer->isTutorial ) {    
                                    logName( nextPlayer->id,
                                             nextPlayer->email,
                                             nextPlayer->name,
                                             nextPlayer->lineageEveID );
                                    }
                                
                                if ( nextPlayer->displayedName != NULL ) delete [] nextPlayer->displayedName;
                                if ( nextPlayer->declaredInfertile ) {
                                    nextPlayer->displayedName = autoSprintf( "%s %s", nextPlayer->name, infertilitySuffix);
                                    } 
                                else {
                                    nextPlayer->displayedName = stringDuplicate( nextPlayer->name );
                                    }
                                
                                playerIndicesToSendNamesAbout.push_back( i );
                                }
                            }
                        
                        if( getFemale( nextPlayer ) ) {
                            char *infertilityDeclaring = isInfertilityDeclaringSay( m.saidText );
                            char *fertilityDeclaring = isFertilityDeclaringSay( m.saidText );
                            if( infertilityDeclaring != NULL && !nextPlayer->declaredInfertile ) {
                                nextPlayer->declaredInfertile = true;
                                
                                if ( nextPlayer->displayedName != NULL ) delete [] nextPlayer->displayedName;
                                if (nextPlayer->name == NULL) {
                                    nextPlayer->displayedName = stringDuplicate( infertilitySuffix );
                                } else {
                                    nextPlayer->displayedName = autoSprintf( "%s %s", nextPlayer->name, infertilitySuffix);
                                }
                                
                                playerIndicesToSendNamesAbout.push_back( i );
                                
                            } else if( fertilityDeclaring != NULL && nextPlayer->declaredInfertile ) {
                                nextPlayer->declaredInfertile = false;
                                
                                if ( nextPlayer->displayedName != NULL ) delete [] nextPlayer->displayedName;
                                if (nextPlayer->name == NULL) {
                                    nextPlayer->displayedName = stringDuplicate( fertilitySuffix );
                                } else {
                                    nextPlayer->displayedName = stringDuplicate( nextPlayer->name );
                                }
                                
                                playerIndicesToSendNamesAbout.push_back( i );
                            }
                        }
                        

                        
                        LiveObject *otherToForgive = NULL;
                        
                        if( isYouForgivingSay( m.saidText ) ) {
                            otherToForgive = 
                                getClosestOtherPlayer( nextPlayer );
                            }
                        else {
                            char *forgiveName = isNamedForgivingSay( m.saidText );
                            if( forgiveName != NULL ) {
                                otherToForgive =
                                    getPlayerByName( forgiveName, nextPlayer );
                                
                                }
                            }
                        
                        if( otherToForgive != NULL ) {
                            clearDBCurse( nextPlayer->id, 
                                          nextPlayer->email, 
                                          otherToForgive->email );
                            
                            char *message = 
                                autoSprintf( 
                                    "CU\n%d 0 %s_%s\n#", 
                                    otherToForgive->id,
                                    getCurseWord( nextPlayer->email,
                                                  otherToForgive->email, 0 ),
                                    getCurseWord( nextPlayer->email,
                                                  otherToForgive->email, 1 ) );
                            sendMessageToPlayer( nextPlayer,
                                                 message, strlen( message ) );
                            delete [] message;
                            }

                        if( nextPlayer->holdingID < 0 ) {

                            // we're holding a baby
                            // (no longer matters if it's our own baby)
                            // (we let adoptive parents name too)
                            
                            LiveObject *babyO =
                                getLiveObject( - nextPlayer->holdingID );
                            
                            if( babyO != NULL && babyO->name == NULL ) {
                                char *name = isBabyNamingSay( m.saidText );

                                if( name != NULL && strcmp( name, "" ) != 0 ) {
                                    nameBaby( nextPlayer, babyO, name,
                                              &playerIndicesToSendNamesAbout );
                                    
                                    if ( babyO->displayedName != NULL ) delete [] babyO->displayedName;
                                    if ( babyO->declaredInfertile ) {
                                        babyO->displayedName = autoSprintf( "%s %s", babyO->name, infertilitySuffix);
                                        } 
                                    else {
                                        babyO->displayedName = stringDuplicate( babyO->name );
                                        }
                                    }
                                }
                            }
                        else {
                            // not holding anyone
                        
                            char *name = isBabyNamingSay( m.saidText );
                                
                            if( name != NULL && strcmp( name, "" ) != 0 ) {
                                // still, check if we're naming a nearby,
                                // nameless non-baby

                                LiveObject *closestOther = 
                                    getClosestOtherPlayer( nextPlayer,
                                                           babyAge, true );

                                if( closestOther != NULL ) {
                                    nameBaby( nextPlayer, closestOther,
                                              name, 
                                              &playerIndicesToSendNamesAbout );
                                    
                                    if ( closestOther->displayedName != NULL ) delete [] closestOther->displayedName;
                                    if ( closestOther->declaredInfertile ) {
                                        closestOther->displayedName = autoSprintf( "%s %s", closestOther->name, infertilitySuffix);
                                        } 
                                    else {
                                        closestOther->displayedName = stringDuplicate( closestOther->name );
                                        }
                                    }
                                }

                            // also check if we're holding something writable
                            unsigned char metaData[ MAP_METADATA_LENGTH ];
                            int len = strlen( m.saidText );
                            
                            if( nextPlayer->holdingID > 0 &&
                                len < MAP_METADATA_LENGTH &&
                                getObject( 
                                    nextPlayer->holdingID )->writable &&
                                // and no metadata already on it
                                ! getMetadata( nextPlayer->holdingID, 
                                               metaData ) ) {

                                char *textToAdd = NULL;
                                

                                if( strstr( 
                                        getObject( nextPlayer->holdingID )->
                                        description,
                                        "+map" ) != NULL ) {
                                    // holding a potential map
                                    // add coordinates to where we're standing
                                    GridPos p = getPlayerPos( nextPlayer );
                                    
                                    char *mapStuff = autoSprintf( 
                                        " *map %d %d %.f",
                                        p.x, p.y, Time::timeSec() );
                                    
                                    int mapStuffLen = strlen( mapStuff );
                                    
                                    char *saidText = 
                                        stringDuplicate( m.saidText );
                                    
                                    int saidLen = strlen( saidText );
                                    
                                    int extra = saidLen + mapStuffLen
                                        - ( MAP_METADATA_LENGTH - 1 );

                                    if( extra > 0 ) {
                                        // too long to fit in metadata,
                                        // trim speech, not map data
                                        
                                        saidLen = saidLen - extra;
                                        
                                        // truncate
                                        saidText[ saidLen ] = '\0';
                                        }                                    
                                    
                                    textToAdd = autoSprintf( 
                                        "%s%s", saidText, mapStuff );
                                    
                                    delete [] saidText;
                                    delete [] mapStuff;
                                    }
                                else {
                                    textToAdd = stringDuplicate( m.saidText );
                                    }

                                int lenToAdd = strlen( textToAdd );
                                
                                // leave room for null char at end
                                if( lenToAdd > MAP_METADATA_LENGTH - 1 ) {
                                    lenToAdd = MAP_METADATA_LENGTH - 1;
                                    }

                                memset( metaData, 0, MAP_METADATA_LENGTH );
                                // this will leave 0 null character at end
                                // left over from memset of full length
                                memcpy( metaData, textToAdd, lenToAdd );
                                
                                delete [] textToAdd;

                                nextPlayer->holdingID = 
                                    addMetadata( nextPlayer->holdingID,
                                                 metaData );

                                TransRecord *writingHappenTrans =
                                    getMetaTrans( 0, nextPlayer->holdingID );
                                
                                if( writingHappenTrans != NULL &&
                                    writingHappenTrans->newTarget > 0 &&
                                    getObject( writingHappenTrans->newTarget )
                                        ->written ) {
                                    // bare hands transition going from
                                    // writable to written
                                    // use this to transform object in 
                                    // hands as we write
                                    handleHoldingChange( 
                                        nextPlayer,
                                        writingHappenTrans->newTarget );
                                    playerIndicesToSendUpdatesAbout.
                                        push_back( i );
                                    }                    
                                }
                            }
                        
                        // trim whitespace and make sure we're not
                        // adding an empty string
                        // empty or whitespace strings causes trouble
                        // elsewhere in code
                        char *cleanSay = trimWhitespace( m.saidText );
                        
                        if( strcmp( cleanSay, "" ) != 0 ) {
                            makePlayerSay( nextPlayer, cleanSay );
                            }
                        delete [] cleanSay;
                        }
                    else if( m.type == KILL ) {
                        playerIndicesToSendUpdatesAbout.push_back( i );
                        if( m.id > 0 && 
                            nextPlayer->holdingID > 0 ) {
                            
                            ObjectRecord *heldObj = 
                                getObject( nextPlayer->holdingID );
                            
                            
                            if( heldObj->deadlyDistance > 0 ) {
                            
                                // player transitioning into kill state?
                            
                                LiveObject *targetPlayer =
                                    getLiveObject( m.id );
                            
                                if( targetPlayer != NULL ) {
                                    
                                    // block intra-family kills with
                                    // otherFamilyOnly weapons
                                    char weaponBlocked = false;
                                    
                                    if( strstr( heldObj->description,
                                                "otherFamilyOnly" ) ) {
                                        // make sure victim is in
                                        // different family
                                        // AND that there's no peace treaty
                                        if( targetPlayer->lineageEveID ==
                                            nextPlayer->lineageEveID
                                            ||
                                            isPeaceTreaty( 
                                                targetPlayer->lineageEveID,
                                                nextPlayer->lineageEveID ) ) {
                                            
                                            weaponBlocked = true;
                                            }
                                        }
                                    
                                    if( ! weaponBlocked ) {
                                        removeAnyKillState( nextPlayer );
                                        
                                        char enteredState =
                                            addKillState( nextPlayer,
                                                          targetPlayer );
                                        
                                        if( enteredState ) {
                                            nextPlayer->emotFrozen = true;
                                            nextPlayer->emotFrozenIndex = 
                                                killEmotionIndex;
                                            
                                            newEmotPlayerIDs.push_back( 
                                                nextPlayer->id );
                                            newEmotIndices.push_back( 
                                                killEmotionIndex );
                                            newEmotTTLs.push_back( 120 );
                                            
                                            if( ! targetPlayer->emotFrozen ) {
                                                
                                                targetPlayer->emotFrozen = true;
                                                targetPlayer->emotFrozenIndex =
                                                    victimEmotionIndex;
                                                
                                                newEmotPlayerIDs.push_back( 
                                                    targetPlayer->id );
                                                newEmotIndices.push_back( 
                                                    victimEmotionIndex );
                                                newEmotTTLs.push_back( 120 );
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    else if( m.type == USE ) {

                        int target = getMapObject( m.x, m.y );
                        
                        // send update even if action fails (to let them
                        // know that action is over)
                        playerIndicesToSendUpdatesAbout.push_back( i );
                        
                        // track whether this USE resulted in something
                        // new on the ground in case of placing a grave
                        int newGroundObject = -1;
                        GridPos newGroundObjectOrigin =
                            { nextPlayer->heldGraveOriginX,
                              nextPlayer->heldGraveOriginY };
                        
                        // save current value here, because it may 
                        // change below
                        int heldGravePlayerID = nextPlayer->heldGravePlayerID;
                        

                        char distanceUseAllowed = false;
                        char requireExactTileUsage = false;
                        
                        if( nextPlayer->holdingID > 0 ) {
                            
                            // holding something
                            ObjectRecord *heldObj = 
                                getObject( nextPlayer->holdingID );
                            
                            if( heldObj->useDistance == 0 ) {
                                requireExactTileUsage = true;
                                }
                            else if( heldObj->useDistance > 1 ) {
                                // it's long-distance

                                GridPos targetPos = { m.x, m.y };
                                GridPos playerPos = { nextPlayer->xd,
                                                      nextPlayer->yd };
                                
                                double d = distance( targetPos,
                                                     playerPos );
                                
                                if( heldObj->useDistance >= d &&
                                    ! directLineBlocked( playerPos, 
                                                         targetPos ) ) {
                                    distanceUseAllowed = true;
                                    }
                                }
                            }
                        

                        if( distanceUseAllowed 
                            ||
                            ( isGridAdjacent( m.x, m.y,
                                            nextPlayer->xd, 
                                            nextPlayer->yd ) &&
                              !requireExactTileUsage )
                            ||
                            ( m.x == nextPlayer->xd &&
                              m.y == nextPlayer->yd ) ) {
                            
                            nextPlayer->actionAttempt = 1;
                            nextPlayer->actionTarget.x = m.x;
                            nextPlayer->actionTarget.y = m.y;
                            
                            if( m.x > nextPlayer->xd ) {
                                nextPlayer->facingOverride = 1;
                                }
                            else if( m.x < nextPlayer->xd ) {
                                nextPlayer->facingOverride = -1;
                                }

                            // can only use on targets next to us for now,
                            // no diags
                            

                            
                            int oldHolding = nextPlayer->holdingID;
                            
                            char accessBlocked =
                                isAccessBlocked( nextPlayer, m.x, m.y, target );
                            
                            if( accessBlocked ) {
                                // ignore action from wrong side
                                // or that players don't own

                                }
                            else if( nextPlayer->dying ) {
                                
                                bool healed = false;
                                    
                                // try healing wound
                                
                                TransRecord *healTrans =
                                    getMetaTrans( nextPlayer->holdingID,
                                                  target );
                                
                                int healTarget = 0;

                                if( healTrans != NULL ) {
                                    
                                    nextPlayer->holdingID = 
                                        healTrans->newActor;
                                    holdingSomethingNew( nextPlayer );
                                    
                                    // their wound has been changed
                                    // no longer track embedded weapon
                                    nextPlayer->embeddedWeaponID = 0;
                                    nextPlayer->embeddedWeaponEtaDecay = 0;
                                    
                                    setMapObject( m.x, m.y,
                                                  healTrans->newTarget );
                                    
                                    setResponsiblePlayer( -1 );
                                    
                                    healed = true;
                                    healTarget = healTrans->target;
                                    
                                    }
                                else {
                                    
                                    ObjectRecord *targetObj = getObject( target );
                                    
                                    if( targetObj != NULL )
                                    if( m.i != -1 && targetObj->permanent &&
                                        targetObj->numSlots > m.i &&
                                        getNumContained( m.x, m.y ) > m.i &&
                                        strstr( targetObj->description,
                                                "+useOnContained" ) != NULL ) {
                                        // a valid slot specified to use
                                        // held object on.
                                        // AND container allows this
                                        
                                        int contTarget = 
                                            getContained( m.x, m.y, m.i );
                                        
                                        char isSubCont = false;
                                        if( contTarget < 0 ) {
                                            contTarget = -contTarget;
                                            isSubCont = true;
                                            }

                                        ObjectRecord *contTargetObj =
                                            getObject( contTarget );
                                        
                                        TransRecord *contTrans =
                                            getPTrans( nextPlayer->holdingID,
                                                       contTarget );
                                        
                                        ObjectRecord *newTarget = NULL;
                                        
                                        if( ! isSubCont &&
                                            contTrans != NULL &&
                                            ( contTrans->newActor == 
                                              nextPlayer->holdingID ||
                                              contTrans->newActor == 0 ||
                                              canPickup( 
                                                  contTrans->newActor,
                                                  computeAge( 
                                                      nextPlayer ) ) ) ) {

                                            // a trans applies, and we
                                            // can hold the resulting actor
                                            if( contTrans->newTarget > 0 ) {
                                                newTarget = getObject(
                                                    contTrans->newTarget );
                                                }
                                            }
                                        if( (newTarget != NULL &&
                                            containmentPermitted(
                                                targetObj->id,
                                                newTarget->id )) || 
                                            (contTrans != NULL && 
                                            contTrans->newTarget == 0) ) {
                                                
                                            int oldHeld = 
                                                nextPlayer->holdingID;
                                            
                                            handleHoldingChange( 
                                                nextPlayer,
                                                contTrans->newActor );
                                            
                                            nextPlayer->heldOriginValid = 0;
                                            nextPlayer->heldOriginX = 0;
                                            nextPlayer->heldOriginY = 0;
                                            nextPlayer->
                                                heldTransitionSourceID = 0;
                                            
                                            if( contTrans->newActor > 0 && 
                                                contTrans->newActor !=
                                                oldHeld ) {
                                                
                                                nextPlayer->
                                                    heldTransitionSourceID
                                                    = contTargetObj->id;
                                                }

                                            
                                            setResponsiblePlayer( 
                                                - nextPlayer->id );
                                            
                                            changeContained( 
                                                m.x, m.y,
                                                m.i, 
                                                contTrans->newTarget );
                                            
                                            setResponsiblePlayer( -1 );
                                            
                                            healed = true;
                                            healTarget = contTarget;

                                            }
                                        }
                                    }
                                
                                if ( healed ) {
                                    
                                    nextPlayer->heldOriginValid = 0;
                                    nextPlayer->heldOriginX = 0;
                                    nextPlayer->heldOriginY = 0;
                                    nextPlayer->heldTransitionSourceID = 
                                        healTarget;
                                    
                                    if( nextPlayer->holdingID == 0 ) {
                                        // not dying anymore
                                        setNoLongerDying( 
                                            nextPlayer,
                                            &playerIndicesToSendHealingAbout );
                                        }
                                    else {
                                        // wound changed?

                                        ForcedEffects e = 
                                            checkForForcedEffects( 
                                                nextPlayer->holdingID );

                                        if( e.emotIndex != -1 ) {
                                            nextPlayer->emotFrozen = true;
                                            newEmotPlayerIDs.push_back( 
                                                nextPlayer->id );
                                            newEmotIndices.push_back( 
                                                e.emotIndex );
                                            newEmotTTLs.push_back( e.ttlSec );
                                            interruptAnyKillEmots( 
                                                nextPlayer->id, e.ttlSec );
                                            }
                                        if( e.foodCapModifier != 1 ) {
                                            nextPlayer->foodCapModifier = 
                                                e.foodCapModifier;
                                            nextPlayer->foodUpdate = true;
                                            }
                                        if( e.feverSet ) {
                                            nextPlayer->fever = e.fever;
                                            }
                                        }
                                    }
                                }
                            else if( target != 0 ) {

                                ObjectRecord *targetObj = getObject( target );
                                
                                //2HOL mechanics to read written objects
                                if( targetObj->written &&
                                    targetObj->clickToRead ) {
                                    GridPos readPos = { m.x, m.y };
                                    forceObjectToRead( nextPlayer, target, readPos, false );
                                    }
                                
                                // see if target object is permanent
                                // and has writing on it.
                                // if so, read by touching it
                                
                                // if( targetObj->permanent &&
                                    // targetObj->written ) {
                                    // forcePlayerToRead( nextPlayer, target );
                                    // }
                                

                                // try using object on this target 
                                
                                TransRecord *r = NULL;
                                char defaultTrans = false;
                                

                                char heldCanBeUsed = false;
                                char containmentTransfer = false;
                                char containmentTransition = false;
                                if( // if what we're holding contains
                                    // stuff, block it from being
                                    // used as a tool
                                    nextPlayer->numContained == 0 ) {
                                    heldCanBeUsed = true;
                                    }
                                else if( nextPlayer->holdingID > 0 ) {
                                    // see if result of trans
                                    // would preserve containment

                                    r = getPTrans( nextPlayer->holdingID,
                                                  target );


                                    ObjectRecord *heldObj = getObject( 
                                        nextPlayer->holdingID );
                                    
                                    if( r != NULL && r->newActor == 0 &&
                                        r->newTarget > 0 ) {
                                        ObjectRecord *newTargetObj =
                                            getObject( r->newTarget );
                                        
                                        if( targetObj->numSlots == 0
                                            && newTargetObj->numSlots >=
                                            heldObj->numSlots
                                            &&
                                            newTargetObj->slotSize >=
                                            heldObj->slotSize ) {
                                            
                                            containmentTransfer = true;
                                            heldCanBeUsed = true;
                                            }
                                        }

                                    if( r == NULL ) {
                                        // no transition applies for this
                                        // held, whether full or empty
                                        
                                        // let it be used anyway, so
                                        // that generic transition (below)
                                        // might apply
                                        heldCanBeUsed = true;
                                        }
                                    
                                    r = NULL;
                                    }
                                
                                

                                if( nextPlayer->holdingID >= 0 &&
                                    heldCanBeUsed ) {
                                    // negative holding is ID of baby
                                    // which can't be used
                                    // (and no bare hand action available)
                                    r = getPTrans( nextPlayer->holdingID,
                                                  target );
                                    
                                    // also check for containment transitions - USE stacking
                                    if( r == NULL && targetObj->numSlots == 0 ) {
                                        r = getPTrans( nextPlayer->holdingID,
                                                      target, false, false, 1 );
                                        if( r != NULL ) containmentTransition = true;
                                        }
                                    
                                    if( r == NULL ) {
                                        // no transition applies
                                        // check if held or target has
                                        // 1-second decay trans defined
                                        // If so, treat it as instant
                                        // and let it go through now
                                        // (skip if result of decay is 0)
                                        TransRecord *heldDecay = 
                                                getPTrans( 
                                                    -1, 
                                                    nextPlayer->holdingID );
                                        if( heldDecay != NULL &&
                                            heldDecay->autoDecaySeconds == 1 &&
                                            heldDecay->newTarget > 0 ) {
                                            // force decay NOW and try again
                                            handleHeldDecay( 
                                             nextPlayer,
                                             i,
                                             &playerIndicesToSendUpdatesAbout,
                                             &playerIndicesToSendHealingAbout );
                                            r = getPTrans( 
                                                nextPlayer->holdingID,
                                                target );
                                            }
                                        
                                        }
                                    if( r == NULL ) {
                                        
                                        int newTarget = 
                                            checkTargetInstantDecay(
                                                target, m.x, m.y );
                                        
                                        // if so, let transition go through
                                        // (skip if result of decay is 0)
                                        if( newTarget != 0 &&
                                            newTarget != target ) {
                                            
                                            target = newTarget;
                                            targetObj = getObject( target );
                                            
                                            r = getPTrans( 
                                                nextPlayer->holdingID,
                                                target );
                                            }
                                        }
                                    }
                                

                                if( r != NULL &&
                                    r->newActor > 0 &&
                                    getObject( r->newActor )->floor ) {
                                    // special case:
                                    // ending up with floor in hand means
                                    // we stick floor UNDER target
                                    // object on ground
                                    
                                    // but only if there's no floor there
                                    // already
                                    if( getMapFloor( m.x, m.y ) == 0 ) {    
                                        setMapFloor( m.x, m.y, r->newActor );
                                        nextPlayer->holdingID = 0;
                                        nextPlayer->holdingEtaDecay = 0;
                                        }
                                    
                                    // always cancel transition in either case
                                    r = NULL;
                                    }

                                
                                if( r != NULL &&
                                    targetObj->numSlots > 0 ) {
                                    // target has number of slots
                                    
                                    int numContained = 
                                        getNumContained( m.x, m.y );
                                    
                                    int numSlotsInNew = 0;
                                    
                                    if( r->newTarget > 0 ) {
                                        numSlotsInNew =
                                            getObject( r->newTarget )->numSlots;
                                        }
                                        
                                    if( numContained > 0 && 
                                        numSlotsInNew > 0 &&
                                        numContained <= numSlotsInNew && 
                                        r->newTarget > 0 &&
                                        getObject( r->newTarget )->slotSize < 
                                        targetObj->slotSize ) {
                                        // container is holding something
                                        // and it is going to have a smaller slotSize
                                        // make sure the contained items
                                        // can fit in the new container
                                        for( int i=0; i<numContained; i++ ) {
                                            int contained = 
                                                    getContained( m.x, m.y, i );
                                            if( contained < 0 ) contained = -contained;
                                            if( !containmentPermitted( r->newTarget, contained ) ) {
                                                heldCanBeUsed = false;
                                                r = NULL;
                                                break;
                                                }
                                            }
                                        }
                                    
                                    if( r != NULL )
                                    if( numContained > numSlotsInNew &&
                                        numSlotsInNew == 0 ) {
                                        // not enough room in new target

                                        // check if new actor will contain
                                        // them (reverse containment transfer)
                                        
                                        int oldSlots = 
                                            getNumContainerSlots( target );
                                        int newSlots = 
                                            getNumContainerSlots( r->newTarget );
                                        
                                        if( r->newActor > 0 &&
                                            nextPlayer->numContained == 0 &&
                                            ( oldSlots > 0 &&
                                            newSlots == 0 && 
                                            r->actor == 0 &&
                                            r->newActor > 0 &&
                                            getNumContainerSlots( r->newActor ) == oldSlots &&
                                            getObject( r->newActor )->slotSize >= targetObj->slotSize )
                                            ) {
                                            // old actor empty
                                            
                                            int numSlotsNewActor =
                                                getObject( r->newActor )->
                                                numSlots;
                                         
                                            numSlotsInNew = numSlotsNewActor;
                                            }
                                        }


                                    if( numContained > numSlotsInNew ) {
                                        // would result in shrinking
                                        // and flinging some contained
                                        // objects
                                        // block it.
                                        heldCanBeUsed = false;
                                        r = NULL;
                                        }
                                        
                                    }
                                  
                                if( r == NULL && 
                                    ( nextPlayer->holdingID != 0 || 
                                      targetObj->permanent ) &&
                                    ( isGridAdjacent( m.x, m.y,
                                                      nextPlayer->xd, 
                                                      nextPlayer->yd ) 
                                      ||
                                      ( m.x == nextPlayer->xd &&
                                        m.y == nextPlayer->yd ) ) ) {
                                    
                                    // block default transitions from
                                    // happening at a distance

                                    // search for default 
                                    r = getPTrans( -2, target );
                                        
                                    if( r != NULL ) {
                                        defaultTrans = true;
                                        }
                                    else if( nextPlayer->holdingID <= 0 || 
                                             targetObj->numSlots == 0 ) {
                                        // also consider bare-hand
                                        // action that produces
                                        // no new held item
                                        
                                        // but only on non-container
                                        // objects (example:  we don't
                                        // want to kick minecart into
                                        // motion every time we try
                                        // to add something to it)
                                        
                                        // treat this the same as
                                        // default
                                        r = getPTrans( 0, target );
                                        
                                        if( r != NULL && 
                                            r->newActor == 0 ) {
                                            
                                            defaultTrans = true;
                                            }
                                        else {
                                            r = NULL;
                                            }
                                        }
                                    }
                                


                                if( r != NULL &&
                                    r->newTarget > 0 &&
                                    r->newTarget != target ) {
                                    
                                    // target would change here
                                    if( getMapFloor( m.x, m.y ) != 0 ) {
                                        // floor present
                                        
                                        // make sure new target allowed 
                                        // to exist on floor
                                        if( strstr( getObject( r->newTarget )->
                                                    description, 
                                                    "groundOnly" ) != NULL ) {
                                            r = NULL;
                                            }
                                        }
                                    }
                                

                                if( r == NULL && 
                                    nextPlayer->holdingID > 0 ) {
                                    
                                    logTransitionFailure( 
                                        nextPlayer->holdingID,
                                        target );
                                    }
                                
                                double playerAge = computeAge( nextPlayer );
                                
                                int hungryWorkCost = 0;
                                
                                if( r != NULL && 
                                    r->newTarget > 0 ) {
                                    char *des =
                                        getObject( r->newTarget )->description;
                                    
                                    char *desPos =
                                        strstr( des, "+hungryWork" );
                                    
                                    if( desPos != NULL ) {
                                        
                                    
                                        sscanf( desPos,
                                                "+hungryWork%d", 
                                                &hungryWorkCost );
                                        
                                        if( nextPlayer->foodStore < 
                                            hungryWorkCost ) {
                                            // block transition,
                                            // not enough food
                                            r = NULL;
                                            }
                                        }
                                    }


                                    // password-protected objects - password creation without saying password first
                                    if( r != NULL &&
                                        oldHolding > 0 &&
                                        r->newTarget > 0 &&
                                        getObject( oldHolding )->passwordAssigner &&
                                        getObject( r->newTarget )->passwordProtectable ) {
                                        
                                        char *found = nextPlayer->saidPassword;
                                        if ( ( found == NULL ) || ( found[0] == '\0') ) {
                                            
                                            // block it
                                            r = NULL;
                                            
                                            // make it clear why the transition didn't go through
                                            sendGlobalMessage( (char*)"SAY A PASSWORD FIRST.**"
                                                "SAY   PASSWORD IS XXX   TO SET YOUR PASSWORD."
                                                , nextPlayer );
                                            }
                                        }


                                if( r != NULL && containmentTransfer ) {
                                    // special case contained items
                                    // moving from actor into new target
                                    // (and hand left empty)
                                    setResponsiblePlayer( - nextPlayer->id );
                                    
                                    setMapObject( m.x, m.y, r->newTarget );
                                    newGroundObject = r->newTarget;
                                    
                                    setResponsiblePlayer( -1 );
                                    
                                    transferHeldContainedToMap( nextPlayer,
                                                                m.x, m.y );
                                    handleHoldingChange( nextPlayer,
                                                         r->newActor );

                                    setHeldGraveOrigin( nextPlayer, m.x, m.y,
                                                        r->newTarget );
                                    }
                                else if( r != NULL &&
                                    // are we old enough to handle
                                    // what we'd get out of this transition?
                                    ( ( r->newActor == 0 &&
                                        playerAge >= defaultActionAge )
                                      || 
                                      ( r->newActor > 0 &&
                                        getObject( r->newActor )->minPickupAge 
                                        <= 
                                        playerAge ) ) 
                                    &&
                                    // does this create a blocking object?
                                    // only consider vertical-blocking
                                    // objects (like vertical walls and doors)
                                    // because these look especially weird
                                    // on top of a player
                                    // We can detect these because they 
                                    // also hug the floor
                                    // Horizontal doors look fine when
                                    // closed on player because of their
                                    // vertical offset.
                                    //
                                    // if so, make sure there's not someone
                                    // standing still there
                                    ( r->newTarget == 0 ||
                                      ! 
                                      ( getObject( r->newTarget )->
                                          blocksWalking
                                        &&
                                        getObject( r->newTarget )->
                                          floorHugging )
                                      ||
                                      isMapSpotEmptyOfPlayers( m.x, 
                                                               m.y ) ) ) {
                                    
                                    if( ! defaultTrans ) {    
                                        handleHoldingChange( nextPlayer,
                                                             r->newActor );
                                        
                                        setHeldGraveOrigin( nextPlayer, 
                                                            m.x, m.y,
                                                            r->newTarget );
                                        
                                        if( r->target > 0 ) {    
                                            nextPlayer->heldTransitionSourceID =
                                                r->target;
                                            }
                                        else {
                                            nextPlayer->heldTransitionSourceID =
                                                -1;
                                            }
                                        }
                                    


                                    // has target shrunken as a container?
                                    int oldSlots = 
                                        getNumContainerSlots( target );
                                    int newSlots = 
                                        getNumContainerSlots( r->newTarget );
                                    
                                    if( oldSlots > 0 &&
                                        newSlots == 0 && 
                                        r->actor == 0 &&
                                        r->newActor > 0
                                        &&
                                        getNumContainerSlots( r->newActor ) ==
                                        oldSlots &&
                                        getObject( r->newActor )->slotSize >=
                                        targetObj->slotSize ) {
                                        
                                        // bare-hand action that results
                                        // in something new in hand
                                        // with same number of slots 
                                        // as target
                                        // keep what was contained

                                        FullMapContained f =
                                            getFullMapContained( m.x, m.y );

                                        setContained( nextPlayer, f );
                                    
                                        clearAllContained( m.x, m.y );
                                        
                                        restretchDecays( 
                                            nextPlayer->numContained,
                                            nextPlayer->containedEtaDecays,
                                            target, r->newActor );
                                        }
                                    else {
                                        // target on ground changed
                                        // and we don't have the same
                                        // number of slots in a new held obj
                                        
                                        shrinkContainer( m.x, m.y, newSlots );
                                    
                                        if( newSlots > 0 ) {    
                                            restretchMapContainedDecays( 
                                                m.x, m.y,
                                                target,
                                                r->newTarget );
                                            }
                                        }
                                        
                                        
                                    // Check for containment transitions - changing container by USE
                                    
                                    if( oldSlots > 0 &&
                                        newSlots > 0 && 
                                        // assume same number of slots for simplicity
                                        oldSlots == newSlots
                                        ) {
                                            
                                        int numContained = 
                                            getNumContained( m.x, m.y );
                                            
                                        if( numContained > 0 ) {
                                            
                                            for( int i=0; i<numContained; i++ ) {
                                            
                                                int contained = 
                                                    getContained( m.x, m.y, i );
                                                
                                                if( contained < 0 ) {
                                                    // again for simplicity
                                                    // block transisionts if it is a subcontainer
                                                    continue;
                                                    }
                                                
                                                TransRecord *containmentTrans = NULL;
                                                int containedID = contained;
                                                int oldContainedID = target;
                                                int newContainerID = r->newTarget;
                                                
                                                // Consider only Any flag here
                                                // The other flags don't make sense here, we're changing the container itself
                                                // not interacting with the contained items
                                                
                                                // IN precedes OUT
                                                int newContainedID = -1;
                                                if( containmentTrans == NULL ) {
                                                    containmentTrans = getPTrans( containedID, newContainerID, false, false, 4 );
                                                    if( containmentTrans == NULL ) containmentTrans = getPTrans( 0, newContainerID, false, false, 4 );
                                                    if( containmentTrans != NULL ) newContainedID = containmentTrans->newActor;
                                                }
                                                if( containmentTrans == NULL ) {
                                                    containmentTrans = getPTrans( oldContainedID, containedID, false, false, 4 );
                                                    if( containmentTrans == NULL ) containmentTrans = getPTrans( 0, containedID, false, false, 4 );
                                                    if( containmentTrans != NULL ) newContainedID = containmentTrans->newTarget;
                                                }
                                                
                                                // Execute containment transitions - changing container by USE
                                                
                                                if( containmentTrans != NULL ) {
                                                    changeContained( m.x, m.y, i, newContainedID );
                                                }
                                                
                                                }
                                            }
                                        }
                                        
                                    
                                    
                                    timeSec_t oldEtaDecay = 
                                        getEtaDecay( m.x, m.y );
                                    
                                    setResponsiblePlayer( - nextPlayer->id );
                                    
                                   
                                    if( r->newTarget > 0 
                                        && getObject( r->newTarget )->floor ) {

                                        // it turns into a floor
                                        setMapObject( m.x, m.y, 0 );
                                        
                                        setMapFloor( m.x, m.y, r->newTarget );
                                        
                                        if( r->newTarget == target ) {
                                            // unchanged
                                            // keep old decay in place
                                            setFloorEtaDecay( m.x, m.y, 
                                                              oldEtaDecay );
                                            }
                                        }
                                    else {    
                                        setMapObject( m.x, m.y, r->newTarget );
                                        newGroundObject = r->newTarget;
                                        }
                                        
                                    // Execute containment transitions - USE stacking - contained
                                    // creation of the container is above
                                    if( containmentTransition ) {
                                        int idToAdd = nextPlayer->holdingID;
                                        if( r->newActor > 0 ) idToAdd = r->newActor;
                 
                                        addContained( 
                                            m.x, m.y,
                                            idToAdd,
                                            nextPlayer->holdingEtaDecay );
                                            
                                        handleHoldingChange( nextPlayer,
                                                             0 );
                                        }
                                    
                                    if( hungryWorkCost > 0 ) {
                                        int oldStore = nextPlayer->foodStore;
                                        
                                        nextPlayer->foodStore -= hungryWorkCost;
                                        
                                        if( nextPlayer->foodStore < 3 ) {
                                            if( oldStore > 3  ) {
                                                // generally leave
                                                // player with 3 food
                                                // unless they had less than
                                                // 3 to start
                                                nextPlayer->foodStore = 3;
                                                }
                                            }
                                        nextPlayer->foodUpdate = true;
                                        }
                                    
                                    
                                    setResponsiblePlayer( -1 );

                                    if( target == r->newTarget ) {
                                        // target not changed
                                        // keep old decay in place
                                        setEtaDecay( m.x, m.y, oldEtaDecay );
                                        }
                                    
                                    if( target > 0 && r->newTarget > 0 &&
                                        target != r->newTarget &&
                                        ! getObject( target )->isOwned &&
                                        getObject( r->newTarget )->isOwned ) {
                                        
                                        // player just created an owned
                                        // object here
                                        GridPos newPos = { m.x, m.y };

                                        nextPlayer->
                                            ownedPositions.push_back( newPos );
                                        newOwnerPos.push_back( newPos );
                                        }

                                    // password-protected objects - password creation
                                    if( oldHolding > 0 &&
                                        r->newTarget > 0 &&
                                        getObject( oldHolding )->passwordAssigner &&
                                        getObject( r->newTarget )->passwordProtectable ) {                                           

                                        char *found = nextPlayer->saidPassword;
                                            
                                        if ( ( found != NULL ) && ( found[0] != '\0') ) {
                                            
                                            std::string password { nextPlayer->saidPassword };
                                            passwordRecord r = { m.x, m.y, password };
                                            passwordRecords.push_back( r );
                                           
                                            persistentMapDBPut( m.x, m.y, 1, password.c_str() );
                                            
                                        }
                                        else {
                                            // player hasn't said a password
                                            // this case should not happen
                                            // it should have been blocked elsewhere
                                        }
                                        
                                    }
                                    
                                    // password-protected objects - deletion
                                    if( target > 0 &&
                                        r->newTarget > 0 &&
                                        target != r->newTarget &&
                                        getObject( target )->passwordProtectable &&
                                        !getObject( r->newTarget )->passwordProtectable ) {
                                        
                                        for( int i=0; i<passwordRecords.size(); i++ ) {
                                            passwordRecord r = passwordRecords.getElementDirect(i);
                                            if( m.x == r.x && m.y == r.y ) {
                                                passwordRecords.deleteElement( i );
                                                persistentMapDBPut( m.x, m.y, 1, "\0" );
                                                
                                                // keep looking, there may be old passwords
                                                // leftover from vog-deleting password-protected objects
                                                // break; 
                                                }
                                            }
                                            
                                        }

                                    if( r->actor == 0 &&
                                        target > 0 && r->newTarget > 0 &&
                                        target != r->newTarget ) {
                                        
                                        TransRecord *oldDecayTrans = 
                                            getTrans( -1, target );
                                        
                                        TransRecord *newDecayTrans = 
                                            getTrans( -1, r->newTarget );
                                        
                                        if( oldDecayTrans != NULL &&
                                            newDecayTrans != NULL  &&
                                            oldDecayTrans->epochAutoDecay ==
                                            newDecayTrans->epochAutoDecay &&
                                            oldDecayTrans->autoDecaySeconds ==
                                            newDecayTrans->autoDecaySeconds &&
                                            oldDecayTrans->autoDecaySeconds 
                                            > 0 ) {
                                            
                                            // old target and new
                                            // target decay into something
                                            // in same amount of time
                                            // and this was a bare-hand
                                            // action
                                            
                                            // doesn't matter if they 
                                            // decay into SAME thing.

                                            // keep old decay time in place
                                            // (instead of resetting timer)
                                            setEtaDecay( m.x, m.y, 
                                                         oldEtaDecay );
                                            }
                                        }
                                    

                                    

                                    if( r->newTarget != 0 ) {
                                        
                                        handleMapChangeToPaths( 
                                            m.x, m.y,
                                            getObject( r->newTarget ),
                                            &playerIndicesToSendUpdatesAbout );
                                        }
                                    }
                                else if( nextPlayer->holdingID == 0 &&
                                         ! targetObj->permanent &&
                                         targetObj->minPickupAge <= 
                                         computeAge( nextPlayer ) ) {
                                    // no bare-hand transition applies to
                                    // this non-permanent target object
                                    
                                    // treat it like pick up
                                    
                                    pickupToHold( nextPlayer, m.x, m.y,
                                                  target );
                                    }         
                                else if( nextPlayer->holdingID >= 0 ) {
                                    
                                    char handled = false;
                                    
                                    if( m.i != -1 && targetObj->permanent &&
                                        targetObj->numSlots > m.i &&
                                        getNumContained( m.x, m.y ) > m.i &&
                                        strstr( targetObj->description,
                                                "+useOnContained" ) != NULL ) {
                                        // a valid slot specified to use
                                        // held object on.
                                        // AND container allows this
                                        
                                        int contTarget = 
                                            getContained( m.x, m.y, m.i );
                                        
                                        char isSubCont = false;
                                        if( contTarget < 0 ) {
                                            contTarget = -contTarget;
                                            isSubCont = true;
                                            }

                                        ObjectRecord *contTargetObj =
                                            getObject( contTarget );
                                        
                                        TransRecord *contTrans =
                                            getPTrans( nextPlayer->holdingID,
                                                       contTarget );
                                        
                                        ObjectRecord *newTarget = NULL;
                                        
                                        // Check if this transition will trigger
                                        // a containment transition
                                        
                                        TransRecord *containmentTrans = NULL;
                                        bool noInContTrans = false;
                                        bool isOutContTrans = false;
                                        
                                        bool blockedByContainmentTrans = false;
                                        
                                        if( ! isSubCont &&
                                            contTrans != NULL &&
                                            ( contTrans->newActor == 
                                              nextPlayer->holdingID ||
                                              contTrans->newActor == 0 ||
                                              canPickup( 
                                                  contTrans->newActor,
                                                  computeAge( 
                                                      nextPlayer ) ) ) ) {

                                            // a trans applies, and we
                                            // can hold the resulting actor
                                            if( contTrans->newTarget > 0 ) {
                                                newTarget = getObject(
                                                    contTrans->newTarget );
                                                }
                                                
                                            // Check if this transition will trigger
                                            // a containment transition
                                                
                                            int numContained = 
                                                getNumContained( m.x, m.y );
                                                           
                                            int containerID = targetObj->id;
                                            int containedID = contTrans->newTarget;
                                            int oldContainedID = contTrans->target;
                                            
                                            // IN containment transitions - useOnContained
                                            
                                            if( numContained == 1 ) {
                                                containmentTrans = getPTrans( containedID, containerID, false, false, 1 );
                                                if( containmentTrans == NULL ) containmentTrans = getPTrans( 0, containerID, false, false, 1 );
                                            } else if( targetObj->numSlots == numContained ) {
                                                containmentTrans = getPTrans( containedID, containerID, false, false, 2 );
                                                if( containmentTrans == NULL ) containmentTrans = getPTrans( 0, containerID, false, false, 2 );
                                            }
                                            
                                            // Any No Swap flag doesn't make sense here as we are useOnContained
                                            // Still keeping it here to potentially preceed Any
                                            if( containmentTrans == NULL ) {
                                                containmentTrans = getPTrans( containedID, containerID, false, false, 3 );
                                                if( containmentTrans == NULL ) containmentTrans = getPTrans( 0, containerID, false, false, 3 );
                                            }
                                            
                                            if( containmentTrans == NULL ) {
                                                containmentTrans = getPTrans( containedID, target, false, false, 4 );
                                                if( containmentTrans == NULL ) containmentTrans = getPTrans( 0, target, false, false, 4 );
                                            }
                                            
                                            if( containmentTrans == NULL ) noInContTrans = true;
                                            
                                            // OUT containment transitions - useOnContained
                                            
                                            if( containmentTrans == NULL ) {
                                                if( numContained == 1 ) {
                                                    containmentTrans = getPTrans( containerID, oldContainedID, false, false, 2 );
                                                    if( containmentTrans == NULL ) containmentTrans = getPTrans( containerID, 0, false, false, 2 );
                                                } else if( targetObj->numSlots == numContained ) {
                                                    containmentTrans = getPTrans( containerID, oldContainedID, false, false, 1 );
                                                    if( containmentTrans == NULL ) containmentTrans = getPTrans( containerID, 0, false, false, 1 );
                                                }
                                            }
                                            
                                            // Any No Swap flag doesn't make sense here as we are useOnContained
                                            // Still keeping it here to potentially preceed Any
                                            if( containmentTrans == NULL ) {
                                                containmentTrans = getPTrans( containerID, oldContainedID, false, false, 3 );
                                                if( containmentTrans == NULL ) containmentTrans = getPTrans( containerID, 0, false, false, 3 );
                                            }
                                            
                                            if( containmentTrans == NULL ) {
                                                containmentTrans = getPTrans( target, oldContainedID, false, false, 4 );
                                                if( containmentTrans == NULL ) containmentTrans = getPTrans( target, 0, false, false, 4 );
                                            }
                                            
                                            if( containmentTrans != NULL && noInContTrans ) isOutContTrans = true;
                                            
                                            
                                            if( containmentTrans != NULL ) {
                                                
                                                // Check that the new container can contain all the objects
                                                
                                                int newContainerID = containmentTrans->newTarget;
                                                if( isOutContTrans ) newContainerID = containmentTrans->newActor;
                                                
                                                int newNumSlots = getNumContainerSlots( newContainerID );
                                                
                                                if( (isOutContTrans && containmentTrans->target != containmentTrans->newTarget) ||
                                                    (!isOutContTrans && containmentTrans->actor != containmentTrans->newActor) ) {
                                                    // This case means that the useOnContained transition
                                                    // triggers a containment transition,
                                                    // both trying to change the object being taken out.
                                                    
                                                    // Let the useOnContained transition preceeds the containment transition
                                                    containmentTrans = NULL;
                                                    blockedByContainmentTrans = false;
                                                    }
                                                else if( numContained > newNumSlots ) {
                                                    containmentTrans = NULL;
                                                    blockedByContainmentTrans = true;
                                                    } 
                                                else {
                                                    bool newContainedOK = false;
                                                    bool otherContainedOK = false;
                                                    
                                                    newContainedOK = containmentPermitted( newContainerID, contTrans->newTarget );
                                                    
                                                    int slotNumber = numContained - 1;
                                                    if( isOutContTrans && slotNumber == m.i ) slotNumber--;
                                                    
                                                    if( slotNumber >= 0 ) {
                                                        
                                                        int contID = getContained( 
                                                            m.x, m.y,
                                                            slotNumber );
                                                            
                                                        if( contID < 0 ) contID *= -1;
                                                    
                                                        while( slotNumber >= 0 &&
                                                               containmentPermitted( newContainerID, contID ) )  {
                                                    
                                                            slotNumber--;
                                                            
                                                            if( isOutContTrans && slotNumber == m.i ) slotNumber--;
                                                            
                                                            if( slotNumber < 0 ) break;
                                                            
                                                            contID = getContained( 
                                                                m.x, m.y,
                                                                slotNumber );
                                                                
                                                        
                                                            if( contID < 0 ) {
                                                                contID *= -1;
                                                                }
                                                                
                                                            }
                                                            
                                                        if( slotNumber >= 0 ) {
                                                            otherContainedOK = false;
                                                            }
                                                        else {
                                                            otherContainedOK = true;
                                                            }
                                                        } 
                                                    else {
                                                        otherContainedOK = true;
                                                        }
                                                        
                                                    if( !otherContainedOK || !newContainedOK ) containmentTrans = NULL;
                                                    if( !newContainedOK ) {
                                                        // Check that the old container can contain the new object
                                                        // If not, block it
                                                        if( !containmentPermitted( containerID, contTrans->newTarget ) ) {
                                                            blockedByContainmentTrans = true;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                            
                                        if( newTarget != NULL &&
                                            containmentPermitted(
                                                targetObj->id,
                                                newTarget->id ) &&
                                                !blockedByContainmentTrans ) {
                                                
                                            int oldHeld = 
                                                nextPlayer->holdingID;
                                            
                                            handleHoldingChange( 
                                                nextPlayer,
                                                contTrans->newActor );
                                            
                                            nextPlayer->heldOriginValid = 0;
                                            nextPlayer->heldOriginX = 0;
                                            nextPlayer->heldOriginY = 0;
                                            nextPlayer->
                                                heldTransitionSourceID = 0;
                                            
                                            if( contTrans->newActor > 0 && 
                                                contTrans->newActor !=
                                                oldHeld ) {
                                                
                                                nextPlayer->
                                                    heldTransitionSourceID
                                                    = contTargetObj->id;
                                                }

                                            
                                            setResponsiblePlayer( 
                                                - nextPlayer->id );
                                            
                                            
                                            
                                            bool shouldResetDecay = true;
                                            if( contTrans->target == contTrans->newTarget ) 
                                                shouldResetDecay = false;

                                            if( contTrans->actor == 0 &&
                                                contTrans->target > 0 && contTrans->newTarget > 0 &&
                                                contTrans->target != contTrans->newTarget ) {
                                                
                                                TransRecord *oldDecayTrans = 
                                                    getTrans( -1, contTrans->target );
                                                
                                                TransRecord *newDecayTrans = 
                                                    getTrans( -1, contTrans->newTarget );
                                                
                                                if( oldDecayTrans != NULL &&
                                                    newDecayTrans != NULL  &&
                                                    oldDecayTrans->epochAutoDecay ==
                                                    newDecayTrans->epochAutoDecay &&
                                                    oldDecayTrans->autoDecaySeconds ==
                                                    newDecayTrans->autoDecaySeconds &&
                                                    oldDecayTrans->autoDecaySeconds 
                                                    > 0 ) {
                                                    
                                                    // old target and new
                                                    // target decay into something
                                                    // in same amount of time
                                                    // and this was a bare-hand
                                                    // action
                                                    
                                                    // doesn't matter if they 
                                                    // decay into SAME thing.

                                                    // keep old decay time in place
                                                    // (instead of resetting timer)
                                                    shouldResetDecay = false;
                                                    
                                                    }
                                                }

                                            changeContained( 
                                                m.x, m.y,
                                                m.i, 
                                                contTrans->newTarget );
                                                
                                                
                                            if( shouldResetDecay ) {
                                                
                                                TransRecord *newDecayT = getMetaTrans( -1, contTrans->newTarget );
                                                
                                                if( newDecayT != NULL ) {
                                                    timeSec_t mapETA = Time::getCurrentTime() + newDecayT->autoDecaySeconds;
                                                    setSlotEtaDecay( m.x, m.y, m.i, mapETA, 0 );
                                                    }
                                                
                                                }
                                                
                                                
                                            // Execute containment transitions - useOnContained - container
                                            // contained is not changed, because this is useOnContained transition
                                            
                                            if( containmentTrans != NULL ) {
                                                int newContainerID = containmentTrans->newTarget;
                                                if( isOutContTrans ) newContainerID = containmentTrans->newActor;
                                                if( containmentTrans == NULL ) setResponsiblePlayer( -1 );
                                                setMapObject( m.x, m.y, newContainerID );
                                                }                     
                                            
                                            setResponsiblePlayer( -1 );
                                            handled = true;
                                            }
                                        }

                                    
                                    // consider other cases
                                    if( ! handled ) {
                                        if( nextPlayer->holdingID == 0 &&
                                            targetObj->permanent ) {
                                    
                                            // try removing from permanent
                                            // container
                                            removeFromContainerToHold( 
                                                nextPlayer,
                                                m.x, m.y,
                                                m.i );
                                            }
                                        else if( nextPlayer->holdingID > 0 ) {
                                            // try adding what we're holding to
                                            // target container
                                            
                                            addHeldToContainer(
                                                nextPlayer, target, m.x, m.y );
                                            }
                                        }
                                    }
                                

                                if( targetObj->permanent &&
                                    (targetObj->foodValue > 0 || targetObj->bonusValue > 0) ) {
                                    
                                    // just touching this object
                                    // causes us to eat from it
                                    
                                    nextPlayer->justAte = true;
                                    nextPlayer->justAteID = 
                                        targetObj->id;

                                    nextPlayer->lastAteID = 
                                        targetObj->id;
                                    nextPlayer->lastAteFillMax =
                                        nextPlayer->foodStore;
                                    
                                    nextPlayer->foodStore += 
                                        targetObj->foodValue;
                                    
                                    updateYum( nextPlayer, targetObj->id );
                                    
                                    int bonus = getEatBonus( nextPlayer );
                                    
                                    logEating( targetObj->id,
                                               targetObj->foodValue + bonus,
                                               computeAge( nextPlayer ),
                                               m.x, m.y );
                                    
                                    nextPlayer->foodStore += bonus;

                                    int cap = 
                                        nextPlayer->lastReportedFoodCapacity;
                                    
                                    if( nextPlayer->foodStore > cap ) {
    
                                        int over = nextPlayer->foodStore - cap;
                                        
                                        nextPlayer->foodStore = cap;

                                        nextPlayer->yummyBonusStore += over;
                                        }

                                    
                                    // we eat everything BUT what
                                    // we picked from it, if anything
                                    if( oldHolding == 0 && 
                                        nextPlayer->holdingID != 0 ) {
                                        
                                        ObjectRecord *newHeld =
                                            getObject( nextPlayer->holdingID );
                                        
                                        if( newHeld->foodValue > 0 ) {
                                            nextPlayer->foodStore -=
                                                newHeld->foodValue;

                                            if( nextPlayer->lastAteFillMax >
                                                nextPlayer->foodStore ) {
                                                
                                                nextPlayer->foodStore =
                                                    nextPlayer->lastAteFillMax;
                                                }
                                            }
                                        
                                        }
                                    
                                    
                                    if( targetObj->alcohol != 0 ) {
                                        drinkAlcohol( nextPlayer,
                                                      targetObj->alcohol );
                                        }
                                        
                                    if( strstr( targetObj->description, "+drug" ) != NULL ) {
                                        doDrug( nextPlayer );
                                        }


                                    nextPlayer->foodDecrementETASeconds =
                                        Time::getCurrentTime() +
                                        computeFoodDecrementTimeSeconds( 
                                            nextPlayer );
                                    
                                    nextPlayer->foodUpdate = true;
                                    }
                                }
                            else if( nextPlayer->holdingID > 0 ) {
                                // target location emtpy
                                // target not where we're standing
                                // we're holding something
                                
                                char usedOnFloor = false;
                                int floorID = getMapFloor( m.x, m.y );
                                
                                if( floorID > 0 ) {
                                    
                                    TransRecord *r = 
                                        getPTrans( nextPlayer->holdingID,
                                                  floorID );
                                
                                    if( r == NULL ) {
                                        logTransitionFailure( 
                                            nextPlayer->holdingID,
                                            floorID );
                                        }
                                        

                                    if( r != NULL && 
                                        // make sure we're not too young
                                        // to hold result of on-floor
                                        // transition
                                        ( r->newActor == 0 ||
                                          getObject( r->newActor )->
                                             minPickupAge <= 
                                          computeAge( nextPlayer ) ) ) {

                                        // applies to floor
                                        int resultID = r->newTarget;
                                        
                                        if( getObject( resultID )->floor ) {
                                            // changing floor to floor
                                            // go ahead
                                            usedOnFloor = true;
                                            
                                            if( resultID != floorID ) {
                                                setMapFloor( m.x, m.y,
                                                             resultID );
                                                }
                                            handleHoldingChange( nextPlayer,
                                                                 r->newActor );
                                            
                                            setHeldGraveOrigin( nextPlayer, 
                                                                m.x, m.y,
                                                                resultID );
                                            }
                                        else {
                                            // changing floor to non-floor
                                            char canPlace = true;
                                            if( getObject( resultID )->
                                                blocksWalking &&
                                                ! isMapSpotEmpty( m.x, m.y ) ) {
                                                canPlace = false;
                                                }
                                            
                                            if( canPlace ) {
                                                setMapFloor( m.x, m.y, 0 );
                                                
                                                setMapObject( m.x, m.y,
                                                              resultID );
                                                
                                                handleHoldingChange( 
                                                    nextPlayer,
                                                    r->newActor );
                                                setHeldGraveOrigin( nextPlayer, 
                                                                    m.x, m.y,
                                                                    resultID );
                                            
                                                usedOnFloor = true;
                                                }
                                            }
                                        }
                                    }
                                


                                // consider a use-on-bare-ground transtion
                                
                                ObjectRecord *obj = 
                                    getObject( nextPlayer->holdingID );
                                
                                if( ! usedOnFloor && obj->foodValue == 0 ) {
                                    
                                    // get no-target transtion
                                    // (not a food transition, since food
                                    //   value is 0)
                                    TransRecord *r = 
                                        getPTrans( nextPlayer->holdingID, 
                                                  -1 );


                                    char canPlace = false;
                                    
                                    if( r != NULL &&
                                        r->newTarget != 0 
                                        && 
                                        // make sure we're not too young
                                        // to hold result of bare ground
                                        // transition
                                        ( r->newActor == 0 ||
                                          getObject( r->newActor )->
                                             minPickupAge <= 
                                          computeAge( nextPlayer ) ) ) {
                                        
                                        canPlace = true;
                                        
                                        ObjectRecord *newTargetObj =
                                            getObject( r->newTarget );
                                        

                                        if( newTargetObj->blocksWalking &&
                                            ! isMapSpotEmpty( m.x, m.y ) ) {
                                            
                                            // can't do on-bare ground
                                            // transition where person 
                                            // standing
                                            // if it creates a blocking 
                                            // object
                                            canPlace = false;
                                            }
                                        else if( 
                                            strstr( newTargetObj->description, 
                                                    "groundOnly" ) != NULL
                                            &&
                                            getMapFloor( m.x, m.y ) != 0 ) {
                                            // floor present
                                        
                                            // new target not allowed 
                                            // to exist on floor
                                            canPlace = false;
                                            }
                                        }
                                    
                                    if( canPlace ) {
                                        nextPlayer->heldTransitionSourceID =
                                            nextPlayer->holdingID;
                                        
                                        if( nextPlayer->numContained > 0 &&
                                            r->newActor == 0 &&
                                            r->newTarget > 0 &&
                                            getObject( r->newTarget )->numSlots 
                                            >= nextPlayer->numContained &&
                                            getObject( r->newTarget )->slotSize
                                            >= obj->slotSize ) {

                                            // use on bare ground with full
                                            // container that leaves
                                            // hand empty
                                            
                                            // and there's room in newTarget

                                            setResponsiblePlayer( 
                                                - nextPlayer->id );

                                            setMapObject( m.x, m.y, 
                                                          r->newTarget );
                                            newGroundObject = r->newTarget;

                                            setResponsiblePlayer( -1 );
                                    
                                            transferHeldContainedToMap( 
                                                nextPlayer, m.x, m.y );
                                            
                                            handleHoldingChange( nextPlayer,
                                                                 r->newActor );
                                            
                                            setHeldGraveOrigin( nextPlayer, 
                                                                m.x, m.y,
                                                                r->newTarget );
                                            }
                                        else {
                                            handleHoldingChange( nextPlayer,
                                                                 r->newActor );
                                            
                                            setHeldGraveOrigin( nextPlayer, 
                                                                m.x, m.y,
                                                                r->newTarget );
                                            
                                            setResponsiblePlayer( 
                                                - nextPlayer->id );
                                            
                                            if( r->newTarget > 0 
                                                && getObject( r->newTarget )->
                                                floor ) {
                                                
                                                setMapFloor( m.x, m.y, 
                                                             r->newTarget );
                                                }
                                            else {    
                                                setMapObject( m.x, m.y, 
                                                              r->newTarget );
                                                newGroundObject = r->newTarget;
                                                }
                                            
                                            setResponsiblePlayer( -1 );
                                            
                                            handleMapChangeToPaths( 
                                             m.x, m.y,
                                             getObject( r->newTarget ),
                                             &playerIndicesToSendUpdatesAbout );
                                            }
                                        }
                                    }
                                }
                            else {
                                // target location emtpy
                                // and we're not holding anything
                                // check bare-hand transition on floor
                                
                                int floorID = getMapFloor( m.x, m.y );
                                
                                if( floorID > 0 ) {
                                    
                                    TransRecord *r = 
                                        getPTrans( 0,
                                                  floorID );
                                        

                                    if( r != NULL && 
                                        // make sure we're not too young
                                        // to hold result of on-floor
                                        // transition
                                        ( r->newActor == 0 ||
                                          getObject( r->newActor )->
                                             minPickupAge <= 
                                          computeAge( nextPlayer ) ) ) {

                                        // applies to floor
                                        int resultID = r->newTarget;
                                        
                                        if( getObject( resultID )->floor ) {
                                            // changing floor to floor
                                            // go ahead
                                            
                                            if( resultID != floorID ) {
                                                setMapFloor( m.x, m.y,
                                                             resultID );
                                                }
                                            handleHoldingChange( nextPlayer,
                                                                 r->newActor );
                                            
                                            setHeldGraveOrigin( nextPlayer, 
                                                                m.x, m.y,
                                                                resultID );
                                            }
                                        else {
                                            // changing floor to non-floor
                                            char canPlace = true;
                                            if( getObject( resultID )->
                                                blocksWalking &&
                                                ! isMapSpotEmpty( m.x, m.y ) ) {
                                                canPlace = false;
                                                }
                                            
                                            if( canPlace ) {
                                                setMapFloor( m.x, m.y, 0 );
                                                
                                                setMapObject( m.x, m.y,
                                                              resultID );
                                                
                                                handleHoldingChange( 
                                                    nextPlayer,
                                                    r->newActor );
                                                setHeldGraveOrigin( nextPlayer, 
                                                                    m.x, m.y,
                                                                    resultID );
                                                }
                                            }
                                        }
                                    }
                                }
                            

                            if( target == 0 && newGroundObject > 0 ) {
                                // target location was empty, and now it's not
                                // check if we moved a grave here
                            
                                ObjectRecord *o = getObject( newGroundObject );
                                
                                if( strstr( o->description, "origGrave" ) 
                                    != NULL ) {
                                    
                                    setGravePlayerID( 
                                        m.x, m.y, heldGravePlayerID );
                                    
                                    int swapDest = 
                                        isGraveSwapDest( m.x, m.y, 
                                                         nextPlayer->id );

                                    GraveMoveInfo g = { 
                                        { newGroundObjectOrigin.x,
                                          newGroundObjectOrigin.y },
                                        { m.x,
                                          m.y }, 
                                        swapDest };
                                    newGraveMoves.push_back( g );
                                    }
                                }
                            }
                            // Log for moderation
                        char *sh = "";
                        char *st = "";
                        char *sn = "";
                        if (nextPlayer->holdingID > 0) {
                            ObjectRecord* o = getObject(nextPlayer->holdingID);
                            sh = o->description;
                        }
                        if (target > 0) {
                            ObjectRecord* o = getObject(target);
                            st = o->description;
                        }
                        if (newGroundObject > 0) {
                            ObjectRecord* o = getObject(newGroundObject);
                            sn = o->description;
                        }
                        AppLog::infoF( "modLog id:%d account:%s %d %d USE x:%d y:%d h:%s(%d) t:%s(%d) n:%s(%d)",
                            nextPlayer->id,
                            nextPlayer->email,
                            nextPlayer->birthPos.x,
                            nextPlayer->birthPos.y,
                            m.x,
                            m.y,
                            sh,
                            nextPlayer->holdingID,
                            st,
                            target,
                            sn,
                            newGroundObject
                            );

                        }
                    else if( m.type == BABY ) {
                        playerIndicesToSendUpdatesAbout.push_back( i );
                        
                        if( computeAge( nextPlayer ) >= minPickupBabyAge 
                            &&
                            ( isGridAdjacent( m.x, m.y,
                                              nextPlayer->xd, 
                                              nextPlayer->yd ) 
                              ||
                              ( m.x == nextPlayer->xd &&
                                m.y == nextPlayer->yd ) ) ) {
                            
                            nextPlayer->actionAttempt = 1;
                            nextPlayer->actionTarget.x = m.x;
                            nextPlayer->actionTarget.y = m.y;
                            
                            if( m.x > nextPlayer->xd ) {
                                nextPlayer->facingOverride = 1;
                                }
                            else if( m.x < nextPlayer->xd ) {
                                nextPlayer->facingOverride = -1;
                                }


                            if( nextPlayer->holdingID == 0 ) {
                                // target location empty and 
                                // and our hands are empty
                                
                                // check if there's a baby to pick up there

                                // is anyone there?
                                LiveObject *hitPlayer = 
                                    getHitPlayer( m.x, m.y, m.id, 
                                                  false, babyAge );
                                
                                if( hitPlayer != NULL &&
                                    !hitPlayer->heldByOther &&
                                    computeAge( hitPlayer ) < babyAge  ) {
                                    
                                    // negative holding IDs to indicate
                                    // holding another player
                                    nextPlayer->holdingID = -hitPlayer->id;
                                    holdingSomethingNew( nextPlayer );
                                    
                                    nextPlayer->holdingEtaDecay = 0;

                                    hitPlayer->heldByOther = true;
                                    hitPlayer->heldByOtherID = nextPlayer->id;
                                    
                                    if( hitPlayer->heldByOtherID ==
                                        hitPlayer->parentID ) {
                                        hitPlayer->everHeldByParent = true;
                                        }
                                    

                                    // force baby to drop what they are
                                    // holding

                                    if( hitPlayer->holdingID != 0 ) {
                                        // never drop held wounds
                                        // they are the only thing a baby can
                                        // while held
                                        if( ! hitPlayer->holdingWound && 
                                            hitPlayer->holdingID > 0 ) {
                                            handleDrop( 
                                                m.x, m.y, hitPlayer,
                                             &playerIndicesToSendUpdatesAbout );
                                            }
                                        }
                                    
                                    if( hitPlayer->xd != hitPlayer->xs
                                        ||
                                        hitPlayer->yd != hitPlayer->ys ) {
                                        
                                        // force baby to stop moving
                                        hitPlayer->xd = m.x;
                                        hitPlayer->yd = m.y;
                                        hitPlayer->xs = m.x;
                                        hitPlayer->ys = m.y;
                                        
                                        // but don't send an update
                                        // about this
                                        // (everyone will get the pick-up
                                        //  update for the holding adult)
                                        }
                                    
                                    // if adult fertile female, baby auto-fed
                                    if( isFertileAge( nextPlayer ) ) {
                                        
                                        hitPlayer->foodStore = 
                                            computeFoodCapacity( hitPlayer );
                
                                        hitPlayer->foodUpdate = true;
                                        hitPlayer->responsiblePlayerID =
                                            nextPlayer->id;
                                        
                                        // reset their food decrement time
                                        hitPlayer->foodDecrementETASeconds =
                                            Time::getCurrentTime() +
                                            computeFoodDecrementTimeSeconds( 
                                                hitPlayer );
                                            
                                        checkForFoodEatingEmot( hitPlayer,
                                                                0 );

                                        // fixed cost to pick up baby
                                        // this still encourages baby-parent
                                        // communication so as not
                                        // to get the most mileage out of 
                                        // food
                                        int nurseCost = 1;
                                        
                                        if( nextPlayer->yummyBonusStore > 0 ) {
                                            nextPlayer->yummyBonusStore -= 
                                                nurseCost;
                                            nurseCost = 0;
                                            if( nextPlayer->yummyBonusStore < 
                                                0 ) {
                                                
                                                // not enough to cover full 
                                                // nurse cost

                                                // pass remaining nurse
                                                // cost onto main food store
                                                nurseCost = - nextPlayer->
                                                    yummyBonusStore;
                                                nextPlayer->yummyBonusStore = 0;
                                                }
                                            }
                                        

                                        nextPlayer->foodStore -= nurseCost;
                                        
                                        if( nextPlayer->foodStore < 0 ) {
                                            // catch mother death later
                                            // at her next food decrement
                                            nextPlayer->foodStore = 0;
                                            }
                                        // leave their food decrement
                                        // time alone
                                        nextPlayer->foodUpdate = true;
                                        }
                                    
                                    nextPlayer->heldOriginValid = 1;
                                    nextPlayer->heldOriginX = m.x;
                                    nextPlayer->heldOriginY = m.y;
                                    nextPlayer->heldTransitionSourceID = -1;
                                    }
                                
                                }
                            }
                        }
                    else if( m.type == SELF || m.type == UBABY ) {
                        playerIndicesToSendUpdatesAbout.push_back( i );
                        
                        char holdingFood = false;
                        char holdingDrugs = false;
                        
                        if( nextPlayer->holdingID > 0 ) {
                            ObjectRecord *obj = 
                                getObject( nextPlayer->holdingID );
                            
                            if( obj->foodValue > 0 || eatEverythingMode ) {
                                holdingFood = true;

                                if( strstr( obj->description, "noFeeding" )
                                    != NULL ) {
                                    // food that triggers effects cannot
                                    // be fed to other people
                                    holdingFood = false;
                                    holdingDrugs = true;
                                    }
                                }
                            }
                        
                        LiveObject *targetPlayer = NULL;
                        
                        if( nextPlayer->holdingID < 0 ) {
                            // holding a baby
                            // don't allow this action through
                            // keep targetPlayer NULL
                            }
                        else if( m.type == SELF ) {
                            if( m.x == nextPlayer->xd &&
                                m.y == nextPlayer->yd ) {
                                
                                // use on self
                                targetPlayer = nextPlayer;
                                }
                            }
                        else if( m.type == UBABY ) {
                            
                            if( isGridAdjacent( m.x, m.y,
                                                nextPlayer->xd, 
                                                nextPlayer->yd ) ||
                                ( m.x == nextPlayer->xd &&
                                  m.y == nextPlayer->yd ) ) {
                                

                                if( m.x > nextPlayer->xd ) {
                                    nextPlayer->facingOverride = 1;
                                    }
                                else if( m.x < nextPlayer->xd ) {
                                    nextPlayer->facingOverride = -1;
                                    }
                                
                                // try click on baby
                                int hitIndex;
                                LiveObject *hitPlayer = 
                                    getHitPlayer( m.x, m.y, m.id,
                                                  false, 
                                                  babyAge, -1, &hitIndex );
                                
                                if( hitPlayer != NULL && holdingDrugs ) {
                                    // can't even feed baby drugs
                                    // too confusing
                                    hitPlayer = NULL;
                                    }

                                if( false ) //food with noFeeding tag cannot be fed even to elderly
                                if( hitPlayer == NULL ||
                                    hitPlayer == nextPlayer ) {
                                    // try click on elderly
                                    hitPlayer = 
                                        getHitPlayer( m.x, m.y, m.id,
                                                      false, -1, 
                                                      55, &hitIndex );
                                    }
                                
                                if( ( hitPlayer == NULL ||
                                      hitPlayer == nextPlayer )
                                    &&
                                    holdingFood ) {
                                    
                                    // feeding action 
                                    // try click on everyone
                                    hitPlayer = 
                                        getHitPlayer( m.x, m.y, m.id,
                                                      false, -1, -1, 
                                                      &hitIndex );
                                    }
                                
                                
                                if( ( hitPlayer == NULL ||
                                      hitPlayer == nextPlayer )
                                    &&
                                    ! holdingDrugs ) {
                                    
                                    // see if clicked-on player is dying
                                    hitPlayer = 
                                        getHitPlayer( m.x, m.y, m.id,
                                                      false, -1, -1, 
                                                      &hitIndex );
                                    if( hitPlayer != NULL &&
                                        ! hitPlayer->dying ) {
                                        hitPlayer = NULL;
                                        }
                                    }
                                

                                if( hitPlayer != NULL &&
                                    hitPlayer != nextPlayer ) {
                                    
                                    targetPlayer = hitPlayer;
                                    
                                    playerIndicesToSendUpdatesAbout.push_back( 
                                        hitIndex );
                                    
                                    targetPlayer->responsiblePlayerID =
                                            nextPlayer->id;
                                    }
                                }
                            }
                        

                        if( targetPlayer != NULL ) {
                            
                            ObjectRecord **clothingSlot = 
                                getClothingSlot( targetPlayer, m.i );
                                
                            int targetClothingID = 0;
                            if( clothingSlot != NULL ) targetClothingID = ( *clothingSlot )->id;
                            char* sh = "";
                            char *st = "";
                            if (nextPlayer->holdingID > 0) {
                                ObjectRecord* o = getObject(nextPlayer->holdingID);
                                sh = o->description;
                            }
                            if (targetClothingID > 0) {
                                ObjectRecord* o = getObject(targetClothingID);
                                st = o->description;
                            }
                            // Log for moderation - cases other than this main one is not logged
                            AppLog::infoF( "modLog id:%d account:%s %d %d SELF x:%d y:%d h:%s(%d) t:%s(%d) %d",
                                nextPlayer->id,
                                nextPlayer->email,
                                nextPlayer->birthPos.x,
                                nextPlayer->birthPos.y,
                                m.x,
                                m.y,
                                sh,
                                nextPlayer->holdingID,
                                st,
                                targetClothingID,
                                m.i
                                );
                            
                            // use on self/baby
                            nextPlayer->actionAttempt = 1;
                            nextPlayer->actionTarget.x = m.x;
                            nextPlayer->actionTarget.y = m.y;
                            

                            if( targetPlayer != nextPlayer &&
                                targetPlayer->dying &&
                                ! holdingFood ) {
                                
                                // try healing wound
                                    
                                TransRecord *healTrans =
                                    getMetaTrans( nextPlayer->holdingID,
                                                  targetPlayer->holdingID );
                                
                                char oldEnough = true;

                                if( healTrans != NULL ) {
                                    int healerWillHold = healTrans->newActor;
                                    
                                    if( healerWillHold > 0 ) {
                                        if( computeAge( nextPlayer ) < 
                                            getObject( healerWillHold )->
                                            minPickupAge ) {
                                            oldEnough = false;
                                            }
                                        }
                                    }
                                

                                if( oldEnough && healTrans != NULL ) {
                                    targetPlayer->holdingID =
                                        healTrans->newTarget;
                                    holdingSomethingNew( targetPlayer );
                                    
                                    // their wound has been changed
                                    // no longer track embedded weapon
                                    targetPlayer->embeddedWeaponID = 0;
                                    targetPlayer->embeddedWeaponEtaDecay = 0;
                                    
                                    
                                    nextPlayer->holdingID = 
                                        healTrans->newActor;
                                    holdingSomethingNew( nextPlayer );
                                    
                                    setFreshEtaDecayForHeld( 
                                        nextPlayer );
                                    setFreshEtaDecayForHeld( 
                                        targetPlayer );
                                    
                                    nextPlayer->heldOriginValid = 0;
                                    nextPlayer->heldOriginX = 0;
                                    nextPlayer->heldOriginY = 0;
                                    nextPlayer->heldTransitionSourceID = 
                                        healTrans->target;
                                    
                                    targetPlayer->heldOriginValid = 0;
                                    targetPlayer->heldOriginX = 0;
                                    targetPlayer->heldOriginY = 0;
                                    targetPlayer->heldTransitionSourceID = 
                                        -1;
                                    
                                    if( targetPlayer->holdingID == 0 ) {
                                        // not dying anymore
                                        setNoLongerDying( 
                                            targetPlayer,
                                            &playerIndicesToSendHealingAbout );
                                        }
                                    else {
                                        // wound changed?

                                        ForcedEffects e = 
                                            checkForForcedEffects( 
                                                targetPlayer->holdingID );
                            
                                        if( e.emotIndex != -1 ) {
                                            targetPlayer->emotFrozen = true;
                                            targetPlayer->emotFrozenIndex =
                                                e.emotIndex;
                                            newEmotPlayerIDs.push_back( 
                                                targetPlayer->id );
                                            newEmotIndices.push_back( 
                                                e.emotIndex );
                                            newEmotTTLs.push_back( e.ttlSec );
                                            interruptAnyKillEmots( 
                                                targetPlayer->id, e.ttlSec );
                                            }
                                        if( e.foodCapModifier != 1 ) {
                                            targetPlayer->foodCapModifier = 
                                                e.foodCapModifier;
                                            targetPlayer->foodUpdate = true;
                                            }
                                        if( e.feverSet ) {
                                            targetPlayer->fever = e.fever;
                                            }
                                        }
                                    }
                                }
                            else if( targetPlayer == nextPlayer &&
                                     nextPlayer->dying &&
                                     m.i >= 0 && 
                                     m.i < NUM_CLOTHING_PIECES ) {
                                
                                ObjectRecord *clickedClothing = 
                                    clothingByIndex( nextPlayer->clothing, 
                                                     m.i );
                                                     
                                int clickedClothingID = 0;
                                
                                if( clickedClothing != NULL ) {
                                    clickedClothingID = clickedClothing->id;
                                }
                                
                                bool healed = false;
                                    
                                // try healing wound
                                
                                TransRecord *healTrans =
                                    getMetaTrans( nextPlayer->holdingID,
                                                  clickedClothingID );
                                
                                int healTarget = 0;

                                if( healTrans != NULL ) {
                                    
                                    nextPlayer->holdingID = 
                                        healTrans->newActor;
                                    holdingSomethingNew( nextPlayer );
                                    
                                    // their wound has been changed
                                    // no longer track embedded weapon
                                    nextPlayer->embeddedWeaponID = 0;
                                    nextPlayer->embeddedWeaponEtaDecay = 0;
                                                  
                                    setClothingByIndex( 
                                        &( nextPlayer->clothing ), 
                                        m.i,
                                        getObject( 
                                            healTrans->newTarget ) );
                                    
                                    setResponsiblePlayer( -1 );
                                    
                                    healed = true;
                                    healTarget = healTrans->target;
                                    
                                    }
                                
                                if ( healed ) {
                                    
                                    nextPlayer->heldOriginValid = 0;
                                    nextPlayer->heldOriginX = 0;
                                    nextPlayer->heldOriginY = 0;
                                    nextPlayer->heldTransitionSourceID = 
                                        healTarget;
                                    
                                    if( nextPlayer->holdingID == 0 ) {
                                        // not dying anymore
                                        setNoLongerDying( 
                                            nextPlayer,
                                            &playerIndicesToSendHealingAbout );
                                        }
                                    else {
                                        // wound changed?

                                        ForcedEffects e = 
                                            checkForForcedEffects( 
                                                nextPlayer->holdingID );

                                        if( e.emotIndex != -1 ) {
                                            nextPlayer->emotFrozen = true;
                                            newEmotPlayerIDs.push_back( 
                                                nextPlayer->id );
                                            newEmotIndices.push_back( 
                                                e.emotIndex );
                                            newEmotTTLs.push_back( e.ttlSec );
                                            interruptAnyKillEmots( 
                                                nextPlayer->id, e.ttlSec );
                                            }
                                        if( e.foodCapModifier != 1 ) {
                                            nextPlayer->foodCapModifier = 
                                                e.foodCapModifier;
                                            nextPlayer->foodUpdate = true;
                                            }
                                        if( e.feverSet ) {
                                            nextPlayer->fever = e.fever;
                                            }
                                        }
                                    }
                                }
                            else if( nextPlayer->holdingID > 0 ) {
                                ObjectRecord *obj = 
                                    getObject( nextPlayer->holdingID );
                                
                                // don't use "live" computed capacity here
                                // because that will allow player to spam
                                // click to pack in food between food
                                // decrements when they are growing
                                // instead, stick to the food cap shown
                                // in the client (what we last reported
                                // to them)
                                int cap = targetPlayer->lastReportedFoodCapacity;
                                

                                // first case:
                                // player clicked on clothing
                                // try adding held into clothing, but if
                                // that fails go on to other cases

                                // except do not force them to eat
                                // something that could have gone
                                // into a full clothing container!
                                char couldHaveGoneIn = false;

                                ObjectRecord *clickedClothing = NULL;
                                TransRecord *clickedClothingTrans = NULL;
                                
                                // is this clickedClothingTrans a containment transition? 
                                char isContainmentTransition = false;
                                
                                if( m.i >= 0 &&
                                    m.i < NUM_CLOTHING_PIECES ) {
                                    clickedClothing =
                                        clothingByIndex( nextPlayer->clothing,
                                                         m.i );
                                    
                                    if( clickedClothing != NULL ) {
                                        
                                        clickedClothingTrans =
                                            getPTrans( nextPlayer->holdingID,
                                                       clickedClothing->id );
                                                       
                                        // Check for containment transitions - clickedClothing
                                        if( clickedClothingTrans == NULL ) {
                                            clickedClothingTrans =
                                                getPTrans( nextPlayer->holdingID,
                                                           clickedClothing->id, false, false, 1 );
                                            isContainmentTransition = true;
                                            }
                                        
                                        if( clickedClothingTrans == NULL ) {
                                            // check if held has instant-decay
                                            TransRecord *heldDecay = 
                                                getPTrans( 
                                                    -1, 
                                                    nextPlayer->holdingID );
                                            if( heldDecay != NULL &&
                                                heldDecay->autoDecaySeconds
                                                == 1 &&
                                                heldDecay->newTarget > 0 ) {
                                                
                                                // force decay NOW and try again
                                                handleHeldDecay(
                                                nextPlayer,
                                                i,
                                                &playerIndicesToSendUpdatesAbout,
                                                &playerIndicesToSendHealingAbout );
                                                clickedClothingTrans =
                                                    getPTrans( 
                                                        nextPlayer->holdingID,
                                                        clickedClothing->id );
                                                }
                                            }
                                        

                                        if( clickedClothingTrans != NULL ) {
                                            int na =
                                                clickedClothingTrans->newActor;
                                            
                                            if( na > 0 &&
                                                getObject( na )->minPickupAge >
                                                computeAge( nextPlayer ) ) {
                                                // too young for trans
                                                clickedClothingTrans = NULL;
                                                }

                                            int nt = 
                                                clickedClothingTrans->newTarget;
                                            
                                            if( nt > 0 &&
                                                getObject( nt )->clothing 
                                                != clickedClothing->clothing ) {
                                                // don't allow transitions
                                                // that leave a non-wearable
                                                // item on your body
                                                // OR convert clothing into
                                                // a different type of clothing
                                                // (converting a shrit to a hat)
                                                clickedClothingTrans = NULL;
                                                }
                                            }
                                        }
                                    }
                                

                                if( targetPlayer == nextPlayer &&
                                    m.i >= 0 && 
                                    m.i < NUM_CLOTHING_PIECES &&
                                    addHeldToClothingContainer( 
                                        nextPlayer,
                                        m.i,
                                        false,
                                        &couldHaveGoneIn) ) {
                                    // worked!
                                    }
                                // next case:  can what they're holding
                                // be used to transform clothing?
                                else if( m.i >= 0 &&
                                         m.i < NUM_CLOTHING_PIECES &&
                                         clickedClothing != NULL &&
                                         clickedClothingTrans != NULL ) {
                                    
                                    // NOTE:
                                    // this is a niave way of handling
                                    // this case, and it doesn't deal
                                    // with all kinds of possible complexities
                                    // (like if the clothing decay time should
                                    //  change, or number of slots change)
                                    // Assuming that we won't add transitions
                                    // for clothing with those complexities
                                    // Works for removing sword
                                    // from backpack

                                    handleHoldingChange(
                                        nextPlayer,
                                        clickedClothingTrans->newActor );
                                    
                                    setClothingByIndex( 
                                        &( nextPlayer->clothing ), 
                                        m.i,
                                        getObject( 
                                            clickedClothingTrans->newTarget ) );
                                            
                                    // Execute containment transitions - clickedClothing
                                    if( isContainmentTransition ) {
                                        addHeldToClothingContainer( 
                                            nextPlayer,
                                            m.i,
                                            false,
                                            &couldHaveGoneIn, true);
                                        }
                                            
                                    }
                                // next case, holding food
                                // that couldn't be put into clicked clothing
                                else if( (obj->foodValue > 0 || obj->bonusValue > 0 || eatEverythingMode) && 
                                         (targetPlayer->foodStore < cap || strstr( obj->description, "modTool")) &&
                                         ! couldHaveGoneIn ) {
                                    
                                    targetPlayer->justAte = true;
                                    targetPlayer->justAteID = 
                                        nextPlayer->holdingID;

                                    targetPlayer->lastAteID = 
                                        nextPlayer->holdingID;
                                    targetPlayer->lastAteFillMax =
                                        targetPlayer->foodStore;
                                    
                                    int bonus = 0;
                                    
                                    if ( eatEverythingMode ) {
                                        // set the sustenance of everything to 1
                                        targetPlayer->foodStore += 1;
                                    }
                                    else {
                                        targetPlayer->foodStore += obj->foodValue;

                                        bonus = getEatBonus( targetPlayer );
                                    }
                                    
                                    updateYum( targetPlayer, obj->id,
                                               targetPlayer == nextPlayer );

                                    logEating( obj->id,
                                               obj->foodValue + bonus,
                                               computeAge( targetPlayer ),
                                               m.x, m.y );
                                    
                                    targetPlayer->foodStore += bonus;

                                    checkForFoodEatingEmot( targetPlayer,
                                                            obj->id );
                                    
                                    if( targetPlayer->foodStore > cap ) {
                                        int over = 
                                            targetPlayer->foodStore - cap;
                                        
                                        targetPlayer->foodStore = cap;

                                        targetPlayer->yummyBonusStore += over;
                                        }
                                        
                                    targetPlayer->foodDecrementETASeconds =
                                        Time::getCurrentTime() +
                                        computeFoodDecrementTimeSeconds( 
                                            targetPlayer );
                                    
                                    // get eat transtion
                                    TransRecord *r = 
                                        getPTrans( nextPlayer->holdingID, 
                                                  -1 );

                                    

                                    if( r != NULL ) {
                                        int oldHolding = nextPlayer->holdingID;
                                        nextPlayer->holdingID = r->newActor;
                                        holdingSomethingNew( nextPlayer,
                                                             oldHolding );

                                        if( oldHolding !=
                                            nextPlayer->holdingID ) {
                                            
                                            setFreshEtaDecayForHeld( 
                                                nextPlayer );
                                            }
                                        }
                                    else {
                                        // default, holding nothing after eating
                                        nextPlayer->holdingID = 0;
                                        nextPlayer->holdingEtaDecay = 0;
                                        }
                                    
                                    if( obj->alcohol != 0 ) {
                                        drinkAlcohol( targetPlayer,
                                                      obj->alcohol );
                                        }
                                        
                                    if( strstr( obj->description, "+drug" ) != NULL ) {
                                        doDrug( targetPlayer );
                                        }


                                    nextPlayer->heldOriginValid = 0;
                                    nextPlayer->heldOriginX = 0;
                                    nextPlayer->heldOriginY = 0;
                                    nextPlayer->heldTransitionSourceID = -1;
                                    
                                    targetPlayer->foodUpdate = true;
                                    }
                                // final case, holding clothing that
                                // we could put on
                                else if( obj->clothing != 'n' &&
                                         ( targetPlayer == nextPlayer
                                           || 
                                           computeAge( targetPlayer ) < 
                                           babyAge) ) {
                                    
                                    // wearable, dress self or baby
                                    
                                    nextPlayer->holdingID = 0;
                                    timeSec_t oldEtaDecay = 
                                        nextPlayer->holdingEtaDecay;
                                    
                                    nextPlayer->holdingEtaDecay = 0;
                                    
                                    nextPlayer->heldOriginValid = 0;
                                    nextPlayer->heldOriginX = 0;
                                    nextPlayer->heldOriginY = 0;
                                    nextPlayer->heldTransitionSourceID = -1;
                                    
                                    ObjectRecord *oldC = NULL;
                                    timeSec_t oldCEtaDecay = 0;
                                    int oldNumContained = 0;
                                    int *oldContainedIDs = NULL;
                                    timeSec_t *oldContainedETADecays = NULL;
                                    

                                    ObjectRecord **clothingSlot = NULL;
                                    int clothingSlotIndex;

                                    switch( obj->clothing ) {
                                        case 'h':
                                            clothingSlot = 
                                                &( targetPlayer->clothing.hat );
                                            clothingSlotIndex = 0;
                                            break;
                                        case 't':
                                            clothingSlot = 
                                              &( targetPlayer->clothing.tunic );
                                            clothingSlotIndex = 1;
                                            break;
                                        case 'b':
                                            clothingSlot = 
                                                &( targetPlayer->
                                                   clothing.bottom );
                                            clothingSlotIndex = 4;
                                            break;
                                        case 'p':
                                            clothingSlot = 
                                                &( targetPlayer->
                                                   clothing.backpack );
                                            clothingSlotIndex = 5;
                                            break;
                                        case 's':
                                            if( targetPlayer->clothing.backShoe
                                                == NULL ) {

                                                clothingSlot = 
                                                    &( targetPlayer->
                                                       clothing.backShoe );
                                                clothingSlotIndex = 3;

                                                }
                                            else if( 
                                                targetPlayer->clothing.frontShoe
                                                == NULL ) {
                                                
                                                clothingSlot = 
                                                    &( targetPlayer->
                                                       clothing.frontShoe );
                                                clothingSlotIndex = 2;
                                                }
                                            else {
                                                // replace whatever shoe
                                                // doesn't match what we're
                                                // holding
                                                
                                                if( targetPlayer->
                                                    clothing.backShoe == 
                                                    obj ) {
                                                    
                                                    clothingSlot = 
                                                        &( targetPlayer->
                                                           clothing.frontShoe );
                                                    clothingSlotIndex = 2;
                                                    }
                                                else if( targetPlayer->
                                                         clothing.frontShoe == 
                                                         obj ) {
                                                    clothingSlot = 
                                                        &( targetPlayer->
                                                           clothing.backShoe );
                                                    clothingSlotIndex = 3;
                                                    }
                                                else {
                                                    // both shoes are
                                                    // different from what
                                                    // we're holding
                                                    
                                                    // pick shoe to swap
                                                    // based on what we
                                                    // clicked on
                                                    
                                                    if( m.i == 3 ) {
                                                        clothingSlot = 
                                                        &( targetPlayer->
                                                           clothing.backShoe );
                                                        clothingSlotIndex = 3;
                                                        }
                                                    else {
                                                        // default to front
                                                        // shoe
                                                        clothingSlot = 
                                                        &( targetPlayer->
                                                           clothing.frontShoe );
                                                        clothingSlotIndex = 2;
                                                        }
                                                    }
                                                }
                                            break;
                                        }
                                    
                                    if( clothingSlot != NULL ) {
                                        
                                        oldC = *clothingSlot;
                                        int ind = clothingSlotIndex;
                                        
                                        oldCEtaDecay = 
                                            targetPlayer->clothingEtaDecay[ind];
                                        
                                        oldNumContained = 
                                            targetPlayer->
                                            clothingContained[ind].size();
                                        
                                        if( oldNumContained > 0 ) {
                                            oldContainedIDs = 
                                                targetPlayer->
                                                clothingContained[ind].
                                                getElementArray();
                                            oldContainedETADecays =
                                                targetPlayer->
                                                clothingContainedEtaDecays[ind].
                                                getElementArray();
                                            }
                                        
                                        *clothingSlot = obj;
                                        targetPlayer->clothingEtaDecay[ind] =
                                            oldEtaDecay;
                                        
                                        targetPlayer->
                                            clothingContained[ind].
                                            deleteAll();
                                        targetPlayer->
                                            clothingContainedEtaDecays[ind].
                                            deleteAll();
                                            
                                        if( nextPlayer->numContained > 0 ) {
                                            
                                            targetPlayer->clothingContained[ind]
                                                .appendArray( 
                                                    nextPlayer->containedIDs,
                                                    nextPlayer->numContained );

                                            targetPlayer->
                                                clothingContainedEtaDecays[ind]
                                                .appendArray( 
                                                    nextPlayer->
                                                    containedEtaDecays,
                                                    nextPlayer->numContained );
                                                

                                            // ignore sub-contained
                                            // because clothing can
                                            // never contain containers
                                            clearPlayerHeldContained( 
                                                nextPlayer );
                                            }
                                            
                                        
                                        if( oldC != NULL ) {
                                            nextPlayer->holdingID =
                                                oldC->id;
                                            holdingSomethingNew( nextPlayer );
                                            
                                            nextPlayer->holdingEtaDecay
                                                = oldCEtaDecay;
                                            
                                            nextPlayer->numContained =
                                                oldNumContained;
                                            
                                            freePlayerContainedArrays(
                                                nextPlayer );
                                            
                                            nextPlayer->containedIDs =
                                                oldContainedIDs;
                                            nextPlayer->containedEtaDecays =
                                                oldContainedETADecays;
                                            
                                            // empty sub-contained vectors
                                            // because clothing never
                                            // never contains containers
                                            nextPlayer->subContainedIDs
                                                = new SimpleVector<int>[
                                                    nextPlayer->numContained ];
                                            nextPlayer->subContainedEtaDecays
                                                = new SimpleVector<timeSec_t>[
                                                    nextPlayer->numContained ];
                                            }
                                        }
                                    }
                                }         
                            else {
                                // empty hand on self/baby, remove clothing

                                int clothingSlotIndex = m.i;
                                
                                ObjectRecord **clothingSlot = 
                                    getClothingSlot( targetPlayer, m.i );
                                
                                
                                TransRecord *bareHandClothingTrans =
                                    getBareHandClothingTrans( nextPlayer,
                                                              clothingSlot );
                                

                                if( targetPlayer == nextPlayer &&
                                    bareHandClothingTrans != NULL ) {
                                    
                                    // bare hand transforms clothing
                                    
                                    // this may not handle all possible cases
                                    // correctly.  A naive implementation for
                                    // now.  Works for removing sword
                                    // from backpack

                                    handleHoldingChange( 
                                        nextPlayer,
                                        bareHandClothingTrans->newActor );
                                    
                                    nextPlayer->heldOriginValid = 0;
                                    nextPlayer->heldOriginX = 0;
                                    nextPlayer->heldOriginY = 0;
                                    

                                    if( bareHandClothingTrans->newTarget > 0 ) {
                                        *clothingSlot = 
                                            getObject( bareHandClothingTrans->
                                                       newTarget );
                                        }
                                    else {
                                        *clothingSlot = NULL;
                                        
                                        int ind = clothingSlotIndex;
                                        
                                        targetPlayer->clothingEtaDecay[ind] = 0;
                                        
                                        targetPlayer->clothingContained[ind].
                                            deleteAll();
                                        
                                        targetPlayer->
                                            clothingContainedEtaDecays[ind].
                                            deleteAll();
                                        }
                                    }
                                else if( clothingSlot != NULL ) {
                                    // bare hand removes clothing
                                    
                                    removeClothingToHold( nextPlayer,
                                                          targetPlayer,
                                                          clothingSlot,
                                                          clothingSlotIndex );
                                    }
                                }
                            }
                        }                    
                    else if( m.type == DROP ) {
                        //Thread::staticSleep( 2000 );
                        
                        // send update even if action fails (to let them
                        // know that action is over)
                        playerIndicesToSendUpdatesAbout.push_back( i );

                        char canDrop = true;
                        
                        if( nextPlayer->holdingID > 0 &&
                            getObject( nextPlayer->holdingID )->permanent ) {
                            canDrop = false;
                            }
                            
                        int target = getMapObject( m.x, m.y );
                        
                        if( nextPlayer->holdingID > 0 &&
                            getObject( nextPlayer->holdingID )->useDistance == 0 &&
                            ( m.x != nextPlayer->xd ||
                              m.y != nextPlayer->yd ) ) {
                            // trying to drop a 0-useDistance object
                            // while not standing on
                            // the exact same tile, blocked
                            canDrop = false;
                            }

                        char *sh = "";
                        char *st = "";
                        if (nextPlayer->holdingID > 0) {
                            ObjectRecord *o = getObject(nextPlayer->holdingID);
                            sh = o->description;
                        }
                        if (target > 0) {
                            ObjectRecord *o = getObject(target);
                            st = o->description;
                        }
                        AppLog::infoF( "modLog id:%d account:%s %d %d DROP x:%d y:%d h:%s(%d) t:%s(%d)",
                            nextPlayer->id,
                            nextPlayer->email,
                            nextPlayer->birthPos.x,
                            nextPlayer->birthPos.y,
                            m.x,
                            m.y,
                            sh,
                            nextPlayer->holdingID,
                            st,
                            target
                            );
                        
                        char accessBlocked = 
                            isAccessBlocked( nextPlayer, 
                                             m.x, m.y, target );
                        
                        
                        if( ! accessBlocked )
                        if( ( isGridAdjacent( m.x, m.y,
                                              nextPlayer->xd, 
                                              nextPlayer->yd ) 
                              ||
                              ( m.x == nextPlayer->xd &&
                                m.y == nextPlayer->yd )  ) ) {
                            
                            nextPlayer->actionAttempt = 1;
                            nextPlayer->actionTarget.x = m.x;
                            nextPlayer->actionTarget.y = m.y;
                            
                            if( m.x > nextPlayer->xd ) {
                                nextPlayer->facingOverride = 1;
                                }
                            else if( m.x < nextPlayer->xd ) {
                                nextPlayer->facingOverride = -1;
                                }

                            if( nextPlayer->holdingID != 0 ) {
                                
                                if( nextPlayer->holdingID < 0 ) {
                                    // baby drop
                                    int target = getMapObject( m.x, m.y );
                                    
                                    if( target == 0 // nothing here
                                        ||
                                        ! getObject( target )->
                                            blocksWalking ) {
                                        handleDrop( 
                                            m.x, m.y, nextPlayer,
                                            &playerIndicesToSendUpdatesAbout );
                                        }    
                                    }
                                else if( canDrop && 
                                         isMapSpotEmpty( m.x, m.y ) ) {
                                
                                    // empty spot to drop non-baby into
                                    
                                    handleDrop( 
                                        m.x, m.y, nextPlayer,
                                        &playerIndicesToSendUpdatesAbout );
                                    }
                                else if( canDrop &&
                                         m.c >= 0 && 
                                         m.c < NUM_CLOTHING_PIECES &&
                                         m.x == nextPlayer->xd &&
                                         m.y == nextPlayer->yd  &&
                                         nextPlayer->holdingID > 0 ) {
                                    
                                    // drop into clothing indicates right-click
                                    // so swap
                                    
                                    // first add to top of container
                                    // if possible
                                    addHeldToClothingContainer( nextPlayer,
                                                                m.c,
                                                                true );
                                    if( nextPlayer->holdingID == 0 ) {
                                        // add to top worked

                                        double playerAge = 
                                            computeAge( nextPlayer );
                                        
                                        // now take off bottom to hold
                                        // but keep looking to find something
                                        // different than what we were
                                        // holding before
                                        // AND that we are old enough to pick
                                        // up
                                        for( int s=0; 
                                             s < nextPlayer->
                                                 clothingContained[m.c].size() 
                                                 - 1;
                                             s++ ) {
                                            
                                            int otherID =
                                                nextPlayer->
                                                clothingContained[m.c].
                                                getElementDirect( s );
                                                
                                            int oldHeldAfterTrans =
                                                nextPlayer->
                                                clothingContained[m.c].
                                                getElementDirect( nextPlayer->clothingContained[m.c].size() - 1 );
                                            
                                            TransRecord *pickUpTrans = getPTrans( 0, otherID );
                                            bool hasPickUpTrans = 
                                                pickUpTrans != NULL && 
                                                pickUpTrans->newTarget == 0;
                                            
                                            if( otherID != 
                                                oldHeldAfterTrans &&
                                                getObject( otherID )->
                                                minPickupAge <= playerAge &&
                                                ( !getObject( otherID )->permanent || 
                                                hasPickUpTrans )
                                                ) {
                                                
                                              removeFromClothingContainerToHold(
                                                    nextPlayer, m.c, s, true );
                                                break;
                                                }
                                            }
                                        
                                        // check to make sure remove worked
                                        // (otherwise swap failed)
                                        ObjectRecord *cObj = 
                                            clothingByIndex( 
                                                nextPlayer->clothing, m.c );
                                        if( nextPlayer->clothingContained[m.c].
                                            size() > cObj->numSlots ) {
                                            
                                            // over-full, remove failed
                                            
                                            // pop top item back off into hand
                                            removeFromClothingContainerToHold(
                                                nextPlayer, m.c, 
                                                nextPlayer->
                                                clothingContained[m.c].
                                                size() - 1, true );
                                            }
                                        }
                                    
                                    }
                                else if( nextPlayer->holdingID > 0 ) {
                                    // non-baby drop
                                    
                                    ObjectRecord *droppedObj
                                        = getObject( 
                                            nextPlayer->holdingID );
                                    
                                    int target = getMapObject( m.x, m.y );
                            
                                    if( target != 0 ) {
                                        
                                        ObjectRecord *targetObj =
                                            getObject( target );
                                        

                                        if( !canDrop ) {
                                            // user may have a permanent object
                                            // stuck in their hand with no place
                                            // to drop it
                                            
                                            // need to check if 
                                            // a use-on-bare-ground
                                            // transition applies.  If so, we
                                            // can treat it like a swap

                                    
                                            if( ! targetObj->permanent 
                                                && getObject( targetObj->id )->minPickupAge < computeAge( nextPlayer ) ) {
                                                // target can be picked up

                                                // "set-down" type bare ground 
                                                // trans exists?
                                                TransRecord
                                                *r = getPTrans( 
                                                    nextPlayer->holdingID, 
                                                    -1 );

                                                if( r != NULL && 
                                                    r->newActor == 0 &&
                                                    r->newTarget > 0 ) {
                                            
                                                    // only applies if the 
                                                    // bare-ground
                                                    // trans leaves nothing in
                                                    // our hand
                                                    
                                                    // now swap it with the 
                                                    // non-permanent object
                                                    // on the ground.

                                                    swapHeldWithGround( 
                                                        nextPlayer,
                                                        target,
                                                        m.x,
                                                        m.y,
                                            &playerIndicesToSendUpdatesAbout );
                                                    }
                                                }
                                            }


                                        int targetSlots =
                                            targetObj->numSlots;
                                        
                                        char canGoIn = false;
                                        
                                        if( canDrop &&
                                            containmentPermitted( 
                                                target,
                                                droppedObj->id ) ) {
                                            canGoIn = true;
                                            }
                                        
                                        char forceUse = false;
                                        
                                        if( canDrop && 
                                            canGoIn &&
                                            targetSlots > 0 &&
                                            nextPlayer->numContained == 0 &&
                                            getNumContained( m.x, m.y ) == 0 ) {
                                            
                                            // container empty
                                            // is there a transition that might
                                            // apply instead?
                                            
                                            // only consider a consuming
                                            // transition (custom containment
                                            // like grapes in a basket which
                                            // aren't in container slots )

                                            TransRecord *t = 
                                                getPTrans( 
                                                    nextPlayer->holdingID, 
                                                    target );
                                            
                                            if( t != NULL && 
                                                t->newActor == 0 ) {
                                                forceUse = true;
                                                }
                                            }
                                        
                                        
                                        bool containerAllowSwap = !targetObj->slotsNoSwap;
                                        
                                        bool targetIsTrulyPermanent = false;
                                        if( targetObj->permanent ) {
                                            // target is permanent
                                            // consider swapping if target has a pick-up transition
                                            TransRecord *pickupTrans = getPTrans( 0, targetObj->id );
                                            bool targetHasPickupTrans = 
                                                pickupTrans != NULL &&
                                                pickupTrans->newActor != 0 &&
                                                pickupTrans->newTarget == 0;
                                            targetIsTrulyPermanent = !targetHasPickupTrans;
                                            }
                                        
                                        // DROP indicates they 
                                        // right-clicked on container
                                        // so use swap mode
                                        if( canDrop && 
                                            canGoIn &&
                                            ! forceUse &&
                                            addHeldToContainer( 
                                                nextPlayer,
                                                target,
                                                m.x, m.y, containerAllowSwap ) ) {
                                            // handled
                                            }
                                        else if( forceUse ||
                                                 ( canDrop && 
                                                   ! canGoIn &&
                                                   targetIsTrulyPermanent &&
                                                   nextPlayer->numContained 
                                                   == 0 ) ) {
                                            // try treating it like
                                            // a USE action
                                            m.type = USE;
                                            m.id = -1;
                                            m.c = -1;
                                            playerIndicesToSendUpdatesAbout.
                                                deleteElementEqualTo( i );
                                            goto RESTART_MESSAGE_ACTION;
                                            }
                                        else if( canDrop && 
                                                 ! canGoIn &&
                                                 ! targetIsTrulyPermanent 
                                                 &&
                                                 canPickup( 
                                                     targetObj->id,
                                                     computeAge( 
                                                         nextPlayer ) ) ) {
                                            // drop onto a spot where
                                            // something exists, and it's
                                            // not a container

                                            // swap what we're holding for
                                            // target
                                            
                                            int oldHeld = 
                                                nextPlayer->holdingID;
                                            int oldNumContained =
                                                nextPlayer->numContained;
                                            
                                            // now swap
                                            swapHeldWithGround( 
                                             nextPlayer, target, m.x, m.y,
                                             &playerIndicesToSendUpdatesAbout );
                                            
                                            if( oldHeld == 
                                                nextPlayer->holdingID &&
                                                oldNumContained ==
                                                nextPlayer->numContained ) {
                                                // no change
                                                // are they the same object?
                                                if( oldNumContained == 0 && 
                                                    oldHeld == target ) {
                                                    // try using empty held
                                                    // on target
                                                    TransRecord *sameTrans
                                                        = getPTrans(
                                                            oldHeld, target );
                                                    if( sameTrans != NULL &&
                                                        sameTrans->newActor ==
                                                        0 ) {
                                                        // keep it simple
                                                        // for now
                                                        // this is usually
                                                        // just about
                                                        // stacking
                                                        handleHoldingChange(
                                                            nextPlayer,
                                                            sameTrans->
                                                            newActor );
                                                        
                                                        setMapObject(
                                                            m.x, m.y,
                                                            sameTrans->
                                                            newTarget );
                                                        }
                                                    else {
                                                        
                                                        // try containment transitions - DROP stacking
                                                        
                                                        TransRecord *contTrans
                                                            = getPTrans(
                                                                oldHeld, target, false, false, 1 );
                                                        if( contTrans != NULL ) {
                                                            handleHoldingChange(
                                                                nextPlayer,
                                                                0 );
                                                            
                                                            setMapObject(
                                                                m.x, m.y,
                                                                contTrans->
                                                                newTarget );
                                                                
                                                            addContained( 
                                                                m.x, m.y,
                                                                contTrans->
                                                                newActor,
                                                                nextPlayer->holdingEtaDecay );
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    else if( canDrop ) {
                                        // no object here
                                        
                                        // maybe there's a person
                                        // standing here

                                        // only allow drop if what we're
                                        // dropping is non-blocking
                                        
                                        
                                        if( ! droppedObj->blocksWalking ) {
                                            
                                             handleDrop( 
                                              m.x, m.y, nextPlayer,
                                              &playerIndicesToSendUpdatesAbout 
                                              );
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    else if( m.type == REMV ) {
                        // send update even if action fails (to let them
                        // know that action is over)
                        playerIndicesToSendUpdatesAbout.push_back( i );
                        
                        int target = getMapObject( m.x, m.y );

                        char *sh = "";
                        char *st = "";
                        if (nextPlayer->holdingID > 0) {
                            ObjectRecord *o = getObject(nextPlayer->holdingID);
                            sh = o->description;
                        }
                        if (target > 0) {
                            ObjectRecord *o = getObject(target);
                            st = o->description;
                        }

                        // Log for moderation
                        AppLog::infoF( "modLog id:%d account:%s %d %d REMV x:%d y:%d h:%s(%d) t:%s(%d)",
                            nextPlayer->id,
                            nextPlayer->email,
                            nextPlayer->birthPos.x,
                            nextPlayer->birthPos.y,
                            m.x,
                            m.y,
                            sh,
                            nextPlayer->holdingID,
                            st,
                            target
                            );
                        
                        if( isGridAdjacent( m.x, m.y, 
                                            nextPlayer->xd, 
                                            nextPlayer->yd ) 
                            ||
                            ( m.x == nextPlayer->xd &&
                              m.y == nextPlayer->yd ) ) {
                            
                            //2HOL mechanics to read written objects
                            if( target > 0 ) {
                                ObjectRecord *targetObj = 
                                    getObject( target );

                                if( targetObj->written &&
                                    targetObj->clickToRead ) {
                                    GridPos readPos = { m.x, m.y };
                                    forceObjectToRead( nextPlayer, target, readPos, false );
                                    }
                                }

                            char accessBlocked =
                                isAccessBlocked( nextPlayer, m.x, m.y, target );
                            
                            
                            char handEmpty = ( nextPlayer->holdingID == 0 );
                            
                            if( ! accessBlocked ) 
                            removeFromContainerToHold( nextPlayer,
                                                       m.x, m.y, m.i );

                            if( ! accessBlocked ) 
                            if( handEmpty &&
                                nextPlayer->holdingID == 0 ) {
                                // hand still empty?
                            
                                int target = getMapObject( m.x, m.y );

                                if( target > 0 ) {
                                    ObjectRecord *targetObj = 
                                        getObject( target );
                                
                                    if( ! targetObj->permanent &&
                                        targetObj->minPickupAge <= 
                                        computeAge( nextPlayer ) ) {
                                    
                                        // treat it like pick up   
                                        pickupToHold( nextPlayer, m.x, m.y, 
                                                      target );
                                        }
                                    else if( targetObj->permanent ) {
                                        // consider bare-hand action
                                        TransRecord *handTrans = getPTrans(
                                            0, target );
                                        
                                        if( handTrans == NULL ) {
                                            // check for instant decay
                                            int newTarget = 
                                                checkTargetInstantDecay(
                                                    target, m.x, m.y );
                                        
                                            // if so, let transition go through
                                            // (skip if result of decay is 0)
                                            if( newTarget != 0 &&
                                                newTarget != target ) {
                                            
                                                target = newTarget;
                                                targetObj = getObject( target );
                                                
                                                handTrans = 
                                                    getPTrans( 0, target );
                                                }
                                            }

                                        if( handTrans != NULL ) {
                                            // try treating it like
                                            // a USE action
                                            m.type = USE;
                                            m.id = -1;
                                            m.c = -1;
                                            playerIndicesToSendUpdatesAbout.
                                                deleteElementEqualTo( i );
                                            goto RESTART_MESSAGE_ACTION;
                                            }
                                        }
                                    }
                                }
                            }
                        }                        
                    else if( m.type == SREMV ) {
                        playerIndicesToSendUpdatesAbout.push_back( i );
                        
                        // remove contained object from clothing
                        char worked = false;
                        
                        if( m.x == nextPlayer->xd &&
                            m.y == nextPlayer->yd &&
                            nextPlayer->holdingID == 0 ) {
                            
                            nextPlayer->actionAttempt = 1;
                            nextPlayer->actionTarget.x = m.x;
                            nextPlayer->actionTarget.y = m.y;
                            
                            if( m.c >= 0 && m.c < NUM_CLOTHING_PIECES ) {
                                worked = removeFromClothingContainerToHold(
                                    nextPlayer, m.c, m.i );
                                }
                            }
                        
                        if( nextPlayer->holdingID == 0 && 
                            m.c >= 0 && m.c < NUM_CLOTHING_PIECES  &&
                            ! worked ) {

                            // hmm... nothing to remove from slots in clothing
                            
                            // player is right-clicking, and maybe they
                            // can't left-click, because there's a 
                            // transition in the way
                            
                            // if so, right click should
                            // remove the clothing itself
                            
                            ObjectRecord **clothingSlot = 
                                getClothingSlot( nextPlayer, m.c );


                            TransRecord *bareHandClothingTrans =
                                getBareHandClothingTrans( nextPlayer,
                                                          clothingSlot );
                                
                            if( bareHandClothingTrans != NULL ) {
                                // there's a transition blocking
                                // regular-click to remove empty
                                // clothing.
                                // allow right click to do it

                                removeClothingToHold( nextPlayer,
                                                      nextPlayer,
                                                      clothingSlot,
                                                      m.c );
                                }
                            }
                        }
                    else if( m.type == EMOT && 
                             ! nextPlayer->emotFrozen ) {
                        // ignore new EMOT requres from player if emot
                        // frozen
                        
                        if( m.i <= SettingsManager::getIntSetting( 
                                "allowedEmotRange", 6 ) ) {
                            
                            SimpleVector<int> *forbidden =
                                SettingsManager::getIntSettingMulti( 
                                    "forbiddenEmots" );
                            
                            if( forbidden->getElementIndex( m.i ) == -1 ) {
                                // not forbidden

                                newEmotPlayerIDs.push_back( nextPlayer->id );
                            
                                newEmotIndices.push_back( m.i );
                                // player-requested emots have no specific TTL
                                newEmotTTLs.push_back( 0 );
                                }
                            delete forbidden;
                            }
                        } 
                    }
                
                if( m.numExtraPos > 0 ) {
                    delete [] m.extraPos;
                    }
                
                if( m.saidText != NULL ) {
                    delete [] m.saidText;
                    }
                if( m.bugText != NULL ) {
                    delete [] m.bugText;
                    }
                }
            }
            
 
        
        // process pending KILL actions
        for( int i=0; i<activeKillStates.size(); i++ ) {
            KillState *s = activeKillStates.getElement( i );
            
            LiveObject *killer = getLiveObject( s->killerID );
            LiveObject *target = getLiveObject( s->targetID );
            
            if( killer == NULL || target == NULL ||
                killer->error || target->error ||
                killer->holdingID != s->killerWeaponID ||
                target->heldByOther ) {
                // either player dead, or held-weapon change
                // or target baby now picked up (safe)
                
                // kill request done
                
                removeKillState( killer, target );

                i--;
                continue;
                }
            
            // kill request still active!
            
            // see if it is realized (close enough)?
            GridPos playerPos = getPlayerPos( killer );
            GridPos targetPos = getPlayerPos( target );
            
            double dist = distance( playerPos, targetPos );
            
            if( getObject( killer->holdingID )->deadlyDistance >= dist &&
                ! directLineBlocked( playerPos, targetPos ) ) {
                // close enough to kill
                
                executeKillAction( getLiveObjectIndex( s->killerID ),
                                   getLiveObjectIndex( s->targetID ),
                                   &playerIndicesToSendUpdatesAbout,
                                   &playerIndicesToSendDyingAbout,
                                   &newEmotPlayerIDs,
                                   &newEmotIndices,
                                   &newEmotTTLs );
                }
            else {
                // still not close enough
                // see if we need to renew emote
                double curTime = Time::getCurrentTime();
                
                if( curTime - s->emotStartTime > s->emotRefreshSeconds ) {
                    s->emotStartTime = curTime;
                    
                    // refresh again in 10 seconds, even if we had a shorter
                    // refresh time because of an intervening emot
                    s->emotRefreshSeconds = 10;

                    newEmotPlayerIDs.push_back( killer->id );
                            
                    newEmotIndices.push_back( killEmotionIndex );
                    newEmotTTLs.push_back( 120 );

                    if( !target->emotFrozen ) {
                        target->emotFrozen = true;
                        newEmotPlayerIDs.push_back( target->id );
                                
                        newEmotIndices.push_back( victimEmotionIndex );
                        target->emotFrozenIndex = victimEmotionIndex;
                        newEmotTTLs.push_back( 120 );
                        }
                    }
                }
            }
        
        //2HOL: check if player is afk or has food effects
        for( int i=0; i<numLive; i++ ) {
            LiveObject *nextPlayer = players.getElement( i );
            double curTime = Time::getCurrentTime();
            
            if( !nextPlayer->tripping && nextPlayer->gonnaBeTripping ) {
                if( curTime >= nextPlayer->trippingEffectStartTime ) {
                    nextPlayer->tripping = true;
                    nextPlayer->gonnaBeTripping = false;
                    makePlayerSay( nextPlayer, (char*)"+TRIPPING+", true );
                    }
                }
            
            if( nextPlayer->tripping ) {
                
                // Uncontrollably flipping
                if( curTime - nextPlayer->lastFlipTime > 0.25 ) {
                    
                    GridPos p = getPlayerPos( nextPlayer );
                    
                    nextPlayer->facingLeft = !nextPlayer->facingLeft;
                    
                    nextPlayer->lastFlipTime = curTime;
                    newFlipPlayerIDs.push_back( nextPlayer->id );
                    newFlipFacingLeft.push_back( 
                        nextPlayer->facingLeft );
                    newFlipPositions.push_back( p );
                    }
                
                if( curTime >= nextPlayer->trippingEffectETA ) {
                    nextPlayer->tripping = false;
                    clearFrozenEmote( nextPlayer, trippingEmotionIndex );
                    }
                else if( !nextPlayer->emotFrozen &&
                    curTime < nextPlayer->trippingEffectETA ) {
                    nextPlayer->emotFrozen = true;
                    nextPlayer->emotFrozenIndex = trippingEmotionIndex;
                    nextPlayer->emotUnfreezeETA = nextPlayer->trippingEffectETA;
                    
                    newEmotPlayerIDs.push_back( nextPlayer->id );
                    newEmotIndices.push_back( trippingEmotionIndex );
                    newEmotTTLs.push_back( nextPlayer->trippingEffectETA );
                    }
                }
            
            if( nextPlayer->drunkennessEffect ) {
                if( Time::getCurrentTime() >= nextPlayer->drunkennessEffectETA ) {
                    nextPlayer->drunkennessEffect = false;
                    clearFrozenEmote( nextPlayer, drunkEmotionIndex );
                    }
                else if( !nextPlayer->emotFrozen &&
                    Time::getCurrentTime() < nextPlayer->drunkennessEffectETA ) {
                    nextPlayer->emotFrozen = true;
                    nextPlayer->emotFrozenIndex = drunkEmotionIndex;
                    nextPlayer->emotUnfreezeETA = nextPlayer->drunkennessEffectETA;
                    
                    newEmotPlayerIDs.push_back( nextPlayer->id );
                    newEmotIndices.push_back( drunkEmotionIndex );
                    newEmotTTLs.push_back( nextPlayer->drunkennessEffectETA );
                    }
                }
            
            if( nextPlayer->connected == false ||
                ( afkTimeSeconds > 0 &&
                Time::getCurrentTime() - nextPlayer->lastActionTime > afkTimeSeconds ) ) {
            
                nextPlayer->isAFK = true;
                
                //Other frozen emotes take priority
                //wounds, murder, food effects, starving, afk
                if( !nextPlayer->emotFrozen ) {
                    nextPlayer->emotFrozen = true;
                    nextPlayer->emotFrozenIndex = afkEmotionIndex;
                    nextPlayer->emotUnfreezeETA = curTime + afkTimeSeconds;
                    
                    newEmotPlayerIDs.push_back( nextPlayer->id );
                    newEmotIndices.push_back( afkEmotionIndex );
                    newEmotTTLs.push_back( curTime + afkTimeSeconds );
                    }
                }
            }
            
        // now that messages have been processed for all
        // loop over and handle all post-message checks

        // for example, if a player later in the list sends a message
        // killing an earlier player, we need to check to see that
        // player deleted afterward here
        for( int i=0; i<numLive; i++ ) {
            LiveObject *nextPlayer = players.getElement( i );
            
            double curTime = Time::getCurrentTime();
            
            
            if( nextPlayer->emotFrozen && 
                nextPlayer->emotUnfreezeETA != 0 &&
                curTime >= nextPlayer->emotUnfreezeETA ) {
                
                nextPlayer->emotFrozen = false;
                nextPlayer->emotUnfreezeETA = 0;
                }
            
            if( ! nextPlayer->error &&
                ! nextPlayer->cravingKnown &&
                computeAge( nextPlayer ) >= minAgeForCravings ) {
                
                sendCraving( nextPlayer );
                }
                


            if( nextPlayer->dying && ! nextPlayer->error &&
                curTime >= nextPlayer->dyingETA ) {
                // finally died
                nextPlayer->error = true;

                
                if( ! nextPlayer->isTutorial ) {
                    // GridPos deathPos = 
                        // getPlayerPos( nextPlayer );
                    
                    int killerID = -1;
                    if( nextPlayer->murderPerpID > 0 ) {
                        killerID = nextPlayer->murderPerpID;
                        }
                    else if( nextPlayer->deathSourceID > 0 ) {
                        // include as negative of ID
                        killerID = - nextPlayer->deathSourceID;
                        }
                    // never have suicide in this case

                    logDeath( nextPlayer->id,
                              nextPlayer->email,
                              nextPlayer->isEve,
                              computeAge( nextPlayer ),
                              getSecondsPlayed( 
                                  nextPlayer ),
                              ! getFemale( nextPlayer ),
                              nextPlayer->xd, nextPlayer->yd,
                              players.size() - 1,
                              false,
                              killerID,
                              nextPlayer->murderPerpEmail );
                                            
                    if( shutdownMode ) {
                        handleShutdownDeath( 
                            nextPlayer, nextPlayer->xd, nextPlayer->yd );
                        }
                    }
                
                nextPlayer->deathLogged = true;
                }
            

                
            if( nextPlayer->isNew ) {
                // their first position is an update
                

                playerIndicesToSendUpdatesAbout.push_back( i );
                playerIndicesToSendLineageAbout.push_back( i );
                
                
                if( nextPlayer->curseStatus.curseLevel > 0 ) {
                    playerIndicesToSendCursesAbout.push_back( i );
                    }
                
                if( usePersonalCurses ) {
                    // send a unique CU message to each player
                    // who has this player cursed
                    
                    // but wait until next step, because other players
                    // haven't heard initial PU about this player yet
                    nextPlayer->isNewCursed = true;
                    }

                nextPlayer->isNew = false;
                
                // force this PU to be sent to everyone
                nextPlayer->updateGlobal = true;
                }
            else if( nextPlayer->isNewCursed ) {
                // update sent about this new player
                // time to send personal curse status (b/c other players
                // know about this player now)
                for( int p=0; p<players.size(); p++ ) {
                    LiveObject *otherPlayer = players.getElement( p );
                    
                    if( otherPlayer == nextPlayer ) {
                        continue;
                        }
                    if( otherPlayer->error ||
                        ! otherPlayer->connected ) {
                        continue;
                        }
                    
                    if( isCursed( otherPlayer->email, 
                                  nextPlayer->email ) ) {
                        char *message = autoSprintf( 
                            "CU\n%d 1 %s_%s\n#",
                            nextPlayer->id,
                            getCurseWord( otherPlayer->email,
                                          nextPlayer->email, 0 ),
                            getCurseWord( otherPlayer->email,
                                          nextPlayer->email, 1 ) );
                        
                        sendMessageToPlayer( otherPlayer,
                                             message, strlen( message ) );
                        delete [] message;
                        }
                    }
                nextPlayer->isNewCursed = false;
                }
            else if( nextPlayer->error && ! nextPlayer->deleteSent ) {
                
                removeAllOwnership( nextPlayer );
                
                decrementLanguageCount( nextPlayer->lineageEveID );
                
                removePlayerLanguageMaps( nextPlayer->id );
                
                if( nextPlayer->heldByOther ) {
                    
                    handleForcedBabyDrop( nextPlayer,
                                          &playerIndicesToSendUpdatesAbout );
                    }                
                else if( nextPlayer->holdingID < 0 ) {
                    LiveObject *babyO = 
                        getLiveObject( - nextPlayer->holdingID );
                    
                    handleForcedBabyDrop( babyO,
                                          &playerIndicesToSendUpdatesAbout );
                    }
                

                newDeleteUpdates.push_back( 
                    getUpdateRecord( nextPlayer, true ) );                
                
                nextPlayer->deathTimeSeconds = Time::getCurrentTime();

                nextPlayer->isNew = false;
                
                nextPlayer->deleteSent = true;
                // wait 10 seconds before closing their connection
                // so they can get the message
                nextPlayer->deleteSentDoneETA = Time::getCurrentTime() + 10;
                
                if( areTriggersEnabled() ) {
                    // add extra time so that rest of triggers can be received
                    // and rest of trigger results can be sent
                    // back to this client
                    
                    // another hour...
                    nextPlayer->deleteSentDoneETA += 3600;
                    // and don't set their error flag after all
                    // keep receiving triggers from them

                    nextPlayer->error = false;
                    }
                else {
                    if( nextPlayer->sock != NULL ) {
                        // stop listening for activity on this socket
                        sockPoll.removeSocket( nextPlayer->sock );
                        }
                    }
                

                GridPos dropPos;
                
                if( nextPlayer->xd == 
                    nextPlayer->xs &&
                    nextPlayer->yd ==
                    nextPlayer->ys ) {
                    // deleted player standing still
                    
                    dropPos.x = nextPlayer->xd;
                    dropPos.y = nextPlayer->yd;
                    }
                else {
                    // player moving
                    
                    dropPos = 
                        computePartialMoveSpot( nextPlayer );
                    }
                
                // report to lineage server once here
                double age = computeAge( nextPlayer );
                
                int killerID = -1;
                if( nextPlayer->murderPerpID > 0 ) {
                    killerID = nextPlayer->murderPerpID;
                    }
                else if( nextPlayer->deathSourceID > 0 ) {
                    // include as negative of ID
                    killerID = - nextPlayer->deathSourceID;
                    }
                else if( nextPlayer->suicide ) {
                    // self id is killer
                    killerID = nextPlayer->id;
                    }
                
                
                
                char male = ! getFemale( nextPlayer );
                
                if( ! nextPlayer->isTutorial ) {
                    recordPlayerLineage( nextPlayer->email, 
                                         age,
                                         nextPlayer->id,
                                         nextPlayer->parentID,
                                         nextPlayer->displayID,
                                         killerID,
                                         nextPlayer->name,
                                         nextPlayer->lastSay,
                                         male );


                    // non-tutorial players only
                    logFitnessDeath( nextPlayer );
                    }
                


                if( SettingsManager::getIntSetting( 
                        "babyApocalypsePossible", 1 ) 
                    &&
                    players.size() > 
                    SettingsManager::getIntSetting(
                        "minActivePlayersForBabyApocalypse", 15 ) ) {
                    
                    double curTime = Time::getCurrentTime();
                    
                    if( ! nextPlayer->isEve ) {
                    
                        // player was born as a baby
                        
                        int barrierRadius = 
                            SettingsManager::getIntSetting( 
                                "barrierRadius", 250 );
                        int barrierOn = SettingsManager::getIntSetting( 
                            "barrierOn", 1 );

                        char insideBarrier = true;
                        
                        if( barrierOn &&
                            ( abs( dropPos.x ) > barrierRadius ||
                              abs( dropPos.y ) > barrierRadius ) ) {
                            
                            insideBarrier = false;
                            }
                              

                        float threshold = SettingsManager::getFloatSetting( 
                            "babySurvivalYearsBeforeApocalypse", 15.0f );
                        
                        if( insideBarrier && age > threshold ) {
                            // baby passed threshold, update last-passed time
                            lastBabyPassedThresholdTime = curTime;
                            }
                        else {
                            // baby died young
                            // OR older, outside barrier
                            // check if we're due for an apocalypse
                            
                            if( lastBabyPassedThresholdTime > 0 &&
                                curTime - lastBabyPassedThresholdTime >
                                SettingsManager::getIntSetting(
                                    "babySurvivalWindowSecondsBeforeApocalypse",
                                    3600 ) ) {
                                // we're outside the window
                                // people have been dying young for a long time
                                
                                triggerApocalypseNow();
                                }
                            else if( lastBabyPassedThresholdTime == 0 ) {
                                // first baby to die, and we have enough
                                // active players.
                                
                                // start window now
                                lastBabyPassedThresholdTime = curTime;
                                }
                            }
                        }
                    }
                else {
                    // not enough players
                    // reset window
                    lastBabyPassedThresholdTime = curTime;
                    }
                

                // don't use age here, because it unfairly gives Eve
                // +14 years that she didn't actually live
                // use true played years instead
                double yearsLived = 
                    getSecondsPlayed( nextPlayer ) * getAgeRate();

                if( ! nextPlayer->isTutorial ) {
                    
                    recordLineage( 
                        nextPlayer->email, 
                        nextPlayer->originalBirthPos,
                        yearsLived, 
                        // count true murder victims here, not suicide
                        ( killerID > 0 && killerID != nextPlayer->id ),
                        // killed other or committed SID suicide
                        nextPlayer->everKilledAnyone || 
                        nextPlayer->suicide );
        
                    if( nextPlayer->suicide ) {
                        // add to player's skip list
                        skipFamily( nextPlayer->email, 
                                    nextPlayer->lineageEveID );
                        }
                    }
                
                

                if( ! nextPlayer->deathLogged ) {
                    char disconnect = true;
                    
                    if( age >= forceDeathAge ) {
                        disconnect = false;
                        }
                    
                    if( ! nextPlayer->isTutorial ) {    
                        logDeath( nextPlayer->id,
                                  nextPlayer->email,
                                  nextPlayer->isEve,
                                  age,
                                  getSecondsPlayed( nextPlayer ),
                                  male,
                                  dropPos.x, dropPos.y,
                                  players.size() - 1,
                                  disconnect,
                                  killerID,
                                  nextPlayer->murderPerpEmail );
                    
                        if( shutdownMode ) {
                            handleShutdownDeath( 
                                nextPlayer, dropPos.x, dropPos.y );
                            }
                        }
                    
                    nextPlayer->deathLogged = true;
                    }
                
                // now that death has been logged, and delete sent,
                // we can clear their email address so that the 
                // can log in again during the deleteSentDoneETA window
                
                if( nextPlayer->email != NULL ) {
                    if( nextPlayer->origEmail != NULL ) {
                        delete [] nextPlayer->origEmail;
                        }
                    nextPlayer->origEmail = 
                        stringDuplicate( nextPlayer->email );
                    delete [] nextPlayer->email;
                    }
                nextPlayer->email = stringDuplicate( "email_cleared" );

                int deathID = getRandomDeathMarker();
                    
                if( nextPlayer->customGraveID > -1 ) {
                    deathID = nextPlayer->customGraveID;
                    }

                char deathMarkerHasSlots = false;
                
                if( deathID > 0 ) {
                    deathMarkerHasSlots = 
                        ( getObject( deathID )->numSlots > 0 );
                    }

                int oldObject = getMapObject( dropPos.x, dropPos.y );
                
                SimpleVector<int> oldContained;
                SimpleVector<timeSec_t> oldContainedETADecay;
                
                if( deathID != 0 ) {
                    
                
                    int nX[4] = { -1, 1,  0, 0 };
                    int nY[4] = {  0, 0, -1, 1 };
                    
                    int n = 0;
                    GridPos centerDropPos = dropPos;
                    
                    while( oldObject != 0 && n < 4 ) {
                        
                        // don't combine graves
                        if( ! isGrave( oldObject ) ) {
                            ObjectRecord *r = getObject( oldObject );
                            
                            if( deathMarkerHasSlots &&
                                r->numSlots == 0 && ! r->permanent 
                                && ! r->rideable ) {
                                
                                // found a containble object
                                // we can empty this spot to make room
                                // for a grave that can go here, and
                                // put the old object into the new grave.
                                
                                oldContained.push_back( oldObject );
                                oldContainedETADecay.push_back(
                                    getEtaDecay( dropPos.x, dropPos.y ) );
                                
                                setMapObject( dropPos.x, dropPos.y, 0 );
                                oldObject = 0;
                                }
                            }
                        
                        oldObject = getMapObject( dropPos.x, dropPos.y );
                        
                        if( oldObject != 0 ) {
                            
                            // try next neighbor
                            dropPos.x = centerDropPos.x + nX[n];
                            dropPos.y = centerDropPos.y + nY[n];
                            
                            n++;
                            oldObject = getMapObject( dropPos.x, dropPos.y );
                            }
                        }
                    }
                

                if( ! isMapSpotEmpty( dropPos.x, dropPos.y, false ) ) {
                    
                    // failed to find an empty spot, or a containable object
                    // at center or four neighbors
                    
                    // search outward in spiral of up to 100 points
                    // look for some empty spot
                    
                    char foundEmpty = false;
                    
                    GridPos newDropPos = findClosestEmptyMapSpot(
                        dropPos.x, dropPos.y, 100, &foundEmpty );
                    
                    if( foundEmpty ) {
                        dropPos = newDropPos;
                        }
                    }


                // assume death markes non-blocking, so it's safe
                // to drop one even if other players standing here
                if( isMapSpotEmpty( dropPos.x, dropPos.y, false ) ) {

                    if( deathID > 0 ) {
                        
                        setResponsiblePlayer( - nextPlayer->id );
                        setMapObject( dropPos.x, dropPos.y, 
                                      deathID );
                        setResponsiblePlayer( -1 );
                        
                        GraveInfo graveInfo = { dropPos, nextPlayer->id,
                                                nextPlayer->lineageEveID };
                        //Only use GV message for players which name and displayedName match
                        //otherwise use GO message to update clients with names for graves
                        if (
                            (nextPlayer->name == NULL && nextPlayer->displayedName == NULL) ||
                            (nextPlayer->name != NULL && nextPlayer->displayedName != NULL && 
                            strcmp(nextPlayer->name, nextPlayer->displayedName) == 0)
                            ) 
                            newGraves.push_back( graveInfo );
                        
                        setGravePlayerID( dropPos.x, dropPos.y,
                                          nextPlayer->id );

                        ObjectRecord *deathObject = getObject( deathID );
                        
                        int roomLeft = deathObject->numSlots;
                        
                        if( roomLeft >= 1 ) {
                            // room for weapon remnant
                            if( nextPlayer->embeddedWeaponID != 0 ) {
                                addContained( 
                                    dropPos.x, dropPos.y,
                                    nextPlayer->embeddedWeaponID,
                                    nextPlayer->embeddedWeaponEtaDecay );
                                roomLeft--;
                                }
                            }
                        
                            
                        if( roomLeft >= 5 ) {
                            // room for clothing
                            
                            if( nextPlayer->clothing.tunic != NULL ) {
                                
                                addContained( 
                                    dropPos.x, dropPos.y,
                                    nextPlayer->clothing.tunic->id,
                                    nextPlayer->clothingEtaDecay[1] );
                                roomLeft--;
                                }
                            if( nextPlayer->clothing.bottom != NULL ) {
                                
                                addContained( 
                                    dropPos.x, dropPos.y,
                                    nextPlayer->clothing.bottom->id,
                                    nextPlayer->clothingEtaDecay[4] );
                                roomLeft--;
                                }
                            if( nextPlayer->clothing.backpack != NULL ) {
                                
                                addContained( 
                                    dropPos.x, dropPos.y,
                                    nextPlayer->clothing.backpack->id,
                                    nextPlayer->clothingEtaDecay[5] );
                                roomLeft--;
                                }
                            if( nextPlayer->clothing.backShoe != NULL ) {
                                
                                addContained( 
                                    dropPos.x, dropPos.y,
                                    nextPlayer->clothing.backShoe->id,
                                    nextPlayer->clothingEtaDecay[3] );
                                roomLeft--;
                                }
                            if( nextPlayer->clothing.frontShoe != NULL ) {
                                
                                addContained( 
                                    dropPos.x, dropPos.y,
                                    nextPlayer->clothing.frontShoe->id,
                                    nextPlayer->clothingEtaDecay[2] );
                                roomLeft--;
                                }
                            if( nextPlayer->clothing.hat != NULL ) {
                                
                                addContained( dropPos.x, dropPos.y,
                                              nextPlayer->clothing.hat->id,
                                              nextPlayer->clothingEtaDecay[0] );
                                roomLeft--;
                                }
                            }
                        
                        // room for what clothing contained
                        timeSec_t curTime = Time::getCurrentTime();
                        
                        for( int c=0; c < NUM_CLOTHING_PIECES && roomLeft > 0; 
                             c++ ) {
                            
                            float oldStretch = 1.0;
                            
                            ObjectRecord *cObj = clothingByIndex( 
                                nextPlayer->clothing, c );
                            
                            if( cObj != NULL ) {
                                oldStretch = cObj->slotTimeStretch;
                                }
                            
                            float newStretch = deathObject->slotTimeStretch;
                            
                            for( int cc=0; 
                                 cc < nextPlayer->clothingContained[c].size() 
                                     &&
                                     roomLeft > 0;
                                 cc++ ) {
                                
                                if( nextPlayer->
                                    clothingContainedEtaDecays[c].
                                    getElementDirect( cc ) != 0 &&
                                    oldStretch != newStretch ) {
                                        
                                    timeSec_t offset = 
                                        nextPlayer->
                                        clothingContainedEtaDecays[c].
                                        getElementDirect( cc ) - 
                                        curTime;
                                        
                                    offset = offset * oldStretch;
                                    offset = offset / newStretch;
                                        
                                    *( nextPlayer->
                                       clothingContainedEtaDecays[c].
                                       getElement( cc ) ) =
                                        curTime + offset;
                                    }

                                addContained( 
                                    dropPos.x, dropPos.y,
                                    nextPlayer->
                                    clothingContained[c].
                                    getElementDirect( cc ),
                                    nextPlayer->
                                    clothingContainedEtaDecays[c].
                                    getElementDirect( cc ) );
                                roomLeft --;
                                }
                            }
                        
                        int oc = 0;
                        
                        while( oc < oldContained.size() && roomLeft > 0 ) {
                            addContained( 
                                dropPos.x, dropPos.y,
                                oldContained.getElementDirect( oc ),
                                oldContainedETADecay.getElementDirect( oc ) );
                            oc++;
                            roomLeft--;                                
                            }
                        }  
                    }
                if( nextPlayer->holdingID != 0 ) {

                    char doNotDrop = false;
                    
                    if( nextPlayer->murderSourceID > 0 ) {
                        
                        TransRecord *woundHit = 
                            getPTrans( nextPlayer->murderSourceID, 
                                      0, true, false );
                        
                        if( woundHit != NULL &&
                            woundHit->newTarget > 0 ) {
                            
                            if( nextPlayer->holdingID == woundHit->newTarget ) {
                                // they are simply holding their wound object
                                // don't drop this on the ground
                                doNotDrop = true;
                                }
                            }
                        }
                    if( nextPlayer->holdingWound ) {
                        // holding a wound from some other, non-murder cause
                        // of death
                        doNotDrop = true;
                        }
                    
                    
                    if( ! doNotDrop ) {
                        // drop what they were holding

                        // this will almost always involve a throw
                        // (death marker, at least, will be in the way)
                        handleDrop( 
                            dropPos.x, dropPos.y, 
                            nextPlayer,
                            &playerIndicesToSendUpdatesAbout );
                        }
                    else {
                        // just clear what they were holding
                        nextPlayer->holdingID = 0;
                        }
                    }
                }
            else if( ! nextPlayer->error ) {
                // other update checks for living players
                
                if( nextPlayer->holdingEtaDecay != 0 &&
                    nextPlayer->holdingEtaDecay < curTime ) {
                
                    // what they're holding has decayed
                    handleHeldDecay( nextPlayer, i,
                                     &playerIndicesToSendUpdatesAbout,
                                     &playerIndicesToSendHealingAbout );
                    }

                // check if anything in the container they are holding
                // has decayed
                if( nextPlayer->holdingID > 0 &&
                    nextPlayer->numContained > 0 ) {
                    
                    char change = false;
                    
                    SimpleVector<int> newContained;
                    SimpleVector<timeSec_t> newContainedETA;

                    SimpleVector< SimpleVector<int> > newSubContained;
                    SimpleVector< SimpleVector<timeSec_t> > newSubContainedETA;
                    
                    for( int c=0; c< nextPlayer->numContained; c++ ) {
                        int oldID = abs( nextPlayer->containedIDs[c] );
                        int newID = oldID;

                        timeSec_t newDecay = 
                            nextPlayer->containedEtaDecays[c];

                        SimpleVector<int> subCont = 
                            nextPlayer->subContainedIDs[c];
                        SimpleVector<timeSec_t> subContDecay = 
                            nextPlayer->subContainedEtaDecays[c];

                        if( newDecay != 0 && newDecay < curTime ) {
                            
                            change = true;
                            
                            TransRecord *t = getPTrans( -1, oldID );

                            newDecay = 0;

                            if( t != NULL ) {
                                
                                newID = t->newTarget;
                            
                                if( newID != 0 ) {
                                    float stretch = 
                                        getObject( nextPlayer->holdingID )->
                                        slotTimeStretch;
                                    
                                    TransRecord *newDecayT = 
                                        getMetaTrans( -1, newID );
                                
                                    if( newDecayT != NULL ) {
                                        newDecay = 
                                            Time::getCurrentTime() +
                                            newDecayT->autoDecaySeconds /
                                            stretch;
                                        }
                                    else {
                                        // no further decay
                                        newDecay = 0;
                                        }
                                    }
                                }
                            }
                        
                        SimpleVector<int> cVec;
                        SimpleVector<timeSec_t> dVec;

                        if( newID != 0 ) {
                            int oldSlots = subCont.size();
                            
                            int newSlots = getObject( newID )->numSlots;
                            
                            if( newID != oldID
                                &&
                                newSlots < oldSlots ) {
                                
                                // shrink sub-contained
                                // this involves items getting lost
                                // but that's okay for now.
                                subCont.shrink( newSlots );
                                subContDecay.shrink( newSlots );
                                }
                            }
                        else {
                            subCont.deleteAll();
                            subContDecay.deleteAll();
                            }

                        // handle decay for each sub-contained object
                        for( int s=0; s<subCont.size(); s++ ) {
                            int oldSubID = subCont.getElementDirect( s );
                            int newSubID = oldSubID;
                            timeSec_t newSubDecay = 
                                subContDecay.getElementDirect( s );
                            
                            if( newSubDecay != 0 && newSubDecay < curTime ) {
                            
                                change = true;
                            
                                TransRecord *t = getPTrans( -1, oldSubID );

                                newSubDecay = 0;

                                if( t != NULL ) {
                                
                                    newSubID = t->newTarget;
                            
                                    if( newSubID != 0 ) {
                                        float subStretch = 
                                            getObject( newID )->
                                            slotTimeStretch;
                                    
                                        TransRecord *newSubDecayT = 
                                            getMetaTrans( -1, newSubID );
                                
                                        if( newSubDecayT != NULL ) {
                                            newSubDecay = 
                                                Time::getCurrentTime() +
                                                newSubDecayT->autoDecaySeconds /
                                                subStretch;
                                            }
                                        else {
                                            // no further decay
                                            newSubDecay = 0;
                                            }
                                        }
                                    }
                                }
                            
                            if( newSubID != 0 ) {
                                cVec.push_back( newSubID );
                                dVec.push_back( newSubDecay );
                                }
                            }
                        
                        if( newID != 0 ) {    
                            newSubContained.push_back( cVec );
                            newSubContainedETA.push_back( dVec );

                            if( cVec.size() > 0 ) {
                                newID *= -1;
                                }
                            
                            newContained.push_back( newID );
                            newContainedETA.push_back( newDecay );
                            }
                        }
                    
                    

                    if( change ) {
                        playerIndicesToSendUpdatesAbout.push_back( i );
                        
                        freePlayerContainedArrays( nextPlayer );
                        
                        nextPlayer->numContained = newContained.size();

                        if( nextPlayer->numContained == 0 ) {
                            nextPlayer->containedIDs = NULL;
                            nextPlayer->containedEtaDecays = NULL;
                            nextPlayer->subContainedIDs = NULL;
                            nextPlayer->subContainedEtaDecays = NULL;
                            }
                        else {
                            nextPlayer->containedIDs = 
                                newContained.getElementArray();
                            nextPlayer->containedEtaDecays = 
                                newContainedETA.getElementArray();
                        
                            nextPlayer->subContainedIDs =
                                newSubContained.getElementArray();
                            nextPlayer->subContainedEtaDecays =
                                newSubContainedETA.getElementArray();
                            }
                        }
                    }
                
                
                // check if their clothing has decayed
                // or what's in their clothing
                for( int c=0; c<NUM_CLOTHING_PIECES; c++ ) {
                    ObjectRecord *cObj = 
                        clothingByIndex( nextPlayer->clothing, c );
                    
                    if( cObj != NULL &&
                        nextPlayer->clothingEtaDecay[c] != 0 &&
                        nextPlayer->clothingEtaDecay[c] < 
                        curTime ) {
                
                        // what they're wearing has decayed

                        int oldID = cObj->id;
                
                        TransRecord *t = getPTrans( -1, oldID );

                        if( t != NULL ) {

                            int newID = t->newTarget;
                            
                            ObjectRecord *newCObj = NULL;
                            if( newID != 0 ) {
                                newCObj = getObject( newID );
                                
                                TransRecord *newDecayT = 
                                    getMetaTrans( -1, newID );
                                
                                if( newDecayT != NULL ) {
                                    nextPlayer->clothingEtaDecay[c] = 
                                        Time::getCurrentTime() + 
                                        newDecayT->autoDecaySeconds;
                                    }
                                else {
                                    // no further decay
                                    nextPlayer->clothingEtaDecay[c] = 0;
                                    }
                                }
                            else {
                                nextPlayer->clothingEtaDecay[c] = 0;
                                }
                            
                            setClothingByIndex( &( nextPlayer->clothing ),
                                                c, newCObj );
                            
                            int oldSlots = 
                                nextPlayer->clothingContained[c].size();

                            int newSlots = getNumContainerSlots( newID );
                    
                            if( newSlots < oldSlots ) {
                                // new container can hold less
                                // truncate
                                
                                // drop extras onto map
                                timeSec_t curTime = Time::getCurrentTime();
                                float stretch = cObj->slotTimeStretch;
                                
                                GridPos dropPos = 
                                    getPlayerPos( nextPlayer );
                            
                                // offset to counter-act offsets built into
                                // drop code
                                dropPos.x += 1;
                                dropPos.y += 1;

                                for( int s=newSlots; s<oldSlots; s++ ) {
                                    
                                    char found = false;
                                    GridPos spot;
                                
                                    if( getMapObject( dropPos.x, 
                                                      dropPos.y ) == 0 ) {
                                        spot = dropPos;
                                        found = true;
                                        }
                                    else {
                                        found = findDropSpot( 
                                            dropPos.x, dropPos.y,
                                            dropPos.x, dropPos.y,
                                            &spot );
                                        }
                            
                            
                                    if( found ) {
                                        setMapObject( 
                                            spot.x, spot.y,
                                            nextPlayer->
                                            clothingContained[c].
                                            getElementDirect( s ) );
                                        
                                        timeSec_t eta =
                                            nextPlayer->
                                            clothingContainedEtaDecays[c].
                                            getElementDirect( s );
                                        
                                        if( stretch != 1.0 ) {
                                            timeSec_t offset = 
                                                eta - curTime;
                    
                                            offset = offset / stretch;
                                            eta = curTime + offset;
                                            }
                                        
                                        setEtaDecay( spot.x, spot.y, eta );
                                        }
                                    }

                                nextPlayer->
                                    clothingContained[c].
                                    shrink( newSlots );
                                
                                nextPlayer->
                                    clothingContainedEtaDecays[c].
                                    shrink( newSlots );
                                }
                            
                            float oldStretch = 
                                cObj->slotTimeStretch;
                            float newStretch;
                            
                            if( newCObj != NULL ) {
                                newStretch = newCObj->slotTimeStretch;
                                }
                            else {
                                newStretch = oldStretch;
                                }
                            
                            if( oldStretch != newStretch ) {
                                timeSec_t curTime = Time::getCurrentTime();
                                
                                for( int cc=0;
                                     cc < nextPlayer->
                                          clothingContainedEtaDecays[c].size();
                                     cc++ ) {
                                    
                                    if( nextPlayer->
                                        clothingContainedEtaDecays[c].
                                        getElementDirect( cc ) != 0 ) {
                                        
                                        timeSec_t offset = 
                                            nextPlayer->
                                            clothingContainedEtaDecays[c].
                                            getElementDirect( cc ) - 
                                            curTime;
                                        
                                        offset = offset * oldStretch;
                                        offset = offset / newStretch;
                                        
                                        *( nextPlayer->
                                           clothingContainedEtaDecays[c].
                                           getElement( cc ) ) =
                                            curTime + offset;
                                        }
                                    }
                                }

                            playerIndicesToSendUpdatesAbout.push_back( i );
                            }
                        else {
                            // no valid decay transition, end it
                            nextPlayer->clothingEtaDecay[c] = 0;
                            }
                        
                        }
                    
                    // check for decay of what's contained in clothing
                    if( cObj != NULL &&
                        nextPlayer->clothingContainedEtaDecays[c].size() > 0 ) {
                        
                        char change = false;
                        
                        SimpleVector<int> newContained;
                        SimpleVector<timeSec_t> newContainedETA;

                        for( int cc=0; 
                             cc <
                                 nextPlayer->
                                 clothingContainedEtaDecays[c].size();
                             cc++ ) {
                            
                            int oldID = nextPlayer->
                                clothingContained[c].getElementDirect( cc );
                            int newID = oldID;
                        
                            timeSec_t decay = 
                                nextPlayer->clothingContainedEtaDecays[c]
                                .getElementDirect( cc );

                            timeSec_t newDecay = decay;
                            
                            if( decay != 0 && decay < curTime ) {
                                
                                change = true;
                            
                                TransRecord *t = getPTrans( -1, oldID );
                                
                                newDecay = 0;

                                if( t != NULL ) {
                                    newID = t->newTarget;
                            
                                    if( newID != 0 ) {
                                        TransRecord *newDecayT = 
                                            getMetaTrans( -1, newID );
                                        
                                        if( newDecayT != NULL ) {
                                            newDecay = 
                                                Time::getCurrentTime() +
                                                newDecayT->
                                                autoDecaySeconds /
                                                cObj->slotTimeStretch;
                                            }
                                        else {
                                            // no further decay
                                            newDecay = 0;
                                            }
                                        }
                                    }
                                }
                        
                            if( newID != 0 ) {
                                newContained.push_back( newID );
                                newContainedETA.push_back( newDecay );
                                } 
                            }
                        
                        if( change ) {
                            playerIndicesToSendUpdatesAbout.push_back( i );
                            
                            // assignment operator for vectors
                            // copies one vector into another
                            // replacing old contents
                            nextPlayer->clothingContained[c] =
                                newContained;
                            nextPlayer->clothingContainedEtaDecays[c] =
                                newContainedETA;
                            }
                        
                        }
                    
                    
                    }
                

                // check if they are done moving
                // if so, send an update
                

                if( nextPlayer->xd != nextPlayer->xs ||
                    nextPlayer->yd != nextPlayer->ys ) {
                
                    
                    // don't end new moves here (moves that 
                    // other players haven't been told about)
                    // even if they have come to an end time-wise
                    // wait until after we've told everyone about them
                    if( ! nextPlayer->newMove && 
                        Time::getCurrentTime() - nextPlayer->moveStartTime
                        >
                        nextPlayer->moveTotalSeconds ) {
                        
                        double moveSpeed = computeMoveSpeed( nextPlayer ) *
                            getPathSpeedModifier( nextPlayer->pathToDest,
                                                  nextPlayer->pathLength );


                        // done
                        nextPlayer->xs = nextPlayer->xd;
                        nextPlayer->ys = nextPlayer->yd;                        

                        //printf( "Player %d's move is done at %d,%d\n",
                        //        nextPlayer->id,
                        //        nextPlayer->xs,
                        //        nextPlayer->ys );

                        if( nextPlayer->pathTruncated ) {
                            // truncated, but never told them about it
                            // force update now
                            nextPlayer->posForced = true;
                            }
                        playerIndicesToSendUpdatesAbout.push_back( i );

                        
                        // if they went far enough and fast enough
                        if( nextPlayer->holdingFlightObject &&
                            moveSpeed >= minFlightSpeed &&
                            ! nextPlayer->pathTruncated &&
                            nextPlayer->pathLength >= 2 ) {
                                    
                            // player takes off ?
                            
                            double xDir = 
                                nextPlayer->pathToDest[ 
                                      nextPlayer->pathLength - 1 ].x
                                  -
                                  nextPlayer->pathToDest[ 
                                      nextPlayer->pathLength - 2 ].x;
                            double yDir = 
                                nextPlayer->pathToDest[ 
                                      nextPlayer->pathLength - 1 ].y
                                  -
                                  nextPlayer->pathToDest[ 
                                      nextPlayer->pathLength - 2 ].y;
                            
                            int beyondEndX = nextPlayer->xs + xDir;
                            int beyondEndY = nextPlayer->ys + yDir;
                            
                            int endFloorID = getMapFloor( nextPlayer->xs,
                                                          nextPlayer->ys );
                            
                            int beyondEndFloorID = getMapFloor( beyondEndX,
                                                                beyondEndY );
                            
                            if( beyondEndFloorID != endFloorID ) {
                                // went all the way to the end of the 
                                // current floor in this direction, 
                                // take off there
                            
                                doublePair takeOffDir = { xDir, yDir };

                                int radiusLimit = -1;
                                
                                int barrierRadius = 
                                    SettingsManager::getIntSetting( 
                                        "barrierRadius", 250 );
                                int barrierOn = SettingsManager::getIntSetting( 
                                    "barrierOn", 1 );
                                
                                if( barrierOn ) {
                                    radiusLimit = barrierRadius;
                                    }

                                GridPos destPos = { -1, -1 };
                                std::string message = "";
                                
                                char foundMap = false;
                                if( Time::getCurrentTime() - 
                                    nextPlayer->forceFlightDestSetTime
                                    < 30 ) {
                                    // map fresh in memory

                                    
                                    destPos = getClosestLandingPos( 
                                        nextPlayer->forceFlightDest,
                                        &foundMap );
                                        
                                    if( foundMap ) {
                                        message = "LANDED AT THE TARGET LOCATION ON THE MAP.";
                                        }
                                    else {
                                        message += "NO LANDING STRIPS ARE FOUND NEAR THE MAP LOCATION.**";
                                        }
                                    
                                    // find strip closest to last
                                    // read map position
                                    AppLog::infoF( 
                                    "Player %d flight taking off from (%d,%d), "
                                    "map dest (%d,%d), found=%d, found (%d,%d)",
                                    nextPlayer->id,
                                    nextPlayer->xs, nextPlayer->ys,
                                    nextPlayer->forceFlightDest.x,
                                    nextPlayer->forceFlightDest.y,
                                    foundMap,
                                    destPos.x, destPos.y );
                                    }                                
                                if( ! foundMap ) {
                                    // find strip in flight direction
                                    
                                    int flightOutcomeFlag = -1;
                                    
                                    destPos = getNextFlightLandingPos(
                                        nextPlayer->xs,
                                        nextPlayer->ys,
                                        takeOffDir,
                                        &flightOutcomeFlag,
                                        radiusLimit );
                                        
                                    if( flightOutcomeFlag == 0 ) {
                                        message = "YOU HAVE CRASH LANDED.**"
                                                  "NO LANDING STRIPS ARE AVAILABLE ANYWHERE.";
                                        }
                                    else if( flightOutcomeFlag == 1 ) {
                                        message += "LANDED AT THE CLOSEST STRIP IN THE TAKE-OFF DIRECTION.";
                                        }
                                    else if( flightOutcomeFlag == 2 ) {
                                        message = "LANDED AT THE ONLY LANDING STRIP AVAILABLE ANYWHERE.";
                                        }
                                    else if( flightOutcomeFlag == 3 ) {
                                        message += "LANDED AT THE CLOSEST STRIP. NONE AVAILABLE IN TAKE-OFF DIRECTION.";
                                        }
                                    
                                    AppLog::infoF( 
                                    "Player %d non-map flight taking off "
                                    "from (%d,%d), "
                                    "flightDir (%f,%f), dest (%d,%d)",
                                    nextPlayer->id,
                                    nextPlayer->xs, nextPlayer->ys,
                                    xDir, yDir,
                                    destPos.x, destPos.y );
                                    }
                                
                                
                                sendGlobalMessage( 
                                    (char*)message.c_str(),
                                    nextPlayer );
                                
                            
                                // send them a brand new map chunk
                                // around their new location
                                // and re-tell them about all players
                                // (relative to their new "birth" location...
                                //  see below)
                                nextPlayer->firstMessageSent = false;
                                nextPlayer->firstMapSent = false;
                                nextPlayer->inFlight = true;
                                
                                int destID = getMapObject( destPos.x,
                                                           destPos.y );
                                    
                                char heldTransHappened = false;
                                    
                                if( destID > 0 &&
                                    getObject( destID )->isFlightLanding ) {
                                    // found a landing place
                                    TransRecord *tr =
                                        getPTrans( nextPlayer->holdingID,
                                                   destID );
                                        
                                    if( tr != NULL ) {
                                        heldTransHappened = true;
                                            
                                        setMapObject( destPos.x, destPos.y,
                                                      tr->newTarget );

                                        transferHeldContainedToMap( 
                                            nextPlayer,
                                            destPos.x, destPos.y );

                                        handleHoldingChange(
                                            nextPlayer,
                                            tr->newActor );
                                            
                                        // stick player next to landing
                                        // pad
                                        destPos.x --;
                                        }
                                    }
                                if( ! heldTransHappened ) {
                                    // crash landing
                                    // force decay of held
                                    // no matter how much time is left
                                    // (flight uses fuel)
                                    TransRecord *decayTrans =
                                        getPTrans( -1, 
                                                   nextPlayer->holdingID );
                                        
                                    if( decayTrans != NULL ) {
                                        handleHoldingChange( 
                                            nextPlayer,
                                            decayTrans->newTarget );
                                        }
                                    }
                                    
                                FlightDest fd = {
                                    nextPlayer->id,
                                    destPos };

                                newFlightDest.push_back( fd );
                                
                                nextPlayer->xd = destPos.x;
                                nextPlayer->xs = destPos.x;
                                nextPlayer->yd = destPos.y;
                                nextPlayer->ys = destPos.y;

                                // reset their birth location
                                // their landing position becomes their
                                // new 0,0 for now
                                
                                // birth-relative coordinates enable the client
                                // (which is on a GPU with 32-bit floats)
                                // to operate at true coordinates well above
                                // the 23-bit preciions of 32-bit floats.
                                
                                // We keep the coordinates small by assuming
                                // that a player can never get too far from
                                // their birth location in one lifetime.
                                
                                // Flight teleportation violates this 
                                // assumption.
                                nextPlayer->birthPos.x = nextPlayer->xs;
                                nextPlayer->birthPos.y = nextPlayer->ys;
                                nextPlayer->heldOriginX = nextPlayer->xs;
                                nextPlayer->heldOriginY = nextPlayer->ys;
                                
                                nextPlayer->actionTarget.x = nextPlayer->xs;
                                nextPlayer->actionTarget.y = nextPlayer->ys;
                                }
                            }
                        }
                    }
                
                // check if we need to decrement their food
                double curTime = Time::getCurrentTime();
                
                if( ! nextPlayer->vogMode &&
                    curTime > 
                    nextPlayer->foodDecrementETASeconds ) {
                    
                    // only if femail of fertile age
                    char heldByFemale = false;
                    
                    if( nextPlayer->heldByOther ) {
                        LiveObject *adultO = getAdultHolding( nextPlayer );
                        
                        if( adultO != NULL &&
                            isFertileAge( adultO ) ) {
                    
                            heldByFemale = true;
                            }
                        }
                    
                    
                    LiveObject *decrementedPlayer = NULL;

                    if( !heldByFemale ) {

                        if( nextPlayer->yummyBonusStore > 0 ) {
                            nextPlayer->yummyBonusStore--;
                            }
                        else {
                            nextPlayer->foodStore--;
                            }
                        decrementedPlayer = nextPlayer;
                        }
                    // if held by fertile female, food for baby is free for
                    // duration of holding
                    
                    // only update the time of the fed player
                    nextPlayer->foodDecrementETASeconds = curTime +
                        computeFoodDecrementTimeSeconds( nextPlayer );

                    if( nextPlayer->drunkenness > 0 ) {
                        // for every unit of food consumed, consume half a
                        // unit of drunkenness
                        nextPlayer->drunkenness -= 0.5;
                        if( nextPlayer->drunkenness < 0 ) {
                            nextPlayer->drunkenness = 0;
                            }
                        }
                    

                    if( decrementedPlayer != NULL &&
                        decrementedPlayer->foodStore < 0 ) {
                        // player has died
                        
                        // break the connection with them

                        // if player was wounded or sick before starving
                        // that should be the reason of death instead
                        if( !decrementedPlayer->deathSourceID ) {
                            if( heldByFemale ) {
                                setDeathReason( decrementedPlayer, 
                                                "nursing_hunger" );
                                }
                            else {
                                setDeathReason( decrementedPlayer, 
                                                "hunger" );
                                }
                            }
                        
                        decrementedPlayer->error = true;
                        decrementedPlayer->errorCauseString = "Player starved";


                        GridPos deathPos;
                                        
                        if( decrementedPlayer->xd == 
                            decrementedPlayer->xs &&
                            decrementedPlayer->yd ==
                            decrementedPlayer->ys ) {
                            // deleted player standing still
                            
                            deathPos.x = decrementedPlayer->xd;
                            deathPos.y = decrementedPlayer->yd;
                            }
                        else {
                            // player moving
                            
                            deathPos = 
                                computePartialMoveSpot( decrementedPlayer );
                            }
                        
                        if( ! decrementedPlayer->deathLogged &&
                            ! decrementedPlayer->isTutorial ) {    
                            
                            
                            // yes, they starved to death here
                            // but also log case where they were wounded
                            // before starving.  Thus, the true cause
                            // of death should be the wounding that made
                            // them helpless and caused them to starve
                            int killerID = -1;
                            if( nextPlayer->murderPerpID > 0 ) {
                                killerID = nextPlayer->murderPerpID;
                                }
                            else if( nextPlayer->deathSourceID > 0 ) {
                                // include as negative of ID
                                killerID = - nextPlayer->deathSourceID;
                                }
                            // never have suicide in this case

                            logDeath( decrementedPlayer->id,
                                      decrementedPlayer->email,
                                      decrementedPlayer->isEve,
                                      computeAge( decrementedPlayer ),
                                      getSecondsPlayed( decrementedPlayer ),
                                      ! getFemale( decrementedPlayer ),
                                      deathPos.x, deathPos.y,
                                      players.size() - 1,
                                      false,
                                      killerID,
                                      nextPlayer->murderPerpEmail );
                            }
                        
                        if( shutdownMode &&
                            ! decrementedPlayer->isTutorial ) {
                            handleShutdownDeath( decrementedPlayer,
                                                 deathPos.x, deathPos.y );
                            }
                                            
                        decrementedPlayer->deathLogged = true;
                                        

                        // no negative
                        decrementedPlayer->foodStore = 0;
                        }
                    
                    if( decrementedPlayer != NULL ) {
                        decrementedPlayer->foodUpdate = true;

                        if( computeAge( decrementedPlayer ) > 
                            defaultActionAge ) {
                            
                            double decTime = 
                                computeFoodDecrementTimeSeconds( 
                                    decrementedPlayer );
                            
                            int totalFood = 
                                decrementedPlayer->yummyBonusStore
                                + decrementedPlayer->foodStore;

                            double totalTime = decTime * (totalFood + 1); // + 1 because we survive at 0 pip 
                            
                            if( totalTime < 20 ) {
                                // 20 seconds left before death
                                // show starving emote
                                
                                // only if their emote isn't frozen
                                
                                // Otherwise it always overwrites 
                                // yellow fever emote for example.
                                
                                // Note also that starving emote 
                                // won't show during tripping and drunk emote
                                
                                // But player chose to be in those states,
                                // they should be responsible not to
                                // starve themselves.
                                if( !decrementedPlayer->emotFrozen ) {
                                    newEmotPlayerIDs.push_back( 
                                        decrementedPlayer->id );
                                
                                    newEmotIndices.push_back( 
                                        starvingEmotionIndex );
                                    
                                    newEmotTTLs.push_back( 30 );
                                    }
                                decrementedPlayer->starving = true;
                                }
                            }
                        }
                    }
                
                }
            
            
            }
        

        
        // check for any that have been individually flagged, but
        // aren't on our list yet (updates caused by external triggers)
        for( int i=0; i<players.size() ; i++ ) {
            LiveObject *nextPlayer = players.getElement( i );
            
            if( nextPlayer->needsUpdate ) {
                playerIndicesToSendUpdatesAbout.push_back( i );
            
                nextPlayer->needsUpdate = false;
                }
            }
        

        if( playerIndicesToSendUpdatesAbout.size() > 0 ) {
            
            SimpleVector<char> updateList;
        
            for( int i=0; i<playerIndicesToSendUpdatesAbout.size(); i++ ) {
                LiveObject *nextPlayer = players.getElement( 
                    playerIndicesToSendUpdatesAbout.getElementDirect( i ) );
                
                char *playerString = autoSprintf( "%d, ", nextPlayer->id );
                updateList.appendElementString( playerString );
                
                delete [] playerString;
                }
            
            char *updateListString = updateList.getElementString();
            
            AppLog::detailF( "Need to send updates about these %d players: %s",
                          playerIndicesToSendUpdatesAbout.size(),
                          updateListString );
            delete [] updateListString;
            }
        


        double currentTimeHeat = Time::getCurrentTime();
        
        if( currentTimeHeat - lastHeatUpdateTime >= heatUpdateTimeStep ) {
            // a heat step has passed
            
            
            // recompute heat map here for next players in line
            int r = 0;
            for( r=lastPlayerIndexHeatRecomputed+1; 
                 r < lastPlayerIndexHeatRecomputed + 1 + 
                     numPlayersRecomputeHeatPerStep
                     &&
                     r < players.size(); r++ ) {
                
                recomputeHeatMap( players.getElement( r ) );
                }
            
            lastPlayerIndexHeatRecomputed = r - 1;
            
            if( r >= players.size() ) {
                // done updating for last player
                // start over
                lastPlayerIndexHeatRecomputed = -1;
                }
            lastHeatUpdateTime = currentTimeHeat;
            }
        



        // update personal heat value of any player that is due
        // once every 2 seconds
        currentTime = Time::getCurrentTime();
        for( int i=0; i< players.size(); i++ ) {
            LiveObject *nextPlayer = players.getElement( i );
            
            if( nextPlayer->error ||
                currentTime - nextPlayer->lastHeatUpdate < heatUpdateSeconds ) {
                continue;
                }
            
            // in case we cross a biome boundary since last time
            // there will be thermal shock that will take them to
            // other side of target temp.
            // 
            // but never make them more comfortable (closer to
            // target) then they were before
            float oldDiffFromTarget = 
                targetHeat - nextPlayer->bodyHeat;


            if( nextPlayer->lastBiomeHeat != nextPlayer->biomeHeat ) {
                
          
                float lastBiomeDiffFromTarget = 
                    targetHeat - nextPlayer->lastBiomeHeat;
            
                float biomeDiffFromTarget = targetHeat - nextPlayer->biomeHeat;
            
                // for any biome
                // there's a "shock" when you enter it, if it's heat value
                // is on the other side of "perfect" from the temp you were at
                if( lastBiomeDiffFromTarget != 0 &&
                    biomeDiffFromTarget != 0 &&
                    sign( oldDiffFromTarget ) != 
                    sign( biomeDiffFromTarget ) ) {
                    
                    
                    // shock them to their mirror temperature on the meter
                    // (reflected across target temp)
                    nextPlayer->bodyHeat = targetHeat + oldDiffFromTarget;
                    }

                // we've handled this shock
                nextPlayer->lastBiomeHeat = nextPlayer->biomeHeat;
                }


            
            float clothingHeat = computeClothingHeat( nextPlayer );
            
            float heldHeat = computeHeldHeat( nextPlayer );
            

            float clothingR = computeClothingR( nextPlayer );

            // clothingR modulates heat lost (or gained) from environment
            float clothingLeak = 1 - clothingR;

            

            // what our body temp will move toward gradually
            // clothing heat and held heat are conductive
            // if they are present, they move envHeat up or down, before
            // we compute diff with body heat
            // (if they are 0, they have no effect)
            float envHeatTarget = clothingHeat + heldHeat + nextPlayer->envHeat;
            
            if( envHeatTarget < targetHeat ) {
                // we're in a cold environment

                if( nextPlayer->isIndoors ) {
                    float targetDiff = targetHeat - envHeatTarget;
                    float indoorAdjustedDiff = targetDiff / 2;
                    envHeatTarget = targetHeat - indoorAdjustedDiff;
                    }
                
                // clothing actually reduces how cold it is
                // based on its R-value

                // in other words, it "closes the gap" between our
                // perfect temp and our environmental temp

                // perfect clothing R would cut the environmental cold
                // factor in half

                float targetDiff = targetHeat - envHeatTarget;
                
                float clothingAdjustedDiff = targetDiff / ( 1 + clothingR );
                
                // how much did clothing improve our situation?
                float improvement = targetDiff - clothingAdjustedDiff;
                
                if( nextPlayer->isIndoors ) {
                    // if indoors, double the improvement of clothing
                    // thus, if it took us half-way to perfect, being
                    // indoors will take us all the way to perfect
                    // think about this as a reduction in the wind chill
                    // factor
                    
                    improvement *= 2;
                    }
                clothingAdjustedDiff = targetDiff - improvement;

                
                envHeatTarget = targetHeat - clothingAdjustedDiff;
                }
            

            // clothing only slows down temp movement AWAY from perfect
            if( abs( targetHeat - envHeatTarget ) <
                abs( targetHeat - nextPlayer->bodyHeat ) ) {
                // env heat is closer to perfect than our current body temp
                // clothing R should not apply in this case
                clothingLeak = 1.0;
                }
            
            
            float heatDelta = 
                clothingLeak * ( envHeatTarget 
                                 - 
                                 nextPlayer->bodyHeat );

            // slow this down a bit
            heatDelta *= 0.5;
            
            // feed through curve that is asymtotic at 1
            // (so we never change heat faster than 1 unit per timestep)
            
            float heatDeltaAbs = fabs( heatDelta );
            float heatDeltaSign = sign( heatDelta );

            float maxDelta = 2;
            // larger values make a sharper "knee"
            float deltaSlope = 0.5;
            
            // max - max/(slope*x+1)
            
            float heatDeltaScaled = 
                maxDelta - maxDelta / ( deltaSlope * heatDeltaAbs + 1 );
            
            heatDeltaScaled *= heatDeltaSign;


            nextPlayer->bodyHeat += heatDeltaScaled;
            
            // cap body heat, so that it doesn't climb way out of range
            // even in extreme situations
            if( nextPlayer->bodyHeat > 2 * targetHeat ) {
                nextPlayer->bodyHeat = 2 * targetHeat;
                }
            else if( nextPlayer->bodyHeat < 0 ) {
                nextPlayer->bodyHeat = 0;
                }
            
            
            float totalBodyHeat = nextPlayer->bodyHeat + nextPlayer->fever;
            
            // 0.25 body heat no longer added in each step above
            // add in a flat constant here to reproduce its effects
            // but only in a cold env (just like the old body heat)
            if( envHeatTarget < targetHeat ) {
                totalBodyHeat += 0.003;
                }



            // convert into 0..1 range, where 0.5 represents targetHeat
            nextPlayer->heat = ( totalBodyHeat / targetHeat ) / 2;
            if( nextPlayer->heat > 1 ) {
                nextPlayer->heat = 1;
                }
            if( nextPlayer->heat < 0 ) {
                nextPlayer->heat = 0;
                }

            nextPlayer->heatUpdate = true;
            nextPlayer->lastHeatUpdate = currentTime;
            }
        

        
        for( int i=0; i<playerIndicesToSendUpdatesAbout.size(); i++ ) {
            LiveObject *nextPlayer = players.getElement( 
                playerIndicesToSendUpdatesAbout.getElementDirect( i ) );

            if( nextPlayer->updateSent ) {
                continue;
                }
            
            
            if( nextPlayer->vogMode ) {
                // VOG players
                // handle this here, to take them out of circulation
                nextPlayer->updateSent = true;
                continue;
                }
            

            // also force-recompute heat maps for players that are getting
            // updated
            // don't bother with this for now
            // all players update on the same cycle
            // recomputeHeatMap( nextPlayer );
            
            
            
            newUpdates.push_back( getUpdateRecord( nextPlayer, false ) );
            
            newUpdatePlayerIDs.push_back( nextPlayer->id );
            

            if( nextPlayer->posForced &&
                nextPlayer->connected &&
                SettingsManager::getIntSetting( "requireClientForceAck", 1 ) ) {
                // block additional moves/actions from this player until
                // we get a FORCE response, syncing them up with
                // their forced position.
                
                // don't do this for disconnected players
                nextPlayer->waitingForForceResponse = true;
                }
            nextPlayer->posForced = false;


            ChangePosition p = { nextPlayer->xs, nextPlayer->ys, 
                                 nextPlayer->updateGlobal };
            newUpdatesPos.push_back( p );


            nextPlayer->updateSent = true;
            nextPlayer->updateGlobal = false;
            }
        
        

        if( newUpdates.size() > 0 ) {
            
            SimpleVector<char> trueUpdateList;
            
            
            for( int i=0; i<newUpdates.size(); i++ ) {
                char *s = autoSprintf( 
                    "%d, ", newUpdatePlayerIDs.getElementDirect( i ) );
                trueUpdateList.appendElementString( s );
                delete [] s;
                }
            
            char *updateListString = trueUpdateList.getElementString();
            
            AppLog::detailF( "Sending updates about these %d players: %s",
                          newUpdatePlayerIDs.size(),
                          updateListString );
            delete [] updateListString;
            }
        
        

        
        SimpleVector<ChangePosition> movesPos;        

        SimpleVector<MoveRecord> moveList = getMoveRecords( true, &movesPos );
        
        
                







        

        

        // add changes from auto-decays on map, 
        // mixed with player-caused changes
        stepMap( &mapChanges, &mapChangesPos );
        
        

        
        if( periodicStepThisStep ) {

            // figure out who has recieved a new curse token
            // they are sent a message about it below (CX)
            SimpleVector<char*> newCurseTokenEmails;
            getNewCurseTokenHolders( &newCurseTokenEmails );
        
            for( int i=0; i<newCurseTokenEmails.size(); i++ ) {
                char *email = newCurseTokenEmails.getElementDirect( i );
                
                for( int j=0; j<numLive; j++ ) {
                    LiveObject *nextPlayer = players.getElement(j);
                    
                    if( strcmp( nextPlayer->email, email ) == 0 ) {
                        
                        nextPlayer->curseTokenCount = 1;
                        nextPlayer->curseTokenUpdate = true;
                        break;
                        }
                    }
                
                delete [] email;
                }
            }





        unsigned char *lineageMessage = NULL;
        int lineageMessageLength = 0;
        
        if( playerIndicesToSendLineageAbout.size() > 0 ) {
            SimpleVector<char> linWorking;
            linWorking.appendElementString( "LN\n" );
            
            int numAdded = 0;
            for( int i=0; i<playerIndicesToSendLineageAbout.size(); i++ ) {
                LiveObject *nextPlayer = players.getElement( 
                    playerIndicesToSendLineageAbout.getElementDirect( i ) );

                if( nextPlayer->error ) {
                    continue;
                    }
                getLineageLineForPlayer( nextPlayer, &linWorking );
                numAdded++;
                }
            
            linWorking.push_back( '#' );
            
            if( numAdded > 0 ) {

                char *lineageMessageText = linWorking.getElementString();
                
                lineageMessageLength = strlen( lineageMessageText );
                
                if( lineageMessageLength < maxUncompressedSize ) {
                    lineageMessage = (unsigned char*)lineageMessageText;
                    }
                else {
                    // compress for all players once here
                    lineageMessage = makeCompressedMessage( 
                        lineageMessageText, 
                        lineageMessageLength, &lineageMessageLength );
                    
                    delete [] lineageMessageText;
                    }
                }
            }




        unsigned char *cursesMessage = NULL;
        int cursesMessageLength = 0;
        
        if( playerIndicesToSendCursesAbout.size() > 0 ) {
            SimpleVector<char> curseWorking;
            curseWorking.appendElementString( "CU\n" );
            
            int numAdded = 0;
            for( int i=0; i<playerIndicesToSendCursesAbout.size(); i++ ) {
                LiveObject *nextPlayer = players.getElement( 
                    playerIndicesToSendCursesAbout.getElementDirect( i ) );

                if( nextPlayer->error ) {
                    continue;
                    }

                char *line = autoSprintf( "%d %d\n", nextPlayer->id,
                                         nextPlayer->curseStatus.curseLevel );
                
                curseWorking.appendElementString( line );
                delete [] line;
                numAdded++;
                }
            
            curseWorking.push_back( '#' );
            
            if( numAdded > 0 ) {

                char *cursesMessageText = curseWorking.getElementString();
                
                cursesMessageLength = strlen( cursesMessageText );
                
                if( cursesMessageLength < maxUncompressedSize ) {
                    cursesMessage = (unsigned char*)cursesMessageText;
                    }
                else {
                    // compress for all players once here
                    cursesMessage = makeCompressedMessage( 
                        cursesMessageText, 
                        cursesMessageLength, &cursesMessageLength );
                    
                    delete [] cursesMessageText;
                    }
                }
            }




        unsigned char *namesMessage = NULL;
        int namesMessageLength = 0;
        
        if( playerIndicesToSendNamesAbout.size() > 0 ) {
            SimpleVector<char> namesWorking;
            namesWorking.appendElementString( "NM\n" );
            
            int numAdded = 0;
            for( int i=0; i<playerIndicesToSendNamesAbout.size(); i++ ) {
                LiveObject *nextPlayer = players.getElement( 
                    playerIndicesToSendNamesAbout.getElementDirect( i ) );

                if( nextPlayer->error ) {
                    continue;
                    }

                char *line = autoSprintf( "%d %s\n", nextPlayer->id,
                                          nextPlayer->displayedName );
                numAdded++;
                namesWorking.appendElementString( line );
                delete [] line;
                }
            
            namesWorking.push_back( '#' );
            
            if( numAdded > 0 ) {

                char *namesMessageText = namesWorking.getElementString();
                
                namesMessageLength = strlen( namesMessageText );
                
                if( namesMessageLength < maxUncompressedSize ) {
                    namesMessage = (unsigned char*)namesMessageText;
                    }
                else {
                    // compress for all players once here
                    namesMessage = makeCompressedMessage( 
                        namesMessageText, 
                        namesMessageLength, &namesMessageLength );
                    
                    delete [] namesMessageText;
                    }
                }
            }



        unsigned char *dyingMessage = NULL;
        int dyingMessageLength = 0;
        
        if( playerIndicesToSendDyingAbout.size() > 0 ) {
            SimpleVector<char> dyingWorking;
            dyingWorking.appendElementString( "DY\n" );
            
            int numAdded = 0;
            for( int i=0; i<playerIndicesToSendDyingAbout.size(); i++ ) {
                LiveObject *nextPlayer = players.getElement( 
                    playerIndicesToSendDyingAbout.getElementDirect( i ) );

                if( nextPlayer->error ) {
                    continue;
                    }
                
                char *line;
                
                if( nextPlayer->holdingEtaDecay > 0 ) {
                    // what they have will cure itself in time
                    // flag as sick
                    line = autoSprintf( "%d 1\n", nextPlayer->id );
                    }
                else {
                    line = autoSprintf( "%d\n", nextPlayer->id );
                    }
                
                numAdded++;
                dyingWorking.appendElementString( line );
                delete [] line;
                }
            
            dyingWorking.push_back( '#' );
            
            if( numAdded > 0 ) {

                char *dyingMessageText = dyingWorking.getElementString();
                
                dyingMessageLength = strlen( dyingMessageText );
                
                if( dyingMessageLength < maxUncompressedSize ) {
                    dyingMessage = (unsigned char*)dyingMessageText;
                    }
                else {
                    // compress for all players once here
                    dyingMessage = makeCompressedMessage( 
                        dyingMessageText, 
                        dyingMessageLength, &dyingMessageLength );
                    
                    delete [] dyingMessageText;
                    }
                }
            }




        unsigned char *healingMessage = NULL;
        int healingMessageLength = 0;
        
        if( playerIndicesToSendHealingAbout.size() > 0 ) {
            SimpleVector<char> healingWorking;
            healingWorking.appendElementString( "HE\n" );
            
            int numAdded = 0;
            for( int i=0; i<playerIndicesToSendHealingAbout.size(); i++ ) {
                LiveObject *nextPlayer = players.getElement( 
                    playerIndicesToSendHealingAbout.getElementDirect( i ) );

                if( nextPlayer->error ) {
                    continue;
                    }

                char *line = autoSprintf( "%d\n", nextPlayer->id );

                numAdded++;
                healingWorking.appendElementString( line );
                delete [] line;
                }
            
            healingWorking.push_back( '#' );
            
            if( numAdded > 0 ) {

                char *healingMessageText = healingWorking.getElementString();
                
                healingMessageLength = strlen( healingMessageText );
                
                if( healingMessageLength < maxUncompressedSize ) {
                    healingMessage = (unsigned char*)healingMessageText;
                    }
                else {
                    // compress for all players once here
                    healingMessage = makeCompressedMessage( 
                        healingMessageText, 
                        healingMessageLength, &healingMessageLength );
                    
                    delete [] healingMessageText;
                    }
                }
            }




        unsigned char *emotMessage = NULL;
        int emotMessageLength = 0;
        
        if( newEmotPlayerIDs.size() > 0 ) {
            SimpleVector<char> emotWorking;
            emotWorking.appendElementString( "PE\n" );
            
            int numAdded = 0;
            for( int i=0; i<newEmotPlayerIDs.size(); i++ ) {
                
                int ttl = newEmotTTLs.getElementDirect( i );
                int pID = newEmotPlayerIDs.getElementDirect( i );
                int eInd = newEmotIndices.getElementDirect( i );
                
                char *line;
                
                if( ttl == 0  ) {
                    line = autoSprintf( 
                        "%d %d\n", pID, eInd );
                    }
                else {
                    line = autoSprintf( 
                        "%d %d %d\n", pID, eInd, ttl );
                        
                    if( ttl == -1 ) {
                        // a new permanent emot
                        LiveObject *pO = getLiveObject( pID );
                        if( pO != NULL ) {
                            pO->permanentEmots.push_back( eInd );
                            }
                        }
                        
                    }
                
                numAdded++;
                emotWorking.appendElementString( line );
                delete [] line;
                }
            
            emotWorking.push_back( '#' );
            
            if( numAdded > 0 ) {

                char *emotMessageText = emotWorking.getElementString();
                
                emotMessageLength = strlen( emotMessageText );
                
                if( emotMessageLength < maxUncompressedSize ) {
                    emotMessage = (unsigned char*)emotMessageText;
                    }
                else {
                    // compress for all players once here
                    emotMessage = makeCompressedMessage( 
                        emotMessageText, 
                        emotMessageLength, &emotMessageLength );
                    
                    delete [] emotMessageText;
                    }
                }
            }

        
        SimpleVector<char*> newOwnerStrings;
        for( int u=0; u<newOwnerPos.size(); u++ ) {
            newOwnerStrings.push_back( 
                getOwnershipString( newOwnerPos.getElementDirect( u ) ) );
            }



        
        // send moves and updates to clients
        
        
        SimpleVector<int> playersReceivingPlayerUpdate;
        

        for( int i=0; i<numLive; i++ ) {
            
            LiveObject *nextPlayer = players.getElement(i);
            
            
            // everyone gets all flight messages
            // even if they haven't gotten first message yet
            // (because the flier will get their first message again
            // when they land, and we need to tell them about flight first)
            if( nextPlayer->firstMapSent ||
                nextPlayer->inFlight ) {
                                
                if( newFlightDest.size() > 0 ) {
                    
                    // compose FD messages for this player
                    
                    for( int u=0; u<newFlightDest.size(); u++ ) {
                        FlightDest *f = newFlightDest.getElement( u );
                        
                        char *flightMessage = 
                            autoSprintf( "FD\n%d %d %d\n#",
                                         f->playerID,
                                         f->destPos.x -
                                         nextPlayer->birthPos.x, 
                                         f->destPos.y -
                                         nextPlayer->birthPos.y );
                        
                        sendMessageToPlayer( nextPlayer, flightMessage,
                                             strlen( flightMessage ) );
                        delete [] flightMessage;
                        }
                    }
                }

            
            
            double maxDist = getMaxChunkDimension();
            double maxDist2 = maxDist * 2;

            
            if( ! nextPlayer->firstMessageSent ) {
                

                // first, send the map chunk around them
                
                int numSent = sendMapChunkMessage( nextPlayer );
                
                if( numSent == -2 ) {
                    // still not sent, try again later
                    continue;
                    }

                
                // next send info about valley lines

                int valleySpacing = 
                    SettingsManager::getIntSetting( "valleySpacing", 40 );
                                  
                char *valleyMessage = 
                    autoSprintf( "VS\n"
                                 "%d %d\n#",
                                 valleySpacing,
                                 nextPlayer->birthPos.y % valleySpacing );
                
                sendMessageToPlayer( nextPlayer, 
                                     valleyMessage, strlen( valleyMessage ) );
                
                delete [] valleyMessage;
                


                SimpleVector<int> outOfRangePlayerIDs;
                

                // now send starting message
                SimpleVector<char> messageBuffer;

                messageBuffer.appendElementString( "PU\n" );

                int numPlayers = players.size();
            
                // must be last in message
                char *playersLine = NULL;
                
                for( int i=0; i<numPlayers; i++ ) {
                
                    LiveObject *o = players.getElement( i );
                
                    if( ( o != nextPlayer && o->error ) 
                        ||
                        o->vogMode ) {
                        continue;
                        }

                    char oWasForced = o->posForced;
                    
                    if( nextPlayer->inFlight || 
                        nextPlayer->vogMode || nextPlayer->postVogMode ) {
                        // not a true first message
                        
                        // force all positions for all players
                        o->posForced = true;
                        }
                    

                    // true mid-move positions for first message
                    // all relative to new player's birth pos
                    char *messageLine = getUpdateLine( o, 
                                                       nextPlayer->birthPos,
                                                       getPlayerPos(
                                                           nextPlayer ),
                                                       false, true );
                    
                    if( nextPlayer->inFlight || 
                        nextPlayer->vogMode || nextPlayer->postVogMode ) {
                        // restore
                        o->posForced = oWasForced;
                        }
                    

                    // skip sending info about errored players in
                    // first message
                    if( o->id != nextPlayer->id ) {
                        messageBuffer.appendElementString( messageLine );
                        delete [] messageLine;
                        
                        double d = intDist( o->xd, o->yd, 
                                            nextPlayer->xd,
                                            nextPlayer->yd );
                        
                        if( d > maxDist ) {
                            outOfRangePlayerIDs.push_back( o->id );
                            }
                        }
                    else {
                        // save until end
                        playersLine = messageLine;
                        }
                    }
                
                if( playersLine != NULL ) {    
                    messageBuffer.appendElementString( playersLine );
                    delete [] playersLine;
                    }
                
                messageBuffer.push_back( '#' );
            
                char *message = messageBuffer.getElementString();


                sendMessageToPlayer( nextPlayer, message, strlen( message ) );
                
                delete [] message;


                // send out-of-range message for all players in PU above
                // that were out of range
                if( outOfRangePlayerIDs.size() > 0 ) {
                    SimpleVector<char> messageChars;
            
                    messageChars.appendElementString( "PO\n" );
            
                    for( int i=0; i<outOfRangePlayerIDs.size(); i++ ) {
                        char buffer[20];
                        sprintf( buffer, "%d\n",
                                 outOfRangePlayerIDs.getElementDirect( i ) );
                                
                        messageChars.appendElementString( buffer );
                        }
                    messageChars.push_back( '#' );

                    char *outOfRangeMessageText = 
                        messageChars.getElementString();
                    
                    sendMessageToPlayer( nextPlayer, outOfRangeMessageText,
                                         strlen( outOfRangeMessageText ) );

                    delete [] outOfRangeMessageText;
                    }
                
                

                char *movesMessage = 
                    getMovesMessage( false, 
                                     nextPlayer->birthPos,
                                     getPlayerPos( nextPlayer ) );
                
                if( movesMessage != NULL ) {
                    
                
                    sendMessageToPlayer( nextPlayer, movesMessage, 
                                         strlen( movesMessage ) );
                
                    delete [] movesMessage;
                    }



                // send lineage for everyone alive
                
                
                SimpleVector<char> linWorking;
                linWorking.appendElementString( "LN\n" );

                int numAdded = 0;
                
                for( int i=0; i<numPlayers; i++ ) {
                
                    LiveObject *o = players.getElement( i );
                
                    if( o->error ) {
                        continue;
                        }
                    
                    getLineageLineForPlayer( o, &linWorking );
                    numAdded++;
                    }
                
                linWorking.push_back( '#' );
            
                if( numAdded > 0 ) {
                    char *linMessage = linWorking.getElementString();


                    sendMessageToPlayer( nextPlayer, linMessage, 
                                         strlen( linMessage ) );
                
                    delete [] linMessage;
                    }



                // send names for everyone alive
                
                SimpleVector<char> namesWorking;
                namesWorking.appendElementString( "NM\n" );

                numAdded = 0;
                
                for( int i=0; i<numPlayers; i++ ) {
                
                    LiveObject *o = players.getElement( i );
                
                    if( o->error || o->displayedName == NULL) {
                        continue;
                        }

                    char *line = autoSprintf( "%d %s\n", o->id, o->displayedName );
                    namesWorking.appendElementString( line );
                    delete [] line;
                    
                    numAdded++;
                    }
                
                namesWorking.push_back( '#' );
            
                if( numAdded > 0 ) {
                    char *namesMessage = namesWorking.getElementString();


                    sendMessageToPlayer( nextPlayer, namesMessage, 
                                         strlen( namesMessage ) );
                
                    delete [] namesMessage;
                    }



                // send cursed status for all living cursed
                
                SimpleVector<char> cursesWorking;
                cursesWorking.appendElementString( "CU\n" );

                numAdded = 0;
                
                for( int i=0; i<numPlayers; i++ ) {
                
                    LiveObject *o = players.getElement( i );
                
                    if( o->error ) {
                        continue;
                        }

                    int level = o->curseStatus.curseLevel;
                    
                    if( level == 0 ) {

                        if( usePersonalCurses ) {
                            if( isCursed( nextPlayer->email,
                                          o->email ) ) {
                                level = 1;
                                }
                            }
                        }
                    
                    if( level == 0 ) {
                        continue;
                        }
                    

                    char *line = autoSprintf( "%d %d\n", o->id, level );
                    cursesWorking.appendElementString( line );
                    delete [] line;
                    
                    numAdded++;
                    }
                
                cursesWorking.push_back( '#' );
            
                if( numAdded > 0 ) {
                    char *cursesMessage = cursesWorking.getElementString();


                    sendMessageToPlayer( nextPlayer, cursesMessage, 
                                         strlen( cursesMessage ) );
                
                    delete [] cursesMessage;
                    }
                

                if( nextPlayer->curseStatus.curseLevel > 0 ) {
                    // send player their personal report about how
                    // many excess curse points they have
                    
                    char *message = autoSprintf( 
                        "CS\n%d#", 
                        nextPlayer->curseStatus.excessPoints );

                    sendMessageToPlayer( nextPlayer, message, 
                                         strlen( message ) );
                
                    delete [] message;
                    }
                



                // send dying for everyone who is dying
                
                SimpleVector<char> dyingWorking;
                dyingWorking.appendElementString( "DY\n" );

                numAdded = 0;
                
                for( int i=0; i<numPlayers; i++ ) {
                
                    LiveObject *o = players.getElement( i );
                
                    if( o->error || ! o->dying ) {
                        continue;
                        }

                    char *line = autoSprintf( "%d\n", o->id );
                    dyingWorking.appendElementString( line );
                    delete [] line;
                    
                    numAdded++;
                    }
                
                dyingWorking.push_back( '#' );
            
                if( numAdded > 0 ) {
                    char *dyingMessage = dyingWorking.getElementString();


                    sendMessageToPlayer( nextPlayer, dyingMessage, 
                                         strlen( dyingMessage ) );
                
                    delete [] dyingMessage;
                    }

                // tell them about all permanent emots
                SimpleVector<char> emotMessageWorking;
                emotMessageWorking.appendElementString( "PE\n" );
                for( int i=0; i<numPlayers; i++ ) {
                
                    LiveObject *o = players.getElement( i );
                
                    if( o->error ) {
                        continue;
                        }
                    for( int e=0; e< o->permanentEmots.size(); e ++ ) {
                        // ttl -2 for permanent but not new
                        char *line = autoSprintf( 
                            "%d %d -2\n",
                            o->id, 
                            o->permanentEmots.getElementDirect( e ) );
                        emotMessageWorking.appendElementString( line );
                        delete [] line;
                        }
                    }
                emotMessageWorking.push_back( '#' );
                
                char *emotMessage = emotMessageWorking.getElementString();
                
                sendMessageToPlayer( nextPlayer, emotMessage, 
                                     strlen( emotMessage ) );
                    
                delete [] emotMessage;
                    

                
                nextPlayer->firstMessageSent = true;
                nextPlayer->inFlight = false;
                nextPlayer->postVogMode = false;
                }
            else {
                // this player has first message, ready for updates/moves
                

                if( nextPlayer->monumentPosSet && 
                    ! nextPlayer->monumentPosSent &&
                    computeAge( nextPlayer ) > 0.5 ) {
                    
                    // they learned about a monument from their mother
                    
                    // wait until they are half a year old to tell them
                    // so they have a chance to load the sound first
                    
                    char *monMessage = 
                        autoSprintf( "MN\n%d %d %d\n#", 
                                     nextPlayer->lastMonumentPos.x -
                                     nextPlayer->birthPos.x, 
                                     nextPlayer->lastMonumentPos.y -
                                     nextPlayer->birthPos.y,
                                     nextPlayer->lastMonumentID );
                    
                    sendMessageToPlayer( nextPlayer, monMessage, 
                                         strlen( monMessage ) );
                    
                    nextPlayer->monumentPosSent = true;
                    
                    delete [] monMessage;
                    }




                // everyone gets all grave messages
                if( newGraves.size() > 0 ) {
                    
                    // compose GV messages for this player
                    
                    for( int u=0; u<newGraves.size(); u++ ) {
                        GraveInfo *g = newGraves.getElement( u );
                        
                        // only graves that are either in-range
                        // OR that are part of our family line.
                        // This prevents leaking relative positions
                        // through grave locations, but still allows
                        // us to return home after a long journey
                        // and find the grave of a family member
                        // who died while we were away.
                        if( distance( g->pos, getPlayerPos( nextPlayer ) )
                            < maxDist2 
                            ||
                            g->lineageEveID == nextPlayer->lineageEveID ) {
                            
                            char *graveMessage = 
                                autoSprintf( "GV\n%d %d %d\n#", 
                                             g->pos.x -
                                             nextPlayer->birthPos.x, 
                                             g->pos.y -
                                             nextPlayer->birthPos.y,
                                             g->playerID );
                            
                            sendMessageToPlayer( nextPlayer, graveMessage,
                                                 strlen( graveMessage ) );
                            delete [] graveMessage;
                            }
                        }
                    }


                // everyone gets all grave move messages
                if( newGraveMoves.size() > 0 ) {
                    
                    // compose GM messages for this player
                    
                    for( int u=0; u<newGraveMoves.size(); u++ ) {
                        GraveMoveInfo *g = newGraveMoves.getElement( u );
                        
                        // lineage info lost once grave moves
                        // and we still don't want long-distance relative
                        // position leaking happening here.
                        // So, far-away grave moves simply won't be 
                        // transmitted.  This may result in some confusion
                        // between different clients that have different
                        // info about graves, but that's okay.

                        // Anyway, if you're far from home, and your relative
                        // dies, you'll hear about the original grave.
                        // But then if someone moves the bones before you
                        // get home, you won't be able to find the grave
                        // by name after that.
                        
                        GridPos playerPos = getPlayerPos( nextPlayer );
                        
                        if( distance( g->posStart, playerPos )
                            < maxDist2 
                            ||
                            distance( g->posEnd, playerPos )
                            < maxDist2 ) {

                            char *graveMessage = 
                            autoSprintf( "GM\n%d %d %d %d %d\n#", 
                                         g->posStart.x -
                                         nextPlayer->birthPos.x,
                                         g->posStart.y -
                                         nextPlayer->birthPos.y,
                                         g->posEnd.x -
                                         nextPlayer->birthPos.x,
                                         g->posEnd.y -
                                         nextPlayer->birthPos.y,
                                         g->swapDest );
                        
                            sendMessageToPlayer( nextPlayer, graveMessage,
                                                 strlen( graveMessage ) );
                            delete [] graveMessage;
                            }
                        }
                    }
                
                
                // everyone gets all owner change messages
                if( newOwnerPos.size() > 0 ) {

                    GridPos nextPlayerPos = getPlayerPos( nextPlayer );
                    
                    // compose OW messages for this player
                    for( int u=0; u<newOwnerPos.size(); u++ ) {
                        GridPos p = newOwnerPos.getElementDirect( u );
                        
                        // only pos that are either in-range
                        // OR are already known to this player.
                        // This prevents leaking relative positions
                        // through owned locations, but still allows
                        // us to instantly learn about important ownership
                        // changes
                        char known = isKnownOwned( nextPlayer, p );
                        
                        if( known ||
                            distance( p, nextPlayerPos )
                            < maxDist2 
                            ||
                            isOwned( nextPlayer, p ) ) {
                            
                            if( ! known ) {
                                // remember that we know about it now
                                nextPlayer->knownOwnedPositions.push_back( p );
                                }

                            char *ownerMessage = 
                                autoSprintf( 
                                    "OW\n%d %d%s\n#", 
                                    p.x -
                                    nextPlayer->birthPos.x, 
                                    p.y -
                                    nextPlayer->birthPos.y,
                                    newOwnerStrings.getElementDirect( u ) );
                            
                            sendMessageToPlayer( nextPlayer, ownerMessage,
                                                 strlen( ownerMessage ) );
                            delete [] ownerMessage;
                            }
                        }
                    }



                if( newFlipPlayerIDs.size() > 0 ) {

                    GridPos nextPlayerPos = getPlayerPos( nextPlayer );

                    // compose FL messages for this player
                    // only for in-range players that flipped
                    SimpleVector<char> messageWorking;
                    
                    char firstLine = true;
                    
                    for( int u=0; u<newFlipPlayerIDs.size(); u++ ) {
                        GridPos p = newFlipPositions.getElementDirect( u );
                        
                        if( distance( p, nextPlayerPos ) < maxDist2 ) {

                            if( firstLine ) {
                                messageWorking.appendElementString( "FL\n" );
                                firstLine = false;
                                }

                            char *line = 
                                autoSprintf( 
                                    "%d %d\n",
                                    newFlipPlayerIDs.getElementDirect( u ),
                                    newFlipFacingLeft.getElementDirect( u ) );
                            
                            messageWorking.appendElementString( line );
                            
                            delete [] line;
                            }
                        }
                    if( messageWorking.size() > 0 ) {
                        messageWorking.push_back( '#' );
                            
                        char *message = messageWorking.getElementString();
                        
                        sendMessageToPlayer( nextPlayer, message,
                                             strlen( message ) );
                        delete [] message;
                        }
                    }

                

                int playerXD = nextPlayer->xd;
                int playerYD = nextPlayer->yd;
                int chunkDimensionX = nextPlayer->mMapD / 2;
                int chunkDimensionY = chunkDimensionX - 2;
                if( nextPlayer->heldByOther ) {
                    LiveObject *holdingPlayer = 
                        getLiveObject( nextPlayer->heldByOtherID );
                
                    if( holdingPlayer != NULL ) {
                        playerXD = holdingPlayer->xd;
                        playerYD = holdingPlayer->yd;
                    }
                }
                //printf("playerXD: %d, lastSentMapX: %d\n", playerXD, nextPlayer->lastSentMapX);

                //printf("playerYD: %d, lastSentMapY: %d\n", playerYD, nextPlayer->lastSentMapY);
                //printf("chunkX: %d, chunkY: %d\n", chunkDimensionX, chunkDimensionY);
                if( abs( playerXD - nextPlayer->lastSentMapX ) > (chunkDimensionY / 4)
                    ||
                    abs( playerYD - nextPlayer->lastSentMapY ) > (chunkDimensionX / 4)
                    ||
                    ! nextPlayer->firstMapSent ) {
                    //printf("sendMapChunkMessage: playerXD:%d, playerYD: %d\n", playerXD, playerYD);
                    // moving out of bounds of chunk, send update
                    // or player flagged as needing first map again
                    
                    sendMapChunkMessage( nextPlayer,
                                         // override if held
                                         nextPlayer->heldByOther,
                                         playerXD,
                                         playerYD );


                    // send updates about any non-moving players
                    // that are in this chunk
                    SimpleVector<char> chunkPlayerUpdates;

                    SimpleVector<char> chunkPlayerMoves;
                    

                    // add chunk updates for held babies first
                    for( int j=0; j<numLive; j++ ) {
                        LiveObject *otherPlayer = players.getElement( j );
                        
                        if( otherPlayer->error ) {
                            continue;
                            }


                        if( otherPlayer->heldByOther ) {
                            LiveObject *adultO = 
                                getAdultHolding( otherPlayer );
                            
                            if( adultO != NULL ) {
                                

                                if( adultO->id != nextPlayer->id &&
                                    otherPlayer->id != nextPlayer->id ) {
                                    // parent not us
                                    // baby not us

                                    double d = intDist( playerXD,
                                                        playerYD,
                                                        adultO->xd,
                                                        adultO->yd );
                            
                            
                                    if( d <= getMaxChunkDimension() / 2 ) {
                                        // adult holding this baby
                                        // is close enough
                                        // send update about baby
                                        char *updateLine = 
                                            getUpdateLine( otherPlayer,
                                                           nextPlayer->birthPos,
                                                           getPlayerPos( 
                                                               nextPlayer ),
                                                           false ); 
                                    
                                        chunkPlayerUpdates.
                                            appendElementString( updateLine );
                                        delete [] updateLine;
                                        }
                                    }
                                }
                            }
                        }
                    
                    
                    int ourHolderID = -1;
                    
                    if( nextPlayer->heldByOther ) {
                        LiveObject *adult = getAdultHolding( nextPlayer );
                        
                        if( adult != NULL ) {
                            ourHolderID = adult->id;
                            }
                        }
                    
                    // now send updates about all non-held babies,
                    // including any adults holding on-chunk babies
                    // here, AFTER we update about the babies

                    // (so their held status overrides the baby's stale
                    //  position status).
                    for( int j=0; j<numLive; j++ ) {
                        LiveObject *otherPlayer = 
                            players.getElement( j );
                        
                        if( otherPlayer->error ||
                            otherPlayer->vogMode ) {
                            continue;
                            }


                        if( !otherPlayer->heldByOther &&
                            otherPlayer->id != nextPlayer->id &&
                            otherPlayer->id != ourHolderID ) {
                            // not us
                            // not a held baby (covered above)
                            // no the adult holding us

                            double d = intDist( playerXD,
                                                playerYD,
                                                otherPlayer->xd,
                                                otherPlayer->yd );
                            
                            
                            if( d <= getMaxChunkDimension() / 2 ) {
                                
                                // send next player a player update
                                // about this player, telling nextPlayer
                                // where this player was last stationary
                                // and what they're holding

                                char *updateLine = 
                                    getUpdateLine( otherPlayer, 
                                                   nextPlayer->birthPos,
                                                   getPlayerPos( nextPlayer ),
                                                   false ); 
                                    
                                chunkPlayerUpdates.appendElementString( 
                                    updateLine );
                                delete [] updateLine;
                                

                                // We don't need to tell player about 
                                // moves in progress on this chunk.
                                // We're receiving move messages from 
                                // a radius of 32
                                // but this chunk has a radius of 16
                                // so we're hearing about player moves
                                // before they're on our chunk.
                                // Player moves have limited length,
                                // so there's no chance of a long move
                                // that started outside of our 32-radius
                                // finishinging inside this new chunk.
                                }
                            }
                        }


                    if( chunkPlayerUpdates.size() > 0 ) {
                        chunkPlayerUpdates.push_back( '#' );
                        char *temp = chunkPlayerUpdates.getElementString();

                        char *message = concatonate( "PU\n", temp );
                        delete [] temp;

                        sendMessageToPlayer( nextPlayer, message, 
                                             strlen( message ) );
                        
                        delete [] message;
                        }

                    
                    if( chunkPlayerMoves.size() > 0 ) {
                        char *temp = chunkPlayerMoves.getElementString();

                        sendMessageToPlayer( nextPlayer, temp, strlen( temp ) );

                        delete [] temp;
                        }
                    }
                // done handling sending new map chunk and player updates
                // for players in the new chunk
                
                

                // EVERYONE gets info about dying players

                // do this first, so that PU messages about what they 
                // are holding post-wound come later                
                if( dyingMessage != NULL && nextPlayer->connected ) {
                    int numSent = 
                        nextPlayer->sock->send( 
                            dyingMessage, 
                            dyingMessageLength, 
                            false, false );
                    
                    nextPlayer->gotPartOfThisFrame = true;

                    if( numSent != dyingMessageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed"  ,__func__ , __LINE__);
                        }
                    }


                // EVERYONE gets info about now-healed players           
                if( healingMessage != NULL && nextPlayer->connected ) {
                    int numSent = 
                        nextPlayer->sock->send( 
                            healingMessage, 
                            healingMessageLength, 
                            false, false );
                    
                    nextPlayer->gotPartOfThisFrame = true;
                    
                    if( numSent != healingMessageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed"  ,__func__ , __LINE__);
                        }
                    }


                // EVERYONE gets info about emots           
                if( emotMessage != NULL && nextPlayer->connected ) {
                    int numSent = 
                        nextPlayer->sock->send( 
                            emotMessage, 
                            emotMessageLength, 
                            false, false );
                    
                    nextPlayer->gotPartOfThisFrame = true;
                    
                    if( numSent != emotMessageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed",  __func__ , __LINE__);
                        }
                    }

                
                // greater than maxDis but within maxDist2
                // for either PU or PM messages
                // (send PO for both, because we can have case
                // were a player coninously walks through the middleDistance
                // w/o ever stopping to create a PU message)
                SimpleVector<int> middleDistancePlayerIDs;
                



                if( newUpdates.size() > 0 && nextPlayer->connected ) {

                    double minUpdateDist = maxDist2 * 2;                    

                    for( int u=0; u<newUpdatesPos.size(); u++ ) {
                        ChangePosition *p = newUpdatesPos.getElement( u );
                        
                        // update messages can be global when a new
                        // player joins or an old player is deleted
                        if( p->global ) {
                            minUpdateDist = 0;
                            }
                        else {
                            double d = intDist( p->x, p->y, 
                                                playerXD, 
                                                playerYD );
                    
                            if( d < minUpdateDist ) {
                                minUpdateDist = d;
                                }
                            if( d > maxDist && d <= maxDist2 ) {
                                middleDistancePlayerIDs.push_back(
                                    newUpdatePlayerIDs.getElementDirect( u ) );
                                }
                            }
                        }

                    if( minUpdateDist <= maxDist ) {
                        // some updates close enough

                        // compose PU mesage for this player
                        
                        unsigned char *updateMessage = NULL;
                        int updateMessageLength = 0;
                        SimpleVector<char> updateChars;
                        
                        for( int u=0; u<newUpdates.size(); u++ ) {
                            ChangePosition *p = newUpdatesPos.getElement( u );
                        
                            double d = intDist( p->x, p->y, 
                                                playerXD, playerYD );
                            
                            if( ! p->global && d > maxDist ) {
                                // skip this one, too far away
                                continue;
                                }

                            if( p->global &&  d > maxDist ) {
                                // out of range global updates should
                                // also be followed by PO message
                                middleDistancePlayerIDs.push_back(
                                    newUpdatePlayerIDs.getElementDirect( u ) );
                                }
                            
                            
                            char *line =
                                getUpdateLineFromRecord( 
                                    newUpdates.getElement( u ),
                                    nextPlayer->birthPos,
                                    getPlayerPos( nextPlayer ) );
                            
                            updateChars.appendElementString( line );
                            delete [] line;
                            }
                        

                        if( updateChars.size() > 0 ) {
                            updateChars.push_back( '#' );
                            char *temp = updateChars.getElementString();

                            char *updateMessageText = 
                                concatonate( "PU\n", temp );
                            delete [] temp;
                            
                            updateMessageLength = strlen( updateMessageText );

                            if( updateMessageLength < maxUncompressedSize ) {
                                updateMessage = 
                                    (unsigned char*)updateMessageText;
                                }
                            else {
                                updateMessage = makeCompressedMessage( 
                                    updateMessageText, 
                                    updateMessageLength, &updateMessageLength );
                
                                delete [] updateMessageText;
                                }
                            }

                        if( updateMessage != NULL ) {
                            playersReceivingPlayerUpdate.push_back( 
                                nextPlayer->id );
                            
                            int numSent = 
                                nextPlayer->sock->send( 
                                    updateMessage, 
                                    updateMessageLength, 
                                    false, false );
                            
                            nextPlayer->gotPartOfThisFrame = true;
                            
                            delete [] updateMessage;
                            
                            if( numSent != updateMessageLength ) {
                                setPlayerDisconnected( nextPlayer, 
                                                       "Socket write failed",  __func__ , __LINE__);
                                }
                            }
                        }
                    }




                if( moveList.size() > 0 && nextPlayer->connected ) {
                    
                    double minUpdateDist = getMaxChunkDimension() * 2;
                    
                    for( int u=0; u<movesPos.size(); u++ ) {
                        ChangePosition *p = movesPos.getElement( u );
                        
                        // move messages are never global

                        double d = intDist( p->x, p->y, 
                                            playerXD, playerYD );
                    
                        if( d < minUpdateDist ) {
                            minUpdateDist = d;
                            }
                        if( d > maxDist && d <= maxDist2 ) {
                            middleDistancePlayerIDs.push_back(
                                moveList.getElement( u )->playerID );
                            }
                        }

                    if( minUpdateDist <= maxDist ) {
                        
                        SimpleVector<MoveRecord> closeMoves;
                        
                        for( int u=0; u<movesPos.size(); u++ ) {
                            ChangePosition *p = movesPos.getElement( u );
                            
                            // move messages are never global
                            
                            double d = intDist( p->x, p->y, 
                                                playerXD, playerYD );
                    
                            if( d > maxDist ) {
                                continue;
                                }
                            closeMoves.push_back( 
                                moveList.getElementDirect( u ) );
                            }
                        
                        if( closeMoves.size() > 0 ) {
                            
                            char *moveMessageText = getMovesMessageFromList( 
                                &closeMoves, nextPlayer->birthPos );
                        
                            unsigned char *moveMessage = NULL;
                            int moveMessageLength = 0;
        
                            if( moveMessageText != NULL ) {
                                moveMessage = (unsigned char*)moveMessageText;
                                moveMessageLength = strlen( moveMessageText );

                                if( moveMessageLength > maxUncompressedSize ) {
                                    moveMessage = makeCompressedMessage( 
                                        moveMessageText,
                                        moveMessageLength,
                                        &moveMessageLength );
                                    delete [] moveMessageText;
                                    }    
                                }

                            int numSent = 
                                nextPlayer->sock->send( 
                                    moveMessage, 
                                    moveMessageLength, 
                                    false, false );
                            
                            nextPlayer->gotPartOfThisFrame = true;
                            
                            delete [] moveMessage;
                            
                            if( numSent != moveMessageLength ) {
                                setPlayerDisconnected( nextPlayer, 
                                                       "Socket write failed" , __func__, __LINE__ );
                                }
                            }
                        }
                    }
                

                
                // now send PO for players that are out of range
                // who are moving or updating above
                if( middleDistancePlayerIDs.size() > 0 
                    && nextPlayer->connected ) {
                    
                    unsigned char *outOfRangeMessage = NULL;
                    int outOfRangeMessageLength = 0;
                    
                    if( middleDistancePlayerIDs.size() > 0 ) {
                        SimpleVector<char> messageChars;
            
                        messageChars.appendElementString( "PO\n" );
            
                        for( int i=0; 
                             i<middleDistancePlayerIDs.size(); i++ ) {
                            char buffer[20];
                            sprintf( 
                                buffer, "%d\n",
                                middleDistancePlayerIDs.
                                getElementDirect( i ) );
                                
                            messageChars.appendElementString( buffer );
                            }
                        messageChars.push_back( '#' );

                        char *outOfRangeMessageText = 
                            messageChars.getElementString();

                        outOfRangeMessageLength = 
                            strlen( outOfRangeMessageText );

                        if( outOfRangeMessageLength < 
                            maxUncompressedSize ) {
                            outOfRangeMessage = 
                                (unsigned char*)outOfRangeMessageText;
                            }
                        else {
                            // compress 
                            outOfRangeMessage = makeCompressedMessage( 
                                outOfRangeMessageText, 
                                outOfRangeMessageLength, 
                                &outOfRangeMessageLength );
                
                            delete [] outOfRangeMessageText;
                            }
                        }
                        
                    int numSent = 
                        nextPlayer->sock->send( 
                            outOfRangeMessage, 
                            outOfRangeMessageLength, 
                            false, false );
                        
                    nextPlayer->gotPartOfThisFrame = true;

                    delete [] outOfRangeMessage;

                    if( numSent != outOfRangeMessageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed" , __func__, __LINE__ );
                        }
                    }



                
                if( mapChanges.size() > 0 && nextPlayer->connected ) {
                    double minUpdateDist = getMaxChunkDimension() * 2;
                    
                    for( int u=0; u<mapChangesPos.size(); u++ ) {
                        ChangePosition *p = mapChangesPos.getElement( u );
                        
                        // map changes are never global

                        double d = intDist( p->x, p->y, 
                                            playerXD, playerYD );
                        
                        if( d < minUpdateDist ) {
                            minUpdateDist = d;
                            }
                        }

                    if( minUpdateDist <= maxDist ) {
                        // at least one thing in map change list is close
                        // enough to this player

                        // format custom map change message for this player
                        
                        
                        unsigned char *mapChangeMessage = NULL;
                        int mapChangeMessageLength = 0;
                        SimpleVector<char> mapChangeChars;

                        for( int u=0; u<mapChanges.size(); u++ ) {
                            ChangePosition *p = mapChangesPos.getElement( u );
                        
                            double d = intDist( p->x, p->y, 
                                                playerXD, playerYD );
                            
                            if( d > maxDist ) {
                                // skip this one, too far away
                                continue;
                                }
                            MapChangeRecord *r = 
                                mapChanges.getElement( u );
                            
                            char *lineString =
                                getMapChangeLineString( 
                                    r,
                                    nextPlayer->birthPos.x,
                                    nextPlayer->birthPos.y );
                            
                            mapChangeChars.appendElementString( lineString );
                            delete [] lineString;
                            }
                        
                        
                        if( mapChangeChars.size() > 0 ) {
                            mapChangeChars.push_back( '#' );
                            char *temp = mapChangeChars.getElementString();

                            char *mapChangeMessageText = 
                                concatonate( "MX\n", temp );
                            delete [] temp;

                            mapChangeMessageLength = 
                                strlen( mapChangeMessageText );
            
                            if( mapChangeMessageLength < 
                                maxUncompressedSize ) {
                                mapChangeMessage = 
                                    (unsigned char*)mapChangeMessageText;
                                }
                            else {
                                mapChangeMessage = makeCompressedMessage( 
                                    mapChangeMessageText, 
                                    mapChangeMessageLength, 
                                    &mapChangeMessageLength );
                
                                delete [] mapChangeMessageText;
                                }
                            }

                        
                        if( mapChangeMessage != NULL ) {

                            int numSent = 
                                nextPlayer->sock->send( 
                                    mapChangeMessage, 
                                    mapChangeMessageLength, 
                                    false, false );
                            
                            nextPlayer->gotPartOfThisFrame = true;
                            
                            delete [] mapChangeMessage;

                            if( numSent != mapChangeMessageLength ) {
                                setPlayerDisconnected( nextPlayer, 
                                                       "Socket write failed" , __func__ , __LINE__);
                                }
                            }
                        }
                    }
                if( newSpeechPos.size() > 0 && nextPlayer->connected ) {
                    double minUpdateDist = getMaxChunkDimension() * 2;
                    
                    for( int u=0; u<newSpeechPos.size(); u++ ) {
                        ChangePosition *p = newSpeechPos.getElement( u );
                        
                        // speech never global

                        double d = intDist( p->x, p->y, 
                                            playerXD, playerYD );
                        
                        if( d < minUpdateDist ) {
                            minUpdateDist = d;
                            }

                        }

                    if( minUpdateDist <= maxDist ) {

                        SimpleVector<char> messageWorking;
                        messageWorking.appendElementString( "PS\n" );
                        
                        
                        for( int u=0; u<newSpeechPos.size(); u++ ) {

                            ChangePosition *p = newSpeechPos.getElement( u );

                            if( p->responsiblePlayerID != -1 && 
                                p->responsiblePlayerID != nextPlayer->id ) 
                                continue;
                        
                            // speech never global
                            
                            double d = intDist( p->x, p->y, 
                                                playerXD, playerYD );
                            
                            if( d < maxDist ) {

                                int speakerID = 
                                    newSpeechPlayerIDs.getElementDirect( u );
                                LiveObject *speakerObj =
                                    getLiveObject( speakerID );
                                
                                int listenerEveID = nextPlayer->lineageEveID;
                                int listenerID = nextPlayer->id;
                                double listenerAge = computeAge( nextPlayer );
                                int listenerParentID = nextPlayer->parentID;
                                
                                int speakerEveID;
                                double speakerAge;
                                int speakerParentID = -1;
                                
                                if( speakerObj != NULL ) {
                                    speakerEveID = speakerObj->lineageEveID;
                                    speakerID = speakerObj->id;
                                    speakerAge = computeAge( speakerObj );
                                    speakerParentID = speakerObj->parentID;
                                    }
                                else {
                                    // speaker dead, doesn't matter what we
                                    // do
                                    speakerEveID = listenerEveID;
                                    speakerID = listenerID;
                                    speakerAge = listenerAge;
                                    }
                                

                                char *trimmedPhrase =
                                    stringDuplicate( newSpeechPhrases.
                                                     getElementDirect( u ) );

                                char *starLoc = 
                                    strstr( trimmedPhrase, " *map" );
                                    
                                if( starLoc != NULL ) {
                                    if( speakerID != listenerID ) {
                                        // only send map metadata through
                                        // if we picked up the map ourselves
                                        // trim it otherwise
                                        
                                        starLoc[0] = '\0';
                                        }
                                    else {
                                        // make coords birth-relative
                                        // to person reading map
                                        int mapX, mapY;

                                        // turn time into relative age in sec
                                        timeSec_t mapT = 0;
                                        
                                        int numRead = 
                                            sscanf( starLoc, 
                                                    " *map %d %d %lf",
                                                    &mapX, &mapY, &mapT );
                                        if( numRead == 2 || numRead == 3 ) {
                                            starLoc[0] = '\0';

                                            timeSec_t age = 0;
                                            
                                            if( numRead == 3 ) {
                                                age = Time::timeSec() - mapT;
                                                }

                                            char *newTrimmed = autoSprintf( 
                                                "%s *map %d %d %.f",
                                                trimmedPhrase,
                                                mapX - nextPlayer->birthPos.x, 
                                                mapY - nextPlayer->birthPos.y,
                                                age );
                                            
                                            delete [] trimmedPhrase;
                                            trimmedPhrase = newTrimmed;

                                            if( speakerObj != NULL ) {
                                                speakerObj->forceFlightDest.x
                                                    = mapX;
                                                speakerObj->forceFlightDest.y
                                                    = mapY;
                                                speakerObj->
                                                    forceFlightDestSetTime
                                                    = Time::getCurrentTime();
                                                }
                                            }
                                        }
                                    }

                                
                                char *translatedPhrase;
                                
                                // skip language filtering in some cases
                                // VOG can talk to anyone
                                // so can force spawns
                                // also, skip in on very low pop servers
                                // (just let everyone talk together)
                                if( nextPlayer->vogMode || 
                                    nextPlayer->forceSpawn || 
                                    ( speakerObj != NULL &&
                                      speakerObj->vogMode ) ||
                                    ( speakerObj != NULL &&
                                      speakerObj->forceSpawn ) ||
                                    players.size() < 
                                    minActivePlayersForLanguages ||
                                    strlen( newSpeechPhrases.getElementDirect( u ) ) == 0 ||
                                    newSpeechPhrases.getElementDirect( u )[0] == '[' ||
                                    newSpeechPhrases.getElementDirect( u )[0] == '+' ) {
                                    
                                    translatedPhrase =
                                        stringDuplicate( trimmedPhrase );
                                    }
                                else {
                                    // int speakerDrunkenness = 0;
                                    
                                    // if( speakerObj != NULL ) {
                                        // speakerDrunkenness =
                                            // speakerObj->drunkenness;
                                        // }

                                    translatedPhrase =
                                        mapLanguagePhrase( 
                                            trimmedPhrase,
                                            speakerEveID,
                                            listenerEveID,
                                            speakerID,
                                            listenerID,
                                            speakerAge,
                                            listenerAge,
                                            speakerParentID,
                                            listenerParentID );
                                    }
                                
                                if( speakerEveID != 
                                    listenerEveID
                                    && speakerAge > 55 
                                    && listenerAge > 55 ) {
                                    
                                    if( strcmp( translatedPhrase, "PEACE" )
                                        == 0 ) {
                                        // an elder speaker
                                        // said PEACE 
                                        // in elder listener's language
                                        addPeaceTreaty( speakerEveID,
                                                        listenerEveID );
                                        }
                                    else if( strcmp( translatedPhrase, 
                                                     "WAR" )
                                             == 0 ) {
                                            // an elder speaker
                                        // said WAR 
                                        // in elder listener's language
                                        removePeaceTreaty( speakerEveID,
                                                           listenerEveID );
                                        }
                                    }
                                
                                if( translatedPhrase[0] != '+' &&
                                    translatedPhrase[0] != '[' ) {
                                    if( speakerObj != NULL &&
                                        speakerObj->drunkenness > 0 ) {
                                        // slur their speech
                                        
                                        char *slurredPhrase =
                                            slurSpeech( speakerObj->id,
                                                        translatedPhrase,
                                                        speakerObj->drunkenness );
                                        
                                        delete [] translatedPhrase;
                                        translatedPhrase = slurredPhrase;
                                        }
                                        
                                    if( speakerObj != NULL &&
                                        speakerObj->tripping ) {
                                        // player is high on drugs and yelling
                                        
                                        char *processedPhrase =
                                            yellingSpeech( speakerObj->id,
                                                        translatedPhrase );
                                        
                                        delete [] translatedPhrase;
                                        translatedPhrase = processedPhrase;
                                        }
                                    }
                                

                                int curseFlag =
                                    newSpeechCurseFlags.getElementDirect( u );

                                char *line = autoSprintf( "%d/%d %s\n", 
                                                          speakerID,
                                                          curseFlag,
                                                          translatedPhrase );
                                delete [] translatedPhrase;
                                delete [] trimmedPhrase;
                                
                                messageWorking.appendElementString( line );
                                
                                delete [] line;
                                }
                            }
                        
                        messageWorking.appendElementString( "#" );
                            
                        char *messageText = 
                            messageWorking.getElementString();
                        
                        int messageLen = strlen( messageText );
                        
                        unsigned char *message = 
                            (unsigned char*) messageText;
                        
                        
                        if( messageLen >= maxUncompressedSize ) {
                            char *old = messageText;
                            int oldLen = messageLen;
                            
                            message = makeCompressedMessage( 
                                old, 
                                oldLen, &messageLen );
                            
                            delete [] old;
                            }
                        
                        
                        int numSent = 
                            nextPlayer->sock->send( 
                                message, 
                                messageLen, 
                                false, false );
                        
                        delete [] message;
                        
                        nextPlayer->gotPartOfThisFrame = true;
                        
                        if( numSent != messageLen ) {
                            setPlayerDisconnected( nextPlayer, 
                                                   "Socket write failed",  __func__ , __LINE__);
                            }
                        }
                    }


                if( newLocationSpeech.size() > 0 && nextPlayer->connected ) {
                    double minUpdateDist = getMaxChunkDimension() * 2;
                    
                    for( int u=0; u<newLocationSpeechPos.size(); u++ ) {
                        ChangePosition *p = 
                            newLocationSpeechPos.getElement( u );
                        
                        //responsiblePlayerID = -1 for range-based speech
                        if( p->responsiblePlayerID != -1 && 
                            p->responsiblePlayerID != nextPlayer->id ) 
                            continue;
                        
                        // locationSpeech never global

                        double d = intDist( p->x, p->y, 
                                            playerXD, playerYD );
                        
                        if( d < minUpdateDist ) {
                            minUpdateDist = d;
                            }
                        }

                    if( minUpdateDist <= maxDist ) {
                        // some of location speech in range
                        
                        SimpleVector<char> working;
                        
                        working.appendElementString( "LS\n" );
                        
                        for( int u=0; u<newLocationSpeechPos.size(); u++ ) {
                            ChangePosition *p = 
                                newLocationSpeechPos.getElement( u );
                                
                            //responsiblePlayerID = -1 for range-based speech
                            if( p->responsiblePlayerID != -1 && 
                                p->responsiblePlayerID != nextPlayer->id ) 
                                continue;
                            
                            char *line = autoSprintf( 
                                "%d %d %s\n",
                                p->x - nextPlayer->birthPos.x, 
                                p->y - nextPlayer->birthPos.y,
                                newLocationSpeech.getElementDirect( u ) );
                            working.appendElementString( line );
                            
                            delete [] line;
                            }
                        working.push_back( '#' );
                        
                        char *message = 
                            working.getElementString();
                        int len = working.size();
                        

                        if( len > maxUncompressedSize ) {
                            int compLen = 0;
                            
                            unsigned char *compMessage = makeCompressedMessage( 
                                message, 
                                len, 
                                &compLen );
                
                            delete [] message;
                            len = compLen;
                            message = (char*)compMessage;
                            }

                        int numSent = 
                            nextPlayer->sock->send( 
                                (unsigned char*)message,
                                len, 
                                false, false );
                        
                        delete [] message;
                        
                        nextPlayer->gotPartOfThisFrame = true;
                        
                        if( numSent != len ) {
                            setPlayerDisconnected( nextPlayer, 
                                                   "Socket write failed",  __func__ , __LINE__);
                            }
                        }
                    }
                


                // EVERYONE gets updates about deleted players                
                if( nextPlayer->connected ) {
                    
                    unsigned char *deleteUpdateMessage = NULL;
                    int deleteUpdateMessageLength = 0;
        
                    SimpleVector<char> deleteUpdateChars;
                
                    for( int u=0; u<newDeleteUpdates.size(); u++ ) {
                    
                        char *line = getUpdateLineFromRecord(
                            newDeleteUpdates.getElement( u ),
                            nextPlayer->birthPos,
                            getPlayerPos( nextPlayer ) );
                    
                        deleteUpdateChars.appendElementString( line );
                    
                        delete [] line;
                        }
                

                    if( deleteUpdateChars.size() > 0 ) {
                        deleteUpdateChars.push_back( '#' );
                        char *temp = deleteUpdateChars.getElementString();
                    
                        char *deleteUpdateMessageText = 
                            concatonate( "PU\n", temp );
                        delete [] temp;
                    
                        deleteUpdateMessageLength = 
                            strlen( deleteUpdateMessageText );

                        if( deleteUpdateMessageLength < maxUncompressedSize ) {
                            deleteUpdateMessage = 
                                (unsigned char*)deleteUpdateMessageText;
                            }
                        else {
                            // compress for all players once here
                            deleteUpdateMessage = makeCompressedMessage( 
                                deleteUpdateMessageText, 
                                deleteUpdateMessageLength, 
                                &deleteUpdateMessageLength );
                
                            delete [] deleteUpdateMessageText;
                            }
                        }

                    if( deleteUpdateMessage != NULL ) {
                        int numSent = 
                            nextPlayer->sock->send( 
                                deleteUpdateMessage, 
                                deleteUpdateMessageLength, 
                                false, false );
                    
                        nextPlayer->gotPartOfThisFrame = true;
                    
                        delete [] deleteUpdateMessage;
                    
                        if( numSent != deleteUpdateMessageLength ) {
                            setPlayerDisconnected( nextPlayer, 
                                                   "Socket write failed",  __func__ , __LINE__);
                            }
                        }
                    }



                // EVERYONE gets lineage info for new babies
                if( lineageMessage != NULL && nextPlayer->connected ) {
                    int numSent = 
                        nextPlayer->sock->send( 
                            lineageMessage, 
                            lineageMessageLength, 
                            false, false );
                    
                    nextPlayer->gotPartOfThisFrame = true;
                    
                    if( numSent != lineageMessageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed",  __func__ , __LINE__);
                        }
                    }


                // EVERYONE gets curse info for new babies
                if( cursesMessage != NULL && nextPlayer->connected ) {
                    int numSent = 
                        nextPlayer->sock->send( 
                            cursesMessage, 
                            cursesMessageLength, 
                            false, false );
                    
                    nextPlayer->gotPartOfThisFrame = true;
                    
                    if( numSent != cursesMessageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed" , __func__ , __LINE__);
                        }
                    }

                // EVERYONE gets newly-given names
                if( namesMessage != NULL && nextPlayer->connected ) {
                    int numSent = 
                        nextPlayer->sock->send( 
                            namesMessage, 
                            namesMessageLength, 
                            false, false );
                    
                    nextPlayer->gotPartOfThisFrame = true;
                    
                    if( numSent != namesMessageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed",  __func__ , __LINE__);
                        }
                    }

                


                if( nextPlayer->foodUpdate ) {
                    // send this player a food status change
                    
                    int cap = computeFoodCapacity( nextPlayer );
                    
                    if( cap < nextPlayer->foodStore ) {
                        nextPlayer->foodStore = cap;
                        }
                    
                    if( cap > nextPlayer->lastReportedFoodCapacity ) {
                        
                        // stomach grew
                        
                        // fill empty space from bonus store automatically
                        int extraCap = 
                            cap - nextPlayer->lastReportedFoodCapacity;
                        
                        while( nextPlayer->yummyBonusStore > 0 && 
                               extraCap > 0 &&
                               nextPlayer->foodStore < cap ) {
                            nextPlayer->foodStore ++;
                            extraCap --;
                            nextPlayer->yummyBonusStore--;
                            }
                        }
                    

                    nextPlayer->lastReportedFoodCapacity = cap;
                    

                    int yumMult = nextPlayer->yummyFoodChain.size() - 1;
                    
                    if( yumMult < 0 ) {
                        yumMult = 0;
                        }
                    
                    if( nextPlayer->connected ) {
                        
                        char *foodMessage = autoSprintf( 
                            "FX\n"
                            "%d %d %d %d %.2f %d "
                            "%d %d\n"
                            "#",
                            nextPlayer->foodStore,
                            cap,
                            hideIDForClient( nextPlayer->lastAteID ),
                            nextPlayer->lastAteFillMax,
                            computeMoveSpeed( nextPlayer ),
                            nextPlayer->responsiblePlayerID,
                            nextPlayer->yummyBonusStore,
                            yumMult );
                        
                        int messageLength = strlen( foodMessage );
                        
                        int numSent = 
                            nextPlayer->sock->send( 
                                (unsigned char*)foodMessage, 
                                messageLength,
                                false, false );
                        
                        nextPlayer->gotPartOfThisFrame = true;
                        
                        if( numSent != messageLength ) {
                            setPlayerDisconnected( nextPlayer, 
                                                   "Socket write failed",  __func__ , __LINE__);
                            }
                        
                        delete [] foodMessage;
                        }
                    
                    nextPlayer->foodUpdate = false;
                    nextPlayer->lastAteID = 0;
                    nextPlayer->lastAteFillMax = 0;
                    }



                if( nextPlayer->heatUpdate && nextPlayer->connected ) {
                    // send this player a heat status change
                    
                    char *heatMessage = autoSprintf( 
                        "HX\n"
                        "%.2f#",
                        nextPlayer->heat );
                     
                    int messageLength = strlen( heatMessage );
                    
                    int numSent = 
                         nextPlayer->sock->send( 
                             (unsigned char*)heatMessage, 
                             messageLength,
                             false, false );
                    
                    nextPlayer->gotPartOfThisFrame = true;
                    
                    if( numSent != messageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed",  __func__ , __LINE__);
                        }
                    
                    delete [] heatMessage;
                    }
                nextPlayer->heatUpdate = false;
                    

                if( nextPlayer->curseTokenUpdate &&
                    nextPlayer->connected ) {
                    // send this player a curse token status change
                    
                    char *tokenMessage = autoSprintf( 
                        "CX\n"
                        "%d#",
                        nextPlayer->curseTokenCount );
                     
                    int messageLength = strlen( tokenMessage );
                    
                    int numSent = 
                         nextPlayer->sock->send( 
                             (unsigned char*)tokenMessage, 
                             messageLength,
                             false, false );

                    nextPlayer->gotPartOfThisFrame = true;
                    
                    if( numSent != messageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed",  __func__, __LINE__);
                        }
                    
                    delete [] tokenMessage;                    
                    }
                nextPlayer->curseTokenUpdate = false;

                }
            }


        for( int u=0; u<moveList.size(); u++ ) {
            MoveRecord *r = moveList.getElement( u );
            delete [] r->formatString;
            }



        for( int u=0; u<mapChanges.size(); u++ ) {
            MapChangeRecord *r = mapChanges.getElement( u );
            delete [] r->formatString;
            }

        if( newUpdates.size() > 0 ) {
            
            SimpleVector<char> playerList;
            
            for( int i=0; i<playersReceivingPlayerUpdate.size(); i++ ) {
                char *playerString = 
                    autoSprintf( 
                        "%d, ",
                        playersReceivingPlayerUpdate.getElementDirect( i ) );
                playerList.appendElementString( playerString );
                delete [] playerString;
                }
            
            char *playerListString = playerList.getElementString();

            AppLog::detailF( "%d/%d players were sent part of a %d-line PU: %s",
                          playersReceivingPlayerUpdate.size(),
                          numLive, newUpdates.size(),
                          playerListString );
            
            delete [] playerListString;
            }
        

        for( int u=0; u<newUpdates.size(); u++ ) {
            UpdateRecord *r = newUpdates.getElement( u );
            delete [] r->formatString;
            }
        
        for( int u=0; u<newDeleteUpdates.size(); u++ ) {
            UpdateRecord *r = newDeleteUpdates.getElement( u );
            delete [] r->formatString;
            }

        
        if( lineageMessage != NULL ) {
            delete [] lineageMessage;
            }
        if( cursesMessage != NULL ) {
            delete [] cursesMessage;
            }
        if( namesMessage != NULL ) {
            delete [] namesMessage;
            }
        if( dyingMessage != NULL ) {
            delete [] dyingMessage;
            }
        if( healingMessage != NULL ) {
            delete [] healingMessage;
            }
        if( emotMessage != NULL ) {
            delete [] emotMessage;
            }
        
        
        newOwnerStrings.deallocateStringElements();
        
        
        // these are global, so we must clear it every loop
        newSpeechPos.deleteAll();
        newSpeechPlayerIDs.deleteAll();
        newSpeechCurseFlags.deleteAll();
        newSpeechPhrases.deallocateStringElements();

        newLocationSpeech.deallocateStringElements();
        newLocationSpeechPos.deleteAll();

        newGraves.deleteAll();
        newGraveMoves.deleteAll();
        
        newEmotPlayerIDs.deleteAll();
        newEmotIndices.deleteAll();
        newEmotTTLs.deleteAll();
        

        
        // handle end-of-frame for all players that need it
        const char *frameMessage = "FM\n#";
        int frameMessageLength = strlen( frameMessage );
        
        for( int i=0; i<players.size(); i++ ) {
            LiveObject *nextPlayer = players.getElement(i);
            
            if( nextPlayer->gotPartOfThisFrame && nextPlayer->connected ) {
                int numSent = 
                    nextPlayer->sock->send( 
                        (unsigned char*)frameMessage, 
                        frameMessageLength,
                        false, false );

                if( numSent != frameMessageLength ) {
                    setPlayerDisconnected( nextPlayer, "Socket write failed",  __func__ , __LINE__);
                    }
                }
            nextPlayer->gotPartOfThisFrame = false;
            }
        

        
        // handle closing any that have an error
        for( int i=0; i<players.size(); i++ ) {
            LiveObject *nextPlayer = players.getElement(i);

            if( nextPlayer->error && nextPlayer->deleteSent &&
                nextPlayer->deleteSentDoneETA < Time::getCurrentTime() ) {
                AppLog::infoF( "Closing connection to player %d on error "
                               "(cause: %s)",
                               nextPlayer->id, nextPlayer->errorCauseString );

                AppLog::infoF( "%d remaining player(s) alive on server ",
                               players.size() - 1 );
                
                addPastPlayer( nextPlayer );

                if( nextPlayer->sock != NULL ) {
                    sockPoll.removeSocket( nextPlayer->sock );
                
                    delete nextPlayer->sock;
                    nextPlayer->sock = NULL;
                    }
                
                if( nextPlayer->sockBuffer != NULL ) {
                    delete nextPlayer->sockBuffer;
                    nextPlayer->sockBuffer = NULL;
                    }
                
                delete nextPlayer->lineage;
                
                delete nextPlayer->ancestorIDs;
                
                nextPlayer->ancestorEmails->deallocateStringElements();
                delete nextPlayer->ancestorEmails;
                
                nextPlayer->ancestorRelNames->deallocateStringElements();
                delete nextPlayer->ancestorRelNames;
                
                delete nextPlayer->ancestorLifeStartTimeSeconds;
                delete nextPlayer->ancestorLifeEndTimeSeconds;


                if( nextPlayer->name != NULL ) {
                    delete [] nextPlayer->name;
                    }
                    
                if( nextPlayer->displayedName != NULL ) {
                    delete [] nextPlayer->displayedName;
                    }

                if( nextPlayer->familyName != NULL ) {
                    delete [] nextPlayer->familyName;
                    }

                if( nextPlayer->lastSay != NULL ) {
                    delete [] nextPlayer->lastSay;
                    }
                
                freePlayerContainedArrays( nextPlayer );
                
                if( nextPlayer->pathToDest != NULL ) {
                    delete [] nextPlayer->pathToDest;
                    }

                if( nextPlayer->email != NULL ) {
                    delete [] nextPlayer->email;
                    }
                if( nextPlayer->origEmail != NULL  ) {
                    delete [] nextPlayer->origEmail;
                    }
                if( nextPlayer->lastBabyEmail != NULL ) {
                    delete [] nextPlayer->lastBabyEmail;
                    }
                if( nextPlayer->lastSidsBabyEmail != NULL ) {
                    delete [] nextPlayer->lastSidsBabyEmail;
                    }

                if( nextPlayer->murderPerpEmail != NULL ) {
                    delete [] nextPlayer->murderPerpEmail;
                    }

                if( nextPlayer->deathReason != NULL ) {
                    delete [] nextPlayer->deathReason;
                    }
                
                nextPlayer->globalMessageQueue.deallocateStringElements();

                delete nextPlayer->babyBirthTimes;
                delete nextPlayer->babyIDs;

                players.deleteElement( i );
                i--;
                }
            }


        if( players.size() == 0 && newConnections.size() == 0 ) {
            if( shutdownMode ) {
                AppLog::info( "No live players or connections in shutdown " 
                              " mode, auto-quitting." );
                quit = true;
                }
            }
        }
    
    // stop listening on server socket immediately, before running
    // cleanup steps.  Cleanup may take a while, and we don't want to leave
    // server socket listening, because it will answer reflector and player
    // connection requests but then just hang there.

    // Closing the server socket makes these connection requests fail
    // instantly (instead of relying on client timeouts).
    delete server;

    quitCleanup();
    
    
    AppLog::info( "Done." );
    
    SettingsManager::setSetting( "forceShutdownMode", 0 );

    return 0;
    }



// implement null versions of these to allow a headless build
// we never call drawObject, but we need to use other objectBank functions


void *getSprite( int ) {
    return NULL;
    }

char *getSpriteTag( int ) {
    return NULL;
    }

char isSpriteBankLoaded() {
    return false;
    }

char markSpriteLive( int ) {
    return false;
    }

void stepSpriteBank() {
    }

void drawSprite( void*, doublePair, double, double, char ) {
    }

void setDrawColor( float inR, float inG, float inB, float inA ) {
    }

void setDrawFade( float ) {
    }

float getTotalGlobalFade() {
    return 1.0f;
    }

void toggleAdditiveTextureColoring( char inAdditive ) {
    }

void toggleAdditiveBlend( char ) {
    }

void drawSquare( doublePair, double ) {
    }

void startAddingToStencil( char, char, float ) {
    }

void startDrawingThroughStencil( char ) {
    }

void stopStencil() {
    }





// dummy implementations of these functions, which are used in editor
// and client, but not server
#include "../gameSource/spriteBank.h"
SpriteRecord *getSpriteRecord( int inSpriteID ) {
    return NULL;
    }

#include "../gameSource/soundBank.h"
void checkIfSoundStillNeeded( int inID ) {
    }



char getSpriteHit( int inID, int inXCenterOffset, int inYCenterOffset ) {
    return false;
    }


char getUsesMultiplicativeBlending( int inID ) {
    return false;
    }


void toggleMultiplicativeBlend( char inMultiplicative ) {
    }


void countLiveUse( SoundUsage inUsage ) {
    }

void unCountLiveUse( SoundUsage inUsage ) {
    }



// animation bank calls these only if lip sync hack is enabled, which
// it never is for server
void *loadSpriteBase( const char*, char ) {
    return NULL;
    }

void freeSprite( void* ) {
    }

void startOutputAllFrames() {
    }

void stopOutputAllFrames() {
    }
